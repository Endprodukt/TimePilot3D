/* game_internal.h - what the game's files share: its constants, its
 * objects, its state and the small helpers, and the functions one file
 * of it offers the others. Nothing outside the game includes this.
 *
 *   game.c      the state, the course of a game, the cabinet, the settings
 *   objects.c   the object pool, the effects, hits, and what falls or flies on its own
 *   enemies.c   the enemies, the big ones, the boss, the guns on the ground
 *   player.c    the plane the player flies, its guns and items, the time tunnel
 *   draw.c      drawing the play, the head-up display, the warp
 *   attract.c   the title, the high scores, who is who, the initials
 */
#ifndef TP3D_GAME_INTERNAL_H
#define TP3D_GAME_INTERNAL_H

#include "game.h"
#include "city.h"
#include "controls.h"
#include "hud.h"
#include "logo.h"
#include "models.h"
#include "service.h"
#include "sound.h"
#include "world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DT              (1.0f / 60.0f)
#define PI              3.14159265f
#define PI2             6.2831853f
#define MAX_OBJ         1024
#define KILL_MARKS      8           /* the counter's marks */
#define EXTRA_EVERY     50000       /* after the first bonus plane */
#define ERA_COUNT       5
/* the player's plane */
#define SPEED_MIN       50.0f       /* m/s: the throttle shut */
#define SPEED_MAX       135.0f      /* the throttle open */
#define SPEED_TOP       175.0f      /* in a dive */
#define THROTTLE_MID    0.5f        /* where the throttle rests with no button held: 92 m/s */
#define THROTTLE_START  THROTTLE_MID
#define THROTTLE_RATE   0.6f        /* of its travel in a second, while a button is held */
#define THROTTLE_CROSS  2.0f        /* times as fast while it is still on the other side of the middle */
#define THROTTLE_BACK   0.2f        /* of its travel in a second, back to the middle when let go */
#define STALL_SPEED     40.0f       /* below it the wing stops flying */
#define STALL_OVER      55.0f       /* and above this it flies again */
#define GRAVITY_PULL    24.0f       /* m/s2 lost climbing straight up, gained diving */
#define TURN_RATE       1.25f       /* rad/s at full yoke, at TURN_SPEED */
#define TURN_SPEED      85.0f       /* slower turns tighter, faster wider */
#define PITCH_RATE      0.85f
#define ROLL_RATE       3.4f        /* rad/s at full yoke with both speed buttons held: a whole roll in under two seconds */
/* No limit to the pitch: pulled far enough the plane goes over on its
 * back and round, a loop. It costs speed on the way up, so it wants the
 * afterburner, or it ends in a stall at the top. */
#define BANK_MAX        0.95f
#define ALT_CRASH       3.0f        /* the ground */
#define ALT_MAX         900.0f
#define PLAYER_RADIUS   6.0f
#define SHIELD_TIME     3.0f        /* seconds safe after a new plane */
#define ENERGY_FULL     100.0f
/* what a hit costs, of ENERGY_FULL, at normal difficulty */
#define DMG_BULLET      12.0f
#define DMG_BOSS_BULLET 18.0f
#define DMG_MG          3.0f        /* one round of a boss's machine gun */
#define DMG_ROCKET      15.0f
#define DMG_LASER       42.0f       /* it locks on and says so: whoever does not break then has it coming */
#define LASER_AIM        1.4f        /* seconds its line follows the plane */
#define LASER_HOLD       0.75f       /* seconds it stands, locked, with the warning: the time to break away in */
#define LASER_BEAM       0.5f        /* seconds the beam burns */
#define LASER_WIDTH      15.0f       /* metres from its line the beam still hits */
#define DMG_HEAD_LASER   45.0f       /* the robot's, out of its head: seldom, and the worst there is */
#define LASER_REACH     900.0f      /* metres a laser's beam goes */
#define DMG_MISSILE     30.0f
#define DMG_RAM         40.0f       /* flying into an enemy (it goes down) */
#define DMG_RAM_BOSS    15.0f       /* touching a boss's hull: a knock that throws the plane off it (boss_push_off), not the end - its weak points are on that hull */
#define RAM_BOSS_REST   2.5f        /* seconds until it can knock again: time to get clear of something that big */
#define DMG_BOSS_BOMB   12.0f       /* one bomb of a boss's row */
#define SALVO_WARN      0.7f        /* seconds a volley of balls is announced at its muzzle before it goes */
#define MG_WARN         0.5f        /* and a machine gun's burst */
#define MG_NEAREST      150.0f      /* a boss's machine gun starts no burst at a plane nearer than this: nobody dodges that */
#define CHUTE_ENERGY    20.0f       /* a parachutist taken aboard gives this back */
#define ERA_ENERGY      30.0f       /* and so does the journey to the next era */
/* the guns */
#define SHOT_SPEED      450.0f
#define SHOT_LIFE       1.5f
#define GUNS_MEET       320.0f      /* m ahead: where the two guns' shots cross, on the sight's line (the pipper is there too) */
#define FIRE_PERIOD     0.11f
#define MAX_SHOTS       30
/* the items */
enum { ITEM_SHIELD, ITEM_MISSILES, ITEM_TRIPLE, ITEM_REPAIR, ITEM_FLARES, ITEM_COUNT };     /* flares last: only where there are missiles */

