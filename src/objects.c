/* objects.c - the object pool and what is in it that is nobody's to fly:
 * the effects (sparks, rings of points, shock waves, the lines of things
 * burst), hits and their consequences, shots, bullets, bombs, missiles
 * on their way, wrecks coming down, items hanging, and the pass over
 * them all each step. */
#include "game_internal.h"

/* this file's own, in the order below */

static void throw_part(const obj *o, int model_index, int cut, vp_vec3 push, float scale);
static float hit_radius(const obj *o);
static int shoot_down(const obj *o);
static void enemy_down(obj *o);
static void site_shot(vp_vec3 at);
static void enemy_hit(obj *o, vp_vec3 at);
static void update_shot(obj *o);
static void update_bullet(obj *o);
static void update_chute(obj *o);
static void update_spark(obj *o);
static void update_score(obj *o);
static void update_wreck(obj *o);
static void update_part(obj *o);
static vp_vec3 spun(vp_vec3 v, vp_vec3 w)       /* v turned by w for one step */;
static void update_debris(obj *o);
static void update_bomb(obj *o);
static void update_item(obj *o);
static void update_pmissile(obj *o);
static void update_pring(obj *o);
static void update_ring(obj *o);

obj *new_obj(int kind)
{
    int i;
    if (kind == K_SPARK || kind == K_DEBRIS || kind == K_PRING) {
        /* and less than the ceiling as the beam comes near what it can do
         * in a frame: all of it up to six tenths of that, a third at the limit */
        float room = G.selftest ? 1.0f : clampf(1.75f - 1.25f * world_load(), 0.3f, 1.0f);   /* the self test: always the same game */
        if ((float)G.fx_count >= (float)FX_BUDGET * room) return NULL;
        G.fx_count += kind == K_PRING ? 4 : 1;
    }
    for (i = 0; i < MAX_OBJ; i++) {
        if (G.objs[i].kind == K_NONE) {
            obj *o = &G.objs[i];
            memset(o, 0, sizeof *o);
            o->kind = kind;
            return o;
        }
    }
    return NULL;
}

int count_kind(int kind)
{
    int i, n = 0;
    for (i = 0; i < MAX_OBJ; i++) if (G.objs[i].kind == kind) n++;
    return n;
}

/* The enemies that are out to get the player, and those of a formation. */

void count_enemies(int *hunters, int *formation)
{
    int i;
    *hunters = *formation = 0;
    for (i = 0; i < MAX_OBJ; i++) {
        if (G.objs[i].kind != K_ENEMY) continue;
        if (G.objs[i].form) (*formation)++;
        else if (!G.objs[i].heavy) (*hunters)++;
    }
}

void clear_kind(int kind)
{
    int i;
    for (i = 0; i < MAX_OBJ; i++) if (G.objs[i].kind == kind) G.objs[i].kind = K_NONE;
}

void clear_all(void)
{
    memset(G.objs, 0, sizeof G.objs);
}

void floating_score(vp_vec3 at, int points)
{
    obj *s = new_obj(K_SCORE);
    if (!s) return;
    s->pos = at;
    s->vel = vp_v3(0, 12.0f, 0);
    s->life = 1.2f;
    s->value = points;
}

/* Sparks of one kind: fire that falls, smoke that hangs, white heat. */

void burst(vp_vec3 at, vp_vec3 drift, int n, float size, float life, int type, int upward)
{
    int i;
    for (i = 0; i < n; i++) {
        obj *s = new_obj(K_SPARK);
        float a = frand() * PI2, b = (frand() - 0.5f) * PI;
        float v = size * (0.4f + frand() * 0.6f);
        if (!s) return;
        s->pos = at;
        s->vel = vp_v3(drift.x * 0.3f + cosf(a) * cosf(b) * v, drift.y * 0.3f + sinf(b) * v,
                       drift.z * 0.3f + sinf(a) * cosf(b) * v);
        if (upward) s->vel.y = fabsf(s->vel.y) * 1.5f;
        s->life = life * (0.6f + frand() * 0.4f);
        s->hp = type;
    }
}

/* A colour from round the colour wheel: h 0..1 and on. */

void hue_rgb(float h, int *r, int *g, int *b)
{
    float t = (h - floorf(h)) * 6.0f, f = t - floorf(t);
    int q = (int)(255.0f * f), p = 255 - q;
    switch ((int)t % 6) {
    case 0:  *r = 255; *g = q;   *b = 0;   break;
    case 1:  *r = p;   *g = 255; *b = 0;   break;
    case 2:  *r = 0;   *g = 255; *b = q;   break;
    case 3:  *r = 0;   *g = p;   *b = 255; break;
    case 4:  *r = q;   *g = 0;   *b = 255; break;
    default: *r = 255; *g = 0;   *b = p;   break;
    }
}

/* A ring of `n` points flying apart from `at` at `speed`, in a plane of
 * its own; its colours run round the wheel as it grows. Tempest 2000's
 * explosions are made of these. */

void point_ring(vp_vec3 at, vp_vec3 drift, int n, float speed, float life)
{
    obj *r = new_obj(K_PRING);
    vp_vec3 u, v;
    if (!r) return;
    u = vp_v3_normalize(vp_v3(frand() - 0.5f, frand() - 0.5f, frand() - 0.5f + 0.01f));
    v = vp_v3_normalize(vp_v3_cross(u, vp_v3(frand() - 0.5f, frand() - 0.5f + 0.01f, frand() - 0.5f)));
    r->pos = at;
    r->vel = vp_v3_scale(drift, 0.3f);
    r->offset = u;
    r->wp = v;
    r->hp = n;
    r->speed = speed;
    r->life = life;
    r->phase = frand();
}

