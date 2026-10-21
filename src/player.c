/* player.c - the plane the player flies: the yoke and the throttle, the
 * flight model with its stall and its loop, the guns and the aim, the
 * items, being hit and being lost, the autopilot of the demo and the
 * self test, what is heard where, and the time tunnel flown. */
#include "game_internal.h"

/* this file's own, in the order below */
static void read_yoke(vp_input *in, float *x, float *y);
static void fire_guns(void);
static int fire_missile(void);
static void autopilot(float *x, float *y, int *fire);
static void kill_player(void);

/* A hit: it costs energy, and the last of it costs the plane. */

/* The plane's roll as it is seen: its bank, and the roll flown by hand. */
float player_roll(void) { return G.p.roll + G.p.flip_roll; }

void damage_player(float amount)
{
    if (!G.p.alive || G.p.shield > 0) return;
    if (G.p.shield_hits > 0) {                  /* the shield takes it: a ring of light, a jolt, no harm */
        G.p.shield_hits--;
        G.p.shake = clampf(G.p.shake + 0.3f, 0, 1.2f);
        point_ring(vp_v3_add(G.p.pos, vp_v3_scale(player_forward(), 14.0f)), player_velocity(), 13, 40.0f, 0.5f);
        shock_ring(G.p.pos, 60.0f, 0.4f, 0);
        sound_fx(FX_HIT_BOSS);
        if (G.p.shield_hits == 0) notice("SHIELD GONE");
        return;
    }
    if (!G.invincible) G.p.energy -= amount * hard()->damage;     /* testing: felt and seen, no harm */
    G.p.hurt = 0.5f;
    /* It is felt: the view shakes, the harder the hit; sparks and white
     * heat fly off the plane and stream back past the canopy; smoke
     * follows. A hard hit bangs as well. */
    G.p.shake = clampf(G.p.shake + amount / 22.0f, 0, 1.2f);
    {
        vp_vec3 at = vp_v3_add(G.p.pos, vp_v3_scale(player_forward(), 22.0f)), v = player_velocity();
        burst(at, v, 12 + (int)(amount / 3.0f), 30.0f, 0.7f, SPARK_FIRE, 0);
        burst(at, v, 8, 45.0f, 0.4f, SPARK_WHITE, 0);
        burst(G.p.pos, v, 5, 10.0f, 1.4f, SPARK_SMOKE, 0);
        if (amount >= 25.0f) pixel_burst(at, v, 0.5f);
    }
    sound_fx(FX_HIT);
    if (amount >= 25.0f) sound_fx(FX_EXPLODE);
    if (G.p.energy <= 0) { G.p.energy = 0; kill_player(); }
}

/* What the yoke says, -1..1 each way, the keys standing in for it. */

static void read_yoke(vp_input *in, float *x, float *y)
{
    float dz = (float)G.yoke_deadzone / 100.0f;
    float ax, ay;

    /* The yoke's whole travel counts, from the edge of the dead zone to
     * the stop: nothing jumps to full on the way (controls.h). */
    controls_axes(in, &ax, &ay);
    if (G.control_stick) {
        /* A stick has switches, not a travel: all or nothing. Here they
         * move a travel of their own - out in a third of a second, back to
         * the middle in well over a second - so the plane is not thrown
         * about, and does not snap level the moment the stick is let go. */
        const float OUT = 3.2f * DT, BACK = 0.8f * DT;
        float want[2], *now[2];
        int i;
        want[0] = ax; want[1] = ay;
        now[0] = &G.stick_x; now[1] = &G.stick_y;
        for (i = 0; i < 2; i++) {
            float step = fabsf(want[i]) > 0.01f ? OUT : BACK;
            *now[i] += clampf(want[i] - *now[i], -step, step);
        }
        *x = G.stick_x;
        *y = G.stick_y;
        return;
    }
    if (dz > 0.9f) dz = 0.9f;
    ax = fabsf(ax) < dz ? 0 : (ax - (ax > 0 ? dz : -dz)) / (1.0f - dz);
    ay = fabsf(ay) < dz ? 0 : (ay - (ay > 0 ? dz : -dz)) / (1.0f - dz);
    *x = clampf(ax, -1, 1);
    *y = clampf(ay, -1, 1);
}

/* The nearest enemy (or missile, or the boss) within `cone` radians of
 * the guns, and where to shoot to meet it. */

obj *gun_target(float cone, vp_vec3 *aim)
{
    vp_vec3 fwd = player_forward();
    obj *best = NULL;
    float best_angle = cone;
    int i;

    for (i = 0; i < MAX_OBJ; i++) {
        obj *o = &G.objs[i];
        vp_vec3 to;
        float d, angle;
        if (o->kind != K_ENEMY && o->kind != K_BOSS && o->kind != K_MISSILE) continue;
        to = vp_v3_sub(o->pos, G.p.pos);
        d = vp_v3_length(to);
        if (d < 10.0f || d > 700.0f) continue;
        angle = acosf(clampf(vp_v3_dot(fwd, vp_v3_scale(to, 1.0f / d)), -1, 1));
        if (angle < best_angle) {
            best_angle = angle;
            best = o;
            /* lead it; of a boss, its nearest gun while it has one */
            *aim = vp_v3_add(o->kind == K_BOSS ? boss_aim_point(o) : o->pos, vp_v3_scale(o->vel, d / (SHOT_SPEED + G.p.speed)));
        }
    }
    return best;
}

