/* sound.c - effects and engines, synthesized once at start.
 *
 * Every sound is a few lines of arithmetic over time, made into a vp_sound
 * with vp_sound_from_samples, the way the SDK's hello example does it. The
 * engines are one-second loops of whole cycles, so they loop without a
 * click. An effect can be replaced by a recording: a WAV file of its name
 * (fx_file) in the game's sounds/ folder is played instead (package/sounds,
 * see BUILD.md).
 */
#include "sound.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE 22050
#define ENGINE_GAIN   2.6f      /* the player's own engine against its samples: it is heard over everything */
#define SOURCE_VOLUME 0.5f      /* how loud the others' engines are against the effects (2026-10-17: were too loud at 0.9) */
#define BURNER_GAIN   1.6f      /* the afterburner's roar against its samples */
#define SHOT_IN_COCKPIT 0.5f    /* the guns heard from the cockpit: behind the pilot, not at the ear */
#define PAN_WIDTH     0.75f     /* how far to its side a thing on that side is heard */
#define MUSIC_GAIN    1.2f      /* the music against the effects */
#define PI2  6.2831853f

static vp_audio *s_audio;
static vp_sound *s_fx[FX_COUNT];
#define FX_VARIANTS 6
static vp_sound *s_fx_var[FX_COUNT][FX_VARIANTS];   /* more recordings of an effect: one at random */
static int       s_fx_vars[FX_COUNT];
static vp_sound *s_enemy_shot_era[5];               /* the enemies' shot of each era, if recorded */
static int       s_sound_era;
static unsigned  s_pick = 0x9E3779B9u;
static vp_sound *s_fx_ear[FX_COUNT][FX_VARIANTS][2];  /* the takes of what happens somewhere, each all in one ear */
static vp_sound *s_enemy_shot_era_ear[5][2];
static float     s_duck = 1.0f;         /* the rest under something big: 1 as loud as set */
static float     s_crowd = 1.0f;        /* the rest under many things at once: 1 as loud as set */
static float     s_music_volume = 0.7f;
static int       s_music;               /* MUSIC_: the piece asked for */
static vp_voice  s_music_voice;
static float     s_clock;               /* seconds of sound_tick */

/* The mix when much happens at once. Every effect plays; what is kept
 * here, until each ends, is how loud it was and when it began, for how
 * busy everything is - each counts with its weight (fx_weight), most in
 * its first moment - and so how much the next ones and the bed give way. */
#define PLAYED 48
static struct played { vp_voice v[2]; int fx; float gain, from, ends; } s_played[PLAYED];

/* How loud the bed under the effects is - the engines, the wind, the
 * others' engines, the passes: down under something big, and under many. */
static float under(void) { return s_duck * s_crowd; }
static vp_sound *s_burner;              /* the afterburner's roar, looped */
static vp_voice  s_burner_voice;
static float     s_burner_level;
static int       s_burner_lit;
static vp_sound *s_engine;              /* the player's own, heard from outside */
static vp_voice  s_engine_voice;
static vp_sound *s_engine_in, *s_wind;  /* the same from its cockpit, and the wind over it */
static vp_voice  s_engine_in_voice, s_wind_voice;
static vp_sound *s_flyby[FLYBY_KINDS][2];  /* each kind's going by, all in the left ear and all in the right */
static int       s_cockpit = 1;

/* What else flies: each kind of engine twice, once all in the left ear
 * and once all in the right, so a source's place between them is the
 * loudness of its two voices (the SDK has no pan of its own). */
#define SOURCES 5                       /* heard at once */
static vp_sound *s_source_sound[SOURCE_KINDS][2];
static struct source {
    int      id, kind, named;           /* named: asked for this step */
    vp_voice voice[2];
    float    pan, gain, pitch;
} s_sources[SOURCES];
static struct { int id, kind; float pan, gain, pitch; } s_wanted[SOURCES];
static int s_wanted_count;
static int       s_era = -1;
static float     s_master = 1.0f, s_effects = 1.0f, s_engine_level = 1.0f;
static float     s_load;
static float     s_engine_scale = 1.0f;
static int       s_muted;

/* --- generators: each gives one sample for time t (seconds) --------------- */

static unsigned s_noise_state;
static float noise(void)            /* white, -1..1 */
{
    s_noise_state = s_noise_state * 1664525u + 1013904223u;
    return (float)(s_noise_state >> 8) / 8388608.0f - 1.0f;
}

static float smooth_s(float u) { u = u < 0 ? 0 : u > 1.0f ? 1.0f : u; return u * u * (3.0f - 2.0f * u); }

static float s_lp;                  /* a one-pole low-pass, reset per sound */
static float lowpass(float x, float k) { s_lp += (x - s_lp) * k; return s_lp; }

static float env(float t, float attack, float length)   /* up fast, down to 0 at `length` */
{
    float a = t < attack ? t / attack : 1.0f;
    float d = 1.0f - t / length;
    return d < 0 ? 0 : a * d;
}

/* Weight. A thump is a sine that falls from f0 to f1 (its phase the
 * integral of that, so it does not warble); what is added up is pushed
 * through a soft limit, which makes it loud without clipping hard and
 * gives the bass overtones a small speaker still plays. */
static float thump(float t, float f0, float f1, float k)
{
    return sinf(PI2 * (f1 * t + (f0 - f1) * (1.0f - expf(-k * t)) / k));
}

static float soft(float x) { return tanhf(x); }

static float s_lp2;                 /* a second low-pass, for a sound that needs two */
static float lowpass2(float x, float k) { s_lp2 += (x - s_lp2) * k; return s_lp2; }

static float gen_shot(float t)
{
    /* the old zap, on top of a body that kicks */
    float f = 900.0f - 2500.0f * t;
    float zap = (0.35f * sinf(PI2 * f * t) + 0.25f * lowpass(noise(), 0.4f)) * env(t, 0.002f, 0.09f);
    float body = 0.9f * thump(t, 170.0f, 55.0f, 38.0f) * expf(-26.0f * t);
    return soft(1.7f * (zap + body)) * env(t, 0.001f, 0.17f);
}

static float gen_enemy_shot(float t)
{
    float f = 500.0f - 1200.0f * t;
    float zap = (0.25f * sinf(PI2 * f * t) + 0.15f * lowpass(noise(), 0.3f)) * env(t, 0.002f, 0.08f);
    float body = 0.55f * thump(t, 125.0f, 48.0f, 34.0f) * expf(-28.0f * t);
    return soft(1.5f * (zap + body)) * env(t, 0.001f, 0.15f);
}

static float gen_explode(float t)
{
    /* a crack, a boom that falls to 32 Hz, and the rumble closing in after it */
    float crack = 0.7f * lowpass(noise(), 0.5f) * expf(-38.0f * t);
    float boom = 1.0f * thump(t, 115.0f, 32.0f, 9.0f) * expf(-4.4f * t);
    float rumble = 0.9f * lowpass2(noise(), 0.035f + 0.14f * expf(-6.0f * t)) * expf(-3.6f * t);
    return soft(1.6f * (crack + boom + rumble)) * env(t, 0.001f, 1.0f);
}

static float gen_big_explode(float t)
{
    /* the same, longer and lower, with a second blow after the first */
    float crack = 0.7f * lowpass(noise(), 0.45f) * expf(-25.0f * t);
    float boom = 1.0f * thump(t, 85.0f, 26.0f, 4.5f) * expf(-1.7f * t);
    float again = t > 0.38f ? 0.7f * thump(t - 0.38f, 70.0f, 28.0f, 6.0f) * expf(-3.0f * (t - 0.38f)) : 0;
    float rumble = 1.0f * lowpass2(noise(), 0.03f + 0.07f * expf(-2.0f * t)) * expf(-1.7f * t);
    return soft(1.5f * (crack + boom + again + rumble)) * env(t, 0.001f, 2.6f);
}

static float gen_death(float t)
{
    float f = 600.0f * expf(-2.5f * t);
    return (0.4f * sinf(PI2 * f * t) + 0.6f * lowpass(noise(), 0.15f)) * expf(-2.2f * t);
}

#define WARP_SECONDS 14.0f      /* as long as the game's journey through time (WARP_TIME) */