void pixel_burst(vp_vec3 at, vp_vec3 drift, float size)
{
    point_ring(at, drift, 13, 60.0f * size, 0.8f);
    point_ring(at, drift, 11, 130.0f * size, 0.8f);
}

/* Four rings in four planes at one speed: a sphere of points. For the big. */

void sphere_burst(vp_vec3 at, vp_vec3 drift, float speed, float life)
{
    int i;
    for (i = 0; i < 4; i++) point_ring(at, drift, 12, speed, life);
}

/* A shock wave: a ring growing at `speed` metres a second, in the air
 * (it faces the viewer) or flat on the ground. */

void shock_ring(vp_vec3 at, float speed, float life, int ground)
{
    obj *r = new_obj(K_RING);
    if (!r) return;
    r->pos = at;
    if (ground) r->pos.y = GROUND_Y + 1.0f;
    r->speed = speed;
    r->life = life;
    r->cut = ground;
}

int keeps(int cut, const float *seg, float edge)
{
    float mid = (seg[0] + seg[3]) * 0.5f;
    switch (cut) {
    case CUT_ONLY_RIGHT: return mid > edge;
    case CUT_ONLY_LEFT:  return mid < -edge;
    case CUT_NOT_RIGHT:  return mid <= edge;
    case CUT_NOT_LEFT:   return mid >= -edge;
    case CUT_CENTRE:     return mid <= edge && mid >= -edge;
    }
    return 1;
}

/* Where a model's wing begins, for breaking it off: a third of the way out. */

float wing_edge(int model_index) { return model_halfspan(model_index) * 0.3f; }

/* The flyer bursts into its own lines: each flies off from the middle,
 * tumbling. `cut` picks which of them, one in `every` is taken. */

void shatter(const obj *o, float force, int cut, int every)
{
    const float *segs;
    int n = model_segments(o->model, &segs), i;
    vp_xf place = flyer_place(o);
    float edge = wing_edge(o->model);

    for (i = 0; i < n; i++) {
        const float *g = segs + 6 * i;
        vp_vec3 a, b, mid, out;
        obj *d;
        if (!keeps(cut, g, edge)) continue;
        if (every > 1 && rnd(every) != 0) continue;
        if (!(d = new_obj(K_DEBRIS))) return;
        a = vp_xf_point(&place, vp_v3(g[0], g[1], g[2]));
        b = vp_xf_point(&place, vp_v3(g[3], g[4], g[5]));
        mid = vp_v3_scale(vp_v3_add(a, b), 0.5f);
        out = vp_v3_add(vp_v3_sub(mid, o->pos), vp_v3(frand() - 0.5f, frand() - 0.5f, frand() - 0.5f));
        d->pos = mid;
        d->offset = vp_v3_sub(a, mid);
        d->wp = vp_v3_sub(b, mid);
        d->vel = vp_v3_add(vp_v3_scale(o->vel, 0.6f), vp_v3_scale(vp_v3_normalize(out), force * (0.4f + frand() * 0.8f)));
        d->ang = vp_v3((frand() - 0.5f) * 14.0f, (frand() - 0.5f) * 14.0f, (frand() - 0.5f) * 14.0f);
        d->life = 0.9f + frand() * 1.1f;
        d->model = o->model;
        {
            int cr, cg, cb;
            model_colour(o->model, model_segment_colour(o->model, i), o->heavy ? TINT_HEAVY : TINT_OWN, &cr, &cg, &cb);
            d->value = (cr << 16) | (cg << 8) | cb;
        }
    }
}

/* One line of wreckage from `a` to `b`, thrown with `vel` and tumbling,
 * in the colour `rgb` (0xRRGGBB), for `life` seconds. */
void debris_line(vp_vec3 a, vp_vec3 b, vp_vec3 vel, float life, int rgb)
{
    obj *d = new_obj(K_DEBRIS);
    vp_vec3 mid = vp_v3_scale(vp_v3_add(a, b), 0.5f);
    if (!d) return;
    d->pos = mid;
    d->offset = vp_v3_sub(a, mid);
    d->wp = vp_v3_sub(b, mid);
    d->vel = vel;
    d->ang = vp_v3((frand() - 0.5f) * 9.0f, (frand() - 0.5f) * 9.0f, (frand() - 0.5f) * 9.0f);
    d->life = life;
    d->value = rgb;
}

/* A piece comes off and tumbles away on its own: the lines `cut` picks
 * of the flyer's model, or a rotor. */

static void throw_part(const obj *o, int model_index, int cut, vp_vec3 push, float scale)
{
    obj *p = new_obj(K_PART);
    if (!p) return;
    p->model = model_index;
    p->cut = cut;
    p->pos = o->pos;
    p->heading = o->heading; p->pitch = o->pitch; p->roll = o->roll;
    p->vel = vp_v3_add(o->vel, push);
    p->ang = vp_v3((frand() - 0.5f) * 5.0f, (frand() - 0.5f) * 5.0f, (frand() < 0.5f ? -1.0f : 1.0f) * (5.0f + frand() * 5.0f));
    p->speed = scale;                   /* how big it is drawn */
    p->heavy = o->heavy;
    p->life = 6.0f;
}

/* --- shots, hits, pickups ------------------------------------------------------- */

/* How near `p` comes to the segment a-b. */

float segment_distance(vp_vec3 a, vp_vec3 b, vp_vec3 p)
{
    vp_vec3 ab = vp_v3_sub(b, a), ap = vp_v3_sub(p, a);
    float len2 = vp_v3_dot(ab, ab), t = len2 > 0 ? clampf(vp_v3_dot(ap, ab) / len2, 0, 1) : 0;
    return vp_v3_length(vp_v3_sub(p, vp_v3_add(a, vp_v3_scale(ab, t))));
}

