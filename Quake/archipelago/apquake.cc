#include "apquake.h"
#include "apquake_def.h"

#include <cstring>
#include <chrono>
#include <fstream>
#include <set>
#include <sys/stat.h>
#include <thread>

#include <json/json.h>

#include "Archipelago.h"

ap_state_t ap_state;
int ap_is_in_game = 0;

static std::string ap_save_dir_name;

static int ap_get_map_count(int episode);

const int ap_episode_count = 5;
const int max_map_count = 8;
const int ap_weapon_count = 8;
const int ap_ammo_count = 4;
const int ap_powerup_count = 9;

static ap_settings_t ap_settings;
static AP_RoomInfo ap_room_info;
static std::vector<int64_t> ap_item_queue;
static bool ap_was_connected = false;
static std::set<int64_t> ap_progressive_locations;
static bool ap_initialized = false;
static std::vector<std::string> ap_cached_messages;

void load_state();

void save_state();

void f_itemclr();
void f_itemrecv(int64_t item_id, bool notify_player);
void f_locrecv(int64_t loc_id);
void f_locinfo(std::vector<AP_NetworkItem> loc_infos);
void f_goal(int);
void f_player_class(int);
void f_skill(int);
void f_random_monsters(int);
void f_random_items(int);
void f_random_music(int);
void f_flip_levels(int);
void f_check_sanity(int);
void f_reset_level_on_death(int);
void f_episode1(int);
void f_episode2(int);
void f_episode3(int);
void f_episode4(int);
void f_episode5(int);
void f_two_ways_keydoors(int);
void load_state();
void save_state();
void APSend(std::string msg);

std::string string_to_hex(const char* str)
{
    static const char hex_digits[] = "0123456789ABCDEF";

        std::string out;
        std::string in = str;

    out.reserve(in.length() * 2);
    for (unsigned char c : in)
    {
        out.push_back(hex_digits[c >> 4]);
        out.push_back(hex_digits[c & 15]);
    }

    return out;
}

static std::vector<std::vector<ap_level_info_t>>& get_level_info_table()
{
	return ap_quake_level_infos;
}

static std::map<int /* ep */, std::map<int /* map */, std::map<int /* index */, int64_t /* loc id */>>>& get_location_table()
{
	return ap_quake_location_table;
}

ap_level_info_t* ap_get_level_info(ap_level_index_t idx)
{
        auto& level_info_table = get_level_info_table();
        if (idx.ep < 0 || idx.ep >= (int)level_info_table.size()) return nullptr;
        if (idx.map < 0 || idx.map >= (int)level_info_table[idx.ep].size()) return nullptr;
        return &level_info_table[idx.ep][idx.map];
}

int validate_quake_location(ap_level_index_t idx, int index)
{
    auto& location_table = get_location_table();
    if (idx.ep < 0 || idx.ep >= (int)location_table.size()) return 0;
    if (idx.map < 0 || idx.map >= (int)location_table[idx.ep + 1].size()) return 0;
    auto location_for_level = location_table[idx.ep + 1][idx.map + 1];
    return location_for_level.count(index);
}

static std::map<int /* ep */, std::map<int /* map */, int /* track */>> quake_music_map = {
	{1, {
		{1, 6},
		{2, 8},
		{3, 9},
		{4, 5},
		{5, 11},
		{6, 4},
		{7, 7},
		{8, 10},
	}},
	{2, {
		{1, 6},
		{2, 8},
		{3, 9},
		{4, 5},
		{5, 11},
		{6, 4},
		{7, 7},
	}},
	{3, {
		{1, 6},
		{2, 8},
		{3, 9},
		{4, 8},
		{5, 11},
		{6, 4},
		{7, 5},
	}},
	{4, {
		{1, 6},
		{2, 8},
		{3, 9},
		{4, 5},
		{5, 11},
		{6, 4},
		{7, 7},
		{8, 10},
	}},
	{5, {
		{1, 4},
	}},
};

static int get_original_music_for_level(int ep, int map)
{
	return quake_music_map[ep][map];
}

