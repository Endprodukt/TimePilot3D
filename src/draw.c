/* draw.c - drawing the play: the sky and everything in it, the plane and
 * its cockpit, the game's own display, the boss's fall and the journey
 * through time. Board units for what lies flat on the picture. */
#include "game_internal.h"
#include "setup.h"

/* this file's own, in the order below */
static void board_dot(float x, float y);
static int flyer_tint(const obj *o);
static void draw_flyer(const obj *o);
static void draw_cut(const obj *o, int tint);
static void draw_wreckage(vp_vec3 eye);
static void draw_sites(vp_vec3 eye);
static void draw_items_hud(void);
static void draw_boss_parts(const obj *o, const vp_xf *place);
static void draw_boss_parts_on(const obj *o, const vp_xf *place);
static void draw_ball(vp_vec3 at, float r, vp_vec3 eye, const int rim[3], const int core[3], float spin, int fine);
static void draw_chute(const obj *o, const vp_xf *place);
static void draw_objects(void);
static void draw_player_plane(void);
static void draw_pipper(void);
static void draw_plane_hud(int cockpit);
static void draw_radar(void);
static void draw_hud(void);
static void draw_cheer(void);
static void draw_scene(int era_index, float grow);
static void draw_practice(void);
static int set_camera(void);
static void draw_bossfall(void);
static void draw_bossintro(void);
static void draw_energy(float y);
static void draw_portal(float t);
static void draw_levelin(void);
static void letterbox_lines(float low, float high);
static void draw_warp(void);

/* Text in board units; what stands for hours in attract mode drifts a few
 * units round, slowly, so no line burns into the tube. */

void text(const char *s, float x, float y, float height, int align, int r, int g, int b)
{
    if (G.state == ATTRACT || G.demo) {
        x += 50.0f * sinf((float)G.frames * 0.0031f);
        y += 35.0f * sinf((float)G.frames * 0.0047f);
    }
    world_level(G.bright[BRIGHT_TEXT]);
    world_rgb(r, g, b);
    world_text(s, x, y, height, align);
}

static void board_dot(float x, float y) { world_bline(x, y, x, y); }

void draw_rotors(const obj *o, const vp_xf *body)
{
    float r = eras[2].rotor_scale;
    vp_xf rotor;

    if (o->model == MODEL_HELICOPTER) {
        rotor = vp_xf_mul2(*body, vp_xf_mul2(vp_xf_translate(0, 2.1f, 0),
                vp_xf_mul2(vp_xf_rot_y(o->spin), vp_xf_scale(r, 1, r))));
        model_draw(MODEL_ROTOR, &rotor, TINT_OWN);
    } else if (o->model == MODEL_GUNSHIP) {
        rotor = vp_xf_mul2(*body, vp_xf_mul2(vp_xf_translate(0, 1.9f, -0.4f),
                vp_xf_mul2(vp_xf_rot_y(o->spin), vp_xf_scale(r * 1.05f, 1, r * 1.05f))));
        model_draw(MODEL_ROTOR, &rotor, TINT_OWN);
    } else if (o->model == MODEL_BIGHELI) {
        r = 9.0f;
        rotor = vp_xf_mul2(*body, vp_xf_mul2(vp_xf_translate(0, 4.6f, 7.5f),
                vp_xf_mul2(vp_xf_rot_y(o->spin), vp_xf_scale(r, 1, r))));
        model_draw(MODEL_ROTOR, &rotor, TINT_OWN);
        rotor = vp_xf_mul2(*body, vp_xf_mul2(vp_xf_translate(0, 6.9f, -9.0f),
                vp_xf_mul2(vp_xf_rot_y(-o->spin + 0.8f), vp_xf_scale(r, 1, r))));
        model_draw(MODEL_ROTOR, &rotor, TINT_OWN);
    }
}

/* Where a flyer's model stands in the world, at the size it is drawn. */

vp_xf flyer_place(const obj *o)
{
    vp_xf place = world_place(o->pos, o->heading, o->pitch, o->roll);
    float s = draw_scale(o);
    /* saucers spin, and do not bank (shot down, they tumble like the rest) */
    if ((o->model == MODEL_UFO || o->model == MODEL_MOTHERSHIP) && (o->kind == K_ENEMY || o->kind == K_BOSS))
        place = world_place(o->pos, o->spin * 0.5f, 0, 0);
    if (s != 1.0f) place = vp_xf_mul2(place, vp_xf_scale(s, s, s));
    return place;
}

/* How a flyer is coloured: white for a moment when hit, the big ones red
 * and yellow, the rest in their own colours. */

static int flyer_tint(const obj *o)
{
    return o->flash > 0 ? TINT_WHITE : o->heavy ? TINT_HEAVY : o->escort ? TINT_ESCORT : TINT_OWN;
}

/* The mothership on its way to being the robot (boss_morph, 0..1): every
 * line of the robot comes from a line of the saucer and goes to its
 * place, slow at both ends and white hot on the way. They go in turn:
 * the legs are let down first, then the body, the arms and shoulders,
 * the head and its fins last. Before its turn a line is the saucer's,
 * after it the robot's. */
static void draw_morph(const obj *o, const vp_xf *place)
{
    const float *from, *to;
    int nf = model_segments(MODEL_MOTHERSHIP, &from), nt = model_segments(MODEL_ROBOT, &to), i;
    float u = G.boss_morph;

    world_level(G.bright[BRIGHT_ENEMIES]);
    if (nf <= 0 || nt <= 0) return;
    for (i = 0; i < nt; i++) {
        int src = (i * nf) / nt;
        const float *g = to + 6 * i, *s = from + 6 * src;
        float my = (g[1] + g[4]) * 0.5f, mx = fabsf(g[0] + g[3]) * 0.5f;
        float delay = my < -5.0f ? 0 : mx > 9.0f ? 0.36f : my > 16.0f ? 0.56f : 0.18f;
        float k = smooth(clampf((u - delay) / 0.42f, 0, 1)), hot = sinf(k * 3.14159f) * 0.7f;
        int r0, g0, b0, r1, g1, b1;
        if (k <= 0 && i > 0 && ((i - 1) * nf) / nt == src) continue;        /* the saucer's line once, not once for each that will come of it */
        model_colour(MODEL_MOTHERSHIP, model_segment_colour(MODEL_MOTHERSHIP, src), TINT_OWN, &r0, &g0, &b0);
        model_colour(MODEL_ROBOT, model_segment_colour(MODEL_ROBOT, i), TINT_OWN, &r1, &g1, &b1);
        if (o->flash > 0) world_rgb(255, 255, 255);
        else {
            float r = (float)r0 + (float)(r1 - r0) * k, gg = (float)g0 + (float)(g1 - g0) * k, b = (float)b0 + (float)(b1 - b0) * k;
            world_rgb((int)(r + (255.0f - r) * hot), (int)(gg + (255.0f - gg) * hot), (int)(b + (255.0f - b) * hot));
        }
        world_line(vp_xf_point(place, vp_v3(s[0] + (g[0] - s[0]) * k, s[1] + (g[1] - s[1]) * k, s[2] + (g[2] - s[2]) * k)),
                   vp_xf_point(place, vp_v3(s[3] + (g[3] - s[3]) * k, s[4] + (g[4] - s[4]) * k, s[5] + (g[5] - s[5]) * k)));
    }
}

/* The robot, limb by limb (boss.c: robot_point): its arms where they are
 * aimed, one that has been shot off falling away from its shoulder and
 * one that is gone left out, its legs in their step. An arm that has
 * just been hit is white. */
static void draw_robot(const obj *o, const vp_xf *place)
{
    const float *g;
    int n = model_segments(MODEL_ROBOT, &g), tint = flyer_tint(o), i;

    world_level(G.bright[BRIGHT_ENEMIES]);
    for (i = 0; i < n; i++) {
        const float *q = g + 6 * i;
        int limb = model_robot_limb(i), side = limb - LIMB_ARM_R, arm = limb == LIMB_ARM_R || limb == LIMB_ARM_L, cr, cg, cb;
        vp_vec3 a, b;
        if (arm && G.arm_state[side] == 2) continue;
        a = vp_xf_point(place, robot_point(limb, vp_v3(q[0], q[1], q[2])));
        b = vp_xf_point(place, robot_point(limb, vp_v3(q[3], q[4], q[5])));
        if (arm && G.arm_state[side] == 1) { a = vp_v3_add(a, G.arm_off[side]); b = vp_v3_add(b, G.arm_off[side]); }
        model_colour(MODEL_ROBOT, model_segment_colour(MODEL_ROBOT, i),
                     arm && G.boss_stage == 2 && G.arm_state[side] == 0 && G.part_flash[side] > 0 ? TINT_WHITE : tint, &cr, &cg, &cb);
        world_rgb(cr, cg, cb);
        world_line(a, b);
    }
    if (world_solid()) {
        /* its faces, limb by limb, as its lines */
        world_group();
        for (i = 0; i < model_face_count(MODEL_ROBOT); i++) {
            const float (*c)[3];
            int tag, k, m = model_face(MODEL_ROBOT, i, &c, &tag), side = tag - LIMB_ARM_R;
            int arm = tag == LIMB_ARM_R || tag == LIMB_ARM_L;
            vp_vec3 p[8];
            if (arm && G.arm_state[side] == 2) continue;
            for (k = 0; k < m; k++) {
                p[k] = vp_xf_point(place, robot_point(tag, vp_v3(c[k][0], c[k][1], c[k][2])));
                if (arm && G.arm_state[side] == 1) p[k] = vp_v3_add(p[k], G.arm_off[side]);
            }
            world_face(p, m);
        }
    }
}

static void draw_flyer(const obj *o)
{
    vp_xf place = flyer_place(o);
    if (o->kind == K_BOSS && G.state == BOSSTURN && G.era == 4 && G.boss_stage == 2 && o->model != MODEL_ROBOT) {
        draw_morph(o, &place);          /* neither saucer nor robot yet, and nothing on it to shoot at */
        return;
    }
    if (o->kind == K_BOSS && o->model == MODEL_ROBOT) draw_robot(o, &place); else
    model_draw(o->model, &place, flyer_tint(o));
    draw_rotors(o, &place);
    if (o->kind == K_BOSS && !G.boss_one_hit) draw_boss_parts(o, &place);
}

/* A shot is never smaller on the screen than `k` of its distance: far
 * off it is drawn bigger than it is, so that it is seen coming. What it
 * hits with stays its own size. */
static float seen(float r, vp_vec3 at, vp_vec3 eye, float k)
{
    float least = dist3(at, eye) * k;
    return r > least ? r : least;
}

/* A square round `w` that faces the eye and beats: what to shoot at. */
static void mark_weak(vp_vec3 w, float half)
{
    world_set_nocut(1);             /* what is to be shot at is seen through whatever is before it */
    vp_vec3 eye = vp3d_eye(world_view());
    vp_vec3 look = vp_v3_normalize(vp_v3_sub(eye, w));
    vp_vec3 br = vp_v3_normalize(vp_v3_cross(fabsf(look.y) > 0.97f ? vp_v3(1, 0, 0) : vp_v3(0, 1, 0), look));
    vp_vec3 bu = vp_v3_cross(look, br);
    float q = half * (1.0f + 0.25f * sinf((float)G.frames * 0.25f));
    br = vp_v3_scale(br, q);
    bu = vp_v3_scale(bu, q);
    world_rgb(255, 220, 0);
    world_line(vp_v3_add(w, vp_v3_add(br, bu)), vp_v3_add(w, vp_v3_sub(br, bu)));
    world_line(vp_v3_add(w, vp_v3_sub(br, bu)), vp_v3_sub(w, vp_v3_add(br, bu)));
    world_line(vp_v3_sub(w, vp_v3_add(br, bu)), vp_v3_add(w, vp_v3_sub(bu, br)));
    world_line(vp_v3_add(w, vp_v3_sub(bu, br)), vp_v3_add(w, vp_v3_add(br, bu)));
    world_set_nocut(0);
}

/* What the stage of the boss's fight has on it, part by part, each as
 * what it is; what is coming out in the film between stages is drawn as
 * far out as it has come (boss_deploy). Then the shield with its
 * stations, and the laser. */
static void draw_boss_parts(const obj *o, const vp_xf *place)
{
    draw_boss_parts_on(o, place);   /* the parts are solid as the rest; their marks are always seen (mark_weak) */
}