#define SHIELD_HITS     2           /* hits a shield takes for the plane */
#define MISSILE_LOAD    6           /* homing missiles in a load */
#define MISSILE_MAX     12
#define TRIPLE_TIME     25.0f       /* seconds of three shots at once */
#define FLARE_TIME      40.0f       /* seconds the flares go out by themselves when a missile comes */
#define FLARE_RANGE     260.0f      /* metres: a missile this near is drawn off by them */
/* What a shot of the enemy's is: the boss's and the big ones' are big and
 * slow (BIG_SHOT_*), seen coming and flown round; flak is drawn bigger
 * than the planes' own. */
enum { SHOT_GUN, SHOT_FLAK, SHOT_BOSS, SHOT_HEAVY,
       SHOT_MG,         /* a boss's machine gun: small, fast, a streak */
       SHOT_ROCKET };   /* an unguided rocket: fast, straight, a dart with smoke behind it */
#define BIG_SHOT_SLOW   0.6f        /* of the era's shot speed */
#define BIG_SHOT_SIZE   2.4f        /* metres across its half: how big it is drawn, and hits */
#define ITEM_POINTS     500
#define DEPOT_HITS      5           /* shots a depot takes */
#define GUN_HITS        3           /* and a gun on the ground */
#define GUN_POINTS      300
#define CHUTE_CATCH_POINTS 3000     /* a parachutist shot, caught as he falls */
#define ASSIST_CONE     0.10f       /* radians at aim_assist 100: 6 degrees */
/* the sky */
#define SPAWN_NEAR      380.0f
#define SPAWN_FAR       650.0f
#define SPAWN_AHEAD     0.75f       /* the share of enemies that come from ahead (spawn_enemy) */
#define DESPAWN         1500.0f
#define SPAWN_EVERY     0.5f
#define BOSS_SPAWN_EVERY 3.0f      /* with a boss up, one lost is made good only slowly */
#define CHUTE_CHANCE    5           /* one fallen enemy in this many leaves a parachutist */
#define FORMATION_SIZE  5
/* times, in seconds */
#define INTRO_TIME      2.5f
#define DYING_TIME      2.2f
#define TUNNEL_APPROACH 7.4f        /* the way out of an era: the mouth opens, a loop, and the run into it */
#define APPROACH_OPEN   1.8f        /* in it: the tunnel's mouth opens ahead, with its sound */
#define APPROACH_LOOP   2.6f        /* then the plane flies a loop (and its whoosh) */
#define LOOP_RADIUS     45.0f       /* metres */
#define LEVEL_IN_TIME   5.6f        /* the way into an era: its tunnel opens, the plane comes out of it */
#define FLYBY_NEAR      75.0f       /* metres: a thing that passes nearer than this is heard rushing by */
#define LEVEL_IN_OPEN   1.6f        /* in it: the mouth opens over this long, then the plane bursts out */
#define TUNNEL_MOUTH    0.5f        /* seconds past the mouth in which the era's sky gives way to the tunnel */
#define TUNNEL_TIME     14.0f       /* the tunnel itself */
#define WARP_TIME       (TUNNEL_APPROACH + TUNNEL_TIME)     /* the whole journey through time */
#define TUNNEL_IN       1.6f        /* the tunnel's first seconds: the plane is carried in */
#define TUNNEL_OUT      1.6f        /* and its last: carried out */
#define PORTAL_RANGE    800.0f      /* metres ahead of the plane the tunnel's mouth opens */
/* flying the time tunnel */
#define TUNNEL_AHEAD    30.0f       /* metres the plane is ahead of the eye */
#define TUNNEL_EYE      7.0f        /* metres the eye is over the plane in the tunnel */
#define TUNNEL_EYE_IN   3.5f        /* and as the plane comes in: it sinks to its place in the first second */
#define TUNNEL_WALL     96.0f       /* metres from the middle to the wall (the rings' flat sides, less the plane) */
#define TUNNEL_PERFECT  32.0f       /* a ring passed this near its middle is a perfect one */
#define TUNNEL_SIDE     150.0f      /* m/s the plane can slide sideways */
#define TUNNEL_UP       120.0f      /* and up or down */
#define DMG_TUNNEL      10.0f       /* what touching the wall costs */
#define TUNNEL_POINTS   50          /* a ring passed */
#define TUNNEL_PERFECT_POINTS 200   /* a ring passed through its middle */
#define TUNNEL_ALL_BONUS 5000       /* every ring of a journey a perfect one */
#define TUNNEL_SLOW     0.65f       /* the tunnel's speed with the throttle shut, of its own */
#define TUNNEL_FAST     1.40f       /* and with it open: a ring is worth half as much, and half as much again */
#define GAME_OVER_TIME  4.0f
#define TITLE_TIME      12.0f
#define SCORES_TIME     8.0f
#define CAST_TIME       12.0f
#define HOWTO_TIME      31.0f       /* how to play: the yoke shown from both sides */
#define HOWTO_BUTTONS   10.0f       /* how to play: when the four buttons begin to be shown, */
#define HOWTO_EACH      3.5f        /* and how long each (fire, look back, faster, slower) */
#define HOWTO_SHOT_AT   0.5f        /* into fire's turn: a burst of HOWTO_SHOTS, one each HOWTO_SHOT_EVERY */
#define HOWTO_SHOTS     5
#define HOWTO_SHOT_EVERY 0.18f
#define ITEMS_TIME      12.0f       /* the items and where they come from (attract page, and after how to play) */
#define TUTORIAL_TIME   (HOWTO_TIME + ITEMS_TIME)
#define CONTINUE_TIME   10.0f       /* the last plane lost: seconds to press START and fly on */
#define DEMO_TIME       30.0f
#define INITIALS_TIMEOUT 30.0f
#define NOTICE_TIME     2.0f
#define GROW_TIME       2.2f        /* an era's buildings rising when the plane arrives */
enum { ATTRACT, INITIALS, INTRO, PLAYING, DYING, WARP, GAME_OVER,
       BOSSFALL,   /* the boss is shot down: its fall is watched to the end, then the time tunnel */
       BOSSINTRO,  /* the boss comes: a warning, and a look at it and its weak points before the fight */
       BOSSTURN,   /* between two stages of its fight: what it brings out next is watched coming out */
       LEVELIN,    /* an era begins: the time tunnel opens in its sky and the plane comes out of it */
       DEBUGSEL,   /* DEBUG_LEVEL_SELECT: before a game, where to begin it */
       TUTORIAL    /* after START: how to play with the yoke, then the items; to lounge music; START skips it */
};