float draw_scale(const obj *o)
{
    if (o->kind == K_PART) return o->speed;             /* it carries its own */
    if (o->size > 0) return o->size;                    /* one of the big ones, or what is left of one */
    if (o->kind == K_BOSS) return bosses[G.era].size;
    if (o->kind == K_WRECK) return o->boss ? bosses[G.era].size : ENEMY_SCALE;
    return o->kind == K_ENEMY ? ENEMY_SCALE : o->kind == K_MISSILE ? MISSILE_SCALE : 1.0f;
}

static float hit_radius(const obj *o)
{
    return model_radius(o->model) * draw_scale(o) * 0.85f + 1.5f;
}

/* The end of a flyer, never twice the same:
 *   it bursts into its own lines, with a shock wave;
 *   or goes up in a ball of fire and smoke;
 *   or loses a wing (a helicopter its rotor), which tumbles away, and
 *     spins down trailing smoke to burst on the ground;
 *   or spins down whole, on fire.
 * What goes down as a wreck is a new object: the enemy itself is gone. */

static int shoot_down(const obj *o)        /* 1: it comes down as a wreck */
{
    int how = o->heavy ? 45 + rnd(55) : rnd(100);        /* a big one always comes down as a wreck */
    int heli = o->model == MODEL_HELICOPTER || o->model == MODEL_BIGHELI || o->model == MODEL_GUNSHIP;

    burst(o->pos, o->vel, 4, 30.0f, 0.5f, SPARK_WHITE, 0);        /* the hit itself */
    pixel_burst(o->pos, o->vel, o->heavy ? 1.6f : 1.0f);         /* and always: rings of points, colours running */
    burst(o->pos, o->vel, 6, 55.0f, 0.9f, SPARK_RAINBOW, 0);
    if (o->heavy) { sphere_burst(o->pos, o->vel, 110.0f, 1.2f); cheer("1500"); }
    if (how < 28) {
        shatter(o, 55.0f, CUT_NONE, 1);
        burst(o->pos, o->vel, 6, 40.0f, 0.8f, SPARK_FIRE, 0);
        shock_ring(o->pos, 90.0f, 0.5f, 0);
    } else if (how < 45) {
        burst(o->pos, o->vel, 22, 60.0f, 1.1f, SPARK_FIRE, 0);
        burst(o->pos, o->vel, 10, 22.0f, 1.8f, SPARK_SMOKE, 0);
        shatter(o, 35.0f, CUT_NONE, 3);
        shock_ring(o->pos, 120.0f, 0.6f, 0);
        shock_ring(o->pos, 60.0f, 0.8f, 0);
    } else {
        obj *w = new_obj(K_WRECK);
        int broken = how < 78;
        if (!w) { shatter(o, 55.0f, CUT_NONE, 1); return 0; }
        w->model = o->model;
        w->pos = o->pos;
        w->vel = o->vel;
        w->heading = o->heading; w->pitch = o->pitch; w->roll = o->roll;
        w->spin = o->spin;
        w->side = frand() < 0.5f ? -1.0f : 1.0f;
        w->turn = w->side * (4.0f + frand() * 5.0f);            /* how fast it rolls: the spin */
        w->life = 9.0f;
        w->rotor = heli;
        w->heavy = o->heavy;
        w->size = draw_scale(o);
        if (o->heavy) {
            burst(o->pos, o->vel, 18, 70.0f, 1.2f, SPARK_FIRE, 0);
            shock_ring(o->pos, 130.0f, 0.7f, 0);
        }
        if (broken && heli) {                                   /* the rotor flies off, still turning */
            w->rotor = 0;
            throw_part(o, MODEL_ROTOR, CUT_NONE, vp_v3((frand() - 0.5f) * 30.0f, 25.0f, (frand() - 0.5f) * 30.0f),
                       (o->model == MODEL_BIGHELI ? 9.0f : eras[2].rotor_scale) * draw_scale(o));
        } else if (broken) {                                    /* a wing: the wreck rolls toward the stump */
            vp_vec3 right = vp_v3(cosf(o->heading), 0, -sinf(o->heading));
            int is_right = w->side > 0;
            w->cut = is_right ? CUT_NOT_RIGHT : CUT_NOT_LEFT;
            throw_part(o, o->model, is_right ? CUT_ONLY_RIGHT : CUT_ONLY_LEFT,
                       vp_v3_add(vp_v3_scale(right, w->side * 18.0f), vp_v3(0, 12.0f, 0)), draw_scale(o));
            w->turn = w->side * (7.0f + frand() * 4.0f);
            burst(o->pos, o->vel, 5, 25.0f, 0.6f, SPARK_FIRE, 0);
        }
        return 1;
    }
    return 0;
}

static void enemy_down(obj *o)
{
    int points = era()->points * (1 + G.loop);

    if (o->form) points *= 2;                            /* they do not fight back, but they do not come near */
    if (o->heavy) points = HEAVY_POINTS * (1 + G.loop);
    /* blown up: the blast; going down as a wreck: the crack of the hit,
     * then the howl of its dive, the blast where it lands (update_wreck) */
    if (shoot_down(o)) {
        sound_at(o->heavy ? FX_EXPLODE : FX_HIT_BOSS, o->pos);
        sound_at(FX_FALL, o->pos);
    } else {
        sound_at(FX_EXPLODE, o->pos);
    }
    add_score(points);
    floating_score(o->pos, points);
    if (o->form && o->form == G.form_id && ++G.form_killed == FORMATION_SIZE) {     /* the last of its five */
        int bonus = FORMATION_BONUS * (1 + G.loop);
        add_score(bonus);
        floating_score(vp_v3_add(o->pos, vp_v3(0, 14.0f, 0)), bonus);
        sound_fx(FX_BONUS);
        notice("FORMATION BONUS");
        cheer("2000");
    }
    if (rnd(CHUTE_CHANCE) == 0 && G.era != 4 && !(o->form && G.form_chute == o->form)) {     /* a parachutist (none from a saucer, one from a formation) */
        obj *c = new_obj(K_CHUTE);
        if (c) {
            if (o->form) G.form_chute = o->form;
            c->model = MODEL_PARACHUTE;
            c->pos = o->pos;
            c->vel = vp_v3((frand() - 0.5f) * 6.0f, -5.0f, (frand() - 0.5f) * 6.0f);
            c->life = 45.0f;
        }
    }
    G.kills += o->heavy ? HEAVY_KILLS : 1;      /* towards the boss: a big one counts for more */
    o->kind = K_NONE;
}

