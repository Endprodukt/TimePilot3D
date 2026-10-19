/* boss.c - the five bosses, each a fight of its own.
 *
 * A boss comes in with a warning (the state BOSSINTRO, drawn in draw.c).
 * Its fight goes in stages. A stage has a goal - parts of the boss to
 * shoot off, or something else that has to go first - and when the goal
 * is reached the boss changes before the player's eyes (BOSSTURN: a short
 * film again, something comes out of it or it becomes something else) and
 * the next stage is another fight, with other weapons, asking for another
 * way to fly. With the last stage's goal reached it falls.
 *
 * What a boss is hit on is never just "its guns". The hull is armour;
 * only the stage's parts take harm, some only from one side:
 *
 *   1910 Zeppelin LZ 62
 *     1 it comes as a bomber: it hangs over the player and lets rows of
 *       bombs go. The open bay can only be hit from below: under it
 *       between two rows, and up.
 *     > the bay is wrecked; it turns to get away
 *     2 its four engines, hung out on its sides. It defends itself with a
 *       machine gun in the gondola and now and then a broadside. Every
 *       engine gone makes it slower, and with the last one it falls: an
 *       airship without engines does not stay up.
 *
 *   Engines are always the last thing a boss loses: without them it
 *   falls, so nothing can come after them.
 *
 *   1940 B-25 Mitchell
 *     1 a joust: a 75 mm cannon in its nose. It comes straight at the
 *       player, firing, goes past, turns and comes again. Its nose can
 *       only be hit from ahead.
 *     > the cannon is gone; it has its turrets left
 *     2 its two engines, out on the wings; the turrets on its back and in
 *       its tail follow the player with machine-gun fire. With the second
 *       engine it falls.
 *
 *   1970 CH-47 Chinook
 *     1 four gunships fly round it as its escort; as long as one of them
 *       is left nothing touches it.
 *     > rocket pods swing out on both sides
 *     2 the pods: salvos of four unguided rockets from each in turn, and
 *       a homing missile now and then.
 *     > it has nothing left
 *     3 it runs, weaving, with only its door gun: the hull is open.
 *
 *   1982 B-52 Stratofortress
 *     1 the laser turret on its back: it takes aim (a thin line, ahead
 *       of the plane - where it will be), holds (the line stands and
 *       blinks), fires (a beam). Whoever flies on straight is hit: turn
 *       when it holds. The turret can be hit from anywhere.
 *     > the turret is gone; it runs
 *     2 catch it: it is fast. Its four engine pods under the wings; the
 *       tail gun fires at whoever follows, and it leaves mines behind.
 *       Every pod gone makes it slower, and with the last it falls.
 *
 *   2001 Mothership
 *     1 it hangs over one place behind a shield no shot goes through.
 *       Three stations on the ground feed the shield, each with a beam up
 *       to it: down there, under its fire, they have to be destroyed.
 *     > the shield goes out, and the saucer unfolds into a robot
 *     2 the robot turns to face the player wherever it goes: rapid fire
 *       from both arms in turn, its two fists thrown as homing missiles,
 *       a laser from its chest. Only the light in its chest takes harm.
 *
 * The self test and the demo fly none of this as a player would: for them
 * a part can be hit from any side. */

#include "game_internal.h"

#define FROM_ANYWHERE 0, 0, 0

const struct boss bosses[ERA_COUNT] = {
    { "ZEPPELIN LZ 62", 1.35f, 2, {
        { "HIT THE BOMB BAY FROM BELOW", "BOMB BAY", NULL, 1, {
            { 0, -7.6f, -12.0f, 6.0f, 36, 0, -1, 0, LOOK_BAY } } },
        { "SHOOT OUT ITS ENGINES", "ENGINES", "BOMB BAY DESTROYED", 4, {
            { 8.5f, -3.0f, 9.0f, 3.2f, 6, FROM_ANYWHERE, LOOK_ENGINE }, { -8.5f, -3.0f, 9.0f, 3.2f, 6, FROM_ANYWHERE, LOOK_ENGINE },
            { 8.5f, -3.0f, -11.0f, 3.2f, 6, FROM_ANYWHERE, LOOK_ENGINE }, { -8.5f, -3.0f, -11.0f, 3.2f, 6, FROM_ANYWHERE, LOOK_ENGINE } } } },
      40.0f, 14.0f, 380.0f, 31.0f, 10.5f, 0, 0 },
    { "B-25 MITCHELL", 1.30f, 2, {
        { "HEAD ON - HIT ITS NOSE", "NOSE", NULL, 1, {
            { 0, 0, 9.5f, 4.5f, 14, 0, 0, 1, LOOK_CANNON } } },
        { "SET ITS ENGINES ON FIRE", "ENGINES", "CANNON DESTROYED", 2, {
            { 5.0f, 0, 3.5f, 2.6f, 9, FROM_ANYWHERE, LOOK_ENGINE }, { -5.0f, 0, 3.5f, 2.6f, 9, FROM_ANYWHERE, LOOK_ENGINE } } } },
      60.0f, 12.0f, 420.0f, 11.0f, 2.6f, 15.0f, 1.6f },
    { "CH-47 CHINOOK", 1.30f, 3, {
        { "SHOOT DOWN ITS ESCORT", "ESCORT", NULL, 0, { { 0, 0, 0, 0, 0, FROM_ANYWHERE, 0 } } },
        { "DESTROY BOTH ROCKET PODS", "ROCKET PODS", "ROCKET PODS", 2, {
            { 6.5f, -0.6f, 1.0f, 2.8f, 12, FROM_ANYWHERE, LOOK_POD }, { -6.5f, -0.6f, 1.0f, 2.8f, 12, FROM_ANYWHERE, LOOK_POD } } },
        { "IT RUNS - FINISH IT", "HULL", "DEFENCELESS", 1, {
            { 0, 0.2f, 0, 8.0f, 14, FROM_ANYWHERE, LOOK_CORE } } } },
      95.0f, 10.0f, 340.0f, 11.0f, 4.2f, 0, 0 },
    { "B-52 STRATOFORTRESS", 1.50f, 2, {
        { "SHOOT THE TURRET ON ITS BACK", "LASER", NULL, 1, {
            { 0, 3.8f, 6.0f, 6.0f, 14, FROM_ANYWHERE, LOOK_LASER } } },
        { "CATCH IT - SHOOT ITS ENGINES", "ENGINE PODS", "LASER DESTROYED", 4, {
            { 8.5f, -2.4f, 3.0f, 2.6f, 7, FROM_ANYWHERE, LOOK_ENGINE }, { -8.5f, -2.4f, 3.0f, 2.6f, 7, FROM_ANYWHERE, LOOK_ENGINE },
            { 17.0f, -2.4f, -2.5f, 2.6f, 7, FROM_ANYWHERE, LOOK_ENGINE }, { -17.0f, -2.4f, -2.5f, 2.6f, 7, FROM_ANYWHERE, LOOK_ENGINE } } } },
      80.0f, 16.0f, 460.0f, 22.0f, 2.6f, 26.0f, 2.2f },
    { "MOTHERSHIP", 1.50f, 4, {
        { "DESTROY THE GROUND STATIONS", "SHIELD", NULL, 3, {
            { 0, 0, 0, 7.0f, 8, FROM_ANYWHERE, LOOK_STATION }, { 0, 0, 0, 7.0f, 8, FROM_ANYWHERE, LOOK_STATION },
            { 0, 0, 0, 7.0f, 8, FROM_ANYWHERE, LOOK_STATION } } },
        { "SHOOT THE DOME ON TOP OF IT", "DOME", "SHIELD DOWN", 1, {
            { 0, 12.0f, 0, 6.5f, 24, FROM_ANYWHERE, LOOK_CORE } } },
        { "SHOOT OFF ITS ARMS", "ARMS", "TRANSFORMATION", 2, {
            { 12.2f, 1.0f, 6.0f, 6.0f, 14, FROM_ANYWHERE, LOOK_ARM }, { -12.2f, 0, 8.0f, 6.5f, 16, FROM_ANYWHERE, LOOK_ARM } } },
        { "NOW ITS HEAD", "HEAD", "ARMS DESTROYED", 1, {
            { 0, 20.0f, 0.5f, 4.5f, 22, 0, 0, 1, LOOK_HEAD } } } },
      110.0f, 10.0f, 360.0f, 0, 9.0f, 30.0f, 0 },
};

/* --- the stage, and what is left of it ------------------------------------------ */

const struct boss_stage *boss_stage(void) { return &bosses[G.era].stage[G.boss_stage]; }

/* Where part `i` of the stage is in the world. */
vp_vec3 boss_part_at(const obj *o, int i)
{
    const struct boss_part *p = &boss_stage()->part[i];
    vp_xf place;
    if (p->look == LOOK_STATION) return G.station[i];
    place = flyer_place(o);
    if (p->look == LOOK_ARM) return vp_xf_point(&place, robot_point(LIMB_ARM_R + i, vp_v3(p->x, p->y, p->z)));     /* where the arm is now */
    return vp_xf_point(&place, vp_v3(p->x, p->y, p->z));
}

