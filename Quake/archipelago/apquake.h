#ifndef _APQUAKE_
#define _APQUAKE_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    const char* ip;
    const char* game;
    const char* player_name;
    const char* passwd;
    void (*message_callback)(const char*);
    void (*give_item_callback)(const char* classname, int spawnflags, int ep, int map);
    void (*victory_callback)();

    int override_skill; int skill;
    int override_monster_rando; int monster_rando;
    int override_item_rando; int item_rando;
    int override_music_rando; int music_rando;
    int override_flip_levels; int flip_levels;
    int force_deathlink_off;
    int override_reset_level_on_death; int reset_level_on_death;
} ap_settings_t;


#define AP_NOTIF_STATE_PENDING 0
#define AP_NOTIF_STATE_DROPPING 1

#define AP_CHECK_MAX 64 // Arbitrary number
#define AP_MAX_THING 1024 // Twice more than current max for every level

typedef struct
{
    const char* name;
    int keys[2];
    int check_count;

} ap_level_info_t;

typedef struct
{
    int completed;
    int keys[2];
    int check_count;
    int unlocked;
    int checks[AP_CHECK_MAX];
    int music;

} ap_level_state_t;

typedef struct
{
    int type;
    int count;
} ap_inventory_slot_t;

typedef struct
{
    int health;
    int armor_points;
    int armor_type;
    int ready_weapon;
    int kill_count;
    int item_count;
    int secret_count;
    int* powers;
    int* weapon_owned;
    int* ammo;
    ap_inventory_slot_t* inventory;

} ap_player_state_t;

typedef struct
{
    ap_level_state_t* level_states;
    ap_player_state_t player_state;
    int ep;
    int map;
    int skill;
    int random_monsters;
    int random_items;
    int random_music;
    int* episodes;
    int victory;
    int reset_level_on_death;
    int goal;
} ap_state_t;

// Don't construct that manually, use ap_make_level_index()
typedef struct
{
    int ep; // 0-based
    int map; // 0-based
} ap_level_index_t;

extern ap_state_t ap_state;
extern int ap_is_in_game;

int apquake_init(ap_settings_t *settings);
void apquake_save_state();
void apquake_shutdown();

const char* apquake_get_seed();

ap_level_state_t* ap_get_level_state(ap_level_index_t idx); // 1-based

ap_level_info_t* ap_get_level_info(ap_level_index_t idx);

void apquake_check_location(ap_level_index_t idx, int index);

int apquake_is_location_progression(ap_level_index_t idx, int index);

void apquake_complete_level(ap_level_index_t idx);

ap_level_index_t ap_make_level_index(int ep /* 1-based */, int map /* 1-based */);

unsigned long long ap_get_rand_seed();

void apquake_update();

#ifdef __cplusplus
}
#endif

#endif