/* DEBUG_LEVEL_SELECT (the choice of where to begin, for testing) is set in
 * service.h, so that the service menu sees it too. */

enum { PAGE_TITLE, PAGE_SCORES, PAGE_CAST, PAGE_HOWTO, PAGE_ITEMS };
enum { YOKE_RIGHT_TRIGGER, YOKE_LEFT_TRIGGER, YOKE_RIGHT_THUMB, YOKE_LEFT_THUMB, YOKE_PLACES };

enum {
    K_NONE, K_ENEMY, K_BOSS, K_BULLET, K_MISSILE, K_SHOT, K_CHUTE, K_SPARK, K_SCORE,
    K_WRECK,        /* a flyer shot down, on its way to the ground */
    K_PART,         /* a piece broken off one: a wing, a rotor */
    K_DEBRIS,       /* one line of a model that has burst, tumbling */
    K_RING,         /* a shock wave */
    K_BOMB,         /* 1910: thrown from a biplane, it falls in an arc */
    K_PRING,        /* a ring of points flying apart, its colours running round */
    K_ITEM,         /* an item out of a depot, hovering to be taken */
    K_PMISSILE      /* one of the player's own homing missiles */
};

/* what a spark is (its hp) */
enum { SPARK_FIRE, SPARK_SMOKE, SPARK_WHITE, SPARK_RAINBOW };

/* which of a model's lines are drawn or thrown (a wreck's or a part's cut) */
enum { CUT_NONE, CUT_ONLY_RIGHT, CUT_ONLY_LEFT, CUT_NOT_RIGHT, CUT_NOT_LEFT, CUT_CENTRE };

/* how an era's enemies fight */
enum { B_PURSUIT, B_BOOMZOOM, B_HOVER, B_JET, B_SAUCER };

/* what one of them is doing */
enum { M_ATTACK, M_EXTEND, M_EVADE };

/* how one of them gets out of the way (its move) */
enum {
    MOVE_BREAK,     /* a hard turn to one side, diving or climbing */
    MOVE_JINK,      /* hard left, hard right, and again: scissors */
    MOVE_SPLIT_S,   /* over on its back and down, coming out the other way */
    MOVE_BARREL,    /* a barrel roll: round a corkscrew, losing no way */
    MOVE_ZOOM       /* straight up to lose speed and let the player go past underneath */
};

enum {
    BRIGHT_GROUND, BRIGHT_CLOUDS, BRIGHT_ENEMIES, BRIGHT_PLAYER,
    BRIGHT_SHOTS, BRIGHT_EFFECTS, BRIGHT_TEXT, BRIGHT_TITLE, BRIGHT_COUNT
};