static void fire_guns(void)
{
    vp_vec3 fwd = player_forward();
    vp_vec3 right = vp_v3(cosf(G.p.heading), 0, -sinf(G.p.heading));
    vp_vec3 dir = fwd, aim, meet;
    int k, shots = G.p.triple > 0 ? 3 : 1;

    if (G.aim_assist > 0 && gun_target(ASSIST_CONE * (float)G.aim_assist / 100.0f, &aim))
        dir = vp_v3_normalize(vp_v3_sub(aim, G.p.pos));
    meet = vp_v3_add(G.p.pos, vp_v3_scale(dir, GUNS_MEET));
    G.p.gun = !G.p.gun;
    for (k = 0; k < shots; k++) {               /* with the triple shot: one straight, one a little to each side */
        obj *s = new_obj(K_SHOT);
        float a = shots == 3 ? 0.05f * (float)(k - 1) : 0, c = cosf(a), n = sinf(a);
        vp_vec3 way;
        if (!s) return;
        s->pos = vp_v3_add(G.p.pos, vp_v3_scale(right, G.p.gun ? 1.6f : -1.6f));
        s->pos = vp_v3_add(s->pos, vp_v3_scale(fwd, 4.0f));
        /* Each gun points in a little, so that both cross on the sight's
         * line GUNS_MEET ahead; and a shot goes with the plane's own speed
         * on top of its own (2026-10-21: without it the stream fell behind
         * the nose in every turn). */
        way = vp_v3_normalize(vp_v3_sub(meet, s->pos));
        s->vel = vp_v3_add(vp_v3_scale(vp_v3(way.x * c - way.z * n, way.y, way.x * n + way.z * c), SHOT_SPEED), player_velocity());
        s->life = SHOT_LIFE;
    }
    {
        static unsigned rnd = 0x1234567u;
        rnd = rnd * 1664525u + 1013904223u;
        sound_fx_ex(FX_SHOT, 1.0f, 0.96f + 0.08f * (float)(rnd >> 8) / 16777216.0f);
    }
}

/* One of the player's homing missiles is away, after whatever is nearest
 * the nose within half a right angle. Gives back 0 if there is nothing
 * to send it after. */

static int fire_missile(void)
{
    vp_vec3 aim;
    obj *m;
    if (!gun_target(0.8f, &aim)) return 0;
    if (!(m = new_obj(K_PMISSILE))) return 0;
    m->model = MODEL_MISSILE;
    m->pos = vp_v3_sub(vp_v3_add(G.p.pos, vp_v3_scale(player_forward(), 5.0f)), vp_v3(0, 2.0f, 0));
    m->heading = course_of(G.p.heading, G.p.pitch);
    m->pitch = asinf(clampf(player_forward().y, -1, 1));
    m->speed = 300.0f;
    m->turn = 2.8f;
    m->life = 4.0f;
    sound_fx(FX_MISSILE);
    return 1;
}

/* An item is the player's. */

const char *const item_names[ITEM_COUNT] = { "SHIELD", "MISSILES", "TRIPLE SHOT", "REPAIR", "FLARES" };

const unsigned char item_rgb[ITEM_COUNT][3] = { { 80, 255, 255 }, { 255, 90, 90 }, { 255, 230, 0 }, { 90, 255, 120 }, { 255, 150, 40 } };

/* Which item a gate or a depot gives is chance, and is not known before:
 * any of them, but no repair for a plane that is as good as whole, and
 * flares only in the eras that have missiles to draw off - there they
 * come up twice as often as the others. */

int random_item(void)
{
    int item;
    if (era()->missiles > 0) {
        item = rnd(ITEM_COUNT + 1);
        if (item >= ITEM_COUNT) item = ITEM_FLARES;
    } else {
        item = rnd(ITEM_FLARES);
    }
    if (item == ITEM_REPAIR && G.p.energy > 0.85f * ENERGY_FULL) item = rnd(ITEM_REPAIR);
    return item;
}

void grant_item(int item)
{
    switch (item) {
    case ITEM_SHIELD:   G.p.shield_hits = SHIELD_HITS; break;
    case ITEM_MISSILES: G.p.missiles += MISSILE_LOAD; if (G.p.missiles > MISSILE_MAX) G.p.missiles = MISSILE_MAX; break;
    case ITEM_TRIPLE:   G.p.triple = TRIPLE_TIME; break;
    case ITEM_FLARES:   G.p.flares = FLARE_TIME; break;
    default:            G.p.energy = ENERGY_FULL; break;
    }
    add_score(ITEM_POINTS);
    cheer(item_names[item]);
    sound_fx(FX_BONUS);
    point_ring(G.p.pos, player_velocity(), 13, 60.0f, 0.6f);
}

/* The demo's and the self test's hands: at the nearest enemy, guns when
 * it is in front, away when it is about to be flown into, up when the
 * ground or a building is near, the throttle back to turn inside one
 * that is close. */

