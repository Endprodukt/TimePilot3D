/* game.c - Time Pilot, flown in 3D.
 *
 * The principle is Konami's: the player's plane is always flying, the sky
 * is endless, and enemies of one era come at it from every side. Shooting
 * down enough of them calls the era's boss; shooting that down sends the
 * plane through time to the next era: 1910, 1940, 1970, 1982, 2001, then
 * 1910 again, harder. Formations cross the sky without attacking and are
 * worth a bonus when all of one are shot down. Parachutists drop from some
 * of the fallen and mend the plane when taken aboard.
 *
 * Here the plane is flown with a yoke and a throttle through a 3D sky,
 * from the cockpit or from behind. It can be flown too slowly and stall;
 * it takes hits until its energy is gone; the ground and what is built on
 * it end the flight at once. Everything is in metres and seconds; one step
 * is a sixtieth of a second.
 */

#include "game_internal.h"
#include "vpbind.h"
#include "setup.h"

struct game_state G;

/* this file's own, in the order below */
static void fresh_plane(void);
static void place_player(void);
static void set_level(int level);
static void start_level(int level, int new_plane);
static void update_levelin(vp_input *in);
static void save_player(int level);
static void load_player(int who);
static int plane_lost(int level);
static void start_game(int players);
static void respawn(void);
static void set_view(int view);
static void fly_on(void);
static int next_player_or_over(void);
static void update_continue(vp_input *in);
static unsigned selftest_fingerprint(void);
static void selftest_report(void);
static void update_playing(vp_input *in);
static int coin_start(vp_input *in);
#if DEBUG_LEVEL_SELECT
static void debug_select(vp_input *in);
#endif
static void start_demo(void);
static void update_initials(vp_input *in);
static void step(vp_input *in);
static void enter_attract(int page);
static void end_game(void);
static const char *settings_path(char *buf, size_t size);
static int yes(const char *value);
static void read_settings(const char *path);
static void load_settings(void);
static void settings_to_service(service_settings *v);
static void settings_from_service(const service_settings *v);
static int save_settings(void);
static void open_service(void);
static void update_service(vp_input *in, int hotkey);
static void open_setup(void);
static void update_setup(vp_input *in);
static void factory_reset(void);
static void default_scores(void);

/* eras: see struct era in game_internal.h. */
const struct era eras[ERA_COUNT] = {
    /* speed and turn as the machines were: a biplane slow and nimble, a 1940 fighter
     * faster and wider, a jet fast with a turn as wide as a town, a helicopter
     * slow and round on the spot. The player's own turn is 1.25 at 85 m/s. */
    { "1910", MODEL_BIPLANE,    MODEL_BLIMP,      B_PURSUIT,  56.0f, 1.15f, 135.0f, 320.0f, 1.5f, 8, 40, 0, 100, 3000, 30.0f, 0,
      0.55f, 0.35f, 0.25f, { 255, 200, 60 } },
    { "1940", MODEL_MONOPLANE,  MODEL_BOMBER,     B_BOOMZOOM, 84.0f, 0.95f, 185.0f, 420.0f, 1.25f, 9, 48, 0, 150, 4000, 55.0f, 0,
      0.8f, 0.55f, 0.25f, { 120, 255, 120 } },
    { "1970", MODEL_HELICOPTER, MODEL_BIGHELI,    B_HOVER,    58.0f, 1.5f, 150.0f, 420.0f, 2.4f, 6, 56, 1, 200, 5000, 45.0f, 6.0f,
      0.5f, 0.55f, 0.0f, { 255, 160, 40 } },
    /* 1982 and 2001 (2026-10-12: under fire all the time, the saucers not to be caught): they shoot less
     * often and from less far, lead the plane less, one missile in the air at a time; a saucer breaks away
     * half as readily and there is one fewer of them */
    { "1982", MODEL_JET,        MODEL_B52,        B_JET,     118.0f, 0.6f, 185.0f, 440.0f, 3.6f, 7, 64, 1, 250, 6000, 75.0f, 0,
      0.6f, 0.65f, 0.35f, { 255, 70, 60 } },
    { "2001", MODEL_UFO,        MODEL_MOTHERSHIP, B_SAUCER,   95.0f, 2.2f, 170.0f, 400.0f, 3.4f, 7, 80, 1, 300, 8000, 50.0f, 0,
      0.5f, 0.45f, 0.0f, { 255, 80, 255 } },
};

/* heavies: see struct heavy in game_internal.h. */
const struct heavy heavies[ERA_COUNT] = {
    { MODEL_BIPLANE,    4.0f,  8 }, /* a giant biplane bomber */
    { MODEL_BOMBER,     0.7f,  10 },
    { MODEL_BIGHELI,    0.7f,  10 },
    { MODEL_B52,        0.45f, 12 },
    { MODEL_MOTHERSHIP, 0.4f,  12 },
};

/* difficulties: see struct difficulty in game_internal.h. */
const struct difficulty difficulties[DIFFICULTY_COUNT] = {
    /* the damage column is 8 percent down from what it was (2026-10-10: a touch too much) */
    { 0.78f, 0.90f, 1.00f, 0.9f, 0 },   /* easy: what normal was at first, a little more */
    { 1.15f, 1.40f, 1.15f, 1.20f, 1 },  /* normal: harder again (2026-10-12: too easy, most of all at the start) */
    { 1.24f, 1.65f, 1.25f, 1.4f, 2 },
    { 1.47f, 2.00f, 1.35f, 1.6f, 3 },
};

void enter(int state)
{
    G.state = state;
    G.state_time = 0;
}

void notice(const char *text)
{
    G.notice = text;
    G.notice_time = NOTICE_TIME;
}

/* A word that comes at the player out of the middle of the picture,
 * growing, its colour running round the wheel: for what is worth a cheer. */

void cheer(const char *text)
{
    G.cheer = text;
    G.cheer_time = 0;
}

void add_score(int points)
{
    if (G.demo) return;
    G.score += points;
    if (G.score > G.hiscore) G.hiscore = G.score;
    if (G.score >= G.next_extra) {
        G.lives++;
        G.next_extra = G.next_extra < EXTRA_EVERY ? EXTRA_EVERY : G.next_extra + EXTRA_EVERY;
        sound_fx(FX_ONEUP);
        notice("EXTRA PLANE");
        cheer("EXTRA PLANE");
    }
}

/* --- the game's course --------------------------------------------------------- */

static void fresh_plane(void)
{
    G.p.pitch = 0;
    G.p.roll = 0;
    G.p.flip_roll = 0;
    G.p.speed = SPEED_MIN + THROTTLE_START * (SPEED_MAX - SPEED_MIN);
    G.p.throttle = THROTTLE_START;
    G.p.energy = ENERGY_FULL;
    G.p.stalled = G.p.pull_up = 0;
    G.p.hurt = G.p.ram_cd = 0;
    G.p.shake = G.p.burner = 0;
    G.p.shield_hits = G.p.missiles = 0;         /* a new plane has no items */
    G.p.triple = G.p.missile_cd = 0;
    if (G.cheat_chutes) {                   /* the testing aid */
        int k;
        for (k = 0; k < 2; k++) {
            obj *c = new_obj(K_CHUTE);
            if (!c) break;
            c->model = MODEL_PARACHUTE;
            c->pos = vp_v3_add(G.p.pos, vp_v3_add(vp_v3_scale(player_forward(), 260.0f + 60.0f * (float)k), vp_v3(k ? 22.0f : -22.0f, k ? 60.0f : -5.0f, 0)));
            c->vel = vp_v3(0, -5.0f, 0);
            c->life = 45.0f;
            c->cut = k;
        }
    }
    if (G.cheat_items) { G.p.shield_hits = SHIELD_HITS; G.p.missiles = MISSILE_MAX; G.p.triple = TRIPLE_TIME; G.p.flares = FLARE_TIME; }   /* the testing aid */
    G.p.alive = 1;
    G.p.fire_cd = 0;
    G.p.rear = 0;
    G.p.shield = SHIELD_TIME;
    G.chute_chain = 0;
}

