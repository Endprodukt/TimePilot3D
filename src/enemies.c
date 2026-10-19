/* enemies.c - the sky's side: the enemies and how each era's fight, the
 * formations, the big ones, the boss and its guns, the homing missiles,
 * the guns on the ground, and keeping the sky full. */
#include "game_internal.h"

/* this file's own, in the order below */
static vp_vec3 random_offset(float r);
static float floor_at(float x, float z);
static void spawn_enemy(void);
static void spawn_formation(void);
static void spawn_heavy(void);
static void throw_bomb(obj *o);
static void spawn_boss(void);
static int hunters_wanted(void);
static void thin_for_boss(void);
static void escort_fly(obj *o);
static void shoot(vp_vec3 from, vp_vec3 dir, float speed, float damage, int what);
static void enemy_fire(obj *o, vp_vec3 dir, float speed, float damage);
static void launch_missile_from(vp_vec3 from, float heading, float pitch);
static void launch_missile(obj *o);
static void keep_in_sky(obj *o);
static void fly(obj *o);
static vp_vec3 lead_point(const obj *o, float shot_speed, float lead);
static int player_threatens(const obj *o, float d);
static int decides_to_evade(obj *o, float d);
static void set_mode(obj *o, int mode, float seconds);
static void nose_guns(obj *o, float d);
static void begin_evading(obj *o);
static void enemy_evades(obj *o, float turn, float speed);
static vp_vec3 attack_target(obj *o, float d, float speed);
static void fight_pursuit(obj *o, float d, float speed);
static void fight_boomzoom(obj *o, float d, float speed);
static void fight_hover(obj *o, float d, float speed);
static void fight_jet(obj *o, float d, float speed);
static void fight_saucer(obj *o, float d, float speed);

/* A point beside the player for an enemy to fly at, so a swarm does not
 * all aim at the same spot. */

static vp_vec3 random_offset(float r)
{
    return vp_v3((frand() - 0.5f) * 2.0f * r, (frand() - 0.5f) * r, (frand() - 0.5f) * 2.0f * r);
}

/* How low the era's flyers may go here: over the ground, and over what
 * is built on it. */

static float floor_at(float x, float z)
{
    return 18.0f + city_top(G.era, x, z, 25.0f);
}

static void spawn_enemy(void)
{
    const struct era *e = era();
    obj *o = new_obj(K_ENEMY);
    float bearing = frand() * PI2, d = SPAWN_NEAR + frand() * (SPAWN_FAR - SPAWN_NEAR);
    vp_vec3 to;

    if (!o) return;
    o->model = e->model;
    o->pos = vp_v3(G.p.pos.x + sinf(bearing) * d, clampf(G.p.pos.y + (frand() - 0.5f) * 240.0f, 60.0f, 700.0f),
                   G.p.pos.z + cosf(bearing) * d);
    if (o->pos.y < floor_at(o->pos.x, o->pos.z) + 30.0f) o->pos.y = floor_at(o->pos.x, o->pos.z) + 30.0f;
    to = vp_v3_sub(G.p.pos, o->pos);
    o->heading = atan2f(to.x, to.z);
    o->speed = e->speed * (0.9f + frand() * 0.2f) * (1.0f + 0.08f * (float)G.loop);
    o->turn = e->turn * (1.0f + 0.1f * (float)G.loop);
    o->timer = 1.0f + frand() * e->fire_period;
    o->offset = random_offset(70.0f);
    o->reaim = 3.0f + frand() * 3.0f;
    o->side = frand() < 0.5f ? -1.0f : 1.0f;
    o->trait = frand();
    o->phase = frand() * PI2;
    o->check = frand() * 0.5f;
    o->wp = o->pos;
    o->hp = 1;
}

/* Five of the era's enemies in a V, crossing ahead of the player from
 * one side to the other. They keep their course and do not shoot. */

static void spawn_formation(void)
{
    const struct era *e = era();
    float side = frand() < 0.5f ? -1.0f : 1.0f;
    float bearing = G.p.heading + side * (0.5f + frand() * 0.4f);
    float heading = wrap_angle(G.p.heading - side * (PI / 2.0f + (frand() - 0.5f) * 0.4f));
    vp_vec3 lead = vp_v3(G.p.pos.x + sinf(bearing) * 520.0f, clampf(G.p.pos.y + (frand() - 0.5f) * 120.0f, 120.0f, 700.0f),
                         G.p.pos.z + cosf(bearing) * 520.0f);
    vp_vec3 fwd = world_forward(heading, 0), right = vp_v3(cosf(heading), 0, -sinf(heading));
    int i;

    if (G.era == 3 && lead.y < 240.0f) lead.y = 240.0f;        /* over the towers */
    G.form_id++;
    G.form_killed = 0;
    sound_fx(FX_FORMATION);             /* a row of quick tones says they are coming */
    for (i = 0; i < FORMATION_SIZE; i++) {
        obj *o = new_obj(K_ENEMY);
        float rank = (float)((i + 1) / 2), wing = (i & 1) ? 1.0f : -1.0f;
        if (!o) return;
        o->model = e->model;
        o->pos = vp_v3_add(lead, vp_v3_add(vp_v3_scale(fwd, -24.0f * rank), vp_v3_scale(right, 22.0f * rank * wing)));
        o->heading = heading;
        o->speed = e->speed * 0.85f;
        o->hp = 1;
        o->form = G.form_id;
    }
}

/* One of the big ones, crossing ahead of the player from one side to the
 * other, a little above. */

static void spawn_heavy(void)
{
    const struct era *e = era();
    const struct heavy *h = &heavies[G.era];
    obj *o = new_obj(K_ENEMY);
    float side = frand() < 0.5f ? -1.0f : 1.0f;
    float bearing = G.p.heading + side * (0.7f + frand() * 0.3f);

    if (!o) return;
    o->model = h->model;
    o->heavy = 1;
    o->size = h->size;
    o->hp = h->hp + G.loop;
    o->pos = vp_v3(G.p.pos.x + sinf(bearing) * 600.0f, clampf(G.p.pos.y + 30.0f + frand() * 60.0f, 260.0f, 700.0f),
                   G.p.pos.z + cosf(bearing) * 600.0f);
    o->heading = wrap_angle(G.p.heading - side * (PI / 2.0f + (frand() - 0.5f) * 0.5f));
    o->speed = e->speed * 0.7f;
    o->timer = 1.5f;
}

/* 1910: a bomb thrown from a biplane. It goes up first and comes down in
 * an arc, aimed to come down where the player will be half the time. */

