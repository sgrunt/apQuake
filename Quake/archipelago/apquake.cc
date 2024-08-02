#include "apquake.h"

#include <cstring>
#include <fstream>
#include <set>
#include <sys/stat.h>

#include <json/json.h>

ap_state_t ap_state;

static std::string ap_save_dir_name;

static int ap_get_map_count(int episode);

const int ap_episode_count = 5;
const int max_map_count = 8;
const int ap_weapon_count = 8;
const int ap_ammo_count = 4;
const int ap_powerup_count = 9;

static std::vector<int64_t> ap_item_queue;
static bool ap_was_connected = false;
static std::set<int64_t> ap_progressive_locations;

void load_state();

void save_state();

int apquake_init() {
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

        ap_was_connected = true;
	ap_save_dir_name = "AP_00000000000000000000_74657374";
	if (mkdir(ap_save_dir_name.c_str(), 0755) == -1) {
	    if (errno != EEXIST) {
	        return 0;
	    }
	}

	load_state();

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
		json_get_int(json["enabled_episodes"][i], ap_state.episodes[i]);
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
#if 0
	for (auto loc_id : ap_progressive_locations)
	{
		json["progressive_locations"].append(loc_id);
	}
#endif

	json["victory"] = ap_state.victory;

	//json["version"] = APQUAKE_VERSION_FULL_TEXT;

	f << json;
}

const char* apquake_get_seed()
{
        return ap_save_dir_name.c_str();
}
