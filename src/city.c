/* city.c - the buildings of each era, made from a few unit shapes.
 *
 * A building is up to six parts; a part is a unit shape (a box, a pyramid,
 * a gabled roof, a six-sided tower) stretched to its size and set in its
 * place. The shapes are meshes, so the SDK draws each edge once and in as
 * few strokes as it can.
 */
#include "city.h"
#include "world.h"

#include <math.h>
#include <string.h>

#define CELL       160.0f      /* metres: one building at most in each */
#define CELL_HALF   (CELL / 2.0f)
#define REACH       920.0f     /* buildings further from the eye are not drawn (the most; see city_set_reach) */
#define FADE_IN     260.0f     /* and over this last stretch before it they come up out of the dark: nothing pops in */

/* How far the city is drawn now: the game sets it for the era, the view
 * and how full the picture is (draw.c). */
static float s_reach = REACH;

void city_set_reach(float reach) { s_reach = reach < 250.0f ? 250.0f : reach > REACH ? REACH : reach; }
#define MAX_PARTS   6

/* The unit shapes: 1 wide (x), 1 deep (z), 1 high (y), standing on y = 0. */
enum { S_BOX, S_BOX_OPEN, S_PYRAMID, S_GABLE, S_HEX, S_COUNT };

typedef struct part {
    int   shape;
    float x, y, z;              /* its middle on the ground plan, and its foot */
    float sx, sy, sz;           /* its size */
} part;

typedef struct building {
    float x, z;                 /* its middle in the world */
    int   n;
    part  p[MAX_PARTS];
    float hx, hz, top;          /* half its ground plan, and its height */
    int   shade;                /* which of the era's wall colours it has */
    int   special;              /* CITY_: not a house */
    int   cx, cz, spent;
    int   mill;                 /* it has sails that turn */
    float mill_y, mill_z, mill_r;
} building;

/* How many cells in a hundred hold a building, and how much bigger than
 * life the era's are drawn: a farmhouse at its true size is a dot from
 * the air. */
static const int   density[5] = { 22, 24, 22, 58, 30 };      /* per cent of the cells built on (1982 and 2001 thinned for a faulty monitor in 0.31.1 to 0.45.1; back since 2026-10-21) */
static const float scale[5]   = { 2.2f, 1.6f, 1.6f, 1.0f, 1.0f };

static vp3d_mesh *s_shape[S_COUNT];

/* The colours of what is built, in each era: three for walls (a building
 * has one of them), one for roofs and spires, one for round towers and
 * tanks. Never the colours of what flies there. */
static const unsigned char s_colour[5][5][3] = {
    /* 1910: plaster, timber and brick; red tiles; grey stone */
    { { 230, 200, 140 }, { 190, 130, 70 }, { 210, 120, 90 }, { 230, 70, 50 }, { 190, 190, 190 } },
    /* 1940: concrete, drab, brick; dark green roofs; rust */
    { { 170, 175, 170 }, { 140, 150, 110 }, { 190, 120, 90 }, { 90, 150, 90 }, { 200, 120, 60 } },
    /* 1970: olive, sand, concrete; red and white masts; silver tanks */
    { { 150, 150, 60 }, { 200, 180, 110 }, { 150, 155, 150 }, { 240, 80, 60 }, { 200, 200, 215 } },
    /* 1982: glass and steel in three blues; red masts */
    { { 70, 150, 230 }, { 120, 120, 240 }, { 60, 210, 210 }, { 255, 70, 70 }, { 160, 180, 220 } },
    /* 2001: light; green spires; blue towers */
    { { 60, 230, 190 }, { 120, 255, 160 }, { 60, 200, 255 }, { 80, 255, 200 }, { 120, 140, 255 } },
};

/* --- the unit shapes ------------------------------------------------------ */

