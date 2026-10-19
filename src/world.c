/* world.c - the view, the brightness, the ground grid, the clouds. */
#include "world.h"

#include <math.h>
#include <stdlib.h>

#define FOV_DEG       62.0f
#define FOG_START     450.0f
#define FOG_END       1600.0f
#define FOG_FLOOR     0.22f
#define MIN_LEVEL     0.15f         /* a beam dimmer than this draws nothing */

#define GRID_STEP     150.0f        /* metres between the ground's lines */
#define GRID_REACH    1050.0f       /* how far from the eye the grid goes */
#define GRID_FADE     420.0f        /* over this last stretch of it the lines go out: the ground has no edge */
#define GRID_PIECES   8             /* a grid line fades into the distance in this many steps */

#define LOAD_LIMIT    250000.0f     /* board units of beam travel a frame, lit and dark: see world_load; the default of the setting */
#define MAX_DASHES    700
#define HORIZON_DIST  2400.0f

#define CLOUD_COUNT   10
#define CLOUD_TILE    1800.0f       /* the clouds repeat every this far */
#define CLOUD_POINTS  8

static vp_dvg    *s_dvg;
static vp3d_view  s_view;
static int        s_vertical;
static float      s_level = 1.0f;
static float      s_fade = 1.0f;
static uint8_t    s_colour[3];      /* the colour set last, as the beam has it */
static float      s_load;
static float      s_load_limit = LOAD_LIMIT;

/* The short lines kept for world_dashes, in board units. */
static struct dash {
    short   x0, y0, x1, y1;
    uint8_t r, g, b, drawn;
    float   w;                      /* 1 / its distance, for what is solid */
} s_dash[MAX_DASHES];
static int s_dash_count;

/* The clouds: where each floats in its tile, how big, and its outline. */
static struct cloud {
    float x, y, z, rx, rz;
    float bump[CLOUD_POINTS];
} s_clouds[CLOUD_COUNT];

/* --- solid things -----------------------------------------------------------------
 *
 * With the setting `occlusion` the buildings are solid: a line is cut
 * where a face of one of them, nearer, covers it (after Battlezone
 * Enhanced's render/solid). The lines of the 3D world are not drawn at
 * once then but kept (put), with the pen and the fog they were drawn
 * with; the faces are kept as they come (world_face); world_flush cuts
 * the lines and draws what is left of them. 1 / distance is straight
 * across a flat face on the screen, so each face carries it as a plane
 * a x + b y + c, and a piece of a line is behind the face where its own
 * 1 / distance is smaller. What is flat on the board (the head-up
 * display, the texts) is never cut. */
#define SOLID_LINES   6000
#define SOLID_FACES   4000
#define SOLID_GROUPS  800           /* the faces of one thing are a group: its box on the board is looked at first */
#define FACE_MAX      8
#define W_REL         0.004f        /* a line is behind a face if it is this much farther away ... */
#define XY_SLACK      0.75f         /* ... and more than this much of a board unit's worth of the face's slope */
#define MIN_PIECE     3.0f          /* board units: what is left of a line and shorter is dropped */
#define MAX_CUTS      64
#define SOLID_NEAR    2.0f          /* metres: nearer than this a line is cut off, a face is no face */

static int s_solid;
static uint8_t s_pen[3];            /* the beam's colour as it was set last */
static struct sline {
    vp_vec3 a, b;
    uint8_t r, g, b_;
    float   fog_end;
} s_line[SOLID_LINES];
static int s_lines;
static struct sface {
    int   n;
    float x[FACE_MAX], y[FACE_MAX];
    float a, b, c, wmax;
    float x0, y0, x1, y1;
} s_face[SOLID_FACES];
static int s_faces;
static struct sgroup {
    int   first, count;
    float x0, y0, x1, y1, wmax;
} s_group[SOLID_GROUPS];
static int s_groups, s_nocut, s_dash_wait;

/* Lines flat on the board that stand for what is behind everything (the
 * tunnel, its clock): with solid things they wait too, and are cut by
 * every face as if they were infinitely far (fmove, fline). */
#define FAR_SEGS 3000
static struct fseg { float x0, y0, x1, y1; uint8_t r, g, b; } s_fseg[FAR_SEGS];
static int s_fsegs, s_far;
static float s_fx, s_fy;

static void pen(uint8_t r, uint8_t g, uint8_t b)
{
    s_pen[0] = r; s_pen[1] = g; s_pen[2] = b;
    vp_color(s_dvg, r, g, b);
}

/* A line of the 3D world: drawn at once, or kept to be cut. */
static void put(vp_vec3 a, vp_vec3 b)
{
    struct sline *l;
    if (!s_solid || s_nocut || s_lines == SOLID_LINES) { vp3d_line(s_dvg, &s_view, a, b); return; }
    l = &s_line[s_lines++];
    l->a = a; l->b = b;
    l->r = s_pen[0]; l->g = s_pen[1]; l->b_ = s_pen[2];
    l->fog_end = s_view.fog_end;
}

void world_set_solid(int on) { s_solid = on; }

static void fmove(float x, float y)
{
    if (s_far && s_solid) { s_fx = x; s_fy = y; return; }
    vp_moveto(s_dvg, (int)lroundf(x), (int)lroundf(y));
}

static void fline(float x, float y)
{
    if (s_far && s_solid) {
        if (s_fsegs < FAR_SEGS) {
            struct fseg *f = &s_fseg[s_fsegs++];
            f->x0 = s_fx; f->y0 = s_fy; f->x1 = x; f->y1 = y;
            f->r = s_pen[0]; f->g = s_pen[1]; f->b = s_pen[2];
        }
        s_fx = x; s_fy = y;
        return;
    }
    vp_lineto(s_dvg, (int)lroundf(x), (int)lroundf(y));
}
void world_set_nocut(int on) { s_nocut = on; }