static void place_player(void)
{
    G.p.pos = vp_v3(0, 260.0f, 0);
    G.p.heading = 0;
    G.p.cam_heading = 0;
    G.p.cam_pitch = 0;
    fresh_plane();
}

static void set_level(int level)
{
    G.level = level;
    G.era = level % ERA_COUNT;
    G.loop = level / ERA_COUNT;
    G.kills = G.cheat_kills > G.kills_per_era ? G.kills_per_era : G.cheat_kills;
    if (G.debug_boss) { G.kills = G.kills_per_era; G.debug_boss = 0; }      /* DEBUG_LEVEL_SELECT: straight to this era's boss, once */
    G.boss_called = G.boss_alive = 0;
    G.spawn_timer = 1.0f;
    G.missile_rest = 8.0f;              /* the era's first missile not at once */
    G.missile_pair = 0;
    G.form_timer = 8.0f;
    G.heavy_timer = 14.0f;
    city_reset();                       /* a new era: its gates, depots and guns are all there again */
    memset(G.site_hits, 0, sizeof G.site_hits);
    G.chute_chain = 0;
    G.level_time = 0;
    G.selftest_level_frames = 0;
}

/* An era begins. The plane that has come through time keeps its energy,
 * and gets a little back. */

static void start_level(int level, int new_plane)
{
    float energy = G.p.energy;
    clear_all();
    set_level(level);
    place_player();
    if (!new_plane) G.p.energy = clampf(energy + ERA_ENERGY, 0, ENERGY_FULL);
    sound_engine(G.era);
    sound_era(G.era);
    /* The way in (LEVELIN): the time tunnel opens in the era's sky behind
     * where the plane will be, and the plane comes out of it, flying on
     * at its speed to be just there when the era begins. The tunnel is in
     * the colour of the era before, its ring in this one's. */
    G.warp_to = G.era;
    G.warp_from = (G.era + ERA_COUNT - 1) % ERA_COUNT;
    G.tun.axis = world_forward(G.p.heading, 0);
    G.tun.from = G.p.pos;
    G.tun.len = G.p.speed * (LEVEL_IN_TIME - LEVEL_IN_OPEN);
    G.tun.portal = vp_v3_sub(G.p.pos, vp_v3_scale(G.tun.axis, G.tun.len));
    G.tun.portal.y = G.p.pos.y;
    sound_fx(FX_PORTAL);
    enter(LEVELIN);
}

/* The way in, each step: the plane is in the tunnel until it opens, then
 * comes out of it along its axis; FIRE ends it early. */
static void update_levelin(vp_input *in)
{
    float t = G.state_time, k = clampf((t - LEVEL_IN_OPEN) / (LEVEL_IN_TIME - LEVEL_IN_OPEN), 0, 1);
    G.p.pos = vp_v3_add(G.tun.portal, vp_v3_scale(G.tun.axis, G.tun.len * k));
    G.p.cam_heading = G.p.heading;
    G.p.cam_pitch = G.p.pitch = 0;
    G.p.roll = 0;
    G.p.burner += ((t > LEVEL_IN_OPEN ? 0.9f : 0.0f) - G.p.burner) * 0.15f;
    if (t >= LEVEL_IN_OPEN && t - DT < LEVEL_IN_OPEN) sound_fx(FX_WHOOSH);
    /* The plane goes by the camera (draw_levelin: on the right of its
     * way, looking back along it) at the end: a jet's pass, loudest just
     * as the film ends, on the side it goes by. */
    if (t >= LEVEL_IN_TIME - 0.7f && t - DT < LEVEL_IN_TIME - 0.7f) {
        float h = G.view_heading;
        vp_vec3 side = vp_v3(-G.tun.axis.z, 0, G.tun.axis.x);
        sound_flyby(FLYBY_JET, 0.8f * vp_v3_dot(side, vp_v3(cosf(h), 0, -sinf(h))), 1.0f, 1.05f);
    }
    if (t >= LEVEL_IN_TIME || (t > 1.0f && !G.demo && !G.selftest
                               && (controls_pressed(in, G.btn_fire) || vpb_menu_confirm(in)))) {
        G.p.pos = G.tun.from;
        G.p.burner = 0;
        enter(INTRO);
    }
}

/* What the player flying has, put by until its next turn; `level` is the
 * era it will be in then. */

static void save_player(int level)
{
    G.pl[G.cur].score = G.score;
    G.pl[G.cur].lives = G.lives;
    G.pl[G.cur].next_extra = G.next_extra;
    G.pl[G.cur].level = level;
    /* into the same era as far as it had come; the boss, if it had been
     * called, comes again */
    G.pl[G.cur].kills = level != G.level ? 0 : G.boss_called ? G.kills_per_era : G.kills;
}

static void load_player(int who)
{
    G.cur = who;
    G.score = G.pl[who].score;
    G.lives = G.pl[who].lives;
    G.next_extra = G.pl[who].next_extra;
}

/* A plane is lost; the player flying would go on in era `level`. With two
 * players the turn passes to the other one, if it still has planes. Gives
 * back 1 if somebody else flies now or the game is over, 0 if the same
 * player flies on. */

static int plane_lost(int level)
{
    if (--G.lives < 0) { G.lives = 0; G.pl[G.cur].out = 1; }
    save_player(level);
    if (G.pl[G.cur].out && G.continues && !G.demo && !G.selftest) {
        /* the last plane: everything stands still while the player is
         * asked whether to fly on (update_continue) */
        G.cont_time = CONTINUE_TIME;
        G.cont_level = level;
        G.cont_tunnel = G.state == WARP;
        sound_engine(-1);
        sound_sources_begin();          /* nothing else that flies is heard while it waits */
        sound_sources_end();
        return 1;
    }
    return next_player_or_over();
}

/* After a lost plane: with two players the turn passes to the other one,
 * if it still has planes; the game is over when nobody has any. Gives back
 * 1 if somebody else flies now or the game is over, 0 if the same player
 * flies on. */

static int next_player_or_over(void)
{
    int other = 1 - G.cur;
    if (G.players == 2 && !G.pl[other].out) {
        load_player(other);
        start_level(G.pl[other].level, 1);
        if (G.pl[other].kills > G.kills) G.kills = G.pl[other].kills;
        return 1;
    }
    if (G.pl[G.cur].out) { enter(GAME_OVER); return 1; }
    return 0;
}

/* Waiting for a continue. Nothing moves; the seconds run down, faster
 * with every press of a button that is not START (each takes a second
 * off). START (with a credit, as a game takes one) flies on where the
 * plane was lost, with the planes a game begins with and the score from
 * nothing. Run out, and the plane is lost as it would have been. */

static void update_continue(vp_input *in)
{
    char name[32];
    if (vp_credits_update(&G.credits, in)) sound_fx(FX_COIN);
    if (vp_input_pressed(in, "start1") && (G.credits.free_play || vp_credits_take(&G.credits, 1))) {
        G.cont_time = 0;
        if (G.score > G.pl[G.cur].best) G.pl[G.cur].best = G.score;     /* still counts at the end, if nothing better comes */
        G.score = 0;
        G.lives = G.start_lives - 1;
        G.next_extra = G.bonus_plane;
        G.pl[G.cur].out = 0;
        save_player(G.cont_level);
        sound_fx(FX_START);
        if (G.cont_tunnel) start_level(G.cont_level, 1);
        else fly_on();
        return;
    }
    if (controls_pressed(in, G.btn_fire) || controls_pressed(in, G.btn_faster) || controls_pressed(in, G.btn_slower)
        || controls_pressed(in, G.btn_rear) || controls_captured(name, sizeof name)) {
        G.cont_time = ceilf(G.cont_time) - 1.0f;
        sound_fx(FX_TINK);
    }
    G.cont_time -= DT;
    if (G.cont_time > 0) return;
    G.cont_time = 0;
    next_player_or_over();
}

