/* game.h - the game: its state, one step, one frame. */
#ifndef TP3D_GAME_H
#define TP3D_GAME_H

#include <vectorpie/vp.h>

void game_init(int vertical);

/* For testing: the next game starts in this era (1 = 1910 .. 5 = 2001). */
void game_start_level(int level);

/* For testing: every level starts with this many enemies already shot
 * (55: the boss comes after the next one; 56: at once; 57: at once, and
 * one hit takes it down). */
void game_cheat_kills(int kills);

/* For testing: in the self test nobody steers in the time tunnel, to see
 * what the wall does. */
void game_tunnel_hands_off(void);

/* For testing: in the self test the plane pulls up into half a loop every
 * twelve seconds and then rolls upright, as a player would by hand. */
void game_cheat_stunt(void);

/* For testing: every era begins with two parachutists before the plane,
 * one whole and one with his canopy shot. */
void game_cheat_chutes(void);

/* For testing: every boss comes with its guns gone, in the second part of
 * its fight. */
void game_cheat_boss2(int stage);       /* which stage, counted from 0: 1 the second, and so on; the last one when it has fewer */

/* For testing: every part of a boss falls to one hit; its stages and the
 * films between them stay. */
void game_cheat_weak(void);

/* For testing: show the debug choice of where to begin, row `row` chosen. */
void game_cheat_debugsel(int row);

/* For testing: the service menu open on its MONITOR page from the start. */
void game_cheat_service(void);

/* For testing: the first start's setup open on page `page` (0 its first
 * question; setup_cheat_page for the others), whether it was done or not. */
void game_cheat_setup(int page);

/* For testing: a game begun with its last plane just lost: the continue's countdown. */
void game_cheat_continue(void);

/* For testing: every new plane has the shield, the missiles and the triple shot. */
void game_cheat_items(void);

/* For testing: attract mode begins on this page (0 the title, 1 the high
 * scores, 2 who is who, 3 the demo, 4 how to play, 5 the items; 6 the items of the
 * tutorial after START). */
void game_attract_page(int page);

/* For testing: the game plays itself, `steps` steps a frame, with lives
 * that never run out, and reports each era on stderr; after `levels` eras
 * the program ends. */
void game_selftest(int steps, int levels);

void game_update(vp_game *g, float dt);     /* one 60 Hz step */
void game_draw(vp_game *g, vp_dvg *d);

#endif /* TP3D_GAME_H */