/* --- the robot's limbs ------------------------------------------------------------
 *
 * A point of the robot's model as its limb holds it now: an arm turned
 * about its shoulder by how far it is raised (and by how far it has
 * tumbled, once it is off), a leg about its hip by where it is in its
 * step. In the model's own units; x right, y up, z ahead. */
vp_vec3 robot_point(int limb, vp_vec3 p)
{
    float py, a, dy, dz, c, n;
    if (limb == LIMB_ARM_R || limb == LIMB_ARM_L) {
        int side = limb - LIMB_ARM_R;
        py = 14.0f;
        a = G.arm_pitch[side] + G.arm_spin[side];
    } else if (limb == LIMB_LEG_R || limb == LIMB_LEG_L) {
        py = -6.0f;
        a = 0.42f * sinf(G.walk) * (limb == LIMB_LEG_R ? 1.0f : -1.0f);
    } else return p;
    dy = p.y - py; dz = p.z;
    c = cosf(a); n = sinf(a);
    return vp_v3(p.x, py + dy * c + dz * n, -dy * n + dz * c);
}

/* The same, in the world. */
static vp_vec3 robot_world(const obj *o, int limb, float x, float y, float z)
{
    vp_xf place = flyer_place(o);
    return vp_xf_point(&place, robot_point(limb, vp_v3(x, y, z)));
}

/* An arm that has been shot off: it comes away from its shoulder. */
static void arm_off(const obj *o, int side)
{
    vp_vec3 right = vp_v3(cosf(o->heading), 0, -sinf(o->heading));
    G.arm_state[side] = 1;
    G.arm_t[side] = 0;
    G.arm_off[side] = vp_v3(0, 0, 0);
    G.arm_vel[side] = vp_v3_add(vp_v3_scale(o->vel, 0.5f), vp_v3_add(vp_v3_scale(right, side ? -16.0f : 16.0f), vp_v3(0, 6.0f, 0)));
}

/* The limbs, each step: the arms that are on it follow the plane up and
 * down, slowly; one that is off falls, tumbling, and blows up when it
 * reaches the ground or after a second and a half; the legs walk as far
 * as the robot goes. */
static void robot_limbs(obj *o)
{
    const float size = bosses[G.era].size;
    const float *g;
    int n = model_segments(MODEL_ROBOT, &g), side, i;
    vp_xf place = flyer_place(o);

    G.walk += fabsf(o->speed) * DT * 0.11f;
    for (side = 0; side < 2; side++) {
        vp_vec3 shoulder = vp_xf_point(&place, vp_v3(side ? -12.0f : 12.0f, 14.0f, 0));
        if (G.arm_state[side] == 0) {
            vp_vec3 to = vp_v3_sub(G.p.pos, shoulder);
            float want = G.state == PLAYING && G.boss_stage == 2 ? clampf(atan2f(to.y, sqrtf(to.x * to.x + to.z * to.z)), -0.9f, 1.2f) : 0;
            G.arm_pitch[side] += (want - G.arm_pitch[side]) * 0.06f;
        } else if (G.arm_state[side] == 1) {
            vp_vec3 at;
            G.arm_vel[side].y -= 45.0f * DT;
            G.arm_off[side] = vp_v3_add(G.arm_off[side], vp_v3_scale(G.arm_vel[side], DT));
            G.arm_spin[side] += (side ? -2.6f : 2.6f) * DT;
            G.arm_t[side] += DT;
            at = vp_v3_add(vp_v3_add(shoulder, G.arm_off[side]), vp_v3(0, -8.0f * size, 0));
            if (G.frames % 3 == 0) burst(at, vp_v3(0, 0, 0), 1, 12.0f, 0.9f, (G.frames / 3) & 1 ? SPARK_FIRE : SPARK_SMOKE, 0);
            if (G.arm_t[side] > 1.5f || at.y < GROUND_Y + 4.0f) {
                /* it goes up, and its lines fly */
                for (i = 0; i < n; i++) {
                    const float *q = g + 6 * i;
                    int cr, cg, cb;
                    if (model_robot_limb(i) != LIMB_ARM_R + side || (i & 1)) continue;
                    model_colour(MODEL_ROBOT, model_segment_colour(MODEL_ROBOT, i), TINT_OWN, &cr, &cg, &cb);
                    debris_line(vp_v3_add(vp_xf_point(&place, robot_point(LIMB_ARM_R + side, vp_v3(q[0], q[1], q[2]))), G.arm_off[side]),
                                vp_v3_add(vp_xf_point(&place, robot_point(LIMB_ARM_R + side, vp_v3(q[3], q[4], q[5]))), G.arm_off[side]),
                                vp_v3((frand() - 0.5f) * 90.0f, 20.0f + frand() * 50.0f, (frand() - 0.5f) * 90.0f), 1.6f + frand(),
                                (cr << 16) | (cg << 8) | cb);
                }
                sphere_burst(at, vp_v3(0, 0, 0), 120.0f, 0.9f);
                pixel_burst(at, vp_v3(0, 0, 0), 1.3f);
                burst(at, vp_v3(0, 0, 0), 20, 80.0f, 1.0f, SPARK_FIRE, 1);
                burst(at, vp_v3(0, 0, 0), 8, 25.0f, 1.8f, SPARK_SMOKE, 1);
                if (at.y < GROUND_Y + 12.0f) shock_ring(vp_v3(at.x, GROUND_Y + 1.0f, at.z), 70.0f, 0.7f, 1);
                sound_at(FX_BIG_EXPLODE, at);
                G.p.shake = clampf(G.p.shake + 0.35f, 0, 1.2f);
                G.arm_state[side] = 2;
            }
        }
    }
}

static int escorts_left(void)
{
    int i, n = 0;
    for (i = 0; i < MAX_OBJ; i++) if (G.objs[i].kind == K_ENEMY && G.objs[i].escort) n++;
    return n;
}

/* What the stage's goal still takes: the hits its parts have left, or the
 * escort still flying. */
int boss_stage_left(void)
{
    int i, n = 0;
    if (boss_stage()->parts == 0) return escorts_left();
    for (i = 0; i < boss_stage()->parts; i++) n += G.part_hp[i];
    return n;
}

static int parts_alive(void)
{
    int i, n = 0;
    for (i = 0; i < boss_stage()->parts; i++) if (G.part_hp[i] > 0) n++;
    return n;
}

/* A stage begins: its parts whole, its clocks at their start, and what
 * the stage needs set up - the Chinook's escort, the shield's stations. */
static void stage_begin(obj *o, int k)
{
    const struct boss_stage *s;
    int i;

    G.boss_stage = k;
    s = boss_stage();
    G.boss_total = 0;
    for (i = 0; i < BOSS_PARTS; i++) {
        G.part_hp[i] = i < s->parts ? s->part[i].hp + (s->part[i].hp * G.loop) / 3 + (G.difficulty - DIFFICULTY_NORMAL) : 0;
        if (i < s->parts && G.part_hp[i] < 2) G.part_hp[i] = 2;
        if (i < s->parts && G.cheat_weak) G.part_hp[i] = 1;
        G.part_flash[i] = 0;
        G.boss_total += G.part_hp[i];
    }
    for (i = 0; i < 4; i++) { G.boss_t[i] = 2.0f + 1.5f * (float)i; G.boss_n[i] = 0; }
    if (G.era == 4) {
        /* the robot's arms: both on it when it has just become the robot;
         * off for good after that (one that is still falling falls on) */
        for (i = 0; i < 2; i++) {
            if (k <= 2) { G.arm_state[i] = 0; G.arm_pitch[i] = G.arm_spin[i] = 0; }
            else if (G.arm_state[i] == 0) G.arm_state[i] = 2;
        }
        if (k <= 2) { G.walk = 0; G.robot_landed = 0; }
    }
    G.boss_kind = 0;
    G.boss_stage_time = 0;
    G.laser_state = 0;
    G.laser_t = 2.5f;
    G.salvo_t = G.warn_t = 0;
    G.boss_anchor = o->pos;
    if (s->parts == 0) {                                /* the escort */
        for (i = 0; i < 4; i++) spawn_escort(o, i);
        G.boss_total = 4;
    }
    for (i = 0; i < s->parts; i++) {
        if (s->part[i].look == LOOK_STATION) {          /* round the place it hangs over, on the ground or on a roof */
            float a = PI2 * (float)i / (float)s->parts + 0.6f;
            vp_vec3 at = vp_v3(o->pos.x + sinf(a) * 250.0f, 0, o->pos.z + cosf(a) * 250.0f);
            at.y = city_top(G.era, at.x, at.z, 12.0f) + STATION_HEIGHT;
            G.station[i] = at;
        }
    }
}

/* A new boss: its first stage. With the testing aid that makes it fall to
 * one hit there is no stage to fight through. */