int apquake_init(ap_settings_t* settings) {
        ap_state.level_states = new ap_level_state_t[ap_episode_count * max_map_count];
        ap_state.episodes = new int[ap_episode_count];
        ap_state.player_state.powers = new int[ap_powerup_count];
        ap_state.player_state.weapon_owned = new int[ap_weapon_count];
        ap_state.player_state.ammo = new int[ap_ammo_count];

        memset(ap_state.level_states, 0, sizeof(ap_level_state_t) * ap_episode_count * max_map_count);
        memset(ap_state.episodes, 0, sizeof(int) * ap_episode_count);
        memset(ap_state.player_state.powers, 0, sizeof(int) * ap_powerup_count);
        memset(ap_state.player_state.weapon_owned, 0, sizeof(int) * ap_weapon_count);
        memset(ap_state.player_state.ammo, 0, sizeof(int) * ap_ammo_count);

        ap_state.player_state.health = 100;
        ap_state.player_state.ready_weapon = 1;
        ap_state.player_state.weapon_owned[0] = 1; // Axe
        ap_state.player_state.weapon_owned[1] = 1; // Shotgun
        ap_state.player_state.ammo[0] = 25; // Shells
        for (int ep = 0; ep < ap_episode_count; ++ep)
        {
                int map_count = ap_get_map_count(ep + 1);
                for (int map = 0; map < map_count; ++map)
                {
                        for (int k = 0; k < AP_CHECK_MAX; ++k)
                        {
                                ap_state.level_states[ep * max_map_count + map].checks[k] = -1;
                        }
                }
        }

        ap_settings = *settings;

        if (ap_settings.override_skill)
                ap_state.skill = ap_settings.skill;
        if (ap_settings.override_monster_rando)
                ap_state.random_monsters = ap_settings.monster_rando;
        if (ap_settings.override_item_rando)
                ap_state.random_items = ap_settings.item_rando;
        if (ap_settings.override_music_rando)
                ap_state.random_music = ap_settings.music_rando;
        if (ap_settings.override_reset_level_on_death)
                ap_state.reset_level_on_death = ap_settings.reset_level_on_death;

	AP_NetworkVersion version = {0, 5, 0};
        AP_SetClientVersion(&version);
	AP_Init(ap_settings.ip, ap_settings.game, ap_settings.player_name, ap_settings.passwd);
        AP_SetDeathLinkSupported(ap_settings.force_deathlink_off ? false : true);
        AP_SetItemClearCallback(f_itemclr);
        AP_SetItemRecvCallback(f_itemrecv);
        AP_SetLocationCheckedCallback(f_locrecv);
        AP_SetLocationInfoCallback(f_locinfo);
        AP_RegisterSlotDataIntCallback("goal", f_goal);
        AP_RegisterSlotDataIntCallback("skill", f_skill);
        AP_RegisterSlotDataIntCallback("random_monsters", f_random_monsters);
        AP_RegisterSlotDataIntCallback("random_pickups", f_random_items);
        AP_RegisterSlotDataIntCallback("random_music", f_random_music);
        AP_RegisterSlotDataIntCallback("reset_level_on_death", f_reset_level_on_death);
        AP_RegisterSlotDataIntCallback("episode1", f_episode1);
        AP_RegisterSlotDataIntCallback("episode2", f_episode2);
        AP_RegisterSlotDataIntCallback("episode3", f_episode3);
        AP_RegisterSlotDataIntCallback("episode4", f_episode4);
        AP_RegisterSlotDataIntCallback("episode5", f_episode5);
	AP_Start();

        auto start_time = std::chrono::steady_clock::now();
        while (true)
        {
                bool should_break = false;
                switch (AP_GetConnectionStatus())
                {
                        case AP_ConnectionStatus::Authenticated:
                        {
                                printf("APQUAKE: Authenticated\n");
                                AP_GetRoomInfo(&ap_room_info);

                                printf("APQUAKE: Room Info:\n");
                                printf("  Network Version: %i.%i.%i\n", ap_room_info.version.major, ap_room_info.version.minor, ap_room_info.version.build);
                                printf("  Tags:\n");
                                for (const auto& tag : ap_room_info.tags)
                                        printf("    %s\n", tag.c_str());
                                printf("  Password required: %s\n", ap_room_info.password_required ? "true" : "false");
                                printf("  Permissions:\n");
                                for (const auto& permission : ap_room_info.permissions)
                                        printf("    %s = %i:\n", permission.first.c_str(), permission.second);
                                printf("  Hint cost: %i\n", ap_room_info.hint_cost);
                                printf("  Location check points: %i\n", ap_room_info.location_check_points);
                                printf("  Data package checksums:\n");
                                for (const auto& kv : ap_room_info.datapackage_checksums)
                                        printf("    %s = %s:\n", kv.first.c_str(), kv.second.c_str());
                                printf("  Seed name: %s\n", ap_room_info.seed_name.c_str());
                                printf("  Time: %f\n", ap_room_info.time);

                                ap_was_connected = true;
                                ap_save_dir_name = "AP_" + ap_room_info.seed_name + "_" + string_to_hex(ap_settings.player_name);

                                // Create a directory where saves will go for this AP seed.
                                printf("APQUAKE: Save directory: %s\n", ap_save_dir_name.c_str());
#if 0
                                if (!AP_FileExists(ap_save_dir_name.c_str()))
                                {
                                        printf("  Doesn't exist, creating...\n");
                                        AP_MakeDirectory(ap_save_dir_name.c_str());
                                }
#endif
				if (mkdir(ap_save_dir_name.c_str(), 0755) == -1) {
				    if (errno != EEXIST) {
				        return 0;
				    }
				}

                                load_state();
                                should_break = true;
                                break;
                        }
                        case AP_ConnectionStatus::ConnectionRefused:
                                printf("APQUAKE: Failed to connect, connection refused\n");
                                return 0;
			default: break;
                }
                if (should_break) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(10))
                {
                        printf("APQUAKE: Failed to connect, timeout 10s\n");
                        return 0;
                }
        }

        int ep_count = 0;
        for (int i = 0; i < ap_episode_count; ++i)
                if (ap_state.episodes[i])
                        ep_count++;
        if (!ep_count)
        {
                printf("APQUAKE: No episode selected, selecting episode 1\n");
                ap_state.episodes[0] = 1;
        }

        // Map original music to every level to start
        for (int ep = 0; ep < ap_episode_count; ++ep)
        {
                int map_count = ap_get_map_count(ep + 1);
                for (int map = 0; map < map_count; ++map)
                        ap_state.level_states[ep * max_map_count + map].music = get_original_music_for_level(ep + 1, map + 1);
        }

	// Randomly shuffle music
	if (ap_state.random_music > 0)
	{
		srand(ap_get_rand_seed());
		// Collect music for all selected levels
		std::vector<int> music_pool;
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			if (ap_state.episodes[ep] || ap_state.random_music == 2)
			{
				int map_count = ap_get_map_count(ep + 1);
				for (int map = 0; map < map_count; ++map)
					music_pool.push_back(ap_state.level_states[ep * max_map_count + map].music);
			}
		}

		// Shuffle
		printf("APQuake: Random Music:\n");
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			if (ap_state.episodes[ep])
			{
				int map_count = ap_get_map_count(ep + 1);
				for (int map = 0; map < map_count; ++map)
				{
					int rnd = rand() % (int)music_pool.size();
					int mus = music_pool[rnd];
					music_pool.erase(music_pool.begin() + rnd);
					ap_state.level_states[ep * max_map_count + map].music = mus;

					printf("  E%iM%i = %i\n", ep + 1, map + 1, mus);
				}
			}
		}
		srand(NULL);
	}

        if (ap_progressive_locations.empty())
        {
                std::set<int64_t> location_scouts;

                const auto& loc_table = get_location_table();
                for (const auto& kv1 : loc_table)
                {
                        if (!ap_state.episodes[kv1.first - 1])
                                continue;
                        for (const auto& kv2 : kv1.second)
                        {
                                for (const auto& kv3 : kv2.second)
                                {
                                        if (kv3.first == -1) continue;
                                        if (validate_quake_location({kv1.first - 1, kv2.first - 1}, kv3.first))
                                        {
                                                location_scouts.insert(kv3.second);
                                        }
                                }
                        }
                }

                printf("APQUAKE: Scouting for %i locations...\n", (int)location_scouts.size());
                AP_SendLocationScouts(location_scouts, 0);

                // Wait for location infos
                start_time = std::chrono::steady_clock::now();
                while (ap_progressive_locations.empty())
                {
                        apquake_update();

                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(10))
                        {
                                printf("APQUAKE: Timeout waiting for LocationScouts. 10s\n  Do you have a VPN active?\n  Checks will all look non-progression.");
                                break;
                        }
                }
        }
        else
        {
                printf("APQUAKE: Scout locations cached loaded\n");
        }

        printf("APQUAKE: Initialized\n");
        ap_initialized = true;

	return 1;
}

