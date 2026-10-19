/* logo.c - the lettering of the marquee, as outlines.
 *
 * The letters are drawn on a grid: a capital is 10 high, a small letter
 * 7, a stroke 2 thick. Each is one or two closed outlines (the second the
 * hole of an e, P or o). "TimePilot" is yellow above the line where the
 * marquee's letters change colour and red below it, so every outline is
 * cut there; the dots of the two i are red, as on the marquee.
 */
#include "logo.h"
#include "world.h"

#define SPLIT   4.3f            /* the height where yellow turns to red */
#define GAP     0.8f            /* between letters */

/* An outline: how many points, then x, y of each. A glyph: its width, then
 * its outlines, then 0. */
static const float g_T[] = { 8,   8, 0,10, 8,10, 8,8, 5,8, 5,0, 3,0, 3,8, 0,8,   0 };
static const float g_i[] = { 2,   4, 0,0, 2,0, 2,7, 0,7,   0 };
static const float g_m[] = { 9,   13, 0,0, 2,0, 2,5, 3.5f,5, 3.5f,0, 5.5f,0, 5.5f,5, 7,5, 7,0, 9,0, 9,6, 8,7, 0,7,   0 };
static const float g_e[] = { 7,   11, 1,0, 7,0, 7,2, 2,2, 2,3, 7,3, 7,6, 6,7, 1,7, 0,6, 0,1,
                                  4, 2,4.6f, 5,4.6f, 5,5.4f, 2,5.4f,   0 };
static const float g_P[] = { 7.5f, 8, 0,0, 2,0, 2,3.5f, 6.5f,3.5f, 7.5f,4.5f, 7.5f,9, 6.5f,10, 0,10,
                                  4, 2,5.5f, 5.5f,5.5f, 5.5f,8, 2,8,   0 };
static const float g_l[] = { 2,   4, 0,0, 2,0, 2,10, 0,10,   0 };
static const float g_o[] = { 7,   8, 1,0, 6,0, 7,1, 7,6, 6,7, 1,7, 0,6, 0,1,
                                  4, 2,2, 5,2, 5,5, 2,5,   0 };
static const float g_t[] = { 4.5f, 14, 1,0, 4.5f,0, 4.5f,2, 3,2, 3,5, 4.5f,5, 4.5f,7, 3,7, 3,9, 1,9, 1,7, 0,7, 0,5, 1,5,   0 };
/* the dot of an i: a little octagon over it */
static const float g_dot[] = { 2, 8, 0.4f,8.1f, 1.6f,8.1f, 2.1f,8.6f, 2.1f,9.7f, 1.6f,10.2f, 0.4f,10.2f, -0.1f,9.7f, -0.1f,8.6f,   0 };

static const float g_3[] = { 7,   14, 0,0, 6,0, 7,1, 7,9, 6,10, 0,10, 0,8, 5,8, 5,6, 1.5f,6, 1.5f,4, 5,4, 5,2, 0,2,   0 };
static const float g_D[] = { 7,   6, 0,0, 5.5f,0, 7,1.5f, 7,8.5f, 5.5f,10, 0,10,
                                  6, 2,2, 4.5f,2, 5,2.5f, 5,7.5f, 4.5f,8, 2,8,   0 };

static const float *const word[] = { g_T, g_i, g_m, g_e, g_P, g_i, g_l, g_o, g_t };
#define LETTERS ((int)(sizeof word / sizeof word[0]))

static float s_ox, s_oy, s_ux, s_uy;        /* where the grid's origin is on the board, and its units */

static void line(float x0, float y0, float x1, float y1)
{
    world_bline(s_ox + x0 * s_ux, s_oy + y0 * s_uy, s_ox + x1 * s_ux, s_oy + y1 * s_uy);
}

/* One glyph's outlines at x. `part`: 0 all of every line, 1 only what is
 * above the split, -1 only what is below. */