void boss_begin(obj *o, int one_hit)
{
    G.boss_one_hit = one_hit;
    G.boss_deploy = 1.0f;
    stage_begin(o, G.cheat_boss2 && !one_hit ? (G.cheat_boss2 < bosses[G.era].stages ? G.cheat_boss2 : bosses[G.era].stages - 1) : 0);
    if (G.cheat_boss2 && !one_hit && G.era == 4 && G.boss_stage >= 2) o->model = MODEL_ROBOT;
    if (G.debug_stage) { G.debug_stage = 0; G.cheat_boss2 = 0; }       /* the debug choice: this boss only */
}

/* The stage's goal is reached: the next stage, with its film before it -
 * or, after the last, the end of the boss. */
static void stage_done(obj *o, vp_vec3 at)
{
    shock_ring(o->pos, 150.0f, 0.7f, 0);
    sphere_burst(o->pos, o->vel, 90.0f, 0.9f);
    sound_fx(FX_BOSS);
    if (G.boss_stage + 1 >= bosses[G.era].stages) {
        o->hp = 1;
        boss_hit(o, at);
        return;
    }
    stage_begin(o, G.boss_stage + 1);
    /* what was in the air against the player is gone with the old stage */
    {
        int i;
        for (i = 0; i < MAX_OBJ; i++) {
            obj *x = &G.objs[i];
            if (x->kind == K_BULLET || x->kind == K_MISSILE || x->kind == K_BOMB) x->kind = K_NONE;
        }
    }
    if (G.p.alive && G.state == PLAYING) {
        G.boss_deploy = 0;
        G.p.stalled = G.p.pull_up = 0;
        G.p.hurt = 0;
        enter(BOSSTURN);
    } else {
        G.boss_deploy = 1.0f;
        if (G.era == 4 && G.boss_stage >= 2) o->model = MODEL_ROBOT;
        notice(boss_stage()->goal);
    }
}

/* During the film between two stages (BOSSTURN): what is coming out of
 * the boss comes out (boss_deploy, 0 to 1, drawn by draw.c); after its
 * second stage the mothership becomes the robot (below). */
static vp_vec3 local(const obj *o, float x, float y, float z);
static void boss_turning(obj *o, float t)
{
    const struct boss_stage *was = G.boss_stage > 0 ? &bosses[G.era].stage[G.boss_stage - 1] : NULL;
    G.boss_deploy = smooth((t - 0.4f) / 2.2f);
    if (was && was->parts > 0 && was->part[0].look == LOOK_BAY) {
        /* The bomb bay that has just been shot to pieces goes up: fires
         * run along it, each bigger than the last; then what was left in
         * it goes off at once, the two doors are torn away and fall, and
         * the frames of its floor after them. It burns on (update_boss). */
        const struct boss_part *p = &was->part[0];
        float w = p->r * 0.8f, l = p->r * 1.6f;
        if (t < BAY_BLOWS) {
            if (fmodf(t, 0.22f) < DT) {
                vp_vec3 at = local(o, p->x + (frand() - 0.5f) * w * 2.0f, p->y - 1.0f, p->z + (frand() - 0.5f) * l * 2.0f);
                float grow = 0.5f + t / BAY_BLOWS;
                burst(at, o->vel, 8, 45.0f * grow, 0.6f, SPARK_FIRE, 0);
                burst(at, o->vel, 3, 60.0f * grow, 0.35f, SPARK_WHITE, 0);
                burst(at, o->vel, 2, 14.0f, 1.3f, SPARK_SMOKE, 0);
                sound_at(FX_EXPLODE, at);
                o->flash = 4;
            }
        } else if (t - DT < BAY_BLOWS) {
            vp_vec3 at = local(o, p->x, p->y - 1.0f, p->z), down = vp_v3_add(vp_v3_scale(o->vel, 0.7f), vp_v3(0, -16.0f, 0));
            vp_vec3 right = vp_v3_sub(local(o, 1.0f, 0, 0), local(o, 0, 0, 0));
            int side, j;
            sphere_burst(at, o->vel, 110.0f, 0.9f);
            pixel_burst(at, o->vel, 1.3f);
            point_ring(at, o->vel, 17, 120.0f, 0.8f);
            burst(at, o->vel, 22, 90.0f, 1.1f, SPARK_FIRE, 0);
            burst(at, o->vel, 10, 30.0f, 1.8f, SPARK_SMOKE, 0);
            sound_at(FX_BIG_EXPLODE, at);
            o->flash = 24;
            G.p.shake = 0.5f;
            for (side = -1; side <= 1; side += 2) {
                /* a door: its four edges, blown out to its side and falling */
                float x0 = p->x + w * (float)side, x1 = x0 + p->r * 0.45f * (float)side, y1 = p->y - p->r * 0.9f;
                vp_vec3 c[4], push = vp_v3_add(down, vp_v3_scale(right, 26.0f * (float)side));
                c[0] = local(o, x0, p->y, p->z - l); c[1] = local(o, x0, p->y, p->z + l);
                c[2] = local(o, x1, y1, p->z + l);   c[3] = local(o, x1, y1, p->z - l);
                for (j = 0; j < 4; j++)
                    debris_line(c[j], c[(j + 1) % 4], vp_v3_add(push, vp_v3((frand() - 0.5f) * 8.0f, (frand() - 0.5f) * 8.0f, (frand() - 0.5f) * 8.0f)),
                                3.2f, 0xC8C8C8);
            }
            for (j = 0; j < 7; j++) {
                /* frames and bomb racks out of its floor */
                float z = p->z - l + 2.0f * l * (float)j / 6.0f;
                debris_line(local(o, p->x - w * frand(), p->y, z), local(o, p->x + w * frand(), p->y - 1.5f * frand(), z + (frand() - 0.5f) * 4.0f),
                            vp_v3_add(down, vp_v3((frand() - 0.5f) * 44.0f, -10.0f - frand() * 26.0f, (frand() - 0.5f) * 44.0f)),
                            2.2f + frand(), j & 1 ? 0xFF8C28 : 0xA0A0AA);
            }
        } else if (G.frames % 3 == 0) {
            vp_vec3 at = local(o, p->x + (frand() - 0.5f) * w, p->y - 1.0f, p->z + (frand() - 0.5f) * l * 1.6f);
            burst(at, o->vel, 2, 18.0f, 0.5f, SPARK_FIRE, 0);
            burst(at, o->vel, 1, 8.0f, 1.6f, SPARK_SMOKE, 0);
        }
    }
    if (was && was->parts > 0 && was->part[0].look == LOOK_LASER) {
        /* The laser turret that has just been shot to pieces: it arcs and
         * burns, then it goes up - its sides fly off, its barrel is
         * thrown high - and what is left of it burns on (update_boss). */
        const struct boss_part *p = &was->part[0];
        vp_vec3 at = local(o, p->x, p->y, p->z);
        if (t < BAY_BLOWS) {
            if (fmodf(t, 0.2f) < DT) {
                float grow = 0.5f + t / BAY_BLOWS;
                burst(at, o->vel, 7, 40.0f * grow, 0.5f, SPARK_WHITE, 0);
                burst(at, o->vel, 4, 30.0f * grow, 0.6f, SPARK_FIRE, 0);
                sound_at(FX_HIT_BOSS, at);
                o->flash = 3;
            }
        } else if (t - DT < BAY_BLOWS) {
            float r = p->r, w = r * 0.7f;
            int j;
            sphere_burst(at, o->vel, 100.0f, 0.9f);
            pixel_burst(at, o->vel, 1.2f);
            point_ring(at, o->vel, 17, 110.0f, 0.8f);
            burst(at, o->vel, 20, 80.0f, 1.0f, SPARK_FIRE, 0);
            burst(at, o->vel, 8, 25.0f, 1.8f, SPARK_SMOKE, 0);
            sound_at(FX_BIG_EXPLODE, at);
            o->flash = 24;
            G.p.shake = 0.5f;
            for (j = 0; j < 4; j++) {
                /* its four sides, each a corner and the top, blown outwards */
                float sx = (j & 1) ? w : -w, sz = (j & 2) ? w : -w;
                vp_vec3 out = vp_v3_sub(local(o, p->x + sx, p->y, p->z + sz), at);
                debris_line(local(o, p->x + sx, p->y - r, p->z + sz), local(o, p->x, p->y, p->z),
                            vp_v3_add(vp_v3_scale(o->vel, 0.7f), vp_v3_add(vp_v3_scale(out, 6.0f), vp_v3(0, 22.0f + frand() * 20.0f, 0))), 2.6f, 0xFF3C28);
                debris_line(local(o, p->x + sx, p->y - r, p->z + sz), local(o, p->x - sx, p->y - r, p->z + sz),
                            vp_v3_add(vp_v3_scale(o->vel, 0.7f), vp_v3_add(vp_v3_scale(out, 4.0f), vp_v3(0, 10.0f + frand() * 20.0f, 0))), 2.2f, 0xC8C8C8);
            }
            /* the barrel, end over end */
            debris_line(at, vp_v3_add(at, vp_v3_scale(forward_of(o), 13.0f)),
                        vp_v3_add(vp_v3_scale(o->vel, 0.8f), vp_v3(0, 55.0f, 0)), 3.2f, 0xFFFFFF);
        } else if (G.frames % 3 == 0) {
            burst(at, o->vel, 2, 16.0f, 0.5f, SPARK_FIRE, 0);
            burst(at, o->vel, 1, 8.0f, 1.6f, SPARK_SMOKE, 0);
        }
    }
    if (G.era == 4 && G.boss_stage == 2 && o->model != MODEL_ROBOT) {
        /* The mothership becomes the robot, and it is watched: it shakes
         * and sparks, then its lines leave the saucer and go to their
         * places in the robot - the legs let down first, the body, the
         * arms and shoulders, the head last (boss_morph; draw.c draws
         * it). When the last line is home it is the robot: a flash, a
         * ring, a bang. */
        float r = model_radius(MODEL_MOTHERSHIP) * bosses[G.era].size;
        G.boss_morph = clampf((t - MORPH_FROM) / MORPH_TIME, 0, 1);
        o->pitch = o->roll = 0;
        if (t < MORPH_FROM) {
            if (G.frames % 3 == 0)
                burst(vp_v3_add(o->pos, vp_v3((frand() - 0.5f) * r * 1.6f, (frand() - 0.5f) * 10.0f, (frand() - 0.5f) * r * 1.6f)),
                      o->vel, 3, 40.0f, 0.4f, SPARK_WHITE, 0);
            if (fmodf(t, 0.3f) < DT) sound_fx(FX_WARN);
            o->flash = ((G.frames / 3) & 1) ? 2 : 0;
        } else if (G.boss_morph < 1.0f) {
            if (G.frames % 4 == 0)
                burst(vp_v3_add(o->pos, vp_v3((frand() - 0.5f) * r, (frand() - 0.7f) * r * 1.2f, (frand() - 0.5f) * r)),
                      o->vel, 2, 30.0f, 0.5f, (G.frames / 4) & 1 ? SPARK_WHITE : SPARK_FIRE, 0);
            if (fmodf(t - MORPH_FROM, MORPH_TIME * 0.25f) < DT) sound_at(FX_HIT_BOSS, o->pos);    /* each part locks home */
        } else {
            sphere_burst(o->pos, o->vel, 150.0f, 1.0f);
            pixel_burst(o->pos, o->vel, 1.6f);
            point_ring(o->pos, o->vel, 19, 150.0f, 0.9f);
            shock_ring(o->pos, 90.0f, 0.7f, 0);
            sound_at(FX_BIG_EXPLODE, o->pos);
            G.p.shake = 0.6f;
            o->model = MODEL_ROBOT;
            o->flash = 30;
        }
    }
}