static void draw_boss_parts_on(const obj *o, const vp_xf *place)
{
    const struct boss_stage *st = boss_stage();
    const float size = bosses[G.era].size, k = G.boss_deploy;
    int i, j;

#define P(x, y, z) vp_xf_point(place, vp_v3((x), (y), (z)))
    world_level(G.bright[BRIGHT_ENEMIES]);
    {
        /* Engines that a later stage is about are on it already, running,
         * in grey and without a mark: they cannot be hurt yet. */
        int later;
        for (later = G.boss_stage + 1; later < bosses[G.era].stages; later++) {
            const struct boss_stage *ls = &bosses[G.era].stage[later];
            for (i = 0; i < ls->parts; i++) {
                const struct boss_part *p = &ls->part[i];
                float r = p->r, turn = (float)G.frames * 0.6f;
                vp_vec3 a[6], b[6];
                if (p->look != LOOK_ENGINE) continue;
                world_rgb(150, 150, 160);
                for (j = 0; j < 6; j++) {
                    float an = PI2 * (float)j / 6.0f;
                    a[j] = P(p->x + r * 0.7f * cosf(an), p->y + r * 0.7f * sinf(an), p->z + r);
                    b[j] = P(p->x + r * 0.5f * cosf(an), p->y + r * 0.5f * sinf(an), p->z - r);
                }
                for (j = 0; j < 6; j++) { world_line(a[j], a[(j + 1) % 6]); world_line(a[j], b[j]); }
                world_line(P(p->x + r * 1.3f * cosf(turn), p->y + r * 1.3f * sinf(turn), p->z + r * 1.1f),
                           P(p->x - r * 1.3f * cosf(turn), p->y - r * 1.3f * sinf(turn), p->z + r * 1.1f));
                world_line(P(p->x, p->y, p->z), P(p->x * 0.4f, p->y * 0.5f, p->z));
            }
        }
    }
    if (G.warn_t > 0 && G.state == PLAYING) {
        /* A salvo is about to come from here: a ring that closes on the
         * muzzle and rays through it, white and red in turn, never small
         * on the screen. */
        vp_vec3 eye = vp3d_eye(world_view()), at = G.warn_at;
        float d = dist3(at, eye), rr = seen(7.0f, at, eye, 0.05f) * (0.45f + 0.55f * clampf(G.warn_t / SALVO_WARN, 0, 1));
        vp_vec3 look = vp_v3_scale(vp_v3_sub(eye, at), d > 1.0f ? 1.0f / d : 0);
        vp_vec3 br = vp_v3_normalize(vp_v3_cross(fabsf(look.y) > 0.97f ? vp_v3(1, 0, 0) : vp_v3(0, 1, 0), look));
        vp_vec3 bu = vp_v3_cross(look, br);
        world_level(G.bright[BRIGHT_SHOTS]);
        world_set_nocut(1);             /* a warning is always seen */
        if ((G.frames / 2) & 1) world_rgb(255, 255, 255); else world_rgb(255, 50, 30);
        for (j = 0; j < 8; j++) {
            float a0 = PI2 * (float)j / 8.0f, a1 = PI2 * (float)(j + 1) / 8.0f;
            world_line(vp_v3_add(at, vp_v3_add(vp_v3_scale(br, rr * cosf(a0)), vp_v3_scale(bu, rr * sinf(a0)))),
                       vp_v3_add(at, vp_v3_add(vp_v3_scale(br, rr * cosf(a1)), vp_v3_scale(bu, rr * sinf(a1)))));
        }
        for (j = 0; j < 4; j++) {
            float a = PI2 * (float)j / 4.0f + 0.785f;
            vp_vec3 dir = vp_v3_add(vp_v3_scale(br, cosf(a)), vp_v3_scale(bu, sinf(a)));
            world_line(vp_v3_add(at, vp_v3_scale(dir, rr * 1.2f)), vp_v3_add(at, vp_v3_scale(dir, rr * 1.9f)));
        }
        world_set_nocut(0);
        world_level(G.bright[BRIGHT_ENEMIES]);
    }
    if (G.boss_stage > 0 && bosses[G.era].stage[G.boss_stage - 1].part[0].look == LOOK_LASER) {
        /* The laser turret of the stage before. In the film until it
         * blows (BAY_BLOWS): still there, white and orange in turn,
         * shaking. After: its ring, bent, and two stumps. */
        const struct boss_part *p = &bosses[G.era].stage[G.boss_stage - 1].part[0];
        float r = p->r, w = r * 0.85f;
        if (G.state == BOSSTURN && G.state_time < BAY_BLOWS) {
            float shake = 0.5f * sinf((float)G.frames * 2.3f);
            vp_vec3 top = P(p->x + shake, p->y, p->z);
            if ((G.frames / 3) & 1) world_rgb(255, 255, 255); else world_rgb(255, 120, 30);
            for (j = 0; j < 8; j++) {
                float a0 = PI2 * (float)j / 8.0f, a1 = PI2 * (float)(j + 1) / 8.0f;
                world_line(P(p->x + w * cosf(a0), p->y - r, p->z + w * sinf(a0)), P(p->x + w * cosf(a1), p->y - r, p->z + w * sinf(a1)));
                if (!(j & 1)) world_line(P(p->x + w * cosf(a0), p->y - r, p->z + w * sinf(a0)), top);
            }
        } else {
            world_rgb(120, 120, 130);
            for (j = 0; j < 8; j++) {
                float a0 = PI2 * (float)j / 8.0f, a1 = PI2 * (float)(j + 1) / 8.0f, up = (j & 1) ? 0.9f : 0;
                world_line(P(p->x + w * cosf(a0), p->y - r + ((j & 1) ? 0 : 0.9f), p->z + w * sinf(a0)), P(p->x + w * cosf(a1), p->y - r + up, p->z + w * sinf(a1)));
            }
            world_rgb(170, 110, 60);
            world_line(P(p->x - w * 0.4f, p->y - r, p->z), P(p->x - w * 0.9f, p->y - r * 0.4f, p->z + 1.0f));
            world_line(P(p->x + w * 0.3f, p->y - r, p->z - 1.0f), P(p->x + w * 0.5f, p->y - r * 0.5f, p->z - 2.0f));
        }
    }
    if (G.boss_stage > 0 && bosses[G.era].stage[G.boss_stage - 1].part[0].look == LOOK_BAY) {
        /* The bomb bay of the stage before. In the film until it blows
         * (BAY_BLOWS): still whole, white and orange in turn, its doors
         * shaking. After: a torn hole, bent frames hanging out of it. */
        const struct boss_part *p = &bosses[G.era].stage[G.boss_stage - 1].part[0];
        float r = p->r, w = r * 0.8f, l = r * 1.6f;
        if (G.state == BOSSTURN && G.state_time < BAY_BLOWS) {
            float shake = 0.5f * sinf((float)G.frames * 2.3f), drop = r * 0.9f;
            if ((G.frames / 3) & 1) world_rgb(255, 255, 255); else world_rgb(255, 120, 30);
            world_line(P(p->x - w, p->y, p->z - l), P(p->x + w, p->y, p->z - l));
            world_line(P(p->x + w, p->y, p->z - l), P(p->x + w, p->y, p->z + l));
            world_line(P(p->x + w, p->y, p->z + l), P(p->x - w, p->y, p->z + l));
            world_line(P(p->x - w, p->y, p->z + l), P(p->x - w, p->y, p->z - l));
            for (j = -1; j <= 1; j += 2) {
                float x0 = p->x + w * (float)j, x1 = x0 + (drop * 0.5f + shake) * (float)j;
                world_line(P(x0, p->y, p->z - l), P(x1, p->y - drop, p->z - l));
                world_line(P(x0, p->y, p->z + l), P(x1, p->y - drop, p->z + l));
                world_line(P(x1, p->y - drop, p->z - l), P(x1, p->y - drop, p->z + l));
            }
        } else {
            world_rgb(120, 120, 130);
            for (j = 0; j < 6; j++) {       /* its torn edge, round the hole */
                float z0 = p->z - l + 2.0f * l * (float)j / 6.0f, z1 = z0 + 2.0f * l / 6.0f;
                float in0 = (j & 1) ? 0.55f : 1.0f, in1 = (j & 1) ? 1.0f : 0.55f;
                world_line(P(p->x - w * in0, p->y, z0), P(p->x - w * in1, p->y, z1));
                world_line(P(p->x + w * in1, p->y, z0), P(p->x + w * in0, p->y, z1));
            }
            world_rgb(170, 110, 60);
            for (j = 0; j < 3; j++) {       /* frames hanging out of it, swinging */
                float z = p->z - l * 0.7f + l * 0.7f * (float)j, sw = 0.8f * sinf((float)G.frames * 0.07f + (float)j * 2.0f);
                float x = p->x + w * (j == 1 ? -0.5f : 0.4f);
                world_line(P(x, p->y, z), P(x + sw, p->y - r * (0.5f + 0.15f * (float)j), z + sw * 0.5f));
            }
        }
    }
    for (i = 0; i < st->parts; i++) {
        const struct boss_part *p = &st->part[i];
        float r = p->r;
        if (G.part_hp[i] <= 0) continue;
        if (G.part_flash[i] > 0) world_rgb(255, 255, 255); else world_rgb(255, 60, 40);
        switch (p->look) {
        case LOOK_ENGINE: {     /* a nacelle: a ring before and one behind, and its propeller turning */
            vp_vec3 a[6], b[6];
            float turn = (float)G.frames * 0.6f;
            for (j = 0; j < 6; j++) {
                float an = PI2 * (float)j / 6.0f;
                a[j] = P(p->x + r * 0.7f * cosf(an), p->y + r * 0.7f * sinf(an), p->z + r);
                b[j] = P(p->x + r * 0.5f * cosf(an), p->y + r * 0.5f * sinf(an), p->z - r);
            }
            for (j = 0; j < 6; j++) { world_line(a[j], a[(j + 1) % 6]); world_line(a[j], b[j]); }
            world_rgb(200, 200, 200);
            world_line(P(p->x + r * 1.3f * cosf(turn), p->y + r * 1.3f * sinf(turn), p->z + r * 1.1f),
                       P(p->x - r * 1.3f * cosf(turn), p->y - r * 1.3f * sinf(turn), p->z + r * 1.1f));
            world_line(P(p->x, p->y, p->z), P(p->x * 0.4f, p->y * 0.5f, p->z));      /* its strut */
            break;
        }
        case LOOK_BAY: {        /* the opening under it, its two doors hanging as far open as they have come */
            float w = r * 0.8f, l = r * 1.6f, drop = r * 0.9f * k;
            if (G.state == PLAYING && G.boss_t[2] < 0.7f && ((G.frames / 2) & 1)) world_rgb(255, 255, 255);     /* a row of bombs is about to go */
            world_line(P(p->x - w, p->y, p->z - l), P(p->x + w, p->y, p->z - l));
            world_line(P(p->x + w, p->y, p->z - l), P(p->x + w, p->y, p->z + l));
            world_line(P(p->x + w, p->y, p->z + l), P(p->x - w, p->y, p->z + l));
            world_line(P(p->x - w, p->y, p->z + l), P(p->x - w, p->y, p->z - l));
            world_rgb(200, 200, 200);
            world_line(P(p->x - w, p->y, p->z - l), P(p->x - w - drop * 0.5f, p->y - drop, p->z - l));
            world_line(P(p->x - w, p->y, p->z + l), P(p->x - w - drop * 0.5f, p->y - drop, p->z + l));
            world_line(P(p->x - w - drop * 0.5f, p->y - drop, p->z - l), P(p->x - w - drop * 0.5f, p->y - drop, p->z + l));
            world_line(P(p->x + w, p->y, p->z - l), P(p->x + w + drop * 0.5f, p->y - drop, p->z - l));
            world_line(P(p->x + w, p->y, p->z + l), P(p->x + w + drop * 0.5f, p->y - drop, p->z + l));
            world_line(P(p->x + w + drop * 0.5f, p->y - drop, p->z - l), P(p->x + w + drop * 0.5f, p->y - drop, p->z + l));
            break;
        }
        case LOOK_CANNON: {     /* a barrel run out of its nose, with a muzzle brake */
            float len = 6.0f * k, w = 0.5f;
            world_line(P(p->x - w, p->y - 0.6f, p->z), P(p->x - w, p->y - 0.6f, p->z + len));
            world_line(P(p->x + w, p->y - 0.6f, p->z), P(p->x + w, p->y - 0.6f, p->z + len));
            world_line(P(p->x - w * 2.0f, p->y - 0.6f, p->z + len), P(p->x + w * 2.0f, p->y - 0.6f, p->z + len));
            world_line(P(p->x, p->y - 0.6f - w * 2.0f, p->z + len), P(p->x, p->y - 0.6f + w * 2.0f, p->z + len));
            break;
        }
        case LOOK_POD: {        /* a box of tubes out on an arm */
            float sx = p->x > 0 ? 1.0f : -1.0f, x = sx * (2.2f + (fabsf(p->x) - 2.2f) * k), h = r * 0.6f, l = r;
            world_line(P(sx * 2.2f, p->y, p->z), P(x, p->y, p->z));
            for (j = 0; j < 2; j++) {
                float zz = p->z + (j ? l : -l);
                world_line(P(x - h, p->y - h, zz), P(x + h, p->y - h, zz));
                world_line(P(x + h, p->y - h, zz), P(x + h, p->y + h, zz));
                world_line(P(x + h, p->y + h, zz), P(x - h, p->y + h, zz));
                world_line(P(x - h, p->y + h, zz), P(x - h, p->y - h, zz));
            }
            world_line(P(x - h, p->y - h, p->z - l), P(x - h, p->y - h, p->z + l));
            world_line(P(x + h, p->y - h, p->z - l), P(x + h, p->y - h, p->z + l));
            world_line(P(x + h, p->y + h, p->z - l), P(x + h, p->y + h, p->z + l));
            world_line(P(x - h, p->y + h, p->z - l), P(x - h, p->y + h, p->z + l));
            break;
        }
        case LOOK_LASER: {      /* a turret on its back: a ring, a dome of four ribs over it, a lens that glows, and a barrel that looks along the beam */
            float h = r * k, w = r * 0.85f;
            vp_vec3 top = P(p->x, p->y - r + h, p->z), tip, ring[8];
            for (j = 0; j < 8; j++) {
                float an = PI2 * (float)j / 8.0f;
                ring[j] = P(p->x + w * cosf(an), p->y - r, p->z + w * sinf(an));
            }
            for (j = 0; j < 8; j++) world_line(ring[j], ring[(j + 1) % 8]);
            for (j = 0; j < 8; j += 2) {
                float an = PI2 * (float)j / 8.0f;
                vp_vec3 mid = P(p->x + w * 0.75f * cosf(an), p->y - r + h * 0.6f, p->z + w * 0.75f * sinf(an));
                world_line(ring[j], mid);
                world_line(mid, top);
            }
            tip = vp_v3_add(top, vp_v3_scale(G.laser_state ? G.laser_dir : forward_of(o), 11.0f * k));
            if (G.laser_state >= 2 || ((G.frames / 6) & 1)) world_rgb(255, 255, 255); else world_rgb(255, 120, 60);
            world_line(top, tip);
            world_line(vp_v3_add(top, vp_v3(0, 1.2f, 0)), tip);
            break;
        }
        case LOOK_STATION:      /* on the ground: drawn below, with the shield */
            break;
        case LOOK_ARM: case LOOK_HEAD:          /* the robot's own limb is what is seen (draw_robot) */
            break;
        default: {              /* LOOK_CORE: a diamond, where the light is */
            float q = r * 0.7f;
            world_line(P(p->x, p->y - q, p->z), P(p->x + q, p->y, p->z)); world_line(P(p->x + q, p->y, p->z), P(p->x, p->y + q, p->z));
            world_line(P(p->x, p->y + q, p->z), P(p->x - q, p->y, p->z)); world_line(P(p->x - q, p->y, p->z), P(p->x, p->y - q, p->z));
            break;
        }
        }
        if (p->look != LOOK_STATION) mark_weak(p->look == LOOK_ARM ? boss_part_at(o, i) : P(p->x, p->y, p->z), (p->r + 1.5f) * size);
    }
#undef P
    if (st->parts > 0 && st->part[0].look == LOOK_STATION) {
        /* The shield: three hoops round the mothership, flickering; under
         * it the stations that feed it - a mast on three legs with a
         * dish - each with its beam up to the ship. */
        float r = model_radius(o->model) * size * 1.2f;
        world_level(G.bright[BRIGHT_EFFECTS]);
        for (j = 0; j < 3; j++) {
            vp_vec3 prev = vp_v3(0, 0, 0);
            int q;
            if ((G.frames + (unsigned)j * 3) % 7 == 0) continue;
            world_rgb(80, 220, 255);
            for (q = 0; q <= 12; q++) {
                float an = PI2 * (float)q / 12.0f + (float)G.frames * 0.01f, c = r * cosf(an), n2 = r * sinf(an);
                vp_vec3 pt = vp_v3_add(o->pos, j == 0 ? vp_v3(c, 0, n2) : j == 1 ? vp_v3(c, n2, 0) : vp_v3(0, c, n2));
                if (q) world_line(prev, pt);
                prev = pt;
            }
        }
        for (i = 0; i < st->parts; i++) {
            vp_vec3 dish = G.station[i], foot = vp_v3(dish.x, dish.y - STATION_HEIGHT, dish.z);
            if (G.part_hp[i] <= 0) continue;
            world_level(G.bright[BRIGHT_ENEMIES]);
            if (G.part_flash[i] > 0) world_rgb(255, 255, 255); else world_rgb(255, 60, 255);
            for (j = 0; j < 3; j++) {
                float an = PI2 * (float)j / 3.0f;
                world_line(vp_v3(foot.x + 9.0f * cosf(an), foot.y, foot.z + 9.0f * sinf(an)), dish);
            }
            for (j = 0; j < 6; j++) {
                float a0 = PI2 * (float)j / 6.0f, a1 = PI2 * (float)(j + 1) / 6.0f;
                world_line(vp_v3(dish.x + 7.0f * cosf(a0), dish.y + 3.0f, dish.z + 7.0f * sinf(a0)),
                           vp_v3(dish.x + 7.0f * cosf(a1), dish.y + 3.0f, dish.z + 7.0f * sinf(a1)));
            }
            mark_weak(dish, 10.0f);
            if ((G.frames / 3 + (unsigned)i) & 1) {
                world_level(G.bright[BRIGHT_EFFECTS]);
                world_rgb(80, 220, 255);
                world_line(dish, o->pos);
            }
        }
    }
    if (G.laser_state && G.state == PLAYING) {
        /* The laser: a thin line while it aims, the line blinking while it
         * holds, three lines of white fire while it burns. */
        vp_vec3 from = G.laser_from, to = vp_v3_add(from, vp_v3_scale(G.laser_dir, LASER_REACH));
        world_level(G.bright[BRIGHT_SHOTS]);
        if (G.laser_state == 1) {
            world_rgb(140, 30, 30);
            world_line(from, to);
        } else if (G.laser_state == 2) {
            if ((G.frames / 3) & 1) { world_rgb(255, 60, 40); world_line(from, to); }
        } else {
            vp_vec3 side = vp_v3_normalize(vp_v3_cross(fabsf(G.laser_dir.y) > 0.97f ? vp_v3(1, 0, 0) : vp_v3(0, 1, 0), G.laser_dir));
            vp_vec3 up = vp_v3_cross(G.laser_dir, side);
            world_rgb(255, 255, 255);
            world_line(from, to);
            world_rgb(255, 80, 60);
            world_line(vp_v3_add(from, vp_v3_scale(side, 2.5f)), vp_v3_add(to, vp_v3_scale(side, 2.5f)));
            world_line(vp_v3_sub(from, vp_v3_scale(up, 2.5f)), vp_v3_sub(to, vp_v3_scale(up, 2.5f)));
        }
    }
}