static void throw_bomb(obj *o)
{
    const float g = 30.0f;
    obj *b = new_obj(K_BOMB);
    float d = dist3(o->pos, G.p.pos), t = clampf(d / 70.0f, 1.2f, 3.5f);
    vp_vec3 target = vp_v3_add(G.p.pos, vp_v3_scale(player_velocity(), t * 0.5f));

    if (!b) return;
    b->pos = o->pos;
    b->vel = vp_v3_scale(vp_v3_sub(target, o->pos), 1.0f / t);
    b->vel.y += 0.5f * g * t;
    b->ang = vp_v3(frand() * 6.0f, frand() * 6.0f, frand() * 6.0f);
    b->life = 6.0f;
    b->value = (int)DMG_BOMB;
    if (d < 500.0f) sound_at(FX_ENEMY_SHOT, o->pos);
}

static void spawn_boss(void)
{
    const struct era *e = era();
    obj *o = new_obj(K_BOSS);
    float bearing = G.p.heading + (frand() < 0.5f ? -0.5f : 0.5f);      /* ahead, to one side */
    int one_hit = G.cheat_kills > G.kills_per_era;                    /* the testing aid: it falls to one hit */

    if (!o) return;
    o->model = e->boss_model;
    o->pos = vp_v3(G.p.pos.x + sinf(bearing) * 620.0f, clampf(G.p.pos.y + 60.0f, 280.0f, 600.0f),
                   G.p.pos.z + cosf(bearing) * 620.0f);
    /* across the player's way, not straight at it: it is seen from the side as it comes */
    o->heading = wrap_angle(atan2f(G.p.pos.x - o->pos.x, G.p.pos.z - o->pos.z) + 1.1f);
    o->speed = e->boss_speed;
    o->turn = 0.4f;
    /* its hull, once its guns are gone: they take their share of the fight, so the hull takes less than it did */
    o->hp = (e->boss_hp * 3) / 5 + 10 * G.loop + 5 * (G.difficulty - DIFFICULTY_NORMAL);
    if (one_hit) o->hp = 1;
    o->value = o->hp;                                           /* what it began with, for the bar */
    boss_begin(o, one_hit);
    o->timer = 2.0f;
    G.boss_alive = 1;
    G.boss_called = 1;
    G.boss_at = G.level_time;
    sound_fx(FX_BOSS);
    /* Its coming is watched (BOSSINTRO): the fight stands still for it,
     * and what was in the air against the player is gone. Not with the
     * testing aid, and not while there is nobody to watch. */
    if (!one_hit && (G.state == PLAYING || G.state == INTRO) && G.p.alive) {
        int i;
        for (i = 0; i < MAX_OBJ; i++) {
            obj *x = &G.objs[i];
            if (x->kind == K_BULLET || x->kind == K_MISSILE || x->kind == K_BOMB) x->kind = K_NONE;
        }
        G.p.stalled = G.p.pull_up = 0;
        G.p.hurt = 0;
        thin_for_boss();
        enter(BOSSINTRO);
    } else {
        notice("BOSS INCOMING");
    }
}

/* An enemy's shot along `dir`. */

/* A shot at the player from `from` along `dir`: every gun there is fires
 * through here, a plane's, a boss's turret, a gun on the ground. */

/* A shot of the enemy's. What the boss and the big ones fire is a big,
 * slow ball: it is seen coming and can be flown round, and hits as hard
 * as it ever did. `speed` is the era's; the slowing is done here. */
static void shoot(vp_vec3 from, vp_vec3 dir, float speed, float damage, int what)
{
    obj *b = new_obj(K_BULLET);
    int big = what == SHOT_BOSS || what == SHOT_HEAVY;
    if (!b) return;
    b->pos = from;
    b->vel = vp_v3_scale(dir, speed * hard()->bullets * (big ? BIG_SHOT_SLOW : 1.0f));
    b->life = big ? 6.5f : 3.5f;
    b->value = (int)damage;
    b->cut = what;
    b->size = big ? BIG_SHOT_SIZE : 0;
    if (dist3(from, G.p.pos) < 500.0f) sound_at(FX_ENEMY_SHOT, from);
}

/* How fast a shot of this one's flies, for aiming it. */
static float shot_speed_of(const obj *o, float speed)
{
    return speed * hard()->bullets * (o->kind == K_BOSS || o->heavy ? BIG_SHOT_SLOW : 1.0f);
}

static void enemy_fire(obj *o, vp_vec3 dir, float speed, float damage)
{
    shoot(vp_v3_add(o->pos, vp_v3_scale(dir, 6.0f)), dir, speed, damage,
          o->kind == K_BOSS ? SHOT_BOSS : o->heavy ? SHOT_HEAVY : SHOT_GUN);
}

/* A homing missile at the player, from `from`, pointing as given at first. */

/* Missiles come in their time, not all the time. A new one may come only
 * when none is in the air and none has been for MISSILE_REST seconds
 * (shorter on the harder settings); where the era allows more than one at
 * once (`most`), the second may only follow right behind the first, as a
 * pair. So after every missile there is air to fight in. */
#define MISSILE_REST 6.5f

static int missile_allowed(int most)
{
    int flying = count_kind(K_MISSILE);
    if (flying == 0) return G.missile_rest <= 0;
    return flying < most && G.missile_pair > 0;
}

/* Once a step: the clocks missile_allowed looks at. */
static void missile_clock(void)
{
    if (count_kind(K_MISSILE) > 0) G.missile_rest = MISSILE_REST / hard()->fire;
    else if (G.missile_rest > 0) G.missile_rest -= DT;
    if (G.missile_pair > 0) G.missile_pair -= DT;
}

static void launch_missile_from(vp_vec3 from, float heading, float pitch)
{
    obj *m;
    if (count_kind(K_MISSILE) == 0) G.missile_pair = 1.2f;
    m = new_obj(K_MISSILE);
    if (!m) return;
    m->model = MODEL_MISSILE;
    m->pos = from;
    m->heading = heading;
    m->pitch = pitch;
    /* slower than the plane with the throttle open, and with a wide
     * turn: it is outrun, and a hard turn across its way leaves it behind */
    m->speed = 122.0f + 8.0f * (float)G.loop;
    m->turn = 0.8f;
    m->life = 6.5f;
    m->hp = 1;
    sound_fx(FX_MISSILE);
}