/* How long the film between two stages runs: the robot's is the long one. */
float boss_turn_time(void) { return G.era == 4 && G.boss_stage == 2 ? ROBOT_TURN_TIME : BOSS_TURN_TIME; }

/* --- hits ------------------------------------------------------------------------ */

static void part_hit(obj *o, int i, vp_vec3 at)
{
    static const char *const gone[] = { "ENGINE OUT", "BOMB BAY HIT", "CANNON OUT", "POD DESTROYED", "LASER OUT", "", "STATION DESTROYED", "ARM SHOT OFF", "" };
    G.part_flash[i] = 8;
    sound_at(FX_HIT_BOSS, at);
    burst(at, o->vel, 4, 30.0f, 0.4f, SPARK_FIRE, 0);
    burst(at, o->vel, 2, 45.0f, 0.3f, SPARK_WHITE, 0);
    if (--G.part_hp[i] > 0) return;

    burst(at, o->vel, 14, 70.0f, 0.9f, SPARK_FIRE, 0);
    burst(at, o->vel, 6, 30.0f, 1.4f, SPARK_SMOKE, 0);
    point_ring(at, o->vel, 13, 80.0f, 0.6f);
    pixel_burst(at, o->vel, 0.8f);
    sound_at(FX_EXPLODE, at);
    add_score(BOSS_PART_POINTS * (1 + G.loop));
    floating_score(at, BOSS_PART_POINTS * (1 + G.loop));
    o->flash = 10;
    if (boss_stage()->part[i].look == LOOK_ARM) arm_off(o, i);
    if (boss_stage_left() > 0) { notice(gone[boss_stage()->part[i].look]); return; }
    stage_done(o, at);
}

/* Can a shot flying along `dir` hit part `i`? Some parts are open to one
 * side only. Not asked of the self test and the demo. */
static int from_its_side(const obj *o, int i, vp_vec3 dir)
{
    const struct boss_part *p = &boss_stage()->part[i];
    vp_xf place;
    vp_vec3 open;
    if ((p->fx == 0 && p->fy == 0 && p->fz == 0) || G.selftest || G.demo) return 1;
    place = flyer_place(o);
    open = vp_v3_normalize(vp_v3_sub(vp_xf_point(&place, vp_v3(p->x + p->fx, p->y + p->fy, p->z + p->fz)),
                                     vp_xf_point(&place, vp_v3(p->x, p->y, p->z))));
    /* The B-25's nose (2026-10-17: head on only was hardly to be done, and
     * took ages) is hit from anywhere ahead of its wings - from the side
     * too - only not from behind; the others from within 75 degrees of
     * the way their part looks. */
    return vp_v3_dot(open, dir) < (p->look == LOOK_CANNON ? 0.15f : -0.25f);
}

/* A shot of the player's has flown from `from` to `to` in this step. Has
 * it hit the boss? Only a part of the stage is hurt by it; the hull (or
 * the shield) stops the shot and that is all. Gives back 1 when the shot
 * is spent. */
int boss_take_shot(obj *o, vp_vec3 from, vp_vec3 to)
{
    const struct boss *b = &bosses[G.era];
    const struct boss_stage *s = boss_stage();
    float r = model_radius(o->model) * b->size, hull;
    vp_vec3 dir = vp_v3_normalize(vp_v3_sub(to, from));
    int i, shield = s->parts > 0 && s->part[0].look == LOOK_STATION;

    if (G.boss_one_hit) {
        if (segment_distance(from, to, o->pos) > r * 0.85f + 1.5f) return 0;
        boss_hit(o, to);
        return 1;
    }
    for (i = 0; i < s->parts; i++) {
        vp_vec3 g;
        if (G.part_hp[i] <= 0) continue;
        g = boss_part_at(o, i);
        if (segment_distance(from, to, g) < s->part[i].r * b->size + 3.5f && from_its_side(o, i, dir)) { part_hit(o, i, g); return 1; }
    }
    /* the armour is its body, not the ball round all of it: a shot must
     * get to a part out on a wing; the shield is the ball, and more */
    hull = shield ? r * 1.2f : r * 0.35f;
    if (segment_distance(from, to, o->pos) > hull + 1.5f) return 0;
    burst(to, o->vel, 2, 20.0f, 0.3f, shield ? SPARK_WHITE : SPARK_SMOKE, 0);
    if (G.frames % 3 == 0) sound_fx(FX_TINK);
    return 1;
}

/* Something bigger than a shot has gone off at the boss - a homing
 * missile, the player's own plane: `hits` hits on the part nearest to it. */
void boss_take_blast(obj *o, vp_vec3 at, int hits)
{
    int k;
    if (G.boss_one_hit) { boss_hit(o, at); return; }
    for (k = 0; k < hits && o->kind == K_BOSS; k++) {
        int i, best = -1;
        float best_d = 1e9f;
        for (i = 0; i < boss_stage()->parts; i++) {
            float d;
            if (G.part_hp[i] <= 0 || boss_stage()->part[i].look == LOOK_STATION) continue;
            d = dist3(boss_part_at(o, i), at);
            if (d < best_d) { best_d = d; best = i; }
        }
        if (best < 0) return;
        part_hit(o, best, boss_part_at(o, best));
    }
}

/* Is `p` within `reach` metres of the boss's body? Its body as it is
 * drawn - a hull as long and as thick as the model's, wings, a disc for
 * the saucer, a standing figure for the robot - not the ball round all of
 * it: over and under an airship there is air. */
int boss_touches(const obj *o, vp_vec3 p, float reach)
{
    const struct boss *b = &bosses[G.era];
    vp_vec3 rel = vp_v3_scale(vp_v3_sub(p, o->pos), 1.0f / b->size);
    float r = reach / b->size, c = cosf(o->heading), n = sinf(o->heading);
    float lx = rel.x * c - rel.z * n, lz = rel.x * n + rel.z * c, ly = rel.y, k;

    if (o->model == MODEL_ROBOT) {
        k = clampf(ly, -28.0f, 23.0f);
        return sqrtf(lx * lx + (ly - k) * (ly - k) + lz * lz) < 13.0f + r;
    }
    if (b->hull_len <= 0) return fabsf(ly) < b->hull_r + r && sqrtf(lx * lx + lz * lz) < b->wing_span + r;
    k = clampf(lz, -b->hull_len, b->hull_len);
    if (sqrtf(lx * lx + ly * ly + (lz - k) * (lz - k)) < b->hull_r + r) return 1;
    if (b->wing_span > 0) {
        k = clampf(lx, -b->wing_span, b->wing_span);
        if (sqrtf((lx - k) * (lx - k) + ly * ly + lz * lz) < b->wing_r + r) return 1;
    }
    return 0;
}