/* A model with some of its lines missing: line by line. */

static void draw_cut(const obj *o, int tint)
{
    const float *segs;
    int n = model_segments(o->model, &segs), i, last = -1;
    vp_xf place = flyer_place(o);
    float edge = wing_edge(o->model);

    for (i = 0; i < n; i++) {
        const float *g = segs + 6 * i;
        int slot = model_segment_colour(o->model, i);
        if (!keeps(o->cut, g, edge)) continue;
        if (slot != last) {
            int r, gr, b;
            model_colour(o->model, slot, tint, &r, &gr, &b);
            world_rgb(r, gr, b);
            last = slot;
        }
        world_line(vp_xf_point(&place, vp_v3(g[0], g[1], g[2])), vp_xf_point(&place, vp_v3(g[3], g[4], g[5])));
    }
}

/* What is left of the flyers shot down: wrecks, the pieces off them, the
 * lines of those that burst, the shock waves. */

static void draw_wreckage(vp_vec3 eye)
{
    int i;

    world_level(G.bright[BRIGHT_ENEMIES]);
    for (i = 0; i < MAX_OBJ; i++) {
        const obj *o = &G.objs[i];
        if (o->kind == K_WRECK) {
            /* burning: it flickers between its colours and fire */
            int tint = ((G.frames + (unsigned)i) & 2) ? TINT_FIRE : o->heavy ? TINT_HEAVY : TINT_OWN;
            if (o->cut) draw_cut(o, tint);
            else {
                vp_xf place = flyer_place(o);
                model_draw(o->model, &place, tint);
                if (o->rotor) draw_rotors(o, &place);
            }
        } else if (o->kind == K_PART) {
            if (o->model == MODEL_ROTOR) {
                vp_xf place = vp_xf_mul2(world_place(o->pos, o->heading + o->spin, o->pitch, o->roll),
                                         vp_xf_scale(o->speed, 1, o->speed));
                model_draw(MODEL_ROTOR, &place, TINT_OWN);
            } else {
                draw_cut(o, o->heavy ? TINT_HEAVY : TINT_OWN);
            }
        }
    }
    world_level(G.bright[BRIGHT_EFFECTS]);
    for (i = 0; i < MAX_OBJ; i++) {
        const obj *o = &G.objs[i];
        if (o->kind == K_DEBRIS) {              /* from white heat to its colour, then dark */
            float k = clampf(o->life * 1.2f, 0.2f, 1.0f), hot = clampf(o->life - 1.0f, 0, 1);
            float r = (float)((o->value >> 16) & 255), g = (float)((o->value >> 8) & 255), b = (float)(o->value & 255);
            world_rgb((int)((r + (255.0f - r) * hot) * k), (int)((g + (255.0f - g) * hot) * k),
                      (int)((b + (255.0f - b) * hot) * k));
            world_dash(vp_v3_add(o->pos, o->offset), vp_v3_add(o->pos, o->wp));
        } else if (o->kind == K_PRING) {
            /* its points, each a short stroke outwards; the colour runs
             * round the ring and round the wheel as it grows, and it turns */
            float r = o->speed * o->age, k = clampf(o->life * 2.5f, 0.15f, 1.0f), turn = o->age * 1.5f;
            int j;
            for (j = 0; j < o->hp; j++) {
                float a = turn + PI2 * (float)j / (float)o->hp, ca = cosf(a), sa = sinf(a);
                vp_vec3 dir = vp_v3_add(vp_v3_scale(o->offset, ca), vp_v3_scale(o->wp, sa));
                vp_vec3 p = vp_v3_add(o->pos, vp_v3_scale(dir, r));
                int cr, cg, cb;
                hue_rgb(o->phase + o->age * 1.3f + (float)j / (float)o->hp * 0.35f, &cr, &cg, &cb);
                world_rgb((int)((float)cr * k), (int)((float)cg * k), (int)((float)cb * k));
                world_dash(p, vp_v3_add(p, vp_v3_scale(dir, 1.5f + r * 0.03f)));
            }
        } else if (o->kind == K_RING) {
            float r = o->speed * o->age, k = clampf(o->life * 2.0f, 0.15f, 1.0f);
            world_rgb((int)(255.0f * k), (int)(230.0f * k), (int)(150.0f * k));
            if (o->cut) {                       /* flat on the ground */
                vp_vec3 prev = vp_v3(o->pos.x + r, o->pos.y, o->pos.z);
                int j;
                for (j = 1; j <= 12; j++) {
                    float a = PI2 * (float)j / 12.0f;
                    vp_vec3 p = vp_v3(o->pos.x + r * cosf(a), o->pos.y, o->pos.z + r * sinf(a));
                    world_line(prev, p);
                    prev = p;
                }
            } else {                            /* in the air: facing whoever looks */
                float sx, sy, d = dist3(o->pos, eye);
                if (d > 5.0f && d < 1500.0f && world_project(o->pos, &sx, &sy))
                    world_bcircle(sx, sy, clampf(r * 3400.0f / d, 4.0f, 2600.0f), 10);
            }
        }
    }
}

/* An item where it is: a square standing on its corner, turning, in the
 * item's colour, with its letter in the middle. */

void draw_item(vp_vec3 at, int item, float size, float turn)
{
    const unsigned char *c = item_rgb[item % ITEM_COUNT];
    vp_vec3 a = vp_v3(size * cosf(turn), 0, size * sinf(turn)), up = vp_v3(0, size, 0);
    float sx, sy, d = dist3(at, vp3d_eye(world_view()));
    char letter[2] = { item_names[item % ITEM_COUNT][0], 0 };

    world_rgb(c[0], c[1], c[2]);
    world_line(vp_v3_add(at, a), vp_v3_add(at, up));
    world_line(vp_v3_add(at, up), vp_v3_sub(at, a));
    world_line(vp_v3_sub(at, a), vp_v3_sub(at, up));
    world_line(vp_v3_sub(at, up), vp_v3_add(at, a));
    if (d < 700.0f && d > 1.0f && !world_hidden(at) && world_project(at, &sx, &sy)) {
        float h = clampf(size * 3400.0f / d * 0.9f, 40.0f, 400.0f);
        world_text(letter, sx, sy - h * 0.5f, h, VP_TEXT_CENTER);
    }
}

/* The mark of a place that holds an item: a white square on its corner,
 * turning, with a question mark in it. Which item it is nobody knows
 * until it is won. */

void draw_mystery(vp_vec3 at, float size, float turn)
{
    vp_vec3 a = vp_v3(size * cosf(turn), 0, size * sinf(turn)), up = vp_v3(0, size, 0);
    float sx, sy, d = dist3(at, vp3d_eye(world_view()));

    world_rgb(255, 255, 255);
    world_line(vp_v3_add(at, a), vp_v3_add(at, up));
    world_line(vp_v3_add(at, up), vp_v3_sub(at, a));
    world_line(vp_v3_sub(at, a), vp_v3_sub(at, up));
    world_line(vp_v3_sub(at, up), vp_v3_add(at, a));
    if (d < 700.0f && d > 1.0f && !world_hidden(at) && world_project(at, &sx, &sy)) {
        float h = clampf(size * 3400.0f / d * 0.9f, 40.0f, 400.0f);
        vp_text_font(VP_FONT_HERSHEY);          /* the arcade lettering has no question mark worth the name */
        vp_text(world_dvg(), "?", sx, sy - h * 0.5f, h, VP_TEXT_CENTER);
    }
}

/* What matters of what stands on the ground: the mark in a gate not yet
 * flown through and over a depot not yet shot to pieces (smaller, and
 * blinking: it has to be shot out first), and each gun's barrel, which
 * follows the plane. */

static void draw_sites(vp_vec3 eye)
{
    city_site sites[16];
    int n = city_sites(G.era, eye, 800.0f, sites, 16), i;
    float turn = (float)G.frames * 0.05f;

    world_level(G.bright[BRIGHT_EFFECTS]);
    for (i = 0; i < n; i++) {
        const city_site *s = &sites[i];
        if (s->kind == CITY_GATE && !s->spent) {
            draw_mystery(s->at, 9.0f, turn + (float)i);
        } else if (s->kind == CITY_DEPOT) {
            if ((G.frames / 12) & 1) draw_mystery(vp_v3_add(s->at, vp_v3(0, 10.0f, 0)), 5.0f, turn + (float)i);
        } else if (s->kind == CITY_GUN) {
            vp_vec3 to = vp_v3_sub(G.p.pos, s->at);
            float d = vp_v3_length(to);
            if (d < 1.0f) continue;
            if (to.y < 0.2f * d) to.y = 0.2f * d;                    /* it cannot point into the ground */
            to = vp_v3_scale(vp_v3_normalize(to), 14.0f);
            world_rgb(255, 60, 60);
            world_line(s->at, vp_v3_add(s->at, to));
            world_line(vp_v3_add(s->at, vp_v3(1.5f, 0, 0)), vp_v3_add(vp_v3_add(s->at, to), vp_v3(1.5f, 0, 0)));
        }
    }
    /* the items already out, hanging, and the player's missiles */
    for (i = 0; i < MAX_OBJ; i++) {
        const obj *o = &G.objs[i];
        if (o->kind == K_ITEM) {
            if (o->life < 5.0f && ((G.frames / 6) & 1)) continue;   /* about to go: it blinks */
            draw_item(o->pos, o->value, 9.0f, turn * 2.0f);
        } else if (o->kind == K_PMISSILE) {
            vp_xf place = vp_xf_mul2(world_place(o->pos, o->heading, o->pitch, 0), vp_xf_scale(2.5f, 2.5f, 2.5f));
            model_draw(MODEL_MISSILE, &place, TINT_OWN);
        }
    }
}

/* What the plane carries, said in a row over the coaming, each in its
 * item's colour. */

static void draw_items_hud(void)
{
    char buf[24];
    float x = CX - 900.0f, y = (G.demo ? HUD_ENERGY_Y_DEMO : HUD_ENERGY_Y) + 100.0f;        /* over the energy bar */

    if (G.p.shield_hits > 0) {
        snprintf(buf, sizeof buf, "SHIELD %d", G.p.shield_hits);
        text(buf, x, y, 55, VP_TEXT_LEFT, item_rgb[0][0], item_rgb[0][1], item_rgb[0][2]);
    }
    if (G.p.missiles > 0) {
        snprintf(buf, sizeof buf, "MISSILES %d", G.p.missiles);
        text(buf, x + 620.0f, y, 55, VP_TEXT_LEFT, item_rgb[1][0], item_rgb[1][1], item_rgb[1][2]);
    }
    if (G.p.triple > 0) {
        snprintf(buf, sizeof buf, "TRIPLE %d", (int)G.p.triple + 1);
        text(buf, x + 1340.0f, y, 55, VP_TEXT_LEFT, item_rgb[2][0], item_rgb[2][1], item_rgb[2][2]);
    }
    if (G.p.flares > 0) {                   /* a line of its own: seldom there with all the others */
        snprintf(buf, sizeof buf, "FLARES %d", (int)G.p.flares + 1);
        text(buf, x + 620.0f, y + 90.0f, 55, VP_TEXT_LEFT, item_rgb[ITEM_FLARES][0], item_rgb[ITEM_FLARES][1], item_rgb[ITEM_FLARES][2]);
    }
}

/* A parachutist, in the round. The canopy is a dome: a wide rim, a
 * narrower ring above it and the top, with eight gores between them, red
 * and white in turn; under it the lines come together at his shoulders.
 * He has a head, a body with some depth, arms up at the lines and legs
 * apart - none of it in one plane. He swings, and turns slowly. Shot
 * (cut), the canopy is a streamer over him and he falls. */