static float s_warp_phase;
static float gen_warp(float t)
{
    /* a sweep that rises all the way, throbbing faster and faster, with a
     * rush of noise that opens up; it fades in the last second */
    float f = 90.0f * expf(0.21f * t) * (1.0f + 0.03f * sinf(PI2 * 9.0f * t));
    float throb = 0.75f + 0.25f * sinf(PI2 * (2.0f + 0.4f * t) * t);
    float a = t < 0.3f ? t / 0.3f : t < WARP_SECONDS - 1.0f ? 1.0f : WARP_SECONDS - t;
    if (t == 0) s_warp_phase = 0;
    s_warp_phase += f / RATE;           /* the pitch changes: the phase is summed, not multiplied */
    if (a < 0) a = 0;
    return (0.28f * sinf(PI2 * s_warp_phase) + 0.14f * sinf(PI2 * s_warp_phase * 1.5f)
            + 0.12f * lowpass(noise(), 0.03f + 0.025f * t)) * a * throb;
}

static float gen_hit(float t)
{
    float bang = (0.5f * lowpass(noise(), 0.5f) + 0.3f * sinf(PI2 * 140.0f * t)) * env(t, 0.001f, 0.18f);
    float body = 0.9f * thump(t, 150.0f, 50.0f, 26.0f) * expf(-15.0f * t);
    return soft(1.6f * (bang + body)) * env(t, 0.001f, 0.28f);
}

static float gen_stall(float t)
{
    return 0.3f * (sinf(PI2 * 620.0f * t) > 0 ? 1.0f : -1.0f) * env(t, 0.002f, 0.12f);
}

static float gen_formation(float t)
{
    /* eight quick tones, high and low by turns */
    int i = (int)(t / 0.06f);
    float f = (i & 1) ? 1320.0f : 990.0f, in = t - (float)i * 0.06f;
    return 0.3f * (sinf(PI2 * f * t) > 0 ? 1.0f : -1.0f) * (in < 0.045f ? 1.0f : 0.0f) * (t < 0.48f ? 1.0f : 0.0f);
}

static float gen_bonus(float t)
{
    static const float notes[6] = { 784.0f, 988.0f, 1175.0f, 1568.0f, 1175.0f, 1568.0f };
    int i = (int)(t / 0.1f);
    if (i > 5) i = 5;
    return 0.33f * (sinf(PI2 * notes[i] * t) + 0.25f * sinf(PI2 * notes[i] * 2.0f * t)) * env(t, 0.003f, 0.7f);
}

static float gen_coin(float t)
{
    float f = t < 0.08f ? 1200.0f : 1600.0f;
    return 0.4f * sinf(PI2 * f * t) * env(t, 0.002f, 0.2f);
}

static float gen_pickup(float t)
{
    static const float notes[4] = { 660.0f, 880.0f, 1100.0f, 1320.0f };
    int i = (int)(t / 0.07f);
    if (i > 3) i = 3;
    return 0.35f * sinf(PI2 * notes[i] * t) * env(t, 0.002f, 0.32f);
}

static float gen_missile(float t)
{
    return 0.5f * lowpass(noise(), 0.05f + 0.5f * t) * env(t, 0.05f, 0.6f);
}

static float gen_tink(float t)
{
    return 0.3f * sinf(PI2 * 1800.0f * t) * env(t, 0.001f, 0.05f);
}

static float gen_start(float t)
{
    static const float notes[6] = { 440.0f, 554.0f, 659.0f, 880.0f, 659.0f, 880.0f };
    int i = (int)(t / 0.12f);
    if (i > 5) i = 5;
    return 0.35f * (sinf(PI2 * notes[i] * t) + 0.3f * sinf(PI2 * notes[i] * 2.0f * t)) * env(t, 0.005f, 0.8f);
}

static float gen_boss(float t)
{
    /* an alarm: two tones, four times */
    float f = ((int)(t / 0.15f) & 1) ? 520.0f : 390.0f;
    return 0.35f * sinf(PI2 * f * t) * (t < 1.2f ? 1.0f : 0.0f) * (0.7f + 0.3f * sinf(PI2 * 30.0f * t));
}

static float gen_hit_boss(float t)
{
    return (0.3f * sinf(PI2 * 220.0f * t) + 0.3f * lowpass(noise(), 0.2f)) * env(t, 0.002f, 0.15f);
}

static float gen_oneup(float t)
{
    static const float notes[8] = { 523.0f, 659.0f, 784.0f, 1047.0f, 784.0f, 1047.0f, 1319.0f, 1568.0f };
    int i = (int)(t / 0.09f);
    if (i > 7) i = 7;
    return 0.35f * sinf(PI2 * notes[i] * t) * env(t, 0.003f, 0.75f);
}

/* The engines. Every frequency is a whole number of cycles a second, so a
 * one-second loop joins up. */
static float gen_engine_1910(float t)       /* a rotary engine's putter */
{
    float w = PI2 * 24.0f * t;
    float pulse = sinf(w) + 0.6f * sinf(2 * w) + 0.4f * sinf(3 * w) + 0.3f * sinf(4 * w) + 0.2f * sinf(5 * w);
    return 0.18f * pulse + 0.08f * lowpass(noise(), 0.1f);
}

static float gen_engine_1940(float t)       /* a V12's drone */
{
    float w = PI2 * 60.0f * t;
    float saw = sinf(w) + 0.5f * sinf(2 * w) + 0.33f * sinf(3 * w) + 0.25f * sinf(4 * w) + 0.2f * sinf(5 * w);
    return 0.14f * saw + 0.05f * sinf(PI2 * 120.0f * t) + 0.04f * lowpass(noise(), 0.15f);
}

static float gen_engine_1970(float t)       /* rotor chop over a turbine */
{
    float chop = sinf(PI2 * 11.0f * t);
    chop = chop < 0 ? 0 : chop * chop * chop;
    return 0.35f * lowpass(noise(), 0.12f) * (0.3f + 0.7f * chop) + 0.06f * sinf(PI2 * 90.0f * t);
}

static float gen_engine_1982(float t)       /* a jet's roar and whine */
{
    return 0.22f * lowpass(noise(), 0.25f) + 0.05f * sinf(PI2 * 310.0f * t) + 0.03f * sinf(PI2 * 620.0f * t);
}

static float gen_engine_2001(float t)       /* a warble */
{
    float mod = 3.0f * sinf(PI2 * 4.0f * t);
    return 0.16f * sinf(PI2 * 220.0f * t + mod) + 0.08f * sinf(PI2 * 330.0f * t - mod) + 0.05f * sinf(PI2 * 55.0f * t);
}

static float gen_engine_player(float t)     /* the plane from the future: a turbine's whine over a hum */
{
    float hum = 0.10f * sinf(PI2 * 70.0f * t) + 0.05f * sinf(PI2 * 140.0f * t + 0.6f * sinf(PI2 * 3.0f * t));
    float whine = 0.05f * sinf(PI2 * 880.0f * t) + 0.03f * sinf(PI2 * 1320.0f * t);
    return hum + whine + 0.10f * lowpass(noise(), 0.3f);
}

/* The same engine from its cockpit: the airframe takes the top off it -
 * a deep hum and its octave, a sub, a rumble, the turbine only faint -
 * and it shakes a little. Whole cycles a second, so it loops. */
static float gen_engine_cockpit(float t)
{
    float hum = 0.16f * sinf(PI2 * 70.0f * t) + 0.09f * sinf(PI2 * 140.0f * t + 0.5f * sinf(PI2 * 3.0f * t))
              + 0.07f * sinf(PI2 * 35.0f * t);
    float rumble = 0.16f * lowpass(noise(), 0.04f);
    float whine = 0.012f * sinf(PI2 * 880.0f * t);
    return (hum + rumble + whine) * (0.88f + 0.12f * sinf(PI2 * 6.0f * t));
}

/* The wind on the plane: a deep rush and a higher hiss over it, gusting
 * once a second. The game raises and lowers it with the speed. */
static float gen_wind(float t)
{
    float gust = 0.8f + 0.2f * sinf(PI2 * 1.0f * t) + 0.08f * sinf(PI2 * 3.0f * t);
    return (0.45f * lowpass(noise(), 0.05f) + 0.12f * lowpass2(noise(), 0.35f)) * gust;
}

/* Things going by (FLYBY_). A plane is heard as it is: its own engine,
 * and the air. All of them swell as they come and die as they go (fly_env)
 * and drop as they pass, their Doppler fall (fly_dop: what was a little
 * high becomes a little low); `u` is the time over the whole of it.
 * A band of noise is the difference of two low-passes of the same noise. */
