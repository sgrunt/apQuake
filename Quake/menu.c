/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#include "bgmusic.h"
#include "apquake.h"

void (*vid_menucmdfn) (void); // johnfitz
void (*vid_menukeyfn) (int key);

enum m_state_e m_state;

static void M_Menu_Play_f (void);
static void M_Menu_Keys_f (void);
static void M_Menu_Help_f (void);
static void M_Menu_Ep1_Select_f (void);
static void M_Menu_Ep2_Select_f (void);
static void M_Menu_Ep3_Select_f (void);
static void M_Menu_Ep4_Select_f (void);
static void M_Menu_Victory_f (void);

static void M_Main_Draw (cb_context_t *cbx);
static void M_Play_Draw (cb_context_t *cbx);
static void M_Options_Draw (cb_context_t *cbx);
static void M_Keys_Draw (cb_context_t *cbx);
static void M_Help_Draw (cb_context_t *cbx);
static void M_Quit_Draw (cb_context_t *cbx);
static void M_Ep1_Select_Draw (cb_context_t *cbx);
static void M_Ep2_Select_Draw (cb_context_t *cbx);
static void M_Ep3_Select_Draw (cb_context_t *cbx);
static void M_Ep4_Select_Draw (cb_context_t *cbx);
static void M_Victory_Draw (cb_context_t *cbx);

static void M_Main_Key (int key);
static void M_Play_Key (int key);
static void M_Options_Key (int key);
static void M_Keys_Key (int key);
static void M_Help_Key (int key);
static void M_Quit_Key (int key);
static void M_Ep1_Select_Key (int key);
static void M_Ep2_Select_Key (int key);
static void M_Ep3_Select_Key (int key);
static void M_Ep4_Select_Key (int key);
static void M_Victory_Key (int key);

static void M_SelectLevel (const char *level);
static void M_Kill ();

qboolean m_entersound; // play after drawing a frame, so caching
					   // won't disrupt the sound
qboolean m_recursiveDraw;

qboolean m_is_quitting = false; // prevents SDL_StartTextInput during quit

enum m_state_e m_return_state;
qboolean	   m_return_onerror;
char		   m_return_reason[32];

static int		m_main_cursor;
static qboolean m_mouse_moved;
static qboolean menu_changed;
static int		m_mouse_x = -1;
static int		m_mouse_y = -1;
static int		m_mouse_x_pixels = -1;
static int		m_mouse_y_pixels = -1;

static int scrollbar_x;
static int scrollbar_y;
static int scrollbar_size;

void M_ConfigureNetSubsystem (void);

extern qboolean keydown[256];

extern cvar_t scr_fov;
extern cvar_t scr_showfps;
extern cvar_t scr_style;
extern cvar_t r_rtshadows;
extern cvar_t r_particles;
extern cvar_t r_md5models;
extern cvar_t r_lerpmodels;
extern cvar_t r_lerpmove;
extern cvar_t r_lerpturn;
extern cvar_t vid_filter;
extern cvar_t vid_palettize;
extern cvar_t vid_anisotropic;
extern cvar_t vid_fsaa;
extern cvar_t vid_fsaamode;
extern cvar_t host_maxfps;
extern cvar_t snd_waterfx;
extern cvar_t cl_bob;
extern cvar_t cl_rollangle;
extern cvar_t v_gunkick;

static qboolean slider_grab;
static qboolean scrollbar_grab;

/*
================
M_GetScale
================
*/
float M_GetScale ()
{
	static float latched_menuscale;
	if (!slider_grab)
		latched_menuscale = scr_menuscale.value;
	return latched_menuscale;
}

/*
================
M_PixelToMenuCanvasCoord
================
*/
static void M_PixelToMenuCanvasCoord (int *x, int *y)
{
	float s = q_min ((float)glwidth / 320.0, (float)glheight / 200.0);
	s = CLAMP (1.0, M_GetScale (), s);
	*x = (*x - (glwidth - 320 * s) / 2) / s;
	*y = (*y - (glheight - 200 * s) / 2) / s;
}

#define MENU_OPTION_STRING (str)

/*
================
M_PrintHighlighted
================
*/
void M_PrintHighlighted (cb_context_t *cbx, int cx, int cy, const char *str)
{
	while (*str)
	{
		Draw_Character (cbx, cx, cy, (*str));
		str++;
		cx += CHARACTER_SIZE;
	}
}

/*
================
M_Print
================
*/
void M_Print (cb_context_t *cbx, int cx, int cy, const char *str)
{
	while (*str)
	{
		Draw_Character (cbx, cx, cy, (*str) + 128);
		str++;
		cx += CHARACTER_SIZE;
	}
}

/*
================
M_PrintElided
================
*/
void M_PrintElided (cb_context_t *cbx, int cx, int cy, const char *str, const int max_length)
{
	int i = 0;
	while (str[i] && i < max_length)
	{
		Draw_Character (cbx, cx, cy, str[i] + 128);
		++i;
		cx += CHARACTER_SIZE;
	}
	if (str[i] != 0)
	{
		for (int j = 0; j < 3; ++j)
		{
			Draw_Character (cbx, cx, cy, '.' + 128);
			cx += CHARACTER_SIZE / 2;
		}
	}
}

/*
================
M_PrintWhite
================
*/
void M_PrintWhite (cb_context_t *cbx, int cx, int cy, const char *str)
{
	while (*str)
	{
		Draw_Character (cbx, cx, cy, *str);
		str++;
		cx += CHARACTER_SIZE;
	}
}

/*
================
M_DrawTransPic
================
*/
void M_DrawTransPic (cb_context_t *cbx, int x, int y, qpic_t *pic)
{
	Draw_Pic (cbx, x, y, pic, 1.0f, false); // johnfitz -- simplified becuase centering is handled elsewhere
}

/*
================
M_DrawPic
================
*/
void M_DrawPic (cb_context_t *cbx, int x, int y, qpic_t *pic)
{
	Draw_Pic (cbx, x, y, pic, 1.0f, false); // johnfitz -- simplified becuase centering is handled elsewhere
}