typedef struct obj {
    int     kind, model;
    vp_vec3 pos, vel;               /* vel: where it goes in a second (flyers set it too) */
    float   heading, pitch, roll, speed;
    float   age, life;              /* seconds alive; seconds left (0 = for ever) */
    int     hp, value;              /* value: a score's points, a bullet's damage */
    float   turn, timer, straight, reaim;   /* an enemy's turn rate, gun timer, how long its evading move is, re-aim timer */
    vp_vec3 offset;                 /* the point beside the player it flies at */
    float   spin;                   /* rotors */
    int     flash;                  /* frames lit after a hit */
    float   heard_d;                /* how far from the player it was last step (hear_the_sky: a pass by) */
    int     heard_by;               /* its pass by has been heard; again once it is far enough off */
    /* how it fights */
    int     mode;                   /* M_ */
    float   mode_time;              /* seconds left in the mode, or to the next decision */
    float   check;                  /* seconds to the next look over its shoulder */
    float   side;                   /* -1 or 1: the way it breaks, strafes */
    float   trait;                  /* 0..1, its own: how keen it is on the player's tail */
    float   tail;                   /* seconds it has been on the player (those on its tail count three times); below 0: seconds it still rests */
    float   phase;                  /* its own place in a weave */
    vp_vec3 wp;                     /* a saucer's next point; a piece of debris's other end */
    int     form;                   /* the formation it flies in, 0 = none */
    int     move;                   /* MOVE_: how it is getting out of the way */
    int     scared;                 /* a shot has just gone past it */
    /* what is left of it */
    vp_vec3 ang;                    /* how it tumbles, radians a second about each axis */
    int     cut;                    /* CUT_: the lines it still has; a ring's: 1 = flat on the ground */
    int     boss;                   /* a wreck: it was the boss */
    int     rotor;                  /* a wreck: its rotors are still on */
    /* one of the big ones */
    int     heavy;                  /* it crosses the sky, takes several hits and is worth HEAVY_POINTS */
    int     escort;                 /* 1..4: one of a boss's escort, flying round it */
    float   size;                   /* how big it is drawn and hit, when not as its kind is (0) */
    float   bomb;                   /* 1910: seconds until it can throw another bomb */
} obj;

/* The eras. Speeds in m/s, turn rates in rad/s, times in seconds.
 *
 * Every pilot can fly: it goes for the player's tail if it is that kind,
 * holds the player's speed once it is there, turns away rather than ram,
 * and gets out of the way when it is aimed at or a shot goes past it, with
 * a move of its time (enemy_evades). How they fight follows their time:
 *   1910  biplanes turn tightly, weave and shoot where the player is;
 *         aimed at, they jink or break.
 *   1940  fighters aim ahead of the player, make a pass and climb away to
 *         come again; aimed at, they break, split-S or barrel roll.
 *   1970  helicopters close in, then stand off and slide sideways round
 *         the player, backing away when it comes near, with rockets.
 *   1982  jets go for the player's tail and stay there at its speed, fire
 *         homing missiles from a distance and break hard when aimed at.
 *   2001  saucers do not fly, they dart: from point to point at once,
 *         shooting in any direction, gone the moment they are aimed at. */
struct era {
    const char *year;
    int   model, boss_model, behave;
    float speed, turn, bullet_speed, fire_range, fire_period;
    int   count;                    /* enemies in the sky at once */
    int   boss_hp, missiles;        /* hits the boss takes; missiles in the air at once */
    int   points, boss_points;
    float boss_speed, rotor_scale;
    float lead;                     /* 0..1: how far ahead of the player they aim */
    float evade;                    /* 0..1: how likely to break when aimed at, each half second */
    float six;                      /* 0..1: how many of them go for the player's tail */
    float colour[3];
};
extern const struct era eras[ERA_COUNT];

/* The big ones. Every era has one that crosses the sky from time to time:
 * it keeps its course, shoots from its turrets, takes several hits and is
 * worth 1500 points, as the red and yellow bombers of the original's 1940
 * are. They are drawn red and yellow in every era. */
struct heavy {
    int   model;
    float size;                     /* of the model's own */
    int   hp;
};
extern const struct heavy heavies[ERA_COUNT];

#define BOSS_INTRO_TIME 7.5f        /* seconds of the boss's coming */
#define BOSS_TURN_TIME  3.8f        /* seconds of the film between two stages of its fight */
#define ROBOT_TURN_TIME 7.2f        /* and of the one in which the mothership becomes the robot */
#define MORPH_FROM      1.0f        /* in that film: the saucer shakes until here, */
#define MORPH_TIME      4.4f        /* then for this long it comes apart and stands up as the robot */
#define BAY_BLOWS       1.6f        /* into that film a wrecked bomb bay goes up: fires before, the doors off at this moment */
#define BOSS_PART_POINTS 500        /* one of its parts shot off */
#define BOSS_PARTS      6           /* the most a stage has */
#define STATION_HEIGHT  26.0f       /* metres a shield station's dish is over what it stands on */

/* The bosses (boss.c): each a fight in stages, each stage with parts to
 * shoot off. A part: where it sits (in the model's own units), how big it
 * is, how many hits it takes, the side it is open to (0, 0, 0: any), and
 * what it looks like. The big balls' volleys are BP_. */