static int ap_get_map_count(int episode) {
    switch (episode)
    {
        case 1:
	case 4:
	default: return 8;

	case 2:
	case 3: return 7;

	case 5: return 1;
    }
}

ap_level_state_t* ap_get_level_state(ap_level_index_t idx)
{
        return &ap_state.level_states[idx.ep * max_map_count + idx.map];
}

static const std::map<int64_t, ap_item_t>& get_item_type_table()
{
	return ap_quake_item_table;
}

const std::map<std::string, int> quake_keys_map = {{"item_key1", 0}, {"item_key2", 1}};

static const std::map<std::string, int>& get_keys_map()
{
	return quake_keys_map;
}

const std::map<std::string, int> quake_weapons_map = {{"weapon_supershotgun", 2}, {"weapon_nailgun", 3}, {"weapon_supernailgun", 4}, {"weapon_grenadelauncher", 5}, {"weapon_rocketlauncher", 6}, {"weapon_lightning", 7}};

const std::map<std::string, int>& get_weapons_map()
{
	return quake_weapons_map;
}

static bool is_loc_checked(ap_level_index_t idx, int index)
{
        auto level_state = ap_get_level_state(idx);
        for (int i = 0; i < level_state->check_count; ++i)
        {
                if (level_state->checks[i] == index) return true;
        }
        return false;
}