/* The plane has touched it: it is thrown off, out of its body by the
 * shortest way and a few metres clear, so that touching is one knock
 * that is understood, not a grinding along its hull. */
void boss_push_off(const obj *o, vp_vec3 *p)
{
    vp_vec3 away = vp_v3_sub(*p, o->pos);
    int i;
    float d = vp_v3_length(away);
    if (d < 1.0f) away = vp_v3(0, -1.0f, 0); else away = vp_v3_scale(away, 1.0f / d);
    for (i = 0; i < 40 && boss_touches(o, *p, PLAYER_RADIUS + 3.0f); i++) *p = vp_v3_add(*p, vp_v3_scale(away, 1.5f));
}

/* Where the guns should be aimed at it: at the stage's live part nearest
 * the line the player's nose is on, or at its middle when it has none. */
vp_vec3 boss_aim_point(const obj *o)
{
    vp_vec3 fwd = player_forward(), best = o->pos;
    float best_dot = -2.0f;
    int i;
    for (i = 0; i < boss_stage()->parts; i++) {
        vp_vec3 g, to;
        float d, dot;
        if (G.part_hp[i] <= 0) continue;
        g = boss_part_at(o, i);
        to = vp_v3_sub(g, G.p.pos);
        d = vp_v3_length(to);
        dot = d > 1.0f ? vp_v3_dot(fwd, to) / d : 1.0f;
        if (dot > best_dot) { best_dot = dot; best = g; }
    }
    return best;
}

/* --- weapons --------------------------------------------------------------------- */

/* Where the player will be when something flying at `speed` from `at`
 * gets there, led by `how` (1 = all the way). */
static vp_vec3 lead(vp_vec3 at, float speed, float how)
{
    float t = dist3(at, G.p.pos) / speed * how;
    return vp_v3_add(G.p.pos, vp_v3_scale(player_velocity(), t));
}

static vp_vec3 aim_ball(vp_vec3 at) { return vp_v3_normalize(vp_v3_sub(lead(at, boss_shot_speed(era()->bullet_speed), 0.5f), at)); }

/* One volley of the big balls, from `at` along `f`. Each can be read as it
 * comes and has its way through. */
static void volley_now(int pattern, vp_vec3 at, vp_vec3 f, float speed)
{
    vp_vec3 r = vp_v3_normalize(vp_v3_cross(fabsf(f.y) > 0.97f ? vp_v3(1, 0, 0) : vp_v3(0, 1, 0), f));
    vp_vec3 u = vp_v3_cross(f, r);
    int k;

    switch (pattern) {
    case BP_FAN:                /* five side by side: over it or under it */
        for (k = -2; k <= 2; k++) boss_shoot(at, vp_v3_normalize(vp_v3_add(f, vp_v3_scale(r, 0.17f * (float)k))), speed);
        break;
    case BP_LADDER:             /* five one over the other: round it to either side */
        for (k = -2; k <= 2; k++) boss_shoot(at, vp_v3_normalize(vp_v3_add(f, vp_v3_scale(u, 0.15f * (float)k))), speed);
        break;
    case BP_BROADSIDE:          /* seven in a wide flat row: over it or under it, there is no way round */
        for (k = -3; k <= 3; k++) boss_shoot(at, vp_v3_normalize(vp_v3_add(f, vp_v3_scale(r, 0.2f * (float)k))), speed);
        break;
    case BP_TWO_RINGS:          /* a small ring in a big one: through the middle, or between the two */
        for (k = 0; k < 6; k++) {
            float a = PI2 * (float)k / 6.0f;
            boss_shoot(at, vp_v3_normalize(vp_v3_add(f, vp_v3_add(vp_v3_scale(r, 0.09f * cosf(a)), vp_v3_scale(u, 0.09f * sinf(a))))), speed);
        }
        for (k = 0; k < 8; k++) {
            float a = PI2 * ((float)k + 0.5f) / 8.0f;
            boss_shoot(at, vp_v3_normalize(vp_v3_add(f, vp_v3_add(vp_v3_scale(r, 0.24f * cosf(a)), vp_v3_scale(u, 0.24f * sinf(a))))), speed);
        }
        break;
    default:                    /* the ones that come shot by shot: BP_SCISSORS, BP_PINWHEEL (volley_goes_on) */
        G.boss_n[1] = pattern == BP_PINWHEEL ? 8 : 5;
        G.boss_kind = pattern;
        G.boss_t[1] = 0;
        break;
    }
}

/* The boss this step is about (update_boss): a volley asked for is kept
 * by where its muzzle is from the boss's middle. */
static const obj *s_boss;

/* A volley is asked for. It does not go at once: its muzzle blinks for
 * SALVO_WARN seconds first, with the warning sound (update_boss lets it
 * go, aimed anew then) - nothing comes out of nowhere. */
static void volley(int pattern, vp_vec3 at, vp_vec3 f, float speed)
{
    (void)f;
    if (G.salvo_t > 0 || !s_boss) return;
    if (dist3(at, G.p.pos) < 180.0f) return;         /* too near to get out of its way: it holds its fire */
    G.salvo_pattern = pattern;
    G.salvo_speed = speed;
    G.salvo_off = vp_v3_sub(at, s_boss->pos);
    G.salvo_t = SALVO_WARN;
    sound_fx(FX_WARN);
}

/* Every boss has a side it cannot shoot to - nobody is told. A direction
 * from it, in its own frame (x right, y up, z ahead), and the cosine of
 * the half angle round it; the last line is the robot's. A plane in
 * there is not aimed at: no burst and no volley starts, the laser lets
 * go, no missile is sent. What is in the air already still flies, and the
 * boss turns - staying in there takes flying. */
static const struct { float x, y, z, cosine; } blind_side[ERA_COUNT + 1] = {
    { 0, 0, -1.0f, 0.90f },             /* the airship: dead astern, behind its fins */
    { 0, -1.0f, 0, 0.85f },             /* the B-25: straight under its belly */
    { 0, 0.5f, -0.866f, 0.87f },        /* the helicopter: behind and above, over its tail rotor */
    { 0, 0, 1.0f, 0.90f },              /* the B-52: dead ahead, its guns are in its tail */
    { 0, -1.0f, 0, 0.82f },             /* the saucer: straight under it */
    { 0, 0, -1.0f, 0.80f },             /* the robot: behind its back */
};

static int in_blind_side(const obj *o)
{
    int which = o->model == MODEL_ROBOT ? ERA_COUNT : G.era;
    vp_vec3 rel = vp_v3_sub(G.p.pos, o->pos);
    float c = cosf(o->heading), n = sinf(o->heading), d = vp_v3_length(rel);
    float lx = rel.x * c - rel.z * n, lz = rel.x * n + rel.z * c;
    if (d < 1.0f) return 0;
    return (lx * blind_side[which].x + rel.y * blind_side[which].y + lz * blind_side[which].z) / d > blind_side[which].cosine;
}

/* A volley that comes shot by shot, going on: the scissors' blades close
 * on the player pair by pair, the pinwheel goes round a circle. */
static void volley_goes_on(vp_vec3 at, float speed)
{
    vp_vec3 f, r, u;
    int shots = G.boss_kind == BP_PINWHEEL ? 8 : 5, k;
    if (G.boss_n[1] <= 0 || (G.boss_t[1] -= DT) > 0) return;
    f = aim_ball(at);
    r = vp_v3_normalize(vp_v3_cross(fabsf(f.y) > 0.97f ? vp_v3(1, 0, 0) : vp_v3(0, 1, 0), f));
    u = vp_v3_cross(f, r);
    k = shots - G.boss_n[1];
    if (G.boss_kind == BP_PINWHEEL) {
        float a = PI2 * (float)k / 8.0f;
        boss_shoot(at, vp_v3_normalize(vp_v3_add(f, vp_v3_add(vp_v3_scale(r, 0.15f * cosf(a)), vp_v3_scale(u, 0.15f * sinf(a))))), speed);
        G.boss_t[1] = 0.16f;
    } else {
        float open = 0.26f - 0.06f * (float)k;
        boss_shoot(at, vp_v3_normalize(vp_v3_add(f, vp_v3_scale(r, open))), speed);
        boss_shoot(at, vp_v3_normalize(vp_v3_add(f, vp_v3_scale(r, -open))), speed);
        G.boss_t[1] = 0.28f;
    }
    G.boss_n[1]--;
}

/* A machine gun at `at`: bursts of `shots`, `pause` seconds apart. A
 * burst walks onto the player: its first rounds go where the plane is,
 * which is behind a plane that flies, and each next one is led a little
 * more, so the streaks are seen coming up from astern - whoever turns
 * while they come is not where the last ones go. The clock is boss_t[0]. */