/* A shot of the player's has hit something built. If it is a depot or a
 * gun, that counts: a depot shot to pieces lets its item out, a gun is
 * worth points. Houses just stop the shot. */

static void site_shot(vp_vec3 at)
{
    city_site site;
    int i, slot = -1, need;

    if (!city_site_at(G.era, at, 2.0f, &site) || site.kind == CITY_GATE) return;
    for (i = 0; i < 24; i++)
        if (G.site_hits[i].hits > 0 && G.site_hits[i].cx == site.cx && G.site_hits[i].cz == site.cz) slot = i;
    if (slot < 0) {
        slot = G.site_hit_next = (G.site_hit_next + 1) % 24;
        G.site_hits[slot].cx = site.cx;
        G.site_hits[slot].cz = site.cz;
        G.site_hits[slot].hits = 0;
    }
    need = site.kind == CITY_DEPOT ? DEPOT_HITS : GUN_HITS;
    burst(at, vp_v3(0, 0, 0), 3, 25.0f, 0.4f, SPARK_FIRE, 1);
    if (++G.site_hits[slot].hits < need) { sound_at(FX_HIT_BOSS, at); return; }

    G.site_hits[slot].hits = 0;
    city_mark(site.cx, site.cz, CITY_GONE);
    burst(site.at, vp_v3(0, 0, 0), 16, 60.0f, 1.2f, SPARK_FIRE, 1);
    burst(site.at, vp_v3(0, 0, 0), 6, 20.0f, 2.0f, SPARK_SMOKE, 1);
    pixel_burst(site.at, vp_v3(0, 30.0f, 0), 1.0f);
    shock_ring(site.at, 70.0f, 0.8f, 1);
    sound_at(FX_EXPLODE, site.at);
    if (site.kind == CITY_GUN) {
        int points = GUN_POINTS * (1 + G.loop);
        add_score(points);
        floating_score(vp_v3_add(site.at, vp_v3(0, 12.0f, 0)), points);
        G.kills++;                              /* a gun shot to pieces counts towards the boss as a plane does */
    } else {                                    /* the item comes up out of the ruin */
        obj *it = new_obj(K_ITEM);
        if (it) {
            /* It stays where the depot stood, at the height of its roof:
             * it does not rise. The depot is gone, so the way to it is clear. */
            it->pos = site.at;
            it->wp = site.at;
            it->value = random_item();          /* only now is it known what was in there */
            it->life = 25.0f;
        }
    }
}

/* A shot lands on an enemy: the big ones take several. */

static void enemy_hit(obj *o, vp_vec3 at)
{
    if (o->heavy && --o->hp > 0) {
        o->flash = 6;
        sound_at(FX_HIT_BOSS, at);
        burst(at, o->vel, 4, 20.0f, 0.4f, SPARK_FIRE, 0);
        return;
    }
    enemy_down(o);
}

void boss_hit(obj *o, vp_vec3 at)
{
    o->flash = 6;
    sound_at(FX_HIT_BOSS, at);
    burst(at, o->vel, 3, 28.0f, 0.4f, SPARK_FIRE, 0);
    burst(at, o->vel, 1, 40.0f, 0.3f, SPARK_WHITE, 0);
    if (o->hp % 10 == 0) point_ring(at, o->vel, 11, 60.0f, 0.5f);        /* every tenth hit shows what it costs it */
    if (--o->hp > 0) return;
    /* It does not just vanish: it breaks up on its way down, burst after
     * burst, and goes in one last one (update_wreck). The way through
     * time opens when it is gone (run_spawning). */
    burst(o->pos, o->vel, 14, 90.0f, 1.4f, SPARK_FIRE, 0);
    burst(o->pos, o->vel, 10, 120.0f, 1.2f, SPARK_RAINBOW, 0);
    shock_ring(o->pos, 160.0f, 0.8f, 0);
    sphere_burst(o->pos, o->vel, 140.0f, 1.2f);
    cheer("EXCELLENT");
    add_score(era()->boss_points);
    floating_score(o->pos, era()->boss_points);
    sound_at(FX_BIG_EXPLODE, o->pos);
    o->kind = K_WRECK;
    o->boss = 1;
    o->life = 30.0f;                    /* it falls all the way to the ground */
    o->age = 0;
    o->flash = 0;
    o->rotor = o->model == MODEL_BIGHELI;
    o->side = frand() < 0.5f ? -1.0f : 1.0f;
    G.boss_alive = 0;
    sound_music_sting(MUSIC_VICTORY);
    sound_fx_ex(FX_FALL, 1.0f, 0.7f);           /* deeper: something that big going down */
    /* Its fall is watched: everything else of the era goes to pieces, the
     * shooting stops, and the view goes to the falling boss (BOSSFALL).
     * With the player's own plane just lost there is nobody to watch;
     * then it falls behind the game, and the tunnel opens when the next
     * plane is flying (run_spawning). */
    {
        int i;
        for (i = 0; i < MAX_OBJ; i++) {
            obj *x = &G.objs[i];
            if (x == o) continue;
            if (x->kind == K_ENEMY) { shatter(x, 45.0f, CUT_NONE, 1); pixel_burst(x->pos, x->vel, 1.0f); x->kind = K_NONE; }
            else if (x->kind == K_BULLET || x->kind == K_MISSILE || x->kind == K_BOMB) x->kind = K_NONE;
        }
    }
    if (G.p.alive && (G.state == PLAYING || G.state == INTRO)) {
        vp_vec3 away = vp_v3_sub(G.p.pos, o->pos);
        float d = vp_v3_length(away);
        G.fall_at = o->pos;
        /* the camera: toward the player from the boss, near enough to
         * fill the picture with it, a little above */
        G.fall_eye = vp_v3_add(o->pos, vp_v3_scale(away, (d > 1.0f ? 1.0f / d : 0) * clampf(d, 80.0f, 80.0f + 5.0f * model_radius(o->model))));     /* near enough to fill the picture */
        G.fall_eye.y += 30.0f;
        G.fall_after = 0;
        G.fall_down = 0;
        G.p.stalled = G.p.pull_up = 0;
        G.p.hurt = 0;
        enter(BOSSFALL);
    }
}