void apquake_shutdown()
{
        if (ap_was_connected)
                save_state();
}


void apquake_save_state()
{
        if (ap_was_connected)
                save_state();
}


static void json_get_int(const Json::Value& json, int& out_or_default)
{
        if (json.isInt())
                out_or_default = json.asInt();
}


static void json_get_bool_or(const Json::Value& json, int& out_or_default)
{
        if (json.isInt())
                out_or_default |= json.asInt();
}


const char* get_weapon_name(int weapon)
{
        switch (weapon)
        {
                case 0: return "Axe";
                case 1: return "Shotgun";
                case 2: return "Super Shotgun";
                case 3: return "Nailgun";
                case 4: return "Super Nailgun";
                case 5: return "Grenade Launcher";
                case 6: return "Rocket Launcher";
                case 7: return "Thunderbolt";
                default: return "UNKNOWN";
        }
}


const char* get_power_name(int weapon)
{
        switch (weapon)
        {
                case 0: return "Megahealth";
                case 1: return "Ring of Shadows";
                case 2: return "Pentagram";
                case 3: return "Biosuit";
                case 4: return "Quad Damage";
                case 5: return "Rune of Earth Magic";
                case 6: return "Rune of Black Magic";
                case 7: return "Rune of Hell Magic";
                case 8: return "Rune of Elder Magic";
                default: return "UNKNOWN";
        }
}


const char* get_ammo_name(int weapon)
{
        switch (weapon)
        {
                case 0: return "Shells";
                case 1: return "Nails";
                case 2: return "Rockets";
                case 3: return "Cells";
                default: return "UNKNOWN";
        }
}