void world_group(void)
{
    struct sgroup *g;
    if (!s_solid || s_groups == SOLID_GROUPS) return;
    if (s_groups > 0 && s_group[s_groups - 1].count == 0) return;     /* the last is still empty: that one */
    g = &s_group[s_groups++];
    g->first = s_faces; g->count = 0;
    g->x0 = g->y0 = 1e30f; g->x1 = g->y1 = -1e30f; g->wmax = 0;
}
int world_solid(void) { return s_solid; }

void world_face(const vp_vec3 *p, int count)
{
    struct sface f;
    float w[FACE_MAX];
    double mx = 0, my = 0, mw = 0, area = 0, sxx = 0, sxy = 0, syy = 0, sxw = 0, syw = 0, det;
    int i;
    if (!s_solid || s_faces == SOLID_FACES || count < 3 || count > FACE_MAX) return;
    f.n = count;
    f.x0 = f.y0 = 1e30f; f.x1 = f.y1 = -1e30f; f.wmax = 0;
    for (i = 0; i < count; i++) {
        vp_vec3 c = vp_xf_point(&s_view.world_to_camera, p[i]);
        if (c.z < SOLID_NEAR) return;                       /* (it reaches behind the eye: nothing is cut by it) */
        if (!vp3d_project(&s_view, p[i], &f.x[i], &f.y[i])) return;
        w[i] = 1.0f / c.z;
        if (f.x[i] < f.x0) f.x0 = f.x[i];
        if (f.x[i] > f.x1) f.x1 = f.x[i];
        if (f.y[i] < f.y0) f.y0 = f.y[i];
        if (f.y[i] > f.y1) f.y1 = f.y[i];
        if (w[i] > f.wmax) f.wmax = w[i];
        mx += f.x[i]; my += f.y[i]; mw += w[i];
    }
    if (f.x1 < 0 || f.x0 > 4095.0f || f.y1 < 0 || f.y0 > 4095.0f) return;      /* off the picture */
    for (i = 0; i < count; i++) {
        int j = (i + 1) % count;
        area += (double)f.x[i] * f.y[j] - (double)f.x[j] * f.y[i];
    }
    if (fabs(area) * 0.5 < 4.0) return;                     /* seen edge-on: it covers nothing */
    /* 1 / distance across it: the plane through its corners, by least squares */
    mx /= count; my /= count; mw /= count;
    for (i = 0; i < count; i++) {
        double dx = f.x[i] - mx, dy = f.y[i] - my, dw = w[i] - mw;
        sxx += dx * dx; sxy += dx * dy; syy += dy * dy; sxw += dx * dw; syw += dy * dw;
    }
    det = sxx * syy - sxy * sxy;
    if (fabs(det) < 1e-6) return;
    f.a = (float)((sxw * syy - syw * sxy) / det);
    f.b = (float)((syw * sxx - sxw * sxy) / det);
    f.c = (float)(mw - f.a * mx - f.b * my);
    if (s_groups == 0) world_group();
    {
        struct sgroup *g = &s_group[s_groups - 1];
        if (g->first + g->count != s_faces) { world_group(); g = &s_group[s_groups - 1]; }
        if (f.x0 < g->x0) g->x0 = f.x0;
        if (f.y0 < g->y0) g->y0 = f.y0;
        if (f.x1 > g->x1) g->x1 = f.x1;
        if (f.y1 > g->y1) g->y1 = f.y1;
        if (f.wmax > g->wmax) g->wmax = f.wmax;
        g->count++;
    }
    s_face[s_faces++] = f;
}

static int face_inside(const struct sface *p, float x, float y)
{
    int in = 0, i, j;
    for (i = 0, j = p->n - 1; i < p->n; j = i++)
        if ((p->y[i] > y) != (p->y[j] > y) &&
            x < (p->x[j] - p->x[i]) * (y - p->y[i]) / (p->y[j] - p->y[i]) + p->x[i])
            in = !in;
    return in;
}

static int float_cmp(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y;
}

static int cut_cmp(const void *a, const void *b)
{
    float x = ((const float *)a)[0], y = ((const float *)b)[0];
    return x < y ? -1 : x > y;
}

/* The parts of a line (board x, y and 1 / distance at both ends) that a
 * face covers, as stretches of 0..1 along it on the board, added to cut[]. */