enum { LOOK_ENGINE, LOOK_BAY, LOOK_CANNON, LOOK_POD, LOOK_LASER, LOOK_CORE, LOOK_STATION,
       LOOK_ARM,        /* an arm of the robot: the arm itself is what is seen, and it moves (boss.c) */
       LOOK_HEAD };     /* its head */
enum { BP_FAN, BP_LADDER, BP_BROADSIDE, BP_TWO_RINGS, BP_SCISSORS, BP_PINWHEEL };
struct boss_part {
    float x, y, z, r;
    int   hp;
    float fx, fy, fz;
    int   look;
};
struct boss_stage {
    const char *goal;               /* said when it begins */
    const char *bar;                /* the word beside what is left of it */
    const char *title;              /* of the film before it; none before the first */
    int   parts;                    /* 0: the goal is the escort */
    struct boss_part part[BOSS_PARTS];
};
struct boss {
    const char *name;               /* its type, for its coming */
    float size;                     /* of the model's own */
    int   stages;
    struct boss_stage stage[4];
    float swing, swing_period;      /* how far it climbs and sinks about the player's height, and how slowly */
    float orbit;                    /* metres it keeps from the player */
    /* Its body, for flying into it, in the model's units: half its
     * length and how thick it is, and half its wings' span and how
     * thick they are (0: none). A length of 0 is a disc: hull_r its
     * half height, wing_span its radius. */
    float hull_len, hull_r, wing_span, wing_r;
};
extern const struct boss bosses[ERA_COUNT];
#define HEAVY_POINTS    1500
#define HEAVY_KILLS     3           /* what a big one counts towards the boss */
#define CHUTE_SIZE      1.7f        /* a parachutist: metres for one unit of his drawing (he is 11 units tall with the canopy) */
#define CHUTE_SAFE      1.5f        /* seconds after he is out in which no shot touches him: the burst that shot his plane down is still coming (3 until 2026-10-21) */
#define MISSILE_POINTS  200         /* a missile shot out of the air */
#define CHUTE_POINTS    1000        /* the first parachutist; each one after it 1000 more */
#define CHUTE_POINTS_MAX 5000
#define FORMATION_BONUS 2000
/* How hard the game is (the service menu's DIFFICULTY). */
struct difficulty {
    float damage;                   /* what a hit costs, times this */
    float fire;                     /* how often the enemies shoot */
    float bullets;                  /* how fast their shots fly */
    float evade;                    /* how readily they break */
    int   count;                    /* enemies in the sky, more or fewer */
};
extern const struct difficulty difficulties[DIFFICULTY_COUNT];

/* The enemies are drawn (and hit) half as big again as their metres say:
 * a vector line has no size of its own, and a true-size biplane half a
 * kilometre off is a speck. The bosses are big enough as they are. */
#define ENEMY_SCALE 1.5f
#define MISSILE_SCALE 2.0f

struct game_state {
    int      state;
    float    state_time;            /* seconds in the current state */
    float    level_time;            /* seconds since the era began */
    float    boss_at;               /* level_time when its boss was called (for the log: how long an era takes) */
    unsigned frames;
    unsigned rng;
    vp_game *game;

    /* the player */
    struct {
        vp_vec3 pos;
        float   heading, pitch, roll;
        float   speed, throttle;
        float   energy;
        int     stalled, pull_up;
        float   fire_cd;
        int     alive, gun;
        float   shield;                     /* seconds a new plane is still safe */
        float   hurt;                       /* seconds the last hit still shows */
        float   shake;                      /* how hard the hit still shakes the view, 0..1 */
        float   burner;                     /* the afterburner, 0 off .. 1 full */
        /* items (grant_item) */
        int     shield_hits;                /* hits the shield still takes */
        int     missiles;                   /* homing missiles left */
        float   missile_cd;                 /* seconds to the next one */
        float   triple;                     /* seconds of three shots at once left */
        float   flares;                     /* seconds of flares left */
        float   flare_show;                 /* seconds flares are still coming out of the tail */
        float   ram_cd;                     /* seconds until flying into something counts again */
        float   cam_heading, cam_pitch;     /* the chase camera, lagging a little */
        /* The roll flown by hand (both speed buttons held, the yoke to the
         * side), added to the plane's own bank: within a quarter turn either
         * way, see update_player. */
        float   flip_roll;
        int     rear;                       /* looking back, while the button is held */
    } p;
    float view_heading;             /* the way the camera looks, for the horizon */

    obj  objs[MAX_OBJ];
    int  score, hiscore, lives, next_extra;