static void launch_missile(obj *o)
{
    vp_vec3 from = vp_v3_add(o->pos, vp_v3(0, -3.0f, 0));
    if (o->model == MODEL_UFO || o->model == MODEL_MOTHERSHIP) {    /* a saucer has no nose: straight at the player */
        vp_vec3 to = vp_v3_sub(G.p.pos, o->pos);
        launch_missile_from(from, atan2f(to.x, to.z), atan2f(to.y, sqrtf(to.x * to.x + to.z * to.z)));
    } else {
        launch_missile_from(from, o->heading, o->pitch);
    }
}

/* The missile nearest the player, and how far it is; NULL if there is none. */

const obj *nearest_missile(float *distance)
{
    const obj *best = NULL;
    float best_d = 1e9f;
    int i;
    for (i = 0; i < MAX_OBJ; i++) {
        float d;
        if (G.objs[i].kind != K_MISSILE || G.objs[i].scared) continue;      /* one that has lost the plane is no danger */
        d = dist3(G.objs[i].pos, G.p.pos);
        if (d < best_d) { best_d = d; best = &G.objs[i]; }
    }
    *distance = best_d;
    return best;
}

/* Is the boss's wreck still coming down? */

int boss_falling(void)
{
    int i;
    for (i = 0; i < MAX_OBJ; i++) if (G.objs[i].kind == K_WRECK && G.objs[i].boss) return 1;
    return 0;
}

/* Steer a flyer toward `target`: the heading and pitch turn at most
 * `turn` radians a second, the roll banks into the turn. */

void steer_toward(obj *o, vp_vec3 target, float turn, float pitch_max)
{
    vp_vec3 to = vp_v3_sub(target, o->pos);
    float horiz = sqrtf(to.x * to.x + to.z * to.z);
    float want_h = atan2f(to.x, to.z), want_p = clampf(atan2f(to.y, horiz), -pitch_max, pitch_max);
    float dh = wrap_angle(want_h - o->heading), dp = want_p - o->pitch;
    float step = turn * DT;

    o->heading += clampf(dh, -step, step);
    o->pitch += clampf(dp, -step * 0.8f, step * 0.8f);
    o->roll += (clampf(-dh * 1.5f, -1.0f, 1.0f) * 0.9f - o->roll) * 0.1f;
}

/* Kept over the ground and the buildings, and under the sky's top. */

static void keep_in_sky(obj *o)
{
    float low = floor_at(o->pos.x, o->pos.z);
    if (o->pos.y < low) { o->pos.y += (low - o->pos.y) * 0.2f + 0.5f; if (o->pitch < 0.15f) o->pitch = 0.15f; }
    if (o->pos.y > ALT_MAX) { o->pos.y = ALT_MAX; if (o->pitch > 0) o->pitch = 0; }
}

static void fly(obj *o)
{
    o->vel = vp_v3_scale(forward_of(o), o->speed);
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    keep_in_sky(o);
}

/* Where to aim to meet the player, `lead` of the way to where it will be. */

static vp_vec3 lead_point(const obj *o, float shot_speed, float lead)
{
    float t = dist3(o->pos, G.p.pos) / shot_speed * lead;
    return vp_v3_add(G.p.pos, vp_v3_scale(player_velocity(), t));
}

/* Is the player's nose on this one, near enough to shoot? */

static int player_threatens(const obj *o, float d)
{
    vp_vec3 to;
    if (!player_targetable() || d > 450.0f || d < 1.0f) return 0;
    to = vp_v3_scale(vp_v3_sub(o->pos, G.p.pos), 1.0f / d);
    return vp_v3_dot(player_forward(), to) > 0.985f;        /* within 10 degrees */
}

/* A look over the shoulder every half second: aimed at, it may break. */

static int decides_to_evade(obj *o, float d)
{
    if (o->mode == M_EVADE) return 0;
    /* a shot gone past it is noticed at once, and taken more seriously
     * than a nose pointed its way */
    if (o->scared) {
        o->scared = 0;
        o->check = 0.5f;
        return frand() < era()->evade * hard()->evade * 1.3f;
    }
    if ((o->check -= DT) > 0) return 0;
    o->check = 0.5f;
    if (!player_threatens(o, d)) return 0;
    return frand() < era()->evade * hard()->evade;
}

static void set_mode(obj *o, int mode, float seconds)
{
    o->mode = mode;
    o->mode_time = seconds;
}

/* The fixed guns of a plane: fired when the player is before its nose. */

static void nose_guns(obj *o, float d)
{
    const struct era *e = era();
    vp_vec3 fwd = forward_of(o), to;

    if ((o->timer -= DT) > 0 || !player_targetable() || d > e->fire_range || d < 1.0f) return;
    to = vp_v3_scale(vp_v3_sub(lead_point(o, e->bullet_speed + o->speed, e->lead), o->pos), 1.0f);
    to = vp_v3_normalize(to);
    if (vp_v3_dot(fwd, to) < 0.975f) return;                 /* not within about 13 degrees */
    if (e->missiles > 0 && d > 180.0f && missile_allowed(e->missiles) && frand() < 0.35f) launch_missile(o);
    else enemy_fire(o, fwd, e->bullet_speed + o->speed, DMG_BULLET);
    o->timer = e->fire_period * (0.7f + 0.6f * frand()) / ((1.0f + 0.15f * (float)G.loop) * hard()->fire);
}

/* Which way out: a move of its time, and one there is room for. */

static void begin_evading(obj *o)
{
    static const int moves_1910[] = { MOVE_JINK, MOVE_BREAK, MOVE_JINK };
    static const int moves_1940[] = { MOVE_BREAK, MOVE_SPLIT_S, MOVE_BARREL, MOVE_JINK };
    static const int moves_1982[] = { MOVE_BREAK, MOVE_SPLIT_S, MOVE_BARREL, MOVE_ZOOM, MOVE_JINK };
    const struct era *e = era();
    float room = o->pos.y - floor_at(o->pos.x, o->pos.z);

    o->side = frand() < 0.5f ? -1.0f : 1.0f;
    o->move = e->behave == B_PURSUIT ? moves_1910[rnd(3)] : e->behave == B_BOOMZOOM ? moves_1940[rnd(4)] : moves_1982[rnd(5)];
    if (o->move == MOVE_SPLIT_S && room < 220.0f) o->move = MOVE_BREAK;     /* no room to go down */
    set_mode(o, M_EVADE, e->behave == B_HOVER ? 0.9f
                         : o->move == MOVE_SPLIT_S ? 2.0f : o->move == MOVE_ZOOM ? 1.6f : 1.2f + frand() * 0.8f);
    o->straight = o->mode_time;         /* how long the move is, for its halves */
}

/* A plane gets out of the way, for as long as the mode lasts. */