#define FLYBY_SECONDS 1.6f
static float s_fly_ph, s_fly_ph2, s_fly_rumble;
static float fly_env(float u) { return u < 0.38f ? (u / 0.38f) * (u / 0.38f) : expf(-(u - 0.38f) * 3.2f); }
static float fly_dop(float u) { return 1.18f - 0.33f * smooth_s((u - 0.25f) / 0.3f); }
static float fly_air(float x, float u)     /* the air: bright coming, dark going */
{
    float k = 0.06f + 0.42f * (1.0f - smooth_s((u - 0.25f) / 0.35f));
    return lowpass(x, k) - lowpass2(x, k * 0.22f);
}
static void fly_reset(float t) { if (t == 0) s_fly_ph = s_fly_ph2 = s_fly_rumble = 0; }

static float gen_flyby_1910(float t)        /* a rotary engine: a rattle of blade beats, each a little crack */
{
    float u = t / FLYBY_SECONDS, a = fly_env(u), x = noise(), p, beat;
    fly_reset(t);
    s_fly_ph += 48.0f * fly_dop(u) / RATE;
    p = s_fly_ph - floorf(s_fly_ph);
    beat = expf(-p * 9.0f) * (0.7f * x + 0.5f * sinf(PI2 * s_fly_ph * 3.0f));
    return soft(3.2f * a * (0.6f * beat + 0.35f * fly_air(x, u)));
}

static float gen_flyby_1940(float t)        /* a V12 and its propeller: a growl of the engine's harmonics */
{
    float u = t / FLYBY_SECONDS, a = fly_env(u), x = noise(), w, growl;
    fly_reset(t);
    s_fly_ph += 92.0f * fly_dop(u) / RATE;
    w = PI2 * s_fly_ph;
    growl = sinf(w) + 0.6f * sinf(2 * w) + 0.45f * sinf(3 * w) + 0.3f * sinf(4 * w) + 0.2f * sinf(6 * w);
    growl *= 0.75f + 0.25f * sinf(PI2 * s_fly_ph / 3.0f);   /* the propeller's beat over it */
    return soft(2.4f * a * (0.2f * growl + 0.45f * fly_air(x, u)));
}

static float gen_flyby_rotor(float t)       /* blades thudding over a turbine's whine */
{
    float u = t / FLYBY_SECONDS, a = fly_env(u), x = noise(), chop;
    fly_reset(t);
    s_fly_ph += 13.0f * fly_dop(u) / RATE;
    s_fly_ph2 += 640.0f * fly_dop(u) / RATE;
    chop = s_fly_ph - floorf(s_fly_ph);
    chop = expf(-chop * 7.0f);
    s_fly_rumble += (x - s_fly_rumble) * 0.04f;
    return soft(7.0f * a * (1.4f * s_fly_rumble * chop + 0.25f * fly_air(x, u) * chop + 0.03f * sinf(PI2 * s_fly_ph2)));
}

static float gen_flyby_jet(float t)         /* the roar, and the air torn right at the pass */
{
    float u = t / FLYBY_SECONDS, a = fly_env(u), x = noise(), roar, tear;
    fly_reset(t);
    s_fly_rumble += (x - s_fly_rumble) * 0.012f;
    roar = s_fly_rumble * (u < 0.45f ? a : expf(-(u - 0.45f) * 2.0f));
    tear = (x - lowpass2(x, 0.5f)) * expf(-((u - 0.38f) * (u - 0.38f)) / 0.004f);
    return 0.75f * soft(2.2f * (0.75f * (lowpass(x, 0.06f + 0.42f * (1.0f - smooth_s((u - 0.25f) / 0.35f))) - s_lp2 * 0.0f) * a
                        + 1.6f * roar + 0.35f * tear));
}

static float gen_flyby_ufo(float t)         /* no air: an electric hum that bends as it passes, a shimmer over it */
{
    float u = t / FLYBY_SECONDS, a = fly_env(u), d = fly_dop(u), hum;
    fly_reset(t);
    s_fly_ph += 120.0f * d * (1.0f + 0.03f * sinf(PI2 * 7.0f * t)) / RATE;
    s_fly_ph2 += 361.0f * d / RATE;
    hum = 0.35f * sinf(PI2 * s_fly_ph) + 0.18f * sinf(PI2 * s_fly_ph * 2.0f)
        + 0.15f * sinf(PI2 * s_fly_ph2) * sinf(PI2 * s_fly_ph * 0.5f);
    return soft(1.5f * a * hum);
}

static float gen_flyby_missile(float t)     /* a rocket: a sharp, bright hiss, quick */
{
    float u = t / FLYBY_SECONDS, a = u < 0.22f ? (u / 0.22f) : expf(-(u - 0.22f) * 7.0f), x = noise();
    float hiss = x - lowpass(x, 0.35f + 0.3f * (1.0f - smooth_s((u - 0.15f) / 0.2f)));
    return soft(2.2f * a * (0.6f * hiss + 0.2f * lowpass2(x, 0.1f)));
}

static float gen_flyby_boss(float t)        /* something huge: a deep drone that shakes, and a long roar */
{
    float u = t / FLYBY_SECONDS, a = u < 0.45f ? (u / 0.45f) : expf(-(u - 0.45f) * 2.2f), x = noise();
    fly_reset(t);
    s_fly_ph += 44.0f * fly_dop(u) / RATE;
    s_fly_rumble += (x - s_fly_rumble) * 0.008f;
    return soft(2.0f * a * (0.35f * sinf(PI2 * s_fly_ph) * (0.8f + 0.2f * sinf(PI2 * s_fly_ph / 4.0f))
                            + 2.2f * s_fly_rumble + 0.2f * fly_air(x, u)));
}

static float gen_missile_loop(float t)      /* a rocket motor: a hiss that flutters */
{
    return 0.35f * lowpass(noise(), 0.45f) * (0.7f + 0.3f * sinf(PI2 * 23.0f * t));
}

static float gen_kill(float t)              /* an enemy shot down (stands in for kill.wav): a thump and a crack */
{
    return 0.5f * (sinf(PI2 * 70.0f * t) * expf(-t / 0.08f) + lowpass(noise(), 0.6f) * expf(-t / 0.03f));
}

static float gen_scream(float t)            /* a parachutist shot (stands in for scream.wav): a voice falling away, wavering */
{
    float f = 700.0f * expf(-0.9f * t), v = sinf(PI2 * f * t + 4.0f * sinf(PI2 * 6.0f * t));
    return 0.35f * (v + 0.3f * sinf(2.0f * PI2 * f * t)) * env(t, 0.03f, 0.9f);
}

static float gen_ring(float t)              /* a ring passed: a bright chime, two bells a fifth apart */
{
    float e = expf(-t / 0.22f) * (t < 0.003f ? t / 0.003f : 1.0f);
    return 0.45f * e * (sinf(PI2 * 1320.0f * t) + 0.6f * sinf(PI2 * 1980.0f * t) + 0.25f * sinf(PI2 * 2640.0f * t) * expf(-t / 0.06f));
}

static float gen_fall(float t)              /* a plane going down (stands in for fall.wav): a high whistle that wavers, air over it */
{
    float tone = sinf(PI2 * 1100.0f * t + 3.0f * sinf(PI2 * 7.0f * t));
    return 0.4f * (tone + 0.25f * sinf(PI2 * 2200.0f * t + 6.0f * sinf(PI2 * 7.0f * t)) + 0.3f * lowpass(noise(), 0.3f));
}

/* The time tunnel's mouth opening: a deep whump, then two voices a hair
 * apart that rise from deep to high, trembling, with a glassy shimmer over
 * them, all of it in an echo that goes on after them. */
#define PORTAL_SECONDS 3.2f
#define ECHO 2646                    /* samples: 0.12 s */
static float s_portal_ph, s_portal_ph2, s_echo[ECHO];
static int s_echo_at;
static float gen_portal(float t)
{
    float rise = smooth_s(t / 2.0f), f = 110.0f * powf(2.0f, 3.4f * rise) * (1.0f + 0.025f * sinf(PI2 * 7.0f * t));
    float a = t < 0.05f ? t / 0.05f : t < 2.0f ? 1.0f : 1.0f - (t - 2.0f) / 0.7f, v, out;
    if (t == 0) { int i; s_portal_ph = s_portal_ph2 = 0; s_echo_at = 0; for (i = 0; i < ECHO; i++) s_echo[i] = 0; }
    if (a < 0) a = 0;
    s_portal_ph += f / RATE;
    s_portal_ph2 += f * 1.012f / RATE;
    v = 0.22f * sinf(PI2 * s_portal_ph) + 0.18f * sinf(PI2 * s_portal_ph2)
      + 0.12f * sinf(PI2 * s_portal_ph * 2.0f) * sinf(PI2 * s_portal_ph * 3.01f)     /* the shimmer */
      + 0.06f * lowpass(noise(), 0.05f + 0.2f * rise);
    v *= a;
    v += 0.7f * thump(t, 70.0f, 32.0f, 14.0f) * expf(-5.0f * t);                       /* the whump as it opens */
    out = v + 0.5f * s_echo[s_echo_at];
    s_echo[s_echo_at] = out;
    s_echo_at = (s_echo_at + 1) % ECHO;
    return soft(out);
}