void load_state()
{
	printf("APQUAKE: Load sate\n");

	std::string filename = ap_save_dir_name + "/apstate.json";
	std::ifstream f(filename);
	if (!f.is_open())
	{
		printf("  None found.\n");
		return; // Could be no state yet, that's fine
	}
	Json::Value json;
	f >> json;
	f.close();

	// Player state
	json_get_int(json["player"]["health"], ap_state.player_state.health);
	json_get_int(json["player"]["armor_points"], ap_state.player_state.armor_points);
	json_get_int(json["player"]["armor_type"], ap_state.player_state.armor_type);
	json_get_int(json["player"]["ready_weapon"], ap_state.player_state.ready_weapon);
	json_get_int(json["player"]["kill_count"], ap_state.player_state.kill_count);
	json_get_int(json["player"]["item_count"], ap_state.player_state.item_count);
	json_get_int(json["player"]["secret_count"], ap_state.player_state.secret_count);
	for (int i = 0; i < ap_powerup_count; ++i)
		json_get_int(json["player"]["powers"][i], ap_state.player_state.powers[i]);
	for (int i = 0; i < ap_weapon_count; ++i)
		json_get_bool_or(json["player"]["weapon_owned"][i], ap_state.player_state.weapon_owned[i]);
	for (int i = 0; i < ap_ammo_count; ++i)
		json_get_int(json["player"]["ammo"][i], ap_state.player_state.ammo[i]);
	
	printf("  Player State:\n");
	printf("    Health %i:\n", ap_state.player_state.health);
	printf("    Armor points %i:\n", ap_state.player_state.armor_points);
	printf("    Armor type %i:\n", ap_state.player_state.armor_type);
	printf("    Ready weapon: %s\n", get_weapon_name(ap_state.player_state.ready_weapon));
	printf("    Kill count %i:\n", ap_state.player_state.kill_count);
	printf("    Item count %i:\n", ap_state.player_state.item_count);
	printf("    Secret count %i:\n", ap_state.player_state.secret_count);
	printf("    Active powerups:\n");
	for (int i = 0; i < ap_powerup_count; ++i)
		if (ap_state.player_state.powers[i])
			printf("    %s\n", get_power_name(i));
	printf("    Owned weapons:\n");
	for (int i = 0; i < ap_weapon_count; ++i)
		if (ap_state.player_state.weapon_owned[i])
			printf("      %s\n", get_weapon_name(i));
	printf("    Ammo:\n");
	for (int i = 0; i < ap_ammo_count; ++i)
		printf("      %s = %i\n", get_ammo_name(i), ap_state.player_state.ammo[i]);

	// Level states
	for (int i = 0; i < ap_episode_count; ++i)
	{
		int map_count = ap_get_map_count(i + 1);
		for (int j = 0; j < map_count; ++j)
		{
			auto level_state = ap_get_level_state(ap_level_index_t{i, j});
			json_get_bool_or(json["episodes"][i][j]["completed"], level_state->completed);
			json_get_bool_or(json["episodes"][i][j]["keys0"], level_state->keys[0]);
			json_get_bool_or(json["episodes"][i][j]["keys1"], level_state->keys[1]);
			json_get_bool_or(json["episodes"][i][j]["unlocked"], level_state->unlocked);
		}
	}

	// Item queue
	for (const auto& item_id_json : json["item_queue"])
	{
		ap_item_queue.push_back(item_id_json.asInt64());
	}

	json_get_int(json["ep"], ap_state.ep);
	printf("  Enabled episodes: ");
	int first = 1;
	for (int i = 0; i < ap_episode_count; ++i)
	{
		ap_state.episodes[i] = json["enabled_episodes"][i].asBool();
		if (ap_state.episodes[i])
		{
			if (!first) printf(", ");
			first = 0;
			printf("%i", i);
		}
	}
	printf("\n");
	json_get_int(json["map"], ap_state.map);
	printf("  Episode: %i\n", ap_state.ep);
	printf("  Map: %i\n", ap_state.map);

	for (const auto& prog_json : json["progressive_locations"])
	{
		ap_progressive_locations.insert(prog_json.asInt64());
	}
	
	json_get_bool_or(json["victory"], ap_state.victory);
	printf("  Victory state: %s\n", ap_state.victory ? "true" : "false");
}


static Json::Value serialize_level(int ep, int map)
{
	auto level_state = ap_get_level_state(ap_level_index_t{ep - 1, map - 1});

	Json::Value json_level;

	json_level["completed"] = level_state->completed;
	json_level["keys0"] = level_state->keys[0];
	json_level["keys1"] = level_state->keys[1];
	json_level["check_count"] = level_state->check_count;
	json_level["unlocked"] = level_state->unlocked;

	Json::Value json_checks(Json::arrayValue);
	for (int k = 0; k < AP_CHECK_MAX; ++k)
	{
		if (level_state->checks[k] == -1)
			continue;
		json_checks.append(level_state->checks[k]);
	}
	json_level["checks"] = json_checks;

	return json_level;
}