    /* One player, or two taking turns: the one flying has the numbers
     * above and below; here is what each has when it is not flying. The
     * turn passes with every plane lost. */
    int  players, cur;
    struct {
        int score, lives, next_extra;
        int level, kills;           /* the era, and how far into it */
        int out;                    /* no planes left */
        int best;                   /* the best score before a continue took it back to 0: it still counts for the table */
    } pl[2];
    int  over_player;               /* at the end: whose initials are asked for next */
    int  level, era, loop;          /* level counts up; era = level mod 5; loop = level div 5 */
    int  kills, boss_called, boss_alive;
    float spawn_timer;
    int  start_level;
    int  form_id, form_killed;      /* the formation in the sky, and how many of it are down */
    int  form_chute;                /* the formation one of whose planes has left a parachutist: no second from it */
    float form_timer;
    float heavy_timer;              /* seconds until the next big one crosses */
    int  chute_chain;               /* parachutists taken aboard in a row, with this plane in this era */
    int  warp_from, warp_to;        /* the eras a journey through time joins */

    /* the boss (boss.c): the stage of its fight, what each of the stage's
     * parts still takes and the frames it is lit after a hit, the hits
     * the stage took when it began (for the bar), how far what the next
     * stage brings out has come out (0..1, the film between stages),
     * clocks and counters for its weapons, where it was when the stage
     * began, the shield's stations, and its laser */
    int   boss_stage, boss_total, boss_one_hit, boss_kind;
    int   part_hp[BOSS_PARTS], part_flash[BOSS_PARTS];
    float boss_deploy;
    float boss_stage_time;          /* seconds the stage has lasted */
    float boss_t[4];
    int   boss_n[4];
    /* A salvo is announced: where it will come from blinks (warn_at, for
     * warn_t seconds more; draw.c) before the first round goes. A volley
     * of balls waits out that time here (salvo_*: its pattern, its speed,
     * its muzzle from the boss's middle). */
    /* The robot's limbs (boss.c, draw.c). An arm, 0 the right and 1 the
     * left: on it (0), shot off and falling (1), gone (2); how far it is
     * raised at the plane (radians), and while it falls how far it has
     * fallen from its shoulder, how fast, how far it has tumbled, how
     * long. The legs: how far along its walk it is. */
    int   arm_state[2];
    float arm_pitch[2], arm_spin[2], arm_t[2];
    vp_vec3 arm_off[2], arm_vel[2];
    float walk;
    int   robot_landed;
    float boss_morph;               /* 0..1: how far the mothership has become the robot, in its film (draw.c) */
    float warn_t;
    vp_vec3 warn_at;
    float salvo_t, salvo_speed;
    int   salvo_pattern;
    vp_vec3 salvo_off;
    vp_vec3 boss_anchor, station[BOSS_PARTS];
    int   laser_state, laser_hit;   /* 0 resting, 1 aiming, 2 holding, 3 the beam */
    float laser_t;
    vp_vec3 laser_from, laser_dir;
    /* Missiles come in their time, not all the time (missile_allowed):
     * seconds still to pass with none in the air before the next may
     * come, and seconds a second one may still follow the first. */
    float missile_rest, missile_pair;
    /* its fall: where it is (the camera looks there), where the camera
     * stands, and how long since it hit the ground */
    vp_vec3 fall_at, fall_eye;
    float   fall_after;
    int     fall_down;

    /* the time tunnel */
    struct {
        float dist;                 /* metres the eye is into it */
        float x, y, vx, vy;         /* where the plane is beside the line the tunnel bends about, and how it moves */
        float amp;                  /* how wide its bends are */
        float touch;                /* seconds until the wall can hurt again */
        int   ring;                 /* the ring the plane passed last */
        int   rings, perfect;       /* how many passed, how many of them through the middle */
        int   bonus;                /* points made in it */
        int   lost;                 /* the plane did not come through */
        float off;                  /* how far from the middle, 0..1 (1 = at the wall) */
        float mark_x, mark_y;       /* which way the middle is, metres */
        float throttle;             /* 0..1, resting in the middle as in the sky: slower or faster through it */
        float good;                 /* seconds the last ring through its middle is still cheered */
        int   good_points;          /* and what it was worth */
        vp_vec3 portal, axis;       /* the tunnel's mouth in the era's sky, and the way into it */
        vp_vec3 from;               /* where the plane was when the mouth opened (coming in: where it will be when the era begins) */
        float   len, run;           /* metres from there to the mouth; the share of that way flown at the first speed */
        float   v0;                 /* the way out: the plane's speed until the run into the mouth */
        vp_vec3 base;               /* and where it would be without its loop (for the camera) */
    } tun;