static void enemy_evades(obj *o, float turn, float speed)
{
    float done = o->straight > 0 ? 1.0f - o->mode_time / o->straight : 1.0f;     /* 0..1 through the move */
    float low = floor_at(o->pos.x, o->pos.z) + 120.0f;
    float want_p;

    switch (o->move) {
    case MOVE_JINK: {           /* scissors: a hard turn one way, then the other, twice a second */
        float way = ((int)(o->mode_time / 0.45f) & 1) ? o->side : -o->side;
        o->heading = wrap_angle(o->heading + way * turn * 1.5f * DT);
        o->pitch += clampf(0.15f * way - o->pitch, -turn * DT, turn * DT);
        o->roll += (-way * 1.2f - o->roll) * 0.25f;
        o->speed = speed;
        break;
    }
    case MOVE_SPLIT_S:          /* half a roll, then down and through, out the other way and faster */
        o->heading = wrap_angle(o->heading + o->side * turn * 1.5f * DT);
        want_p = done < 0.6f ? -0.9f : 0.1f;
        if (o->pos.y < low) want_p = 0.3f;
        o->pitch += clampf(want_p - o->pitch, -turn * 1.6f * DT, turn * 1.6f * DT);
        o->roll = wrap_angle(o->roll + (done < 0.6f ? o->side * 5.0f * DT : -o->roll * 0.1f));
        o->speed = speed * 1.25f;
        break;
    case MOVE_BARREL: {         /* round a corkscrew about its course */
        vp_vec3 right = vp_v3(cosf(o->heading), 0, -sinf(o->heading));
        float ph = done * PI2;
        o->roll = wrap_angle(o->side * ph);
        o->pos = vp_v3_add(o->pos, vp_v3_scale(right, o->side * cosf(ph) * 30.0f * DT));
        o->pos.y += sinf(ph) * 30.0f * DT;
        o->heading = wrap_angle(o->heading + o->side * turn * 0.4f * DT);
        o->speed = speed;
        break;
    }
    case MOVE_ZOOM:             /* up, slowing, so that what was behind it flies past below */
        o->heading = wrap_angle(o->heading + o->side * turn * 0.6f * DT);
        o->pitch += clampf((done < 0.7f ? 0.8f : -0.2f) - o->pitch, -turn * 1.4f * DT, turn * 1.4f * DT);
        o->roll += (-o->side * 0.5f - o->roll) * 0.1f;
        o->speed = speed * (done < 0.7f ? 0.7f : 1.1f);
        break;
    default:                    /* a break: hard round to one side, diving, or climbing when low */
        want_p = o->pos.y < low ? 0.45f : -0.35f * o->side;
        o->heading = wrap_angle(o->heading + o->side * turn * 1.3f * DT);
        o->pitch += clampf(want_p - o->pitch, -turn * DT, turn * DT);
        o->roll += (-o->side * 1.2f - o->roll) * 0.15f;
        o->speed = speed * 1.1f;
        break;
    }
}

/* Where a plane on the attack flies, and how fast. One that is the kind
 * for it (its trait against the era's `six`) does not come straight in:
 * it swings wide round the player to come in from behind, and once it is
 * behind and near it holds the player's speed and stays there. The others,
 * and all of them from behind, fly at where the player will be, at their
 * own speed: slow down and they go past.
 *
 * Not the whole swarm at once, and nobody for ever. Only so many are on
 * the player at a time (ENGAGED_AT_ONCE, more with the difficulty); the
 * rest circle it out of reach and wait their turn, and can be hunted
 * there. One that is on the player keeps at it for ENGAGE_TIME seconds -
 * the seconds on its tail count three times - then breaks off and rests
 * with the others. Only so many sit on the tail together; one too many
 * breaks off at once. */

#define ENGAGED_AT_ONCE 4           /* on easy; one more with every step of the difficulty, and one more where nobody has missiles */
#define ENGAGE_TIME   8.0f          /* seconds one keeps at the player, times the difficulty's `evade` */
#define ENGAGE_REST   3.5f          /* seconds it then keeps away */
#define TAIL_NEAR     260.0f        /* metres behind the player that count as on its tail */
#define LOITER_RANGE  420.0f        /* metres from the player the ones that wait circle at */

/* Is this one on the player in this step? Called once a step for every
 * enemy that fights; it keeps the count and the plane's own clock (see
 * above). `tailing`: it is close behind the player. The helicopters and
 * the saucers ask too: the ones that are not on hold off and do not
 * shoot. */
static int takes_part(obj *o, int tailing)
{
    static unsigned counted_at;     /* the step the swarm was last counted in, and the counts of this one */
    static int engaged, on_tail;
    int slots = ENGAGED_AT_ONCE + hard()->count + (era()->missiles == 0 ? 1 : 0) - (G.era >= 3 ? 1 : 0);       /* one fewer in the last two eras: their shots are the fastest */
    int room = 1 + hard()->count / 2;               /* on the tail at once: one on easy and normal, two on hard and ace (2026-10-14: too many behind) */

    if (counted_at != G.frames) { counted_at = G.frames; engaged = on_tail = 0; }
    if (o->tail < 0) {                              /* resting */
        o->tail += DT;
        if (o->tail > 0) o->tail = 0;
        return 0;
    }
    if (engaged >= slots) return 0;                 /* waiting its turn */
    engaged++;
    o->tail += tailing ? 4.0f * DT : DT;           /* a second on the tail counts four: it does not stay there long */
    if (o->tail > ENGAGE_TIME * hard()->evade || (tailing && on_tail >= room))
        o->tail = -ENGAGE_REST * (0.7f + 0.6f * frand());
    else if (tailing) on_tail++;
    return 1;
}

static vp_vec3 attack_target(obj *o, float d, float speed)
{
    const struct era *e = era();
    vp_vec3 pfwd = player_forward(), rel = vp_v3_sub(o->pos, G.p.pos), target;
    int behind = vp_v3_dot(pfwd, rel) < 0, six = o->trait < e->six;
    int tailing = behind && d < TAIL_NEAR;
    float want = speed;

    if (!takes_part(o, tailing)) {
        /* waiting, or resting: round the player in a wide circle */
        /* Each has its own place on the circle (its `phase`, drifting
         * slowly), so they stand all round the player and not in one
         * crowd; it goes there along the circle, not across the middle. */
        float here = atan2f(rel.x, rel.z), place = o->phase + o->side * o->age * 0.05f;
        float bearing = here + clampf(wrap_angle(place - here), -0.7f, 0.7f);
        target = vp_v3_add(G.p.pos, vp_v3(sinf(bearing) * LOITER_RANGE, o->offset.y + 40.0f * o->side, cosf(bearing) * LOITER_RANGE));
        o->speed += (speed - o->speed) * 0.04f;
        return target;
    }

    if (!behind && d > 150.0f && six)
        target = vp_v3_add(vp_v3_sub(G.p.pos, vp_v3_scale(pfwd, 230.0f)), o->offset);
    else
        target = vp_v3_add(lead_point(o, e->bullet_speed + speed, e->lead), vp_v3_scale(o->offset, behind ? 0.6f : 0.3f));
    if (tailing && six) want = clampf(G.p.speed + 8.0f, speed * 0.75f, speed * 1.3f);
    o->speed += (want - o->speed) * 0.04f;
    return target;
}