/*
================
M_DrawTextBox
================
*/
static void M_DrawTextBox (cb_context_t *cbx, int x, int y, int width, int lines)
{
	qpic_t *p;
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	M_DrawTransPic (cbx, cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic (cbx, cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp");
	M_DrawTransPic (cbx, cx, cy + 8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		M_DrawTransPic (cbx, cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp");
			M_DrawTransPic (cbx, cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp");
		M_DrawTransPic (cbx, cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp");
	M_DrawTransPic (cbx, cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic (cbx, cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp");
	M_DrawTransPic (cbx, cx, cy + 8, p);
}

/*
================
M_MenuChanged
================
*/
void M_MenuChanged ()
{
	m_entersound = true;
	menu_changed = true;
}

#define SLIDER_SIZE	  10
#define SLIDER_EXTENT ((SLIDER_SIZE - 1) * 8)
#define SLIDER_START  (MENU_SLIDER_X + 4)
#define SLIDER_END	  (SLIDER_START + SLIDER_EXTENT)

/*
================
M_DrawSlider
================
*/
void M_DrawSlider (cb_context_t *cbx, int x, int y, float value)
{
	value = CLAMP (0.0f, value, 1.0f);
	Draw_Character (cbx, x - 8, y, 128);
	for (int i = 0; i < SLIDER_SIZE; i++)
		Draw_Character (cbx, x + i * 8, y, 129);
	Draw_Character (cbx, x + SLIDER_SIZE * 8, y, 130);
	Draw_Character (cbx, x + (SLIDER_SIZE - 1) * 8 * value, y, 131);
}

/*
================
M_GetSliderPos
================
*/
float M_GetSliderPos (float low, float high, float current, qboolean backward, qboolean mouse, float clamped_mouse, int dir, float step, float snap_start)
{
	float f;

	if (mouse)
	{
		if (backward)
			f = high + (low - high) * (clamped_mouse - SLIDER_START) / SLIDER_EXTENT;
		else
			f = low + (high - low) * (clamped_mouse - SLIDER_START) / SLIDER_EXTENT;
	}
	else
	{
		if (backward)
			f = current - dir * step;
		else
			f = current + dir * step;
	}
	if (!mouse || f > snap_start)
		f = (int)(f / step + 0.5f) * step;
	if (f < low)
		f = low;
	else if (f > high)
		f = high;

	return f;
}

/*
================
M_DrawScrollbar
================
*/
void M_DrawScrollbar (cb_context_t *cbx, int x, int y, float value, float size)
{
	scrollbar_x = x;
	scrollbar_y = y - 8;
	scrollbar_size = (size + 2) * CHARACTER_SIZE;
	value = CLAMP (0.0f, value, 1.0f);
	Draw_Character (cbx, x, y - CHARACTER_SIZE, 128 + 256);
	for (int i = 0; i < size; i++)
		Draw_Character (cbx, x, y + i * CHARACTER_SIZE, 129 + 256);
	Draw_Character (cbx, x, y + size * CHARACTER_SIZE, 130 + 256);
	Draw_Character (cbx, x, y + (size - 1) * CHARACTER_SIZE * value, 131 + 256);
}

/*
================
M_DrawCheckbox
================
*/
void M_DrawCheckbox (cb_context_t *cbx, int x, int y, int on)
{
	if (on)
		M_Print (cbx, x, y, "on");
	else
		M_Print (cbx, x, y, "off");
}

//=============================================================================

int m_save_demonum;

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
	M_MenuChanged ();

	if (key_dest == key_menu)
	{
		if (m_state != m_main)
		{
			M_Menu_Main_f ();
			return;
		}

		IN_Activate ();
		key_dest = key_game;
		m_state = m_none;
		return;
	}
	if (key_dest == key_console)
	{
		Con_ToggleConsole_f ();
	}
	else
	{
		M_Menu_Main_f ();
	}
}

/*
================
M_InScrollbar
================
*/
static qboolean M_InScrollbar ()
{
	return scrollbar_grab || (scrollbar_size && (m_mouse_x >= scrollbar_x) && (m_mouse_x <= (scrollbar_x + 8)) && (m_mouse_y >= scrollbar_y) &&
							  (m_mouse_y <= (scrollbar_y + scrollbar_size)) && (m_mouse_y <= (scrollbar_y + scrollbar_size)));
}

/*
================
M_HandleScrollBarKeys
================
*/
qboolean M_HandleScrollBarKeys (const int key, int *cursor, int *first_drawn, const int num_total, const int max_on_screen)
{
	const int prev_cursor = *cursor;
	qboolean  handled_mouse = false;

	if (num_total == 0)
	{
		*cursor = 0;
		*first_drawn = 0;
		return false;
	}

	switch (key)
	{
	case K_MOUSE1:
		if (M_InScrollbar () && ((num_total - max_on_screen) > 0) && !slider_grab)
		{
			handled_mouse = true;
			scrollbar_grab = true;
			int clamped_mouse = CLAMP (scrollbar_y + 8, m_mouse_y, scrollbar_y + scrollbar_size - 8);
			*first_drawn = ((float)clamped_mouse - scrollbar_y - 8) / (scrollbar_size - 16) * (num_total - max_on_screen) + 0.5f;
			if (*cursor < *first_drawn)
				*cursor = *first_drawn;
			else if (*cursor >= *first_drawn + max_on_screen)
				*cursor = *first_drawn + max_on_screen - 1;
		}
		break;

	case K_HOME:
		*cursor = 0;
		*first_drawn = 0;
		break;

	case K_END:
		*cursor = num_total - 1;
		*first_drawn = num_total - max_on_screen;
		break;

	case K_PGUP:
		*cursor = q_max (0, *cursor - max_on_screen);
		*first_drawn = q_max (0, *first_drawn - max_on_screen);
		break;

	case K_PGDN:
		*cursor = q_min (num_total - 1, *cursor + max_on_screen);
		*first_drawn = q_min (*first_drawn + max_on_screen, num_total - max_on_screen);
		break;

	case K_UPARROW:
		if (*cursor == 0)
			*cursor = num_total - 1;
		else
			--*cursor;
		break;

	case K_DOWNARROW:
		if (*cursor == num_total - 1)
			*cursor = 0;
		else
			++*cursor;
		break;

	case K_MWHEELUP:
		*first_drawn = q_max (0, *first_drawn - 1);
		*cursor = q_min (*cursor, *first_drawn + max_on_screen - 1);
		break;

	case K_MWHEELDOWN:
		*first_drawn = q_min (*first_drawn + 1, num_total - max_on_screen);
		*cursor = q_max (*cursor, *first_drawn);
		break;
	}

	if (*cursor != prev_cursor)
		S_LocalSound ("misc/menu1.wav");

	if (num_total <= max_on_screen)
		*first_drawn = 0;
	else
		*first_drawn = CLAMP (*cursor - max_on_screen + 1, *first_drawn, *cursor);

	return handled_mouse;
}

/*
================
M_UpdateCursorForList
================
*/
void M_Mouse_UpdateListCursor (int *cursor, int left, int right, int top, int item_height, int num_items, int scroll_offset)
{
	if (!scrollbar_grab && !slider_grab && m_mouse_moved && (num_items > 0) && (m_mouse_x >= left) && (m_mouse_x <= right) && (m_mouse_y >= top) &&
		(m_mouse_y <= (top + item_height * num_items)))
		*cursor = scroll_offset + CLAMP (0, (m_mouse_y - top) / item_height, num_items - 1);
}

/*
================
M_Mouse_UpdateCursor
================
*/
void M_Mouse_UpdateCursor (int *cursor, int left, int right, int top, int item_height, int index)
{
	if (m_mouse_moved && (m_mouse_x >= left) && (m_mouse_x <= right) && (m_mouse_y >= top) && (m_mouse_y <= (top + item_height)))
		*cursor = index;
}

//=============================================================================
/* MAIN MENU */

#define MAIN_ITEMS 4

void M_Menu_Main_f (void)
{
	M_MenuChanged ();
	if (key_dest != key_menu)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
	IN_Deactivate (true);
	key_dest = key_menu;
	m_state = m_main;
}

void M_Main_Draw (cb_context_t *cbx)
{
	int		f;
	qpic_t *p;

	M_DrawTransPic (cbx, 16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic (cbx, (320 - p->width) / 2, 4, p);

	M_DrawTransPic (cbx, 72, 32, sv.active ? Draw_CachePic ("gfx/apmenu2.lmp") : Draw_CachePic ("gfx/apmenu.lmp"));

	f = (int)(realtime * 10) % 6;

	M_Mouse_UpdateListCursor (&m_main_cursor, 70, 320, 32, 20, MAIN_ITEMS, 0);
	M_DrawTransPic (cbx, 54, 32 + m_main_cursor * 20, Draw_CachePic (va ("gfx/menudot%i.lmp", f + 1)));
}

void M_Main_Key (int key)
{
	switch (key)
	{
	case K_MOUSE2:
	case K_ESCAPE:
	case K_BBUTTON:
		IN_Activate ();
		key_dest = key_game;
		m_state = m_none;
		cls.demonum = m_save_demonum;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		switch (m_main_cursor)
		{
		case 0:
			if (sv.active)
				M_Menu_Options_f ();
			else
				M_Menu_Play_f ();
			break;

		case 1:
			if (sv.active)
				M_Kill ();
			else
				M_Menu_Options_f ();
			break;

		case 2:
			M_Menu_Help_f ();
			break;

		case 3:
			M_Menu_Quit_f ();
			break;
		}
	}
}

static void M_Kill () {
	if (!sv.active)
		return;

	IN_Activate ();
	key_dest = key_game;
	m_state = m_none;
	Cbuf_AddText("kill\n");	
}

//=============================================================================
/* [AP] PLAY MENU */

typedef struct
{
	const char *name;
	const char *description;
} level_t;

static level_t levels[] = {
	{"start", "Entrance"}, // 0

	{"e1m1", "Slipgate Complex"}, // 1
	{"e1m2", "Castle of the Damned"},
	{"e1m3", "The Necropolis"},
	{"e1m4", "The Grisly Grotto"},
	{"e1m5", "Gloom Keep"},
	{"e1m6", "The Door To Chthon"},
	{"e1m7", "The House of Chthon"},
	{"e1m8", "Ziggurat Vertigo"},

	{"e2m1", "The Installation"}, // 9
	{"e2m2", "Ogre Citadel"},
	{"e2m3", "Crypt of Decay"},
	{"e2m4", "The Ebon Fortress"},
	{"e2m5", "The Wizard's Manse"},
	{"e2m6", "The Dismal Oubliette"},
	{"e2m7", "Underearth"},

	{"e3m1", "Termination Central"}, // 16
	{"e3m2", "The Vaults of Zin"},
	{"e3m3", "The Tomb of Terror"},
	{"e3m4", "Satan's Dark Delight"},
	{"e3m5", "Wind Tunnels"},
	{"e3m6", "Chambers of Torment"},
	{"e3m7", "The Haunted Halls"},

	{"e4m1", "The Sewage System"}, // 23
	{"e4m2", "The Tower of Despair"},
	{"e4m3", "The Elder God Shrine"},
	{"e4m4", "The Palace of Hate"},
	{"e4m5", "Hell's Atrium"},
	{"e4m6", "The Pain Maze"},
	{"e4m7", "Azure Agony"},
	{"e4m8", "The Nameless City"},

	{"end", "Shub-Niggurath's Pit"}, // 31

	{"dm1", "Place of Two Deaths"}, // 32
	{"dm2", "Claustrophobopolis"},
	{"dm3", "The Abandoned Base"},
	{"dm4", "The Bad Place"},
	{"dm5", "The Cistern"},
	{"dm6", "The Dark Zone"}};

typedef struct
{
	const char *description;
	int			firstLevel;
	int			levels;
} episode_t;

static episode_t episodes[] = {{"Welcome to Quake", 0, 1}, {"Doomed Dimension", 1, 8}, {"Realm of Black Magic", 9, 7}, {"Netherworld", 16, 7},
							   {"The Elder World", 23, 8}, {"Final Level", 31, 1},	   {"Deathmatch Arena", 32, 6}};

int m_play_cursor;

static int M_Play_NumItems ()
{
	int ret = 0;
	int i;
	for (i = 0; i < 5; i++) {
		if (ap_state.episodes[i])
			ret++;
	}
	return ret;
}

static int GetEpForSlot(int slot) {
	int i = 0, tmpslot = 0;
	for (i = 0; i < 5; i++) {
		if (ap_state.episodes[i]) {
			if (tmpslot == slot)
				return i + 1;
			tmpslot++;
		}
	}
	return 0;
}

static void M_Menu_Play_f (void)
{
	if (sv.active)
		return;
	M_MenuChanged ();
	IN_Deactivate (true);
	key_dest = key_menu;
	m_state = m_play;
}

static char level_description[128];

static const char *M_GetLevelDescription (int ep, int map)
{
	ap_level_index_t lv = ap_make_level_index(ep, map);
	ap_level_state_t *level_state = ap_get_level_state(lv);
	ap_level_info_t *level_info = ap_get_level_info(lv);
	char lockstate = ' ';
	char key1 = ' ';
	char key2 = ' ';
	if (!level_state->unlocked)
		lockstate = '\x01'; // M_Print rotates these by 128
	if (level_state->completed)
		lockstate = '\x0d';
	if (level_info->keys[0])
	{
		if (level_state->keys[0])
			key1 = '\x8b';
		else
			key1 = '\x8f';
	}
	if (level_info->keys[1])
	{
		if (level_state->keys[1])
			key2 = '\x0b';
		else
			key2 = '\x0f';
	}
	if (key1 == ' ' && key2 != ' ') {
		key1 = key2;
		key2 = ' ';
	}
	q_snprintf(level_description, 128, "%c %s (%d/%d) %c%c",
		lockstate,
		levels[episodes[ep].firstLevel + map - 1].description,
		level_state->check_count,
		level_info->check_count,
		key1,
		key2);
	return level_description;
}

static void M_Play_Draw (cb_context_t *cbx)
{
	qpic_t *p;
	const int top = MENU_TOP;
	const int num_items = M_Play_NumItems ();

	M_DrawTransPic (cbx, 16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic (cbx, (320 - p->width) / 2, 4, p);
	//M_DrawTransPic (cbx, 72, 32, Draw_CachePic ("gfx/sp_menu.lmp"));

	for (int i = 0; i < num_items; i++)
	{
		int ep = GetEpForSlot(i);
		const int y = top + i * CHARACTER_SIZE;
		if (ep == 5)
			M_Print (cbx, MENU_LABEL_X, y, M_GetLevelDescription(5, 1));
		else
			M_Print (cbx, MENU_LABEL_X, y, episodes[ep].description);
	}

	M_Mouse_UpdateListCursor (&m_play_cursor, MENU_CURSOR_X, 320, top, CHARACTER_SIZE, num_items, 0);
	Draw_Character (cbx, MENU_CURSOR_X, top + (m_play_cursor) * CHARACTER_SIZE, 12 + ((int)(realtime * 4) & 1));
}

static void M_Play_Key (int key)
{
	switch (key)
	{
	case K_MOUSE2:
	case K_ESCAPE:
	case K_BBUTTON:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_play_cursor >= M_Play_NumItems ())
			m_play_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_play_cursor < 0)
			m_play_cursor = M_Play_NumItems () - 1;
		break;

	case K_MOUSE1:
	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
		m_entersound = true;

		switch (GetEpForSlot(m_play_cursor))
		{
		case 1:
			M_Menu_Ep1_Select_f();
			break;

		case 2:
			M_Menu_Ep2_Select_f();
			break;

		case 3:
			M_Menu_Ep3_Select_f();
			break;

		case 4:
			M_Menu_Ep4_Select_f();
			break;

		case 5:
			M_SelectLevel("end");
			break;
		}
	}
}

void Host_Loadgame_f(const char *savename);
ap_level_index_t get_level_for_map_name(const char *mapname);

static void M_SelectLevel(const char *level)
{
	char filename[260];
	FILE* file;

	ap_level_state_t* level_state = ap_get_level_state(get_level_for_map_name(level));
	if (!level_state->unlocked) {
		m_entersound = false;
		S_LocalSound("doors/medtry.wav");
		return;
	}

        IN_Activate ();
        key_dest = key_game;
	Cbuf_AddText ("maxplayers 1\n");
	Cbuf_AddText ("deathmatch 0\n");
	Cbuf_AddText ("coop 0\n");
	Cbuf_AddText ( va ( "skill %d\n", ap_state.skill ) );

	snprintf(filename, 260, "%s/%s.sav", apquake_get_seed(), level);
	file = fopen(filename, "rb");
	if (file) {
		fclose(file);
		Host_Loadgame_f(level);
	} else {
		Cbuf_AddText ( va ( "map %s\n", level ) );
	}
}

//=============================================================================
/* [AP] LEVEL SELECT MENUS */

static int m_ep1_select_cursor = 0;

static void M_Menu_Ep1_Select_f (void)
{
	if (sv.active)
		return;
	IN_Deactivate (true);
	key_dest = key_menu;
	m_state = m_ep1_select;
	m_entersound = true;
}

static void M_Ep1_Select_Draw (cb_context_t *cbx)
{
	qpic_t *p;
	const int top = MENU_TOP;

	M_DrawTransPic (cbx, 16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic (cbx, (320 - p->width) / 2, 4, p);

	for (int i = 0; i < episodes[1].levels; i++)
	{
		const int y = top + i * CHARACTER_SIZE;
		M_Print (cbx, MENU_LABEL_X, y, M_GetLevelDescription(1, i + 1));
	}

	M_Mouse_UpdateListCursor (&m_ep1_select_cursor, MENU_CURSOR_X, 320, top, CHARACTER_SIZE, episodes[1].levels, 0);
	Draw_Character (cbx, MENU_CURSOR_X, top + (m_ep1_select_cursor) * CHARACTER_SIZE, 12 + ((int)(realtime * 4) & 1));
}

static void M_Ep1_Select_Key (int key)
{
	switch (key)
	{
	case K_MOUSE2:
	case K_ESCAPE:
	case K_BBUTTON:
		M_Menu_Play_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_ep1_select_cursor >= episodes[1].levels)
			m_ep1_select_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_ep1_select_cursor < 0)
			m_ep1_select_cursor = episodes[1].levels - 1;
		break;

	case K_MOUSE1:
	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
		m_entersound = true;
		M_SelectLevel(levels[episodes[1].firstLevel + m_ep1_select_cursor].name);
		break;
	}
}

static int m_ep2_select_cursor = 0;

static void M_Menu_Ep2_Select_f (void)
{
	if (sv.active)
		return;
	IN_Deactivate (true);
	key_dest = key_menu;
	m_state = m_ep2_select;
	m_entersound = true;
}

static void M_Ep2_Select_Draw (cb_context_t *cbx)
{
	qpic_t *p;
	const int top = MENU_TOP;

	M_DrawTransPic (cbx, 16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic (cbx, (320 - p->width) / 2, 4, p);

	for (int i = 0; i < episodes[2].levels; i++)
	{
		const int y = top + i * CHARACTER_SIZE;
		M_Print (cbx, MENU_LABEL_X, y, M_GetLevelDescription(2, i + 1));
	}

	M_Mouse_UpdateListCursor (&m_ep2_select_cursor, MENU_CURSOR_X, 320, top, CHARACTER_SIZE, episodes[2].levels, 0);
	Draw_Character (cbx, MENU_CURSOR_X, top + (m_ep2_select_cursor) * CHARACTER_SIZE, 12 + ((int)(realtime * 4) & 1));
}

static void M_Ep2_Select_Key (int key)
{
	switch (key)
	{
	case K_MOUSE2:
	case K_ESCAPE:
	case K_BBUTTON:
		M_Menu_Play_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_ep2_select_cursor >= episodes[2].levels)
			m_ep2_select_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_ep2_select_cursor < 0)
			m_ep2_select_cursor = episodes[2].levels - 1;
		break;

	case K_MOUSE1:
	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
		m_entersound = true;
		M_SelectLevel(levels[episodes[2].firstLevel + m_ep2_select_cursor].name);
		break;
	}
}

static int m_ep3_select_cursor = 0;

static void M_Menu_Ep3_Select_f (void)
{
	if (sv.active)
		return;
	IN_Deactivate (true);
	key_dest = key_menu;
	m_state = m_ep3_select;
	m_entersound = true;
}

static void M_Ep3_Select_Draw (cb_context_t *cbx)
{
	qpic_t *p;
	const int top = MENU_TOP;

	M_DrawTransPic (cbx, 16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic (cbx, (320 - p->width) / 2, 4, p);

	for (int i = 0; i < episodes[3].levels; i++)
	{
		const int y = top + i * CHARACTER_SIZE;
		M_Print (cbx, MENU_LABEL_X, y, M_GetLevelDescription(3, i + 1));
	}

	M_Mouse_UpdateListCursor (&m_ep3_select_cursor, MENU_CURSOR_X, 320, top, CHARACTER_SIZE, episodes[3].levels, 0);
	Draw_Character (cbx, MENU_CURSOR_X, top + (m_ep3_select_cursor) * CHARACTER_SIZE, 12 + ((int)(realtime * 4) & 1));
}

static void M_Ep3_Select_Key (int key)
{
	switch (key)
	{
	case K_MOUSE2:
	case K_ESCAPE:
	case K_BBUTTON:
		M_Menu_Play_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_ep3_select_cursor >= episodes[3].levels)
			m_ep3_select_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_ep3_select_cursor < 0)
			m_ep3_select_cursor = episodes[3].levels - 1;
		break;

	case K_MOUSE1:
	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
		m_entersound = true;
		M_SelectLevel(levels[episodes[3].firstLevel + m_ep3_select_cursor].name);
		break;
	}
}

static int m_ep4_select_cursor = 0;

static void M_Menu_Ep4_Select_f (void)
{
	if (sv.active)
		return;
	IN_Deactivate (true);
	key_dest = key_menu;
	m_state = m_ep4_select;
	m_entersound = true;
}

static void M_Ep4_Select_Draw (cb_context_t *cbx)
{
	qpic_t *p;
	const int top = MENU_TOP;

	M_DrawTransPic (cbx, 16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic (cbx, (320 - p->width) / 2, 4, p);

	for (int i = 0; i < episodes[4].levels; i++)
	{
		const int y = top + i * CHARACTER_SIZE;
		M_Print (cbx, MENU_LABEL_X, y, M_GetLevelDescription(4, i + 1));
	}

	M_Mouse_UpdateListCursor (&m_ep4_select_cursor, MENU_CURSOR_X, 320, top, CHARACTER_SIZE, episodes[4].levels, 0);
	Draw_Character (cbx, MENU_CURSOR_X, top + (m_ep4_select_cursor) * CHARACTER_SIZE, 12 + ((int)(realtime * 4) & 1));
}

static void M_Ep4_Select_Key (int key)
{
	switch (key)
	{
	case K_MOUSE2:
	case K_ESCAPE:
	case K_BBUTTON:
		M_Menu_Play_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_ep4_select_cursor >= episodes[4].levels)
			m_ep4_select_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_ep4_select_cursor < 0)
			m_ep4_select_cursor = episodes[4].levels - 1;
		break;

	case K_MOUSE1:
	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
		m_entersound = true;
		M_SelectLevel(levels[episodes[4].firstLevel + m_ep4_select_cursor].name);
		break;
	}
}

//=============================================================================
/* GAME OPTIONS MENU */

enum
{
	GAME_OPT_SCALE,
	GAME_OPT_SBALPHA,
	GAME_OPT_MOUSESPEED,
	GAME_OPT_VIEWBOB,
	GAME_OPT_VIEWROLL,
	GAME_OPT_GUNKICK,
	GAME_OPT_SHOWGUN,
	GAME_OPT_ALWAYRUN,
	GAME_OPT_INVMOUSE,
	GAME_OPT_HUD_DETAIL,
	GAME_OPT_HUD_STYLE,
	GAME_OPT_CROSSHAIR,
	GAME_OPT_STARTUP_DEMOS,
	GAME_OPT_SHOWFPS,
	GAME_OPTIONS_ITEMS
};

#define GAME_OPTIONS_PER_PAGE MAX_MENU_LINES
static int game_options_cursor = 0;
static int first_game_option = 0;

static void M_Menu_GameOptions_f (void)
{
	IN_Deactivate (true);
	key_dest = key_menu;
	m_state = m_game;
	m_entersound = true;
}

static void M_GameOptions_AdjustSliders (int dir, qboolean mouse)
{
	if (scrollbar_grab)
		return;

	float f, clamped_mouse = CLAMP (SLIDER_START, (float)m_mouse_x, SLIDER_END);

	if (fabsf (clamped_mouse - (float)m_mouse_x) > 12.0f)
		mouse = false;

	if (dir)
		S_LocalSound ("misc/menu3.wav");

	if (mouse)
		slider_grab = true;

	switch (game_options_cursor)
	{
	case GAME_OPT_SCALE: // console and menu scale
		if (scr_relativescale.value)
		{
			f = M_GetSliderPos (1, 3.0f, scr_relativescale.value, false, mouse, clamped_mouse, dir, 0.1, 999);
			Cvar_SetValue ("scr_relativescale", f);
		}
		else
		{
			f = M_GetSliderPos (1, ((vid.width + 31) / 32) / 10.0, scr_conscale.value, false, mouse, clamped_mouse, dir, 0.1, 999);
			Cvar_SetValue ("scr_conscale", f);
			Cvar_SetValue ("scr_menuscale", f);
			Cvar_SetValue ("scr_sbarscale", f);
		}
		break;
	case GAME_OPT_MOUSESPEED: // mouse speed
		f = M_GetSliderPos (1, 11, sensitivity.value, false, mouse, clamped_mouse, dir, 0.5, 999);
		Cvar_SetValue ("sensitivity", f);
		break;
	case GAME_OPT_SBALPHA: // statusbar alpha
		f = M_GetSliderPos (0, 1, 1.0f - scr_sbaralpha.value, true, mouse, clamped_mouse, dir, 0.05, 999);
		Cvar_SetValue ("scr_sbaralpha", 1.0f - f);
		break;
	case GAME_OPT_VIEWBOB: // statusbar alpha
		f = (1.0f - M_GetSliderPos (0, 1, 1.0f - (cl_bob.value * 20.0f), true, mouse, clamped_mouse, dir, 0.05, 999)) / 20.0f;
		Cvar_SetValue ("cl_bob", f);
		break;
	case GAME_OPT_VIEWROLL: // statusbar alpha
		f = (1.0f - M_GetSliderPos (0, 1, 1.0f - (cl_rollangle.value * 0.2f), true, mouse, clamped_mouse, dir, 0.05, 999)) / 0.2f;
		Cvar_SetValue ("cl_rollangle", f);
		break;
	case GAME_OPT_GUNKICK: // gun kick
		Cvar_SetValue ("v_gunkick", ((int)v_gunkick.value + 3 + dir) % 3);
		break;
	case GAME_OPT_SHOWGUN: // gun kick
		Cvar_SetValue ("r_drawviewmodel", ((int)r_drawviewmodel.value + 2 + dir) % 2);
		break;
	case GAME_OPT_ALWAYRUN: // always run
		Cvar_SetValue ("cl_alwaysrun", !(cl_alwaysrun.value || cl_forwardspeed.value > 200));

		// The past vanilla "always run" option set these two CVARs, so reset them too when
		// changing in case the user previously used that option.
		Cvar_SetValue ("cl_forwardspeed", 200);
		Cvar_SetValue ("cl_backspeed", 200);
		break;
	case GAME_OPT_INVMOUSE:
		Cvar_SetValue ("m_pitch", -m_pitch.value);
		break;
	case GAME_OPT_CROSSHAIR:
		Cvar_SetValue ("crosshair", ((int)crosshair.value + 3 + dir) % 3);
		break;
	case GAME_OPT_HUD_DETAIL: // interface detail
		// cycles through 120 (none), 110 (standard), 100 (full)
		if (scr_viewsize.value <= 100.0f)
			Cvar_SetValue ("viewsize", dir < 0 ? 110.0f : 120.0f);
		else if (scr_viewsize.value <= 110.0f)
			Cvar_SetValue ("viewsize", dir < 0 ? 120.0f : 100.0f);
		else
			Cvar_SetValue ("viewsize", dir < 0 ? 100.0f : 110.0f);
		break;
	case GAME_OPT_HUD_STYLE:
		Cvar_SetValue ("scr_style", ((int)scr_style.value + 3 + dir) % 3);
		break;
	case GAME_OPT_STARTUP_DEMOS:
		Cvar_SetValue ("cl_startdemos", ((int)cl_startdemos.value + 2 + dir) % 2);
		break;
	case GAME_OPT_SHOWFPS:
		Cvar_SetValue ("scr_showfps", ((int)scr_showfps.value + 2 + dir) % 2);
		break;
	}
}

static void M_GameOptions_Key (int k)
{
	if (M_HandleScrollBarKeys (k, &game_options_cursor, &first_game_option, GAME_OPTIONS_ITEMS, GAME_OPTIONS_PER_PAGE))
		return;

	switch (k)
	{
	case K_MOUSE2:
	case K_ESCAPE:
	case K_BBUTTON:
		M_Menu_Options_f ();
		break;

	case K_MOUSE1:
	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
		m_entersound = true;
		M_GameOptions_AdjustSliders (1, k == K_MOUSE1);
		return;

	case K_LEFTARROW:
		M_GameOptions_AdjustSliders (-1, false);
		break;

	case K_RIGHTARROW:
		M_GameOptions_AdjustSliders (1, false);
		break;
	}
}
static void M_GameOptions_Draw (cb_context_t *cbx)
{
	float	  l, r;
	qpic_t	 *p;
	const int top = MENU_TOP;

	M_DrawTransPic (cbx, 16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic (cbx, (320 - p->width) / 2, 4, p);

	// Draw the items in the order of the enum defined above:

	for (int i = 0; i < GAME_OPTIONS_PER_PAGE && i < (int)GAME_OPTIONS_ITEMS; i++)
	{
		const int y = top + i * CHARACTER_SIZE;
		switch (i + first_game_option)
		{
		case GAME_OPT_SCALE:
			M_Print (cbx, MENU_LABEL_X, y, "Interface Scale");
			l = scr_relativescale.value ? 2.0f : ((vid.width / 320.0) - 1);
			r = l > 0 ? ((scr_relativescale.value ? scr_relativescale.value : scr_conscale.value) - 1) / l : 0;
			M_DrawSlider (cbx, MENU_SLIDER_X, y, r);
			break;

		case GAME_OPT_SBALPHA:
			M_Print (cbx, MENU_LABEL_X, y, "HUD Opacity");
			r = scr_sbaralpha.value; // scr_sbaralpha range is 1.0 to 0.0
			M_DrawSlider (cbx, MENU_SLIDER_X, y, r);
			break;

		case GAME_OPT_MOUSESPEED:
			M_Print (cbx, MENU_LABEL_X, y, "Mouse Speed");
			r = (sensitivity.value - 1) / 10;
			M_DrawSlider (cbx, MENU_SLIDER_X, y, r);
			break;

		case GAME_OPT_VIEWBOB:
			M_Print (cbx, MENU_LABEL_X, y, "View Bob");
			r = cl_bob.value * 20.0f;
			M_DrawSlider (cbx, MENU_SLIDER_X, y, r);
			break;

		case GAME_OPT_VIEWROLL:
			M_Print (cbx, MENU_LABEL_X, y, "View Roll");
			r = cl_rollangle.value * 0.2f;
			M_DrawSlider (cbx, MENU_SLIDER_X, y, r);
			break;

		case GAME_OPT_GUNKICK:
			M_Print (cbx, MENU_LABEL_X, y, "Gun Kick");
			M_Print (cbx, MENU_VALUE_X, y, (v_gunkick.value == 2) ? "smooth" : (v_gunkick.value == 1) ? "classic" : "off");
			break;

		case GAME_OPT_SHOWGUN:
			M_Print (cbx, MENU_LABEL_X, y, "Show Gun");
			M_DrawCheckbox (cbx, MENU_VALUE_X, y, r_drawviewmodel.value);
			break;

		case GAME_OPT_ALWAYRUN:
			M_Print (cbx, MENU_LABEL_X, y, "Always Run");
			M_DrawCheckbox (cbx, MENU_VALUE_X, y, cl_alwaysrun.value || cl_forwardspeed.value > 200.0);
			break;

		case GAME_OPT_INVMOUSE:
			M_Print (cbx, MENU_LABEL_X, y, "Invert Mouse");
			M_DrawCheckbox (cbx, MENU_VALUE_X, y, m_pitch.value < 0);
			break;

		case GAME_OPT_HUD_DETAIL:
			M_Print (cbx, MENU_LABEL_X, y, "HUD Detail");
			if (scr_viewsize.value >= 120.0f)
				M_Print (cbx, MENU_VALUE_X, y, "None");
			else if (scr_viewsize.value >= 110.0f)
				M_Print (cbx, MENU_VALUE_X, y, "Minimal");
			else
				M_Print (cbx, MENU_VALUE_X, y, "Full");
			break;

		case GAME_OPT_HUD_STYLE:
			M_Print (cbx, MENU_LABEL_X, y, "HUD Style");
			if (scr_style.value < 1.0f)
				M_Print (cbx, MENU_VALUE_X, y, "Mod");
			else if (scr_style.value < 2.0f)
				M_Print (cbx, MENU_VALUE_X, y, "Classic");
			else
				M_Print (cbx, MENU_VALUE_X, y, "Modern");
			break;

		case GAME_OPT_CROSSHAIR:
			M_Print (cbx, MENU_LABEL_X, y, "Crosshair");
			if (crosshair.value == 0.0f)
				M_Print (cbx, MENU_VALUE_X, y, "off");
			else if (crosshair.value < 2.0f)
				M_PrintHighlighted (cbx, MENU_VALUE_X, y, "+");
			else
				M_PrintHighlighted (cbx, MENU_VALUE_X + 2, y - 1, ".");
			break;

		case GAME_OPT_STARTUP_DEMOS:
			M_Print (cbx, MENU_LABEL_X, y, "Startup Demos");
			M_DrawCheckbox (cbx, MENU_VALUE_X, y, cl_startdemos.value);
			break;

		case GAME_OPT_SHOWFPS:
			M_Print (cbx, MENU_LABEL_X, y, "Show FPS");
			M_DrawCheckbox (cbx, MENU_VALUE_X, y, scr_showfps.value);
			break;
		}
	}

#if (GAME_OPTIONS_ITEMS > GAME_OPTIONS_PER_PAGE)
		M_DrawScrollbar (
			cbx, MENU_SCROLLBAR_X, MENU_TOP + CHARACTER_SIZE, (float)(first_game_option) / (GAME_OPTIONS_ITEMS - GAME_OPTIONS_PER_PAGE),
			GAME_OPTIONS_PER_PAGE - 2);
#endif

	// cursor
	M_Mouse_UpdateListCursor (&game_options_cursor, MENU_CURSOR_X, 320, top, CHARACTER_SIZE, GAME_OPTIONS_PER_PAGE, first_game_option);
	Draw_Character (cbx, MENU_CURSOR_X, top + (game_options_cursor - first_game_option) * CHARACTER_SIZE, 12 + ((int)(realtime * 4) & 1));
}

//=============================================================================
/* GRAPHICS OPTIONS MENU */

enum
{
	GRAPHICS_OPT_GAMMA,
	GRAPHICS_OPT_CONTRAST,
	GRAPHICS_OPT_FOV,
	GRAPHICS_OPT_8BIT_COLOR,
	GRAPHICS_OPT_FILTER,
	GRAPHICS_OPT_MAX_FPS,
	GRAPHICS_OPT_ANTIALIASING_SAMPLES,
	GRAPHICS_OPT_ANTIALIASING_MODE,
	GRAPHICS_OPT_RENDER_SCALE,
	GRAPHICS_OPT_ANISOTROPY,
	GRAPHICS_OPT_UNDERWATER,
	GRAPHICS_OPT_MODELS,
	GRAPHICS_OPT_MODEL_INTERPOLATION,
	GRAPHICS_OPT_PARTICLES,
	GRAPHICS_OPT_SHADOWS,
	GRAPHICS_OPTIONS_ITEMS,
};

static int M_GraphicsOptions_NumItems ()
{
	return GRAPHICS_OPTIONS_ITEMS - (vulkan_globals.ray_query ? 0 : 1);
}

static int graphics_options_cursor = 0;

static void M_Menu_GraphicsOptions_f (void)
{
	IN_Deactivate (true);
	key_dest = key_menu;
	m_state = m_graphics;
	m_entersound = true;
}

static void M_GraphicsOptions_ChooseNextAASamples (int dir)
{
	int value = vid_fsaa.value;

	if (dir > 0)
	{
		if (value >= 16)
			value = 0;
		else if (value >= 8)
			value = 16;
		else if (value >= 4)
			value = 8;
		else if (value >= 2)
			value = 4;
		else
			value = 2;
	}
	else
	{
		if (value <= 0)
			value = 16;
		else if (value <= 2)
			value = 0;
		else if (value <= 4)
			value = 2;
		else if (value <= 8)
			value = 4;
		else if (value <= 16)
			value = 8;
		else
			value = 16;
	}

	Cvar_SetValueQuick (&vid_fsaa, (float)value);
}

static void M_GraphicsOptions_ChooseNextRenderScale (int dir)
{
	int value = r_scale.value;

	if (dir > 0)
	{
		if (value >= 8)
			value = 0;
		else if (value >= 4)
			value = 8;
		else if (value >= 2)
			value = 4;
		else
			value = 2;
	}
	else
	{
		if (value <= 0)
			value = 8;
		else if (value <= 2)
			value = 0;
		else if (value <= 4)
			value = 2;
		else if (value <= 8)
			value = 4;
		else
			value = 8;
	}

	Cvar_SetValueQuick (&r_scale, (float)value);
}

static void M_GraphicsOptions_ChooseNextParticles (int dir)
{
	int value = r_particles.value;

	if (dir > 0)
	{
		if (value == 0)
			value = 2;
		else if (value == 2)
			value = 1;
		else
			value = 0;
	}
	else
	{
		if (value == 0)
			value = 1;
		else if (value == 2)
			value = 0;
		else
			value = 2;
	}

	Cvar_SetValueQuick (&r_particles, (float)value);
}

static void M_GraphicsOptions_AdjustSliders (int dir, qboolean mouse)
{
	float f, clamped_mouse = CLAMP (SLIDER_START, (float)m_mouse_x, SLIDER_END);

	if (fabsf (clamped_mouse - (float)m_mouse_x) > 12.0f)
		mouse = false;

	if (dir)
		S_LocalSound ("misc/menu3.wav");

	if (mouse)
		slider_grab = true;

	switch (graphics_options_cursor)
	{
	case GRAPHICS_OPT_GAMMA:
		f = M_GetSliderPos (0.5, 1, vid_gamma.value, true, mouse, clamped_mouse, dir, 0.05, 999);
		Cvar_SetValue ("gamma", f);
		break;
	case GRAPHICS_OPT_CONTRAST:
		f = M_GetSliderPos (1, 2, vid_contrast.value, false, mouse, clamped_mouse, dir, 0.1, 999);
		Cvar_SetValue ("contrast", f);
		break;
	case GRAPHICS_OPT_FOV:
		f = M_GetSliderPos (80, 130, scr_fov.value, false, mouse, clamped_mouse, dir, 5, 999);
		Cvar_SetValue ("fov", f);
		break;
	case GRAPHICS_OPT_8BIT_COLOR:
		Cvar_SetValueQuick (&vid_palettize, (float)(((int)vid_palettize.value + 2 + dir) % 2));
		break;
	case GRAPHICS_OPT_FILTER:
		Cvar_SetValueQuick (&vid_filter, (float)(((int)vid_filter.value + 2 + dir) % 2));
		break;
	case GRAPHICS_OPT_MAX_FPS:
		Cvar_SetValueQuick (&host_maxfps, (float)CLAMP (0, (((int)host_maxfps.value + (dir * 10)) / 10) * 10, 1000));
		break;
	case GRAPHICS_OPT_ANTIALIASING_SAMPLES:
		M_GraphicsOptions_ChooseNextAASamples (dir);
		Cbuf_AddText ("vid_restart\n");
		break;
	case GRAPHICS_OPT_ANTIALIASING_MODE:
		if (vulkan_globals.device_features.sampleRateShading)
			Cvar_SetValueQuick (&vid_fsaamode, (float)(((int)vid_fsaamode.value + 2 + dir) % 2));
		break;
	case GRAPHICS_OPT_RENDER_SCALE:
		M_GraphicsOptions_ChooseNextRenderScale (dir);
		break;
	case GRAPHICS_OPT_ANISOTROPY:
		Cvar_SetValueQuick (&vid_anisotropic, (float)(((int)vid_anisotropic.value + 2 + dir) % 2));
		break;
	case GRAPHICS_OPT_UNDERWATER:
		Cvar_SetValueQuick (&r_waterwarp, (float)(((int)r_waterwarp.value + 3 + dir) % 3));
		break;
	case GRAPHICS_OPT_MODELS:
		Cvar_SetValueQuick (&r_md5models, (float)(((int)r_md5models.value + 2 + dir) % 2));
		break;
	case GRAPHICS_OPT_MODEL_INTERPOLATION:
		Cvar_SetValueQuick (&r_lerpmodels, (float)(((int)r_lerpmodels.value + 2 + dir) % 2));
		Cvar_SetValueQuick (&r_lerpmove, r_lerpmodels.value);
		Cvar_SetValueQuick (&r_lerpturn, r_lerpmodels.value);
		break;
	case GRAPHICS_OPT_PARTICLES:
		M_GraphicsOptions_ChooseNextParticles (dir);
		break;
	case GRAPHICS_OPT_SHADOWS:
		if (vulkan_globals.ray_query)
			Cvar_SetValueQuick (&r_rtshadows, (float)(((int)r_rtshadows.value + 2 + dir) % 2));
		break;
	}
}

static void M_GraphicsOptions_Key (int k)
{
	switch (k)
	{
	case K_MOUSE2:
	case K_ESCAPE:
	case K_BBUTTON:
		M_Menu_Options_f ();
		break;

	case K_MOUSE1:
	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
		m_entersound = true;
		M_GraphicsOptions_AdjustSliders (1, k == K_MOUSE1);
		return;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		graphics_options_cursor--;
		if (graphics_options_cursor < 0)
			graphics_options_cursor = M_GraphicsOptions_NumItems () - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		graphics_options_cursor++;
		if (graphics_options_cursor >= M_GraphicsOptions_NumItems ())
			graphics_options_cursor = 0;
		break;

	case K_LEFTARROW:
		M_GraphicsOptions_AdjustSliders (-1, false);
		break;

	case K_RIGHTARROW:
		M_GraphicsOptions_AdjustSliders (1, false);
		break;
	}
}
static void M_GraphicsOptions_Draw (cb_context_t *cbx)
{
	float	  r = 0.0f;
	qpic_t	 *p;
	const int top = MENU_TOP;

	M_DrawTransPic (cbx, 16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic (cbx, (320 - p->width) / 2, 4, p);

	// Draw the items in the order of the enum defined above:
	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_GAMMA, "Gamma");
	r = (1.0 - vid_gamma.value) / 0.5;
	M_DrawSlider (cbx, MENU_SLIDER_X, top + CHARACTER_SIZE * GRAPHICS_OPT_GAMMA, r);

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_CONTRAST, "Contrast");
	r = vid_contrast.value - 1.0;
	M_DrawSlider (cbx, MENU_SLIDER_X, top + CHARACTER_SIZE * GRAPHICS_OPT_CONTRAST, r);

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_FOV, "Field of View");
	r = (scr_fov.value - 80) / (130 - 80);
	M_DrawSlider (cbx, MENU_SLIDER_X, top + CHARACTER_SIZE * GRAPHICS_OPT_FOV, r);

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_8BIT_COLOR, "8-bit Color");
	M_DrawCheckbox (cbx, MENU_VALUE_X, top + CHARACTER_SIZE * GRAPHICS_OPT_8BIT_COLOR, vid_palettize.value);

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_FILTER, "Textures");
	M_Print (cbx, MENU_VALUE_X, top + CHARACTER_SIZE * GRAPHICS_OPT_FILTER, (vid_filter.value == 0) ? "smooth" : "classic");

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_MAX_FPS, "Max FPS");
	if (host_maxfps.value <= 0)
		M_Print (cbx, MENU_VALUE_X, top + CHARACTER_SIZE * GRAPHICS_OPT_MAX_FPS, "no limit");
	else
		M_Print (cbx, MENU_VALUE_X, top + CHARACTER_SIZE * GRAPHICS_OPT_MAX_FPS, va ("%" SDL_PRIu32, q_min ((uint32_t)host_maxfps.value, (uint32_t)1000)));

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_ANTIALIASING_SAMPLES, "Antialiasing");
	M_Print (
		cbx, MENU_VALUE_X, top + CHARACTER_SIZE * GRAPHICS_OPT_ANTIALIASING_SAMPLES,
		((int)vid_fsaa.value >= 2) ? va ("%ix", CLAMP (2, (int)vid_fsaa.value, 16)) : "off");

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_ANTIALIASING_MODE, "AA mode");
	M_Print (
		cbx, MENU_VALUE_X, top + CHARACTER_SIZE * GRAPHICS_OPT_ANTIALIASING_MODE,
		(((int)vid_fsaamode.value == 0) || !vulkan_globals.device_features.sampleRateShading) ? "Multisample" : "Supersample");

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_RENDER_SCALE, "Render Scale");
	M_Print (cbx, MENU_VALUE_X, top + CHARACTER_SIZE * GRAPHICS_OPT_RENDER_SCALE, (r_scale.value >= 2) ? va ("1/%i", (int)r_scale.value) : "off");

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_ANISOTROPY, "Anisotropic");
	M_Print (
		cbx, MENU_VALUE_X, top + CHARACTER_SIZE * GRAPHICS_OPT_ANISOTROPY,
		(vid_anisotropic.value == 0) ? "off" : va ("on (%gx)", vulkan_globals.device_properties.limits.maxSamplerAnisotropy));

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_UNDERWATER, "Underwater FX");
	M_Print (
		cbx, MENU_VALUE_X, top + CHARACTER_SIZE * GRAPHICS_OPT_UNDERWATER,
		(r_waterwarp.value == 0) ? "off" : ((r_waterwarp.value == 1) ? "Classic" : "glQuake"));

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_MODELS, "Models");
	M_Print (cbx, MENU_VALUE_X, top + CHARACTER_SIZE * GRAPHICS_OPT_MODELS, (r_md5models.value == 0) ? "classic" : "remastered");

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_MODEL_INTERPOLATION, "Animations");
	M_Print (cbx, MENU_VALUE_X, top + CHARACTER_SIZE * GRAPHICS_OPT_MODEL_INTERPOLATION, (r_lerpmodels.value == 0) ? "classic" : "smooth");

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_PARTICLES, "Particles");
	M_Print (
		cbx, MENU_VALUE_X, top + CHARACTER_SIZE * GRAPHICS_OPT_PARTICLES,
		((int)r_particles.value == 0) ? "off" : (((int)r_particles.value == 2) ? "Classic" : "glQuake"));

	if (vulkan_globals.ray_query)
	{
		M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * GRAPHICS_OPT_SHADOWS, "Dynamic Shadows");
		M_DrawCheckbox (cbx, MENU_VALUE_X, top + CHARACTER_SIZE * GRAPHICS_OPT_SHADOWS, r_rtshadows.value);
	}

	// cursor
	M_Mouse_UpdateListCursor (&graphics_options_cursor, MENU_CURSOR_X, 320, top, CHARACTER_SIZE, M_GraphicsOptions_NumItems (), 0);
	Draw_Character (cbx, MENU_CURSOR_X, top + graphics_options_cursor * CHARACTER_SIZE, 12 + ((int)(realtime * 4) & 1));
}