    /* the view and the settings */
    int   chase, hud_only;            /* the view now: from behind; else the display without the cockpit round it */
    int   view_ini;                   /* the view a game starts in (VIEW_)*/
    int   start_lives, difficulty, bonus_plane, kills_per_era;
    float yoke_sens;
    int   control_stick;              /* 0: flown with the yoke; 1: with a stick and buttons, whose switches are eased into a travel */
    float stick_x, stick_y;           /* that travel, -1..1 */
    int   yoke_deadzone, invert_pitch, aim_assist;
    float volume, effects_volume, engine_volume, music_volume;
    int   attract_sound, free_play;
    int   picture_size;               /* percent of the board's range the picture fills, 60..100 */
    int   beam_limit;                 /* thousand board units of beam travel a frame that count as full load, 100..250 */
    char  version[16];                /* as game.ini says, shown small on the title */
    int   level_select;               /* DEBUG_LEVEL_SELECT built in: START opens it (the service menu can turn it off) */
    int   invincible;                 /* for testing: hits, the ground, buildings and the tunnel's wall do no harm */
    int   setup, setup_done;          /* the first start's setup (setup.h) is open; it has been gone through once */
    int   practice, practice_invincible;   /* the setup's test flight is on (start_practice); invincible as it was before */
    int   continues;                  /* the last plane lost, START flies on (without the score) */
    /* Waiting for a continue: while cont_time runs nothing moves (the picture
     * stands as it was); then the plane is lost for good. cont_level is the
     * era it would fly on in, cont_tunnel says it was lost in the tunnel. */
    float cont_time;
    int   cont_level, cont_tunnel;
    int   occlusion;                 /* the buildings are solid: what is behind them is not seen */
    float bright[BRIGHT_COUNT];
    char  btn_fire[128], btn_faster[128], btn_slower[128], btn_rear[128];   /* vpbind.h's controls */
    char  menu_key[128];            /* opens the service menu besides F2 */
    /* Which of the yoke's buttons, by the number it reports, sits where:
     * right trigger, left trigger, right thumb, left thumb (YOKE_PLACES).
     * The yoke does not say; the service menu asks. For how to play. */
    int   yoke_layout[4];
    int   muted, sound_ready, service;
    const char *notice;
    float notice_time;
    int   fx_count;                 /* how much there is in the air that is only for show (new_obj) */
    /* special places shot at: how often each has been hit */
    struct { int cx, cz, hits; } site_hits[24];
    int   site_hit_next;
    const char *cheer;              /* a word zooming out of the picture, see cheer() */
    float cheer_time;

    /* the cabinet */
    vp_credits credits;
    vp_score   table[VP_HISCORE_MAX];
    int        table_count, rank;
    char       initials[4];
    int        initial_pos, initial_letter;
    float      initial_repeat;

    /* attract mode */
    int   page, demo, demo_era;
    float demo_time;

    /* the self test */
    int   selftest, selftest_levels, selftest_deaths, quit;
    unsigned selftest_tailed;       /* steps of the era in which an enemy sat close behind the player, nose on it */
    unsigned selftest_level_frames;
    int   cheat_kills;              /* a testing aid: the kills a level starts with */
    int   cheat_items;              /* a testing aid: every new plane has every item */
    int   debug_sel, debug_players, debug_boss;     /* DEBUG_LEVEL_SELECT: the line chosen, for how many, and the boss to come at once */
    int   debug_y;                  /* the yoke's way the last step, for one move a push */
    int   debug_stage;              /* the debug choice asked for a stage of one boss: cheat_boss2 is taken back when that boss has come */
    int   cheat_weak;               /* a testing aid: every part of a boss falls to one hit, so its stages go by quickly */
    int   cheat_boss2;              /* a testing aid: a boss comes without its guns, in the second part of its fight */
    int   cheat_chutes;             /* a testing aid: two parachutists hang before the plane when an era begins, one of them shot */
    int   cheat_stunt;              /* a testing aid: the self test flies half a loop and rolls upright, every twelve seconds */
    int   cheat_hands_off;          /* a testing aid: the self test does not steer in the time tunnel */
};
extern struct game_state G;

/* --- small things --------------------------------------------------------- */
static inline float frand(void)                /* 0..1 */
{
    G.rng = G.rng * 1664525u + 1013904223u;
    return (float)(G.rng >> 8) / 16777216.0f;
}

static inline int rnd(int n) { return n > 0 ? (int)(frand() * (float)n) % n : 0; }

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static inline float smooth(float u)            /* 0..1, slow at both ends */
{
    u = clampf(u, 0, 1);
    return u * u * (3.0f - 2.0f * u);
}

static inline float wrap_angle(float a)
{
    while (a > PI) a -= PI2;
    while (a < -PI) a += PI2;
    return a;
}

static inline float dist3(vp_vec3 a, vp_vec3 b) { return vp_v3_length(vp_v3_sub(a, b)); }

/* Which way over the ground a heading and pitch go: past straight up or
 * down the plane is on its back and flies the other way. */
static inline float course_of(float heading, float pitch)
{
    return cosf(pitch) >= 0 ? heading : wrap_angle(heading + PI);
}

static inline const struct era *era(void) { return &eras[G.era]; }

static inline const struct difficulty *hard(void) { return &difficulties[G.difficulty]; }

/* --- objects -------------------------------------------------------------- */
/* What is only for show (sparks, the lines of things burst, rings of
 * points) has a ceiling: with more than this in the air the beam and the
 * Pi both fall behind, and the picture stutters. More is not started
 * until some of it has gone. A ring of points counts for four. */