/* A rush of air going by: noise through a filter that opens and closes,
 * a low swell under it. */
static float gen_whoosh(float t)
{
    float u = t / 1.3f, bell = u < 1.0f ? sinf(3.14159f * u) : 0;
    float k = 0.015f + 0.30f * bell * bell;
    return soft(1.4f * (0.55f * lowpass(noise(), k) * bell * bell + 0.25f * sinf(PI2 * (90.0f - 50.0f * u) * t) * bell));
}

static float gen_warn(float t)
{
    float f = t < 0.07f ? 1480.0f : 1110.0f;
    return 0.3f * (sinf(PI2 * f * t) > 0 ? 1.0f : -1.0f) * (t < 0.14f ? 1.0f : 0.0f);
}

/* The file names a recording of each effect has (sounds/<name>.wav). */
static const char *const fx_file[FX_COUNT] = {
    [FX_SHOT] = "shot", [FX_EXPLODE] = "explode", [FX_BIG_EXPLODE] = "big_explode", [FX_DEATH] = "death",
    [FX_WARP] = "warp", [FX_COIN] = "coin", [FX_PICKUP] = "pickup", [FX_MISSILE] = "missile", [FX_TINK] = "tink",
    [FX_START] = "start", [FX_BOSS] = "boss", [FX_HIT_BOSS] = "hit_boss", [FX_ONEUP] = "oneup",
    [FX_ENEMY_SHOT] = "enemy_shot", [FX_HIT] = "hit", [FX_STALL] = "stall", [FX_BONUS] = "bonus",
    [FX_FORMATION] = "formation", [FX_WARN] = "warn", [FX_PORTAL] = "portal", [FX_WHOOSH] = "whoosh",
    [FX_BURNER] = "burner", [FX_FALL] = "fall", [FX_RING] = "ring", [FX_SCREAM] = "scream", [FX_KILL] = "kill",
};

/* What happens somewhere, and is heard from its side. */
static int placed(int fx) { return fx == FX_EXPLODE || fx == FX_BIG_EXPLODE || fx == FX_ENEMY_SHOT || fx == FX_HIT_BOSS || fx == FX_FALL || fx == FX_SCREAM || fx == FX_KILL; }

/* --- making them --------------------------------------------------------- */

/* A testing aid: with TP3D_SOUNDS_OUT set to a folder, every effect made
 * here is also written there as <name>.wav (16 bit, mono), to be listened
 * to, or used as the start of a recording of one's own. */
static const char *s_dump_name;
static void dump_wav(const float *buf, int n)
{
    const char *dir = getenv("TP3D_SOUNDS_OUT");
    char path[512];
    FILE *f;
    unsigned char h[44];
    unsigned data = (unsigned)n * 2u, i;
    if (!dir || !s_dump_name) return;
    snprintf(path, sizeof path, "%s/%s.wav", dir, s_dump_name);
    if (!(f = fopen(path, "wb"))) return;
    memcpy(h, "RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x01\0\x01\0\0\0\0\0\0\0\0\0\x02\0\x10\0data\0\0\0\0", 44);
    h[4] = (unsigned char)(data + 36); h[5] = (unsigned char)((data + 36) >> 8); h[6] = (unsigned char)((data + 36) >> 16); h[7] = (unsigned char)((data + 36) >> 24);
    h[24] = (unsigned char)RATE; h[25] = (unsigned char)(RATE >> 8); h[26] = (unsigned char)(RATE >> 16);
    h[28] = (unsigned char)(RATE * 2); h[29] = (unsigned char)((RATE * 2) >> 8); h[30] = (unsigned char)((RATE * 2) >> 16);
    h[40] = (unsigned char)data; h[41] = (unsigned char)(data >> 8); h[42] = (unsigned char)(data >> 16); h[43] = (unsigned char)(data >> 24);
    fwrite(h, 1, 44, f);
    for (i = 0; i < (unsigned)n; i++) {
        int v = (int)(buf[i] * 32767.0f);
        unsigned char b[2] = { (unsigned char)(v & 255), (unsigned char)((v >> 8) & 255) };
        fwrite(b, 1, 2, f);
    }
    fclose(f);
}

static vp_sound *synth(float seconds, float (*gen)(float t))
{
    int n = (int)(seconds * RATE), i;
    float *buf = malloc(sizeof(float) * (size_t)n);
    vp_sound *s = NULL;

    if (!buf) return NULL;
    s_noise_state = 0x1234567u;
    s_lp = s_lp2 = 0;
    for (i = 0; i < n; i++) {
        float v = gen((float)i / RATE);
        buf[i] = v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
    }
    dump_wav(buf, n);
    s_dump_name = NULL;
    vp_sound_from_samples(s_audio, buf, n, 1, RATE, &s);
    free(buf);
    return s;
}

/* `seconds` of a sound all in one ear: `right` 0 the left, 1 the right. */
static vp_sound *synth_ear_for(float seconds, float (*gen)(float t), int right)
{
    int n = (int)(seconds * RATE), i;
    float *buf = malloc(sizeof(float) * 2u * (size_t)n);
    vp_sound *s = NULL;

    if (!buf) return NULL;
    s_noise_state = 0x1234567u;
    s_lp = s_lp2 = 0;
    for (i = 0; i < n; i++) {
        float v = gen((float)i / RATE);
        v = v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
        buf[2 * i] = right ? 0.0f : v;
        buf[2 * i + 1] = right ? v : 0.0f;
    }
    vp_sound_from_samples(s_audio, buf, n, 2, RATE, &s);
    free(buf);
    return s;
}

/* A recording standing in for a loop or a sound made here, if there is one
 * (sounds/<name>.wav). */
static vp_sound *recording(const char *name, vp_sound *made)
{
    char rel[64], path[512];
    vp_sound *rec = NULL;
    snprintf(rel, sizeof rel, "sounds/%s.wav", name);
    if (vp_sound_load(s_audio, vp_asset_path(rel, path, sizeof path), &rec) == VP_OK && rec) {
        fprintf(stderr, "Time Pilot 3D: sound %s from %s\n", name, rel);
        return rec;
    }
    return made;
}

/* A recording read here, so that it can be put in one ear: a WAV file of
 * 16-bit PCM (mono, or stereo made mono), as tools/sounds_make.py writes
 * them. Gives the samples (malloc'd, the caller frees) and their count and
 * rate, or NULL. */
static float *read_wav16(const char *path, int *frames, int *rate)
{
    FILE *f = fopen(path, "rb");
    unsigned char h[12], ck[8];
    int channels = 0, bits = 0;
    float *out = NULL;
    if (!f) return NULL;
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { fclose(f); return NULL; }
    while (fread(ck, 1, 8, f) == 8) {
        unsigned size = ck[4] | (ck[5] << 8) | (ck[6] << 16) | ((unsigned)ck[7] << 24);
        if (!memcmp(ck, "fmt ", 4)) {
            unsigned char fm[16];
            if (size < 16 || fread(fm, 1, 16, f) != 16) break;
            if ((fm[0] | (fm[1] << 8)) != 1) break;                 /* PCM only */
            channels = fm[2] | (fm[3] << 8);
            *rate = (int)(fm[4] | (fm[5] << 8) | (fm[6] << 16) | ((unsigned)fm[7] << 24));
            bits = fm[14] | (fm[15] << 8);
            fseek(f, (long)(size - 16 + (size & 1)), SEEK_CUR);
        } else if (!memcmp(ck, "data", 4)) {
            int n, i;
            unsigned char *raw;
            if (bits != 16 || channels < 1 || channels > 2) break;
            n = (int)(size / (2u * (unsigned)channels));
            raw = malloc(size);
            out = malloc(sizeof(float) * (size_t)(n > 0 ? n : 1));
            if (!raw || !out || fread(raw, 1, size, f) != size) { free(raw); free(out); out = NULL; break; }
            for (i = 0; i < n; i++) {
                float v = 0;
                int c;
                for (c = 0; c < channels; c++) {
                    const unsigned char *b = raw + 2 * (i * channels + c);
                    v += (float)(short)(b[0] | (b[1] << 8)) / 32768.0f;
                }
                out[i] = v / (float)channels;
            }
            free(raw);
            *frames = n;
            break;
        } else {
            fseek(f, (long)(size + (size & 1)), SEEK_CUR);
        }
    }
    fclose(f);
    return out;
}

