/* hud.h - the fighter's head-up display and its cockpit.
 *
 * The plane the player flies is the same jet in every era, and looks out
 * through the same glass: a pitch ladder that stays level with the world,
 * a heading tape, speed and height, the throttle, the energy left, a box
 * round the target, and the warnings.
 */
#ifndef TP3D_HUD_H
#define TP3D_HUD_H

#include <vectorpie/vp.h>

#define HUD_ENERGY_Y 470.0f     /* the energy bar: low in the picture, over the row of planes left and enemies to go */
#define HUD_ENERGY_Y_DEMO 760.0f /* in the demo: over the coin line */

typedef struct hud_state {
    vp_vec3  eye;               /* where the camera is */
    float    heading, pitch;    /* the plane's, radians */
    float    speed;             /* m/s */
    float    altitude;          /* m */
    float    throttle;          /* 0..1 */
    float    energy;            /* 0..1 */
    int      cockpit;           /* 1: the cockpit and the whole display; 0: the chase view's few */
    float    energy_y;          /* where the energy bar is, board units */
    int      frame;             /* with the cockpit: its blue lines too (coaming, glass, canopy); 0: the display alone */
    int      rear;              /* looking back: a mirror's frame, and only what still matters */
    int      stalled, pull_up;
    int      burner;            /* the afterburner is lit */
    float    hurt;              /* seconds since the last hit still to show */
    unsigned frames;            /* for what blinks */
    /* the target: where it is on the board, how far, and whether the guns have it */
    int      target, locked;
    float    target_x, target_y, target_range;
    /* the nearest enemy when none is in sight: which way to turn (radians, 0 = right, anticlockwise) */
    int      arrow;
    float    arrow_angle;
    /* the boss, as long as there is one: where it is on the board when it
     * is in sight, else which way to turn for it */
    int      boss, boss_seen;
    float    boss_x, boss_y, boss_angle;
    /* a homing missile after the plane: which way it is, and how far */
    int      missile;
    int      laser;             /* a boss's laser: 1 it is taking aim at the plane, 2 it has locked on - break away now */
    float    missile_angle, missile_range;
    float    bright, bright_text;
} hud_state;

/* The camera must be set (world_pose) to the view the display belongs to. */
void hud_draw(const hud_state *s);

#endif /* TP3D_HUD_H */
