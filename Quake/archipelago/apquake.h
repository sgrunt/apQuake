#ifndef _APQUAKE_
#define _APQUAKE_

#ifdef __cplusplus
extern "C"
{
#endif

#define AP_CHECK_MAX 64 // Arbitrary number

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

extern ap_state_t ap_state;

int apquake_init();

#ifdef __cplusplus
}
#endif

#endif