static void autopilot(float *x, float *y, int *fire)
{
    vp_vec3 fwd = player_forward(), ahead;
    obj *best = NULL;
    float best_d = 1e9f, safe;
    int i;

    *x = *y = 0;
    *fire = 0;
    for (i = 0; i < MAX_OBJ; i++) {
        obj *o = &G.objs[i];
        float d;
        if (o->kind != K_ENEMY && o->kind != K_BOSS) continue;
        d = dist3(o->pos, G.p.pos) + (o->kind == K_BOSS ? -200.0f : 0);
        if (d < best_d) { best_d = d; best = o; }
    }
    /* after a boss that is far off: the throttle open, or a fast one is never caught */
    G.p.throttle += ((best && best->kind == K_BOSS && best_d > 60.0f ? 1.0f : THROTTLE_MID) - G.p.throttle) * 0.05f;
    if (best) {
        vp_vec3 to = vp_v3_sub(best->kind == K_BOSS ? boss_aim_point(best) : best->pos, G.p.pos);     /* of a boss, the part to shoot at */
        float horiz = sqrtf(to.x * to.x + to.z * to.z);
        float dh = wrap_angle(atan2f(to.x, to.z) - G.p.heading);
        float dp = clampf(atan2f(to.y, horiz), -0.5f, 0.5f) - clampf(G.p.pitch, -1.0f, 1.0f);
        *x = clampf(dh * 2.5f, -1, 1);
        *y = clampf(dp * 2.5f, -1, 1);               /* back (positive) climbs */
        if (fabsf(dh) < 0.12f && fabsf(dp) < 0.12f && vp_v3_length(to) < 500.0f) *fire = 1;
        if (vp_v3_length(to) < (best->kind == K_BOSS ? 130.0f : 30.0f)) { *x = dh > 0 ? -1.0f : 1.0f; *y = 0.6f; }
        /* One that is near and not ahead is inside the turn: slower turns
         * tighter. Otherwise the cruising speed. */
        G.p.throttle += ((vp_v3_length(to) < 260.0f && fabsf(dh) > 0.4f ? 0.05f : 0.45f) - G.p.throttle) * 0.05f;
    }
    /* the ground, and what stands a second and three seconds ahead */
    safe = 110.0f;
    ahead = vp_v3_add(G.p.pos, vp_v3_scale(fwd, G.p.speed));
    safe += city_top(G.era, ahead.x, ahead.z, 40.0f);
    ahead = vp_v3_add(G.p.pos, vp_v3_scale(fwd, G.p.speed * 3.0f));
    if (city_top(G.era, ahead.x, ahead.z, 40.0f) + 110.0f > safe) safe = city_top(G.era, ahead.x, ahead.z, 40.0f) + 110.0f;
    if (G.p.pos.y < safe + 60.0f && *y < 0) *y = 0;
    if (G.p.pos.y < safe) *y = 1;                    /* pull up */
    if (G.p.pos.y > 650.0f) *y = -0.5f;
    if (G.p.stalled) { *y = 0; *x = 0; }             /* let the nose fall, and the speed come back */
    if (G.invert_pitch) *y = -*y;                    /* read back through the setting */
}

/* The plane flies: the yoke turns and tilts it, the throttle and the
 * slope it flies set its speed, and too little speed stalls it. */