static void make_box(vp3d_mesh *m, int with_floor)
{
    int a[4], b[4], i;
    a[0] = vp3d_mesh_vertex(m, -0.5f, 0, -0.5f); a[1] = vp3d_mesh_vertex(m, 0.5f, 0, -0.5f);
    a[2] = vp3d_mesh_vertex(m, 0.5f, 0, 0.5f);   a[3] = vp3d_mesh_vertex(m, -0.5f, 0, 0.5f);
    b[0] = vp3d_mesh_vertex(m, -0.5f, 1, -0.5f); b[1] = vp3d_mesh_vertex(m, 0.5f, 1, -0.5f);
    b[2] = vp3d_mesh_vertex(m, 0.5f, 1, 0.5f);   b[3] = vp3d_mesh_vertex(m, -0.5f, 1, 0.5f);
    for (i = 0; i < 4; i++) {
        if (with_floor) vp3d_mesh_line(m, a[i], a[(i + 1) % 4]);
        vp3d_mesh_line(m, b[i], b[(i + 1) % 4]);
        vp3d_mesh_line(m, a[i], b[i]);
    }
}

static void make_pyramid(vp3d_mesh *m)
{
    int a[4], top, i;
    a[0] = vp3d_mesh_vertex(m, -0.5f, 0, -0.5f); a[1] = vp3d_mesh_vertex(m, 0.5f, 0, -0.5f);
    a[2] = vp3d_mesh_vertex(m, 0.5f, 0, 0.5f);   a[3] = vp3d_mesh_vertex(m, -0.5f, 0, 0.5f);
    top = vp3d_mesh_vertex(m, 0, 1, 0);
    for (i = 0; i < 4; i++) {
        vp3d_mesh_line(m, a[i], a[(i + 1) % 4]);
        vp3d_mesh_line(m, a[i], top);
    }
}

/* A roof: the ridge runs along z. Its foot is the top of the box under
 * it, which that box draws. */
static void make_gable(vp3d_mesh *m)
{
    int a0 = vp3d_mesh_vertex(m, -0.5f, 0, -0.5f), a1 = vp3d_mesh_vertex(m, 0.5f, 0, -0.5f);
    int a2 = vp3d_mesh_vertex(m, 0.5f, 0, 0.5f),   a3 = vp3d_mesh_vertex(m, -0.5f, 0, 0.5f);
    int r0 = vp3d_mesh_vertex(m, 0, 1, -0.5f),     r1 = vp3d_mesh_vertex(m, 0, 1, 0.5f);
    vp3d_mesh_line(m, a0, r0); vp3d_mesh_line(m, r0, a1);
    vp3d_mesh_line(m, r0, r1);
    vp3d_mesh_line(m, a2, r1); vp3d_mesh_line(m, r1, a3);
}

static void make_hex(vp3d_mesh *m)
{
    int a[6], b[6], i;
    for (i = 0; i < 6; i++) {
        float ang = 6.2831853f * (float)i / 6.0f;
        a[i] = vp3d_mesh_vertex(m, 0.5f * cosf(ang), 0, 0.5f * sinf(ang));
        b[i] = vp3d_mesh_vertex(m, 0.5f * cosf(ang), 1, 0.5f * sinf(ang));
    }
    for (i = 0; i < 6; i++) {
        vp3d_mesh_line(m, b[i], b[(i + 1) % 6]);
        vp3d_mesh_line(m, a[i], b[i]);
    }
}

/* A unit shape drawn solid: its lines, and its faces for what is behind
 * it to be cut by (world.c). The same lines as its mesh. */