static void update_shot(obj *o)
{
    vp_vec3 from = o->pos;
    int i;

    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    if ((o->life -= DT) <= 0 || o->pos.y < GROUND_Y) { o->kind = K_NONE; return; }
    if (city_hit(G.era, o->pos, 0.5f)) { site_shot(o->pos); o->kind = K_NONE; return; }
    for (i = 0; i < MAX_OBJ; i++) {
        obj *t = &G.objs[i];
        if (t->kind == K_BULLET && G.era == 4) {         /* 2001: what the saucers shoot can be shot */
            if (segment_distance(from, o->pos, t->pos) > 4.0f) continue;
            burst(t->pos, vp_v3(0, 0, 0), 3, 15.0f, 0.3f, SPARK_WHITE, 0);
            t->kind = o->kind = K_NONE;
            return;
        }
        if (t->kind == K_CHUTE && !t->cut && t->age > CHUTE_SAFE) {
            /* Shot by mistake: the canopy folds, he falls screaming. The run
             * of parachutists is over; caught on the way down, he is worth
             * CHUTE_CATCH_POINTS all the same (update_chute). */
            if (segment_distance(from, o->pos, vp_v3_add(t->pos, vp_v3(0, CHUTE_SIZE * 4.0f, 0))) > CHUTE_SIZE * 5.5f) continue;
            t->cut = 1;
            t->life = 20.0f;
            burst(vp_v3_add(t->pos, vp_v3(0, CHUTE_SIZE * 7.0f, 0)), t->vel, 6, 18.0f, 0.5f, SPARK_WHITE, 0);
            G.chute_chain = 0;
            notice("PARACHUTE SHOT");
            sound_at(FX_SCREAM, t->pos);
            o->kind = K_NONE;
            return;
        }
        if (t->kind == K_BOSS) {                        /* its guns, or its hull: boss.c */
            if (!boss_take_shot(t, from, o->pos)) continue;
            o->kind = K_NONE;
            return;
        }
        if (t->kind != K_ENEMY && t->kind != K_MISSILE) continue;
        {
            float miss = segment_distance(from, o->pos, t->pos) - hit_radius(t);
            if (miss > 0) {
                if (miss < 28.0f && t->kind == K_ENEMY) t->scared = 1;      /* it went past close: the pilot saw it */
                continue;
            }
        }
        o->kind = K_NONE;
        if (t->kind == K_ENEMY) enemy_hit(t, o->pos);
        else {
            burst(t->pos, t->vel, 8, 30.0f, 0.5f, SPARK_FIRE, 0);
            point_ring(t->pos, t->vel, 11, 70.0f, 0.5f);
            add_score(MISSILE_POINTS * (1 + G.loop));
            floating_score(t->pos, MISSILE_POINTS * (1 + G.loop));
            sound_at(FX_EXPLODE, t->pos);
            t->kind = K_NONE;
        }
        return;
    }
}

static void update_bullet(obj *o)
{
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    if (o->cut == SHOT_ROCKET && G.frames % 3 == 0) burst(o->pos, vp_v3(0, 0, 0), 1, 3.0f, 0.5f, SPARK_SMOKE, 0);    /* its smoke */
    if ((o->life -= DT) <= 0 || o->pos.y < GROUND_Y || city_hit(G.era, o->pos, 0.5f)) { o->kind = K_NONE; return; }
    if (player_targetable() && dist3(o->pos, G.p.pos) < PLAYER_RADIUS + (o->size > 0 ? o->size : 1.5f)) {
        o->kind = K_NONE;
        if (o->size > 0) burst(o->pos, o->vel, 8, 35.0f, 0.5f, SPARK_FIRE, 0);
        damage_player((float)o->value);
    }
}