static void start_game(int players)
{
    int i;
    G.players = players;
    for (i = 0; i < 2; i++) {
        G.pl[i].score = 0;
        G.pl[i].lives = G.start_lives - 1;  /* the one flying is not in the row of spares */
        G.pl[i].next_extra = G.bonus_plane;
        G.pl[i].level = G.start_level;
        G.pl[i].kills = 0;
        G.pl[i].best = 0;
        G.pl[i].out = i >= players;
    }
    load_player(0);
    G.demo = 0;
    G.rank = -1;
    set_view(G.view_ini);
    sound_mute(0);
    G.muted = 0;
    if (!sound_music_sting(MUSIC_START)) sound_fx(FX_START);
    /* How to play, once, before the first era (not for the self test) */
    if (G.selftest) start_level(G.start_level, 1);
    else { sound_engine(-1); enter(TUTORIAL); }     /* (a START in the demo leaves its engine running) */
}

void start_warp(void)
{
    int i;
    for (i = 0; i < MAX_OBJ; i++) {
        obj *o = &G.objs[i];
        if (o->kind == K_ENEMY || o->kind == K_BULLET || o->kind == K_MISSILE || o->kind == K_CHUTE || o->kind == K_BOMB) {
            if (o->kind == K_ENEMY) { shatter(o, 45.0f, CUT_NONE, 1); burst(o->pos, o->vel, 4, 30.0f, 0.6f, SPARK_FIRE, 0); }
            o->kind = K_NONE;
        }
    }
    /* how long the era took, for run.log: to compare with the original's (about a minute and a half) */
    if (!G.selftest && !G.demo)
        fprintf(stderr, "Time Pilot 3D: era %s took %d:%02d - %d:%02d to its boss (%d kills), %d:%02d for the boss\n", era()->year,
                (int)G.level_time / 60, (int)G.level_time % 60, (int)G.boss_at / 60, (int)G.boss_at % 60, G.kills_per_era,
                (int)(G.level_time - G.boss_at) / 60, (int)(G.level_time - G.boss_at) % 60);
    G.warp_from = G.era;
    G.warp_to = (G.era + 1) % ERA_COUNT;
    memset(&G.tun, 0, sizeof G.tun);
    /* wider bends with every era flown, and with the difficulty */
    G.tun.amp = clampf(0.75f + 0.07f * (float)G.level + 0.1f * (float)(G.difficulty - DIFFICULTY_NORMAL), 0.6f, 1.35f);
    world_tunnel_centre(TUNNEL_AHEAD, G.tun.amp, &G.tun.x, &G.tun.y);
    G.tun.ring = (int)(TUNNEL_AHEAD / TUNNEL_STEP);
    /* The tunnel's mouth opens ahead of the plane, level with it, high
     * enough over whatever stands there; the plane flies into it first
     * (update_tunnel), and the tunnel's own sound begins when it is in. */
    G.tun.axis = world_forward(G.p.heading, 0);
    G.tun.portal = vp_v3_add(G.p.pos, vp_v3_scale(G.tun.axis, PORTAL_RANGE));
    G.tun.portal.y = clampf(G.p.pos.y, 140.0f, 600.0f);
    {
        float low = city_top(G.era, G.tun.portal.x, G.tun.portal.z, 120.0f) + 150.0f;
        if (G.tun.portal.y < low) G.tun.portal.y = low;
    }
    /* the way there is a straight line, flown in exactly TUNNEL_APPROACH
     * seconds: from the speed the plane has, faster and faster */
    {
        vp_vec3 way = vp_v3_sub(G.tun.portal, G.p.pos);
        G.tun.from = G.p.pos;
        G.tun.len = sqrtf(way.x * way.x + way.y * way.y + way.z * way.z);
        G.tun.axis = vp_v3_scale(way, 1.0f / G.tun.len);
        G.tun.run = clampf(G.p.speed * TUNNEL_APPROACH / G.tun.len, 0.45f, 0.9f);
        /* its speed until the run: its own, but slow enough to leave a run to the mouth (tunnel_approach) */
        G.tun.v0 = clampf(G.p.speed, 40.0f, 0.5f * G.tun.len / (APPROACH_OPEN + APPROACH_LOOP));
        G.tun.base = G.p.pos;
    }
    sound_fx(FX_PORTAL);                /* the mouth opens */
    G.p.hurt = 0;
    G.p.stalled = G.p.pull_up = 0;
    G.p.flip_roll = 0;
    G.tun.throttle = THROTTLE_MID;
    G.p.ram_cd = 10.0f;                 /* nothing is flown into on the way out */
    enter(WARP);
}

/* After a lost plane: a new one where the old one was, high enough; the
 * enemies near the spot are gone, the rest fly on. */

static void respawn(void)
{
    if (G.selftest) { /* planes that never run out */ }
    else if (G.demo) { enter_attract(PAGE_CAST); return; }
    else if (plane_lost(G.level)) return;
    fly_on();
}

/* The next plane, where the last was lost: nothing near it, high enough. */

static void fly_on(void)
{
    float low;
    int i;

    for (i = 0; i < MAX_OBJ; i++) {
        obj *o = &G.objs[i];
        if ((o->kind == K_ENEMY || o->kind == K_BOSS) && dist3(o->pos, G.p.pos) < 350.0f) {
            if (o->kind == K_BOSS) {             /* pushed away rather than taken */
                vp_vec3 away = vp_v3_normalize(vp_v3_sub(o->pos, G.p.pos));
                o->pos = vp_v3_add(G.p.pos, vp_v3_scale(away, 600.0f));
            } else {
                o->kind = K_NONE;
            }
        }
    }
    fresh_plane();
    low = city_top(G.era, G.p.pos.x, G.p.pos.z, 80.0f) + 120.0f;
    if (low < 220.0f) low = 220.0f;
    if (G.p.pos.y < low) G.p.pos.y = low;
    G.p.cam_heading = G.p.heading;
    G.p.cam_pitch = 0;
    sound_engine(G.era);
    enter(INTRO);
}

/* A fingerprint of the whole state of play: where the plane and every
 * object are, what they are, and the numbers of the game. Two runs of the
 * self test that agree in it have played the same game; one line of code
 * that changes any behaviour changes it. For checking that a rebuild of
 * the code has changed nothing. */

static unsigned selftest_fingerprint(void)
{
    unsigned h = 2166136261u;
    int i;
#define MIX(v) do { unsigned u_ = (unsigned)(v); h = (h ^ u_) * 16777619u; } while (0)
#define MIXF(v) MIX((int)((v) * 64.0f))
    MIX(G.score); MIX(G.kills); MIX(G.lives); MIX(G.frames); MIX(G.rng); MIX(G.state);
    MIXF(G.p.pos.x); MIXF(G.p.pos.y); MIXF(G.p.pos.z); MIXF(G.p.heading); MIXF(G.p.pitch);
    MIXF(G.p.speed); MIXF(G.p.energy); MIXF(G.p.throttle);
    for (i = 0; i < MAX_OBJ; i++) {
        const obj *o = &G.objs[i];
        if (o->kind == K_NONE) continue;
        MIX(o->kind); MIX(o->model); MIX(o->hp); MIX(o->mode);
        MIXF(o->pos.x); MIXF(o->pos.y); MIXF(o->pos.z); MIXF(o->heading); MIXF(o->pitch); MIXF(o->life);
    }
#undef MIX
#undef MIXF
    return h;
}