static void machine_gun(vp_vec3 at, int shots, float pause)
{
    const struct era *e = era();
    if (G.boss_n[0] == shots && G.boss_t[0] > 0) { G.warn_at = at; G.warn_t = G.boss_t[0]; }      /* its burst is announced */
    if (G.boss_n[0] > 0) {
        if ((G.boss_t[0] -= DT) <= 0) {
            float how = shots > 1 ? 0.95f * (float)(shots - G.boss_n[0]) / (float)(shots - 1) : 0.6f;
            vp_vec3 to = vp_v3_sub(lead(at, boss_mg_speed(e->bullet_speed), how), at);
            float d = vp_v3_length(to), off = d * 0.02f;
            to = vp_v3_add(to, vp_v3((frand() - 0.5f) * off, (frand() - 0.5f) * off, (frand() - 0.5f) * off));
            boss_mg(at, vp_v3_normalize(to), e->bullet_speed);
            G.boss_n[0]--;
            G.boss_t[0] = G.boss_n[0] > 0 ? 0.11f : pause / hard()->fire;
        }
    } else if ((G.boss_t[0] -= DT) <= 0) {
        if (dist3(at, G.p.pos) < MG_NEAREST) { G.boss_t[0] = 0.4f; return; }      /* not at point blank */
        G.boss_n[0] = shots;
        G.boss_t[0] = MG_WARN;              /* its muzzle blinks first */
        sound_fx(FX_WARN);
    }
}

/* The laser, from `from`: it rests, takes aim (its line follows the
 * player), holds (the line stands and blinks - get off it), fires (a beam
 * down that line, for half a second), and rests again for `rest` seconds.
 * Drawn by draw.c from laser_state, laser_from and laser_dir. */
static float s_laser_damage = DMG_LASER;

static void laser(vp_vec3 from, float rest)
{
    /* How the plane's way is bending, for where it will be: its velocity
     * now against a step ago, smoothed. */
    static vp_vec3 was, bend;
    vp_vec3 v = player_velocity();
    vp_vec3 now_bend = vp_v3_scale(vp_v3_sub(v, was), 1.0f / DT);
    float bl = vp_v3_length(now_bend);
    if (bl > 130.0f) now_bend = vp_v3_scale(now_bend, 130.0f / bl);       /* a new plane, a jolt: not a turn */
    bend = vp_v3_add(bend, vp_v3_scale(vp_v3_sub(now_bend, bend), 0.15f));
    was = v;

    G.laser_from = from;
    G.laser_t -= DT;
    switch (G.laser_state) {
    case 0:
        if (G.laser_t <= 0) { G.laser_state = 1; G.laser_t = LASER_AIM; G.laser_dir = vp_v3_normalize(vp_v3_sub(G.p.pos, from)); }
        break;
    case 1: {                   /* taking aim: its line is on the plane, and stays on it */
        vp_vec3 want = vp_v3_normalize(vp_v3_sub(G.p.pos, from));
        G.laser_dir = vp_v3_normalize(vp_v3_add(G.laser_dir, vp_v3_scale(vp_v3_sub(want, G.laser_dir), 0.5f)));
        if (G.laser_t <= 0) {
            /* Locked. The line goes to where the plane will be when the
             * beam comes if it flies on as it is flying - straight, or
             * round the turn it is in - and stands there. Whoever does
             * nothing is hit; a hard pull another way gets out. */
            float t = LASER_HOLD + LASER_BEAM * 0.3f;
            vp_vec3 there = vp_v3_add(G.p.pos, vp_v3_add(vp_v3_scale(v, t), vp_v3_scale(bend, 0.5f * t * t)));
            G.laser_dir = vp_v3_normalize(vp_v3_sub(there, from));
            G.laser_state = 2;
            G.laser_t = LASER_HOLD;
            sound_fx(FX_WARN);
        }
        break;
    }
    case 2:                     /* locked: it says so (the HUD), and waits */
        if (fmodf(G.laser_t, 0.25f) < DT) sound_fx(FX_WARN);
        if (G.laser_t <= 0) { G.laser_state = 3; G.laser_t = LASER_BEAM; G.laser_hit = 0; sound_fx(FX_MISSILE); }
        break;
    default: {                  /* the beam */
        vp_vec3 to = vp_v3_sub(G.p.pos, from);
        float along = vp_v3_dot(to, G.laser_dir);
        if (!G.laser_hit && along > 0 && along < LASER_REACH
            && vp_v3_length(vp_v3_sub(to, vp_v3_scale(G.laser_dir, along))) < LASER_WIDTH) {
            G.laser_hit = 1;
            damage_player(s_laser_damage);
            sound_fx(FX_EXPLODE);
        }
        if (G.laser_t <= 0) { G.laser_state = 0; G.laser_t = rest / hard()->fire; }
        break;
    }
    }
}

/* --- each boss, each stage: where it wants to fly, and what it fires ----------- */

struct fly { vp_vec3 target; float speed, turn; };

static vp_vec3 orbit(const obj *o, float radius, float bob, float rate)
{
    float a = o->age * rate;
    return vp_v3(G.p.pos.x + sinf(a) * radius, G.p.pos.y + 50.0f + bob, G.p.pos.z + cosf(a) * radius);
}

static vp_vec3 local(const obj *o, float x, float y, float z)
{
    vp_xf place = flyer_place(o);
    return vp_xf_point(&place, vp_v3(x, y, z));
}

static void zeppelin(obj *o, int armed, struct fly *f)
{
    const struct era *e = era();
    if (G.boss_stage == 0) {
        /* the bomber: over the player, as well as an airship can; rows of bombs, three and a breath */
        float dx = G.p.pos.x - o->pos.x, dz = G.p.pos.z - o->pos.z;
        f->target = vp_v3(G.p.pos.x, G.p.pos.y + 150.0f, G.p.pos.z);
        f->speed *= 1.25f;
        if (!armed) return;
        /* and its gondola's gun, at whoever comes for it (not at point blank: machine_gun) */
        machine_gun(local(o, 0, -10.0f, 1.0f), 8, 3.0f);
        /* Its bombs are its weapon here, and nothing beside them: three
         * to a row, 30 m apart along it - gaps a plane goes through - and
         * every other row shifted by half of that. They fall straight.
         * The bay blinks white before a row goes (draw.c, boss_t[2]). */
        if ((G.boss_t[2] -= DT) <= 0) {
            if (dx * dx + dz * dz < 300.0f * 300.0f && o->pos.y > G.p.pos.y - 20.0f) {
                /* They go down in lines along it, and the lanes between
                 * them change row by row: one row leaves the lane right
                 * under the keel free (lines left and right of it), the
                 * next fills it and leaves two lanes beside it free. Who
                 * has seen it moves with them - the way in to the bay. */
                vp_vec3 fwd = forward_of(o), right = vp_v3(cosf(o->heading), 0, -sinf(o->heading));
                float shift = (G.boss_n[2] & 1) ? 15.0f : 0;
                static const float keel_free[2] = { -26.0f, 26.0f }, keel_bombed[3] = { -52.0f, 0.0f, 52.0f };
                const float *lines = (G.boss_n[2] & 1) ? keel_bombed : keel_free;
                int k, j, count = (G.boss_n[2] & 1) ? 3 : 2;
                for (j = 0; j < count; j++)
                    for (k = -1; k <= 1; k++)
                        boss_bomb(vp_v3_add(vp_v3_add(local(o, 0, -8.0f, -12.0f), vp_v3_scale(fwd, 30.0f * (float)k + shift)),
                                            vp_v3_scale(right, lines[j])),
                                  vp_v3_add(vp_v3_scale(o->vel, 0.6f), vp_v3(0, -6.0f, 0)));
                sound_fx(FX_WARN);
                G.boss_n[2]++;
            }
            G.boss_t[2] = (G.boss_n[2] % 3 == 0 ? 4.5f : 2.2f) / hard()->fire;
        }
    } else {
        /* its bay wrecked, it fights for its engines: slower with every one of them gone */
        f->speed *= 0.55f + 0.45f * (float)parts_alive() / 4.0f;
        if (!armed) return;
        machine_gun(local(o, 0, -10.0f, 1.0f), 8, 2.4f);
        if ((G.boss_t[2] -= DT) <= 0) {
            vp_vec3 at = local(o, 0, -4.0f, 0);
            volley(BP_BROADSIDE, at, aim_ball(at), e->bullet_speed);
            G.boss_t[2] = 5.5f / hard()->fire;
        }
    }
}