static int cover(const struct sface *p, float x0, float y0, float w0, float x1, float y1, float w1, float (*cut)[2], int n)
{
    float lx0 = x0 < x1 ? x0 : x1, lx1 = x0 < x1 ? x1 : x0, ly0 = y0 < y1 ? y0 : y1, ly1 = y0 < y1 ? y1 : y0;
    float dx = x1 - x0, dy = y1 - y0, ts[FACE_MAX + 2], slack;
    int nt = 0, i, j;
    if (lx1 < p->x0 || lx0 > p->x1 || ly1 < p->y0 || ly0 > p->y1) return n;
    if (w0 > p->wmax * 1.02f && w1 > p->wmax * 1.02f) return n;          /* all of it nearer */
    ts[nt++] = 0.0f;
    for (i = 0, j = p->n - 1; i < p->n; j = i++) {
        float ex = p->x[i] - p->x[j], ey = p->y[i] - p->y[j];
        float den = dx * ey - dy * ex, t, u;
        if (fabsf(den) < 1e-9f) continue;
        t = ((p->x[j] - x0) * ey - (p->y[j] - y0) * ex) / den;
        u = ((p->x[j] - x0) * dy - (p->y[j] - y0) * dx) / den;
        if (t > 0.0f && t < 1.0f && u >= 0.0f && u <= 1.0f) ts[nt++] = t;
    }
    ts[nt++] = 1.0f;
    qsort(ts, (size_t)nt, sizeof ts[0], float_cmp);
    slack = W_REL * p->wmax + XY_SLACK * (fabsf(p->a) + fabsf(p->b));
    for (i = 0; i + 1 < nt && n < MAX_CUTS; i++) {
        float ta = ts[i], tb = ts[i + 1], tm, da, db;
        if (tb - ta < 1e-5f) continue;
        tm = 0.5f * (ta + tb);
        if (!face_inside(p, x0 + dx * tm, y0 + dy * tm)) continue;
        /* how much nearer the face is than the line at each end of the stretch (above 0: the line is behind) */
        da = p->a * (x0 + dx * ta) + p->b * (y0 + dy * ta) + p->c - (w0 + (w1 - w0) * ta);
        db = p->a * (x0 + dx * tb) + p->b * (y0 + dy * tb) + p->c - (w0 + (w1 - w0) * tb);
        if (da <= slack && db <= slack) continue;
        if (da <= slack || db <= slack) {                   /* through the face's plane: behind it from there on */
            float tc = ta + (tb - ta) * (slack - da) / (db - da);
            if (da <= slack) ta = tc; else tb = tc;
        }
        cut[n][0] = ta;
        cut[n][1] = tb;
        n++;
    }
    return n;
}

static vp_vec3 along(vp_vec3 a, vp_vec3 b, float t) { return vp_v3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }

void world_flush(void)
{
    float keep = s_view.fog_end;
    int i, k;
    for (i = 0; i < s_lines; i++) {
        const struct sline *l = &s_line[i];
        vp_vec3 a = l->a, b = l->b, ca, cb;
        float x0, y0, x1, y1, w0, w1, cut[MAX_CUTS][2], len, at;
        int nc = 0;
        vp_color(s_dvg, l->r, l->g, l->b_);
        s_view.fog_end = l->fog_end;
        if (!s_faces) { vp3d_line(s_dvg, &s_view, a, b); continue; }
        ca = vp_xf_point(&s_view.world_to_camera, a);
        cb = vp_xf_point(&s_view.world_to_camera, b);
        if (ca.z < SOLID_NEAR && cb.z < SOLID_NEAR) continue;
        if (ca.z < SOLID_NEAR) { a = along(a, b, (SOLID_NEAR - ca.z) / (cb.z - ca.z)); ca = vp_xf_point(&s_view.world_to_camera, a); }
        if (cb.z < SOLID_NEAR) { b = along(b, a, (SOLID_NEAR - cb.z) / (ca.z - cb.z)); cb = vp_xf_point(&s_view.world_to_camera, b); }
        if (!vp3d_project(&s_view, a, &x0, &y0) || !vp3d_project(&s_view, b, &x1, &y1)) { vp3d_line(s_dvg, &s_view, a, b); continue; }
        w0 = 1.0f / ca.z; w1 = 1.0f / cb.z;
        {
            float lx0 = x0 < x1 ? x0 : x1, lx1 = x0 < x1 ? x1 : x0, ly0 = y0 < y1 ? y0 : y1, ly1 = y0 < y1 ? y1 : y0;
            int g, f;
            for (g = 0; g < s_groups && nc < MAX_CUTS; g++) {
                const struct sgroup *gr = &s_group[g];
                if (lx1 < gr->x0 || lx0 > gr->x1 || ly1 < gr->y0 || ly0 > gr->y1) continue;
                if (w0 > gr->wmax * 1.02f && w1 > gr->wmax * 1.02f) continue;
                for (f = gr->first; f < gr->first + gr->count && nc < MAX_CUTS; f++)
                    nc = cover(&s_face[f], x0, y0, w0, x1, y1, w1, cut, nc);
            }
        }
        if (!nc) { vp3d_line(s_dvg, &s_view, a, b); continue; }
        qsort(cut, (size_t)nc, sizeof cut[0], cut_cmp);
        len = hypotf(x1 - x0, y1 - y0);
        at = 0.0f;
        for (k = 0; k <= nc; k++) {
            float to = k < nc ? cut[k][0] : 1.0f;
            if (to - at > 0.0f && (to - at) * len >= MIN_PIECE) {
                /* from along the board to along the line in the world: 1 / distance goes straight on the board */
                float ta = at * w1 / ((1.0f - at) * w0 + at * w1), tb = to * w1 / ((1.0f - to) * w0 + to * w1);
                vp3d_line(s_dvg, &s_view, along(a, b, ta), along(a, b, tb));
            }
            if (k < nc && cut[k][1] > at) at = cut[k][1];
        }
    }
    s_view.fog_end = keep;
    s_lines = 0;
    for (i = 0; i < s_fsegs; i++) {
        const struct fseg *f = &s_fseg[i];
        float cut[MAX_CUTS][2], at = 0.0f, len = hypotf(f->x1 - f->x0, f->y1 - f->y0);
        int nc = 0, g, fc;
        vp_color(s_dvg, f->r, f->g, f->b);
        for (g = 0; g < s_groups && nc < MAX_CUTS; g++) {
            const struct sgroup *gr = &s_group[g];
            if ((f->x0 < gr->x0 && f->x1 < gr->x0) || (f->x0 > gr->x1 && f->x1 > gr->x1) ||
                (f->y0 < gr->y0 && f->y1 < gr->y0) || (f->y0 > gr->y1 && f->y1 > gr->y1)) continue;
            for (fc = gr->first; fc < gr->first + gr->count && nc < MAX_CUTS; fc++)
                nc = cover(&s_face[fc], f->x0, f->y0, 0, f->x1, f->y1, 0, cut, nc);
        }
        if (nc) qsort(cut, (size_t)nc, sizeof cut[0], cut_cmp);
        for (k = 0; k <= nc; k++) {
            float to = k < nc ? cut[k][0] : 1.0f;
            if (to - at > 0.0f && ((to - at) * len >= MIN_PIECE || len < MIN_PIECE)) {
                vp_moveto(s_dvg, (int)lroundf(f->x0 + (f->x1 - f->x0) * at), (int)lroundf(f->y0 + (f->y1 - f->y0) * at));
                vp_lineto(s_dvg, (int)lroundf(f->x0 + (f->x1 - f->x0) * to), (int)lroundf(f->y0 + (f->y1 - f->y0) * to));
            }
            if (k < nc && cut[k][1] > at) at = cut[k][1];
        }
    }
    s_fsegs = 0;
    if (s_dash_wait) world_dashes();    /* the short lines kept for now, cut by every face of the frame */
    vp_color(s_dvg, s_pen[0], s_pen[1], s_pen[2]);
}