/* 1910: at the player, weaving up and down; past it, on for a moment
 * before turning back. And bombs. */

static void fight_pursuit(obj *o, float d, float speed)
{
    if (o->mode == M_EVADE) {
        enemy_evades(o, o->turn, speed);
    } else if (o->mode == M_EXTEND) {
        o->speed = speed;
        o->roll *= 0.95f;
    } else {
        vp_vec3 target = attack_target(o, d, speed);
        target.y += 25.0f * sinf(o->age * 1.1f + o->phase);
        steer_toward(o, target, o->turn, 0.5f);
        if (d < 60.0f) set_mode(o, M_EXTEND, 1.5f + frand());
    }
    fly(o);
    nose_guns(o, d);
    /* and bombs, thrown at the player from above or level with it */
    if ((o->bomb -= DT) <= 0 && player_targetable() && d < 340.0f && d > 80.0f && o->pos.y > G.p.pos.y - 40.0f) {
        throw_bomb(o);
        o->bomb = (4.0f + frand() * 4.0f) / hard()->fire;
    }
}

/* 1940: a pass aimed ahead of the player, then away in a climb to turn
 * and come again. */

static void fight_boomzoom(obj *o, float d, float speed)
{
    if (o->mode == M_EVADE) {
        enemy_evades(o, o->turn, speed);
    } else if (o->mode == M_EXTEND) {
        o->speed = speed * 1.2f;
        o->pitch += clampf(0.3f - o->pitch, -0.6f * DT, 0.6f * DT);
        o->roll *= 0.95f;
    } else {
        steer_toward(o, attack_target(o, d, speed), o->turn, 0.7f);
        if (d < 90.0f) set_mode(o, M_EXTEND, 2.5f + frand());
    }
    fly(o);
    nose_guns(o, d);
}

/* 1970: in to a few hundred metres, then nose on the player, sliding
 * sideways round it and backing off when it comes close. */

static void fight_hover(obj *o, float d, float speed)
{
    vp_vec3 right = vp_v3(cosf(o->heading), 0, -sinf(o->heading));
    if (o->mode == M_EVADE) {                   /* up or down, fast, and aside */
        steer_toward(o, G.p.pos, o->turn * 1.5f, 0.6f);
        o->speed = 0;
        o->pos.y += o->side * 38.0f * DT;
        o->pos = vp_v3_add(o->pos, vp_v3_scale(right, -o->side * 30.0f * DT));
    } else if (!takes_part(o, 0)) {
        /* not its turn: it hangs half a kilometre off, nose on the player,
         * drifting round it, and holds its fire */
        steer_toward(o, G.p.pos, o->turn, 0.5f);
        o->speed += ((d < 440.0f ? -18.0f : d > 540.0f ? speed * 0.7f : 0.0f) - o->speed) * 0.05f;
        o->pos = vp_v3_add(o->pos, vp_v3_scale(right, o->side * 12.0f * DT));
        fly(o);
        return;
    } else if (d > 330.0f) {
        o->speed = speed;
        steer_toward(o, vp_v3_add(G.p.pos, o->offset), o->turn, 0.5f);
    } else {
        steer_toward(o, G.p.pos, o->turn * 1.5f, 0.6f);
        o->speed += ((d < 160.0f ? -22.0f : 8.0f) - o->speed) * 0.05f;
        o->pos = vp_v3_add(o->pos, vp_v3_scale(right, o->side * 26.0f * DT));
        o->pos.y += 8.0f * sinf(o->age * 1.3f + o->phase) * DT;
        o->roll += (-o->side * 0.3f - o->roll) * 0.1f;
        if (o->mode_time <= 0) { o->side = -o->side; o->mode_time = 2.0f + frand() * 2.5f; }
    }
    fly(o);
    nose_guns(o, d);
}

/* 1982: round to the player's tail and onto it, matching its speed
 * there; too close, away on the burner and round again. */

static void fight_jet(obj *o, float d, float speed)
{
    if (o->mode == M_EVADE) {
        enemy_evades(o, o->turn * 1.2f, speed);
    } else if (o->mode == M_EXTEND) {
        o->speed = speed * 1.3f;
        o->roll *= 0.95f;
    } else {
        steer_toward(o, attack_target(o, d, speed), o->turn, 0.8f);
        if (d < 70.0f) set_mode(o, M_EXTEND, 1.8f + frand());
    }
    fly(o);
    nose_guns(o, d);
}

/* 2001: from point to point round the player, at once and in straight
 * lines, shooting whichever way the player is. */

static void fight_saucer(obj *o, float d, float speed)
{
    const struct era *e = era();
    vp_vec3 to;
    float len, low;

    int on = takes_part(o, 0);          /* not its turn: it darts about farther off, and does not shoot */

    if (o->mode == M_EVADE) { o->mode = M_ATTACK; o->mode_time = 0; speed *= 1.3f; }    /* gone, now */
    if (o->mode_time <= 0) {
        float a = frand() * PI2, r = on ? 160.0f + frand() * 200.0f : 400.0f + frand() * 160.0f;
        o->wp = vp_v3_add(vp_v3_add(G.p.pos, player_velocity()),
                          vp_v3(cosf(a) * r, (frand() - 0.5f) * 160.0f, sinf(a) * r));
        o->wp.y = clampf(o->wp.y, floor_at(o->wp.x, o->wp.z) + 20.0f, ALT_MAX - 50.0f);
        /* A dart, and then it hangs where it has got to: long enough to
         * be aimed at and hit. It takes a moment to get going and to
         * stop - it is not somewhere else from one frame to the next. */
        o->mode_time = 1.6f + frand() * 1.4f;
        o->speed = speed * (0.9f + frand() * 0.5f);
    }
    to = vp_v3_sub(o->wp, o->pos);
    len = vp_v3_length(to);
    {
        vp_vec3 want = len > 6.0f ? vp_v3_scale(to, o->speed / len) : vp_v3(0, 0, 0);     /* there: it hangs still */
        o->vel = vp_v3_add(o->vel, vp_v3_scale(vp_v3_sub(want, o->vel), 0.09f));
    }
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    low = floor_at(o->pos.x, o->pos.z);
    if (o->pos.y < low) o->pos.y = low;

    if (on && (o->timer -= DT) <= 0 && player_targetable() && d < e->fire_range) {
        if (e->missiles > 0 && d > 200.0f && missile_allowed(e->missiles) && frand() < 0.2f) launch_missile(o);
        else enemy_fire(o, vp_v3_normalize(vp_v3_sub(lead_point(o, e->bullet_speed, e->lead), o->pos)),
                        e->bullet_speed, DMG_BULLET);
        o->timer = e->fire_period * (0.7f + 0.6f * frand()) / ((1.0f + 0.15f * (float)G.loop) * hard()->fire);
    }
}