static void b25(obj *o, float d, int armed, struct fly *f)
{
    const struct era *e = era();
    vp_vec3 fwd = forward_of(o), to = vp_v3_scale(vp_v3_sub(G.p.pos, o->pos), d > 1.0f ? 1.0f / d : 0);
    int behind = vp_v3_dot(to, fwd) < 0;
    if (G.boss_stage == 1) {
        f->speed *= 1.15f * (0.7f + 0.3f * (float)parts_alive() / 2.0f);
        if (!armed) return;
        machine_gun(behind ? local(o, 0, 0.4f, -11.0f) : local(o, 0, 1.8f, -2.0f), 6, 1.7f);    /* the tail gunner, or the turret on its back */
    } else {
        /* the joust: at the player, past it, on for three seconds, round and at it again */
        if (G.boss_n[2] == 0) {
            f->target = lead(o->pos, o->speed + 1.0f, 0.5f);
            f->turn = 0.65f;
            f->speed *= 1.2f;                       /* not so fast at it: the nose is in front longer */
            if (d < 140.0f) { G.boss_n[2] = 1; G.boss_t[2] = 2.4f; }      /* and round again sooner */
        } else {
            f->target = vp_v3_add(o->pos, vp_v3_scale(fwd, 600.0f));
            f->target.y = G.p.pos.y + 40.0f;
            f->speed *= 1.35f;
            if ((G.boss_t[2] -= DT) <= 0) G.boss_n[2] = 0;
        }
        if (!armed) return;
        if (G.boss_n[2] == 0 && vp_v3_dot(to, fwd) > 0.88f && d > 170.0f && d < 640.0f) {
            vp_vec3 at = local(o, 0, 0, 13.0f);
            volley_goes_on(at, e->bullet_speed * 1.3f);
            if (G.boss_n[1] <= 0 && (G.boss_t[3] -= DT) <= 0) {            /* the cannon: scissors, then a fan, in turn */
                volley((G.boss_n[3]++ & 1) ? BP_FAN : BP_SCISSORS, at, aim_ball(at), e->bullet_speed * 1.3f);
                G.boss_t[3] = 1.6f / hard()->fire;
            }
        } else if (behind && G.boss_n[2] == 1) {
            G.boss_n[1] = 0;
        }
        /* past the plane, the turret on its back fires after it */
        if (behind) machine_gun(local(o, 0, 1.8f, -2.0f), 6, 1.8f);
    }
}

static void chinook(obj *o, float d, int armed, struct fly *f)
{
    const struct era *e = era();
    vp_vec3 right = vp_v3(cosf(o->heading), 0, -sinf(o->heading));
    float side = vp_v3_dot(vp_v3_sub(G.p.pos, o->pos), right) > 0 ? 1.0f : -1.0f;
    if (G.boss_stage == 0) {
        /* behind its escort: rings and ladders from the door that faces the player */
        if (G.state == PLAYING && o->age > 1.5f && escorts_left() == 0) { stage_done(o, o->pos); return; }
        if (!armed) return;
        if ((G.boss_t[2] -= DT) <= 0) {
            vp_vec3 at = local(o, 2.4f * side, 0, 2.0f);
            volley((G.boss_n[2]++ & 1) ? BP_LADDER : BP_TWO_RINGS, at, aim_ball(at), e->bullet_speed);
            G.boss_t[2] = 3.4f / hard()->fire;
        }
    } else if (G.boss_stage == 1) {
        /* the pods, in turn: four rockets each, straight where the player will be */
        if (!armed) return;
        if (G.boss_n[0] > 0) {
            if ((G.boss_t[0] -= DT) <= 0) {
                int i = G.part_hp[G.boss_n[2] & 1] > 0 ? G.boss_n[2] & 1 : 1 - (G.boss_n[2] & 1);
                vp_vec3 at = boss_part_at(o, i);
                boss_rocket(at, vp_v3_normalize(vp_v3_sub(lead(at, boss_rocket_speed(e->bullet_speed), 0.9f), at)), e->bullet_speed);
                G.boss_n[0]--;
                G.boss_t[0] = G.boss_n[0] > 0 ? 0.16f : 2.6f / hard()->fire;
                if (G.boss_n[0] == 0) G.boss_n[2]++;
            }
        } else if ((G.boss_t[0] -= DT) <= 0) {
            G.boss_n[0] = 4;
        }
        if ((G.boss_t[1] -= DT) <= 0) { boss_missile_if_allowed(o); G.boss_t[1] = 9.0f / hard()->fire; }
    } else {
        /* nothing left but a door gun: away from the player, weaving */
        f->target = vp_v3(o->pos.x + (o->pos.x - G.p.pos.x) + 300.0f * sinf(o->age * 0.7f), G.p.pos.y + 30.0f * sinf(o->age * 0.9f),
                          o->pos.z + (o->pos.z - G.p.pos.z) + 300.0f * cosf(o->age * 0.5f));
        if (d > 620.0f) f->target = G.p.pos;            /* not out of the fight: round it comes */
        f->speed *= 1.5f;
        f->turn = 0.55f;
        if (armed) machine_gun(local(o, 2.4f * side, 0, 2.0f), 5, 2.0f);
    }
}

static void b52(obj *o, float d, int armed, struct fly *f)
{
    vp_vec3 fwd = forward_of(o), to = vp_v3_scale(vp_v3_sub(G.p.pos, o->pos), d > 1.0f ? 1.0f / d : 0);
    if (G.boss_stage == 1) {
        /* long fast legs, slower with every pod; its tail gun, and mines in its wake */
        f->target = orbit(o, 560.0f, 0, 0.13f);
        f->speed *= 1.3f * (0.6f + 0.4f * (float)parts_alive() / 4.0f);
        f->turn = 0.3f;
        if (!armed) return;
        if (vp_v3_dot(to, fwd) < -0.2f && d < 580.0f) machine_gun(local(o, 0, 0.6f, -22.0f), 7, 1.5f);
        if ((G.boss_t[2] -= DT) <= 0) {
            boss_ball(local(o, 0, 0, -20.0f), vp_v3((frand() - 0.5f) * 10.0f, -4.0f + (frand() - 0.5f) * 8.0f, (frand() - 0.5f) * 10.0f), 7.0f);
            G.boss_t[2] = 1.3f / hard()->fire;
        }
    } else {
        /* the laser: it circles the plane slowly and a little under it, so
         * that its back with the turret is turned up to the plane and can
         * be got at (2026-10-17: it could hardly be hit) */
        f->speed *= 0.6f;
        f->target.y = G.p.pos.y - 35.0f;
        if (!armed) { G.laser_state = 0; return; }
        laser(boss_part_at(o, 0), 2.6f);
        /* and its guns: the tail gun at whoever is behind it, the turret
         * under its belly at everybody else; from under its wings a
         * ladder and a fan of the big balls in turn */
        machine_gun(vp_v3_dot(to, fwd) < -0.2f ? local(o, 0, 0.6f, -22.0f) : local(o, 0, -2.8f, 5.0f), 7, 2.0f);
        if ((G.boss_t[2] -= DT) <= 0) {
            vp_vec3 at = local(o, (G.boss_n[2] & 1) ? 9.0f : -9.0f, -2.0f, 3.0f);
            volley((G.boss_n[2]++ & 1) ? BP_FAN : BP_LADDER, at, aim_ball(at), era()->bullet_speed);
            G.boss_t[2] = 5.0f / hard()->fire;
        }
    }
}

static void mothership(obj *o, float d, int armed, struct fly *f)
{
    const struct era *e = era();
    if (G.boss_stage == 0) {
        /* over its place, behind the shield: fire down at whoever goes for the stations */
        f->target = vp_v3(G.boss_anchor.x + 40.0f * sinf(o->age * 0.3f), 330.0f, G.boss_anchor.z + 40.0f * cosf(o->age * 0.3f));
        f->speed *= 0.5f;
        if (!armed) return;
        machine_gun(local(o, 0, -6.0f, 0), 8, 2.0f);
        volley_goes_on(o->pos, e->bullet_speed);
        if (G.boss_n[1] <= 0 && (G.boss_t[2] -= DT) <= 0) {
            volley(BP_PINWHEEL, o->pos, aim_ball(o->pos), e->bullet_speed);
            G.boss_t[2] = 6.0f / hard()->fire;
        }
    } else if (G.boss_stage == 1) {
        /* Its shield gone, it comes for the player itself: round the
         * plane in a tight ring, fast, rising and sinking; two rings and
         * a pinwheel in turn, its gun between them. The dome on top of
         * it is what it can be hurt at. */
        f->target = orbit(o, 300.0f, 70.0f * sinf(o->age * 0.7f), 0.3f);
        f->speed *= 1.35f;
        f->turn = 0.8f;
        if (!armed) return;
        machine_gun(local(o, 0, -6.0f, 0), 6, 2.6f);
        volley_goes_on(o->pos, e->bullet_speed);
        if (G.boss_n[1] <= 0 && (G.boss_t[2] -= DT) <= 0) {
            volley((G.boss_n[2]++ & 1) ? BP_TWO_RINGS : BP_PINWHEEL, o->pos, aim_ball(o->pos), e->bullet_speed);
            G.boss_t[2] = 4.5f / hard()->fire;
        }
    } else {
        /* The robot. It has come down and walks at the plane, slowly, on
         * the ground, turning as slowly; whoever stays close is trodden
         * towards, whoever runs is followed. */
        const float feet = 28.0f * bosses[G.era].size;
        f->target = vp_v3(G.p.pos.x, GROUND_Y + feet, G.p.pos.z);
        f->turn = 0.45f;
        f->speed = d > 150.0f ? 13.0f : 0.0f;
        s_laser_damage = DMG_HEAD_LASER;
        if (!armed) { G.laser_state = 0; return; }
        if (G.boss_stage == 2) {
            /* Its arms, aimed at the plane: the small gun in the right
             * one in bursts, the big one in the left a single heavy ball
             * now and then, announced at its mouth. Out of its head, once
             * in a long while, the laser - the worst it has. */
            if (G.part_hp[0] > 0) machine_gun(robot_world(o, LIMB_ARM_R, 12.2f, 1.0f, 14.0f), 8, 2.4f);
            if (G.part_hp[1] > 0) {
                vp_vec3 at = robot_world(o, LIMB_ARM_L, -12.3f, -4.0f, 20.5f);
                if (G.boss_n[2] && G.boss_t[2] > 0) { G.warn_at = at; G.warn_t = G.boss_t[2]; }
                if ((G.boss_t[2] -= DT) <= 0) {
                    if (!G.boss_n[2]) {
                        if (dist3(at, G.p.pos) > 180.0f) { G.boss_n[2] = 1; G.boss_t[2] = SALVO_WARN; sound_fx(FX_WARN); }
                        else G.boss_t[2] = 0.5f;
                    } else {
                        boss_shoot(at, aim_ball(at), e->bullet_speed * 1.2f);
                        sound_at(FX_EXPLODE, at);
                        G.boss_n[2] = 0;
                        G.boss_t[2] = 3.4f / hard()->fire;
                    }
                }
            }
            laser(local(o, 0, 20.0f, 3.5f), 11.0f);
        } else {
            /* Without arms: the laser far more often, and now and then two missiles out of the pack on its back. */
            laser(local(o, 0, 20.0f, 3.5f), 4.5f);
            if ((G.boss_t[2] -= DT) <= 0) {
                boss_missile_from(local(o, 5.0f, 25.0f, -9.0f), o->heading, 0.6f);
                boss_missile_from(local(o, -5.0f, 25.0f, -9.0f), o->heading, 0.6f);
                G.boss_t[2] = 9.0f / hard()->fire;
            }
        }
    }
}