static void selftest_report(void)
{
    fprintf(stderr, "level %d (%s, loop %d): %u frames, %d deaths so far, score %d, tailed %u%%, fingerprint %08x\n",
            G.level + 1, era()->year, G.loop, G.selftest_level_frames, G.selftest_deaths, G.score,
            G.selftest_level_frames ? 100u * G.selftest_tailed / G.selftest_level_frames : 0u, selftest_fingerprint());
    G.selftest_tailed = 0;
}

/* The self test's measure of how hard the player is pressed: is there,
 * in this step, an enemy within 260 m behind it with its nose on it? */
static void selftest_watch(void)
{
    vp_vec3 pfwd = player_forward();
    int i;
    for (i = 0; i < MAX_OBJ; i++) {
        const obj *o = &G.objs[i];
        vp_vec3 rel;
        float d;
        if (o->kind != K_ENEMY || o->form || o->heavy) continue;
        rel = vp_v3_sub(o->pos, G.p.pos);
        d = vp_v3_length(rel);
        if (d < 1.0f || d > 260.0f || vp_v3_dot(pfwd, rel) >= 0) continue;
        if (vp_v3_dot(forward_of(o), vp_v3_scale(rel, -1.0f / d)) > 0.9f) { G.selftest_tailed++; return; }
    }
}

static void update_playing(vp_input *in)
{
    update_player(in);
    if (!G.p.alive) return;                 /* into the ground this step */
    update_objects();
    check_collisions();
    run_sites();
    run_spawning();
    if (G.selftest) selftest_watch();
}

/* START 1 with a credit: one player. START 2 with two: two, taking turns. */

static int coin_start(vp_input *in)
{
    int players;
    if (vp_input_pressed(in, "start2") && vp_credits_take(&G.credits, 2)) players = 2;
    else if (vp_input_pressed(in, "start1") && vp_credits_take(&G.credits, 1)) players = 1;
    else return 0;
    vp_lamps_set(0);
#if DEBUG_LEVEL_SELECT
    if (G.level_select) {               /* the service menu can turn it off */
        G.debug_players = players;
        G.debug_y = 0;
        G.demo = 0;
        enter(DEBUGSEL);
        return 1;
    }
#endif
    start_game(players);
    return 1;
}

#if DEBUG_LEVEL_SELECT
/* FOR TESTING, TO BE TAKEN OUT. Where the game begins: ten lines, each
 * era and each era's boss; the yoke (or up and down) moves, fire or START
 * takes. */
/* The rows of the choice: for each era the era itself, then its boss in
 * each stage of its fight. Row `row` is era *era_of, and stage *stage of
 * its boss, or -1 for the era from its beginning. */
int debug_rows(void)
{
    int e, n = 0;
    for (e = 0; e < ERA_COUNT; e++) n += 1 + bosses[e].stages;
    return n;
}

void debug_row(int row, int *era_of, int *stage)
{
    int e;
    for (e = 0; e < ERA_COUNT; e++) {
        if (row < 1 + bosses[e].stages) { *era_of = e; *stage = row - 1; return; }
        row -= 1 + bosses[e].stages;
    }
    *era_of = 0; *stage = -1;
}

static void debug_select(vp_input *in)
{
    float x, y;
    int way, yes_now;
    controls_menu(in, &x, &y, &yes_now);
    int rows = debug_rows(), era_of, stage;
    way = y < -0.5f ? -1 : y > 0.5f ? 1 : 0;
    if (way && way != G.debug_y) { G.debug_sel = (G.debug_sel + way + rows) % rows; sound_fx(FX_TINK); }
    G.debug_y = way;
    if (G.state_time > 0.3f && (yes_now || controls_pressed(in, G.btn_fire))) {     /* START or fire, as it says */
        debug_row(G.debug_sel, &era_of, &stage);
        G.start_level = era_of;
        G.debug_boss = stage >= 0;
        G.cheat_boss2 = stage > 0 ? stage : 0;      /* that stage of its fight, for this one boss (boss_begin) */
        G.debug_stage = stage > 0;
        start_game(G.debug_players);
    }
}
#endif

static void set_view(int view)
{
    G.chase = view == VIEW_CHASE;
    G.hud_only = view == VIEW_HUD;
}

static void start_demo(void)
{
    G.demo = 1;
    G.demo_time = 0;
    G.players = 1;
    G.cur = 0;
    G.score = 0;
    G.lives = 2;
    set_view(G.demo_era & 1 ? VIEW_CHASE : VIEW_COCKPIT);     /* the cockpit one time, from behind the next */
    start_level(G.demo_era % ERA_COUNT, 1);
    G.demo_era++;
}

static void update_initials(vp_input *in)
{
    static const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ .";
    int n = (int)sizeof letters - 1, dir = 0;
    int way = vpb_menu_way_y();         /* the menu's own: the arrows, any stick, hat or the yoke */

    if (way < 0) dir = -1;
    if (way > 0) dir = 1;
    if (dir) {
        G.initial_repeat -= DT;
        if (G.initial_repeat <= 0) {   /* the first press moves at once, held it repeats */
            G.initial_letter = ((G.initial_letter - dir) % n + n) % n;    /* up: the next letter */
            G.initial_repeat = 0.18f;
            sound_fx(FX_TINK);
        }
    } else {
        G.initial_repeat = 0;
    }
    G.initials[G.initial_pos] = letters[G.initial_letter];
    /* START 1, Enter, and fire (not the press still held from the fight: it must be new, and not at once) */
    if (vpb_menu_confirm(in) || (G.state_time > 0.3f && controls_pressed(in, G.btn_fire))) {
        sound_fx(FX_PICKUP);
        G.initial_pos++;
        G.state_time = 0;
    }
    if (G.initial_pos < 3 && G.state_time < INITIALS_TIMEOUT) return;
    G.initials[3] = 0;
    G.rank = vp_hiscore_insert(G.initials, G.score);
    G.over_player++;
    end_game();                         /* the other player's initials, or the high scores */
}