static void update_chute(obj *o)
{
    o->age += DT;
    if (o->cut) o->vel.y += (-70.0f - o->vel.y) * 0.03f;    /* the canopy gone: down, faster and faster */
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    if (o->cut && o->pos.y < GROUND_Y + 1.0f) {
        burst(o->pos, vp_v3(0, 0, 0), 5, 12.0f, 0.6f, SPARK_SMOKE, 1);
        o->kind = K_NONE;
        return;
    }
    if ((o->life -= DT) <= 0 || (!o->cut && o->pos.y < GROUND_Y + 8.0f) || dist3(o->pos, G.p.pos) > DESPAWN) { o->kind = K_NONE; return; }
    if (o->cut && player_targetable() && dist3(o->pos, G.p.pos) < 20.0f) {
        /* one that was shot, caught as he falls: the extra bonus */
        int points = CHUTE_CATCH_POINTS * (1 + G.loop);
        add_score(points);
        floating_score(o->pos, points);
        cheer("SAVED");
        sound_fx(FX_BONUS);
        o->kind = K_NONE;
        return;
    }
    if (!o->cut && player_targetable() && dist3(vp_v3_add(o->pos, vp_v3(0, CHUTE_SIZE * 4.0f, 0)), G.p.pos) < 20.0f) {
        /* 1000 for the first, 1000 more for each one after it, up to
         * 5000: until a plane is lost or the era left */
        int points = CHUTE_POINTS * (1 + G.chute_chain);
        if (points > CHUTE_POINTS_MAX) points = CHUTE_POINTS_MAX;
        G.chute_chain++;
        add_score(points);
        floating_score(o->pos, points);
        G.p.energy = clampf(G.p.energy + CHUTE_ENERGY, 0, ENERGY_FULL);
        sound_fx(FX_PICKUP);
        o->kind = K_NONE;
    }
}

static void update_spark(obj *o)
{
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    if (o->hp == SPARK_SMOKE) { o->vel = vp_v3_scale(o->vel, 0.95f); o->vel.y += 4.0f * DT; }   /* it hangs, and rises */
    else o->vel.y -= 25.0f * DT;
    o->vel = vp_v3_scale(o->vel, 0.985f);
    if ((o->life -= DT) <= 0) o->kind = K_NONE;
}

static void update_score(obj *o)
{
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    if ((o->life -= DT) <= 0) o->kind = K_NONE;
}

/* What comes down: a wreck spins about its length, noses over and turns
 * as it falls, trailing fire and smoke, and bursts where it lands. The
 * boss's wreck sinks slowly and breaks up as it goes. */

static void update_wreck(obj *o)
{
    o->age += DT;
    o->life -= DT;
    if (o->rotor) o->spin += 12.0f * DT;                 /* running down */
    if (o->boss) {
        /* The boss comes down the whole way. It noses over and rolls,
         * slowly at first and then faster; part after part of it goes in
         * bursts; a third of the way down one wing comes off, two thirds
         * down the other; fire and smoke stream from it; and where it
         * meets the ground it goes up in the biggest burst of the game. */
        float r = model_radius(o->model) * 0.6f, fallen = o->age;
        int burn = G.frames % 9 == 0;
        o->vel.y -= 16.0f * DT;
        o->vel.x *= 0.99f; o->vel.z *= 0.99f;
        o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
        o->roll += o->side * (0.3f + 0.5f * fallen) * DT;
        o->heading = wrap_angle(o->heading + o->side * 0.25f * DT);
        o->pitch += (-0.8f - o->pitch) * 0.25f * DT;
        G.fall_at = o->pos;
        if (G.frames % 3 == 0) burst(o->pos, vp_v3(0, 0, 0), 1, 14.0f, 0.8f, SPARK_FIRE, 0);
        if (G.frames % 6 == 0) burst(o->pos, vp_v3(0, 0, 0), 1, 8.0f, 1.6f, SPARK_SMOKE, 0);
        if (burn) {                                     /* another part of it goes */
            vp_vec3 at = vp_v3_add(o->pos, vp_v3((frand() - 0.5f) * 2.0f * r, (frand() - 0.5f) * r, (frand() - 0.5f) * 2.0f * r));
            burst(at, o->vel, 5, 45.0f, 0.8f, SPARK_FIRE, 0);
            burst(at, o->vel, 3, 60.0f, 0.7f, SPARK_RAINBOW, 0);
            shatter(o, 30.0f, o->cut, 20);
            if (G.frames % 27 == 0) { pixel_burst(at, o->vel, 0.9f); shock_ring(at, 70.0f, 0.5f, 0); sound_at(FX_EXPLODE, at); }
        }
        if (o->cut == CUT_NONE && fallen > 1.2f) {      /* the first wing */
            vp_vec3 right = vp_v3(cosf(o->heading), 0, -sinf(o->heading));
            throw_part(o, o->model, CUT_ONLY_RIGHT, vp_v3_add(vp_v3_scale(right, 25.0f), vp_v3(0, 18.0f, 0)), 1.0f);
            o->cut = CUT_NOT_RIGHT;
            sphere_burst(o->pos, o->vel, 80.0f, 1.0f);
            sound_at(FX_BIG_EXPLODE, o->pos);
        } else if (o->cut == CUT_NOT_RIGHT && fallen > 2.6f) {      /* and the other */
            vp_vec3 right = vp_v3(cosf(o->heading), 0, -sinf(o->heading));
            throw_part(o, o->model, CUT_ONLY_LEFT, vp_v3_add(vp_v3_scale(right, -25.0f), vp_v3(0, 18.0f, 0)), 1.0f);
            o->cut = CUT_CENTRE;
            sphere_burst(o->pos, o->vel, 80.0f, 1.0f);
            sound_at(FX_BIG_EXPLODE, o->pos);
        }
        if (o->life <= 0 || o->pos.y < GROUND_Y + r * 0.4f) {       /* the ground */
            vp_vec3 up = vp_v3(0, 40.0f, 0);
            int i;
            o->pos.y = GROUND_Y + 4.0f;
            o->vel = vp_v3(o->vel.x * 0.2f, 0, o->vel.z * 0.2f);
            shatter(o, 110.0f, o->cut, 2);                          /* every second line of what is left */
            burst(o->pos, up, 30, 150.0f, 2.2f, SPARK_FIRE, 1);
            burst(o->pos, up, 22, 190.0f, 2.0f, SPARK_RAINBOW, 1);
            burst(o->pos, up, 8, 80.0f, 1.2f, SPARK_WHITE, 1);
            burst(o->pos, up, 8, 30.0f, 3.0f, SPARK_SMOKE, 1);
            shock_ring(o->pos, 320.0f, 1.3f, 1);
            shock_ring(o->pos, 200.0f, 1.6f, 1);
            shock_ring(o->pos, 110.0f, 2.0f, 1);
            shock_ring(o->pos, 260.0f, 0.9f, 0);
            sphere_burst(o->pos, up, 110.0f, 1.6f);
            sphere_burst(o->pos, up, 230.0f, 1.6f);
            for (i = 0; i < 2; i++)                                 /* and smaller ones round it */
                pixel_burst(vp_v3_add(o->pos, vp_v3((frand() - 0.5f) * 3.0f * r, 10.0f + frand() * 30.0f, (frand() - 0.5f) * 3.0f * r)),
                            up, 0.8f + frand() * 0.6f);
            sound_at(FX_BIG_EXPLODE, o->pos);
            sound_fx(FX_DEATH);
            G.fall_down = 1;
            G.p.shake = 1.2f;
            o->kind = K_NONE;
        }
        return;
    }
    o->vel.y -= 28.0f * DT;
    o->vel.x *= 0.995f; o->vel.z *= 0.995f;
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    o->roll += o->turn * DT;
    o->heading = wrap_angle(o->heading + o->side * 1.3f * DT);
    o->pitch += (-1.15f - o->pitch) * 0.9f * DT;
    if (G.frames % 3 == 0) burst(o->pos, vp_v3(0, 0, 0), 1, 6.0f, 0.5f, SPARK_FIRE, 0);
    if (G.frames % 4 == 0) burst(o->pos, vp_v3(0, 0, 0), 1, 4.0f, 1.6f, SPARK_SMOKE, 0);
    if (o->pos.y <= GROUND_Y + 2.0f || city_hit(G.era, o->pos, 3.0f)) {
        int ground = o->pos.y <= GROUND_Y + 2.0f;
        if (ground) o->pos.y = GROUND_Y + 2.0f;
        o->vel = vp_v3(o->vel.x * 0.3f, 0, o->vel.z * 0.3f);
        shatter(o, 40.0f, o->cut, 1);
        burst(o->pos, o->vel, 16, 55.0f, 1.2f, SPARK_FIRE, 1);
        burst(o->pos, o->vel, 6, 14.0f, 2.2f, SPARK_SMOKE, 1);
        burst(o->pos, o->vel, 10, 70.0f, 1.0f, SPARK_RAINBOW, 1);
        pixel_burst(vp_v3_add(o->pos, vp_v3(0, 6.0f, 0)), vp_v3(0, 30.0f, 0), o->heavy ? 1.5f : 1.0f);
        shock_ring(o->pos, 70.0f, 0.9f, ground);
        if (ground) shock_ring(o->pos, 35.0f, 1.3f, 1);
        if (dist3(o->pos, G.p.pos) < 900.0f) sound_at(FX_EXPLODE, o->pos);
        o->kind = K_NONE;
    } else if (o->life <= 0) {
        shatter(o, 45.0f, o->cut, 1);
        o->kind = K_NONE;
    }
}