/* The boss, each step: where it flies, and what it fires. */
void update_boss(obj *o)
{
    const struct era *e = era();
    const struct boss *b = &bosses[G.era];
    float d = dist3(o->pos, G.p.pos);
    float bob = b->swing * sinf(o->age * PI2 / b->swing_period);
    struct fly f;
    int i, armed = player_targetable() && d < 650.0f && !G.boss_one_hit;

    o->age += DT;
    s_boss = o;
    G.warn_t = 0;
    s_laser_damage = DMG_LASER;
    if (o->model == MODEL_ROBOT) robot_limbs(o);
    if (armed && in_blind_side(o)) armed = 0;
    if (G.state == PLAYING) G.boss_stage_time += DT;
    {
        /* Every boss fights in a rhythm that can be learnt: so long at it,
         * then so long quiet, the same all the stage through. In the quiet
         * it starts nothing - steam goes up from it while it lasts - and
         * that is the moment to go in. */
        static const float rhythm[ERA_COUNT + 1][2] = {
            { 7.0f, 4.5f },             /* the airship */
            { 8.0f, 3.5f },             /* the B-25 */
            { 7.5f, 4.0f },             /* the helicopter */
            { 9.0f, 4.0f },             /* the B-52 */
            { 8.0f, 4.0f },             /* the saucer */
            { 9.0f, 4.5f },             /* the robot */
        };
        int which = o->model == MODEL_ROBOT ? ERA_COUNT : G.era;
        float fire = rhythm[which][0] / hard()->fire, rest = rhythm[which][1];
        if (G.state == PLAYING && fmodf(G.boss_stage_time, fire + rest) >= fire) {
            armed = 0;
            if (G.frames % 4 == 0)
                burst(local(o, (frand() - 0.5f) * 8.0f, o->model == MODEL_ROBOT ? 22.0f : 7.0f, (frand() - 0.5f) * 10.0f),
                      vp_v3(0, 14.0f, 0), 1, 6.0f, 1.0f, SPARK_WHITE, 0);
        }
    }
    /* The self test is no player: its hands cannot do what some stages
     * ask (under a bomb bay, down to a station). So that it still goes
     * through every stage, a stage it has not got through in a minute
     * and a half wears off by itself - a hit every half second, or one
     * of the escort leaving. */
    if (G.selftest && G.state == PLAYING && G.boss_stage_time > 90.0f && G.frames % 30 == 0) {
        if (boss_stage()->parts == 0) {
            for (i = 0; i < MAX_OBJ; i++) if (G.objs[i].kind == K_ENEMY && G.objs[i].escort) { G.objs[i].escort = 0; break; }
        } else {
            for (i = 0; i < boss_stage()->parts; i++) if (G.part_hp[i] > 0) { part_hit(o, i, boss_part_at(o, i)); break; }
            if (o->kind != K_BOSS) return;
        }
    }
    f.target = orbit(o, b->orbit, bob, 0.22f);
    f.speed = e->boss_speed;
    f.turn = 0.4f;
    if (G.state == BOSSTURN) boss_turning(o, G.state_time);
    switch (G.era) {
    case 0:  zeppelin(o, armed, &f);      break;
    case 1:  b25(o, d, armed, &f);        break;
    case 2:  chinook(o, d, armed, &f);    break;
    case 3:  b52(o, d, armed, &f);        break;
    default: mothership(o, d, armed, &f); break;
    }
    if (o->kind != K_BOSS) return;                       /* it fell in this step */
    if (G.salvo_t > 0) {
        /* a volley that has been announced: it goes when its time is up */
        vp_vec3 at = vp_v3_add(o->pos, G.salvo_off);
        if (!armed || G.state != PLAYING) G.salvo_t = 0;
        else {
            G.warn_at = at;
            G.warn_t = G.salvo_t;
            if ((G.salvo_t -= DT) <= 0) volley_now(G.salvo_pattern, at, aim_ball(at), G.salvo_speed);
        }
    }
    if (o->model != MODEL_ROBOT) f.target.y = clampf(f.target.y, 170.0f, 680.0f);       /* the robot walks on the ground */
    if (G.state == BOSSINTRO || G.state == BOSSTURN) {
        /* in its film: straight and level, to be looked at */
        o->pitch *= 0.95f;
    } else {
        steer_toward(o, f.target, f.turn, 0.35f);
        /* a big ship's nose does not point where it climbs: it rises and sinks as it is */
        if (o->model == MODEL_ROBOT) {
            /* it comes down fast, and the ground knows when it has landed */
            o->pos.y += clampf(f.target.y - o->pos.y, -45.0f * DT, 45.0f * DT);
            o->pitch = 0;
            if (!G.robot_landed && fabsf(f.target.y - o->pos.y) < 0.5f) {
                G.robot_landed = 1;
                shock_ring(vp_v3(o->pos.x, GROUND_Y + 1.0f, o->pos.z), 110.0f, 0.9f, 1);
                burst(vp_v3(o->pos.x, GROUND_Y + 2.0f, o->pos.z), vp_v3(0, 0, 0), 16, 60.0f, 1.2f, SPARK_SMOKE, 1);
                sound_at(FX_BIG_EXPLODE, o->pos);
                G.p.shake = clampf(G.p.shake + 0.4f, 0, 1.2f);
            }
        } else
        o->pos.y += clampf(f.target.y - o->pos.y, -22.0f * DT, 22.0f * DT);
        o->pitch *= 0.9f;
    }
    o->speed += (f.speed - o->speed) * 0.03f;
    o->roll *= 0.5f;                                     /* a big ship barely banks */
    o->vel = vp_v3_scale(forward_of(o), o->speed);
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    o->spin += 22.0f * DT;
    if (o->flash > 0) o->flash--;
    for (i = 0; i < BOSS_PARTS; i++) if (G.part_flash[i] > 0) G.part_flash[i]--;
    /* an engine that is out trails smoke: those of a stage that is over,
     * and those of this stage that are gone */
    if (G.frames % 6 == 0) {
        int k;
        for (k = 0; k <= G.boss_stage; k++) {
            const struct boss_stage *sk = &b->stage[k];
            for (i = 0; i < sk->parts; i++)
                if (sk->part[i].look == LOOK_ENGINE && (k < G.boss_stage || G.part_hp[i] <= 0))
                    burst(local(o, sk->part[i].x, sk->part[i].y, sk->part[i].z), vp_v3(0, 0, 0), 1, 5.0f, 1.2f, SPARK_SMOKE, 0);
                else if (sk->part[i].look == LOOK_LASER && k < G.boss_stage && G.state != BOSSTURN)     /* the wrecked turret smokes on */
                    burst(local(o, sk->part[i].x, sk->part[i].y - 2.0f, sk->part[i].z), vp_v3(0, 0, 0), 1, 6.0f, 1.4f,
                          (G.frames / 6) % 3 == 0 ? SPARK_FIRE : SPARK_SMOKE, 0);
                else if (sk->part[i].look == LOOK_BAY && k < G.boss_stage && G.state != BOSSTURN)      /* the wrecked bay smokes on */
                    burst(local(o, sk->part[i].x, sk->part[i].y - 1.0f, sk->part[i].z + (frand() - 0.5f) * sk->part[i].r * 2.0f),
                          vp_v3(0, 0, 0), 1, 6.0f, 1.4f, (G.frames / 6) % 3 == 0 ? SPARK_FIRE : SPARK_SMOKE, 0);
        }
    }
}
