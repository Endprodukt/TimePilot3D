/* service.h - the service menu: game.ini's settings, changed on the machine. */
#ifndef TP3D_SERVICE_H
#define TP3D_SERVICE_H

#include <vectorpie/vp.h>

/* FOR TESTING, TO BE TAKEN OUT: with this 1, START does not begin the game
 * at once but asks where to begin it - any era, or any era's boss in any
 * stage of its fight (debug_select in game.c; drawn in draw.c). The
 * service menu's LEVEL SELECT turns it off and on while it is built in.
 * Set it to 0 before the game is let out. */
#define DEBUG_LEVEL_SELECT 1

/* The controls a player can lay out, in game.ini's order. */
enum { SERVICE_FIRE, SERVICE_FASTER, SERVICE_SLOWER, SERVICE_REAR, SERVICE_BUTTONS };

/* How many kinds of thing have a brightness of their own (game.c's BRIGHT_ names). */
#define SERVICE_BRIGHTS 8

/* How hard the game is: game.c's table has a row for each. */
enum { DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_ACE, DIFFICULTY_COUNT };

/* What is seen in a game (START goes round them in this order): the
 * head-up display alone, the cockpit round it, from behind. */
enum { VIEW_HUD, VIEW_COCKPIT, VIEW_CHASE, VIEW_COUNT };

/* What game.ini holds. */
typedef struct service_settings {
    /* the game */
    int   lives;                        /* planes a game starts with, 1..9 */
    int   difficulty;                   /* DIFFICULTY_ */
    int   bonus_plane;                  /* the score the first extra plane comes at */
    int   kills_per_era;                /* enemies to shoot down before the boss comes */
    int   start_era;                    /* the era a game begins in, 0..4: for trying things out */
    int   free_play, attract_sound;     /* yes or no */
    int   reset_scores;                 /* yes: SAVE empties the high score table */
    /* the controls */
    float yoke_sensitivity;             /* 1 = the game's own turn rate */
    int   yoke_deadzone;                /* percent of full travel, 0..50 */
    int   invert_pitch;                 /* yes: forward pulls the nose up */
    int   view;                         /* the view a game starts in: VIEW_ */
    int   aim_assist;                   /* 0..100 */
    char  button[SERVICE_BUTTONS][128]; /* vpbind.h's controls, '+' between several */
    char  menu_key[128];                /* what opens this menu besides F2 (vpbind.h's controls) */
    int   control_stick;                /* 0 the yoke, 1 a stick and buttons */
    int   yoke_layout[4];               /* the yoke's button numbers: right trigger, left trigger, right thumb, left thumb */
    /* sound and picture */
    int   volume, effects_volume, engine_volume, music_volume;    /* 0..100 */
    int   brightness[SERVICE_BRIGHTS];  /* 0..100 */
    int   level_select;                 /* yes: START opens the debug choice of where to begin (DEBUG_LEVEL_SELECT) */
    int   invincible;                   /* yes, for testing: nothing harms the plane */
    int   continues;                    /* yes: the last plane lost, START flies on (the score starts again) */
    int   occlusion;                    /* yes: the buildings are solid */
    /* the monitor */
    int   picture_size;                 /* percent, 60..100 */
    int   beam_limit;                   /* thousand board units a frame, 100..250 */
} service_settings;

/* SERVICE_SETUP: CONTROL SETUP was chosen - the menu closes without saving
 * and the first start's setup (setup.h) begins. */
enum { SERVICE_OPEN = 0, SERVICE_SAVED, SERVICE_CANCELLED, SERVICE_SETUP };

/* A button's standard for these settings (SERVICE_ index): on the yoke
 * where its layout says (fire the right trigger, look back the left,
 * faster and slower the thumbs), with a stick buttons 1 to 4 and the keys
 * AdvanceMAME gives those. */
const char *service_standard(const service_settings *v, int b);

/* Once every game step, menu open or not: 1 when the menu's key has just
 * been pressed - F2 always, and whatever MENU BUTTON has (service_menu_key).
 * (It also reads every control for the step: vpb_update.) */
int service_hotkey(void);

/* What opens the menu besides F2 (vpbind.h's controls; "key_F2" alone is
 * the standard). */
void service_menu_key(const char *controls);

/* Open the menu on these settings. */
void service_open(const service_settings *now);

/* One step. `hotkey` is service_hotkey's answer (the key again cancels). */
int service_update(vp_input *in, int hotkey);

/* The settings as they stand in the menu: the ones to take over once
 * service_update has answered SERVICE_SAVED. */
const service_settings *service_get(void);

/* The frame must have begun (world_begin). */
void service_draw(void);

/* While the menu is open: the picture size as it is set in it right now,
 * so that changing it is seen at once. */
int service_picture_size(void);

/* For testing: with the menu open, turn straight to its MONITOR page. */
void service_show_monitor(void);

#endif /* TP3D_SERVICE_H */
