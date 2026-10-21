/* sound.h - effects and the engine, made in code; any effect can be replaced by a WAV file (sound.c). */
#ifndef TP3D_SOUND_H
#define TP3D_SOUND_H

#include <vectorpie/vp.h>

enum {
    FX_SHOT = 0,        /* the player's guns */
    FX_EXPLODE,         /* an enemy goes down */
    FX_BIG_EXPLODE,     /* a boss goes down */
    FX_DEATH,           /* the player goes down */
    FX_WARP,            /* the time warp */
    FX_COIN,
    FX_PICKUP,          /* a parachutist taken aboard */
    FX_MISSILE,         /* a missile launched */
    FX_TINK,            /* a menu step */
    FX_START,           /* a game begins */
    FX_BOSS,            /* the boss arrives */
    FX_HIT_BOSS,        /* a shot lands on the boss */
    FX_ONEUP,
    FX_ENEMY_SHOT,      /* an enemy fires */
    FX_HIT,             /* the player's plane takes a hit */
    FX_STALL,           /* the stall warning's beep */
    FX_BONUS,           /* a whole formation shot down */
    FX_FORMATION,       /* a formation is coming: a row of quick tones */
    FX_WARN,            /* a missile is after the plane */
    FX_PORTAL,          /* the time tunnel's mouth opens */
    FX_WHOOSH,          /* the plane rushes by: out of the tunnel, round its loop */
    FX_BURNER,          /* the afterburner lights */
    FX_FALL,            /* a plane shot down goes into its dive: the howl before it hits */
    FX_RING,            /* a ring of the time tunnel passed: a chime (higher and louder through its middle) */
    FX_SCREAM,          /* a parachutist shot: a scream as he falls (scream.wav and its takes, one at random) */
    FX_KILL,            /* an enemy shot down: a dry crack to say so, at once and near, under its explosion */
    FX_COUNT
};

/* Once the game's audio exists; NULL (no sound device) is fine. */
void sound_init(vp_audio *a);

/* How loud, each 0..1: everything, the effects, the engine, the music. */
void sound_volumes(float master, float effects, float engine, float music);

/* Silence: nothing plays or starts until it is lifted. */
void sound_mute(int on);

void sound_fx(int fx);

/* An effect `gain` (0..1) as loud and at `pitch` (1 as made). Where there
 * are several recordings of it (<name>.wav, <name>_2.wav ... _4.wav) one is
 * taken at random; the enemies' shot is the era's own if it has one
 * (enemy_shot_1910.wav ...). */
void sound_fx_ex(int fx, float gain, float pitch);

/* The same where it happens: `pan` -1 hard left .. 1 hard right of the
 * way the player looks. An explosion, a shot or a hit on the boss is heard
 * from its side (each take is kept once for each ear); the rest in the
 * middle. */
void sound_fx_at(int fx, float gain, float pitch, float pan);

/* Every game step, `dt` seconds: the rest comes back up after something
 * big (a death, a blast near) has pushed it down (ducking). */
void sound_tick(float dt);

/* The afterburner, 0 out .. 1 full: a roar looped while it burns, and a
 * thump as it lights. */
void sound_burner(float level);

/* The era whose enemies' shots are heard (0..4). */
void sound_era(int era);

/* The player's own engine, looped: any number from 0 up starts it, -1
 * stops it. It is the same plane in every era, and sounds the same. */
void sound_engine(int on);

/* How fast the engine runs, 0..1: the plane's speed between its slowest
 * and its fastest. The wind over the plane goes with it. */
void sound_engine_load(float load);

/* Where the plane is heard from: 1 in its cockpit - the engine dull and
 * deep, the wind on the canopy loud - or 0 from outside, the engine bright
 * with its turbine's whine. The two are faded into each other. */
void sound_engine_view(int cockpit);

/* How loud the plane's own engine is, 0..1 of itself: down in the film of
 * the way into an era, coming up as the flight begins. */
void sound_engine_scale(float k);

/* Something has just gone by close: its own sound of going by (FLYBY_),
 * on that side (pan -1..1), gain 0..1, pitch 1 as made (higher for a
 * faster pass). */
enum {
    FLYBY_1910,         /* a rotary engine's propeller: a rattle that drops */
    FLYBY_1940,         /* a V12's propeller: a growl that drops */
    FLYBY_ROTOR,        /* a helicopter: blades thudding over a turbine */
    FLYBY_JET,          /* a jet: a roar and the air torn */
    FLYBY_UFO,          /* a saucer: an electric hum that bends, no air */
    FLYBY_MISSILE,      /* a missile: a sharp hiss */
    FLYBY_BOSS,         /* a boss: a deep drone that shakes */
    FLYBY_KINDS
};
void sound_flyby(int kind, float pan, float gain, float pitch);

/* What else flies has its engine with it, and is heard where it is: to
 * the left or the right, louder the nearer, higher coming and lower going.
 * Every game step: begin, one call for each thing worth hearing, end. The
 * nearest few are heard; one that is no longer named falls silent.
 *   id     which thing it is, the same from step to step (not 0)
 *   kind   SOURCE_: what it sounds like
 *   pan    -1 hard left .. 1 hard right
 *   gain   0..1, how loud before the volume settings
 *   pitch  1 = as recorded */
enum {
    SOURCE_1910 = 0, SOURCE_1940, SOURCE_1970, SOURCE_1982, SOURCE_2001,    /* the eras' engines */
    SOURCE_MISSILE,
    SOURCE_KINDS
};
void sound_sources_begin(void);
void sound_source(int id, int kind, float pan, float gain, float pitch);
void sound_sources_end(void);

/* The music (package/music, made by tools/music_make.py): one piece at a
 * time, each from its start when asked for; the one before fades. The
 * loops go round until another is asked for; the others play once.
 * Silence (sound_mute: the attract without its sound) stops it too. */
enum {
    MUSIC_NONE = 0,
    MUSIC_ATTRACT,      /* the attract mode, a loop */
    MUSIC_HISCORE,      /* entering the initials, a loop */
    MUSIC_LEVELIN,      /* the way into an era: out of the time tunnel */
    MUSIC_APPROACH,     /* the way out of an era, into the tunnel's mouth */
    MUSIC_TUNNEL,       /* the time tunnel, a loop */
    MUSIC_BOSS,         /* a boss comes (BOSSINTRO): threat and an alarm */
    MUSIC_LOUNGE,       /* how to play, before the first era (TUTORIAL): relaxed, a loop */
    MUSIC_START,        /* START pressed: over whatever plays (sound_music_sting) */
    MUSIC_VICTORY,      /* a boss shot down: a fanfare (sound_music_sting) */
    MUSIC_COUNT
};
void sound_music(int piece);

/* A short piece over the music that plays; 0 if there is none to play. */
int sound_music_sting(int piece);

#endif /* TP3D_SOUND_H */