//=============================================================================
/* SOUND OPTIONS MENU */

enum
{
	SOUND_OPT_SNDVOL,
	SOUND_OPT_MUSICVOL,
	SOUND_OPT_MUSICEXT,
	SOUND_OPT_WATERFX,
	SOUND_OPTIONS_ITEMS
};

static int sound_options_cursor = 0;

static void M_Menu_SoundOptions_f (void)
{
	IN_Deactivate (true);
	key_dest = key_menu;
	m_state = m_sound;
	m_entersound = true;
}

static void M_SoundOptions_AdjustSliders (int dir, qboolean mouse)
{
	float f, clamped_mouse = CLAMP (SLIDER_START, (float)m_mouse_x, SLIDER_END);

	if (fabsf (clamped_mouse - (float)m_mouse_x) > 12.0f)
		mouse = false;

	if (dir)
		S_LocalSound ("misc/menu3.wav");

	if (mouse)
		slider_grab = true;

	switch (sound_options_cursor)
	{
	case SOUND_OPT_SNDVOL:
		f = M_GetSliderPos (0, 1, sfxvolume.value, false, mouse, clamped_mouse, dir, 0.1, 999);
		Cvar_SetValue ("volume", f);
		break;
	case SOUND_OPT_MUSICVOL:
		f = M_GetSliderPos (0, 1, bgmvolume.value, false, mouse, clamped_mouse, dir, 0.1, 999);
		Cvar_SetValue ("bgmvolume", f);
		break;
	case SOUND_OPT_MUSICEXT:
		Cvar_SetValueQuick (&bgm_extmusic, (float)(((int)bgm_extmusic.value + 2 + dir) % 2));
		break;
	case SOUND_OPT_WATERFX:
		Cvar_SetValueQuick (&snd_waterfx, (float)(((int)snd_waterfx.value + 2 + dir) % 2));
		break;
	}
}