/* Is a point of the board (with its 1 / distance) behind a face? For the
 * short lines of world_dashes. */
static int hidden(float x, float y, float w)
{
    int g, k;
    for (g = 0; g < s_groups; g++) {
        const struct sgroup *gr = &s_group[g];
        if (x < gr->x0 || x > gr->x1 || y < gr->y0 || y > gr->y1 || w > gr->wmax * 1.02f) continue;
        for (k = gr->first; k < gr->first + gr->count; k++) {
        const struct sface *p = &s_face[k];
        float slack;
        if (x < p->x0 || x > p->x1 || y < p->y0 || y > p->y1 || w > p->wmax * 1.02f) continue;
        slack = W_REL * p->wmax + XY_SLACK * (fabsf(p->a) + fabsf(p->b));
        if (face_inside(p, x, y) && p->a * x + p->b * y + p->c - w > slack) return 1;
        }
    }
    return 0;
}

static unsigned s_rng = 0xC10DD5u;
static float frand(void)
{
    s_rng = s_rng * 1664525u + 1013904223u;
    return (float)(s_rng >> 8) / 16777216.0f;
}

void world_init(int vertical)
{
    int i, j;

    s_vertical = vertical;
    vp3d_view_init(&s_view, FOV_DEG);
    if (vertical) s_view.aspect = 3.0f / 4.0f;
    s_view.near_z = 1.0f;
    s_view.far_z = WORLD_FAR;
    s_view.fog_start = FOG_START;
    s_view.fog_end = FOG_END;
    s_view.fog_floor = FOG_FLOOR;
    for (i = 0; i < CLOUD_COUNT; i++) {
        struct cloud *c = &s_clouds[i];
        c->x = frand() * CLOUD_TILE;
        c->z = frand() * CLOUD_TILE;
        c->y = 120.0f + frand() * 420.0f;
        c->rx = 40.0f + frand() * 70.0f;
        c->rz = 25.0f + frand() * 40.0f;
        for (j = 0; j < CLOUD_POINTS; j++) c->bump[j] = 0.75f + frand() * 0.5f;
    }
}

int world_vertical(void) { return s_vertical; }

void world_begin(vp_dvg *d)
{
    vp_dvg_stats st;
    s_dvg = d;
    s_dash_count = 0;
    s_lines = s_faces = s_groups = s_fsegs = 0;
    s_far = 0;
    s_nocut = 0;
    s_dash_wait = 0;
    /* what the frame before cost the beam, smoothed over a few frames */
    vp_dvg_get_stats(d, &st);
    s_load += ((float)(st.lit_length + st.blank_length) / s_load_limit - s_load) * 0.3f;
}

void world_set_load_limit(float units) { s_load_limit = units < 50000.0f ? 50000.0f : units; }

float world_load(void) { return s_load; }

/* How bright something `z` metres ahead still is: the view's own fog. */
static float fog_at(float z)
{
    float t;
    if (z <= FOG_START) return 1.0f;
    if (z >= FOG_END) return FOG_FLOOR;
    t = (z - FOG_START) / (FOG_END - FOG_START);
    return 1.0f + (FOG_FLOOR - 1.0f) * t;
}
vp_dvg *world_dvg(void) { return s_dvg; }
vp3d_view *world_view(void) { return &s_view; }

void world_pose(vp_vec3 pos, float heading, float pitch, float roll)
{
    vp3d_set_pose(&s_view, pos, heading, pitch, roll);
}

vp_xf world_place(vp_vec3 pos, float heading, float pitch, float roll)
{
    /* The same order as the SDK's camera pose: turn, tilt, roll, move. */
    return vp_xf_mul2(vp_xf_translate(pos.x, pos.y, pos.z),
           vp_xf_mul2(vp_xf_rot_y(heading),
           vp_xf_mul2(vp_xf_rot_x(-pitch), vp_xf_rot_z(roll))));
}

vp_vec3 world_forward(float heading, float pitch)
{
    float c = cosf(pitch);
    return vp_v3(sinf(heading) * c, sinf(pitch), cosf(heading) * c);
}

void world_level(float level) { s_level = level; }

void world_fade(float fade) { s_fade = fade < 0 ? 0 : fade > 1.0f ? 1.0f : fade; }