/* A recording of `name` (sounds/<name>.wav) put all in one ear, or NULL. */
static vp_sound *recording_ear(const char *name, int right)
{
    char rel[64], path[512];
    int n = 0, rate = 0, i;
    float *mono, *buf;
    vp_sound *s = NULL;
    snprintf(rel, sizeof rel, "sounds/%s.wav", name);
    if (!(mono = read_wav16(vp_asset_path(rel, path, sizeof path), &n, &rate)) || n < 1) return NULL;
    buf = malloc(sizeof(float) * 2u * (size_t)n);
    if (buf) {
        for (i = 0; i < n; i++) { buf[2 * i] = right ? 0.0f : mono[i]; buf[2 * i + 1] = right ? mono[i] : 0.0f; }
        vp_sound_from_samples(s_audio, buf, n, 2, rate, &s);
        free(buf);
    }
    free(mono);
    if (s && !right) fprintf(stderr, "Time Pilot 3D: sound %s from %s, heard from its side\n", name, rel);
    return s;
}

/* A one-second loop all in one ear: `right` 0 the left, 1 the right. */
static vp_sound *synth_ear(float (*gen)(float t), int right)
{
    int n = RATE, i;
    float *buf = malloc(sizeof(float) * 2u * (size_t)n);
    vp_sound *s = NULL;

    if (!buf) return NULL;
    s_noise_state = 0x1234567u;
    s_lp = 0;
    for (i = 0; i < n; i++) {
        float v = gen((float)i / RATE);
        v = v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
        buf[2 * i] = right ? 0.0f : v;
        buf[2 * i + 1] = right ? v : 0.0f;
    }
    vp_sound_from_samples(s_audio, buf, n, 2, RATE, &s);
    free(buf);
    return s;
}

void sound_init(vp_audio *a)
{
    s_audio = a;
    if (!a) return;
    vp_audio_set_master(a, s_master);
    s_dump_name = fx_file[FX_SHOT]; s_fx[FX_SHOT]        = synth(0.17f, gen_shot);
    s_dump_name = fx_file[FX_ENEMY_SHOT]; s_fx[FX_ENEMY_SHOT]  = synth(0.15f, gen_enemy_shot);
    s_dump_name = fx_file[FX_EXPLODE]; s_fx[FX_EXPLODE]     = synth(1.0f,  gen_explode);
    s_dump_name = fx_file[FX_BIG_EXPLODE]; s_fx[FX_BIG_EXPLODE] = synth(2.6f,  gen_big_explode);
    s_dump_name = fx_file[FX_DEATH]; s_fx[FX_DEATH]       = synth(1.6f,  gen_death);
    s_dump_name = fx_file[FX_WARP]; s_fx[FX_WARP]        = synth(WARP_SECONDS, gen_warp);
    s_dump_name = fx_file[FX_HIT]; s_fx[FX_HIT]         = synth(0.28f, gen_hit);
    s_dump_name = fx_file[FX_STALL]; s_fx[FX_STALL]       = synth(0.13f, gen_stall);
    s_dump_name = fx_file[FX_BONUS]; s_fx[FX_BONUS]       = synth(0.7f,  gen_bonus);
    s_dump_name = fx_file[FX_FORMATION]; s_fx[FX_FORMATION]   = synth(0.5f,  gen_formation);
    s_dump_name = fx_file[FX_COIN]; s_fx[FX_COIN]        = synth(0.22f, gen_coin);
    s_dump_name = fx_file[FX_PICKUP]; s_fx[FX_PICKUP]      = synth(0.33f, gen_pickup);
    s_dump_name = fx_file[FX_MISSILE]; s_fx[FX_MISSILE]     = synth(0.6f,  gen_missile);
    s_dump_name = fx_file[FX_TINK]; s_fx[FX_TINK]        = synth(0.06f, gen_tink);
    s_dump_name = fx_file[FX_START]; s_fx[FX_START]       = synth(0.8f,  gen_start);
    s_dump_name = fx_file[FX_BOSS]; s_fx[FX_BOSS]        = synth(1.2f,  gen_boss);
    s_dump_name = fx_file[FX_HIT_BOSS]; s_fx[FX_HIT_BOSS]    = synth(0.16f, gen_hit_boss);
    s_dump_name = fx_file[FX_ONEUP]; s_fx[FX_ONEUP]       = synth(0.75f, gen_oneup);
    s_dump_name = fx_file[FX_WARN]; s_fx[FX_WARN] = synth(0.15f, gen_warn);
    s_dump_name = fx_file[FX_PORTAL]; s_fx[FX_PORTAL] = synth(PORTAL_SECONDS, gen_portal);
    s_dump_name = fx_file[FX_WHOOSH]; s_fx[FX_WHOOSH] = synth(1.3f, gen_whoosh);
    s_dump_name = fx_file[FX_BURNER]; s_fx[FX_BURNER] = synth(0.8f, gen_whoosh);
    s_dump_name = fx_file[FX_FALL]; s_fx[FX_FALL] = synth(2.5f, gen_fall);
    s_dump_name = fx_file[FX_RING]; s_fx[FX_RING] = synth(0.9f, gen_ring);
    s_dump_name = fx_file[FX_SCREAM]; s_fx[FX_SCREAM] = synth(1.2f, gen_scream);
    s_dump_name = fx_file[FX_KILL]; s_fx[FX_KILL] = synth(0.3f, gen_kill);
    {
        /* A recording in the game's sounds/ folder by an effect's name
         * stands in for the one made here. */
        char rel[64], path[512];
        int k;
        for (k = 0; k < FX_COUNT; k++) {
            vp_sound *rec = NULL;
            if (!fx_file[k]) continue;
            snprintf(rel, sizeof rel, "sounds/%s.wav", fx_file[k]);
            if (vp_sound_load(a, vp_asset_path(rel, path, sizeof path), &rec) == VP_OK && rec) {
                s_fx[k] = rec;
                fprintf(stderr, "Time Pilot 3D: sound %s from %s\n", fx_file[k], rel);
                if (placed(k)) { s_fx_ear[k][0][0] = recording_ear(fx_file[k], 0); s_fx_ear[k][0][1] = recording_ear(fx_file[k], 1); }
            }
            /* and its other takes, <name>_2 to _6 */
            {
                int v;
                s_fx_var[k][0] = s_fx[k];
                s_fx_vars[k] = 1;
                for (v = 2; v <= FX_VARIANTS; v++) {
                    rec = NULL;
                    snprintf(rel, sizeof rel, "sounds/%s_%d.wav", fx_file[k], v);
                    if (vp_sound_load(a, vp_asset_path(rel, path, sizeof path), &rec) == VP_OK && rec) {
                        if (placed(k)) {
                            char take[48];
                            snprintf(take, sizeof take, "%s_%d", fx_file[k], v);
                            s_fx_ear[k][s_fx_vars[k]][0] = recording_ear(take, 0);
                            s_fx_ear[k][s_fx_vars[k]][1] = recording_ear(take, 1);
                        }
                        s_fx_var[k][s_fx_vars[k]++] = rec;
                        fprintf(stderr, "Time Pilot 3D: sound %s, take %d, from %s\n", fx_file[k], v, rel);
                    }
                }
            }
        }
        {
            static const char *const era[5] = { "1910", "1940", "1970", "1982", "2001" };
            int e;
            for (e = 0; e < 5; e++) {
                vp_sound *rec = NULL;
                snprintf(rel, sizeof rel, "sounds/enemy_shot_%s.wav", era[e]);
                if (vp_sound_load(a, vp_asset_path(rel, path, sizeof path), &rec) == VP_OK && rec) {
                    s_enemy_shot_era[e] = rec;
                    snprintf(rel, sizeof rel, "enemy_shot_%s", era[e]);
                    s_enemy_shot_era_ear[e][0] = recording_ear(rel, 0);
                    s_enemy_shot_era_ear[e][1] = recording_ear(rel, 1);
                    fprintf(stderr, "Time Pilot 3D: sound enemy_shot of %s from %s\n", era[e], rel);
                }
            }
        }
    }
    s_dump_name = "engine_outside"; s_engine = recording("engine_outside", synth(1.0f, gen_engine_player));
    s_dump_name = "engine_cockpit"; s_engine_in = recording("engine_cockpit", synth(1.0f, gen_engine_cockpit));
    s_dump_name = "wind"; s_wind = recording("wind", synth(1.0f, gen_wind));
    s_dump_name = "afterburner"; s_burner = recording("afterburner", synth(1.0f, gen_engine_1982));
    {
        /* Each kind's going by, in each ear. A recording of the kind's own
         * name, or else one for all of them (flyby.wav), plays instead, in
         * the middle. With TP3D_SOUNDS_OUT each is also written once. */
        static float (*const gens[FLYBY_KINDS])(float) = {
            gen_flyby_1910, gen_flyby_1940, gen_flyby_rotor, gen_flyby_jet, gen_flyby_ufo, gen_flyby_missile, gen_flyby_boss
        };
        static const char *const names[FLYBY_KINDS] = {
            "flyby_1910", "flyby_1940", "flyby_rotor", "flyby_jet", "flyby_ufo", "flyby_missile", "flyby_boss"
        };
        int k;
        for (k = 0; k < FLYBY_KINDS; k++) {
            if (getenv("TP3D_SOUNDS_OUT")) { s_dump_name = names[k]; vp_sound_free(a, synth(FLYBY_SECONDS, gens[k])); }
            /* a recording of its own, or the one for all, each in its ear; else made here */
            s_flyby[k][0] = recording_ear(names[k], 0);
            s_flyby[k][1] = s_flyby[k][0] ? recording_ear(names[k], 1) : NULL;
            if (!s_flyby[k][0]) { s_flyby[k][0] = recording_ear("flyby", 0); s_flyby[k][1] = s_flyby[k][0] ? recording_ear("flyby", 1) : NULL; }
            if (s_flyby[k][0] && s_flyby[k][1]) continue;
            {
                vp_sound *rec = recording(names[k], NULL);      /* a file read only by the SDK (not 16-bit PCM): in the middle */
                if (!rec) rec = recording("flyby", NULL);
                if (rec) { s_flyby[k][0] = s_flyby[k][1] = rec; continue; }
            }
            s_flyby[k][0] = synth_ear_for(FLYBY_SECONDS, gens[k], 0);
            s_flyby[k][1] = synth_ear_for(FLYBY_SECONDS, gens[k], 1);
        }
    }
    {
        static float (*const gens[SOURCE_KINDS])(float) = {
            gen_engine_1910, gen_engine_1940, gen_engine_1970, gen_engine_1982, gen_engine_2001, gen_missile_loop
        };
        /* a recording of an engine (sounds/engine_1970.wav ...), looped, stands in for the one made here */
        static const char *const names[SOURCE_KINDS] = {
            "engine_1910", "engine_1940", "engine_1970", "engine_1982", "engine_2001", "engine_missile"
        };
        int k;
        for (k = 0; k < SOURCE_KINDS; k++) {
            s_source_sound[k][0] = recording_ear(names[k], 0);
            s_source_sound[k][1] = s_source_sound[k][0] ? recording_ear(names[k], 1) : NULL;
            if (s_source_sound[k][0] && s_source_sound[k][1]) continue;
            s_source_sound[k][0] = synth_ear(gens[k], 0);
            s_source_sound[k][1] = synth_ear(gens[k], 1);
        }
    }
}