static void M_SoundOptions_Key (int k)
{
	switch (k)
	{
	case K_MOUSE2:
	case K_ESCAPE:
	case K_BBUTTON:
		M_Menu_Options_f ();
		break;

	case K_MOUSE1:
	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
		m_entersound = true;
		M_SoundOptions_AdjustSliders (1, k == K_MOUSE1);
		return;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		sound_options_cursor--;
		if (sound_options_cursor < 0)
			sound_options_cursor = SOUND_OPTIONS_ITEMS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		sound_options_cursor++;
		if (sound_options_cursor >= SOUND_OPTIONS_ITEMS)
			sound_options_cursor = 0;
		break;

	case K_LEFTARROW:
		M_SoundOptions_AdjustSliders (-1, false);
		break;

	case K_RIGHTARROW:
		M_SoundOptions_AdjustSliders (1, false);
		break;
	}
}
static void M_SoundOptions_Draw (cb_context_t *cbx)
{
	qpic_t	 *p;
	const int top = MENU_TOP;

	M_DrawTransPic (cbx, 16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic (cbx, (320 - p->width) / 2, 4, p);

	// Draw the items in the order of the enum defined above:

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * SOUND_OPT_SNDVOL, "Sound Volume");
	M_DrawSlider (cbx, MENU_SLIDER_X, top + CHARACTER_SIZE * SOUND_OPT_SNDVOL, sfxvolume.value);

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * SOUND_OPT_MUSICVOL, "Music Volume");
	M_DrawSlider (cbx, MENU_SLIDER_X, top + CHARACTER_SIZE * SOUND_OPT_MUSICVOL, bgmvolume.value);

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * SOUND_OPT_MUSICEXT, "External Music");
	M_DrawCheckbox (cbx, MENU_VALUE_X, top + CHARACTER_SIZE * SOUND_OPT_MUSICEXT, bgm_extmusic.value);

	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * SOUND_OPT_WATERFX, "Underwater FX");
	M_DrawCheckbox (cbx, MENU_VALUE_X, top + CHARACTER_SIZE * SOUND_OPT_WATERFX, snd_waterfx.value);

	// cursor
	M_Mouse_UpdateListCursor (&sound_options_cursor, MENU_CURSOR_X, 320, top, CHARACTER_SIZE, SOUND_OPTIONS_ITEMS, 0);
	Draw_Character (cbx, MENU_CURSOR_X, top + sound_options_cursor * CHARACTER_SIZE, 12 + ((int)(realtime * 4) & 1));
}