static void draw_chute(const obj *o, const vp_xf *place)
{
    const float u = CHUTE_SIZE;
    vp_vec3 rim[8], mid[8], top, sh_l, sh_r, hip_l, hip_r, neck;
    float open = o->cut ? 0.12f + 0.05f * sinf(o->age * 23.0f) : 1.0f;      /* how far the canopy is spread */
    float high = o->cut ? 10.0f : 7.0f;                                     /* its rim over his shoulders */
    int k;

#define AT(x, y, z) vp_xf_point(place, vp_v3((x) * u, (y) * u, (z) * u))
    top = AT(o->cut ? 0.8f * sinf(o->age * 9.0f) : 0, high + 3.0f, 0);
    for (k = 0; k < 8; k++) {
        float a = PI2 * (float)k / 8.0f, flap = o->cut ? 0.6f * sinf(o->age * 17.0f + (float)k) : 0;
        rim[k] = AT(4.8f * open * cosf(a), high + flap, 4.8f * open * sinf(a));
        mid[k] = AT(3.5f * open * cosf(a), high + 2.0f + flap, 3.5f * open * sinf(a));
    }
    world_level(G.bright[BRIGHT_EFFECTS]);
    /* the canopy */
    world_rgb(255, 255, 255);
    for (k = 0; k < 8; k++) world_line(rim[k], rim[(k + 1) % 8]);
    for (k = 0; k < 8; k++) world_line(mid[k], mid[(k + 1) % 8]);
    for (k = 0; k < 8; k++) {
        if (k & 1) world_rgb(255, 70, 50); else world_rgb(255, 255, 255);
        world_line(rim[k], mid[k]);
        world_line(mid[k], top);
    }
    /* the man: shoulders and hips are a box seen from any side */
    sh_l = AT(-0.6f, 2.2f, 0); sh_r = AT(0.6f, 2.2f, 0);
    hip_l = AT(-0.4f, 0.5f, 0.25f); hip_r = AT(0.4f, 0.5f, -0.25f);
    neck = AT(0, 2.5f, 0);
    /* the lines: four to each shoulder */
    world_rgb(150, 150, 170);
    for (k = 0; k < 8; k += 2) world_line(rim[k], cosf(PI2 * (float)k / 8.0f) < 0 ? sh_l : sh_r);
    world_rgb(120, 255, 200);
    world_line(sh_l, sh_r); world_line(sh_l, hip_l); world_line(sh_r, hip_r); world_line(hip_l, hip_r);
    world_line(sh_l, hip_r);                                                /* across his chest: the harness */
    /* head: a small diamond */
    world_line(neck, AT(-0.35f, 2.95f, 0)); world_line(AT(-0.35f, 2.95f, 0), AT(0, 3.4f, 0));
    world_line(AT(0, 3.4f, 0), AT(0.35f, 2.95f, 0)); world_line(AT(0.35f, 2.95f, 0), neck);
    /* arms up at the lines, bent; legs apart, one before the other */
    world_line(sh_l, AT(-1.3f, 2.6f, 0.3f)); world_line(AT(-1.3f, 2.6f, 0.3f), AT(-1.0f, 3.5f, 0));
    world_line(sh_r, AT(1.3f, 2.6f, -0.3f)); world_line(AT(1.3f, 2.6f, -0.3f), AT(1.0f, 3.5f, 0));
    world_line(hip_l, AT(-0.6f, -0.5f, 0.6f)); world_line(AT(-0.6f, -0.5f, 0.6f), AT(-0.5f, -1.5f, 0.2f));
    world_line(hip_r, AT(0.6f, -0.5f, -0.5f)); world_line(AT(0.6f, -0.5f, -0.5f), AT(0.5f, -1.5f, -0.9f));
    if (world_solid()) {
        /* solid: the canopy's two bands of cloth, the man's body and head */
        vp_vec3 q[4];
        world_group();
        for (k = 0; k < 8; k++) {
            q[0] = rim[k]; q[1] = rim[(k + 1) % 8]; q[2] = mid[(k + 1) % 8]; q[3] = mid[k];
            world_face(q, 4);
            q[0] = mid[k]; q[1] = mid[(k + 1) % 8]; q[2] = top;
            world_face(q, 3);
        }
        q[0] = sh_l; q[1] = sh_r; q[2] = hip_r; q[3] = hip_l;
        world_face(q, 4);
        q[0] = neck; q[1] = AT(-0.35f, 2.95f, 0); q[2] = AT(0, 3.4f, 0); q[3] = AT(0.35f, 2.95f, 0);
        world_face(q, 4);
    }
#undef AT
}

/* A ball, for a shot. Its outline is a circle that always faces the eye,
 * so it is round from wherever it is seen; inside it its equator, and for
 * the fine ones a meridian that turns - a ball, not a disc. Fewer strokes
 * the farther off it is; how far is far goes by its size. */
static void draw_ball(vp_vec3 at, float r, vp_vec3 eye, const int rim[3], const int core[3], float spin, int fine)
{
    float d = dist3(at, eye), near = r * 80.0f, mid = r * 150.0f;
    vp_vec3 look = vp_v3_scale(vp_v3_sub(eye, at), d > 1.0f ? 1.0f / d : 0);
    vp_vec3 br = vp_v3_normalize(vp_v3_cross(fabsf(look.y) > 0.97f ? vp_v3(1, 0, 0) : vp_v3(0, 1, 0), look));
    vp_vec3 bu = vp_v3_cross(look, br);
    int n = fine ? (d < near ? 10 : d < mid ? 8 : 6) : (d < near ? 8 : 6), k;
    vp_vec3 prev = vp_v3_add(at, vp_v3_scale(br, r));

    world_rgb(rim[0], rim[1], rim[2]);
    for (k = 1; k <= n; k++) {
        float a = PI2 * (float)k / (float)n;
        vp_vec3 q = vp_v3_add(at, vp_v3_add(vp_v3_scale(br, r * cosf(a)), vp_v3_scale(bu, r * sinf(a))));
        world_dash(prev, q);
        prev = q;
    }
    if (d >= mid) return;
    {
        int m = fine && d < near ? 8 : 6;
        world_rgb(core[0], core[1], core[2]);
        prev = vp_v3_add(at, vp_v3(r, 0, 0));
        for (k = 1; k <= m; k++) {                  /* the equator */
            float a = PI2 * (float)k / (float)m;
            vp_vec3 q = vp_v3_add(at, vp_v3(r * cosf(a), 0, r * sinf(a)));
            world_dash(prev, q);
            prev = q;
        }
    }
    if (fine && d < near) {                         /* and the meridian, turning */
        vp_vec3 side = vp_v3(cosf(spin), 0, sinf(spin));
        prev = vp_v3_add(at, vp_v3(0, r, 0));
        for (k = 1; k <= 8; k++) {
            float a = PI2 * (float)k / 8.0f;
            vp_vec3 q = vp_v3_add(at, vp_v3_add(vp_v3(0, r * cosf(a), 0), vp_v3_scale(side, r * sinf(a))));
            world_dash(prev, q);
            prev = q;
        }
    }
}

static void draw_objects(void)
{
    int i;
    vp_vec3 eye = vp3d_eye(world_view());

    /* the enemies and the boss */
    world_level(G.bright[BRIGHT_ENEMIES]);
    for (i = 0; i < MAX_OBJ; i++) {
        const obj *o = &G.objs[i];
        if (o->kind != K_ENEMY && o->kind != K_BOSS && o->kind != K_MISSILE) continue;
        draw_flyer(o);
    }
    draw_wreckage(eye);
    draw_sites(eye);
    /* parachutists */
    world_level(G.bright[BRIGHT_EFFECTS]);
    world_rgb(255, 255, 255);
    for (i = 0; i < MAX_OBJ; i++) {
        const obj *o = &G.objs[i];
        vp_xf place;
        if (o->kind != K_CHUTE) continue;
        place = world_place(o->pos, o->age * 0.5f, 0.14f * sinf(o->age * 1.7f), 0.14f * cosf(o->age * 1.3f));
        draw_chute(o, &place);
    }
    /* enemy fire: yellow dashes along their way, as the original's are
     * yellow; what the saucers shoot is round, here a green cross */
    world_level(G.bright[BRIGHT_SHOTS]);
    for (i = 0; i < MAX_OBJ; i++) {
        const obj *o = &G.objs[i];
        if (o->kind == K_BULLET && o->size > 0) {
            /* the boss's and the big ones': a big ball of fire */
            static const int rim_a[3] = { 255, 150, 40 }, rim_b[3] = { 255, 90, 30 }, core[3] = { 255, 60, 30 };
            draw_ball(o->pos, seen(o->size, o->pos, eye, 0.014f), eye, ((G.frames / 4 + (unsigned)i) & 1) ? rim_a : rim_b, core,
                      (float)G.frames * 0.2f + (float)i, 1);
        } else if (o->kind == K_BULLET && o->cut == SHOT_MG) {
            /* a boss's machine gun: tracer in the era's colour of fire. A
             * streak alone is a point to whoever it comes straight at -
             * and that is who has to see it - so it has a head, a small
             * ball with a white heart, and its streak behind that. */
            static const int fire[3] = { 255, 150, 40 }, white[3] = { 255, 255, 255 }, green[3] = { 120, 255, 160 };
            float r = seen(1.1f, o->pos, eye, 0.009f);
            draw_ball(o->pos, r, eye, G.era == 4 ? green : fire, white, 0, 0);
            if (G.era == 4) world_rgb(120, 255, 160); else world_rgb(255, 170, 60);
            world_dash(o->pos, vp_v3_sub(o->pos, vp_v3_scale(vp_v3_normalize(o->vel), 9.0f + r * 6.0f)));
        } else if (o->kind == K_BULLET && o->cut == SHOT_ROCKET) {
            /* a rocket: a dart, its head white */
            vp_vec3 dir = vp_v3_normalize(o->vel), tail = vp_v3_sub(o->pos, vp_v3_scale(dir, 7.0f));
            vp_vec3 fin = vp_v3_normalize(vp_v3_cross(fabsf(dir.y) > 0.97f ? vp_v3(1, 0, 0) : vp_v3(0, 1, 0), dir));
            {
                static const int hot[3] = { 255, 120, 30 }, white[3] = { 255, 255, 255 };
                draw_ball(o->pos, seen(1.0f, o->pos, eye, 0.009f), eye, hot, white, 0, 0);       /* its head, seen from ahead too */
            }
            world_rgb(255, 255, 255);
            world_dash(o->pos, tail);
            world_rgb(255, 120, 30);
            world_dash(tail, vp_v3_add(vp_v3_sub(tail, vp_v3_scale(dir, 2.0f)), vp_v3_scale(fin, 1.6f)));
            world_dash(tail, vp_v3_sub(vp_v3_sub(tail, vp_v3_scale(dir, 2.0f)), vp_v3_scale(fin, 1.6f)));
        } else if (o->kind == K_BULLET) {
            /* everybody else's: a small ball, yellow as the original's
             * shots are - green from a saucer */
            static const int yellow[3] = { 255, 230, 0 }, pale[3] = { 255, 255, 170 };
            static const int green[3] = { 120, 255, 160 }, light[3] = { 210, 255, 220 };
            draw_ball(o->pos, o->cut == SHOT_FLAK ? 1.5f : 0.9f, eye, G.era == 4 ? green : yellow, G.era == 4 ? light : pale, 0, 0);
        } else if (o->kind == K_BOMB) {         /* a tin can, tumbling: its length and its two ends */
            float a = o->age * 7.0f + o->ang.x;
            vp_vec3 ax = vp_v3(1.8f * cosf(a), 1.8f * sinf(a), 1.8f * sinf(a * 0.7f + o->ang.y));
            vp_vec3 side = vp_v3(-ax.y * 0.5f, ax.x * 0.5f, 0.6f);
            vp_vec3 p, q;
            if (o->size > 0) {          /* a boss's: big, red and white in turn, a ring round it - seen from under it too */
                static const int red[3] = { 255, 60, 40 }, white[3] = { 255, 255, 255 };
                ax = vp_v3_scale(ax, o->size);
                side = vp_v3_scale(side, o->size);
                draw_ball(o->pos, seen(1.4f * o->size, o->pos, eye, 0.012f), eye, ((G.frames / 3) & 1) ? white : red, red, 0, 0);
            }
            p = vp_v3_add(o->pos, ax); q = vp_v3_sub(o->pos, ax);
            if (o->size > 0) world_rgb(255, 120, 60); else
            world_rgb(210, 210, 220);
            world_line(vp_v3_sub(p, side), vp_v3_add(p, side));
            world_line(vp_v3_add(p, side), vp_v3_add(q, side));
            world_line(vp_v3_add(q, side), vp_v3_sub(q, side));
            world_line(vp_v3_sub(q, side), vp_v3_sub(p, side));
        }
    }
    /* the player's shots: white, as the original's */
    world_rgb(235, 255, 255);
    for (i = 0; i < MAX_OBJ; i++) {
        const obj *o = &G.objs[i];
        if (o->kind != K_SHOT) continue;
        world_dash(vp_v3_sub(o->pos, vp_v3_scale(o->vel, 0.018f)), o->pos);
    }
    /* sparks */
    world_level(G.bright[BRIGHT_EFFECTS]);
    for (i = 0; i < MAX_OBJ; i++) {
        const obj *o = &G.objs[i];
        float heat;
        if (o->kind != K_SPARK) continue;
        heat = clampf(o->life * 1.5f, 0.2f, 1.0f);
        if (o->hp == SPARK_SMOKE) {             /* a short grey stroke, standing */
            world_rgb((int)(130.0f * heat), (int)(130.0f * heat), (int)(140.0f * heat));
            world_dash(o->pos, vp_v3_add(o->pos, vp_v3(0, 2.5f, 0)));
            continue;
        }
        if (o->hp == SPARK_WHITE) world_rgb(255, 255, 255);
        else if (o->hp == SPARK_RAINBOW) {          /* every one its own colour, running */
            int cr, cg, cb;
            hue_rgb((float)i * 0.137f + o->life * 1.5f, &cr, &cg, &cb);
            world_rgb((int)((float)cr * heat), (int)((float)cg * heat), (int)((float)cb * heat));
        }
        else world_rgb(255, (int)(200.0f * heat), (int)(60.0f * heat));
        world_dash(o->pos, vp_v3_add(o->pos, vp_v3_scale(o->vel, 0.03f)));
    }
    /* Everything short and scattered has only been collected so far:
     * sparks, shots, the lines of things burst, the points of the rings.
     * Now it is drawn, in the order that is shortest for the beam. */
    world_dashes();
    /* floating scores, as text where they are */
    world_level(G.bright[BRIGHT_TEXT]);
    world_rgb(255, 255, 255);
    for (i = 0; i < MAX_OBJ; i++) {
        const obj *o = &G.objs[i];
        float sx, sy, d;
        char buf[16];
        if (o->kind != K_SCORE) continue;
        if (world_hidden(o->pos) || !world_project(o->pos, &sx, &sy)) continue;     /* behind the cockpit or a building: not seen */
        d = dist3(o->pos, eye);
        if (d > 900.0f) continue;
        snprintf(buf, sizeof buf, "%d", o->value);
        world_text(buf, sx, sy, clampf(60.0f * 300.0f / d, 50.0f, 160.0f), VP_TEXT_CENTER);
    }
}

/* The afterburner: from each of the plane's two nozzles a flame, orange
 * outside and white hot inside, as long as `power` (0..1) and never still. */