static void shape_solid(int shape, const vp_xf *place)
{
    vp_vec3 a[6], b[6], t0, t1;
    int i, n = shape == S_HEX ? 6 : 4;
    world_group();
    if (shape == S_HEX) {
        for (i = 0; i < 6; i++) {
            float ang = 6.2831853f * (float)i / 6.0f;
            a[i] = vp_xf_point(place, vp_v3(0.5f * cosf(ang), 0, 0.5f * sinf(ang)));
            b[i] = vp_xf_point(place, vp_v3(0.5f * cosf(ang), 1, 0.5f * sinf(ang)));
        }
    } else {
        static const float cx[4] = { -0.5f, 0.5f, 0.5f, -0.5f }, cz[4] = { -0.5f, -0.5f, 0.5f, 0.5f };
        for (i = 0; i < 4; i++) {
            a[i] = vp_xf_point(place, vp_v3(cx[i], 0, cz[i]));
            b[i] = vp_xf_point(place, vp_v3(cx[i], 1, cz[i]));
        }
    }
    switch (shape) {
    case S_BOX: case S_BOX_OPEN: case S_HEX:
        for (i = 0; i < n; i++) {
            vp_vec3 side[4];
            if (shape == S_BOX) world_line(a[i], a[(i + 1) % n]);
            world_line(b[i], b[(i + 1) % n]);
            world_line(a[i], b[i]);
            side[0] = a[i]; side[1] = a[(i + 1) % n]; side[2] = b[(i + 1) % n]; side[3] = b[i];
            world_face(side, 4);
        }
        world_face(b, n);
        break;
    case S_PYRAMID:
        t0 = vp_xf_point(place, vp_v3(0, 1, 0));
        for (i = 0; i < 4; i++) {
            vp_vec3 tri[3];
            world_line(a[i], a[(i + 1) % 4]);
            world_line(a[i], t0);
            tri[0] = a[i]; tri[1] = a[(i + 1) % 4]; tri[2] = t0;
            world_face(tri, 3);
        }
        break;
    default: {                  /* S_GABLE: its foot is drawn by the box under it */
        vp_vec3 q[4];
        t0 = vp_xf_point(place, vp_v3(0, 1, -0.5f));
        t1 = vp_xf_point(place, vp_v3(0, 1, 0.5f));
        world_line(a[0], t0); world_line(t0, a[1]); world_line(t0, t1);
        world_line(a[2], t1); world_line(t1, a[3]);
        q[0] = a[0]; q[1] = t0; q[2] = t1; q[3] = a[3]; world_face(q, 4);
        q[0] = a[1]; q[1] = t0; q[2] = t1; q[3] = a[2]; world_face(q, 4);
        q[0] = a[0]; q[1] = a[1]; q[2] = t0; world_face(q, 3);
        q[0] = a[3]; q[1] = a[2]; q[2] = t1; world_face(q, 3);
        break;
    }
    }
}

void city_init(void)
{
    int i;
    for (i = 0; i < S_COUNT; i++) vp3d_mesh_create(&s_shape[i]);
    make_box(s_shape[S_BOX], 1);
    make_box(s_shape[S_BOX_OPEN], 0);
    make_pyramid(s_shape[S_PYRAMID]);
    make_gable(s_shape[S_GABLE]);
    make_hex(s_shape[S_HEX]);
    for (i = 0; i < S_COUNT; i++) vp3d_mesh_finish(s_shape[i]);
}

/* What the game has said of special places: flown through, or destroyed. */
#define MAX_MARKS 160
static struct { int cx, cz, how; } s_marks[MAX_MARKS];
static int s_mark_count;

static int mark_of(int cx, int cz)
{
    int i;
    for (i = 0; i < s_mark_count; i++)
        if (s_marks[i].cx == cx && s_marks[i].cz == cz) return s_marks[i].how;
    return 0;
}

void city_mark(int cx, int cz, int how)
{
    int i;
    for (i = 0; i < s_mark_count; i++)
        if (s_marks[i].cx == cx && s_marks[i].cz == cz) { s_marks[i].how = how; return; }
    if (s_mark_count == MAX_MARKS) s_mark_count = 0;       /* the oldest are forgotten: they are far behind */
    s_marks[s_mark_count].cx = cx;
    s_marks[s_mark_count].cz = cz;
    s_marks[s_mark_count].how = how;
    s_mark_count++;
}

void city_reset(void) { s_mark_count = 0; }

/* --- which building stands where ------------------------------------------- */

static unsigned s_h;
static float hrand(void)        /* 0..1, the next number of the cell's own row */
{
    s_h = s_h * 1664525u + 1013904223u;
    return (float)(s_h >> 8) / 16777216.0f;
}

static void add(building *b, int shape, float x, float y, float z, float sx, float sy, float sz)
{
    part *p;
    if (b->n >= MAX_PARTS) return;
    p = &b->p[b->n++];
    if (shape == S_BOX && y == 0.0f) shape = S_BOX_OPEN;    /* its floor lies on the ground: not drawn */
    p->shape = shape;
    p->x = x; p->y = y; p->z = z;
    p->sx = sx; p->sy = sy; p->sz = sz;
}