std::vector<ap_level_index_t> get_level_indices()
{
	std::vector<ap_level_index_t> ret;

	for (int i = 0; i < ap_episode_count; ++i)
	{
		int map_count = ap_get_map_count(i + 1);
		for (int j = 0; j < map_count; ++j)
		{
			ret.push_back({i + 1, j + 1});
		}
	}

	return ret;
}


void save_state()
{
	std::string filename = ap_save_dir_name + "/apstate.json";
	std::ofstream f(filename);
	if (!f.is_open())
	{
		printf("Failed to save AP state.\n");
#if WIN32
		MessageBoxA(nullptr, "Failed to save player state. That's bad.", "Error", MB_OK);
#endif
		return; // Ok that's bad. we won't save player state
	}

	// Player state
	Json::Value json;
	Json::Value json_player;
	json_player["health"] = ap_state.player_state.health;
	json_player["armor_points"] = ap_state.player_state.armor_points;
	json_player["armor_type"] = ap_state.player_state.armor_type;
	json_player["ready_weapon"] = ap_state.player_state.ready_weapon;
	json_player["kill_count"] = ap_state.player_state.kill_count;
	json_player["item_count"] = ap_state.player_state.item_count;
	json_player["secret_count"] = ap_state.player_state.secret_count;

	Json::Value json_powers(Json::arrayValue);
	for (int i = 0; i < ap_powerup_count; ++i)
		json_powers.append(ap_state.player_state.powers[i]);
	json_player["powers"] = json_powers;

	Json::Value json_weapon_owned(Json::arrayValue);
	for (int i = 0; i < ap_weapon_count; ++i)
		json_weapon_owned.append(ap_state.player_state.weapon_owned[i]);
	json_player["weapon_owned"] = json_weapon_owned;

	Json::Value json_ammo(Json::arrayValue);
	for (int i = 0; i < ap_ammo_count; ++i)
		json_ammo.append(ap_state.player_state.ammo[i]);
	json_player["ammo"] = json_ammo;

	json["player"] = json_player;

	// Level states
	Json::Value json_episodes(Json::arrayValue);
	for (int i = 0; i < ap_episode_count; ++i)
	{
		Json::Value json_levels(Json::arrayValue);
		int map_count = ap_get_map_count(i + 1);
		for (int j = 0; j < map_count; ++j)
		{
			json_levels.append(serialize_level(i + 1, j + 1));
		}
		json_episodes.append(json_levels);
	}
	json["episodes"] = json_episodes;

	// Item queue
	Json::Value json_item_queue(Json::arrayValue);
	for (auto item_id : ap_item_queue)
	{
		json_item_queue.append(item_id);
	}
	json["item_queue"] = json_item_queue;

	json["ep"] = ap_state.ep;
	for (int i = 0; i < ap_episode_count; ++i)
		json["enabled_episodes"][i] = ap_state.episodes[i] ? true : false;
	json["map"] = ap_state.map;

	// Progression items (So we don't scout everytime we connect)
	for (auto loc_id : ap_progressive_locations)
	{
		json["progressive_locations"].append(loc_id);
	}

	json["victory"] = ap_state.victory;

	//json["version"] = APQUAKE_VERSION_FULL_TEXT;

	f << json;
}

void f_itemclr()
{
}

std::string get_exmx_name(const std::string& name)
{
        auto pos = name.find_first_of('(');
        if (pos == std::string::npos) return name;
        return name.substr(pos);
}