void update_enemy(obj *o)
{
    const struct era *e = era();
    float d = dist3(o->pos, G.p.pos);
    float speed = e->speed * (1.0f + 0.08f * (float)G.loop);

    o->age += DT;
    o->spin += 30.0f * DT;
    if (o->flash > 0) o->flash--;
    if (d > DESPAWN) { o->kind = K_NONE; return; }      /* lost: another will come */

    if (o->escort) { escort_fly(o); if (o->escort) return; }
    if (o->form || o->heavy) {                           /* on its course, whatever happens */
        fly(o);
        if (d > 1000.0f && vp_v3_dot(o->vel, vp_v3_sub(G.p.pos, o->pos)) < 0) { o->kind = K_NONE; return; }
        if (o->heavy && (o->timer -= DT) <= 0 && player_targetable() && d < 520.0f) {    /* its turrets */
            enemy_fire(o, vp_v3_normalize(vp_v3_sub(lead_point(o, shot_speed_of(o, e->bullet_speed), 0.6f), o->pos)), e->bullet_speed, DMG_BULLET);
            o->timer = (d < 300.0f ? 1.2f : 2.2f) / hard()->fire;      /* faster at close quarters */
        }
        return;
    }
    if ((o->reaim -= DT) <= 0) { o->offset = random_offset(70.0f); o->reaim = 3.0f + frand() * 3.0f; }
    if (o->mode_time > 0) o->mode_time -= DT;
    if (o->mode != M_ATTACK && o->mode_time <= 0 && e->behave != B_HOVER) o->mode = M_ATTACK;
    if (o->mode == M_EVADE && o->mode_time <= 0) o->mode = M_ATTACK;
    if (decides_to_evade(o, d)) begin_evading(o);
    {
        /* Not wing to wing: two that come nearer each other than
         * SPREAD push apart, so the player never meets a wall of them. */
        const float SPREAD = 75.0f;
        int j;
        for (j = 0; j < MAX_OBJ; j++) {
            const obj *q = &G.objs[j];
            vp_vec3 apart;
            float dd;
            if (q == o || q->kind != K_ENEMY || q->form || q->heavy) continue;
            apart = vp_v3_sub(o->pos, q->pos);
            dd = vp_v3_length(apart);
            if (dd < SPREAD && dd > 0.5f) o->pos = vp_v3_add(o->pos, vp_v3_scale(apart, (SPREAD - dd) / SPREAD * 16.0f * DT / dd));
        }
    }
    /* About to fly into the player: no pilot does that on purpose. A
     * plane coming at it nose on turns away to the side it is already on. */
    if (o->mode == M_ATTACK && e->behave != B_SAUCER && e->behave != B_HOVER && d < 75.0f && d > 1.0f) {
        vp_vec3 to = vp_v3_scale(vp_v3_sub(G.p.pos, o->pos), 1.0f / d);
        if (vp_v3_dot(forward_of(o), to) > 0.8f) {
            vp_vec3 right = vp_v3(cosf(o->heading), 0, -sinf(o->heading));
            o->move = MOVE_BREAK;
            o->side = vp_v3_dot(right, to) > 0 ? -1.0f : 1.0f;
            set_mode(o, M_EVADE, 0.8f);
            o->straight = 0.8f;
        }
    }
    switch (e->behave) {
    case B_PURSUIT:  fight_pursuit(o, d, speed);  break;
    case B_BOOMZOOM: fight_boomzoom(o, d, speed); break;
    case B_HOVER:    fight_hover(o, d, speed);    break;
    case B_JET:      fight_jet(o, d, speed);      break;
    default:         fight_saucer(o, d, speed);   break;
    }
}

/* What the boss fires with (boss.c): its balls at the era's speed, balls
 * with a speed and a life of their own (mines, a tail gunner's burst),
 * bombs, and missiles - in the time all missiles keep, or at once. */
float boss_shot_speed(float era_speed) { return era_speed * hard()->bullets * BIG_SHOT_SLOW; }
float boss_mg_speed(float era_speed) { return era_speed * hard()->bullets * 1.05f; }
float boss_rocket_speed(float era_speed) { return era_speed * hard()->bullets * 1.3f; }

/* A round of its machine gun: small and fast. */
void boss_mg(vp_vec3 from, vp_vec3 dir, float era_speed)
{
    obj *b = new_obj(K_BULLET);
    if (!b) return;
    b->pos = from;
    b->vel = vp_v3_scale(dir, boss_mg_speed(era_speed));
    b->life = 2.2f;
    b->value = (int)DMG_MG;
    b->cut = SHOT_MG;
    burst(from, vp_v3(0, 0, 0), 1, 12.0f, 0.12f, SPARK_WHITE, 0);     /* its muzzle flashes: where it comes from is seen */
    if (G.frames % 2 == 0 && dist3(from, G.p.pos) < 600.0f) sound_at(FX_ENEMY_SHOT, from);
}

/* An unguided rocket: straight where it is pointed. */
void boss_rocket(vp_vec3 from, vp_vec3 dir, float era_speed)
{
    obj *b = new_obj(K_BULLET);
    if (!b) return;
    b->pos = from;
    b->vel = vp_v3_scale(dir, boss_rocket_speed(era_speed));
    b->life = 3.5f;
    b->value = (int)DMG_ROCKET;
    b->cut = SHOT_ROCKET;
    sound_fx(FX_MISSILE);
}

/* One of a boss's escort (the Chinook's four gunships, ice blue): it
 * flies round the boss, nose on the player, and fires its machine gun in
 * bursts; and in turn, one after the other on a fixed round, each breaks
 * out for a run at the player with rockets (escort_fly). As long as one
 * of them is left the boss takes no harm, and no other helicopter comes
 * (hunters_wanted). */