static void plan(building *b, int era)
{
    int kind = (int)(hrand() * 4.0f) & 3;
    float u = hrand(), v = hrand();

    switch (era) {
    case 0:     /* 1910: the countryside */
        if (kind == 0) {            /* a farmhouse */
            add(b, S_BOX, 0, 0, 0, 14, 7, 10);
            add(b, S_GABLE, 0, 7, 0, 14, 5, 10);
        } else if (kind == 1) {     /* a barn */
            add(b, S_BOX, 0, 0, 0, 16, 9, 24);
            add(b, S_GABLE, 0, 9, 0, 16, 7, 24);
        } else if (kind == 2) {     /* a church */
            add(b, S_BOX, 0, 0, -4, 10, 9, 22);
            add(b, S_GABLE, 0, 9, -4, 10, 6, 22);
            add(b, S_BOX, 0, 0, 11, 8, 22, 8);
            add(b, S_PYRAMID, 0, 22, 11, 8, 16, 8);
        } else {                    /* a windmill */
            add(b, S_HEX, 0, 0, 0, 9, 20, 9);
            add(b, S_PYRAMID, 0, 20, 0, 10, 6, 10);
            b->mill = 1; b->mill_y = 18; b->mill_z = 6; b->mill_r = 15;
        }
        break;
    case 1:     /* 1940: an airfield, a factory */
        if (kind == 0) {            /* a hangar */
            add(b, S_BOX, 0, 0, 0, 36, 10, 44);
            add(b, S_GABLE, 0, 10, 0, 36, 8, 44);
        } else if (kind == 1) {     /* a control tower */
            add(b, S_BOX, 0, 0, 0, 8, 24, 8);
            add(b, S_BOX, 0, 24, 0, 14, 6, 14);
        } else if (kind == 2) {     /* a factory with two chimneys */
            add(b, S_BOX, 0, 0, 0, 40, 14, 28);
            add(b, S_BOX, -12, 14, 6, 5, 22, 5);
            add(b, S_BOX, 8, 14, 6, 5, 28, 5);
        } else {                    /* two barracks */
            add(b, S_BOX, -10, 0, 0, 10, 6, 36);
            add(b, S_GABLE, -10, 6, 0, 10, 4, 36);
            add(b, S_BOX, 10, 0, 0, 10, 6, 36);
            add(b, S_GABLE, 10, 6, 0, 10, 4, 36);
        }
        break;
    case 2:     /* 1970: a base in the field */
        if (kind == 0) {            /* a radio mast */
            add(b, S_BOX, 0, 0, 0, 12, 5, 12);
            add(b, S_PYRAMID, 0, 5, 0, 7, 70 + 30 * u, 7);
        } else if (kind == 1) {     /* a bunker */
            add(b, S_BOX, 0, 0, 0, 30, 6, 24);
            add(b, S_PYRAMID, 0, 6, 0, 30, 5, 24);
        } else if (kind == 2) {     /* fuel tanks */
            add(b, S_HEX, -12, 0, 0, 18, 12, 18);
            add(b, S_HEX, 12, 0, 0, 18, 12, 18);
        } else {                    /* a watch tower beside a block */
            add(b, S_BOX, -14, 0, 0, 4, 20, 4);
            add(b, S_BOX, -14, 20, 0, 10, 5, 10);
            add(b, S_PYRAMID, -14, 25, 0, 10, 4, 10);
            add(b, S_BOX, 10, 0, 0, 20, 14 + 26 * u, 14);
        }
        break;
    case 3: {   /* 1982: a city */
        float w = 30 + 22 * u, d = 30 + 22 * v, h = 55 + 115 * hrand();
        if (kind == 0) {            /* a tower */
            add(b, S_BOX, 0, 0, 0, w, h, d);
        } else if (kind == 1) {     /* a tower stepped back, with a mast */
            add(b, S_BOX, 0, 0, 0, w, h * 0.6f, d);
            add(b, S_BOX, 0, h * 0.6f, 0, w * 0.6f, h * 0.4f, d * 0.6f);
            add(b, S_PYRAMID, 0, h, 0, 3, 26, 3);
        } else if (kind == 2) {     /* twins */
            add(b, S_BOX, -16, 0, 0, 22, h, 26);
            add(b, S_BOX, 16, 0, 0, 22, h * 0.8f, 26);
        } else {                    /* a wide low block */
            add(b, S_BOX, 0, 0, 0, 64, 22 + 14 * u, 60);
        }
        break;
    }
    default:    /* 2001: what the future built */
        if (kind == 0) {            /* a spire */
            add(b, S_HEX, 0, 0, 0, 40, 8, 40);
            add(b, S_PYRAMID, 0, 8, 0, 24, 140 + 70 * u, 24);
        } else if (kind == 1) {     /* a dome */
            add(b, S_HEX, 0, 0, 0, 64, 14, 64);
            add(b, S_PYRAMID, 0, 14, 0, 55, 30, 55);
        } else if (kind == 2) {     /* a tower with a deck on top */
            float h = 90 + 50 * u;
            add(b, S_HEX, 0, 0, 0, 14, h, 14);
            add(b, S_HEX, 0, h, 0, 44, 9, 44);
            add(b, S_PYRAMID, 0, h + 9, 0, 38, 14, 38);
        } else {                    /* a pyramid */
            add(b, S_PYRAMID, 0, 0, 0, 78, 50 + 20 * u, 78);
        }
        break;
    }
}