void f_itemrecv(int64_t item_id, bool notify_player)
{
	const auto& item_type_table = get_item_type_table();
	auto it = item_type_table.find(item_id);
	if (it == item_type_table.end())
		return; // Skip
	ap_item_t item = it->second;
	ap_level_index_t idx = {item.ep - 1, item.map - 1};
	ap_level_info_t* level_info = ap_get_level_info(idx);

	std::string notif_text;

	auto level_state = ap_get_level_state(idx);

	// Key?
	const auto& keys_map = get_keys_map();
	auto key_it = keys_map.find(item.classname);
	if (key_it != keys_map.end())
	{
		level_state->keys[key_it->second] = 1;
		notif_text = get_exmx_name(level_info->name);
	}

	// Weapon?
	const auto& weapons_map = get_weapons_map();
	auto weapon_it = weapons_map.find(item.classname);
	if (weapon_it != weapons_map.end())
		ap_state.player_state.weapon_owned[weapon_it->second] = 1;

	// Is it a level?
	if (item.classname == "level")
	{
		level_state->unlocked = 1;
		notif_text = get_exmx_name(level_info->name);
	}

	// Level complete?
	if (item.classname == "exit")
		level_state->completed = 1;

	if (!notify_player) return;

	if (!ap_is_in_game)
	{
		ap_item_queue.push_back(item_id);
		return;
	}

	// Give item to player
	ap_settings.give_item_callback(item.classname.c_str(), item.spawnflags, item.ep, item.map);
}


bool find_location(int64_t loc_id, int &ep, int &map, int &index)
{
	ep = -1;
	map = -1;
	index = -1;

	const auto& loc_table = get_location_table();
	for (const auto& loc_map_table : loc_table)
	{
		for (const auto& loc_index_table : loc_map_table.second)
		{
			for (const auto& loc_index : loc_index_table.second)
			{
				if (loc_index.second == loc_id)
				{
					ep = loc_map_table.first;
					map = loc_index_table.first;
					index = loc_index.first;
					break;
				}
			}
			if (ep != -1) break;
		}
		if (ep != -1) break;
	}
	return (ep > 0);
}


void f_locrecv(int64_t loc_id)
{
	// Find where this location is
	int ep = -1;
	int map = -1;
	int index = -1;
	if (!find_location(loc_id, ep, map, index))
	{
		printf("APQUAKE: In f_locrecv, loc id not found: %i\n", (int)loc_id);
		return; // Loc not found
	}

	ap_level_index_t idx = {ep - 1, map - 1};

	// Make sure we didn't already check it
	if (is_loc_checked(idx, index)) return;
	if (index == -1 || index == -2) return;

	auto level_state = ap_get_level_state(idx);
	level_state->checks[level_state->check_count] = index;
	level_state->check_count++;
}


void f_locinfo(std::vector<AP_NetworkItem> loc_infos)
{
	for (const auto& loc_info : loc_infos)
	{
		if (loc_info.flags & 1)
			ap_progressive_locations.insert(loc_info.location);
	}
}


void f_goal(int goal)
{
	ap_state.goal = goal;
}


void f_skill(int skill)
{
	if (ap_settings.override_skill)
		return;

	ap_state.skill = skill;
}


void f_random_monsters(int random_monsters)
{
	if (ap_settings.override_monster_rando)
		return;

	ap_state.random_monsters = random_monsters;
}


void f_random_items(int random_items)
{
	if (ap_settings.override_item_rando)
		return;

	ap_state.random_items = random_items;
}


void f_random_music(int random_music)
{
	if (ap_settings.override_music_rando)
		return;

	ap_state.random_music = random_music;
}


void f_reset_level_on_death(int reset_level_on_death)
{
	if (ap_settings.override_reset_level_on_death)
		return;

	ap_state.reset_level_on_death = reset_level_on_death;
}


void f_episode1(int ep)
{
	ap_state.episodes[0] = ep;
}


void f_episode2(int ep)
{
	ap_state.episodes[1] = ep;
}


void f_episode3(int ep)
{
	ap_state.episodes[2] = ep;
}


void f_episode4(int ep)
{
	ap_state.episodes[3] = ep;
}


void f_episode5(int ep)
{
	ap_state.episodes[4] = ep;
}

const char* apquake_get_seed()
{
        return ap_save_dir_name.c_str();
}

void apquake_check_location(ap_level_index_t idx, int index)
{
        int64_t id = 0;
        const auto& loc_table = get_location_table();

        auto it1 = loc_table.find(idx.ep + 1);
        if (it1 == loc_table.end()) return;

        auto it2 = it1->second.find(idx.map + 1);
        if (it2 == it1->second.end()) return;

        auto it3 = it2->second.find(index);
        if (it3 == it2->second.end()) return;

        id = it3->second;

        if (index >= 0)
        {
                if (is_loc_checked(idx, index))
                {
                        printf("APDOOM: Location already checked\n");
                }
                else
                { // We get back from AP
                        //auto level_state = ap_get_level_state(ep, map);
                        //level_state->checks[level_state->check_count] = index;
                        //level_state->check_count++;
                }
        }
        AP_SendItem(id);
}