void world_rgb(int r, int g, int b)
{
    float level = s_level < MIN_LEVEL ? MIN_LEVEL : s_level > 1.0f ? 1.0f : s_level;
    level *= s_fade;
    s_colour[0] = (uint8_t)lroundf((float)r * level);
    s_colour[1] = (uint8_t)lroundf((float)g * level);
    s_colour[2] = (uint8_t)lroundf((float)b * level);
    pen(s_colour[0], s_colour[1], s_colour[2]);
}

/* The board's square is stretched to the tube: how many board units up
 * are as long there as one across. */
static float board_aspect(void) { return s_vertical ? 3.0f / 4.0f : 4.0f / 3.0f; }

void world_bline(float x0, float y0, float x1, float y1)
{
    fmove(x0, y0);
    fline(x1, y1);
}

void world_bcircle(float cx, float cy, float r, int n)
{
    float ry = r * board_aspect();
    int i;
    fmove(cx + r, cy);
    for (i = 1; i <= n; i++) {
        float a = 6.2831853f * (float)i / (float)n;
        fline(cx + r * cosf(a), cy + ry * sinf(a));
    }
}

/* --- the way through time ------------------------------------------------ */

#define TUN_RINGS  12
#define TUN_STEP   TUNNEL_STEP
#define TUN_SIDES  8
#define TUN_RADIUS TUNNEL_RADIUS
#define TUN_FOCAL  1700.0f      /* board units for one metre across at one metre */

/* Two bends laid over each other each way, so it never repeats soon. How
 * fast the middle moves sideways for a plane flying down it is the
 * amplitudes times these rates times its speed: at 250 m/s about 150 m/s
 * at the worst, which is what a plane in it must be able to follow. */
void world_tunnel_centre(float s, float amp, float *x, float *y)
{
    *x = amp * (120.0f * sinf(s * 0.0030f) + 35.0f * sinf(s * 0.0071f + 1.0f));
    *y = amp * (80.0f * sinf(s * 0.0024f + 1.0f) + 30.0f * sinf(s * 0.0063f));
}

static void tunnel_colour(const float from[3], const float to[3], float mix, float white, float level)
{
    float c[3];
    int i;
    if (level > 1.0f) level = 1.0f;
    if (level < 0) level = 0;
    for (i = 0; i < 3; i++) {
        c[i] = from[i] + (to[i] - from[i]) * mix;
        c[i] += (255.0f - c[i]) * white;
        c[i] *= level * s_fade;
    }
    pen((uint8_t)lroundf(c[0]), (uint8_t)lroundf(c[1]), (uint8_t)lroundf(c[2]));
}

/* Rings stand every TUN_STEP metres along a line that snakes; each is
 * turned a little further than the one before, so the tunnel twists as it
 * comes. Four rails run through their corners. Drawn from the far end to
 * the near one, each ring one stroke. */
void world_draw_tunnel(float dist, float eye_x, float eye_y, float amp, float bright,
                       const float from[3], const float to[3], float mix, float *far_x, float *far_y,
                       float target, float ahead)
{
    s_far = 1;
    const float half = (VP_DVG_COORD_MAX + 1) / 2.0f;
    const float fy = TUN_FOCAL * board_aspect();
    float frac = fmodf(dist, TUN_STEP);
    float px[TUN_RINGS][TUN_SIDES], py[TUN_RINGS][TUN_SIDES], depth[TUN_RINGS];
    int i, k;

    if (frac < 0) frac += TUN_STEP;
    for (i = 0; i < TUN_RINGS; i++) {
        float z = (float)(i + 1) * TUN_STEP - frac, at = dist + z;
        float ox, oy, twist = at * 0.007f;
        world_tunnel_centre(at, amp, &ox, &oy);
        ox -= eye_x;
        oy -= eye_y;
        depth[i] = z;
        if (z < 14.0f) continue;        /* about to pass the viewer */
        for (k = 0; k < TUN_SIDES; k++) {
            float a = twist + 6.2831853f * (float)k / (float)TUN_SIDES;
            px[i][k] = half + (ox + TUN_RADIUS * cosf(a)) / z * TUN_FOCAL;
            py[i][k] = half + (oy + TUN_RADIUS * sinf(a)) / z * fy;
        }
        if (i == TUN_RINGS - 1) {
            if (far_x) *far_x = half + ox / z * TUN_FOCAL;
            if (far_y) *far_y = half + oy / z * fy;
        }
    }
    for (i = TUN_RINGS - 1; i >= 0; i--) {
        float level = 1.0f - depth[i] / ((float)TUN_RINGS * TUN_STEP);
        int odd = (int)floorf((dist + depth[i]) / TUN_STEP + 0.5f) & 1;
        if (depth[i] < 14.0f) continue;
        tunnel_colour(from, to, mix, odd ? 0.55f : 0.0f, bright * (0.2f + level * level));
        fmove((int)px[i][0], (int)py[i][0]);
        for (k = 1; k <= TUN_SIDES; k++)
            fline((int)px[i][k % TUN_SIDES], (int)py[i][k % TUN_SIDES]);
    }
    if (target > 0) {
        /* where to fly: a small ring in the middle of each of the next
         * three, the nearest the brightest */
        int shown = 0;
        for (i = 0; i < TUN_RINGS && shown < 3; i++) {
            float z = depth[i], ox, oy, level = shown == 0 ? 1.0f : shown == 1 ? 0.6f : 0.35f;
            if (z < ahead) continue;
            world_tunnel_centre(dist + z, amp, &ox, &oy);
            ox -= eye_x;
            oy -= eye_y;
            pen(0, (uint8_t)lroundf(255.0f * level * bright * s_fade), (uint8_t)lroundf(120.0f * level * bright * s_fade));
            for (k = 0; k <= 8; k++) {
                float a = 6.2831853f * (float)(k % 8) / 8.0f + 0.3927f;
                int x = (int)(half + (ox + target * cosf(a)) / z * TUN_FOCAL), y = (int)(half + (oy + target * sinf(a)) / z * fy);
                if (k) fline(x, y); else fmove(x, y);
            }
            shown++;
        }
    }
    tunnel_colour(from, to, mix, 0.0f, bright * 0.45f);
    for (k = 0; k < TUN_SIDES; k += 2) {
        int started = 0;
        for (i = TUN_RINGS - 1; i >= 0; i--) {
            if (depth[i] < 14.0f) continue;
            if (!started) { fmove((int)px[i][k], (int)py[i][k]); started = 1; }
            else fline((int)px[i][k], (int)py[i][k]);
        }
    }
    s_far = 0;
}