/* How loud each effect is against the rest, set by ear. */
static float fx_level(int fx)
{
    switch (fx) {
    case FX_SHOT:       return 0.6f;
    case FX_ENEMY_SHOT: return 0.35f;
    case FX_EXPLODE:    return 1.25f;     /* a blast is heard over the fight (2026-10-17) */
    case FX_HIT:        return 0.9f;
    case FX_BIG_EXPLODE: return 1.25f;
    case FX_HIT_BOSS:   return 1.1f;      /* a hit on something that takes several: clearly heard */
    case FX_RING:       return 1.1f;      /* over the tunnel's music */
    case FX_KILL:       return 0.8f;      /* clear, not loud: a confirmation, the explosion does the rest (2026-10-21) */
    case FX_SCREAM:     return 0.45f;     /* heard, not loud: the recordings are hot (mean 0.24 to 0.34, peaks at full) - under an explosion (2026-10-21) */
    case FX_DEATH:      return 1.0f;
    case FX_WARP:       return s_music_volume > 0.05f ? 0.45f : 0.9f;    /* under the tunnel's music, the air */
    case FX_TINK:       return 0.5f;
    case FX_PORTAL:     return 1.0f;
    case FX_WHOOSH:     return 0.9f;
    case FX_FALL:       return 0.75f;      /* under the fight, not over it */
    default:            return 0.7f;
    }
}

static void engine_levels(float fade);

static int music_loops(int piece);
static vp_voice music_play(int piece, float fade_in);

void sound_volumes(float master, float effects, float engine, float music)
{
    s_master = master;
    s_effects = effects;
    s_engine_level = engine;
    s_music_volume = music;
    if (!s_audio) return;
    vp_audio_set_master(s_audio, master);
    engine_levels(0.1f);
    /* turned down to nothing the music stops; up again, the loop asked for begins */
    if (s_music_voice && music < 0.01f) { vp_voice_stop(s_audio, s_music_voice, 0.2f); s_music_voice = 0; }
    else if (s_music_voice) vp_voice_set_volume(s_audio, s_music_voice, MUSIC_GAIN * music, 0.1f);
    else if (music_loops(s_music)) s_music_voice = music_play(s_music, 0.3f);
}

static void engine_start(int era);
static void source_stop(struct source *src, float fade);

void sound_mute(int on)
{
    if (on == s_muted) return;
    s_muted = on;
    if (on) {
        int i;
        if (s_audio && s_engine_voice) vp_voice_stop(s_audio, s_engine_voice, 0.2f);
        if (s_audio && s_engine_in_voice) vp_voice_stop(s_audio, s_engine_in_voice, 0.2f);
        if (s_audio && s_wind_voice) vp_voice_stop(s_audio, s_wind_voice, 0.2f);
        if (s_audio && s_burner_voice) vp_voice_stop(s_audio, s_burner_voice, 0.2f);
        s_engine_voice = s_engine_in_voice = s_wind_voice = s_burner_voice = 0;
        s_burner_level = 0;
        if (s_audio && s_music_voice) vp_voice_stop(s_audio, s_music_voice, 0.3f);
        s_music_voice = 0;
        s_music = MUSIC_NONE;
        for (i = 0; i < SOURCES; i++) source_stop(&s_sources[i], 0.2f);
    } else {
        engine_start(s_era);            /* the engine that was running comes back */
    }
}

void sound_fx(int fx) { sound_fx_ex(fx, 1.0f, 1.0f); }

void sound_era(int era) { s_sound_era = era < 0 ? 0 : era % 5; }

/* Ducking: under a death or a blast near, the rest gives way for a
 * moment - a lot for the death and a big one, a little for an enemy
 * going down - and comes back up over a second (sound_tick). The big
 * sounds themselves are not pushed down. */
static void duck_under(int fx, float gain)
{
    float to = fx == FX_DEATH ? 0.3f : fx == FX_BIG_EXPLODE ? 1.0f - 0.65f * gain : fx == FX_EXPLODE ? 1.0f - 0.5f * gain : 1.0f;
    if (to < s_duck) s_duck = to;
}

static int ducked(int fx)
{
    return !(fx == FX_DEATH || fx == FX_BIG_EXPLODE || fx == FX_EXPLODE || fx == FX_WARP || fx == FX_PORTAL || fx == FX_HIT_BOSS || fx == FX_RING || fx == FX_KILL);
}

/* How much each counts towards how busy the mix is. */
static float fx_weight(int fx)
{
    switch (fx) {
    case FX_EXPLODE: case FX_DEATH: return 1.0f;
    case FX_BIG_EXPLODE: return 1.5f;
    case FX_SHOT:        return 0.3f;
    case FX_ENEMY_SHOT:  return 0.4f;
    case FX_HIT: case FX_HIT_BOSS: return 0.5f;
    case -1:             return 0.6f;          /* a pass (sound_flyby) */
    default:             return 0.3f;
    }
}

static void played_prune(void)
{
    int i;
    for (i = 0; i < PLAYED; i++)
        if (s_played[i].v[0] && (s_clock > s_played[i].ends || !vp_voice_playing(s_audio, s_played[i].v[0]))) s_played[i].v[0] = 0;
}