void draw_burner(const vp_xf *place, float power)
{
    int side, k;
    if (power < 0.05f) return;
    for (side = -1; side <= 1; side += 2) {
        float x = 0.95f * (float)side;
        float flick = 0.8f + 0.2f * sinf((float)G.frames * 1.7f + (float)side) + 0.15f * sinf((float)G.frames * 3.1f);
        float len = power * 9.0f * flick;
        vp_vec3 tip = vp_xf_point(place, vp_v3(x, 0.02f, -6.4f - len));
        vp_vec3 core = vp_xf_point(place, vp_v3(x, 0.02f, -6.4f - len * 0.5f));
        static const float cx[4] = { -0.6f, 0.6f, 0.6f, -0.6f }, cy[4] = { 0.45f, 0.45f, -0.4f, -0.4f };
        world_rgb(255, 130, 0);
        for (k = 0; k < 4; k++) world_line(vp_xf_point(place, vp_v3(x + cx[k], cy[k], -6.4f)), tip);
        world_rgb(255, 255, 200);
        world_line(vp_xf_point(place, vp_v3(x - 0.3f, 0.02f, -6.4f)), core);
        world_line(core, vp_xf_point(place, vp_v3(x + 0.3f, 0.02f, -6.4f)));
        /* a shock diamond down the flame when it is fully lit */
        if (power > 0.8f) {
            vp_vec3 m = vp_xf_point(place, vp_v3(x, 0.02f, -6.4f - len * 0.72f));
            world_rgb(120, 180, 255);
            world_line(vp_xf_point(place, vp_v3(x - 0.25f, 0.02f, -6.4f - len * 0.6f)), m);
            world_line(m, vp_xf_point(place, vp_v3(x + 0.25f, 0.02f, -6.4f - len * 0.6f)));
        }
    }
}

static void draw_player_plane(void)
{
    vp_xf place;
    if (!G.p.alive) return;
    if (G.p.shield > 0 && G.state != BOSSFALL && G.state != BOSSINTRO && G.state != BOSSTURN && G.state != WARP && G.state != LEVELIN && ((G.frames / 4) & 1)) return;      /* a new plane blinks, but not in the film */
    world_level(G.bright[BRIGHT_PLAYER]);
    place = world_place(G.p.pos, G.p.heading, G.p.pitch, player_roll());
    model_draw(MODEL_PLAYER, &place, G.p.hurt > 0 ? TINT_WHITE : TINT_OWN);
    draw_burner(&place, G.p.burner);
    if (G.p.shield_hits > 0) {              /* the shield: a six-sided ring round the plane, two when it is whole */
        int k, j;
        world_rgb(item_rgb[0][0], item_rgb[0][1], item_rgb[0][2]);
        for (k = 0; k < G.p.shield_hits; k++) {
            float r = 8.5f + 1.2f * (float)k, turn = (float)G.frames * 0.04f * (k ? -1.0f : 1.0f);
            vp_vec3 prev = vp_xf_point(&place, vp_v3(r * cosf(turn), r * sinf(turn), 0));
            for (j = 1; j <= 6; j++) {
                float a = turn + PI2 * (float)j / 6.0f;
                vp_vec3 p = vp_xf_point(&place, vp_v3(r * cosf(a), r * sinf(a), 0));
                world_line(prev, p);
                prev = p;
            }
        }
    }
}

/* The pipper ahead of the plane in the chase view. */

static void draw_pipper(void)
{
    vp_vec3 ahead = vp_v3_add(G.p.pos, vp_v3_scale(player_forward(), 320.0f));
    float sx, sy;
    if (!G.p.alive || !world_project(ahead, &sx, &sy)) return;
    world_level(G.bright[BRIGHT_PLAYER]);
    world_rgb(255, 255, 0);
    world_bcircle(sx, sy, 60, 6);
}

/* What the head-up display needs to know, the target above all: the one
 * the guns would follow, else the one nearest the nose, else which way
 * the nearest of all is. */

static void draw_plane_hud(int cockpit)
{
    const vp3d_view *view = world_view();
    vp_vec3 fwd = player_forward(), aim;
    hud_state h;
    obj *t, *nearest = NULL, *ahead = NULL;
    float best_d = 1e9f, best_dot = 0.86f;      /* within 30 degrees of the nose */
    int i;

    memset(&h, 0, sizeof h);
    h.eye = vp3d_eye(view);
    /* the way the nose really points: its course over the ground, and how
     * far above or below the horizon, whichever way up the plane is */
    h.heading = course_of(G.p.heading, G.p.pitch);
    h.pitch = asinf(clampf(fwd.y, -1, 1));
    h.speed = G.p.speed;
    h.altitude = G.p.pos.y;
    h.throttle = G.p.throttle;
    h.energy = G.p.energy / ENERGY_FULL;
    h.cockpit = cockpit;
    h.energy_y = G.demo ? HUD_ENERGY_Y_DEMO : HUD_ENERGY_Y;
    h.burner = G.p.burner > 0.3f;
    h.rear = G.p.rear;
    h.stalled = G.p.stalled;
    h.pull_up = G.p.pull_up;
    h.hurt = G.p.hurt;
    h.frames = G.frames;
    h.bright = G.bright[BRIGHT_PLAYER];
    h.bright_text = G.bright[BRIGHT_TEXT];

    for (i = 0; i < MAX_OBJ; i++) {
        obj *o = &G.objs[i];
        vp_vec3 to;
        float d, dot;
        if (o->kind != K_ENEMY && o->kind != K_BOSS) continue;
        to = vp_v3_sub(o->pos, G.p.pos);
        d = vp_v3_length(to);
        if (d < 1.0f) continue;
        dot = vp_v3_dot(fwd, vp_v3_scale(to, 1.0f / d));
        if (d < best_d) { best_d = d; nearest = o; }
        if (dot > best_dot && d < 900.0f) { best_dot = dot; ahead = o; }
    }
    t = G.aim_assist > 0 ? gun_target(ASSIST_CONE * (float)G.aim_assist / 100.0f, &aim) : NULL;
    if (t && t->kind != K_MISSILE) { ahead = t; h.locked = 1; }
    {
        /* the nearest missile, wherever the plane looks */
        float md;
        const obj *m = nearest_missile(&md);
        if (m && md < 700.0f) {
            vp_vec3 c = vp_xf_point(&view->world_to_camera, m->pos);
            h.missile = 1;
            h.missile_angle = atan2f(c.y, c.x);
            h.missile_range = md;
        }
    }
    {
        /* the boss, while there is one: always shown where it is */
        for (i = 0; i < MAX_OBJ; i++) if (G.objs[i].kind == K_BOSS) break;
        if (i < MAX_OBJ) {
            vp_vec3 c = vp_xf_point(&view->world_to_camera, G.objs[i].pos);
            h.boss = 1;
            /* in sight only when it is on the screen, clear of its edges
             * and of the panel: before the eye but off to one side it has
             * to get the arrow, not corners nobody sees */
            h.boss_seen = world_project(G.objs[i].pos, &h.boss_x, &h.boss_y)
                       && h.boss_x > 350.0f && h.boss_x < 3745.0f && h.boss_y > 900.0f && h.boss_y < 3550.0f;
            h.boss_angle = atan2f(c.y, c.x);
        }
    }
    if (G.boss_alive && G.state == PLAYING && (G.laser_state == 1 || G.laser_state == 2)) h.laser = G.laser_state;
    if (h.rear) { hud_draw(&h); return; }       /* the guns point the other way */
    if (ahead && world_project(ahead->pos, &h.target_x, &h.target_y)) {
        h.target = 1;
        h.target_range = dist3(ahead->pos, G.p.pos);
    } else if (nearest) {
        vp_vec3 c = vp_xf_point(&view->world_to_camera, nearest->pos);
        h.arrow = 1;
        h.arrow_angle = atan2f(c.y, c.x);
    }
    hud_draw(&h);
}

/* The radar: the sky round the plane, seen from above, the plane's nose up. */

static void draw_radar(void)
{
    const float cx = 3560.0f, cy = 500.0f, r = 300.0f, range = 900.0f;
    const float ry = r * (world_vertical() ? 0.75f : 4.0f / 3.0f);
    float course = course_of(G.p.heading, G.p.pitch), ch = cosf(course), sh = sinf(course);
    int i;

    world_level(G.bright[BRIGHT_TEXT]);
    world_rgb(0, 160, 0);
    world_bcircle(cx, cy, r, 12);
    world_rgb(0, 255, 255);
    world_bline(cx - 30, cy - 30, cx, cy + 40);
    world_bline(cx, cy + 40, cx + 30, cy - 30);
    {
        /* what stands on the ground: an item to be had as a cross in its
         * (white: which one is not known), a gun as a red tick */
        city_site sites[16];
        int n = city_sites(G.era, G.p.pos, range, sites, 16);
        for (i = 0; i < n; i++) {
            vp_vec3 rel = vp_v3_sub(sites[i].at, G.p.pos);
            float rx = cx + (rel.x * ch - rel.z * sh) / range * r, rz = cy + (rel.x * sh + rel.z * ch) / range * ry;
            if (sites[i].kind == CITY_GUN) {
                world_rgb(255, 60, 60);
                world_bline(rx - 10, rz - 10, rx + 10, rz + 10);
            } else if (!sites[i].spent) {
                world_rgb(255, 255, 255);
                world_bline(rx - 20, rz, rx + 20, rz);
                world_bline(rx, rz - 20, rx, rz + 20);
            }
        }
    }
    for (i = 0; i < MAX_OBJ; i++) {
        const obj *o = &G.objs[i];
        vp_vec3 rel;
        float rx, rz, d;
        if (o->kind != K_ENEMY && o->kind != K_BOSS && o->kind != K_CHUTE) continue;
        rel = vp_v3_sub(o->pos, G.p.pos);
        rx = rel.x * ch - rel.z * sh;
        rz = rel.x * sh + rel.z * ch;
        d = sqrtf(rx * rx + rz * rz);
        if (d > range) { rx *= range / d; rz *= range / d; }
        rx = cx + rx / range * r;
        rz = cy + rz / range * ry;
        if (o->kind == K_BOSS) {
            if ((G.frames / 8) & 1) continue;
            world_rgb(255, 80, 80);
            world_bline(rx - 45, rz, rx, rz + 45); world_bline(rx, rz + 45, rx + 45, rz);
            world_bline(rx + 45, rz, rx, rz - 45); world_bline(rx, rz - 45, rx - 45, rz);
        } else if (o->kind == K_CHUTE) {
            world_rgb(255, 255, 255);
            board_dot(rx, rz);
        } else if (o->heavy) {          /* a big one: a square, red */
            world_rgb(255, 60, 30);
            world_bline(rx - 28, rz - 28, rx + 28, rz - 28); world_bline(rx + 28, rz - 28, rx + 28, rz + 28);
            world_bline(rx + 28, rz + 28, rx - 28, rz + 28); world_bline(rx - 28, rz + 28, rx - 28, rz - 28);
        } else if (o->form) {           /* a formation: a short stroke, in green */
            world_rgb(120, 255, 120);
            world_bline(rx - 14, rz, rx + 14, rz);
        } else {
            world_rgb(255, 200, 0);
            board_dot(rx, rz);
        }
    }
}

/* The cockpit round the head-up display: the instrument panel under its
 * coaming with the hood over the display, three dials, the sills up the
 * sides and the canopy's bows over the head, the display's glass. It is
 * laid out on the board as it is seen, and set COCKPIT_DEPTH in front of
 * the eye (world_board_points), so that it moves with the view and - with
 * SOLID OBJECTS - hides what is behind it as anything solid does: panel,
 * sills and bows are faces, the glass is not. Its lines are always drawn
 * whole (world_set_nocut). The head-up display and the game's own display
 * are board lines and go over it. */

#define COCKPIT_DEPTH 3.0f              /* metres: past the nearest a face may be (SOLID_NEAR in world.c) */

/* A shape given on the board (x, y pairs): its outline, and with `face`
 * its face as well. */
static void cockpit_part(const float *board, int n, int face, int closed)
{
    vp_vec3 p[16];
    int i;
    if (n > 16) n = 16;
    world_board_points(board, n, COCKPIT_DEPTH, p);
    if (face && n <= 8 && world_solid()) { world_group(); world_face(p, n); }
    for (i = 0; i + 1 < n; i++) world_line(p[i], p[i + 1]);
    if (closed) world_line(p[n - 1], p[0]);
}

/* A round dial and its needle, the needle at `value` 0..1 of its sweep. */
static void cockpit_dial(float cx, float cy, float r, float value)
{
    float b[2 * 13], needle[4];
    float a = 3.927f - 4.712f * clampf(value, 0, 1);       /* from lower left round over the top to lower right */
    int i;
    for (i = 0; i < 12; i++) {
        float t = 6.2831853f * (float)i / 12.0f;
        b[2 * i] = cx + r * cosf(t);
        b[2 * i + 1] = cy + r * sinf(t) * (world_vertical() ? 0.75f : 4.0f / 3.0f);
    }
    b[24] = b[0]; b[25] = b[1];
    world_rgb(0, 120, 190);
    cockpit_part(b, 13, 0, 0);
    needle[0] = cx; needle[1] = cy;
    needle[2] = cx + 0.8f * r * cosf(a);
    needle[3] = cy + 0.8f * r * sinf(a) * (world_vertical() ? 0.75f : 4.0f / 3.0f);
    world_rgb(255, 170, 60);
    cockpit_part(needle, 2, 0, 0);
}

static void draw_cockpit(void)
{
    /* the panel: everything below the coaming, its top a low hump */
    static const float panel[] = { -400, -600, -400, 395, 820, 900, 1400, 1000, 2695, 1000, 3275, 900, 4495, 395, 4495, -600 };     /* past the board's bottom and sides: what runs off the bottom is cut to its end (2026-10-21) */
    /* its lit edge and the lines up the sides keep off the board's own edge, where they would come
     * and go with the least shake of the view */
    static const float edge[] = { 14, 566, 820, 900, 1400, 1000, 2695, 1000, 3275, 900, 4081, 566 };
    static const float coaming[] = { 14, 526, 820, 860, 1400, 960, 2695, 960, 3275, 860, 4081, 526 };
    static const float hood[] = { 1400, 1000, 1480, 1100, 2615, 1100, 2695, 1000 };
    static const float glass[] = { 1330, 1000, 1200, 3260, 2895, 3260, 2765, 1000 };
    /* each side: its sill up from the panel, the bow over the head in two pieces */
    static const float sill[] = { 14, 566, 120, 610, 100, 2330, 14, 2300 };
    static const float bow_low[] = { 14, 2300, 100, 2330, 510, 3470, 420, 3500 };
    static const float bow_high[] = { 420, 3500, 510, 3470, 1360, 4081, 1250, 4081 };
    const float *const sides[3] = { sill, bow_low, bow_high };
    float m[8];
    int k, j, i;

    world_level(G.bright[BRIGHT_PLAYER]);
    world_set_nocut(1);
    world_set_front(1);
    world_rgb(0, 150, 220);
    cockpit_part(edge, 6, 0, 0);                        /* the coaming's edge, lit */
    if (world_solid()) {
        vp_vec3 p[8];
        world_board_points(panel, 8, COCKPIT_DEPTH, p);
        world_group();
        world_face(p, 8);
    }
    world_rgb(0, 80, 130);
    cockpit_part(coaming, 6, 0, 0);                     /* the pad along it */
    world_rgb(0, 150, 220);
    cockpit_part(hood, 4, 1, 0);
    for (k = 0; k < 3; k++) {
        for (j = 0; j < 2; j++) {                       /* the left side, then the right as its mirror */
            for (i = 0; i < 4; i++) {
                m[2 * i] = j ? 4095.0f - sides[k][2 * i] : sides[k][2 * i];
                m[2 * i + 1] = sides[k][2 * i + 1];
            }
            world_rgb(0, 150, 220);
            cockpit_part(m, 4, 1, 1);
        }
    }
    world_rgb(0, 90, 150);
    cockpit_part(glass, 4, 0, 0);
    /* the dials, in a row in the middle under the hood: the throttle, the speed, the height */
    cockpit_dial(1610, 815, 95, G.p.throttle);
    cockpit_dial(CX, 830, 95, (G.p.speed - SPEED_MIN) / (SPEED_TOP - SPEED_MIN));
    cockpit_dial(4096.0f - 1610.0f, 815, 95, G.p.pos.y / 1000.0f);
    world_set_front(0);
    world_set_nocut(0);
}