void world_draw_clock(float cx, float cy, float r, float turns)
{
    float ry = r * board_aspect();
    int i;

    s_far = 1;
    world_bcircle(cx, cy, r, 12);
    for (i = 0; i < 4; i++) {
        float a = 1.5707963f * (float)i;
        world_bline(cx + 0.82f * r * cosf(a), cy + 0.82f * ry * sinf(a), cx + r * cosf(a), cy + ry * sinf(a));
    }
    {
        /* clockwise from twelve: the hour hand, then the minute hand */
        float h = 1.5707963f - turns * 6.2831853f, m = 1.5707963f - turns * 12.0f * 6.2831853f;
        world_bline(cx, cy, cx + 0.5f * r * cosf(h), cy + 0.5f * ry * sinf(h));
        world_bline(cx, cy, cx + 0.85f * r * cosf(m), cy + 0.85f * ry * sinf(m));
    }
    s_far = 0;
}

void world_mesh(const vp3d_mesh *m, const vp_xf *place)
{
    vp3d_draw_mesh_wire(s_dvg, &s_view, m, place);
}

void world_line(vp_vec3 a, vp_vec3 b)
{
    put(a, b);
}

int world_project(vp_vec3 p, float *sx, float *sy)
{
    return vp3d_project(&s_view, p, sx, sy);
}

void world_dash(vp_vec3 a, vp_vec3 b)
{
    struct dash *d;
    float ax, ay, bx, by, fog;
    vp_vec3 ca;

    if (s_dash_count == MAX_DASHES) return;
    /* one with an end behind the eye is left out: they are too short to be worth cutting */
    if (!vp3d_project(&s_view, a, &ax, &ay) || !vp3d_project(&s_view, b, &bx, &by)) return;
    if (ax < -500.0f || ax > 4600.0f || ay < -500.0f || ay > 4600.0f) return;
    if (bx < -500.0f || bx > 4600.0f || by < -500.0f || by > 4600.0f) return;
    ca = vp_xf_point(&s_view.world_to_camera, a);
    if (ca.z > WORLD_FAR) return;
    fog = fog_at(ca.z);
    d = &s_dash[s_dash_count++];
    d->x0 = (short)ax; d->y0 = (short)ay;
    d->x1 = (short)bx; d->y1 = (short)by;
    d->r = (uint8_t)((float)s_colour[0] * fog);
    d->g = (uint8_t)((float)s_colour[1] * fog);
    d->b = (uint8_t)((float)s_colour[2] * fog);
    d->drawn = 0;
    d->w = ca.z > 1.0f ? 1.0f / ca.z : 1.0f;
}

/* From where the beam is, always on to the nearest end of a line not yet
 * drawn, and through it to its other end. Not the shortest way round them
 * all, but near it, and found in a few hundred thousand comparisons. */
void world_dashes(void)
{
    int px = 2048, py = 2048, left = s_dash_count, i;
    uint8_t r = 0, g = 0, b = 0;
    int coloured = 0;

    if (s_solid && !s_dash_wait) {       /* solid: they wait with the rest for the end of the frame (world_flush) */
        s_dash_wait = 1;
        return;
    }
    s_dash_wait = 0;
    if (s_solid && s_faces)             /* what is behind something solid is not drawn */
        for (i = 0; i < s_dash_count; i++)
            if (hidden(0.5f * (float)(s_dash[i].x0 + s_dash[i].x1), 0.5f * (float)(s_dash[i].y0 + s_dash[i].y1), s_dash[i].w)) {
                s_dash[i].drawn = 1;
                left--;
            }

    while (left-- > 0) {
        long best = 0x7fffffffL;
        int which = -1, backwards = 0;
        struct dash *d;
        for (i = 0; i < s_dash_count; i++) {
            long dx, dy, d0, d1;
            d = &s_dash[i];
            if (d->drawn) continue;
            dx = d->x0 - px; dy = d->y0 - py; d0 = dx * dx + dy * dy;
            dx = d->x1 - px; dy = d->y1 - py; d1 = dx * dx + dy * dy;
            if (d0 < best) { best = d0; which = i; backwards = 0; }
            if (d1 < best) { best = d1; which = i; backwards = 1; }
        }
        if (which < 0) break;
        d = &s_dash[which];
        d->drawn = 1;
        if (!coloured || d->r != r || d->g != g || d->b != b) {
            r = d->r; g = d->g; b = d->b;
            coloured = 1;
            pen(r, g, b);
        }
        if (backwards) {
            vp_moveto(s_dvg, d->x1, d->y1); vp_lineto(s_dvg, d->x0, d->y0);
            px = d->x0; py = d->y0;
        } else {
            vp_moveto(s_dvg, d->x0, d->y0); vp_lineto(s_dvg, d->x1, d->y1);
            px = d->x1; py = d->y1;
        }
    }
    s_dash_count = 0;
}