void update_player(vp_input *in)
{
    float x, y, pitch_cmd, authority, target, accel, turn, roll_x = 0;
    vp_vec3 fwd;
    int fire, rolling = 0;

    if (G.demo || G.selftest) {
        autopilot(&x, &y, &fire);
        if (G.cheat_stunt && fmodf(G.level_time, 12.0f) > 4.0f && fmodf(G.level_time, 12.0f) < 11.0f) {
            /* the testing aid: up and over onto its back, then a roll upright */
            float when = fmodf(G.level_time, 12.0f);
            x = 0;
            y = when < 8.5f && cosf(G.p.pitch) > -0.9f ? (G.invert_pitch ? -1.0f : 1.0f) : 0;
            if (when >= 8.5f && cosf(G.p.pitch) < 0) { rolling = 1; x = 1.0f; }
        }
    } else {
        int faster = controls_held(in, G.btn_faster), slower = controls_held(in, G.btn_slower);
        read_yoke(in, &x, &y);
        rolling = faster && slower;
        fire = controls_held(in, G.btn_fire);
        /* The throttle rests in the middle. A button held moves it that
         * way, twice as fast while it is still on the other side of the
         * middle; let go, it drifts back slowly. */
        if (faster && !slower)
            G.p.throttle += THROTTLE_RATE * (G.p.throttle < THROTTLE_MID ? THROTTLE_CROSS : 1.0f) * DT;
        else if (slower && !faster)
            G.p.throttle -= THROTTLE_RATE * (G.p.throttle > THROTTLE_MID ? THROTTLE_CROSS : 1.0f) * DT;
        else
            G.p.throttle += clampf(THROTTLE_MID - G.p.throttle, -THROTTLE_BACK * DT, THROTTLE_BACK * DT);
    }
    G.p.rear = !G.demo && !G.selftest && controls_held(in, G.btn_rear);
    /* The afterburner lights while the plane is being pushed faster, and
     * stays half lit as long as the throttle is well open. */
    {
        float want = !G.demo && !G.selftest && controls_held(in, G.btn_faster) ? 1.0f
                   : G.p.throttle > 0.7f ? 0.55f : 0.0f;
        G.p.burner += (want - G.p.burner) * 0.15f;
    }
    G.p.shake *= 0.93f;
    /* a plane shot half to pieces smokes, and worse than that it burns */
    if (G.p.energy < 45.0f && G.frames % 5 == 0)
        burst(vp_v3_sub(G.p.pos, vp_v3_scale(player_forward(), 6.0f)), vp_v3(0, 0, 0), 1, 4.0f, 1.5f, SPARK_SMOKE, 0);
    if (G.p.energy < 25.0f && G.frames % 7 == 0)
        burst(vp_v3_sub(G.p.pos, vp_v3_scale(player_forward(), 6.0f)), vp_v3(0, 0, 0), 2, 8.0f, 0.5f, SPARK_FIRE, 0);
    G.p.throttle = clampf(G.p.throttle, 0, 1);
    if (rolling) { roll_x = x; x = 0; }     /* both speed buttons held: the yoke's sideways way rolls, and does not turn */
    /* The yoke reports forward as negative, like a stick up: forward dives
     * (the aircraft way) unless the setting turns it round. */
    pitch_cmd = G.invert_pitch ? -y : y;

    /* the wing: flying, or not */
    if (!G.p.stalled && G.p.speed < STALL_SPEED) G.p.stalled = 1;
    if (G.p.stalled && G.p.speed > STALL_OVER) G.p.stalled = 0;
    authority = G.p.stalled ? 0.25f : 1.0f;          /* the controls go slack */
    turn = TURN_RATE * clampf(TURN_SPEED / G.p.speed, 0.65f, 1.5f);

    /* On its back the plane turns the other way over the ground for the
     * same yoke, so that left stays the pilot's left all the way round. */
    G.p.heading = wrap_angle(G.p.heading + x * turn * clampf(cosf(G.p.pitch) * 4.0f, -1, 1) * G.yoke_sens * authority * DT);
    G.p.pitch += pitch_cmd * PITCH_RATE * G.yoke_sens * authority * DT;
    if (G.p.stalled) {                                  /* the nose drops, the shorter way to straight down */
        float down = G.p.pitch > PI / 2.0f ? 2.0f * PI - 2.2f : fabsf(G.p.pitch) <= PI / 2.0f ? -0.9f : -2.2f;
        G.p.pitch += (down - G.p.pitch) * 1.6f * DT;
    }
    if (G.p.pos.y > ALT_MAX - 60.0f && sinf(G.p.pitch) > 0)             /* the sky's top: the nose is eased level */
        G.p.pitch += (fabsf(G.p.pitch) < PI / 2.0f ? -0.6f : 0.6f) * DT;
    G.p.pitch = wrap_angle(G.p.pitch);
    /* The roll, by hand: with both speed buttons held the yoke's
     * sideways way rolls the plane about its length, as far and as often
     * as it is held - upright again after half a loop, on its back in
     * level flight, or right round. Past a quarter turn the same attitude
     * is said the other way (round by half a turn, the pitch mirrored,
     * the roll half a turn less): nothing moves, and the roll kept here
     * never passes a quarter turn. Let go, the plane settles on the
     * nearer of upright and on its back. */
    if (rolling) G.p.flip_roll -= roll_x * ROLL_RATE * G.yoke_sens * authority * DT;
    else G.p.flip_roll -= G.p.flip_roll * 0.1f;
    if (fabsf(G.p.flip_roll) > PI / 2.0f) {
        G.p.flip_roll += G.p.flip_roll > 0 ? -PI : PI;
        G.p.heading = wrap_angle(G.p.heading + PI);
        G.p.pitch = wrap_angle(PI - G.p.pitch);
        G.p.cam_heading = wrap_angle(G.p.cam_heading + PI);
        G.p.cam_pitch = wrap_angle(PI - G.p.cam_pitch);
    }
    G.p.roll += (-x * BANK_MAX + (G.p.stalled ? 0.5f * sinf((float)G.frames * 0.12f) : 0) - G.p.roll) * 0.08f;

    /* the speed: toward what the throttle asks, less in a climb and a
     * turn, more in a dive */
    target = SPEED_MIN + G.p.throttle * (SPEED_MAX - SPEED_MIN);
    accel = clampf((target - G.p.speed) * 0.45f, -22.0f, 45.0f);
    accel -= GRAVITY_PULL * sinf(G.p.pitch);
    accel -= fabsf(x) * 5.0f;
    G.p.speed = clampf(G.p.speed + accel * DT, 15.0f, SPEED_TOP);

    fwd = player_forward();
    G.p.pos = vp_v3_add(G.p.pos, vp_v3_scale(fwd, G.p.speed * DT));
    if (G.p.stalled) G.p.pos.y -= 10.0f * DT;        /* and it sinks */
    if (G.p.pos.y > ALT_MAX) G.p.pos.y = ALT_MAX;

    /* the chase camera follows a little behind the turn */
    G.p.cam_heading += wrap_angle(G.p.heading - G.p.cam_heading) * 0.12f;
    G.p.cam_pitch = wrap_angle(G.p.cam_pitch + wrap_angle(G.p.pitch - G.p.cam_pitch) * 0.12f);

    if (G.p.shield > 0) G.p.shield -= DT;
    if (G.p.hurt > 0) G.p.hurt -= DT;
    if (G.p.ram_cd > 0) G.p.ram_cd -= DT;
    G.p.fire_cd -= DT;
    if (fire && G.p.fire_cd <= 0 && count_kind(K_SHOT) < MAX_SHOTS) {
        fire_guns();
        G.p.fire_cd = FIRE_PERIOD;
    }
    /* the homing missiles go by themselves while the guns fire: one
     * every 0.8 seconds, as long as there is something to send it after */
    if (G.p.triple > 0) G.p.triple -= DT;
    if (G.p.flares > 0) G.p.flares -= DT;
    if (G.p.flare_show > 0) {               /* flares out of the tail: white fire, falling behind */
        G.p.flare_show -= DT;
        if (G.frames % 4 == 0) {
            vp_vec3 back = vp_v3_scale(player_forward(), -1.0f);
            vp_vec3 side = vp_v3(cosf(G.p.heading), 0, -sinf(G.p.heading));
            float way = (G.frames / 4) & 1 ? 1.0f : -1.0f;
            vp_vec3 v = vp_v3_add(vp_v3_add(vp_v3_scale(back, 35.0f), vp_v3_scale(side, 22.0f * way)), vp_v3(0, -14.0f, 0));
            burst(vp_v3_add(G.p.pos, vp_v3_scale(back, 7.0f)), vp_v3_add(player_velocity(), v), 3, 9.0f, 1.1f, SPARK_WHITE, 0);
            burst(vp_v3_add(G.p.pos, vp_v3_scale(back, 7.0f)), vp_v3_add(player_velocity(), v), 2, 6.0f, 0.9f, SPARK_FIRE, 0);
        }
    }
    if (G.p.missile_cd > 0) G.p.missile_cd -= DT;
    if (fire && G.p.missiles > 0 && G.p.missile_cd <= 0 && fire_missile()) {
        G.p.missiles--;
        G.p.missile_cd = 0.8f;
    }
    if (G.p.stalled && G.frames % 18 == 0) sound_fx(FX_STALL);
    {
        /* a missile after the plane: a two-tone warning, quicker the nearer */
        float near;
        if (nearest_missile(&near) && near < 700.0f && G.frames % (near < 150.0f ? 10u : near < 350.0f ? 18u : 30u) == 0) sound_fx(FX_WARN);
    }
    /* the engine's note follows the throttle at once, and the speed as it comes */
    sound_engine_load(clampf(0.35f * (G.p.speed - SPEED_MIN) / (SPEED_TOP - SPEED_MIN) + 0.65f * G.p.throttle, 0, 1));

    /* the ground ahead: less than two and a half seconds away */
    G.p.pull_up = G.p.pos.y < 45.0f
               || (fwd.y < -0.05f && (G.p.pos.y - ALT_CRASH) / (-fwd.y * G.p.speed) < 2.5f);

    /* the ground, and what is built on it: the end of the plane, whatever
     * its energy */
    if (G.invincible && (G.p.pos.y <= ALT_CRASH || city_hit(G.era, G.p.pos, 2.5f))) {
        /* testing: thrown up over it, nose up, with a jolt */
        G.p.pos.y = (G.p.pos.y <= ALT_CRASH ? ALT_CRASH : city_top(G.era, G.p.pos.x, G.p.pos.z, 0)) + 15.0f;
        if (G.p.pitch < 0.3f) G.p.pitch = 0.3f;
        G.p.shake = clampf(G.p.shake + 0.4f, 0, 1.2f);
    } else if (G.p.pos.y <= ALT_CRASH || city_hit(G.era, G.p.pos, 2.5f)) {
        if (G.p.pos.y < ALT_CRASH) G.p.pos.y = ALT_CRASH;
        G.p.energy = 0;
        kill_player();
    }
}