static void step(vp_input *in)
{
    G.frames++;
    if (G.cont_time > 0) { update_continue(in); return; }
    G.state_time += DT;
    G.level_time += DT;
    G.selftest_level_frames++;
    if (G.notice_time > 0) G.notice_time -= DT;
    G.cheer_time += DT;
    {
        int quiet = G.selftest || (!G.attract_sound && (G.state == ATTRACT || G.demo));
        if (quiet != G.muted) { G.muted = quiet; sound_mute(quiet); }
    }
    {
        /* The music for where the game is: the attract's (and its demo's),
         * the initials', and the films of the way out of an era, through
         * the tunnel and into the next. In flight none: the sky is heard. */
        int m = MUSIC_NONE;
        if (G.selftest) m = MUSIC_NONE;
        else if (G.state == ATTRACT || G.demo) m = MUSIC_ATTRACT;
        else if (G.state == INITIALS) m = MUSIC_HISCORE;
        else if (G.state == LEVELIN) m = MUSIC_LEVELIN;
        else if (G.state == BOSSINTRO) m = MUSIC_BOSS;
        else if (G.state == TUTORIAL) m = MUSIC_LOUNGE;
        else if (G.state == WARP) m = G.state_time < TUNNEL_APPROACH ? MUSIC_APPROACH : MUSIC_TUNNEL;
        sound_music(m);
    }
    if (vp_credits_update(&G.credits, in)) sound_fx(FX_COIN);

    if (G.demo) {
        vp_credits_blink(&G.credits, (double)G.frames / 60.0);
        if (coin_start(in)) return;
        if ((G.demo_time += DT) >= DEMO_TIME && G.state != WARP) { enter_attract(PAGE_CAST); return; }
    }

    switch (G.state) {
    case ATTRACT:
        /* title, high scores, the game playing itself, who is who, and round again */
        vp_credits_blink(&G.credits, (double)G.frames / 60.0);
        if (coin_start(in)) return;
        if (G.page == PAGE_TITLE && G.state_time >= TITLE_TIME) { G.page = PAGE_HOWTO; G.state_time = 0; }
        else if (G.page == PAGE_HOWTO && G.state_time >= HOWTO_TIME) { G.page = PAGE_SCORES; G.state_time = 0; }
        else if (G.page == PAGE_SCORES && G.state_time >= SCORES_TIME) start_demo();
        else if (G.page == PAGE_CAST && G.state_time >= CAST_TIME) { G.page = PAGE_ITEMS; G.state_time = 0; }
        else if (G.page == PAGE_ITEMS && G.state_time >= ITEMS_TIME) { G.page = PAGE_TITLE; G.state_time = 0; G.rank = -1; }
        if (G.page == PAGE_HOWTO) howto_sounds(G.state_time);
        break;
    case TUTORIAL:
        /* how to play and the items, through to the end or until START */
        if (G.state_time < HOWTO_TIME) howto_sounds(G.state_time);
        if (G.state_time >= TUTORIAL_TIME || (G.state_time > 0.5f && vp_input_pressed(in, "start1")))
            start_level(G.start_level, 1);
        break;
    case INITIALS:
        update_initials(in);
        break;
    case LEVELIN:
        update_levelin(in);
        break;
    case INTRO:
        update_playing(in);
        if (G.state == INTRO && G.state_time >= INTRO_TIME) enter(PLAYING);
        break;
    case PLAYING:
        update_playing(in);
        break;
    case DYING:
        update_objects();
        if (G.state_time >= DYING_TIME) respawn();
        break;
    case WARP:
        update_objects();                   /* what is left of the era, falling */
        update_tunnel(in);
        if (G.state_time >= WARP_TIME) {
            if (G.selftest) {
                selftest_report();
                if (G.level + 1 >= G.selftest_levels) {
                    fprintf(stderr, "selftest done: %d levels, %d deaths, score %d\n", G.level + 1, G.selftest_deaths, G.score);
                    G.quit = 1;
                    vp_game_quit(G.game);
                    return;
                }
            }
            if (G.tun.lost && !G.selftest) {   /* the plane was lost on the way: the next one arrives */
                if (G.demo) { enter_attract(PAGE_CAST); return; }
                if (plane_lost(G.level + 1)) return;
                start_level(G.level + 1, 1);
            } else {
                start_level(G.level + 1, 0);
            }
        }
        break;
#if DEBUG_LEVEL_SELECT
    case DEBUGSEL:
        debug_select(in);
        break;
#endif
    case BOSSINTRO:
        /* The boss comes. Nobody flies and nobody shoots; the plane holds
         * its height in a slow turn, the boss flies in to be looked at
         * (draw_bossintro), a siren sounds. Then the fight. */
        update_objects();
        G.p.pitch += -G.p.pitch * 0.05f;
        G.p.heading = wrap_angle(G.p.heading + 0.12f * DT);
        G.p.roll += (-0.15f - G.p.roll) * 0.05f;
        G.p.pos = vp_v3_add(G.p.pos, vp_v3_scale(player_forward(), G.p.speed * DT));
        if (G.p.pos.y < 200.0f) G.p.pos.y += 20.0f * DT;
        G.p.cam_heading = G.p.heading;
        G.p.cam_pitch = G.p.pitch;
        G.p.shake *= 0.95f;
        if (G.state_time < 4.0f && fmodf(G.state_time, 0.55f) < DT) sound_fx(FX_WARN);
        if (G.state_time >= BOSS_INTRO_TIME || !G.boss_alive) { notice(boss_stage()->goal); enter(PLAYING); }
        break;
    case BOSSTURN:
        /* Between two stages of the boss's fight: the same stillness,
         * shorter, while it brings out what it fights with next
         * (boss.c, draw_bossintro). */
        update_objects();
        G.p.pitch += -G.p.pitch * 0.05f;
        G.p.heading = wrap_angle(G.p.heading + 0.12f * DT);
        G.p.roll += (-0.15f - G.p.roll) * 0.05f;
        G.p.pos = vp_v3_add(G.p.pos, vp_v3_scale(player_forward(), G.p.speed * DT));
        if (G.p.pos.y < 200.0f) G.p.pos.y += 20.0f * DT;
        G.p.cam_heading = G.p.heading;
        G.p.cam_pitch = G.p.pitch;
        G.p.shake *= 0.95f;
        if (G.state_time >= boss_turn_time() || !G.boss_alive) { G.boss_deploy = 1.0f; notice(boss_stage()->goal); enter(PLAYING); }
        break;
    case BOSSFALL: {
        /* Nobody flies and nobody shoots: the plane holds a wide level
         * turn by itself while the boss comes down. Two seconds after it
         * has hit the ground the way through time opens. */
        update_objects();
        G.p.pitch += -G.p.pitch * 0.05f;
        G.p.heading = wrap_angle(G.p.heading + 0.25f * DT);
        G.p.roll += (-0.3f - G.p.roll) * 0.05f;
        G.p.pos = vp_v3_add(G.p.pos, vp_v3_scale(player_forward(), G.p.speed * DT));
        if (G.p.pos.y < 200.0f) G.p.pos.y += 20.0f * DT;
        G.p.cam_heading = G.p.heading;
        G.p.cam_pitch = G.p.pitch;
        G.p.shake *= 0.95f;
        if (!boss_falling() && (G.fall_after += DT) > 2.2f) start_warp();
        break;
    }
    case GAME_OVER:
        update_objects();
        if (G.state_time >= GAME_OVER_TIME || vp_input_pressed(in, "start1")) { G.over_player = 0; end_game(); }
        break;
    }
    /* START changes the view while a game is flown (a coin's START was
     * taken above, and ends this step): the display alone, the cockpit,
     * from behind, and round again. */
    if (!G.demo && !G.selftest && (G.state == PLAYING || G.state == INTRO) && G.state_time > 0.2f
        && vp_input_pressed(in, "start1"))
        set_view(G.chase ? VIEW_HUD : G.hud_only ? VIEW_COCKPIT : VIEW_CHASE);

    hear_the_sky();

    if (G.selftest && G.selftest_level_frames > 150000) {
        fprintf(stderr, "level %d (%s): stuck after %u frames (kills %d, boss %s, stage %d, left of it %d, state %d)\n", G.level + 1, era()->year,
                G.selftest_level_frames, G.kills, G.boss_alive ? "alive" : G.boss_called ? "down" : "not called",
                G.boss_stage + 1, boss_stage_left(), G.state);
        G.quit = 1;
        vp_game_quit(G.game);
    }
}

/* --- the cabinet --------------------------------------------------------------- */

static void enter_attract(int page)
{
    G.demo = 0;
    G.cont_time = 0;
    G.page = page;
    G.table_count = vp_hiscore_load(G.table);
    if (G.table_count > 0 && G.table[0].score > G.hiscore) G.hiscore = G.table[0].score;
    clear_all();
    place_player();
    sound_engine(-1);
    enter(ATTRACT);
}

/* The game is over: each player whose score belongs in the table is asked
 * for its initials, one after the other; then the high scores. */

static void end_game(void)
{
    for (; G.over_player < G.players && !G.demo && !G.selftest; G.over_player++) {
        G.score = G.pl[G.over_player].score;
        if (G.pl[G.over_player].best > G.score) G.score = G.pl[G.over_player].best;     /* a score from before a continue */
        if (vp_hiscore_rank(G.score) < 0) continue;
        memcpy(G.initials, "A  ", 4);
        G.initial_pos = G.initial_letter = 0;
        G.initial_repeat = 0;
        sound_engine(-1);
        enter(INITIALS);
        return;
    }
    G.over_player = 0;
    enter_attract(PAGE_SCORES);
}

/* --- settings ------------------------------------------------------------------- */

