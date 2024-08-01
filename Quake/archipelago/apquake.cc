#include "apquake.h"

#include <cstring>

ap_state_t ap_state;

static int ap_get_map_count(int episode);

int apquake_init() {
	const int ap_episode_count = 5;
	const int max_map_count = 8;
	const int ap_weapon_count = 8;
	const int ap_ammo_count = 4;
	const int ap_powerup_count = 9;

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

	return 1;
}

static int ap_get_map_count(int episode) {
    switch (episode)
    {
        case 0:
	case 3:
	default: return 8;

	case 1:
	case 2: return 7;

	case 4: return 1;
    }
}