/* What else flies is heard where it is: every enemy, big one, boss and
 * missile has its engine with it, to the left or right of the way the
 * player looks, louder the nearer, and higher coming than going. The
 * sound module keeps the loudest few. */

/* An effect that happens somewhere: the farther from the plane, the
 * quieter (a third as loud at twice its `reach`, slower than the square
 * of the distance: far things are still heard), a little deeper far off,
 * from its side of the way the player looks, and never twice quite the
 * same - its pitch a little up or down. */
void sound_at(int fx, vp_vec3 at)
{
    static unsigned rnd = 0x2545F491u;
    float reach = fx == FX_BIG_EXPLODE ? 650.0f : fx == FX_EXPLODE ? 340.0f : fx == FX_ENEMY_SHOT ? 210.0f : fx == FX_FALL ? 280.0f
                : fx == FX_SCREAM ? 300.0f : 320.0f;
    vp_vec3 to = vp_v3_sub(at, G.p.pos);
    float d = vp_v3_length(to), k = d / reach, gain = 1.0f / (1.0f + powf(k, 1.6f)), r, h = G.view_heading;
    if (gain < 0.03f) return;
    rnd = rnd * 1664525u + 1013904223u;
    r = (float)(rnd >> 8) / 16777216.0f;
    sound_fx_at(fx, gain, (fx == FX_ENEMY_SHOT || fx == FX_SCREAM ? 0.95f + 0.1f * r : 0.86f + 0.26f * r) * (1.0f - 0.12f * clampf(d / 1500.0f, 0, 1)),
                d > 1.0f ? vp_v3_dot(to, vp_v3(cosf(h), 0, -sinf(h))) / d : 0.0f);
}