int apquake_is_location_progression(ap_level_index_t idx, int index)
{
        const auto& loc_table = get_location_table();

        auto it1 = loc_table.find(idx.ep + 1);
        if (it1 == loc_table.end()) return 0;

        auto it2 = it1->second.find(idx.map + 1);
        if (it2 == it1->second.end()) return 0;

        auto it3 = it2->second.find(index);
        if (it3 == it2->second.end()) return 0;

        int64_t id = it3->second;

        return (ap_progressive_locations.find(id) != ap_progressive_locations.end()) ? 1 : 0;
}

void apquake_complete_level(ap_level_index_t idx)
{
	ap_get_level_state(idx)->completed = 1;
        apquake_check_location(idx, -1); // -1 is complete location
}

ap_level_index_t ap_make_level_index(int ep /* 1-based */, int map /* 1-based */)
{
	return { ep - 1, map - 1 };
}

unsigned long long ap_get_rand_seed()
{
    const char *str = apquake_get_seed();
    unsigned long long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

std::string mask_text(std::string text) {
	std::string ret = text;
	for (size_t i = 0; i < ret.length(); i++) {
		ret[i] |= 128;
	}
	return ret;
}

void apquake_update()
{
	if (ap_initialized)
	{
		if (!ap_cached_messages.empty())
		{
			for (const auto& cached_msg : ap_cached_messages)
				ap_settings.message_callback(cached_msg.c_str());
			ap_cached_messages.clear();
		}
	}

	while (AP_IsMessagePending())
	{
		AP_Message* msg = AP_GetLatestMessage();

		std::string colored_msg;

		switch (msg->type)
		{
			case AP_MessageType::ItemSend:
			{
				AP_ItemSendMessage* o_msg = static_cast<AP_ItemSendMessage*>(msg);
				colored_msg = mask_text(o_msg->item) + " was sent to " + mask_text(o_msg->recvPlayer);
				break;
			}
			case AP_MessageType::ItemRecv:
			{
				AP_ItemRecvMessage* o_msg = static_cast<AP_ItemRecvMessage*>(msg);
				colored_msg = "Received " + mask_text(o_msg->item) + " from " + mask_text(o_msg->sendPlayer);
				break;
			}
			case AP_MessageType::Hint:
			{
				AP_HintMessage* o_msg = static_cast<AP_HintMessage*>(msg);
				colored_msg = mask_text(o_msg->item) + " from " + mask_text(o_msg->sendPlayer) + " to " + mask_text(o_msg->recvPlayer) + " at " + mask_text(o_msg->location + (o_msg->checked ? " (Checked)" : " (Unchecked)"));
				break;
			}
			default:
			{
				colored_msg = msg->text;
				break;
			}
		}

		printf("APQUAKE: %s\n", msg->text.c_str());

		if (ap_initialized)
			ap_settings.message_callback(colored_msg.c_str());
		else
			ap_cached_messages.push_back(colored_msg);

		AP_ClearLatestMessage();
	}

	// Check if we're in game, then dequeue the items
	if (ap_is_in_game)
	{
		while (!ap_item_queue.empty())
		{
			auto item_id = ap_item_queue.front();
			ap_item_queue.erase(ap_item_queue.begin());
			f_itemrecv(item_id, true);
		}
	}
}

void apquake_check_victory() {
	if (ap_state.victory) return;

	if (ap_state.goal == 1)
	{
		if (!ap_get_level_state(ap_level_index_t{5, 1})->completed) return;
	}
	else
	{
		// All levels
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			if (!ap_state.episodes[ep]) continue;

			int map_count = ap_get_map_count(ep + 1);
			for (int map = 0; map < map_count; ++map)
			{
				if (!ap_get_level_state(ap_level_index_t{ep, map})->completed) return;
			}
		}
	}

	ap_state.victory = 1;

	AP_StoryComplete();
	ap_settings.victory_callback();
}