#define FX_BUDGET 170
static inline vp_vec3 forward_of(const obj *o) { return world_forward(o->heading, o->pitch); }

static inline vp_vec3 player_forward(void) { return world_forward(G.p.heading, G.p.pitch); }

static inline vp_vec3 player_velocity(void) { return vp_v3_scale(player_forward(), G.p.speed); }

/* --- the sky: enemies, formations, the boss, their fire -------------------------- */
static inline int player_targetable(void) { return G.p.alive && (G.state == PLAYING || G.state == INTRO); }

/* --- drawing --------------------------------------------------------------------- */
#define CX 2048.0f
#define CY 2048.0f

/* --- what the files offer each other, in the order of the files above ------- */

/* game.c */
void enter(int state);
void notice(const char *text);
void cheer(const char *text);
void add_score(int points);
void start_warp(void);

/* objects.c */
obj *new_obj(int kind);
int count_kind(int kind);
void count_enemies(int *hunters, int *formation);
void clear_kind(int kind);
void clear_all(void);
void burst(vp_vec3 at, vp_vec3 drift, int n, float size, float life, int type, int upward);
void hue_rgb(float h, int *r, int *g, int *b);
void point_ring(vp_vec3 at, vp_vec3 drift, int n, float speed, float life);
void pixel_burst(vp_vec3 at, vp_vec3 drift, float size);
void sphere_burst(vp_vec3 at, vp_vec3 drift, float speed, float life);
void shock_ring(vp_vec3 at, float speed, float life, int ground);
int keeps(int cut, const float *seg, float edge);
float wing_edge(int model_index);
void shatter(const obj *o, float force, int cut, int every);
float boss_turn_time(void);
void sound_at(int fx, vp_vec3 at);
int debug_rows(void);
void debug_row(int row, int *era_of, int *stage);
vp_vec3 robot_point(int limb, vp_vec3 p);
void debris_line(vp_vec3 a, vp_vec3 b, vp_vec3 vel, float life, int rgb);
float draw_scale(const obj *o);
void check_collisions(void);
void update_objects(void);

/* enemies.c */
const obj *nearest_missile(float *distance);
int boss_falling(void);
void steer_toward(obj *o, vp_vec3 target, float turn, float pitch_max);
void update_enemy(obj *o);
/* boss.c */
const struct boss_stage *boss_stage(void);
vp_vec3 boss_part_at(const obj *o, int i);
int boss_stage_left(void);
void boss_begin(obj *o, int one_hit);
int boss_take_shot(obj *o, vp_vec3 from, vp_vec3 to);
void boss_take_blast(obj *o, vp_vec3 at, int hits);
vp_vec3 boss_aim_point(const obj *o);
int boss_touches(const obj *o, vp_vec3 p, float reach);
void boss_push_off(const obj *o, vp_vec3 *p);
void update_boss(obj *o);
/* what the boss fires with, and its escort (enemies.c) */
float boss_shot_speed(float era_speed);
float boss_mg_speed(float era_speed);
float boss_rocket_speed(float era_speed);
void boss_shoot(vp_vec3 from, vp_vec3 dir, float era_speed);
void boss_mg(vp_vec3 from, vp_vec3 dir, float era_speed);
void boss_rocket(vp_vec3 from, vp_vec3 dir, float era_speed);
void boss_ball(vp_vec3 at, vp_vec3 vel, float life);
void boss_bomb(vp_vec3 at, vp_vec3 vel);
void boss_missile_from(vp_vec3 from, float heading, float pitch);
void boss_missile_if_allowed(obj *o);
void spawn_escort(const obj *boss, int k);
/* objects.c */
void boss_hit(obj *o, vp_vec3 at);
void floating_score(vp_vec3 at, int points);
float segment_distance(vp_vec3 a, vp_vec3 b, vp_vec3 p);
void update_missile(obj *o);
void run_sites(void);
void run_spawning(void);

/* player.c */
void damage_player(float amount);
float player_roll(void);
obj *gun_target(float cone, vp_vec3 *aim);
extern const char *const item_names[ITEM_COUNT];
extern const unsigned char item_rgb[ITEM_COUNT][3];
int random_item(void);
void grant_item(int item);
void update_player(vp_input *in);
void hear_the_sky(void);
void update_tunnel(vp_input *in);

/* draw.c */
void text(const char *s, float x, float y, float height, int align, int r, int g, int b);
void draw_rotors(const obj *o, const vp_xf *body);
vp_xf flyer_place(const obj *o);
void draw_burner(const vp_xf *place, float power);
void draw_coin_line(void);
void draw_item(vp_vec3 at, int item, float size, float turn);
void draw_mystery(vp_vec3 at, float size, float turn);
void draw_continue(void);

/* attract.c */
void draw_items_page(int tutorial);
void howto_sounds(float t);
void draw_title(void);
void draw_scores(void);
void draw_cast(void);
void draw_howto(void);
void draw_howto_stick(void);
void draw_initials(void);

#endif /* TP3D_GAME_INTERNAL_H */