static void outline(const float *g, float x, int part)
{
    const float *p = g + 1;
    while (*p > 0) {
        int n = (int)*p, i;
        p++;
        for (i = 0; i < n; i++) {
            float ax = x + p[2 * i], ay = p[2 * i + 1];
            float bx = x + p[2 * ((i + 1) % n)], by = p[2 * ((i + 1) % n) + 1];
            if (part) {
                int a_up = ay > SPLIT, b_up = by > SPLIT, want = part > 0;
                if (a_up != b_up) {                 /* it crosses: the half on the wanted side */
                    float t = (SPLIT - ay) / (by - ay), mx = ax + (bx - ax) * t;
                    if (a_up == want) line(ax, ay, mx, SPLIT); else line(mx, SPLIT, bx, by);
                    continue;
                }
                if (a_up != want) continue;
                if (ay == SPLIT && by == SPLIT) continue;
            }
            line(ax, ay, bx, by);
        }
        p += 2 * n;
    }
}

/* A glyph with depth: its face, the same again set back, and a line from
 * every corner of the one to the other. */
static void solid(const float *g, float x, float dx, float dy)
{
    const float *p = g + 1;

    world_rgb(40, 90, 230);
    while (*p > 0) {
        int n = (int)*p, i;
        p++;
        for (i = 0; i < n; i++) {
            float ax = x + p[2 * i], ay = p[2 * i + 1];
            float bx = x + p[2 * ((i + 1) % n)], by = p[2 * ((i + 1) % n) + 1];
            line(ax + dx, ay + dy, bx + dx, by + dy);
            line(ax, ay, ax + dx, ay + dy);
        }
        p += 2 * n;
    }
    world_rgb(190, 215, 255);
    outline(g, x, 0);
}

/* Three stripes beside the "3D", red, orange and yellow from the top, each
 * shorter than the one above and cut on the slant: a wing. `dir` -1 to
 * the left of x, 1 to the right. */
static void stripes(float x, float dir)
{
    static const unsigned char c[3][3] = { { 255, 50, 30 }, { 255, 140, 0 }, { 255, 220, 0 } };
    int i;
    for (i = 0; i < 3; i++) {
        float top = 8.6f - 2.1f * (float)i, bottom = top - 1.5f;
        float near_top = x + dir * 1.2f * (float)i, near_bottom = near_top + dir * 0.9f;
        float far_top = x + dir * (22.0f - 3.2f * (float)i), far_bottom = far_top - dir * 1.5f;
        world_rgb(c[i][0], c[i][1], c[i][2]);
        line(near_top, top, far_top, top);
        line(far_top, top, far_bottom, bottom);
        line(far_bottom, bottom, near_bottom, bottom);
        line(near_bottom, bottom, near_top, top);
    }
}

void logo_draw(float cx, float top, float width, float bright)
{
    float total = 0, x;
    int i, pass;

    for (i = 0; i < LETTERS; i++) total += word[i][0] + (i ? GAP : 0);
    world_level(bright);

    /* "TimePilot": the board's units are taller than wide on the tube's
     * 4:3, so the grid's are stretched to match. */
    s_ux = width / total;
    s_uy = s_ux * (world_vertical() ? 0.75f : 4.0f / 3.0f);
    s_ox = cx - width / 2.0f;
    s_oy = top - 10.0f * s_uy;
    for (pass = 1; pass >= -1; pass -= 2) {         /* all the yellow, then all the red */
        if (pass > 0) world_rgb(255, 220, 0); else world_rgb(255, 50, 30);
        for (i = 0, x = 0; i < LETTERS; i++) {
            outline(word[i], x, pass);
            if (pass < 0 && word[i] == g_i) outline(g_dot, x, 0);
            x += word[i][0] + GAP;
        }
    }

    /* "3D" under it, two thirds the size, with its wings */
    s_ux *= 0.62f;
    s_uy *= 0.62f;
    s_ox = cx - 7.6f * s_ux;
    s_oy -= 11.6f * s_uy;
    solid(g_3, 0, 0.9f, -0.9f);
    solid(g_D, 8.2f, 0.9f, -0.9f);
    stripes(-1.6f, -1.0f);
    stripes(17.8f, 1.0f);
}