/* A line of the ground, fading into the distance in a few steps of its
 * own. The SDK's fog would cut a line that runs from under the plane to
 * the horizon into up to twelve pieces, each a word to the board; a few
 * are enough for the eye. So the view's fog is off while the ground is
 * drawn, and each piece gets its brightness here: the fog's, times `edge`
 * (0..1, the whole line's), and with `reach` over 0 going out towards
 * that distance from the eye, so the grid ends in the dark and not at an
 * edge. A piece that would be black is not sent. */
static void ground_line(vp_vec3 a, vp_vec3 b, vp_vec3 eye, int pieces, float edge, float reach)
{
    vp_vec3 from = a;
    int k;
    for (k = 1; k <= pieces; k++) {
        float u = (float)k / (float)pieces;
        vp_vec3 to = vp_v3(a.x + (b.x - a.x) * u, a.y, a.z + (b.z - a.z) * u);
        vp_vec3 mid = vp_v3((from.x + to.x) * 0.5f - eye.x, eye.y, (from.z + to.z) * 0.5f - eye.z);
        float flat = sqrtf(mid.x * mid.x + mid.z * mid.z);
        float lit = fog_at(sqrtf(flat * flat + mid.y * mid.y)) * edge;
        if (reach > 0) {
            float out = (reach - flat) / GRID_FADE;
            lit *= out < 0 ? 0 : out > 1.0f ? 1.0f : out;
        }
        if (lit > 0.03f) {
            pen((uint8_t)((float)s_colour[0] * lit), (uint8_t)((float)s_colour[1] * lit),
                            (uint8_t)((float)s_colour[2] * lit));
            put(from, to);
        }
        from = to;
    }
}

/* The ground: lines along x and along z through the grid points nearest
 * the eye, out to GRID_REACH; the horizon as one long line across the
 * view. Each line is one stroke; the beam jumps between them. A line runs
 * from GRID_REACH behind the eye to GRID_REACH ahead of it - with the eye,
 * not with the grid, so its ends do not jump - and both it and the lines
 * far out to the side go out towards the edge: nothing pops in. */
void world_draw_ground(vp_vec3 eye, float heading, int coarse)
{
    float step = coarse ? 2.0f * GRID_STEP : GRID_STEP;
    float x0 = floorf((eye.x - GRID_REACH) / step) * step;
    float z0 = floorf((eye.z - GRID_REACH) / step) * step;
    float x, z, alt = eye.y - GROUND_Y, fog_end = s_view.fog_end;
    vp_vec3 fwd = vp_v3(sinf(heading), 0, cosf(heading));
    vp_vec3 right = vp_v3(cosf(heading), 0, -sinf(heading));
    vp_vec3 h = vp_v3(eye.x + fwd.x * HORIZON_DIST, GROUND_Y, eye.z + fwd.z * HORIZON_DIST);
    uint8_t base[3] = { s_colour[0], s_colour[1], s_colour[2] };
    int pieces = coarse ? GRID_PIECES / 2 : GRID_PIECES, i;

    s_view.fog_end = 0;                 /* no fog of the SDK's: see ground_line */
    /* the horizon, drawn where the ground lines run out */
    ground_line(vp_v3(h.x - right.x * HORIZON_DIST * 1.5f, h.y, h.z - right.z * HORIZON_DIST * 1.5f),
                vp_v3(h.x + right.x * HORIZON_DIST * 1.5f, h.y, h.z + right.z * HORIZON_DIST * 1.5f), eye, 1, 1.0f, 0);

    /* Lines in the two directions, alternating ends so the beam's jumps
     * stay short. Lines far below a high flyer are not worth the beam. */
    if (alt <= 900.0f) {
        for (i = 0, x = x0; x <= eye.x + GRID_REACH; x += step, i++) {
            vp_vec3 a = vp_v3(x, GROUND_Y, eye.z - GRID_REACH), b = vp_v3(x, GROUND_Y, eye.z + GRID_REACH);
            float edge = (GRID_REACH - fabsf(x - eye.x)) / GRID_FADE;
            if (edge <= 0.03f) continue;
            if (edge > 1.0f) edge = 1.0f;
            if (i & 1) ground_line(b, a, eye, pieces, edge, GRID_REACH); else ground_line(a, b, eye, pieces, edge, GRID_REACH);
        }
        for (i = 0, z = z0; z <= eye.z + GRID_REACH; z += step, i++) {
            vp_vec3 a = vp_v3(eye.x - GRID_REACH, GROUND_Y, z), b = vp_v3(eye.x + GRID_REACH, GROUND_Y, z);
            float edge = (GRID_REACH - fabsf(z - eye.z)) / GRID_FADE;
            if (edge <= 0.03f) continue;
            if (edge > 1.0f) edge = 1.0f;
            if (i & 1) ground_line(b, a, eye, pieces, edge, GRID_REACH); else ground_line(a, b, eye, pieces, edge, GRID_REACH);
        }
    }
    s_view.fog_end = fog_end;
    pen(base[0], base[1], base[2]);
}

/* What is far off in the sky comes up out of the dark over its last 400
 * metres before the distance it is drawn to, instead of popping in: sets
 * the pen to `base` dimmed for something `d` metres off (over the ground),
 * and says whether it is worth drawing at all. */
static int far_fade(float d, const uint8_t base[3])
{
    float lit = (FOG_END - 100.0f - d) / 400.0f;
    if (lit > 1.0f) lit = 1.0f;
    if (lit < 0.04f) return 0;
    pen((uint8_t)((float)base[0] * lit), (uint8_t)((float)base[1] * lit), (uint8_t)((float)base[2] * lit));
    return 1;
}