static const char *const bright_key[BRIGHT_COUNT] = {
    "brightness_ground", "brightness_clouds", "brightness_enemies", "brightness_player",
    "brightness_shots", "brightness_effects", "brightness_text", "brightness_title"
};

static const char *const difficulty_key[DIFFICULTY_COUNT] = { "easy", "normal", "hard", "ace" };

static const char *settings_path(char *buf, size_t size)
{
    snprintf(buf, size, "%s/settings.ini", vp_save_dir());
    return buf;
}

static int yes(const char *value) { return value[0] == 'y' || value[0] == '1'; }

static void read_settings(const char *path)
{
    char line[512], key[64], value[128];
    FILE *f = fopen(path, "r");
    int i, no_frame = 0;

    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, " %63[A-Za-z_] = %127[^ ;#\r\n]", key, value) != 2) continue;
        if (!strcmp(key, "yoke_sensitivity"))    G.yoke_sens = clampf((float)atof(value), 0.25f, 3.0f);
        else if (!strcmp(key, "yoke_deadzone"))  G.yoke_deadzone = atoi(value);
        else if (!strcmp(key, "invert_pitch"))   G.invert_pitch = yes(value);
        else if (!strcmp(key, "cockpit_frame"))  no_frame = !yes(value);      /* before 0.55.0: the cockpit's lines off */
        else if (!strcmp(key, "occlusion"))      G.occlusion = yes(value);
        else if (!strcmp(key, "level_select"))   G.level_select = yes(value);
        else if (!strcmp(key, "invincible"))     G.invincible = yes(value);
        else if (!strcmp(key, "continue"))       G.continues = yes(value);
        else if (!strcmp(key, "setup_done"))     G.setup_done = yes(value);
        else if (!strcmp(key, "version"))        snprintf(G.version, sizeof G.version, "V%.14s", value);
        else if (!strcmp(key, "picture_size"))   G.picture_size = atoi(value);
        else if (!strcmp(key, "beam_limit"))     G.beam_limit = atoi(value);
        else if (!strcmp(key, "view"))           G.view_ini = value[0] == 'c' && value[1] == 'h' ? VIEW_CHASE : value[0] == 'h' ? VIEW_HUD : VIEW_COCKPIT;
        else if (!strcmp(key, "aim_assist"))     G.aim_assist = atoi(value);
        else if (!strcmp(key, "lives"))          G.start_lives = atoi(value);
        else if (!strcmp(key, "bonus_plane"))    G.bonus_plane = atoi(value);
        else if (!strcmp(key, "enemies_per_era")) G.kills_per_era = atoi(value);
        else if (!strcmp(key, "start_era"))      G.start_level = (int)clampf((float)atoi(value), 1, ERA_COUNT) - 1;
        else if (!strcmp(key, "volume"))         G.volume = (float)atof(value) / 100.0f;
        else if (!strcmp(key, "effects_volume")) G.effects_volume = (float)atof(value) / 100.0f;
        else if (!strcmp(key, "engine_volume"))  G.engine_volume = (float)atof(value) / 100.0f;
        else if (!strcmp(key, "music_volume"))   G.music_volume = (float)atof(value) / 100.0f;
        else if (!strcmp(key, "attract_sound"))  G.attract_sound = yes(value);
        else if (!strcmp(key, "free_play"))      G.free_play = yes(value);
        else if (!strcmp(key, "fire_button"))    snprintf(G.btn_fire, sizeof G.btn_fire, "%s", value);
        else if (!strcmp(key, "faster_button"))  snprintf(G.btn_faster, sizeof G.btn_faster, "%s", value);
        else if (!strcmp(key, "slower_button"))  snprintf(G.btn_slower, sizeof G.btn_slower, "%s", value);
        else if (!strcmp(key, "rear_button"))    snprintf(G.btn_rear, sizeof G.btn_rear, "%s", value);
        else if (!strcmp(key, "menu_button"))    snprintf(G.menu_key, sizeof G.menu_key, "%s", value);
        else if (!strcmp(key, "control"))        G.control_stick = value[0] == 'j' || value[0] == 's';
        else if (!strcmp(key, "yoke_layout"))    sscanf(value, "%d %d %d %d", &G.yoke_layout[0], &G.yoke_layout[1], &G.yoke_layout[2], &G.yoke_layout[3]);
        else if (!strcmp(key, "difficulty")) {
            for (i = 0; i < DIFFICULTY_COUNT; i++)
                if (!strcmp(value, difficulty_key[i])) G.difficulty = i;
        }
        for (i = 0; i < BRIGHT_COUNT; i++)
            if (!strcmp(key, bright_key[i])) G.bright[i] = (float)atof(value) / 100.0f;
    }
    fclose(f);
    if (no_frame && G.view_ini == VIEW_COCKPIT) G.view_ini = VIEW_HUD;
    G.yoke_deadzone = (int)clampf((float)G.yoke_deadzone, 0, 50);
    G.aim_assist = (int)clampf((float)G.aim_assist, 0, 100);
    G.picture_size = (int)clampf((float)G.picture_size, 60, 100);
    G.beam_limit = (int)clampf((float)G.beam_limit, 100, 250);
    world_set_load_limit((float)G.beam_limit * 1000.0f);
    controls_use_yoke(!G.control_stick);
    G.start_lives = (int)clampf((float)G.start_lives, 1, 9);
    G.bonus_plane = (int)clampf((float)G.bonus_plane, 5000, 100000);
    G.kills_per_era = (int)clampf((float)G.kills_per_era, 8, 96);
}

/* What the game starts with, then game.ini next to the executable, then
 * save/settings.ini (what the service menu saved), each over the one before. */

static void load_settings(void)
{
    static const int bright_default[BRIGHT_COUNT] = { 45, 40, 90, 100, 100, 80, 70, 100 };
    char path[1024];
    int i;

    G.yoke_sens = 1.0f;
    G.yoke_deadzone = 12;
    G.aim_assist = 40;
    G.occlusion = 1;
    G.level_select = 0;                     /* a developer's aid: off unless asked for (2026-10-21) */
    G.view_ini = VIEW_HUD;                  /* the display alone, unless game.ini or the service menu say otherwise */
    G.continues = 1;
    G.picture_size = 100;
    G.beam_limit = 250;
    G.start_lives = 3;
    G.difficulty = DIFFICULTY_NORMAL;
    G.bonus_plane = 10000;
    G.kills_per_era = 32;           /* the original's 56 made an era here last too long (2026-10-17): the 3D sky is slower to clear */
    G.volume = 0.6f;
    G.effects_volume = 0.8f;
    G.engine_volume = 0.4f;
    G.music_volume = 0.9f;
    for (i = 0; i < BRIGHT_COUNT; i++) G.bright[i] = (float)bright_default[i] / 100.0f;
    strcpy(G.btn_fire, "key_Left_Ctrl+joy1_button1");     /* a stick's buttons 1..4 and AdvanceMAME's keys */
    strcpy(G.btn_faster, "key_Left_Alt+joy1_button2");
    strcpy(G.btn_slower, "key_Space+joy1_button3");
    strcpy(G.btn_rear, "key_Left_Shift+joy1_button4");
    strcpy(G.menu_key, "key_F2");
    for (i = 0; i < 4; i++) G.yoke_layout[i] = i + 1;
    read_settings(vp_asset_path("game.ini", path, sizeof path));
    read_settings(settings_path(path, sizeof path));
    vpb_normalize(G.btn_fire, sizeof G.btn_fire);       /* advmame.rc names of an older file */
    vpb_normalize(G.btn_faster, sizeof G.btn_faster);
    vpb_normalize(G.btn_slower, sizeof G.btn_slower);
    vpb_normalize(G.btn_rear, sizeof G.btn_rear);
    vpb_normalize(G.menu_key, sizeof G.menu_key);
    service_menu_key(G.menu_key);
    set_view(G.view_ini);
}