void hear_the_sky(void)
{
    float h = G.view_heading;
    vp_vec3 right = vp_v3(cosf(h), 0, -sinf(h)), pvel = player_velocity();
    int i;

    sound_tick(DT);
    /* in the cockpit only while flown from it; in every film from outside */
    sound_engine_view(G.p.alive && !G.chase && !G.p.rear && (G.state == PLAYING || G.state == INTRO));
    sound_sources_begin();
    if (G.p.alive && (G.state == PLAYING || G.state == INTRO)) {
        for (i = 0; i < MAX_OBJ; i++) {
            obj *o = &G.objs[i];
            vp_vec3 to;
            float d, gain, closing, reach;
            int kind;
            if (o->kind != K_ENEMY && o->kind != K_BOSS && o->kind != K_MISSILE) continue;
            to = vp_v3_sub(o->pos, G.p.pos);
            d = vp_v3_length(to);
            if (d < 1.0f) d = 1.0f;
            /* the big ones carry further */
            reach = o->kind == K_BOSS ? 320.0f : o->heavy ? 230.0f : o->kind == K_MISSILE ? 140.0f : 130.0f;
            gain = 1.0f / (1.0f + (d / reach) * (d / reach));
            /* how fast the two come together, as a share of the speed of sound */
            closing = -vp_v3_dot(vp_v3_sub(o->vel, pvel), to) / d / 340.0f;
            kind = o->kind == K_MISSILE ? SOURCE_MISSILE : SOURCE_1910 + G.era;
            sound_source(i + 1, kind, vp_v3_dot(to, right) / d, gain,
                         (o->kind == K_BOSS ? 0.6f : o->heavy ? 0.75f : 1.0f) * clampf(1.0f + closing, 0.7f, 1.5f));
            {
                /* A pass: nearer than FLYBY_NEAR (more for the big ones) and
                 * now going away again, fast enough against the plane - a
                 * rush past on its side, higher the faster, deeper the
                 * bigger. Once; again when it has been far off. */
                float near = o->kind == K_BOSS ? 2.2f * FLYBY_NEAR : o->heavy ? 1.5f * FLYBY_NEAR : FLYBY_NEAR;
                float rel = vp_v3_length(vp_v3_sub(o->vel, pvel));
                if (o->heard_d > 0 && d > o->heard_d && o->heard_d < near && !o->heard_by && rel > 35.0f) {
                    static const int by_era[ERA_COUNT] = { FLYBY_1910, FLYBY_1940, FLYBY_ROTOR, FLYBY_JET, FLYBY_UFO };
                    int what = o->kind == K_MISSILE ? FLYBY_MISSILE : o->kind == K_BOSS ? FLYBY_BOSS
                             : o->model == MODEL_GUNSHIP || o->model == MODEL_BIGHELI ? FLYBY_ROTOR : by_era[G.era];
                    sound_flyby(what, vp_v3_dot(to, right) / d, clampf(1.2f - o->heard_d / near, 0.35f, 1.0f),
                                (o->heavy ? 0.75f : 1.0f) * clampf(0.8f + rel / 400.0f, 0.8f, 1.3f));
                    o->heard_by = 1;
                }
                if (d > 2.5f * near) o->heard_by = 0;
                o->heard_d = d;
            }
        }
    }
    sound_sources_end();
    /* the player's own afterburner, in the middle: in flight, coming out
     * of the tunnel, and on the run into the next one */
    sound_burner(G.p.alive && (G.state == PLAYING || G.state == INTRO
                               || (G.state == WARP && G.state_time < TUNNEL_APPROACH)) ? G.p.burner : 0.0f);
    /* the engine kept down in the film of the way in (the music has it),
     * and up over the first seconds of flight */
    sound_engine_scale(G.state == LEVELIN ? 0.25f : G.state == INTRO ? 0.25f + 0.75f * clampf(G.state_time / INTRO_TIME, 0, 1) : 1.0f);
}

static void kill_player(void)
{
    if (!G.p.alive) return;
    G.p.alive = 0;
    G.p.stalled = G.p.pull_up = 0;
    {
        /* the plane itself goes to pieces, as its enemies do */
        int low = G.p.pos.y <= 10.0f;
        obj self;
        memset(&self, 0, sizeof self);
        self.kind = K_PART;                 /* drawn at its own size */
        self.speed = 1.0f;
        self.model = MODEL_PLAYER;
        self.pos = G.p.pos;
        self.vel = low ? vp_v3(0, 30.0f, 0) : player_velocity();
        self.heading = G.p.heading; self.pitch = G.p.pitch; self.roll = G.p.roll;
        shatter(&self, 50.0f, CUT_NONE, 1);
        burst(G.p.pos, self.vel, 20, 60.0f, 1.4f, SPARK_FIRE, low);
        burst(G.p.pos, self.vel, 20, 90.0f, 1.5f, SPARK_RAINBOW, low);
        sphere_burst(G.p.pos, self.vel, 100.0f, 1.4f);
        burst(G.p.pos, self.vel, 8, 20.0f, 2.0f, SPARK_SMOKE, low);
        shock_ring(G.p.pos, 110.0f, 0.8f, low);
    }
    sound_fx(FX_DEATH);
    sound_engine(-1);
    clear_kind(K_BULLET);
    clear_kind(K_MISSILE);
    clear_kind(K_BOMB);
    G.selftest_deaths++;
    enter(DYING);
}