void spawn_escort(const obj *boss, int k)
{
    obj *o = new_obj(K_ENEMY);
    if (!o) return;
    o->model = MODEL_GUNSHIP;           /* the Chinook's own: not one of the era's swarm */
    o->escort = k + 1;
    o->phase = PI2 * (float)k / 4.0f;
    o->pos = vp_v3_add(boss->pos, vp_v3(sinf(o->phase) * 90.0f, 10.0f, cosf(o->phase) * 90.0f));
    o->hp = 3;
    o->turn = 2.0f;
    o->timer = 2.0f + 0.7f * (float)k;
    o->side = k & 1 ? 1.0f : -1.0f;
}

static void escort_fly(obj *o)
{
    const struct era *e = era();
    const obj *boss = NULL;
    vp_vec3 want, to;
    float a, d = dist3(o->pos, G.p.pos);
    int i;

    for (i = 0; i < MAX_OBJ; i++) if (G.objs[i].kind == K_BOSS) { boss = &G.objs[i]; break; }
    if (!boss) { o->escort = 0; return; }               /* no boss any more: one of the swarm */
    a = o->phase + o->age * 0.7f;
    want = vp_v3_add(boss->pos, vp_v3(sinf(a) * 95.0f, 18.0f * sinf(a * 2.0f), cosf(a) * 95.0f));
    {
        /* Its run: every RUN_EVERY seconds the next of the four in their
         * order (a turn whose one is down passes empty: a breath). It
         * blinks white and its warning sounds, it comes at the plane, two
         * rockets go, and it is back in the ring. */
        const float RUN_EVERY = 6.0f, RUN = 3.4f, TELL = 0.6f, FIRE = 1.5f;
        float t = G.boss_stage_time, into = fmodf(t, RUN_EVERY);
        int turn = (int)(t / RUN_EVERY) % 4 + 1;
        if (G.state == PLAYING && player_targetable() && o->escort == turn && into < RUN) {
            vp_vec3 ahead = vp_v3_add(G.p.pos, vp_v3_add(vp_v3_scale(player_velocity(), 1.2f), vp_v3(0, 25.0f, 0)));
            if (into >= TELL - DT && into < TELL) sound_fx(FX_WARN);
            if (into >= TELL && into < FIRE) o->flash = (G.frames / 4) & 1 ? 2 : 0;
            if (into < FIRE + 0.4f) want = vp_v3_add(o->pos, vp_v3_scale(vp_v3_normalize(vp_v3_sub(ahead, o->pos)), 60.0f));
            if ((into >= FIRE && into - DT < FIRE) || (into >= FIRE + 0.25f && into - DT < FIRE + 0.25f)) {
                vp_vec3 at = vp_v3_add(o->pos, vp_v3(0, -1.5f, 0));
                boss_rocket(at, vp_v3_normalize(vp_v3_sub(vp_v3_add(G.p.pos, vp_v3_scale(player_velocity(),
                            d / boss_rocket_speed(e->bullet_speed) * 0.9f)), at)), e->bullet_speed);
            }
        }
    }
    to = vp_v3_sub(want, o->pos);
    o->vel = vp_v3_add(boss->vel, vp_v3_scale(to, 2.0f));
    {
        float v = vp_v3_length(o->vel);         /* not faster than a helicopter goes */
        if (v > 120.0f) o->vel = vp_v3_scale(o->vel, 120.0f / v);
    }
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    o->heading = atan2f(G.p.pos.x - o->pos.x, G.p.pos.z - o->pos.z);
    o->pitch = 0;
    o->roll = 0.2f * sinf(a);
    o->spin += 30.0f * DT;              /* its rotor turns */
    if (o->mode_time > 0) o->mode_time -= DT;
    if (!player_targetable() || d > 560.0f) return;
    /* its gun: five rounds, then a breath */
    if ((o->timer -= DT) <= 0) {
        vp_vec3 aim = vp_v3_add(G.p.pos, vp_v3_scale(player_velocity(), d / boss_mg_speed(e->bullet_speed) * 0.5f));
        boss_mg(o->pos, vp_v3_normalize(vp_v3_sub(aim, o->pos)), e->bullet_speed);
        o->scared++;
        o->timer = o->scared % 5 ? 0.1f : 2.6f / hard()->fire;
    }
}

void boss_shoot(vp_vec3 from, vp_vec3 dir, float era_speed) { shoot(from, dir, era_speed, DMG_BOSS_BULLET, SHOT_BOSS); }

void boss_ball(vp_vec3 at, vp_vec3 vel, float life)
{
    obj *b = new_obj(K_BULLET);
    if (!b) return;
    b->pos = at;
    b->vel = vel;
    b->life = life;
    b->value = (int)DMG_BOSS_BULLET;
    b->cut = SHOT_BOSS;
    b->size = BIG_SHOT_SIZE;
}

void boss_bomb(vp_vec3 at, vp_vec3 vel)
{
    obj *b = new_obj(K_BOMB);
    if (!b) return;
    b->pos = at;
    b->vel = vel;
    b->ang = vp_v3(frand() * 6.0f, frand() * 6.0f, frand() * 6.0f);
    b->life = 9.0f;
    b->value = (int)DMG_BOSS_BOMB;
    b->size = 2.6f;             /* big, and drawn as a boss's (draw.c): it has to be seen coming down */
}

void boss_missile_from(vp_vec3 from, float heading, float pitch) { launch_missile_from(from, heading, pitch); }

void boss_missile_if_allowed(obj *o) { if (missile_allowed(era()->missiles)) launch_missile(o); }