/* The planes in reserve, as small triangles, with the word over them. */

static void draw_lives(int lives)
{
    int i;
    text("LIVES", 150, 330, 70, VP_TEXT_LEFT, 0, 255, 255);
    world_level(G.bright[BRIGHT_TEXT]);
    world_rgb(0, 255, 255);
    for (i = 0; i < lives && i < 8; i++) {
        float x = 200.0f + 150.0f * (float)i, y = 230.0f;
        world_bline(x - 50, y - 40, x, y + 50);
        world_bline(x, y + 50, x + 50, y - 40);
        world_bline(x + 50, y - 40, x - 50, y - 40);
    }
}

/* The enemies still to go before the boss: eight marks, and the number. */

static void draw_kill_marks(int marks, int left)
{
    char buf[16];
    int i;
    world_level(G.bright[BRIGHT_TEXT]);
    for (i = 0; i < KILL_MARKS; i++) {
        float x = CX - 420 + 120.0f * (float)i;
        if (i < marks) world_rgb(255, 200, 0); else world_rgb(50, 40, 0);
        world_bline(x, 200, x + 80, 200);
        world_bline(x + 80, 200, x + 40, 270);
        world_bline(x + 40, 270, x, 200);
    }
    snprintf(buf, sizeof buf, "%d", left);
    text(buf, CX + 620, 190, 90, VP_TEXT_LEFT, 255, 200, 0);
}

/* The game's own display: the score, the era, the planes left, how many
 * enemies are still to go, the radar, and what the game has to say. */

static void draw_hud(void)
{
    char buf[48];
    int i, marks, hp, left = G.kills_per_era - G.kills, per_mark = (G.kills_per_era + KILL_MARKS - 1) / KILL_MARKS;

    if (G.players == 2 && !G.demo) {
        /* whose turn it is, with its score; the other's small under it */
        snprintf(buf, sizeof buf, "P%d %d", G.cur + 1, G.score);
        text(buf, 160, 3850, 120, VP_TEXT_LEFT, 0, 255, 0);
        snprintf(buf, sizeof buf, "P%d %d", 2 - G.cur, G.pl[1 - G.cur].score);
        text(buf, 160, 3700, 80, VP_TEXT_LEFT, 0, 130, 0);
    } else {
        snprintf(buf, sizeof buf, "%d", G.score);
        text(buf, 160, 3850, 120, VP_TEXT_LEFT, 0, 255, 0);
    }
    snprintf(buf, sizeof buf, "HI %d", G.hiscore);
    text(buf, CX, 3850, 100, VP_TEXT_CENTER, 0, 200, 255);
    text(era()->year, 3935, 3850, 120, VP_TEXT_RIGHT, 255, 255, 0);
    if (G.loop > 0) { snprintf(buf, sizeof buf, "LOOP %d", G.loop + 1); text(buf, 3935, 3700, 80, VP_TEXT_RIGHT, 255, 255, 0); }

    if (G.demo) {
        /* Nobody's game: no planes in reserve and no enemies to count. The
         * bottom of the picture is the coin line's, where it always is. */
        draw_radar();
        if (G.state == INTRO && G.level_time < INTRO_TIME + 0.5f)
            text(era()->year, CX, 2680, 280, VP_TEXT_CENTER, 255, 255, 0);
        text("DEMO", CX, 3620, 110, VP_TEXT_CENTER, 255, 255, 255);
        return;
    }

    /* from the year at the top right the beam goes down to the radar,
     * then along the bottom: its way round the picture's edge */
    draw_radar();

    /* the planes in reserve */
    draw_lives(G.lives);

    /* the enemies still to go: eight marks */
    if (left < 0) left = 0;
    marks = (left + per_mark - 1) / per_mark;
    if (G.boss_called) {
        /* what the stage of its fight still takes, under the stage's own word */
        char word[32];
        int last = G.boss_stage + 1 >= bosses[G.era].stages || G.boss_one_hit;
        snprintf(word, sizeof word, "BOSS %s", G.boss_one_hit ? "" : boss_stage()->bar);
        text(word, CX, 150, 90, VP_TEXT_CENTER, 255, last ? 80 : 220, last ? 80 : 0);
        hp = G.boss_one_hit ? 40 : G.boss_total > 0 ? (boss_stage_left() * 40 + G.boss_total - 1) / G.boss_total : 0;
        world_level(G.bright[BRIGHT_TEXT]);
        world_rgb(255, last ? 80 : 220, last ? 80 : 0);
        for (i = 0; i < hp && i < 40; i++) world_bline(CX - 400 + 20.0f * (float)i, 300, CX - 400 + 20.0f * (float)i, 360);
    } else {
        draw_kill_marks(marks, left);
    }

    if (G.state == INTRO && G.level_time < INTRO_TIME + 0.5f)
        text(era()->year, CX, 2680, 280, VP_TEXT_CENTER, 255, 255, 0);
    if (G.state == INTRO && G.players == 2)
        text(G.cur ? "PLAYER 2" : "PLAYER 1", CX, 2330, 200, VP_TEXT_CENTER, 255, 255, 255);
    if (G.state == GAME_OVER) text("GAME OVER", CX, 2600, 300, VP_TEXT_CENTER, 255, 80, 80);
    if (G.demo) text("DEMO", CX, 3620, 110, VP_TEXT_CENTER, 255, 255, 255);
    else if (G.notice_time > 0 && G.notice) text(G.notice, CX, 3500, 110, VP_TEXT_CENTER, 255, 255, 255);
}

/* What cheer() said: out of the middle, growing, through the colours. */

static void draw_cheer(void)
{
    float t = G.cheer_time / 1.3f, h;
    int r, g, b;
    if (!G.cheer || t >= 1.0f || G.demo) return;
    h = 70.0f + 520.0f * t * t;
    hue_rgb(G.cheer_time * 2.2f, &r, &g, &b);
    world_level(G.bright[BRIGHT_TEXT] * (1.0f - t * t) * 1.4f);
    world_rgb(r, g, b);
    world_text(G.cheer, CX, 2300.0f - h * 0.5f, h, VP_TEXT_CENTER);
}

/* Over the joystick's test flight: what it is, the sensitivity, and
 * which button does what (game.c, update_practice). */

static void draw_practice(void)
{
    char buf[48];
    text("TEST FLIGHT", CX, 3380, 130, VP_TEXT_CENTER, 255, 255, 0);
    snprintf(buf, sizeof buf, "STICK SENSITIVITY %d.%02d", (int)(G.yoke_sens + 0.001f), (int)(G.yoke_sens * 100.0f + 0.5f) % 100);
    text(buf, CX, 3200, 110, VP_TEXT_CENTER, 255, 255, 255);
    text("FIRE +  LOOK BACK -   FASTER ++  SLOWER --", CX, 1300, 75, VP_TEXT_CENTER, 0, 255, 120);
    if ((G.frames / 30) & 1) text("START - DONE", CX, 1170, 90, VP_TEXT_CENTER, 255, 255, 255);
}

/* The last plane is lost: the picture stands, and over it the question
 * and the seconds left to answer it. */

void draw_continue(void)
{
    char buf[16];
    int left = (int)ceilf(G.cont_time);
    int can = G.credits.free_play || vp_credits_enough(&G.credits, 1);

    world_fade(1.0f);
    if (G.players == 2) text(G.cur ? "PLAYER 2" : "PLAYER 1", CX, 3250, 130, VP_TEXT_CENTER, 255, 255, 255);
    text("CONTINUE?", CX, 2850, 260, VP_TEXT_CENTER, 255, 255, 0);
    snprintf(buf, sizeof buf, "%d", left < 0 ? 0 : left);
    text(buf, CX, 1700, 700, VP_TEXT_CENTER, 255, 80, 80);
    if ((G.frames / 20) & 1) text(can ? "PRESS START" : "INSERT COIN", CX, 1350, 150, VP_TEXT_CENTER, 255, 255, 255);
    text("ANY OTHER BUTTON - COUNT FASTER", CX, 960, 70, VP_TEXT_CENTER, 0, 150, 0);
}

/* Not the original's: it has no coin slot. */

void draw_coin_line(void)
{
    char buf[32];
    const char *ask = vp_credits_enough(&G.credits, 2) ? "PRESS 1 OR 2 PLAYER START"
                    : vp_credits_enough(&G.credits, 1) ? "PRESS START" : "INSERT COIN";

    /* always in the same two places at the bottom, on every page of
     * attract mode and over the demo */
    if (G.state == TUTORIAL) {              /* how to play before the first era: it can be skipped */
        if ((G.frames / 30) & 1) text("START - SKIP", CX, 520, 120, VP_TEXT_CENTER, 255, 255, 255);
        return;
    }
    if (G.credits.free_play) snprintf(buf, sizeof buf, "FREE PLAY");
    else snprintf(buf, sizeof buf, "CREDITS %d", G.credits.credits);
    text(buf, CX, 330, 90, VP_TEXT_CENTER, 0, 255, 0);
    if ((G.frames / 30) & 1) text(ask, CX, 520, 120, VP_TEXT_CENTER, 255, 255, 255);
}

/* The land, the sky and everything in it, from the camera that is set. */

static void draw_scene(int era_index, float grow)
{
    vp_vec3 eye = vp3d_eye(world_view());
    world_level(G.bright[BRIGHT_GROUND]);
    world_rgb(0, 200, 90);
    /* With the beam at its limit the ground is drawn with every second
     * line, and the sky without its clouds: the fight goes before the view.
     * Each switches back only well under where it switched (2026-10-21: at
     * the edge it went from one to the other every few frames, and the
     * grid flickered). */
    static int coarse, no_sky;
    float load = world_load();
    coarse = coarse ? load > 0.85f : load > 0.95f;
    no_sky = no_sky ? load > 0.95f : load > 1.05f;
    world_draw_ground(eye, G.view_heading, coarse);
    world_level(G.bright[BRIGHT_GROUND] * 1.4f);
    /* The city as far as it goes, in every era and view (2026-10-21: the
     * shorter reach of 0.35.1 was for a monitor whose left edge gave way
     * under the load; the monitor is mended). */
    city_set_reach(920.0f);
    if (!G.practice) city_draw(era_index, eye, (float)G.frames / 60.0f, grow);     /* the test flight: the ground alone */
    world_level(G.bright[BRIGHT_CLOUDS]);
    if (no_sky || G.practice) {
        /* nothing in the sky this frame */
    } else if (era_index == 4) {               /* 2001 is far out: rocks drift where the clouds were, and hurt nobody */
        world_rgb(200, 160, 120);
        world_draw_asteroids(eye, (float)G.frames / 60.0f);
    } else {
        world_rgb(235, 238, 245);           /* white, a breath of blue (2026-10-21: they were blue) */
        world_draw_clouds(eye);
    }
    draw_objects();
}

/* The camera: in the cockpit, or behind the plane. */

static int set_camera(void)
{
    int chase = G.chase || !G.p.alive, rear = G.p.rear && G.p.alive;
    /* a hit shakes the view: it rolls, nods and jumps, less and less */
    float k = G.p.alive ? G.p.shake : 0, f = (float)G.frames;
    float shake_roll = k * 0.09f * sinf(f * 1.9f), shake_pitch = k * 0.035f * sinf(f * 2.7f + 1.0f);
    vp_vec3 jolt = vp_v3(k * 0.8f * sinf(f * 3.3f), k * 0.8f * sinf(f * 2.3f + 2.0f), 0);
    float look_pitch = 0;
    if (chase) {
        /* behind and above the plane; looking back, ahead of it and
         * above, so the plane is seen from the front with what follows it */
        vp_vec3 ahead = world_forward(G.p.cam_heading, G.p.cam_pitch);
        vp_vec3 eye = vp_v3_add(vp_v3_add(G.p.pos, vp_v3_scale(ahead, rear ? 30.0f : -30.0f)), vp_v3(0, 9.0f, 0));
        if (eye.y < 4.0f) eye.y = 4.0f;
        G.view_heading = rear ? wrap_angle(G.p.cam_heading + PI) : G.p.cam_heading;
        look_pitch = G.p.cam_pitch;
        /* it looks a little down on the plane - up, when both are on their
         * backs - and rolls a quarter of the plane's bank and all of the
         * roll that turns it upright */
        world_pose(vp_v3_add(eye, jolt), G.view_heading,
                   (rear ? -G.p.cam_pitch : G.p.cam_pitch) - 0.1f * clampf(cosf(G.p.cam_pitch) * 4.0f, -1, 1) + shake_pitch,
                   (G.p.roll * 0.25f + G.p.flip_roll) * (rear ? -1.0f : 1.0f) + shake_roll);
    } else if (rear) {
        /* over the shoulder: the same horizon, the other way round */
        vp_vec3 eye = vp_v3_sub(G.p.pos, vp_v3_scale(player_forward(), 3.0f));
        G.view_heading = wrap_angle(G.p.heading + PI);
        look_pitch = G.p.pitch;
        world_pose(vp_v3_add(eye, jolt), G.view_heading, -G.p.pitch + shake_pitch, -player_roll() + shake_roll);
    } else {
        vp_vec3 eye = vp_v3_add(G.p.pos, vp_v3_scale(player_forward(), 3.0f));
        G.view_heading = G.p.heading;
        look_pitch = G.p.pitch;
        world_pose(vp_v3_add(eye, jolt), G.p.heading, G.p.pitch + shake_pitch, player_roll() + shake_roll);
    }
    G.view_heading = course_of(G.view_heading, look_pitch);     /* on its back: the horizon is the other way */
    return chase;
}

/* The boss's fall, watched from where the camera was set down when it was
 * shot: it turns to follow the wreck to the ground, shaking when it hits.
 * A bar above and below, as in a film; the score stays. */