/* Each cloud is a flat, bumpy ring with a puffy arc standing over it, so
 * it reads as a cloud edge-on too; drawn where its nearest repeat is. */
void world_draw_clouds(vp_vec3 eye)
{
    const uint8_t base[3] = { s_colour[0], s_colour[1], s_colour[2] };
    int i, j;
    for (i = 0; i < CLOUD_COUNT; i++) {
        const struct cloud *c = &s_clouds[i];
        float dx = c->x - eye.x, dz = c->z - eye.z;
        float cx, cz, h = 0.45f * c->rz;
        vp_vec3 prev = vp_v3(0, 0, 0);

        dx -= CLOUD_TILE * floorf(dx / CLOUD_TILE + 0.5f);     /* the repeat nearest the eye */
        dz -= CLOUD_TILE * floorf(dz / CLOUD_TILE + 0.5f);
        if (dx * dx + dz * dz > (FOG_END - 100.0f) * (FOG_END - 100.0f)) continue;
        if (!far_fade(sqrtf(dx * dx + dz * dz), base)) continue;
        cx = eye.x + dx;
        cz = eye.z + dz;
        /* the base: a ring whose radius bulges in and out */
        for (j = 0; j <= CLOUD_POINTS; j++) {
            int k = j % CLOUD_POINTS;
            float a = 6.2831853f * (float)k / (float)CLOUD_POINTS;
            vp_vec3 p = vp_v3(cx + c->rx * c->bump[k] * cosf(a), c->y, cz + c->rz * c->bump[k] * sinf(a));
            if (j > 0) world_line(prev, p);
            prev = p;
        }
        /* the top: an arc of bumps from one side to the other, along x */
        for (j = 0; j <= 6; j++) {
            float u = (float)j / 6.0f, a = 3.14159265f * u;
            float bump = 1.0f + 0.35f * fabsf(sinf(a * 2.5f));
            vp_vec3 p = vp_v3(cx - c->rx * cosf(a), c->y + h * sinf(a) * bump, cz);
            if (j > 0) world_line(prev, p);
            prev = p;
        }
    }
}

/* The rocks take the clouds' places. Each is an eight-sided lump, its six
 * corners pulled in and out, drawn as three rings (its twelve edges, each
 * ring one stroke), turning about its upright. */
void world_draw_asteroids(vp_vec3 eye, float time)
{
    const uint8_t base[3] = { s_colour[0], s_colour[1], s_colour[2] };
    int i, j, k;
    for (i = 0; i < CLOUD_COUNT; i++) {
        const struct cloud *c = &s_clouds[i];
        float dx = c->x - eye.x, dz = c->z - eye.z, r = c->rz * 0.8f;
        float a = time * (0.15f + 0.05f * (float)(i % 4)) + (float)i, ca = cosf(a), sa = sinf(a);
        vp_vec3 centre, v[6];
        static const int rings[3][4] = { { 0, 4, 1, 5 }, { 0, 2, 1, 3 }, { 2, 4, 3, 5 } };

        dx -= CLOUD_TILE * floorf(dx / CLOUD_TILE + 0.5f);
        dz -= CLOUD_TILE * floorf(dz / CLOUD_TILE + 0.5f);
        if (dx * dx + dz * dz > (FOG_END - 100.0f) * (FOG_END - 100.0f)) continue;
        if (!far_fade(sqrtf(dx * dx + dz * dz), base)) continue;
        centre = vp_v3(eye.x + dx, c->y + 60.0f, eye.z + dz);
        for (j = 0; j < 6; j++) {       /* +x -x +y -y +z -z, each at its own distance */
            float d = r * c->bump[j], x = j == 0 ? d : j == 1 ? -d : 0, y = j == 2 ? d : j == 3 ? -d : 0;
            float z = j == 4 ? d : j == 5 ? -d : 0;
            v[j] = vp_v3(centre.x + x * ca - z * sa, centre.y + y, centre.z + x * sa + z * ca);
        }
        for (k = 0; k < 3; k++)
            for (j = 0; j < 4; j++) world_line(v[rings[k][j]], v[rings[k][(j + 1) % 4]]);
    }
}

/* Streaks from the middle of the screen outwards, growing with t, in board
 * space: the way through time. */
void world_draw_warp(float t, unsigned seed)
{
    const int n = 28;
    float half = (VP_DVG_COORD_MAX + 1) / 2.0f;
    int i;

    for (i = 0; i < n; i++) {
        unsigned r = (seed + (unsigned)i * 2654435761u);
        float a = 6.2831853f * (float)i / (float)n + (float)(r & 255) / 1200.0f;
        float phase = (float)((r >> 8) & 255) / 256.0f;
        float u = fmodf(t * 0.9f + phase, 1.0f);           /* 0 at the middle, 1 at the edge */
        float inner = 120.0f + 2400.0f * u * u;
        float outer = inner + 60.0f + 900.0f * u;
        float ca = cosf(a), sa = sinf(a);
        vp_moveto(s_dvg, (int)(half + ca * inner), (int)(half + sa * inner));
        vp_lineto(s_dvg, (int)(half + ca * outer), (int)(half + sa * outer));
    }
}

void world_text(const char *s, float x, float y, float height, int align)
{
    /* The SDK measures each letter of this font with the gap after it (8
     * wide and 4 of gap, in 12 to the height), so centred text sat half a
     * gap left of its place and right-aligned text a whole gap: put back. */
    if (align == VP_TEXT_CENTER) x += height * (2.0f / 12.0f);
    else if (align == VP_TEXT_RIGHT) x += height * (4.0f / 12.0f);
    vp_text_font(VP_FONT_ASTEROIDS);
    vp_text(s_dvg, s, x, y, height, align);
}