/* Kept from now: an effect just begun on these voices, `seconds` long. */
static void played_add(int fx, vp_voice v0, vp_voice v1, float gain, float seconds)
{
    int i, at = 0;
    if (!v0) return;
    for (i = 0; i < PLAYED; i++) {
        if (!s_played[i].v[0]) { at = i; break; }
        if (s_played[i].from < s_played[at].from) at = i;     /* all kept: the oldest is forgotten */
    }
    s_played[at].v[0] = v0;
    s_played[at].v[1] = v1;
    s_played[at].fx = fx;
    s_played[at].gain = gain;
    s_played[at].from = s_clock;
    s_played[at].ends = s_clock + seconds;
}

/* How busy the mix is now: every effect sounding by its weight and its
 * loudness, falling away over its length (a blast's tail counts little). */
static float busy(void)
{
    float sum = 0;
    int i;
    for (i = 0; i < PLAYED; i++) {
        float u;
        if (!s_played[i].v[0]) continue;
        u = 1.0f - (s_clock - s_played[i].from) / (s_played[i].ends - s_played[i].from + 0.001f);
        if (u > 0) sum += fx_weight(s_played[i].fx) * s_played[i].gain * u * u;
    }
    return sum;
}

/* A few voices are always kept free, so that the music (and the death)
 * has one however much else sounds. */
#define VOICES_KEPT 3
static int voices_left(void)
{
    vp_audio_stats st;
    vp_audio_get_stats(s_audio, &st);
    return VP_AUDIO_VOICES - st.voices;
}

static void fx_start(int fx, float gain, float pitch, int at_side, float pan)
{
    vp_sound *snd, **ear;
    vp_voice v0, v1 = 0;
    float heard = gain > 1.0f ? 1.0f : gain;
    int v = 0;
    played_prune();
    if (fx != FX_DEATH && voices_left() <= VOICES_KEPT + (at_side ? 1 : 0)) return;
    snd = s_fx[fx];
    ear = s_fx_ear[fx][0];
    if (fx == FX_ENEMY_SHOT && s_enemy_shot_era[s_sound_era]) {
        snd = s_enemy_shot_era[s_sound_era];
        ear = s_enemy_shot_era_ear[s_sound_era];
    } else if (s_fx_vars[fx] > 1) {
        s_pick = s_pick * 1664525u + 1013904223u;
        v = (int)((s_pick >> 16) % (unsigned)s_fx_vars[fx]);
        snd = s_fx_var[fx][v];
        ear = s_fx_ear[fx][v];
    }
    duck_under(fx, heard);
    /* in a crowd a new one comes in quieter: a third down when about four
     * blasts sound at once; the death and the warp always whole */
    gain *= fx_level(fx) * s_effects * (ducked(fx) ? s_duck : 1.0f)
          * (!ducked(fx) ? 1.0f : 1.0f / sqrtf(1.0f + 0.3f * busy()));
    if (fx == FX_SHOT && s_cockpit) gain *= SHOT_IN_COCKPIT;
    if (at_side && ear[0] && ear[1]) {
        /* each ear as loud as the middle would be, the near one up to a
         * third louder, the far one down to half */
        float p = (pan < -1.0f ? -1.0f : pan > 1.0f ? 1.0f : pan) * PAN_WIDTH;
        v0 = vp_sound_play(s_audio, ear[0], gain * sqrtf(1.0f - p), pitch, 0);
        v1 = vp_sound_play(s_audio, ear[1], gain * sqrtf(1.0f + p), pitch, 0);
        snd = ear[0];
    } else {
        v0 = vp_sound_play(s_audio, snd, gain, pitch, 0);
    }
    played_add(fx, v0 ? v0 : v1, v0 ? v1 : 0, heard, vp_sound_seconds(snd) / (pitch > 0.1f ? pitch : 0.1f));
}

static void fx_play(int fx, float gain, float pitch, int at_side, float pan)
{
    /* silence (the attract without its sound) lets only a coin through */
    if (!s_audio || (s_muted && fx != FX_COIN) || fx < 0 || fx >= FX_COUNT || !s_fx[fx]) return;
    fx_start(fx, gain, pitch, at_side, pan);
}

void sound_fx_ex(int fx, float gain, float pitch) { fx_play(fx, gain, pitch, 0, 0.0f); }

void sound_fx_at(int fx, float gain, float pitch, float pan) { fx_play(fx, gain, pitch, 1, pan); }

/* How loud each of the plane's three loops is, for where it is heard from. */
static void engine_levels(float fade)
{
    float out = s_cockpit ? 0.0f : under() * s_engine_scale, in = s_cockpit ? 1.15f * under() * s_engine_scale : 0.0f;
    float wind = (s_cockpit ? 0.35f : 0.22f) * (0.25f + 0.75f * (s_load < 0 ? 0.4f : s_load)) * under();
    if (!s_audio) return;
    if (s_engine_voice) vp_voice_set_volume(s_audio, s_engine_voice, s_engine_level * ENGINE_GAIN * out, fade);
    if (s_engine_in_voice) vp_voice_set_volume(s_audio, s_engine_in_voice, s_engine_level * ENGINE_GAIN * in, fade);
    if (s_wind_voice) vp_voice_set_volume(s_audio, s_wind_voice, s_engine_level * wind, fade);
}

static void engine_start(int era)
{
    if (s_audio && s_engine_voice) vp_voice_stop(s_audio, s_engine_voice, 0.3f);
    if (s_audio && s_engine_in_voice) vp_voice_stop(s_audio, s_engine_in_voice, 0.3f);
    if (s_audio && s_wind_voice) vp_voice_stop(s_audio, s_wind_voice, 0.3f);
    s_engine_voice = s_engine_in_voice = s_wind_voice = 0;
    if (!s_audio || s_muted || era < 0 || !s_engine) return;
    s_engine_voice = vp_sound_play(s_audio, s_engine, 0.0f, 1.0f, 1);
    if (s_engine_in) s_engine_in_voice = vp_sound_play(s_audio, s_engine_in, 0.0f, 1.0f, 1);
    if (s_wind) s_wind_voice = vp_sound_play(s_audio, s_wind, 0.0f, 1.0f, 1);
    s_load = -1.0f;                     /* the next load is set whatever it is */
    engine_levels(0.4f);
}

void sound_engine_scale(float k)
{
    k = k < 0 ? 0 : k > 1.0f ? 1.0f : k;
    if (fabsf(k - s_engine_scale) < 0.01f) return;
    s_engine_scale = k;
    engine_levels(0.1f);
}

void sound_engine_view(int cockpit)
{
    cockpit = cockpit ? 1 : 0;
    if (cockpit == s_cockpit) return;
    s_cockpit = cockpit;
    engine_levels(0.35f);
    if (s_audio && s_burner_voice)
        vp_voice_set_volume(s_audio, s_burner_voice, s_burner_level * BURNER_GAIN * (s_cockpit ? 0.7f : 1.0f) * s_engine_level * under(), 0.35f);
}

void sound_flyby(int kind, float pan, float gain, float pitch)
{
    float level[2];
    int e;
    if (kind < 0 || kind >= FLYBY_KINDS) kind = FLYBY_JET;
    if (!s_audio || s_muted || !s_flyby[kind][0]) return;
    pan = pan < -1.0f ? -1.0f : pan > 1.0f ? 1.0f : pan;
    level[0] = sqrtf(0.5f * (1.0f - pan));
    level[1] = sqrtf(0.5f * (1.0f + pan));
    gain *= under();
    if (s_flyby[kind][0] == s_flyby[kind][1]) { vp_sound_play(s_audio, s_flyby[kind][0], gain * s_effects * 0.4f, pitch, 0); return; }
    {
        vp_voice v[2] = { 0, 0 };
        played_prune();
        if (voices_left() <= VOICES_KEPT + 1) return;
        for (e = 0; e < 2; e++)
            if (level[e] > 0.05f) v[e] = vp_sound_play(s_audio, s_flyby[kind][e], gain * level[e] * s_effects * 0.55f, pitch, 0);
        played_add(-1, v[0] ? v[0] : v[1], v[0] ? v[1] : 0, gain > 1.0f ? 1.0f : gain,
                   vp_sound_seconds(s_flyby[kind][0]) / (pitch > 0.1f ? pitch : 0.1f));
    }
}

void sound_engine(int on)
{
    on = on >= 0 ? 0 : -1;
    if (on == s_era) return;
    s_era = on;
    engine_start(on);
}

/* --- what else flies ------------------------------------------------------------ */

static void source_stop(struct source *src, float fade)
{
    int e;
    for (e = 0; e < 2; e++) {
        if (s_audio && src->voice[e]) vp_voice_stop(s_audio, src->voice[e], fade);
        src->voice[e] = 0;
    }
    src->id = 0;
}