/* The special places, the same in every era but for their colours. */
static void plan_special(building *b, int special)
{
    if (special == CITY_GATE) {             /* two arches, 52 wide and 40 high inside, 28 apart (2026-10-22: were 40 by 34 - the frame was hit when clear of it) */
        int k;
        for (k = -1; k <= 1; k += 2) {
            float z = 14.0f * (float)k;
            add(b, S_BOX, -28, 0, z, 4, 40, 4);
            add(b, S_BOX, 28, 0, z, 4, 40, 4);
            add(b, S_BOX, 0, 40, z, 60, 4, 4);
        }
    } else if (special == CITY_DEPOT) {     /* a store with a pointed roof */
        add(b, S_BOX, 0, 0, 0, 26, 18, 26);
        add(b, S_PYRAMID, 0, 18, 0, 26, 9, 26);
    } else {                                /* a gun: a round pit and its mount */
        add(b, S_HEX, 0, 0, 0, 18, 5, 18);
        add(b, S_BOX, 0, 5, 0, 6, 4, 6);
    }
}

/* The building of a cell, if it has one. */
static int cell_building(int era, int cx, int cz, building *b)
{
    float s, room;
    int i;

    if (era < 0 || era > 4) return 0;
    s_h = (unsigned)cx * 73856093u ^ (unsigned)cz * 19349663u ^ (unsigned)(era + 1) * 83492791u;
    s_h ^= s_h >> 13; s_h *= 0x5bd1e995u; s_h ^= s_h >> 15;
    {
        /* A few cells are special, whatever the era's density. A place
         * with an item is not left to chance, which could leave an era
         * without one: in every block of 8 by 8 cells (1.3 km a side)
         * there is exactly one, a gate or a depot, in a cell of its own -
         * one within sight nearly always, and never two together. Guns
         * are chance: about one cell in thirty. */
        int bx = cx < 0 ? (cx - 7) / 8 : cx / 8, bz = cz < 0 ? (cz - 7) / 8 : cz / 8;
        unsigned k = (unsigned)bx * 2654435761u ^ (unsigned)bz * 2246822519u ^ (unsigned)(era + 7) * 3266489917u;
        int roll = (int)(hrand() * 1000.0f), mark = mark_of(cx, cz), special;
        k ^= k >> 15; k *= 0x2c1b3c6du; k ^= k >> 12; k *= 0x297a2d39u; k ^= k >> 15;
        if ((int)(k % 64u) == (cx - bx * 8) + 8 * (cz - bz * 8)) special = (k >> 8) & 1 ? CITY_GATE : CITY_DEPOT;
        else special = roll >= 11 && roll < 45 ? CITY_GUN : CITY_NONE;
        if (special == CITY_NONE && (int)(hrand() * 100.0f) >= density[era]) return 0;
        if (special != CITY_NONE && mark == CITY_GONE) return 0;        /* shot to pieces: nothing stands there */
        memset(b, 0, sizeof *b);
        b->special = special;
        b->spent = mark == CITY_SPENT;
        b->cx = cx;
        b->cz = cz;
    }
    b->shade = (int)(hrand() * 3.0f) % 3;
    if (b->special) plan_special(b, b->special); else plan(b, era);
    s = b->special ? 1.0f : scale[era];
    b->hx = b->hz = b->top = 0;
    for (i = 0; i < b->n; i++) {
        part *p = &b->p[i];
        float ex, ez;
        p->x *= s; p->y *= s; p->z *= s; p->sx *= s; p->sy *= s; p->sz *= s;
        ex = fabsf(p->x) + p->sx / 2.0f;
        ez = fabsf(p->z) + p->sz / 2.0f;
        if (ex > b->hx) b->hx = ex;
        if (ez > b->hz) b->hz = ez;
        if (p->y + p->sy > b->top) b->top = p->y + p->sy;
    }
    b->mill_y *= s; b->mill_z *= s; b->mill_r *= s;
    /* Somewhere in its cell, and all of it inside. */
    room = CELL_HALF - 4.0f - (b->hx > b->hz ? b->hx : b->hz);
    if (room < 0) room = 0;
    b->x = ((float)cx + 0.5f) * CELL + (hrand() * 2.0f - 1.0f) * room;
    b->z = ((float)cz + 0.5f) * CELL + (hrand() * 2.0f - 1.0f) * room;
    return 1;
}