void update_missile(obj *o)
{
    o->age += DT;
    if ((o->life -= DT) <= 0) { burst(o->pos, o->vel, 4, 20.0f, 0.4f, SPARK_FIRE, 0); o->kind = K_NONE; return; }
    {
        /* It can lose the plane (scared 1): once it has flown so far past
         * that the plane is well off its nose, it does not come round
         * again, flies on and burns out. With flares out it goes for
         * them instead (scared 2), and bursts there. */
        vp_vec3 to = vp_v3_sub(G.p.pos, o->pos);
        float d = vp_v3_length(to);
        if (!o->scared && player_targetable() && d > 1.0f) {
            if (G.p.flares > 0 && d < FLARE_RANGE) {
                vp_vec3 back = vp_v3_scale(player_forward(), -40.0f);
                o->scared = 2;
                o->wp = vp_v3_add(vp_v3_add(G.p.pos, back), vp_v3(0, -12.0f, 0));
                G.p.flare_show = 0.9f;
                sound_fx(FX_MISSILE);
            } else if (o->age > 0.8f && d < 240.0f && vp_v3_dot(forward_of(o), vp_v3_scale(to, 1.0f / d)) < 0.3f) {
                o->scared = 1;
                o->life = 4.0f;             /* on it flies, straight, for these seconds: it can be shot down for its points */
            }
        }
        if (o->scared == 2) {
            o->wp.y -= 10.0f * DT;
            steer_toward(o, o->wp, 2.5f, 1.2f);
            if (dist3(o->pos, o->wp) < 14.0f) {
                burst(o->pos, o->vel, 10, 40.0f, 0.6f, SPARK_FIRE, 0);
                burst(o->pos, vp_v3(0, 0, 0), 8, 30.0f, 0.8f, SPARK_WHITE, 0);
                shock_ring(o->pos, 40.0f, 0.4f, 0);
                sound_at(FX_EXPLODE, o->pos);
                o->kind = K_NONE;
                return;
            }
        } else if (!o->scared && player_targetable()) {
            steer_toward(o, G.p.pos, o->turn, 1.2f);
        }
    }
    o->roll = 0;
    o->vel = vp_v3_scale(forward_of(o), o->speed);
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    if (o->pos.y < GROUND_Y || city_hit(G.era, o->pos, 1.0f)) { burst(o->pos, vp_v3(0, 0, 0), 5, 25.0f, 0.5f, SPARK_FIRE, 0); o->kind = K_NONE; return; }
    if (dist3(o->pos, G.p.pos) > DESPAWN) o->kind = K_NONE;
}

/* What stands on the ground and matters, each step: a gate flown through
 * gives its item; a gun shoots at the plane when it is near and low
 * enough, in its own rhythm, and in the eras that have them now and then
 * sends a missile up instead. */

void run_sites(void)
{
    const struct era *e = era();
    city_site sites[16];
    int n, i;

    if (!player_targetable()) return;
    n = city_sites(G.era, G.p.pos, 560.0f, sites, 16);
    for (i = 0; i < n; i++) {
        const city_site *s = &sites[i];
        float d = dist3(s->at, G.p.pos);
        if (s->kind == CITY_GATE && !s->spent && d < 17.0f) {
            city_mark(s->cx, s->cz, CITY_SPENT);
            grant_item(random_item());
        } else if (s->kind == CITY_GUN && d < 520.0f && G.p.pos.y < 420.0f) {
            unsigned period = (unsigned)(150.0f / hard()->fire), beat = G.frames + (unsigned)(s->cx * 37 + s->cz * 91);
            if (period < 30) period = 30;
            if (beat % period != 0) continue;
            if (e->missiles > 0 && (beat / period) % 3 == 2 && missile_allowed(e->missiles)) {
                /* a missile off the ground, steeply up at first */
                launch_missile_from(vp_v3_add(s->at, vp_v3(0, 4.0f, 0)),
                                    atan2f(G.p.pos.x - s->at.x, G.p.pos.z - s->at.z), 1.2f);
            } else {
                /* flak: aimed where the plane will be, so flying straight over a gun is the way to be hit */
                float speed = e->bullet_speed * 1.2f;
                vp_vec3 from = vp_v3_add(s->at, vp_v3(0, 3.0f, 0));
                vp_vec3 aim = vp_v3_add(G.p.pos, vp_v3_scale(player_velocity(), d / (speed * hard()->bullets) * 0.8f));
                shoot(from, vp_v3_normalize(vp_v3_sub(aim, from)), speed, DMG_BULLET, SHOT_FLAK);
            }
        }
    }
}

/* Keep the sky full: the era's count of enemies, fewer beside the boss;
 * and now and then a formation crossing. */

/* How many hunt the player at once: the era's number, more each time
 * round and on the harder settings; with a boss up, a third of that - the
 * boss is the fight, the others only keep it from being a duel. */
static int hunters_wanted(void)
{
    int want = era()->count + G.loop + hard()->count;
    if (want < 3) want = 3;
    if (G.boss_alive) want = (want + 2) / 3;
    if (G.boss_alive && boss_stage()->parts == 0) want = 0;     /* its escort is the fight, and it is not to be lost in a swarm */
    return want;
}

/* The boss comes: what flew against the player thins out to what comes
 * with a boss, while its film looks at it - a formation and a big one go,
 * of the rest the nearest stay. Its own escort stays. Before, all of it
 * stayed and came at once when the film ended: a wall. */
static void thin_for_boss(void)
{
    int keep = hunters_wanted(), i;
    for (;;) {
        int far = -1, n = 0;
        float far_d = -1.0f;
        for (i = 0; i < MAX_OBJ; i++) {
            const obj *x = &G.objs[i];
            float d;
            if (x->kind != K_ENEMY || x->escort) continue;
            d = dist3(x->pos, G.p.pos) + (x->form || x->heavy ? 1e6f : 0.0f);      /* those go first, all of them */
            if (!x->form && !x->heavy) n++;
            if (d > far_d) { far_d = d; far = i; }
        }
        if (far < 0 || (n <= keep && far_d < 1e6f)) break;
        G.objs[far].kind = K_NONE;
    }
}

void run_spawning(void)
{
    int want = hunters_wanted(), hunters, formation;

    if (G.state == WARP) return;
    missile_clock();
    /* The boss shot down: once the last of its wreck has burst, the way
     * through time opens. Nothing more comes meanwhile. */
    if (G.boss_called && !G.boss_alive && count_kind(K_BOSS) == 0) {
        if (!boss_falling() && G.state == PLAYING) start_warp();
        return;
    }
    count_enemies(&hunters, &formation);
    if (!G.boss_called && (G.heavy_timer -= DT) <= 0) {
        int i, there = 0;
        for (i = 0; i < MAX_OBJ; i++) if (G.objs[i].kind == K_ENEMY && G.objs[i].heavy) there = 1;
        if (!there) spawn_heavy();
        G.heavy_timer = 22.0f + frand() * 12.0f;
    }
    if (!G.boss_called && (G.form_timer -= DT) <= 0 && formation == 0) {
        spawn_formation();
        G.form_timer = 16.0f + frand() * 12.0f;
    }
    G.spawn_timer -= DT;
    if (G.spawn_timer > 0) return;
    if (G.kills >= G.kills_per_era && !G.boss_called) { spawn_boss(); G.spawn_timer = 2.0f; return; }
    if (hunters < want) { spawn_enemy(); G.spawn_timer = G.boss_alive ? BOSS_SPAWN_EVERY : SPAWN_EVERY; }
}