static void draw_bossfall(void)
{
    float k = G.p.shake, f = (float)G.frames;
    vp_vec3 eye = vp_v3_add(G.fall_eye, vp_v3(k * 3.0f * sinf(f * 3.3f), k * 3.0f * sinf(f * 2.3f + 2.0f), 0));
    vp_vec3 look = G.fall_at;
    char buf[32];

    if (eye.y < 12.0f) eye.y = 12.0f;
    if (look.y < 10.0f) look.y = 10.0f;
    if (vp3d_look_at(world_view(), eye, look, vp_v3(0, 1, 0)))
        G.view_heading = atan2f(look.x - eye.x, look.z - eye.z);
    /* letterbox: a line above and below, and nothing drawn outside them */
    vp_dvg_set_clip(world_dvg(), 0, 500, VP_DVG_COORD_MAX, 3600);
    draw_scene(G.era, 1.0f);
    draw_player_plane();
    draw_cheer();
    snprintf(buf, sizeof buf, "%d", G.score);
    text(buf, 160, 560, 110, VP_TEXT_LEFT, 0, 255, 0);
    text(era()->year, 3935, 560, 110, VP_TEXT_RIGHT, 255, 255, 0);
    world_flush();
    vp_dvg_reset_clip(world_dvg());
    letterbox_lines(500.0f, 3600.0f);
}

/* The boss's films, between the two bars. Its coming (BOSSINTRO): the
 * camera goes round it as it flies in with two of the era's planes beside
 * it, closing up; WARNING blinks; under it its type is typed out letter
 * by letter between two rules, as on a file, and then what is to be done.
 * And between two stages of its fight (BOSSTURN): close on it from the
 * side while what it fights with next comes out of it, with that thing's
 * name. The squares on its parts say what to shoot at (draw_boss_parts). */
static void draw_bossintro(void)
{
    const struct boss *b = &bosses[G.era];
    const struct era *e = era();
    const obj *o = NULL;
    int turn = G.state == BOSSTURN;
    float t = G.state_time, r, ang, away;
    vp_vec3 eye;
    char buf[64];
    int i, shown;

    for (i = 0; i < MAX_OBJ; i++) if (G.objs[i].kind == K_BOSS) { o = &G.objs[i]; break; }
    if (!o) return;
    r = model_radius(o->model) * b->size;
    if (turn) {
        /* from its side and a little below, slowly round to its front */
        ang = o->heading + 1.9f - 0.9f * smooth(t / boss_turn_time());
        if (G.era == 4 && G.boss_stage >= 2) {
            /* the robot stands far taller than the saucer is: further back, so that all of it is in the picture,
             * and by the saucer's size all through, so that the camera does not jump when it has become the robot */
            r = model_radius(MODEL_MOTHERSHIP) * b->size;
            away = r * 4.3f;
        } else
        away = r * 2.6f;
        eye = vp_v3_add(o->pos, vp_v3(sinf(ang) * away, -r * 0.35f + r * 0.5f * smooth(t / boss_turn_time()), cosf(ang) * away));
    } else {
        /* from ahead of it and to one side, round its nose to the other side, nearer all the time */
        ang = o->heading + 0.9f - 2.6f * smooth(t / BOSS_INTRO_TIME);
        away = r * (4.4f - 1.9f * smooth(t / 5.0f));
        eye = vp_v3_add(o->pos, vp_v3(sinf(ang) * away, r * (0.25f + 0.5f * sinf(t * 0.45f)), cosf(ang) * away));
    }
    if (eye.y < 12.0f) eye.y = 12.0f;
    if (vp3d_look_at(world_view(), eye, o->pos, vp_v3(0, 1, 0)))
        G.view_heading = atan2f(o->pos.x - eye.x, o->pos.z - eye.z);

    vp_dvg_set_clip(world_dvg(), 0, 500, VP_DVG_COORD_MAX, 3600);
    world_fade(smooth(t / 0.6f));
    draw_scene(G.era, 1.0f);
    if (!turn) {
        /* its escort: two of the era's own, back on either side and a little
         * below - the Chinook's own gunships - their rotors turning */
        vp_vec3 fwd = forward_of(o), right = vp_v3(cosf(o->heading), 0, -sinf(o->heading));
        int model = G.era == 2 ? MODEL_GUNSHIP : e->model, k;
        world_level(G.bright[BRIGHT_ENEMIES]);
        for (k = -1; k <= 1; k += 2) {
            vp_vec3 at = vp_v3_add(o->pos, vp_v3_add(vp_v3_scale(right, 1.25f * r * (float)k),
                                   vp_v3_add(vp_v3_scale(fwd, -0.9f * r), vp_v3(0, -0.25f * r + 3.0f * sinf(t * 1.3f + (float)k), 0))));
            vp_xf place = vp_xf_mul2(world_place(at, o->heading, 0, 0.12f * (float)k), vp_xf_scale(ENEMY_SCALE, ENEMY_SCALE, ENEMY_SCALE));
            obj spun;
            model_draw(model, &place, TINT_OWN);
            memset(&spun, 0, sizeof spun);
            spun.model = model;
            spun.spin = t * 25.0f + (float)k;
            draw_rotors(&spun, &place);
        }
    }
    world_fade(1.0f);

    if (turn) {
        /* what it brings out, typed between two rules */
        const char *title = boss_stage()->title ? boss_stage()->title : "";
        shown = (int)((t - 0.4f) * 14.0f);
        if (shown > 0) {
            snprintf(buf, sizeof buf, "%.*s", shown, title);
            text(buf, CX, 900, strlen(title) > 14 ? 165.0f : 230.0f, VP_TEXT_CENTER, 255, 220, 0);
            world_level(G.bright[BRIGHT_TEXT]);
            world_rgb(255, 220, 0);
            world_bline(500, 1190, 3595, 1190);
            world_bline(3595, 850, 500, 850);
        }
        if (t > 2.0f) text(boss_stage()->goal, CX, 640, 100, VP_TEXT_CENTER, 255, 255, 255);
    } else {
        /* WARNING, between two red rules, blinking */
        if (t < 5.0f && ((G.frames / 10) & 1)) {
            text("WARNING", CX, 3180, 280, VP_TEXT_CENTER, 255, 50, 40);
            world_level(G.bright[BRIGHT_TEXT]);
            world_rgb(255, 50, 40);
            world_bline(700, 3520, 3395, 3520);
            world_bline(3395, 3120, 700, 3120);
        }
        /* its type, typed out; then what is to be done */
        shown = (int)((t - 1.0f) * 12.0f);
        if (shown > 0) {
            snprintf(buf, sizeof buf, "%.*s", shown, b->name);
            text(buf, CX, 900, strlen(b->name) > 15 ? 150.0f : 210.0f, VP_TEXT_CENTER, 255, 255, 255);
            world_level(G.bright[BRIGHT_TEXT]);
            world_rgb(200, 200, 200);
            world_bline(500, 1170, 3595, 1170);
            world_bline(3595, 850, 500, 850);
            text("ENEMY CLASS", 520, 1200, 70, VP_TEXT_LEFT, 200, 200, 200);
        }
        if (t > 3.6f) text(boss_stage()->goal, CX, 640, 100, VP_TEXT_CENTER, 255, 220, 0);
    }
    world_flush();
    vp_dvg_reset_clip(world_dvg());
    letterbox_lines(500.0f, 3600.0f);
}

/* --- the journey through time -----------------------------------------------------
 *
 * Fourteen seconds, and a game of its own. The era the plane leaves fades
 * and what is left of it falls away; the tunnel opens in that era's colour
 * and turns to the colour of the one ahead. The plane is carried in, then
 * it is the player's to fly: the tunnel bends, and the plane has to follow
 * it (update_tunnel). At the far end a clock runs with its hands flying,
 * and the year counts from the one to the other. At the end the plane is
 * carried out, everything goes white lines, and the new era's buildings
 * rise out of its ground. */

/* The energy left, for where the head-up display is not drawn. */

static void draw_energy(float y)
{
    const float x0 = CX - 520.0f, x1 = CX + 520.0f, h = 34.0f;
    float e = clampf(G.p.energy / ENERGY_FULL, 0, 1);
    int i;

    world_level(G.bright[BRIGHT_PLAYER]);
    world_rgb(70, 110, 140);
    world_bline(x0, y - h, x1, y - h);
    world_bline(x1, y - h, x1, y + h);
    world_bline(x1, y + h, x0, y + h);
    world_bline(x0, y + h, x0, y - h);
    if (e <= 0 || (e < 0.25f && ((G.frames / 8) & 1))) return;
    if (e < 0.25f) world_rgb(255, 60, 40); else if (e < 0.5f) world_rgb(255, 220, 0); else world_rgb(0, 255, 120);
    for (i = -1; i <= 1; i++)
        world_bline(x0 + 8.0f, y + (float)i * 14.0f, x0 + 8.0f + (x1 - x0 - 16.0f) * e, y + (float)i * 14.0f);
}

/* The tunnel's mouth, in the era's sky: the first rings of the tunnel
 * itself, standing in a row along the way in, as wide and as far apart as
 * they are inside, in the colour of the era left; round the first a ring
 * in the colour of the one ahead, pulsing, and short rays. It opens out
 * of a point. */
static void draw_portal_at(float grow);

static void draw_portal(float t) { draw_portal_at(smooth(t / 1.4f)); }

/* The mouth `grow` (0..1) open. Opening, its first ring is a flash. */
static void draw_portal_at(float grow)
{
    const float *from = eras[G.warp_from].colour, *to = eras[G.warp_to].colour;
    vp_vec3 right = vp_v3_normalize(vp_v3_cross(vp_v3(0, 1, 0), G.tun.axis)), up = vp_v3_cross(G.tun.axis, right);
    float f = (float)G.frames;
    vp_vec3 corner[6][8];
    int k, j;

    world_level(G.bright[BRIGHT_EFFECTS]);
    for (k = 0; k < 6; k++) {
        vp_vec3 mid = vp_v3_add(G.tun.portal, vp_v3_scale(G.tun.axis, TUNNEL_STEP * (float)k));
        float white = (k & 1) ? 0.55f : 0.0f, level = 1.0f - 0.13f * (float)k;
        world_rgb((int)((from[0] * (1.0f - white) + 255.0f * white) * level),
                  (int)((from[1] * (1.0f - white) + 255.0f * white) * level),
                  (int)((from[2] * (1.0f - white) + 255.0f * white) * level));
        for (j = 0; j < 8; j++) {
            float ang = TUNNEL_STEP * (float)k * 0.007f + PI2 * (float)j / 8.0f;
            corner[k][j] = vp_v3_add(mid, vp_v3_add(vp_v3_scale(right, TUNNEL_RADIUS * grow * cosf(ang)),
                                                    vp_v3_scale(up, TUNNEL_RADIUS * grow * sinf(ang))));
        }
        for (j = 0; j < 8; j++) world_line(corner[k][j], corner[k][(j + 1) % 8]);
    }
    world_rgb((int)(from[0] * 0.45f), (int)(from[1] * 0.45f), (int)(from[2] * 0.45f));
    for (j = 0; j < 8; j += 2)          /* the tunnel's long lines */
        for (k = 0; k < 5; k++) world_line(corner[k][j], corner[k + 1][j]);
    {
        /* the ring round the mouth, turning, and its rays */
        float r = TUNNEL_RADIUS * grow * (1.16f + 0.04f * sinf(f * 0.15f));
        vp_vec3 prev = vp_v3(0, 0, 0);
        world_rgb((int)to[0], (int)to[1], (int)to[2]);
        for (j = 0; j <= 8; j++) {
            float ang = f * 0.03f + PI2 * (float)(j % 8) / 8.0f;
            vp_vec3 q = vp_v3_add(G.tun.portal, vp_v3_add(vp_v3_scale(right, r * cosf(ang)), vp_v3_scale(up, r * sinf(ang))));
            if (j) world_line(prev, q);
            prev = q;
        }
        world_rgb(255, 255, 255);
        for (j = 0; j < 8; j++) {
            float ang = -f * 0.02f + PI2 * (float)j / 8.0f + 0.2f;
            float r0 = r * 1.08f, r1 = r * (1.22f + 0.1f * sinf(f * 0.2f + (float)j)) + 60.0f * (1.0f - grow);
            vp_vec3 dir = vp_v3_add(vp_v3_scale(right, cosf(ang)), vp_v3_scale(up, sinf(ang)));
            world_line(vp_v3_add(G.tun.portal, vp_v3_scale(dir, r0)), vp_v3_add(G.tun.portal, vp_v3_scale(dir, r1)));
        }
    }
}

/* The camera of the way in: it looks at the plane from behind and beside
 * it, comes round behind it and closes up, its look going from the plane
 * to the mouth; it ends exactly where the tunnel's own eye is as the
 * plane comes in - TUNNEL_AHEAD behind the plane, TUNNEL_EYE_IN over it,
 * looking along the way - so the plane does not move in the picture when
 * the one view gives way to the other. */
static void approach_camera(float t)
{
    /* From the side while the mouth opens and the plane loops (the eye
     * goes with where the plane would be without its loop, so the loop is
     * seen whole); then round behind it for the run, ending exactly where
     * the tunnel's own eye is as the plane comes in - TUNNEL_AHEAD behind,
     * TUNNEL_EYE_IN over it, looking along the way - so the plane does not
     * move in the picture when the one view gives way to the other. */
    const float t_run = APPROACH_OPEN + APPROACH_LOOP;
    float close = smooth((t - t_run) / (TUNNEL_APPROACH - t_run));
    vp_vec3 up = vp_v3(0, 1, 0), right = vp_v3_normalize(vp_v3_cross(up, G.tun.axis));
    vp_vec3 at = vp_v3_add(vp_v3_scale(G.tun.base, 1.0f - close), vp_v3_scale(G.p.pos, close));
    float back = 40.0f + (TUNNEL_AHEAD - 40.0f) * close, high = 30.0f + (TUNNEL_EYE_IN - 30.0f) * close;
    float side = 120.0f * (1.0f - close);
    vp_vec3 eye = vp_v3_add(vp_v3_sub(at, vp_v3_scale(G.tun.axis, back)), vp_v3_add(vp_v3_scale(up, high), vp_v3_scale(right, side)));
    float watch = smooth((t - 0.8f) / 1.0f);        /* first the mouth, then the plane */
    vp_vec3 early = vp_v3_add(vp_v3_scale(G.tun.portal, 1.0f - watch), vp_v3_scale(G.p.pos, watch));
    vp_vec3 late = vp_v3_add(vp_v3_add(G.p.pos, vp_v3_scale(G.tun.axis, 8.0f + 3000.0f * close * close)), vp_v3_scale(up, high * close));
    vp_vec3 look = vp_v3_add(vp_v3_scale(early, 1.0f - close), vp_v3_scale(late, close));

    if (eye.y < 4.0f) eye.y = 4.0f;
    if (vp3d_look_at(world_view(), eye, look, up))
        G.view_heading = atan2f(look.x - eye.x, look.z - eye.z);
}

/* The way into an era (LEVELIN), between the bars: from beside where the
 * plane will be, the tunnel is seen opening in the sky behind it; the
 * plane bursts out of it and comes on past; the mouth closes behind it.
 * The year is typed out between two rules. */