/* The time tunnel is flown. It bends; the plane keeps its line unless the
 * yoke slides it after the tunnel. The wall costs energy and throws the
 * plane back in; every ring passed is worth points, through its middle
 * four times as many. With the energy gone the plane is lost on the way,
 * and the next era is reached with the next one. */

/* The way into the tunnel: the plane flies out of the era into the mouth
 * that has opened ahead of it, hands off, on full power. It is on a rail:
 * a straight line to the middle of the mouth, reached to the frame when
 * TUNNEL_APPROACH is over, and flown on at the last speed for the moment
 * the era's sky is still seen (TUNNEL_MOUTH). Wings level first, then one
 * slow roll on the way in. */
static void tunnel_approach(void)
{
    /* The way out of the era (TUNNEL_APPROACH): the tunnel's mouth opens
     * ahead while the plane flies on at its speed (APPROACH_OPEN); then it
     * flies a loop, a whole one, still going on along the way
     * (APPROACH_LOOP, LOOP_RADIUS); then it opens up and runs into the
     * mouth, faster and faster, to be there exactly at TUNNEL_APPROACH. */
    const float t = G.state_time, t_loop = APPROACH_OPEN, t_run = APPROACH_OPEN + APPROACH_LOOP;
    const float rest = TUNNEL_APPROACH - t_run, v0 = G.tun.v0;
    float d, vf, vy = 0, up = 0, in_loop = 0;

    if (t < t_run) { d = v0 * t; vf = v0; }
    else {
        float tau = t - t_run, acc = 2.0f * (G.tun.len - v0 * TUNNEL_APPROACH) / (rest * rest);
        d = v0 * t_run + v0 * tau + 0.5f * acc * tau * tau;
        vf = v0 + acc * tau;
    }
    G.tun.base = vp_v3_add(G.tun.from, vp_v3_scale(G.tun.axis, d));
    if (t > t_loop && t < t_run) {
        float u = (t - t_loop) / APPROACH_LOOP, ang = PI2 * smooth(u), w = PI2 * 6.0f * u * (1.0f - u) / APPROACH_LOOP;
        d += LOOP_RADIUS * sinf(ang);
        up = LOOP_RADIUS * (1.0f - cosf(ang));
        vf += LOOP_RADIUS * w * cosf(ang);
        vy = LOOP_RADIUS * w * sinf(ang);
        in_loop = 1;
        if (t - DT <= t_loop) sound_fx(FX_WHOOSH);
    }
    G.p.pos = vp_v3_add(vp_v3_add(G.tun.from, vp_v3_scale(G.tun.axis, d)), vp_v3(0, up, 0));
    G.p.heading = atan2f(G.tun.axis.x, G.tun.axis.z);
    if (in_loop) G.p.pitch = atan2f(vy, vf);                     /* the nose round with the loop, over the top on its back */
    else G.p.pitch += (asinf(clampf(G.tun.axis.y, -1.0f, 1.0f)) - G.p.pitch) * 0.1f;
    G.p.roll += -G.p.roll * 0.12f;
    G.p.speed = sqrtf(vf * vf + vy * vy);
    G.p.burner += ((t < t_loop ? 0.2f : in_loop ? 0.6f : 1.0f) - G.p.burner) * 0.1f;
    G.p.cam_heading = G.p.heading;
    G.p.cam_pitch = G.p.pitch;
    G.p.shake *= 0.93f;
    if (t < TUNNEL_APPROACH) {
        if (G.p.hurt > 0) G.p.hurt -= DT;
        sound_engine_load(t < t_run ? 0.5f : 1.0f);
    }
}