//=============================================================================
/* OPTIONS MENU */

enum
{
	OPT_GAME = 0,
	OPT_CONTROLS,
	OPT_VIDEO,
	OPT_GRAPHICS,
	OPT_SOUND,
	OPT_PADDING,
	OPT_DEFAULTS,
	OPTIONS_ITEMS
};

static int options_cursor;

void M_Menu_Options_f (void)
{
	M_MenuChanged ();
	IN_Deactivate (true);
	key_dest = key_menu;
	m_state = m_options;
}

static void M_Options_Draw (cb_context_t *cbx)
{
	qpic_t	 *p;
	const int top = 40;

	M_DrawTransPic (cbx, 16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic (cbx, (320 - p->width) / 2, 4, p);

	// Draw the items in the order of the enum defined above:
	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * OPT_GAME, "Game");
	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * OPT_CONTROLS, "Key Bindings");
	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * OPT_VIDEO, "Video");
	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * OPT_GRAPHICS, "Graphics");
	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * OPT_SOUND, "Sound");
	M_Print (cbx, MENU_LABEL_X, top + CHARACTER_SIZE * OPT_DEFAULTS, "Reset config");

	// cursor
	M_Mouse_UpdateListCursor (&options_cursor, MENU_LABEL_X, 320, top, CHARACTER_SIZE, OPTIONS_ITEMS, 0);
	if (options_cursor == OPT_PADDING)
		options_cursor = OPT_SOUND;
	Draw_Character (cbx, MENU_CURSOR_X, top + options_cursor * CHARACTER_SIZE, 12 + ((int)(realtime * 4) & 1));
}