static void draw_levelin(void)
{
    float t = G.state_time, open = smooth(t / 1.4f), shut = 1.0f - smooth((t - (LEVEL_IN_OPEN + 1.6f)) / 1.0f);
    vp_vec3 up = vp_v3(0, 1, 0), right = vp_v3_normalize(vp_v3_cross(up, G.tun.axis));
    vp_vec3 eye = vp_v3_add(vp_v3_add(G.tun.from, vp_v3_scale(G.tun.axis, 70.0f)), vp_v3_add(vp_v3_scale(right, 95.0f), vp_v3_scale(up, 18.0f)));
    float watch = smooth((t - LEVEL_IN_OPEN) / 1.4f);
    vp_vec3 look = vp_v3_add(vp_v3_scale(G.tun.portal, 1.0f - watch), vp_v3_scale(G.p.pos, watch));
    char buf[48];
    int shown;

    if (eye.y < 12.0f) eye.y = 12.0f;
    if (vp3d_look_at(world_view(), eye, look, up))
        G.view_heading = atan2f(look.x - eye.x, look.z - eye.z);
    vp_dvg_set_clip(world_dvg(), 0, 500, VP_DVG_COORD_MAX, 3600);
    world_fade(smooth(t / 0.8f));
    draw_scene(G.era, smooth(t / 3.0f));
    world_fade(1.0f);
    if (open * shut > 0.01f) {
        /* its rings run back from the mouth, away from where the plane comes out */
        vp_vec3 keep = G.tun.axis;
        G.tun.axis = vp_v3_scale(keep, -1.0f);
        draw_portal_at(open * shut);
        G.tun.axis = keep;
    }
    if (t >= LEVEL_IN_OPEN) draw_player_plane();
    /* the year, typed out between two rules, and the stage */
    shown = (int)((t - 1.2f) * 12.0f);
    if (shown > 0) {
        const float *c = eras[G.era].colour;
        snprintf(buf, sizeof buf, "%.*s", shown, eras[G.era].year);
        text("A.D.", CX - 820.0f, 960, 110, VP_TEXT_RIGHT, 200, 200, 200);
        text(buf, CX, 900, 300, VP_TEXT_CENTER, (int)c[0], (int)c[1], (int)c[2]);
        world_level(G.bright[BRIGHT_TEXT]);
        world_rgb(200, 200, 200);
        world_bline(500, 1250, 3595, 1250);
        world_bline(3595, 830, 500, 830);
        if (t > 2.6f) {
            snprintf(buf, sizeof buf, "STAGE %d", G.level + 1);
            text(buf, CX, 640, 100, VP_TEXT_CENTER, 255, 255, 255);
        }
    }
    world_flush();
    vp_dvg_reset_clip(world_dvg());
    letterbox_lines(500.0f, 3600.0f);
}

/* The two lines of the film's bars, at these heights. */
static void letterbox_lines(float low, float high)
{
    world_level(G.bright[BRIGHT_TEXT]);
    world_rgb(120, 120, 140);
    world_bline(0, high, 4095, high);
    world_bline(4095, low, 0, low);
}

static void draw_warp(void)
{
    float t = G.state_time, tt = t - TUNNEL_APPROACH;
    int from_year = atoi(eras[G.warp_from].year), to_year = atoi(eras[G.warp_to].year);
    float u = smooth((tt - 1.0f) / (TUNNEL_TIME - 2.6f));
    float in = smooth(tt / 0.7f), out = 1.0f - smooth((tt - (TUNNEL_TIME - 0.8f)) / 0.8f);
    int flown = tt >= TUNNEL_IN && tt <= TUNNEL_TIME - TUNNEL_OUT && !G.tun.lost;
    /* the film's bars of the boss's fall stay for the way in, and open
     * as the plane goes through the mouth: the tunnel has the whole picture */
    int bars = tt < TUNNEL_MOUTH;
    float open = smooth(tt / TUNNEL_MOUTH), bar_low = 500.0f * (1.0f - open), bar_high = 3600.0f + 495.0f * open;
    float fx = CX, fy = CY;
    char buf[32];

    if (bars) vp_dvg_set_clip(world_dvg(), 0, (int)bar_low, VP_DVG_COORD_MAX, (int)bar_high);

    /* The way in: the era still round the plane, the tunnel's mouth ahead
     * growing until the plane is through its first ring; the era fades in
     * the last half second, the mouth's rings as the tunnel's own come up. */
    if (tt < TUNNEL_MOUTH) {
        float era_left = 1.0f - smooth((t - (TUNNEL_APPROACH - 0.7f)) / 0.8f);
        approach_camera(t);
        if (era_left > 0.02f) {
            world_fade(era_left);
            draw_scene(G.warp_from, 1.0f);
        }
        world_fade(1.0f - smooth(tt / TUNNEL_MOUTH));
        draw_portal(t);
        world_fade(1.0f);
        if (tt < 0) {
            draw_player_plane();
            if ((G.frames / 8) & 1) text("TIME WARP", CX, 3250, 220, VP_TEXT_CENTER, 255, 255, 255);
            snprintf(buf, sizeof buf, "%d", G.score);
            text(buf, 160, 560, 110, VP_TEXT_LEFT, 0, 255, 0);
            text(eras[G.warp_from].year, 3935, 560, 110, VP_TEXT_RIGHT, 255, 255, 0);
            world_flush();
    vp_dvg_reset_clip(world_dvg());
            letterbox_lines(bar_low, bar_high);
            return;
        }
    }

    /* the tunnel, from just behind and above the plane */
    world_fade(in * out);
    world_draw_tunnel(G.tun.dist, G.tun.x, G.tun.y + 7.0f, G.tun.amp, 1.0f, eras[G.warp_from].colour,
                      eras[G.warp_to].colour, smooth((tt / TUNNEL_TIME - 0.2f) / 0.6f), &fx, &fy,
                      flown ? TUNNEL_PERFECT : 0, TUNNEL_AHEAD);
    /* the clock at the end of the way: forwards, or back to 1910 */
    world_level(G.bright[BRIGHT_EFFECTS]);
    world_rgb(255, 255, 255);
    world_draw_clock(fx, fy, 95.0f, (G.warp_to > G.warp_from ? 1.0f : -1.0f) * tt * tt * 0.09f);

    world_pose(vp_v3(0, 0, 0), 0, 0, 0);
    world_fade(out);                    /* the plane is there at once: it is the one that flew in */
    if (!G.tun.lost) {
        /* the plane: it banks the way it slides, and at the end it is off
         * into the new time */
        float away = TUNNEL_AHEAD + 420.0f * smooth((tt - (TUNNEL_TIME - 1.3f)) / 1.3f);
        float under = TUNNEL_EYE_IN + (TUNNEL_EYE - TUNNEL_EYE_IN) * smooth(tt / 1.2f);
        vp_xf place = world_place(vp_v3(0, -under, away), G.tun.vx / TUNNEL_SIDE * 0.25f, G.tun.vy / TUNNEL_UP * 0.3f,
                                  -G.tun.vx / TUNNEL_SIDE * 0.9f);
        world_level(G.bright[BRIGHT_PLAYER]);
        model_draw(MODEL_PLAYER, &place, G.p.hurt > 0 ? TINT_WHITE : TINT_OWN);
        /* the burner shows the throttle; at the end it is full, whatever it was */
        draw_burner(&place, clampf(0.25f + 0.75f * G.tun.throttle + smooth((tt - (TUNNEL_TIME - 1.6f)) / 1.0f), 0, 1));
    }
    world_fade(in * out);
    world_fade(1.0f);

    /* the last second: white streaks from the middle */
    if (tt > TUNNEL_TIME - 1.6f) {
        world_fade(smooth((tt - (TUNNEL_TIME - 1.6f)) / 0.8f) * (1.0f - smooth((tt - (TUNNEL_TIME - 0.4f)) / 0.4f)));
        world_level(G.bright[BRIGHT_EFFECTS]);
        world_rgb(255, 255, 255);
        world_draw_warp(tt * 1.4f, 7u);
        world_fade(1.0f);
    }
    if (G.p.hurt > 0) {                 /* the wall: the edge of the picture flashes */
        world_level(G.bright[BRIGHT_TEXT]);
        world_rgb(255, 60, 40);
        world_bline(40, 40, 4055, 40); world_bline(4055, 40, 4055, 4055);
        world_bline(4055, 4055, 40, 4055); world_bline(40, 4055, 40, 40);
    }

    /* the year, counting */
    snprintf(buf, sizeof buf, "%d", (int)lroundf((float)from_year + (float)(to_year - from_year) * u));
    {
        const float *a = eras[G.warp_from].colour, *b = eras[G.warp_to].colour;
        text(buf, CX, 3350, 260, VP_TEXT_CENTER,
             (int)(a[0] + (b[0] - a[0]) * u), (int)(a[1] + (b[1] - a[1]) * u), (int)(a[2] + (b[2] - a[2]) * u));
    }
    if (tt < TUNNEL_IN + 2.2f && ((G.frames / 8) & 1)) text("FLY THROUGH THE GREEN RINGS", CX, 2900, 150, VP_TEXT_CENTER, 0, 255, 120);
    if (G.tun.lost) text("PLANE LOST", CX, 2100, 260, VP_TEXT_CENTER, 255, 60, 40);
    else if (flown && G.tun.off > 0.75f && ((G.frames / 6) & 1)) text("WALL", CX, 2700, 200, VP_TEXT_CENTER, 255, 60, 40);

    /* the score, what the tunnel has brought, and the energy */
    snprintf(buf, sizeof buf, "%d", G.score);
    text(buf, 160, 3850, 120, VP_TEXT_LEFT, 0, 255, 0);
    if (flown) {
        /* the throttle, and what it makes a ring worth */
        snprintf(buf, sizeof buf, "SPEED %d%%  RING X%d.%d", (int)(100.0f * (G.tun.throttle < THROTTLE_MID
                 ? TUNNEL_SLOW + (1.0f - TUNNEL_SLOW) * G.tun.throttle / THROTTLE_MID
                 : 1.0f + (TUNNEL_FAST - 1.0f) * (G.tun.throttle - THROTTLE_MID) / (1.0f - THROTTLE_MID)) + 0.5f),
                 (int)(0.5f + G.tun.throttle), (int)((0.5f + G.tun.throttle) * 10.0f + 0.5f) % 10);
        text(buf, 160, 3650, 80, VP_TEXT_LEFT, 255, 220, 0);
        if (G.tun.good > 0) {
            snprintf(buf, sizeof buf, "PERFECT %d", G.tun.good_points);
            text(buf, CX, HUD_ENERGY_Y + 130.0f, 100, VP_TEXT_CENTER, 0, 255, 120);
        }
    }
    snprintf(buf, sizeof buf, "PERFECT %d OF %d", G.tun.perfect, G.tun.rings);
    text(buf, 3935, 3850, 90, VP_TEXT_RIGHT, 0, 255, 120);
    if (tt > TUNNEL_TIME - TUNNEL_OUT) {
        snprintf(buf, sizeof buf, "TUNNEL BONUS %d", G.tun.bonus);
        text(buf, CX, 2800, 200, VP_TEXT_CENTER, 255, 255, 0);
        if (G.tun.rings > 0 && G.tun.perfect == G.tun.rings) text("PERFECT FLIGHT", CX, 2450, 200, VP_TEXT_CENTER, 0, 255, 120);
    }
    draw_energy(HUD_ENERGY_Y);
    if (bars) {
        world_flush();
    vp_dvg_reset_clip(world_dvg());
        world_fade(1.0f - open);
        letterbox_lines(bar_low, bar_high);
        world_fade(1.0f);
    }
}

static void draw_frame(vp_dvg *d);

void game_draw(vp_game *g, vp_dvg *d)
{
    (void)g;
    world_set_solid(G.occlusion);
    draw_frame(d);
    world_flush();          /* what is kept of the 3D world (solid buildings): drawn now */
    if (G.cont_time > 0 && !G.service && !G.setup) draw_continue();     /* over the picture that stands */
}

static void draw_frame(vp_dvg *d)
{
    int chase;
    world_begin(d);
    world_fade(1.0f);
    /* the picture's size: the setting, or what the open service menu has it at just now */
    vp_dvg_set_scale(d, (float)(G.service ? service_picture_size() : G.picture_size) / 100.0f);
    if (G.service) { service_draw(); return; }
    if (G.setup) { setup_draw(); return; }

    if (G.state == ATTRACT) {
        if (G.page == PAGE_TITLE) draw_title();
        else if (G.page == PAGE_SCORES) draw_scores();
        else if (G.page == PAGE_HOWTO) { if (G.control_stick) draw_howto_stick(); else draw_howto(); }
        else if (G.page == PAGE_ITEMS) draw_items_page(0);
        else draw_cast();
        return;
    }
    if (G.state == INITIALS) { draw_initials(); return; }
    if (G.state == TUTORIAL) {
        /* the yoke's, whatever the cabinet is flown with; then the items */
        if (G.state_time < HOWTO_TIME) { if (G.control_stick) draw_howto_stick(); else draw_howto(); }
        else draw_items_page(1);
        return;
    }
#if DEBUG_LEVEL_SELECT
    if (G.state == DEBUGSEL) {              /* FOR TESTING, TO BE TAKEN OUT: where to begin */
        char buf[64];
        int i, rows = debug_rows();
        text("DEBUG - START WHERE", CX, 3720, 130, VP_TEXT_CENTER, 255, 255, 0);
        for (i = 0; i < rows; i++) {
            int here = i == G.debug_sel, era_of, stage, lit;
            debug_row(i, &era_of, &stage);
            if (stage < 0) snprintf(buf, sizeof buf, "%s%s", here ? "> " : "", eras[era_of].year);
            else snprintf(buf, sizeof buf, "%s%s BOSS %d - %s", here ? "> " : "", eras[era_of].year, stage + 1, bosses[era_of].stage[stage].bar);
            lit = here ? 255 : stage < 0 ? 170 : 110;
            text(buf, stage < 0 ? 900.0f : 1150.0f, 3470.0f - 172.0f * (float)i, here ? 120 : 95, VP_TEXT_LEFT, lit, lit, lit);
        }
        text("YOKE - CHOOSE   FIRE OR START - GO", CX, 300, 90, VP_TEXT_CENTER, 0, 150, 0);
        return;
    }
#endif
    if (G.state == WARP) { draw_warp(); return; }
    if (G.state == BOSSFALL) { draw_bossfall(); return; }
    if (G.state == BOSSINTRO || G.state == BOSSTURN) { draw_bossintro(); return; }
    if (G.state == LEVELIN) { draw_levelin(); return; }

    /* An era the plane has just come to fades in, its buildings rising. */
    chase = set_camera();
    /* the cockpit first: its faces have to be there before anything that
     * is put on the board at a place in the world asks whether it is hidden */
    if (!chase && !G.hud_only && !G.p.rear && G.p.alive) draw_cockpit();
    world_fade(smooth(G.level_time / 1.0f));
    draw_scene(G.era, smooth(G.level_time / GROW_TIME));
    world_fade(1.0f);
    if (chase) { draw_player_plane(); if (!G.p.rear) draw_pipper(); }
    if (G.p.alive) draw_plane_hud(!chase);
    if (G.p.alive) draw_items_hud();
    draw_hud();
    draw_cheer();
    if (G.demo) draw_coin_line();
    if (G.practice) draw_practice();
}