static void settings_to_service(service_settings *v)
{
    int i;
    memset(v, 0, sizeof *v);
    v->lives            = G.start_lives;
    v->difficulty       = G.difficulty;
    v->bonus_plane      = G.bonus_plane;
    v->kills_per_era    = G.kills_per_era;
    v->start_era        = G.start_level % ERA_COUNT;
    v->yoke_sensitivity = G.yoke_sens;
    v->yoke_deadzone    = G.yoke_deadzone;
    v->invert_pitch     = G.invert_pitch;
    v->occlusion        = G.occlusion;
    v->level_select     = G.level_select;
    v->invincible       = G.invincible;
    v->continues        = G.continues;
    v->picture_size     = G.picture_size;
    v->beam_limit       = G.beam_limit;
    v->view             = G.view_ini;
    v->aim_assist       = G.aim_assist;
    v->volume           = (int)(G.volume * 100.0f + 0.5f);
    v->effects_volume   = (int)(G.effects_volume * 100.0f + 0.5f);
    v->engine_volume    = (int)(G.engine_volume * 100.0f + 0.5f);
    v->music_volume     = (int)(G.music_volume * 100.0f + 0.5f);
    v->attract_sound    = G.attract_sound;
    v->free_play        = G.free_play;
    for (i = 0; i < BRIGHT_COUNT; i++) v->brightness[i] = (int)(G.bright[i] * 100.0f + 0.5f);
    snprintf(v->button[SERVICE_FIRE], sizeof v->button[0], "%s", G.btn_fire);
    snprintf(v->button[SERVICE_FASTER], sizeof v->button[0], "%s", G.btn_faster);
    snprintf(v->button[SERVICE_SLOWER], sizeof v->button[0], "%s", G.btn_slower);
    snprintf(v->button[SERVICE_REAR], sizeof v->button[0], "%s", G.btn_rear);
    snprintf(v->menu_key, sizeof v->menu_key, "%s", G.menu_key);
    for (i = 0; i < 4; i++) v->yoke_layout[i] = G.yoke_layout[i];
    v->control_stick = G.control_stick;
}

/* Everything here works from the next frame on, or from the next game
 * (the planes a game starts with); nothing needs a restart. */

static void settings_from_service(const service_settings *v)
{
    int i;
    G.start_lives    = v->lives;
    G.difficulty     = v->difficulty;
    G.bonus_plane    = v->bonus_plane;
    G.kills_per_era  = v->kills_per_era;
    G.start_level    = v->start_era;
    G.demo_era       = G.start_level;
    G.yoke_sens      = v->yoke_sensitivity;
    G.yoke_deadzone  = v->yoke_deadzone;
    G.invert_pitch   = v->invert_pitch;
    G.occlusion      = v->occlusion;
    G.level_select   = v->level_select;
    G.invincible     = v->invincible;
    G.continues      = v->continues;
    G.picture_size   = v->picture_size;
    G.beam_limit     = v->beam_limit;
    world_set_load_limit((float)G.beam_limit * 1000.0f);
    G.view_ini       = v->view;
    G.aim_assist     = v->aim_assist;
    G.volume         = (float)v->volume / 100.0f;
    G.effects_volume = (float)v->effects_volume / 100.0f;
    G.engine_volume  = (float)v->engine_volume / 100.0f;
    G.music_volume   = (float)v->music_volume / 100.0f;
    G.attract_sound  = v->attract_sound;
    G.free_play      = v->free_play;
    G.credits.free_play = v->free_play;
    for (i = 0; i < BRIGHT_COUNT; i++) G.bright[i] = (float)v->brightness[i] / 100.0f;
    snprintf(G.btn_fire, sizeof G.btn_fire, "%s", v->button[SERVICE_FIRE]);
    snprintf(G.btn_faster, sizeof G.btn_faster, "%s", v->button[SERVICE_FASTER]);
    snprintf(G.btn_slower, sizeof G.btn_slower, "%s", v->button[SERVICE_SLOWER]);
    snprintf(G.btn_rear, sizeof G.btn_rear, "%s", v->button[SERVICE_REAR]);
    snprintf(G.menu_key, sizeof G.menu_key, "%s", v->menu_key);
    service_menu_key(G.menu_key);
    for (i = 0; i < 4; i++) G.yoke_layout[i] = v->yoke_layout[i];
    G.control_stick = v->control_stick;
    controls_use_yoke(!G.control_stick);        /* after the line above: the yoke is read by what was chosen now */
    G.stick_x = G.stick_y = 0;
    if (v->reset_scores) {              /* the table as the game first had it */
        remove(vp_hiscore_path());
        default_scores();
        G.table_count = vp_hiscore_load(G.table);
        G.hiscore = G.table_count > 0 ? G.table[0].score : 0;
        if (G.score > G.hiscore) G.hiscore = G.score;
        G.rank = -1;
    }
}

static int save_settings(void)
{
    char path[1024];
    FILE *f = fopen(settings_path(path, sizeof path), "w");
    int i;

    if (!f) return 0;
    fprintf(f, "; Written by the service menu. What stands here goes before game.ini.\n");
    fprintf(f, "lives = %d\n", G.start_lives);
    fprintf(f, "difficulty = %s\n", difficulty_key[G.difficulty]);
    fprintf(f, "bonus_plane = %d\n", G.bonus_plane);
    fprintf(f, "enemies_per_era = %d\n", G.kills_per_era);
    fprintf(f, "start_era = %d\n", G.start_level % ERA_COUNT + 1);
    fprintf(f, "free_play = %s\n", G.free_play ? "yes" : "no");
    fprintf(f, "attract_sound = %s\n", G.attract_sound ? "yes" : "no");
    fprintf(f, "yoke_sensitivity = %.2f\n", (double)G.yoke_sens);
    fprintf(f, "yoke_deadzone = %d\n", G.yoke_deadzone);
    fprintf(f, "invert_pitch = %s\n", G.invert_pitch ? "yes" : "no");
    fprintf(f, "occlusion = %s\n", G.occlusion ? "yes" : "no");
    fprintf(f, "level_select = %s\n", G.level_select ? "yes" : "no");
    fprintf(f, "invincible = %s\n", G.invincible ? "yes" : "no");
    fprintf(f, "continue = %s\n", G.continues ? "yes" : "no");
    fprintf(f, "setup_done = %s\n", G.setup_done ? "yes" : "no");
    fprintf(f, "picture_size = %d\n", G.picture_size);
    fprintf(f, "beam_limit = %d\n", G.beam_limit);
    fprintf(f, "view = %s\n", G.view_ini == VIEW_CHASE ? "chase" : G.view_ini == VIEW_HUD ? "hud" : "cockpit");
    fprintf(f, "aim_assist = %d\n", G.aim_assist);
    fprintf(f, "fire_button = %s\n", G.btn_fire);
    fprintf(f, "faster_button = %s\n", G.btn_faster);
    fprintf(f, "slower_button = %s\n", G.btn_slower);
    fprintf(f, "rear_button = %s\n", G.btn_rear);
    fprintf(f, "menu_button = %s\n", G.menu_key);
    fprintf(f, "control = %s\n", G.control_stick ? "joystick" : "yoke");
    fprintf(f, "yoke_layout = %d %d %d %d\n", G.yoke_layout[0], G.yoke_layout[1], G.yoke_layout[2], G.yoke_layout[3]);
    fprintf(f, "volume = %d\n", (int)(G.volume * 100.0f + 0.5f));
    fprintf(f, "effects_volume = %d\n", (int)(G.effects_volume * 100.0f + 0.5f));
    fprintf(f, "engine_volume = %d\n", (int)(G.engine_volume * 100.0f + 0.5f));
    fprintf(f, "music_volume = %d\n", (int)(G.music_volume * 100.0f + 0.5f));
    for (i = 0; i < BRIGHT_COUNT; i++)
        fprintf(f, "%s = %d\n", bright_key[i], (int)(G.bright[i] * 100.0f + 0.5f));
    return fclose(f) == 0;
}