static int cell_of(float v) { return (int)floorf(v / CELL); }

/* --- drawing ------------------------------------------------------------------ */

void city_draw(int era, vp_vec3 eye, float time, float grow)
{
    const vp3d_view *view = world_view();
    const float reach = s_reach, fade = s_reach * 0.28f;
    int cx0 = cell_of(eye.x - reach), cx1 = cell_of(eye.x + reach);
    int cz0 = cell_of(eye.z - reach), cz1 = cell_of(eye.z + reach);
    int cx, cz, i;
    building b;

    if (grow <= 0.01f) return;
    if (grow > 1.0f) grow = 1.0f;
    for (cz = cz0; cz <= cz1; cz++) {
        for (cx = cx0; cx <= cx1; cx++) {
            vp_vec3 c;
            float dx, dz, lit;
            if (!cell_building(era, cx, cz, &b)) continue;
            dx = b.x - eye.x; dz = b.z - eye.z;
            if (dx * dx + dz * dz > reach * reach) continue;
            lit = (reach - sqrtf(dx * dx + dz * dz)) / fade;
            if (lit > 1.0f) lit = 1.0f;
            if (lit < 0.04f) continue;
            /* behind the viewer, or off to the side of what is seen */
            c = vp_xf_point(&view->world_to_camera, vp_v3(b.x, b.top * 0.5f, b.z));
            if (c.z < -(b.hx + b.hz)) continue;
            if (fabsf(c.x) > 0.8f * c.z + b.hx + b.hz + 60.0f) continue;
            for (i = 0; i < b.n; i++) {
                const part *p = &b.p[i];
                static const unsigned char special_colour[4][3] = {
                    { 0, 0, 0 }, { 255, 170, 40 }, { 255, 255, 255 }, { 200, 90, 90 }
                };
                const unsigned char *col = b.special ? special_colour[b.special]
                                       : s_colour[era][p->shape == S_PYRAMID || p->shape == S_GABLE ? 3
                                                      : p->shape == S_HEX ? 4 : b.shade];
                vp_xf place = vp_xf_mul2(vp_xf_translate(b.x + p->x, p->y * grow, b.z + p->z),
                                         vp_xf_scale(p->sx, p->sy * grow, p->sz));
                world_rgb((int)((float)col[0] * lit), (int)((float)col[1] * lit), (int)((float)col[2] * lit));
                if (world_solid()) shape_solid(p->shape, &place); else world_mesh(s_shape[p->shape], &place);
            }
            if (b.mill) {           /* four sails, turning */
                vp_vec3 hub = vp_v3(b.x, b.mill_y * grow, b.z + b.mill_z);
                float a0 = time * 0.9f + (float)(cx * 7 + cz * 3);
                world_rgb((int)(255.0f * lit), (int)(255.0f * lit), (int)(255.0f * lit));
                for (i = 0; i < 4; i++) {
                    float a = a0 + 1.5707963f * (float)i;
                    world_line(hub, vp_v3(hub.x + b.mill_r * cosf(a), hub.y + b.mill_r * sinf(a) * grow, hub.z));
                }
            }
        }
    }
}

/* One special place by itself, as it stands in every era, where `at` puts
 * it (its foot at the origin of `at`): for the pages that explain it. */