/* A piece broken off: it tumbles and falls, and is gone where it lands. */

static void update_part(obj *o)
{
    o->vel.y -= 22.0f * DT;
    o->vel = vp_v3_scale(o->vel, 0.995f);
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    o->heading += o->ang.y * DT;
    o->pitch += o->ang.x * DT;
    o->roll += o->ang.z * DT;
    o->spin += 25.0f * DT;
    if ((o->life -= DT) <= 0 || o->pos.y <= GROUND_Y + 1.0f || city_hit(G.era, o->pos, 1.0f)) {
        burst(o->pos, vp_v3(0, 0, 0), 5, 25.0f, 0.6f, SPARK_FIRE, 1);
        o->kind = K_NONE;
    }
}

static vp_vec3 spun(vp_vec3 v, vp_vec3 w)       /* v turned by w for one step */
{
    return vp_v3_add(v, vp_v3_scale(vp_v3_cross(w, v), DT));
}

/* One line of a burst model: it flies, falls and tumbles about its middle. */

static void update_debris(obj *o)
{
    o->vel.y -= 20.0f * DT;
    o->vel = vp_v3_scale(o->vel, 0.99f);
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    o->offset = spun(o->offset, o->ang);
    o->wp = spun(o->wp, o->ang);
    if ((o->life -= DT) <= 0 || o->pos.y < GROUND_Y) o->kind = K_NONE;
}

/* A bomb: up, over and down, tumbling; it bursts on the plane, on the
 * ground or on a building. */

static void update_bomb(obj *o)
{
    o->age += DT;
    o->vel.y -= 30.0f * DT;
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    if (o->age > 0.5f && player_targetable() && dist3(o->pos, G.p.pos) < PLAYER_RADIUS + 4.0f) {
        burst(o->pos, o->vel, 8, 35.0f, 0.6f, SPARK_FIRE, 0);
        o->kind = K_NONE;
        damage_player((float)o->value);
        return;
    }
    if ((o->life -= DT) <= 0 || o->pos.y <= GROUND_Y + 1.0f || city_hit(G.era, o->pos, 1.0f)) {
        burst(o->pos, vp_v3(0, 0, 0), 8, 30.0f, 0.7f, SPARK_FIRE, 1);
        if (o->pos.y <= GROUND_Y + 1.0f) shock_ring(o->pos, 30.0f, 0.6f, 1);
        o->kind = K_NONE;
    }
}

/* An item out of a depot: it hangs where the depot's roof was, turning,
 * and is the player's when the plane comes through it. */

static void update_item(obj *o)
{
    o->age += DT;
    o->pos.y += clampf(o->wp.y - o->pos.y, 0, 30.0f * DT);
    if ((o->life -= DT) <= 0) { o->kind = K_NONE; return; }
    if (player_targetable() && dist3(o->pos, G.p.pos) < 26.0f) {
        grant_item(o->value);
        o->kind = K_NONE;
    }
}