static void open_service(void)
{
    service_settings now;
    settings_to_service(&now);
    service_open(&now);
    G.service = 1;
    sound_music(MUSIC_NONE);
    G.muted = 0;
    sound_mute(0);
    sound_engine(-1);
    sound_sources_begin();              /* and nothing else that flies is heard in the menu */
    sound_sources_end();
}

static void update_service(vp_input *in, int hotkey)
{
    int result = service_update(in, hotkey);

    if (result == SERVICE_OPEN) return;
    G.service = 0;
    if (result == SERVICE_SETUP) { open_setup(); return; }       /* CONTROL SETUP: what the menu had is not kept */
    if (result == SERVICE_FACTORY) { factory_reset(); return; }
    notice("SETTINGS NOT SAVED");
    if (result == SERVICE_SAVED) {
        settings_from_service(service_get());
        notice(save_settings() ? "SETTINGS SAVED" : "CANNOT SAVE SETTINGS");
        sound_fx(FX_PICKUP);
    }
    sound_volumes(G.volume, G.effects_volume, G.engine_volume, G.music_volume);
    /* Out of the service menu the game always begins again, from its
     * title: whatever was being played is over, and every setting counts
     * from a clean start. */
    enter_attract(PAGE_TITLE);
}

/* The first start (setup.h): until it has been gone through once, the
 * game begins there, not on its title. Its music is the lounge piece of how
 * to play; it is heard even with the attract's sound off, as the menu is. */

static void open_setup(void)
{
    service_settings now;
    settings_to_service(&now);
    setup_open(&now);
    G.setup = 1;
    sound_engine(-1);
    sound_sources_begin();
    sound_sources_end();
}

static void update_setup(vp_input *in)
{
    if (G.muted) { G.muted = 0; sound_mute(0); }
    sound_music(MUSIC_LOUNGE);
    if (setup_update(in) == SERVICE_OPEN) return;
    G.setup = 0;
    settings_from_service(setup_get());
    G.setup_done = 1;
    notice(save_settings() ? "SETTINGS SAVED" : "CANNOT SAVE SETTINGS");
    sound_volumes(G.volume, G.effects_volume, G.engine_volume, G.music_volume);
    enter_attract(PAGE_TITLE);
}

/* FACTORY RESET AND RESTART: save/settings.ini and the high scores go,
 * every setting is as game.ini has it (load_settings), the table as the
 * game first had it, and the game begins again with the first start's
 * setup, as on a new cabinet. */

static void factory_reset(void)
{
    char path[1024];
    remove(settings_path(path, sizeof path));
    remove(vp_hiscore_path());
    G.setup_done = 0;
    G.invincible = 0;
    G.continues = 1;
    G.view_ini = VIEW_HUD;
    G.control_stick = 0;
    G.invert_pitch = 0;
    G.free_play = 0;
    G.attract_sound = 0;
    G.start_level = G.demo_era = 0;
    load_settings();
    G.credits.free_play = G.free_play;
    default_scores();
    G.table_count = vp_hiscore_load(G.table);
    G.hiscore = G.table_count > 0 ? G.table[0].score : 0;
    G.rank = -1;
    sound_volumes(G.volume, G.effects_volume, G.engine_volume, G.music_volume);
    fprintf(stderr, "Time Pilot 3D: factory reset\n");
    enter_attract(PAGE_TITLE);
    open_setup();
}

/* --- init, the step, the self test ------------------------------------------------- */

/* A new table is filled with ten modest scores, as an arcade game's ROM
 * comes with, so the attract page has something to show. */

static void default_scores(void)
{
    static const char *const names[10] = { "KON", "AMI", "TPX", "YOK", "ACE", "SKY", "RED", "BAR", "ON.", "VPI" };
    int i;
    if (vp_hiscore_load(G.table) > 0) return;
    for (i = 0; i < 10; i++) vp_hiscore_insert(names[i], 10000 - 900 * i);
}

void game_init(int vertical)
{
    memset(&G, 0, sizeof G);
    G.rng = 0x7A1E51u;
    load_settings();
    world_init(vertical);
    models_init();
    city_init();
    vp_credits_init(&G.credits);
    if (G.free_play) G.credits.free_play = 1;
    default_scores();
    G.rank = -1;
    enter_attract(PAGE_TITLE);
    if (!G.setup_done) open_setup();
}

void game_start_level(int level)
{
    G.start_level = level > 0 ? level - 1 : 0;
    G.demo_era = G.start_level;             /* the demo shows it too */
}

void game_cheat_kills(int kills)
{
    G.cheat_kills = kills < 0 ? 0 : kills > G.kills_per_era + 1 ? G.kills_per_era + 1 : kills;
}

void game_tunnel_hands_off(void) { G.cheat_hands_off = 1; }

void game_cheat_stunt(void) { G.cheat_stunt = 1; }

void game_cheat_chutes(void) { G.cheat_chutes = 1; }

void game_cheat_boss2(int stage) { G.cheat_boss2 = stage > 0 ? stage : 1; }

void game_cheat_weak(void) { G.cheat_weak = 1; }

void game_cheat_debugsel(int row)
{
#if DEBUG_LEVEL_SELECT
    G.setup = 0;
    G.debug_players = 1;
    G.debug_sel = row;
    enter(DEBUGSEL);
#else
    (void)row;
#endif
}

void game_cheat_service(void) { G.setup = 0; open_service(); service_show_monitor(); }

void game_cheat_setup(int page) { open_setup(); setup_cheat_page(page); }

void game_cheat_continue(void)
{
    G.setup = 0;
    G.continues = 1;
    start_game(1);
    start_level(G.start_level, 1);
    enter(DYING);
    G.p.alive = 0;
    G.lives = 0;
    plane_lost(G.level);
}

void game_cheat_items(void) { G.cheat_items = 1; }

void game_selftest(int steps, int levels)
{
    G.setup = 0;                        /* never stops at the first start's questions */
    G.invincible = 0;                   /* and is the game as it is */
    G.selftest = steps > 0 ? steps : 1;
    G.selftest_levels = levels > 0 ? levels : 5;
    G.credits.free_play = 1;
    start_game(1);
}

void game_attract_page(int page)
{
    G.setup = 0;
    if (page == 3) { enter_attract(PAGE_SCORES); G.state_time = SCORES_TIME; }  /* straight on to the demo */
    else if (page == 6) {                       /* the tutorial after START: its items */
        start_game(1);
        G.state_time = HOWTO_TIME;
    }
    else enter_attract(page == 1 ? PAGE_SCORES : page == 2 ? PAGE_CAST : page == 4 ? PAGE_HOWTO : page == 5 ? PAGE_ITEMS : PAGE_TITLE);
}

void game_update(vp_game *g, float dt)
{
    vp_input *in = vp_game_input(g);
    int i;
    (void)dt;

    G.game = g;
    controls_update();
    if (!G.sound_ready) {
        G.sound_ready = 1;
        sound_init(vp_game_audio(g));
        sound_volumes(G.volume, G.effects_volume, G.engine_volume, G.music_volume);
        G.muted = !G.attract_sound;
        sound_mute(G.muted);
    }
    if (G.selftest) {
        for (i = 0; i < G.selftest && !G.quit; i++) step(in);
        return;
    }
    {
        int hotkey = service_hotkey();
        if (G.service) { update_service(in, hotkey); return; }
        if (hotkey) { G.setup = 0; open_service(); return; }    /* out of the setup too: it comes again at the next start */
        if (G.setup) { update_setup(in); return; }
    }
    step(in);
}