void M_Options_Key (int k)
{
	switch (k)
	{
	case K_MOUSE2:
	case K_ESCAPE:
	case K_BBUTTON:
		M_Menu_Main_f ();
		break;

	case K_MOUSE1:
	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
		m_entersound = true;
		m_mouse_x_pixels = -1;
		switch (options_cursor)
		{
		case OPT_GAME:
			M_Menu_GameOptions_f ();
			break;
		case OPT_CONTROLS:
			M_Menu_Keys_f ();
			break;
		case OPT_DEFAULTS:
			if (SCR_ModalMessage (
					"This will reset all controls\n"
					"and stored cvars. Continue? (y/n)\n",
					15.0f))
			{
				Cbuf_AddText ("resetcfg\n");
				Cbuf_AddText ("exec default.cfg\n");
			}
			break;
		case OPT_VIDEO:
			M_Menu_Video_f ();
			break;
		case OPT_GRAPHICS:
			M_Menu_GraphicsOptions_f ();
			break;
		case OPT_SOUND:
			M_Menu_SoundOptions_f ();
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		--options_cursor;
		if (options_cursor == OPT_PADDING)
			--options_cursor;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		++options_cursor;
		if (options_cursor == OPT_PADDING)
			++options_cursor;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;
	}
}

//=============================================================================
/* KEYS MENU */

const char *bindnames[][2] = {
	{"+forward", "Move Forward"},
	{"+back", "Move Backward"},
	{"+moveleft", "Strafe Left"},
	{"+moveright", "Strafe Right"},
	{"+jump", "Jump / Swim up"},
	{"+attack", "Attack"},
	{"+speed", "Run"},
	{"+zoom", "Quick zoom"},
	{"+moveup", "Swim up"},
	{"+movedown", "Swim down"},
	{"+showscores", "Show Scores"},
	{"impulse 10", "Next weapon"},
	{"impulse 12", "Prev weapon"},
	{"impulse 1", "Axe"},
	{"impulse 2", "Shotgun"},
	{"impulse 3", "Super Shotgun"},
	{"impulse 4", "Nailgun"},
	{"impulse 5", "Super Nailgun"},
	{"impulse 6", "Grenade Lnchr."},
	{"impulse 7", "Rocket Lnchr."},
	{"impulse 8", "Thunderbolt"},
	{"toggleconsole", "Toggle console"},
};

#define NUMCOMMANDS countof (bindnames)

static int		keys_cursor;
static qboolean bind_grab;

void M_Menu_Keys_f (void)
{
	M_MenuChanged ();
	IN_Deactivate (true);
	key_dest = key_menu;
	m_state = m_keys;
}

void M_FindKeysForCommand (const char *command, int *twokeys)
{
	int	  count;
	int	  j;
	char *b;

	twokeys[0] = twokeys[1] = -1;
	count = 0;

	for (j = 0; j < MAX_KEYS; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strcmp (b, command))
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

void M_UnbindCommand (const char *command)
{
	int	  j;
	char *b;

	for (j = 0; j < MAX_KEYS; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strcmp (b, command))
			Key_SetBinding (j, NULL);
	}
}

extern qpic_t *pic_up, *pic_down;

#define BINDS_PER_PAGE 19

static int first_key;

static void M_Keys_Draw (cb_context_t *cbx)
{
	int			i, x, y;
	int			keys[2];
	const char *name;
	qpic_t	   *p;
	int			keys_height = q_min (BINDS_PER_PAGE, NUMCOMMANDS - first_key);

	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic (cbx, (320 - p->width) / 2, 4, p);

	if (bind_grab)
		M_Print (cbx, 12, 32, "Press a key or button for this action");
	else
		M_Print (cbx, 18, 32, "Enter to change, backspace to clear");

	// search for known bindings
	for (i = 0; i < BINDS_PER_PAGE && i < (int)NUMCOMMANDS; i++)
	{
		y = 48 + 8 * i;

		M_Print (cbx, 10, y, bindnames[i + first_key][1]);

		M_FindKeysForCommand (bindnames[i + first_key][0], keys);

		if (keys[0] == -1)
		{
			M_Print (cbx, 140, y, "???");
		}
		else
		{
			name = Key_KeynumToString (keys[0]);
			M_Print (cbx, 140, y, name);
			x = strlen (name) * 8;
			if (keys[1] != -1)
			{
				name = Key_KeynumToString (keys[1]);
				M_PrintHighlighted (cbx, 138 + x, y, ",");
				M_Print (cbx, 138 + x + 12, y, name);
				x = x + 12 + strlen (name) * 8;
			}
		}
	}

	if (NUMCOMMANDS > BINDS_PER_PAGE)
		M_DrawScrollbar (cbx, MENU_SCROLLBAR_X, 56, (float)(first_key) / (NUMCOMMANDS - BINDS_PER_PAGE), BINDS_PER_PAGE - 2);

	if (bind_grab)
		Draw_Character (cbx, 130, 48 + (keys_cursor - first_key) * 8, '=');
	else
	{
		M_Mouse_UpdateListCursor (&keys_cursor, 12, 400, 48, 8, keys_height, first_key);
		Draw_Character (cbx, 0, 48 + (keys_cursor - first_key) * 8, 12 + ((int)(realtime * 4) & 1));
	}
}

void M_Keys_Key (int k)
{
	char cmd[80];
	int	 keys[2];

	if (bind_grab)
	{ // defining a key
		S_LocalSound ("misc/menu1.wav");
		if ((k != K_ESCAPE) && (k != '`'))
		{
			q_snprintf (cmd, sizeof (cmd), "bind \"%s\" \"%s\"\n", Key_KeynumToString (k), bindnames[keys_cursor][0]);
			Cbuf_InsertText (cmd);
		}

		bind_grab = false;
		IN_Deactivate (true); // deactivate because we're returning to the menu
		return;
	}

	if (M_HandleScrollBarKeys (k, &keys_cursor, &first_key, (int)NUMCOMMANDS, BINDS_PER_PAGE))
		return;

	switch (k)
	{
	case K_MOUSE2:
	case K_ESCAPE:
	case K_BBUTTON:
		M_Menu_Options_f ();
		break;

	case K_MOUSE1:
	case K_ENTER: // go into bind mode
	case K_KP_ENTER:
	case K_ABUTTON:
		M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
		S_LocalSound ("misc/menu2.wav");
		if (keys[1] != -1)
			M_UnbindCommand (bindnames[keys_cursor][0]);
		bind_grab = true;
		IN_Activate (); // activate to allow mouse key binding
		break;

	case K_BACKSPACE: // delete bindings
	case K_DEL:
		S_LocalSound ("misc/menu2.wav");
		M_UnbindCommand (bindnames[keys_cursor][0]);
		break;
	}
}

//=============================================================================
/* HELP MENU */

int help_page;
#define NUM_HELP_PAGES 6

static void M_Menu_Help_f (void)
{
	M_MenuChanged ();
	IN_Deactivate (true);
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
	help_page = 0;
}

static void M_Help_Draw (cb_context_t *cbx)
{
	M_DrawPic (cbx, 0, 0, Draw_CachePic (va ("gfx/help%i.lmp", help_page)));
}

static void M_Help_Key (int key)
{
	switch (key)
	{
	case K_MOUSE2:
	case K_ESCAPE:
	case K_BBUTTON:
		M_Menu_Main_f ();
		break;

	case K_MOUSE1:
	case K_MWHEELDOWN:
	case K_UPARROW:
	case K_RIGHTARROW:
		m_entersound = true;
		if (++help_page >= NUM_HELP_PAGES)
			help_page = 0;
		break;

	case K_MWHEELUP:
	case K_DOWNARROW:
	case K_LEFTARROW:
		m_entersound = true;
		if (--help_page < 0)
			help_page = NUM_HELP_PAGES - 1;
		break;
	}
}

//=============================================================================
/* QUIT MENU */

static int			  msg_number;
static enum m_state_e m_quit_prevstate;
static qboolean		  was_in_menus;

void M_Menu_Quit_f (void)
{
	if (m_state == m_quit)
		return;
	was_in_menus = (key_dest == key_menu);
	IN_Deactivate (true);
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
	msg_number = rand () & 7;
}

static void M_Quit_Key (int key)
{
	if (key == K_ESCAPE)
	{
		if (was_in_menus)
		{
			m_state = m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			IN_Activate ();
			key_dest = key_game;
			m_state = m_none;
		}
	}
}

static void M_Quit_Char (int key)
{
	switch (key)
	{
	case 'n':
	case 'N':
		if (was_in_menus)
		{
			m_state = m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			IN_Activate ();
			key_dest = key_game;
			m_state = m_none;
		}
		break;

	case 'y':
	case 'Y':
		m_is_quitting = true;
		IN_Deactivate (true);
		key_dest = key_console;
		Cbuf_InsertText ("quit");
		break;

	default:
		break;
	}
}

static qboolean M_Quit_TextEntry (void)
{
	return true;
}

static void M_Quit_Draw (cb_context_t *cbx) // johnfitz -- modified for new quit message
{
	char msg1[40];
	char msg2[] = "by Axel Gneiting"; /* msg2/msg3 are mostly [40] */
	char msg3[] = "Press y to quit";
	int	 boxlen;

	if (was_in_menus)
	{
		m_state = m_quit_prevstate;
		m_recursiveDraw = true;
		M_Draw (cbx);
		m_state = m_quit;
	}

	q_snprintf (msg1, sizeof (msg1), ENGINE_NAME_AND_VER);

	// okay, this is kind of fucked up.  M_DrawTextBox will always act as if
	// width is even. Also, the width and lines values are for the interior of the box,
	// but the x and y values include the border.
	boxlen = q_max (strlen (msg1), q_max ((sizeof (msg2) - 1), (sizeof (msg3) - 1))) + 1;
	if (boxlen & 1)
		boxlen++;
	M_DrawTextBox (cbx, 160 - 4 * (boxlen + 2), 76, boxlen, 4);

	// now do the text
	M_Print (cbx, 160 - 4 * strlen (msg1), 88, msg1);
	M_Print (cbx, 160 - 4 * (sizeof (msg2) - 1), 96, msg2);
	M_PrintWhite (cbx, 160 - 4 * (sizeof (msg3) - 1), 104, msg3);
}

//=============================================================================
/* Victory screen */ 


double victory_time = 0.;

static void M_Menu_Victory_f (void)
{
	if (sv.active)
		return;

	M_MenuChanged ();
	IN_Deactivate (true);
	key_dest = key_menu;
	m_state = m_victory;
	m_entersound = true;
	cl.cdtrack = 3;
	cl.looptrack = 3;
	BGM_PlayCDtrack ((byte)cl.cdtrack, true);
	victory_time = realtime;
	SCR_CenterPrint("\nCongratulations and well done! You have\nbeaten the seed, and its hundreds of\nchecks and locations. You have proven\nthat your skill and your cunning are\ngreater than all the powers of Quake.\nYou are the master now. We salute you.");
}

void SCR_DrawCenterString_Remaining (cb_context_t *cbx, int remaining);
extern cvar_t scr_printspeed;

static void M_Victory_Draw (cb_context_t *cbx)
{
	qpic_t *pic;
	int remaining;
        pic = Draw_CachePic ("gfx/finale.lmp");
        Draw_Pic (cbx, (320 - pic->width) / 2, 16, pic, 1.0f, false);
	remaining = scr_printspeed.value * (realtime - victory_time);
	SCR_DrawCenterString_Remaining(cbx, remaining);
}

static void M_Victory_Key (int key)
{
	switch (key)
	{
	case K_MOUSE2:
	case K_ESCAPE:
	case K_BBUTTON:
		M_Menu_Play_f ();
		break;
	}
}

//=============================================================================
/* Credits menu -- used by the 2021 re-release */

static void M_Menu_Credits_f (void) {}

//=============================================================================
/* Menu Subsystem */

void M_Init (void)
{
	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_play", M_Menu_Play_f);
	Cmd_AddCommand ("menu_ep1_select", M_Menu_Ep1_Select_f);
	Cmd_AddCommand ("menu_ep2_select", M_Menu_Ep2_Select_f);
	Cmd_AddCommand ("menu_ep3_select", M_Menu_Ep3_Select_f);
	Cmd_AddCommand ("menu_ep4_select", M_Menu_Ep4_Select_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_victory", M_Menu_Victory_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
	Cmd_AddCommand ("menu_credits", M_Menu_Credits_f); // needed by the 2021 re-release
}

void M_NewGame (void)
{
	m_main_cursor = 0;
}

void M_UpdateMouse (void)
{
	int new_mouse_x;
	int new_mouse_y;
	SDL_GetMouseState (&new_mouse_x, &new_mouse_y);
	m_mouse_moved = !menu_changed && ((m_mouse_x_pixels != new_mouse_x) || (m_mouse_y_pixels != new_mouse_y));
	m_mouse_x_pixels = new_mouse_x;
	m_mouse_y_pixels = new_mouse_y;
	menu_changed = false;

	m_mouse_x = new_mouse_x;
	m_mouse_y = new_mouse_y;
	M_PixelToMenuCanvasCoord (&m_mouse_x, &m_mouse_y);

	if (scrollbar_grab)
	{
		if (keydown[K_MOUSE1] && M_InScrollbar ())
			M_Keydown (K_MOUSE1);
		else
			scrollbar_grab = false;
	}
	else if (slider_grab)
	{
		if (keydown[K_MOUSE1] && (m_state == m_game) && (game_options_cursor >= GAME_OPT_SCALE) && (game_options_cursor <= GAME_OPT_VIEWROLL))
			M_GameOptions_AdjustSliders (0, true);
		else if (
			keydown[K_MOUSE1] && (m_state == m_graphics) && (graphics_options_cursor >= GRAPHICS_OPT_GAMMA) && (graphics_options_cursor <= GRAPHICS_OPT_FOV))
			M_GraphicsOptions_AdjustSliders (0, true);
		else if (keydown[K_MOUSE1] && (m_state == m_sound) && (graphics_options_cursor >= SOUND_OPT_SNDVOL) && (graphics_options_cursor <= SOUND_OPT_MUSICVOL))
			M_SoundOptions_AdjustSliders (0, true);
		else
			slider_grab = false;
	}

	scrollbar_size = 0;
}

void M_Draw (cb_context_t *cbx)
{
	if (m_state == m_none || key_dest != key_menu)
		return;

	if (!m_recursiveDraw)
	{
		if (scr_con_current)
		{
			Draw_ConsoleBackground (cbx);
			S_ExtraUpdate ();
		}

		Draw_FadeScreen (cbx); // johnfitz -- fade even if console fills screen
	}
	else
	{
		m_recursiveDraw = false;
	}

	GL_SetCanvas (cbx, CANVAS_MENU); // johnfitz

	switch (m_state)
	{
	case m_none:
		break;

	case m_main:
		M_Main_Draw (cbx);
		break;

	case m_play:
		M_Play_Draw (cbx);
		break;

	case m_options:
		M_Options_Draw (cbx);
		break;

	case m_game:
		M_GameOptions_Draw (cbx);
		break;

	case m_keys:
		M_Keys_Draw (cbx);
		break;

	case m_video:
		M_Video_Draw (cbx);
		break;

	case m_graphics:
		M_GraphicsOptions_Draw (cbx);
		break;

	case m_sound:
		M_SoundOptions_Draw (cbx);
		break;

	case m_help:
		M_Help_Draw (cbx);
		break;

	case m_ep1_select:
		M_Ep1_Select_Draw (cbx);
		break;

	case m_ep2_select:
		M_Ep2_Select_Draw (cbx);
		break;

	case m_ep3_select:
		M_Ep3_Select_Draw (cbx);
		break;

	case m_ep4_select:
		M_Ep4_Select_Draw (cbx);
		break;

	case m_victory:
		M_Victory_Draw (cbx);
		break;

	case m_quit:
		if (!fitzmode)
		{ /* QuakeSpasm customization: */
			/* Quit now! S.A. */
			m_is_quitting = true;
			key_dest = key_console;
			Cbuf_InsertText ("quit");
		}
		else
			M_Quit_Draw (cbx);
		break;
	}

	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}

	S_ExtraUpdate ();
}

void M_Keydown (int key)
{
	switch (m_state)
	{
	case m_none:
		return;

	case m_main:
		M_Main_Key (key);
		return;

	case m_play:
		M_Play_Key (key);
		return;

	case m_options:
		M_Options_Key (key);
		return;

	case m_game:
		M_GameOptions_Key (key);
		return;

	case m_graphics:
		M_GraphicsOptions_Key (key);
		return;

	case m_keys:
		M_Keys_Key (key);
		return;

	case m_video:
		M_Video_Key (key);
		return;

	case m_sound:
		M_SoundOptions_Key (key);
		return;

	case m_help:
		M_Help_Key (key);
		return;

	case m_ep1_select:
		M_Ep1_Select_Key (key);
		return;

	case m_ep2_select:
		M_Ep2_Select_Key (key);
		return;

	case m_ep3_select:
		M_Ep3_Select_Key (key);
		return;

	case m_ep4_select:
		M_Ep4_Select_Key (key);
		return;

	case m_victory:
		M_Victory_Key (key);
		return;

	case m_quit:
		M_Quit_Key (key);
		return;
	}
}

void M_Charinput (int key)
{
	switch (m_state)
	{
	case m_quit:
		M_Quit_Char (key);
		return;
	default:
		return;
	}
}

qboolean M_TextEntry (void)
{
	switch (m_state)
	{
	case m_quit:
		return M_Quit_TextEntry ();
	default:
		return false;
	}
}