/* One of the player's missiles: after whatever is nearest its nose, and
 * into it. An enemy goes down; a big one or a boss takes it as six shots. */

static void update_pmissile(obj *o)
{
    vp_vec3 fwd = forward_of(o);
    obj *best = NULL;
    float best_dot = 0.5f;
    int i, k;

    o->age += DT;
    if ((o->life -= DT) <= 0 || o->pos.y < GROUND_Y || city_hit(G.era, o->pos, 1.0f)) {
        burst(o->pos, vp_v3(0, 0, 0), 5, 25.0f, 0.5f, SPARK_FIRE, 0);
        o->kind = K_NONE;
        return;
    }
    for (i = 0; i < MAX_OBJ; i++) {
        obj *t = &G.objs[i];
        vp_vec3 to;
        float d, dot;
        if (t->kind != K_ENEMY && t->kind != K_BOSS) continue;
        to = vp_v3_sub(t->pos, o->pos);
        d = vp_v3_length(to);
        if (d < hit_radius(t) + 5.0f) {             /* there */
            burst(o->pos, t->vel, 10, 50.0f, 0.7f, SPARK_FIRE, 0);
            pixel_burst(o->pos, t->vel, 0.8f);
            sound_at(FX_EXPLODE, o->pos);
            o->kind = K_NONE;
            if (t->kind == K_BOSS) boss_take_blast(t, o->pos, 6);
            else for (k = 0; k < 6 && t->kind == K_ENEMY; k++) enemy_hit(t, o->pos);
            return;
        }
        if (d > 900.0f) continue;
        dot = vp_v3_dot(fwd, vp_v3_scale(to, 1.0f / d));
        if (dot > best_dot) { best_dot = dot; best = t; }
    }
    if (best) steer_toward(o, best->pos, o->turn, 1.3f);
    o->roll = 0;
    o->vel = vp_v3_scale(forward_of(o), o->speed);
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    if (G.frames % 3 == 0) burst(o->pos, vp_v3(0, 0, 0), 1, 3.0f, 0.3f, SPARK_WHITE, 0);
}

static void update_pring(obj *o)
{
    o->age += DT;
    o->pos = vp_v3_add(o->pos, vp_v3_scale(o->vel, DT));
    if ((o->life -= DT) <= 0) o->kind = K_NONE;
}

static void update_ring(obj *o)
{
    o->age += DT;
    if ((o->life -= DT) <= 0) o->kind = K_NONE;
}

/* The player against what flies: the enemy flown into goes down, and the
 * plane pays for it with more energy than a shot costs. */

void check_collisions(void)
{
    int i;
    if (!player_targetable() || G.p.shield > 0 || G.p.ram_cd > 0) return;
    for (i = 0; i < MAX_OBJ; i++) {
        obj *o = &G.objs[i];
        if (o->kind != K_ENEMY && o->kind != K_BOSS && o->kind != K_MISSILE) continue;
        if (o->kind == K_BOSS ? !boss_touches(o, G.p.pos, PLAYER_RADIUS * 0.6f)
                              : dist3(o->pos, G.p.pos) > hit_radius(o) + PLAYER_RADIUS * 0.6f) continue;
        G.p.ram_cd = 0.8f;
        if (o->kind == K_ENEMY) { enemy_hit(o, G.p.pos); damage_player(DMG_RAM); }
        else if (o->kind == K_MISSILE) { burst(o->pos, o->vel, 5, 25.0f, 0.5f, SPARK_FIRE, 0); o->kind = K_NONE; damage_player(DMG_MISSILE); }
        else {
            /* The boss's last hit point taken by flying into it starts the
             * journey: the plane comes through, as it does in the original. */
            boss_take_blast(o, G.p.pos, 1);
            if (G.boss_alive) {
                burst(G.p.pos, vp_v3(0, 0, 0), 10, 40.0f, 0.5f, SPARK_WHITE, 0);
                sound_fx(FX_HIT_BOSS);
                boss_push_off(o, &G.p.pos);
                G.p.shake = clampf(G.p.shake + 0.8f, 0, 1.2f);
                damage_player(DMG_RAM_BOSS);
                G.p.ram_cd = RAM_BOSS_REST;
            }
        }
        return;
    }
}

void update_objects(void)
{
    int i;
    /* what there is for show, counted afresh (new_obj) */
    G.fx_count = 0;
    for (i = 0; i < MAX_OBJ; i++) {
        int k = G.objs[i].kind;
        if (k == K_SPARK || k == K_DEBRIS) G.fx_count++;
        else if (k == K_PRING) G.fx_count += 4;
    }
    for (i = 0; i < MAX_OBJ; i++) {
        obj *o = &G.objs[i];
        switch (o->kind) {
        case K_ENEMY:   update_enemy(o);   break;
        case K_BOSS:    update_boss(o);    break;
        case K_MISSILE: update_missile(o); break;
        case K_BULLET:  update_bullet(o);  break;
        case K_SHOT:    update_shot(o);    break;
        case K_CHUTE:   update_chute(o);   break;
        case K_SPARK:   update_spark(o);   break;
        case K_SCORE:   update_score(o);   break;
        case K_WRECK:   update_wreck(o);   break;
        case K_PART:    update_part(o);    break;
        case K_DEBRIS:  update_debris(o);  break;
        case K_RING:    update_ring(o);    break;
        case K_BOMB:    update_bomb(o);    break;
        case K_PRING:   update_pring(o);   break;
        case K_ITEM:    update_item(o);    break;
        case K_PMISSILE: update_pmissile(o); break;
        }
    }
}