void sound_sources_begin(void) { s_wanted_count = 0; }

/* The loudest few are kept: a new one louder than the quietest takes its place. */
void sound_source(int id, int kind, float pan, float gain, float pitch)
{
    int i, at = s_wanted_count;

    if (id == 0 || kind < 0 || kind >= SOURCE_KINDS || gain < 0.02f) return;
    if (at == SOURCES) {
        at = 0;
        for (i = 1; i < SOURCES; i++) if (s_wanted[i].gain < s_wanted[at].gain) at = i;
        if (s_wanted[at].gain >= gain) return;
    } else {
        s_wanted_count++;
    }
    s_wanted[at].id = id;
    s_wanted[at].kind = kind;
    s_wanted[at].pan = pan < -1.0f ? -1.0f : pan > 1.0f ? 1.0f : pan;
    s_wanted[at].gain = gain > 1.0f ? 1.0f : gain;
    s_wanted[at].pitch = pitch < 0.5f ? 0.5f : pitch > 2.0f ? 2.0f : pitch;
}

/* Each thing wanted keeps the voices it had; a new one takes a free pair.
 * The place between the ears is equal power: both at 0.7 in the middle. */
void sound_sources_end(void)
{
    int i, j, e;

    if (!s_audio || s_muted) return;
    for (j = 0; j < SOURCES; j++) s_sources[j].named = 0;
    for (i = 0; i < s_wanted_count; i++) {
        struct source *src = NULL;
        float level[2];
        for (j = 0; j < SOURCES; j++)
            if (s_sources[j].id == s_wanted[i].id && s_sources[j].kind == s_wanted[i].kind) src = &s_sources[j];
        if (!src) {
            for (j = 0; j < SOURCES && !src; j++) if (s_sources[j].id == 0) src = &s_sources[j];
            if (!src) continue;         /* all in use this step: next step one will be free */
            src->id = s_wanted[i].id;
            src->kind = s_wanted[i].kind;
            for (e = 0; e < 2; e++)
                src->voice[e] = s_source_sound[src->kind][e]
                              ? vp_sound_play(s_audio, s_source_sound[src->kind][e], 0.0f, s_wanted[i].pitch, 1) : 0;
            src->pitch = s_wanted[i].pitch;
        }
        src->named = 1;
        level[0] = sqrtf(0.5f * (1.0f - s_wanted[i].pan));
        level[1] = sqrtf(0.5f * (1.0f + s_wanted[i].pan));
        for (e = 0; e < 2; e++) {
            if (!src->voice[e]) continue;
            vp_voice_set_volume(s_audio, src->voice[e], s_wanted[i].gain * level[e] * s_effects * SOURCE_VOLUME * under(), 0.08f);
            if (fabsf(s_wanted[i].pitch - src->pitch) > 0.01f) vp_voice_set_pitch(s_audio, src->voice[e], s_wanted[i].pitch);
        }
        if (fabsf(s_wanted[i].pitch - src->pitch) > 0.01f) src->pitch = s_wanted[i].pitch;
    }
    for (j = 0; j < SOURCES; j++)
        if (s_sources[j].id && !s_sources[j].named) source_stop(&s_sources[j], 0.25f);
}

void sound_engine_load(float load)
{
    if (load < 0) load = 0;
    if (load > 1) load = 1;
    if (!s_audio || !s_engine_voice) return;
    if (fabsf(load - s_load) < 0.03f) return;
    s_load = load;
    /* From outside the engine climbs further than from inside, where the
     * airframe takes its top; the wind rises with the speed. */
    vp_voice_set_pitch(s_audio, s_engine_voice, 0.9f + 0.9f * load);
    if (s_engine_in_voice) vp_voice_set_pitch(s_audio, s_engine_in_voice, 0.85f + 0.7f * load);
    if (s_wind_voice) vp_voice_set_pitch(s_audio, s_wind_voice, 0.7f + 0.6f * load);
    engine_levels(0.15f);
}

/* The afterburner's roar, as loud as it burns; quieter in the cockpit. */
static void burner_volume(float fade)
{
    if (s_audio && s_burner_voice)
        vp_voice_set_volume(s_audio, s_burner_voice, s_burner_level * BURNER_GAIN * (s_cockpit ? 0.7f : 1.0f) * s_engine_level * under(), fade);
}

void sound_burner(float level)
{
    if (!s_audio || s_muted || !s_burner) return;
    level = level < 0 ? 0 : level > 1.0f ? 1.0f : level;
    /* lit: a thump once, again only after it has nearly gone out */
    if (level > 0.6f && !s_burner_lit) { s_burner_lit = 1; fx_play(FX_BURNER, 1.0f, 1.0f, 0, 0.0f); }
    if (level < 0.3f) s_burner_lit = 0;
    if (level < 0.02f) {
        if (s_burner_voice) vp_voice_stop(s_audio, s_burner_voice, 0.3f);
        s_burner_voice = 0;
        s_burner_level = 0;
        return;
    }
    if (!s_burner_voice) s_burner_voice = vp_sound_play(s_audio, s_burner, 0.0f, 1.0f, 1);
    if (fabsf(level - s_burner_level) < 0.02f) return;
    s_burner_level = level;
    vp_voice_set_pitch(s_audio, s_burner_voice, 0.85f + 0.3f * level);
    burner_volume(0.08f);
}

void sound_tick(float dt)
{
    float was = under(), k = dt * 1.4f, crowd;
    s_clock += dt;
    s_duck += (1.0f - s_duck) * (k > 1.0f ? 1.0f : k);
    if (s_duck > 0.995f) s_duck = 1.0f;
    /* the bed gives way to a crowd of effects: about a quarter down when
     * four blasts sound at once, back up as they die away */
    if (s_audio) { played_prune(); crowd = 1.0f / (1.0f + 0.08f * busy()); }
    else crowd = 1.0f;
    s_crowd += (crowd - s_crowd) * (crowd < s_crowd ? 0.5f : (dt * 2.0f > 1.0f ? 1.0f : dt * 2.0f));
    if (s_crowd > 0.995f) s_crowd = 1.0f;
    if (fabsf(under() - was) > 0.01f || (under() == 1.0f && was < 1.0f)) {
        engine_levels(0.05f);
        burner_volume(0.05f);
    }
}

/* --- the music ------------------------------------------------------------------ */

static const char *const music_file[MUSIC_COUNT] = {
    [MUSIC_ATTRACT] = "attract", [MUSIC_HISCORE] = "hiscore", [MUSIC_LEVELIN] = "levelin",
    [MUSIC_APPROACH] = "approach", [MUSIC_TUNNEL] = "tunnel", [MUSIC_BOSS] = "boss", [MUSIC_LOUNGE] = "lounge", [MUSIC_START] = "start",
    [MUSIC_VICTORY] = "victory",
};

static int music_loops(int piece) { return piece == MUSIC_ATTRACT || piece == MUSIC_HISCORE || piece == MUSIC_TUNNEL || piece == MUSIC_LOUNGE; }

static vp_voice music_play(int piece, float fade_in)
{
    char rel[64], path[512];
    vp_voice v;
    if (!s_audio || s_muted || piece <= MUSIC_NONE || piece >= MUSIC_COUNT || !music_file[piece] || s_music_volume < 0.01f) return 0;
    snprintf(rel, sizeof rel, "music/%s.wav", music_file[piece]);
    v = vp_music_play(s_audio, vp_asset_path(rel, path, sizeof path),
                      MUSIC_GAIN * s_music_volume * (piece == MUSIC_START || piece == MUSIC_VICTORY ? 1.2f : 1.0f), music_loops(piece), fade_in);
    if (!v) fprintf(stderr, "Time Pilot 3D: music %s does not play: %s\n", music_file[piece], vp_last_error());
    return v;
}

void sound_music(int piece)
{
    if (piece == s_music) return;
    /* the one before goes: a loop fades slowly, a piece that ends anyway quickly */
    if (s_audio && s_music_voice) vp_voice_stop(s_audio, s_music_voice, music_loops(s_music) && s_music != MUSIC_TUNNEL ? 1.2f : 0.35f);
    s_music_voice = 0;
    s_music = piece;
    /* the attract and the initials come in softly; the rest are on the beat */
    s_music_voice = music_play(piece, piece == MUSIC_ATTRACT || piece == MUSIC_HISCORE ? 0.8f : 0.0f);
}

int sound_music_sting(int piece)
{
    return music_play(piece, 0.0f) != 0;
}