void update_tunnel(vp_input *in)
{
    float t = G.state_time - TUNNEL_APPROACH;
    /* The throttle works here as in the sky, resting in the middle:
     * shut, the tunnel comes slower and is easier; open, faster, and
     * every ring is worth more. */
    float pace = G.tun.throttle < THROTTLE_MID
               ? TUNNEL_SLOW + (1.0f - TUNNEL_SLOW) * G.tun.throttle / THROTTLE_MID
               : 1.0f + (TUNNEL_FAST - 1.0f) * (G.tun.throttle - THROTTLE_MID) / (1.0f - THROTTLE_MID);
    float speed = (150.0f + 13.0f * t + 10.0f * (float)G.loop) * pace;
    float s, cx, cy, dx, dy, d;
    int ring, carried = t < TUNNEL_IN || t > TUNNEL_TIME - TUNNEL_OUT || G.tun.lost;

    if (t < TUNNEL_MOUTH) tunnel_approach();
    if (t < 0) return;
    if (t < DT) { sound_fx(FX_WARP); sound_engine(-1); }      /* in: the tunnel's own sound */
    G.tun.dist += speed * DT;
    s = G.tun.dist + TUNNEL_AHEAD;
    world_tunnel_centre(s, G.tun.amp, &cx, &cy);
    if (G.p.hurt > 0) G.p.hurt -= DT;
    if (G.tun.touch > 0) G.tun.touch -= DT;
    if (G.tun.good > 0) G.tun.good -= DT;

    if (carried && t < TUNNEL_IN) {     /* in along the middle, hands off */
        G.tun.vx = (cx - G.tun.x) * 4.0f;
        G.tun.vy = (cy - G.tun.y) * 4.0f;
    } else if (carried) {               /* and out: the way to the middle is eased into, no jolt */
        G.tun.vx += ((cx - G.tun.x) * 2.5f - G.tun.vx) * 0.06f;
        G.tun.vy += ((cy - G.tun.y) * 2.5f - G.tun.vy) * 0.06f;
    } else {
        float x, y;
        if (G.cheat_hands_off) {
            x = y = 0;                  /* the testing aid: nobody steers */
        } else if (G.demo || G.selftest) {     /* after where the middle will be in a quarter of a second */
            float ax, ay;
            world_tunnel_centre(s + speed * 0.25f, G.tun.amp, &ax, &ay);
            x = clampf((ax - G.tun.x) * 4.0f / TUNNEL_SIDE, -1, 1);
            y = clampf((ay - G.tun.y) * 4.0f / TUNNEL_UP, -1, 1);
        } else {
            int faster = controls_held(in, G.btn_faster), slower = controls_held(in, G.btn_slower);
            read_yoke(in, &x, &y);
            if (G.invert_pitch) y = -y; /* the yoke back climbs, as in the sky */
            if (faster && !slower)
                G.tun.throttle += THROTTLE_RATE * (G.tun.throttle < THROTTLE_MID ? THROTTLE_CROSS : 1.0f) * DT;
            else if (slower && !faster)
                G.tun.throttle -= THROTTLE_RATE * (G.tun.throttle > THROTTLE_MID ? THROTTLE_CROSS : 1.0f) * DT;
            else
                G.tun.throttle += clampf(THROTTLE_MID - G.tun.throttle, -THROTTLE_BACK * DT, THROTTLE_BACK * DT);
            G.tun.throttle = clampf(G.tun.throttle, 0, 1);
        }
        G.tun.vx += (x * TUNNEL_SIDE * G.yoke_sens - G.tun.vx) * 0.12f;
        G.tun.vy += (y * TUNNEL_UP * G.yoke_sens - G.tun.vy) * 0.12f;
    }
    G.tun.x += G.tun.vx * DT;
    G.tun.y += G.tun.vy * DT;

    dx = G.tun.x - cx;
    dy = G.tun.y - cy;
    d = sqrtf(dx * dx + dy * dy);
    G.tun.off = clampf(d / TUNNEL_WALL, 0, 1);
    G.tun.mark_x = -dx;
    G.tun.mark_y = -dy;
    if (carried) return;

    if (d > TUNNEL_WALL) {              /* the wall: back inside, and away from it */
        float k = (TUNNEL_WALL - 6.0f) / d;
        G.tun.x = cx + dx * k;
        G.tun.y = cy + dy * k;
        G.tun.vx = -dx / d * 60.0f;
        G.tun.vy = -dy / d * 60.0f;
        if (G.tun.touch <= 0) {
            G.tun.touch = 0.45f;
            if (!G.invincible) G.p.energy -= DMG_TUNNEL * hard()->damage;
            G.p.hurt = 0.35f;
            sound_fx(FX_HIT);
            if (G.p.energy <= 0) {      /* it does not come through */
                G.p.energy = 0;
                G.tun.lost = 1;
                G.selftest_deaths++;
                sound_fx(FX_DEATH);
            }
        }
    }
    ring = (int)(s / TUNNEL_STEP);
    if (ring != G.tun.ring) {           /* through another ring */
        /* worth more the faster it is flown: half at the slowest, half as much again at the fastest */
        int points = (d < TUNNEL_PERFECT ? TUNNEL_PERFECT_POINTS : TUNNEL_POINTS) * (1 + G.loop);
        points = (int)((float)points * (0.5f + G.tun.throttle) / 10.0f + 0.5f) * 10;
        G.tun.ring = ring;
        G.tun.rings++;
        /* through its middle a click, loud over the tunnel's music; the
         * chime on every ring (FX_RING, 0.50.1) got on the nerves (2026-10-21) */
        if (d < TUNNEL_PERFECT) { G.tun.perfect++; G.tun.good = 0.5f; G.tun.good_points = points; sound_fx_ex(FX_TINK, 2.4f, 1.0f); }
        G.tun.bonus += points;
        add_score(points);
    }
    /* the last ring of the flown part: all of them through the middle? */
    if (t + DT > TUNNEL_TIME - TUNNEL_OUT && G.tun.rings > 0 && G.tun.perfect == G.tun.rings) {
        cheer("PERFECT");
        G.tun.bonus += TUNNEL_ALL_BONUS * (1 + G.loop);
        add_score(TUNNEL_ALL_BONUS * (1 + G.loop));
        sound_fx(FX_BONUS);
    }
}