void city_draw_site(int kind, const vp_xf *at)
{
    static const unsigned char colour[4][3] = { { 0, 0, 0 }, { 255, 170, 40 }, { 255, 255, 255 }, { 200, 90, 90 } };
    building b;
    int i;

    if (kind < CITY_GATE || kind > CITY_GUN) return;
    memset(&b, 0, sizeof b);
    plan_special(&b, kind);
    world_rgb(colour[kind][0], colour[kind][1], colour[kind][2]);
    for (i = 0; i < b.n; i++) {
        const part *p = &b.p[i];
        vp_xf place = vp_xf_mul2(*at, vp_xf_mul2(vp_xf_translate(p->x, p->y, p->z), vp_xf_scale(p->sx, p->sy, p->sz)));
        if (world_solid()) shape_solid(p->shape, &place); else world_mesh(s_shape[p->shape], &place);
    }
}

/* --- being in the way ----------------------------------------------------------- */

static int part_hit(const building *b, const part *p, vp_vec3 q, float r)
{
    float dx = fabsf(q.x - (b->x + p->x)), dz = fabsf(q.z - (b->z + p->z));
    float up = q.y - p->y, fx = 1.0f, fz = 1.0f;

    if (up < -r || up > p->sy + r) return 0;
    if (p->shape == S_PYRAMID || p->shape == S_GABLE) {     /* narrower the higher */
        float f = 1.0f - up / p->sy;
        if (f < 0) f = 0;
        if (f > 1.0f) f = 1.0f;
        fx = f;
        if (p->shape == S_PYRAMID) fz = f;
    }
    return dx < p->sx / 2.0f * fx + r && dz < p->sz / 2.0f * fz + r;
}

int city_hit(int era, vp_vec3 q, float radius)
{
    int cx = cell_of(q.x), cz = cell_of(q.z), i;
    building b;

    if (q.y > 260.0f) return 0;             /* nothing is that high */
    if (!cell_building(era, cx, cz, &b)) return 0;
    if (q.y > b.top + radius) return 0;
    for (i = 0; i < b.n; i++)
        if (part_hit(&b, &b.p[i], q, radius)) return 1;
    return 0;
}

float city_top(int era, float x, float z, float margin)
{
    int cx0 = cell_of(x - margin), cx1 = cell_of(x + margin);
    int cz0 = cell_of(z - margin), cz1 = cell_of(z + margin);
    int cx, cz;
    float top = 0;
    building b;

    for (cz = cz0; cz <= cz1; cz++)
        for (cx = cx0; cx <= cx1; cx++) {
            if (!cell_building(era, cx, cz, &b)) continue;
            if (fabsf(x - b.x) < b.hx + margin && fabsf(z - b.z) < b.hz + margin && b.top > top) top = b.top;
        }
    return top;
}

/* --- the special places, for the game ---------------------------------------------- */

static void site_of(const building *b, city_site *out)
{
    out->kind = b->special;
    out->cx = b->cx;
    out->cz = b->cz;
    out->spent = b->spent;
    out->at = vp_v3(b->x, b->special == CITY_GATE ? GATE_MIDDLE : b->top, b->z);
}

int city_sites(int era, vp_vec3 eye, float reach, city_site *out, int max)
{
    int cx0 = cell_of(eye.x - reach), cx1 = cell_of(eye.x + reach);
    int cz0 = cell_of(eye.z - reach), cz1 = cell_of(eye.z + reach);
    int cx, cz, n = 0;
    building b;

    for (cz = cz0; cz <= cz1; cz++)
        for (cx = cx0; cx <= cx1; cx++) {
            float dx, dz;
            if (n == max) return n;
            if (!cell_building(era, cx, cz, &b) || !b.special) continue;
            dx = b.x - eye.x; dz = b.z - eye.z;
            if (dx * dx + dz * dz > reach * reach) continue;
            site_of(&b, &out[n++]);
        }
    return n;
}

int city_site_at(int era, vp_vec3 p, float radius, city_site *out)
{
    int cx0 = cell_of(p.x - radius), cx1 = cell_of(p.x + radius);
    int cz0 = cell_of(p.z - radius), cz1 = cell_of(p.z + radius);
    int cx, cz;
    building b;

    for (cz = cz0; cz <= cz1; cz++)
        for (cx = cx0; cx <= cx1; cx++) {
            if (!cell_building(era, cx, cz, &b) || !b.special) continue;
            if (fabsf(p.x - b.x) > b.hx + radius || fabsf(p.z - b.z) > b.hz + radius || p.y > b.top + radius) continue;
            site_of(&b, out);
            return 1;
        }
    return 0;
}
