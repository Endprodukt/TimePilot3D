/* models.c - the shapes, drawn by hand as points and lines, in colours.
 *
 * Every line costs the beam; the shapes are kept to what reads as the
 * thing from a distance: a ring or two for a fuselage, the wings as
 * outlines, the tail as a few strokes. Symmetric parts are given once and
 * mirrored (mseg).
 *
 * A model has up to four colours. Its lines are sorted by colour into as
 * many meshes, so the SDK still joins each colour's lines into strokes and
 * the beam changes colour three times a model at most. While a shape is
 * built, C(n) says which colour the lines that follow have; points are not
 * shared between colours.
 */
#include "models.h"
#include "world.h"

#include <math.h>

#define MAX_VERTS    512
#define MAX_SEGMENTS 256

typedef unsigned char rgb[3];

static vp3d_mesh *s_mesh[MODEL_COUNT][MODEL_COLOURS];
static float      s_radius[MODEL_COUNT];
static vp3d_mesh *s_cur;

/* Each model's lines once more, as plain end points: what a wreck is
 * drawn from, and what flies apart when it bursts. */
static float         s_segments[MODEL_COUNT][MAX_SEGMENTS][6];
static unsigned char s_segment_colour[MODEL_COUNT][MAX_SEGMENTS];
static int           s_segment_count[MODEL_COUNT];
static float         s_halfspan[MODEL_COUNT];
static float         s_vert[MODEL_COLOURS][MAX_VERTS][3];
static int           s_which, s_slot;

/* Each model's faces, for solid drawing (world_face): found as it is
 * built - a closed ring of lines is a face, so is each square of a tube
 * between two rings and each triangle of a cone. Lines on their own
 * (seg) make none. A face's tag is the robot's limb it is part of. */
#define MAX_FACES 200
static float         s_facept[MODEL_COUNT][MAX_FACES][8][3];
static unsigned char s_face_n[MODEL_COUNT][MAX_FACES], s_face_tag[MODEL_COUNT][MAX_FACES];
static int           s_face_count[MODEL_COUNT], s_tag;

static void add_face(const int *v, int n)
{
    int k, i;
    if (n < 3) return;
    for (k = 1; k + 1 < n; k += 6) {            /* more than 8 corners: in fans of up to 8 from the first */
        int m = n - k > 7 ? 7 : n - k, f = s_face_count[s_which];
        if (f >= MAX_FACES) return;
        for (i = 0; i < 8 && i <= m; i++) {
            int idx = i == 0 ? v[0] : v[k + i - 1];
            if (idx < 0 || idx >= MAX_VERTS) return;
            s_facept[s_which][f][i][0] = s_vert[s_slot][idx][0];
            s_facept[s_which][f][i][1] = s_vert[s_slot][idx][1];
            s_facept[s_which][f][i][2] = s_vert[s_slot][idx][2];
        }
        s_face_n[s_which][f] = (unsigned char)(m + 1);
        s_face_tag[s_which][f] = (unsigned char)s_tag;
        s_face_count[s_which] = f + 1;
        if (n <= 8) return;
    }
}

/* A face by the coordinates of its corners (x, y, z, x, y, z, ...): the
 * flat parts that are drawn as single lines (wings, fins, tail planes). */
static void facef(const float *p, int n)
{
    int f = s_face_count[s_which], i;
    if (n < 3 || n > 8 || f >= MAX_FACES) return;
    for (i = 0; i < n; i++) {
        s_facept[s_which][f][i][0] = p[3 * i]; s_facept[s_which][f][i][1] = p[3 * i + 1]; s_facept[s_which][f][i][2] = p[3 * i + 2];
    }
    s_face_n[s_which][f] = (unsigned char)n;
    s_face_tag[s_which][f] = (unsigned char)s_tag;
    s_face_count[s_which] = f + 1;
}

/* The same, and its mirror image in x. */
static void mfacef(const float *p, int n)
{
    float q[24];
    int i;
    facef(p, n);
    for (i = 0; i < 3 * n && i < 24; i++) q[i] = (i % 3 == 0) ? -p[i] : p[i];
    facef(q, n);
}

/* The skin between two rings round the y axis (radius r0 at height y0,
 * r1 at y1; a radius of 0 is a point): `n` faces. */
static void skin_y(float r0, float y0, float r1, float y1, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        float a0 = 6.2831853f * (float)i / (float)n, a1 = 6.2831853f * (float)(i + 1) / (float)n;
        float q[12] = { r0 * sinf(a0), y0, r0 * cosf(a0), r0 * sinf(a1), y0, r0 * cosf(a1),
                        r1 * sinf(a1), y1, r1 * cosf(a1), r1 * sinf(a0), y1, r1 * cosf(a0) };
        facef(q, r1 > 0 ? 4 : 3);
    }
}

/* The skin between two rings round the z axis (as ring() makes them). */
static void skin_z(float r0, float z0, float r1, float z1, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        float a0 = 6.2831853f * (float)i / (float)n, a1 = 6.2831853f * (float)(i + 1) / (float)n;
        float q[12] = { r0 * sinf(a0), r0 * cosf(a0), z0, r0 * sinf(a1), r0 * cosf(a1), z0,
                        r1 * sinf(a1), r1 * cosf(a1), z1, r1 * sinf(a0), r1 * cosf(a0), z1 };
        if (r0 <= 0) { float t[9] = { 0, 0, z0, q[6], q[7], q[8], q[9], q[10], q[11] }; facef(t, 3); }
        else facef(q, r1 > 0 ? 4 : 3);
    }
}

int model_face_count(int which) { return which >= 0 && which < MODEL_COUNT ? s_face_count[which] : 0; }

int model_face(int which, int i, const float (**corners)[3], int *tag)
{
    *corners = (const float (*)[3])s_facept[which][i];
    if (tag) *tag = s_face_tag[which][i];
    return s_face_n[which][i];
}

/* The faces of a model where `place` puts it, given to world_face. */
void model_faces(int which, const vp_xf *place)
{
    int i, k;
    if (!world_solid() || which < 0 || which >= MODEL_COUNT) return;
    world_group();
    for (i = 0; i < s_face_count[which]; i++) {
        vp_vec3 p[8];
        for (k = 0; k < s_face_n[which][i]; k++)
            p[k] = vp_xf_point(place, vp_v3(s_facept[which][i][k][0], s_facept[which][i][k][1], s_facept[which][i][k][2]));
        world_face(p, s_face_n[which][i]);
    }
}

/* The colours of each model, by the number C() gives its lines. */
static const rgb s_palette[MODEL_COUNT][MODEL_COLOURS] = {
    /* the player's: blue body, pale wings, the nozzles' fire, a green canopy */
    { { 70, 150, 255 }, { 200, 235, 255 }, { 255, 170, 0 }, { 0, 255, 190 } },
    /* biplane: wooden body, canvas wings, grey struts and wheels, red tail and propeller */
    { { 230, 170, 60 }, { 255, 230, 150 }, { 160, 160, 160 }, { 255, 70, 50 } },
    /* monoplane: green body, lighter wings, yellow spinner and propeller, blue canopy */
    { { 90, 200, 80 }, { 160, 255, 120 }, { 255, 220, 0 }, { 120, 200, 255 } },
    /* helicopter: khaki body, olive boom, grey skids, blue glass */
    { { 220, 180, 70 }, { 150, 170, 60 }, { 160, 160, 160 }, { 120, 200, 255 } },
    /* jet: red body, orange wings, dark red tails, yellow canopy */
    { { 255, 60, 40 }, { 255, 130, 50 }, { 200, 40, 90 }, { 255, 230, 0 } },
    /* saucer: magenta rim, cyan dome, violet belly, yellow light */
    { { 255, 80, 255 }, { 80, 255, 255 }, { 150, 90, 255 }, { 255, 255, 0 } },
    /* airship: silver rings, grey hull lines, brown gondola, red fins */
    { { 220, 220, 235 }, { 140, 140, 170 }, { 210, 150, 70 }, { 255, 70, 50 } },
    /* bomber: olive body, green wings, grey engines, yellow tail */
    { { 130, 170, 90 }, { 170, 230, 120 }, { 180, 180, 180 }, { 255, 220, 0 } },
    /* big helicopter: olive body, khaki pylons, grey wheels and ramp, blue glass */
    { { 150, 170, 60 }, { 220, 180, 70 }, { 160, 160, 160 }, { 120, 200, 255 } },
    /* jet bomber: blue-grey body, steel wings, orange engines, red fin */
    { { 170, 190, 230 }, { 110, 140, 220 }, { 255, 150, 40 }, { 255, 60, 40 } },
    /* mothership: magenta rim, cyan deck, violet belly, yellow top */
    { { 255, 80, 255 }, { 80, 255, 255 }, { 150, 90, 255 }, { 255, 255, 0 } },
    /* rotor */
    { { 230, 230, 230 }, { 230, 230, 230 }, { 230, 230, 230 }, { 230, 230, 230 } },
    /* parachutist: white canopy, red gores, grey lines, yellow figure */
    { { 255, 255, 255 }, { 255, 80, 80 }, { 170, 170, 170 }, { 255, 220, 0 } },
    /* missile: white body, red fins */
    { { 255, 255, 255 }, { 255, 70, 50 }, { 255, 255, 255 }, { 255, 255, 255 } },
    /* the robot: white armour, a violet chest and pack, red skirt, shield and feet, yellow fin and lights */
    { { 235, 240, 255 }, { 130, 90, 255 }, { 255, 60, 50 }, { 255, 235, 0 } },
    /* the gunship: grey-green body, pale wings and tail, red rocket pods, yellow glass and gun */
    { { 150, 190, 150 }, { 210, 225, 200 }, { 255, 70, 50 }, { 255, 220, 0 } },
};

/* The big ones that cross every era are red and yellow whatever they are,
 * as the bombers of the original's 1940 are: seen at once for what they
 * are worth. */
static const rgb s_heavy[MODEL_COLOURS] = { { 255, 50, 30 }, { 255, 220, 0 }, { 255, 140, 0 }, { 255, 255, 255 } };
static const rgb s_escort[MODEL_COLOURS] = { { 90, 210, 255 }, { 220, 250, 255 }, { 40, 140, 255 }, { 255, 255, 255 } };

/* --- building helpers ---------------------------------------------------- */

/* The lines that follow have this colour of the model's. */
static void C(int slot)
{
    s_slot = slot;
    if (!s_mesh[s_which][slot]) vp3d_mesh_create(&s_mesh[s_which][slot]);
    s_cur = s_mesh[s_which][slot];
}

static int V(float x, float y, float z)
{
    int i = vp3d_mesh_vertex(s_cur, x, y, z);
    if (i >= 0 && i < MAX_VERTS) { s_vert[s_slot][i][0] = x; s_vert[s_slot][i][1] = y; s_vert[s_slot][i][2] = z; }
    if (fabsf(x) > s_halfspan[s_which]) s_halfspan[s_which] = fabsf(x);
    return i;
}

static void E(int a, int b)
{
    int n = s_segment_count[s_which];
    vp3d_mesh_line(s_cur, a, b);
    if (n < MAX_SEGMENTS && a >= 0 && a < MAX_VERTS && b >= 0 && b < MAX_VERTS) {
        float *s = s_segments[s_which][n];
        const float *p = s_vert[s_slot][a], *q = s_vert[s_slot][b];
        s[0] = p[0]; s[1] = p[1]; s[2] = p[2];
        s[3] = q[0]; s[4] = q[1]; s[5] = q[2];
        s_segment_colour[s_which][n] = (unsigned char)s_slot;
        s_segment_count[s_which] = n + 1;
    }
}

/* A closed loop through the given vertices. */
static void loop(const int *v, int n)
{
    int i;
    for (i = 0; i < n; i++) E(v[i], v[(i + 1) % n]);
    add_face(v, n);
}

/* A line from one point to another, both new. */
static void seg(float x0, float y0, float z0, float x1, float y1, float z1)
{
    E(V(x0, y0, z0), V(x1, y1, z1));
}

/* The same, and its mirror image in x. */
static void mseg(float x0, float y0, float z0, float x1, float y1, float z1)
{
    seg(x0, y0, z0, x1, y1, z1);
    seg(-x0, y0, z0, -x1, y1, z1);
}

/* A ring of `n` points round the z axis at depth z, radius rx by ry,
 * into v[]. */
static void ring(int *v, int n, float rx, float ry, float z, float cy)
{
    int i;
    for (i = 0; i < n; i++) {
        float a = 6.2831853f * (float)i / (float)n;
        v[i] = V(rx * sinf(a), cy + ry * cosf(a), z);
    }
}

/* A ring round the y axis (a disc seen from above) at height y. */
static void ring_y(int *v, int n, float r, float y)
{
    int i;
    for (i = 0; i < n; i++) {
        float a = 6.2831853f * (float)i / (float)n;
        v[i] = V(r * sinf(a), y, r * cosf(a));
    }
}

/* Lines between two rings of the same size: a tube. */
static void tube(const int *a, const int *b, int n)
{
    int i;
    for (i = 0; i < n; i++) E(a[i], b[i]);
    for (i = 0; i < n; i++) {
        int q[4];
        q[0] = a[i]; q[1] = a[(i + 1) % n]; q[2] = b[(i + 1) % n]; q[3] = b[i];
        add_face(q, 4);
    }
}

/* Lines from every point of a ring to one point: a cone. */
static void cone(const int *r, int n, int apex)
{
    int i;
    for (i = 0; i < n; i++) E(r[i], apex);
    for (i = 0; i < n; i++) {
        int t[3];
        t[0] = r[i]; t[1] = r[(i + 1) % n]; t[2] = apex;
        add_face(t, 3);
    }
}

/* A box: x0..x1, y0..y1, z0..z1, 12 edges. */
static void box(float x0, float y0, float z0, float x1, float y1, float z1)
{
    int a[4], b[4];
    a[0] = V(x0, y0, z0); a[1] = V(x1, y0, z0); a[2] = V(x1, y1, z0); a[3] = V(x0, y1, z0);
    b[0] = V(x0, y0, z1); b[1] = V(x1, y0, z1); b[2] = V(x1, y1, z1); b[3] = V(x0, y1, z1);
    loop(a, 4); loop(b, 4); tube(a, b, 4);
}

/* A point on a circle round the y axis. */
static float cx(float r, int i, int n) { return r * sinf(6.2831853f * (float)i / (float)n); }
static float cz(float r, int i, int n) { return r * cosf(6.2831853f * (float)i / (float)n); }

static void begin(int which)
{
    s_which = which;
    s_tag = 0;
    C(0);
}

static void end(int which)
{
    int slot;
    s_radius[which] = 0;
    for (slot = 0; slot < MODEL_COLOURS; slot++) {
        if (!s_mesh[which][slot]) continue;
        vp3d_mesh_finish(s_mesh[which][slot]);
        if (vp3d_mesh_radius(s_mesh[which][slot]) > s_radius[which]) s_radius[which] = vp3d_mesh_radius(s_mesh[which][slot]);
    }
    s_cur = NULL;
}

/* --- the shapes ----------------------------------------------------------- */

/* The player's fighter comes from the future, and looks it: a needle nose
 * on a flat body with a keel, wings swept FORWARD with fins at their tips,
 * canards beside the canopy, two fins canted outwards, two square
 * nozzles. It is seen mostly from behind. */
static void make_player(void)
{
    int nose, c[4], m[4], t[4], i;
    C(0);
    nose = V(0, 0, 9.5f);
    /* the body: three diamonds, wider and flatter toward the tail */
    c[0] = V(0, 0.9f, 4.0f);   c[1] = V(1.0f, 0, 4.0f);   c[2] = V(0, -0.5f, 4.0f);   c[3] = V(-1.0f, 0, 4.0f);
    m[0] = V(0, 1.0f, -1.0f);  m[1] = V(2.3f, 0, -1.0f);  m[2] = V(0, -0.8f, -1.0f);  m[3] = V(-2.3f, 0, -1.0f);
    t[0] = V(0, 0.6f, -5.5f);  t[1] = V(2.0f, 0, -5.5f);  t[2] = V(0, -0.5f, -5.5f);  t[3] = V(-2.0f, 0, -5.5f);
    cone(c, 4, nose); loop(c, 4); tube(c, m, 4); loop(m, 4); tube(m, t, 4); loop(t, 4);
    /* two fins, canted outwards */
    mseg(1.2f, 0.5f, -2.8f, 2.9f, 3.3f, -6.0f);
    mseg(2.9f, 3.3f, -6.0f, 1.6f, 0.3f, -5.5f);
    { static const float fin[] = { 1.2f, 0.5f, -2.8f, 2.9f, 3.3f, -6.0f, 1.6f, 0.3f, -5.5f }; mfacef(fin, 3); }
    { static const float wing[] = { 2.3f, 0, -1.0f, 7.2f, 0.4f, 2.4f, 7.2f, 0.4f, 0.8f, 2.0f, 0, -5.0f }; mfacef(wing, 4); }
    { static const float tipfin[] = { 7.2f, 0.4f, 2.4f, 7.9f, 2.0f, 0.4f, 7.2f, 0.4f, 0.8f }; mfacef(tipfin, 3); }
    { static const float canard[] = { 1.0f, 0.1f, 4.6f, 3.4f, 0.1f, 3.3f, 1.2f, 0.1f, 2.8f }; mfacef(canard, 3); }
    C(1);
    /* the wings, swept forward, each with a fin standing at its tip */
    mseg(2.3f, 0, -1.0f, 7.2f, 0.4f, 2.4f);
    mseg(7.2f, 0.4f, 2.4f, 7.2f, 0.4f, 0.8f);
    mseg(7.2f, 0.4f, 0.8f, 2.0f, 0, -5.0f);
    mseg(7.2f, 0.4f, 2.4f, 7.9f, 2.0f, 0.4f);
    mseg(7.9f, 2.0f, 0.4f, 7.2f, 0.4f, 0.8f);
    /* canards */
    mseg(1.0f, 0.1f, 4.6f, 3.4f, 0.1f, 3.3f);
    mseg(3.4f, 0.1f, 3.3f, 1.2f, 0.1f, 2.8f);
    C(2);
    /* two nozzles: a square behind each half of the tail */
    for (i = -1; i <= 1; i += 2) {
        float x = 0.95f * (float)i;
        int a = V(x - 0.6f, 0.45f, -6.4f), b = V(x + 0.6f, 0.45f, -6.4f);
        int d = V(x + 0.6f, -0.4f, -6.4f), e = V(x - 0.6f, -0.4f, -6.4f);
        int q[4];
        q[0] = a; q[1] = b; q[2] = d; q[3] = e;
        loop(q, 4);
        seg(x, 0.3f, -5.5f, x, 0.45f, -6.4f);
        seg(x, -0.25f, -5.5f, x, -0.4f, -6.4f);
    }
    C(3);
    /* the canopy */
    seg(0, 0.9f, 4.0f, 0, 1.8f, 1.8f);
    seg(0, 1.8f, 1.8f, 0, 1.0f, -1.0f);
}

/* 1910: a biplane. */
static void make_biplane(void)
{
    int nose, c[4], tail;
    C(0);
    nose = V(0, 0, 3.6f);
    c[0] = V(-0.6f, -0.6f, 0.8f); c[1] = V(0.6f, -0.6f, 0.8f); c[2] = V(0.6f, 0.7f, 0.8f); c[3] = V(-0.6f, 0.7f, 0.8f);
    tail = V(0, 0.3f, -3.6f);
    cone(c, 4, nose); loop(c, 4); cone(c, 4, tail);
    C(1);
    /* two wings, the upper ahead a little */
    {
        int lo[4], up[4];
        lo[0] = V(-4.5f, -0.6f, 0.9f); lo[1] = V(4.5f, -0.6f, 0.9f); lo[2] = V(4.5f, -0.6f, -0.4f); lo[3] = V(-4.5f, -0.6f, -0.4f);
        up[0] = V(-4.8f, 1.5f, 1.2f);  up[1] = V(4.8f, 1.5f, 1.2f);  up[2] = V(4.8f, 1.5f, -0.1f);  up[3] = V(-4.8f, 1.5f, -0.1f);
        loop(lo, 4); loop(up, 4);
    }
    C(2);
    /* the struts between them, and the wheels */
    mseg(2.6f, -0.6f, 0.9f, 2.6f, 1.5f, 1.2f);
    mseg(2.6f, -0.6f, -0.4f, 2.6f, 1.5f, -0.1f);
    mseg(1.0f, -0.6f, 0.5f, 1.0f, -1.6f, 0.3f);
    C(3);
    /* the tail: a flat plane and a fin */
    mseg(0, 0.3f, -2.6f, 1.7f, 0.3f, -3.6f);
    mseg(1.7f, 0.3f, -3.6f, 0, 0.3f, -3.6f);
    seg(0, 0.3f, -2.6f, 0, 1.6f, -3.6f);
    seg(0, 1.6f, -3.6f, 0, 0.3f, -3.6f);
    { static const float tp[] = { 0, 0.3f, -2.6f, 1.7f, 0.3f, -3.6f, 0, 0.3f, -3.6f }; mfacef(tp, 3); }
    { static const float fin[] = { 0, 0.3f, -2.6f, 0, 1.6f, -3.6f, 0, 0.3f, -3.6f }; facef(fin, 3); }
    /* the propeller */
    seg(-1.3f, 0, 3.7f, 1.3f, 0, 3.7f);
    seg(0, -1.3f, 3.7f, 0, 1.3f, 3.7f);
}

/* 1940: a monoplane fighter. */
static void make_monoplane(void)
{
    int spinner, n[4], c[4], tail;
    C(0);
    spinner = V(0, 0, 4.6f);
    n[0] = V(-0.6f, -0.6f, 3.4f); n[1] = V(0.6f, -0.6f, 3.4f); n[2] = V(0.6f, 0.6f, 3.4f); n[3] = V(-0.6f, 0.6f, 3.4f);
    c[0] = V(-0.6f, -0.7f, 0.3f); c[1] = V(0.6f, -0.7f, 0.3f); c[2] = V(0.6f, 0.8f, 0.3f); c[3] = V(-0.6f, 0.8f, 0.3f);
    tail = V(0, 0.3f, -4.5f);
    cone(n, 4, spinner); loop(n, 4); tube(n, c, 4); loop(c, 4); cone(c, 4, tail);
    C(1);
    /* the wing: a trapezoid each side */
    mseg(0.6f, -0.4f, 1.3f, 5.5f, -0.2f, 0.4f);
    mseg(5.5f, -0.2f, 0.4f, 5.5f, -0.2f, -0.5f);
    mseg(5.5f, -0.2f, -0.5f, 0.6f, -0.4f, -0.9f);
    { static const float wing[] = { 0.6f, -0.4f, 1.3f, 5.5f, -0.2f, 0.4f, 5.5f, -0.2f, -0.5f, 0.6f, -0.4f, -0.9f }; mfacef(wing, 4); }
    /* the tail */
    mseg(0, 0.3f, -3.0f, 2.2f, 0.3f, -4.0f);
    mseg(2.2f, 0.3f, -4.0f, 0, 0.3f, -4.5f);
    seg(0, 0.3f, -2.8f, 0, 1.9f, -4.3f);
    seg(0, 1.9f, -4.3f, 0, 0.3f, -4.5f);
    { static const float tp[] = { 0, 0.3f, -3.0f, 2.2f, 0.3f, -4.0f, 0, 0.3f, -4.5f }; mfacef(tp, 3); }
    { static const float fin[] = { 0, 0.3f, -2.8f, 0, 1.9f, -4.3f, 0, 0.3f, -4.5f }; facef(fin, 3); }
    C(3);
    /* the canopy */
    seg(0, 0.8f, 0.3f, 0, 1.3f, -0.5f);
    seg(0, 1.3f, -0.5f, 0, 0.7f, -1.4f);
    C(2);
    /* the propeller */
    seg(-1.5f, 0, 4.4f, 1.5f, 0, 4.4f);
    seg(0, -1.5f, 4.4f, 0, 1.5f, 4.4f);
}

/* 1970: a helicopter, its rotors drawn by the game (MODEL_ROTOR). */
static void make_helicopter(void)
{
    int f[4], r[4], boom, fin_top, i;
    C(0);
    f[0] = V(-1.2f, -1.1f, 1.2f); f[1] = V(1.2f, -1.1f, 1.2f); f[2] = V(1.2f, 1.3f, 1.2f); f[3] = V(-1.2f, 1.3f, 1.2f);
    r[0] = V(-1.0f, -0.9f, -1.3f); r[1] = V(1.0f, -0.9f, -1.3f); r[2] = V(1.0f, 1.1f, -1.3f); r[3] = V(-1.0f, 1.1f, -1.3f);
    loop(f, 4); tube(f, r, 4); loop(r, 4);
    /* the rotor mast */
    seg(0, 1.3f, 0, 0, 2.1f, 0);
    C(3);
    /* the glass nose */
    for (i = 0; i < 4; i++) {
        static const float fx[4] = { -1.2f, 1.2f, 1.2f, -1.2f }, fy[4] = { -1.1f, -1.1f, 1.3f, 1.3f };
        seg(fx[i], fy[i], 1.2f, 0, -0.2f, 3.2f);
    }
    C(1);
    /* the tail boom and its fin */
    boom = V(0, 0.5f, -6.5f);
    E(V(1.0f, 1.1f, -1.3f), boom); E(V(-1.0f, 1.1f, -1.3f), boom);
    E(V(-1.0f, -0.9f, -1.3f), boom); E(V(1.0f, -0.9f, -1.3f), boom);
    fin_top = V(0, 2.0f, -6.8f);
    E(boom, fin_top);
    seg(0, 2.0f, -6.8f, 0, 0.8f, -5.5f);
    {
        /* the boom's four sides, the fin, the glass nose's four */
        static const float rx[4] = { -1.0f, 1.0f, 1.0f, -1.0f }, ry[4] = { -0.9f, -0.9f, 1.1f, 1.1f };
        static const float fx[4] = { -1.2f, 1.2f, 1.2f, -1.2f }, fy[4] = { -1.1f, -1.1f, 1.3f, 1.3f };
        static const float fin[] = { 0, 0.5f, -6.5f, 0, 2.0f, -6.8f, 0, 0.8f, -5.5f };
        int k;
        for (k = 0; k < 4; k++) {
            float t[9] = { rx[k], ry[k], -1.3f, rx[(k + 1) % 4], ry[(k + 1) % 4], -1.3f, 0, 0.5f, -6.5f };
            float g[9] = { fx[k], fy[k], 1.2f, fx[(k + 1) % 4], fy[(k + 1) % 4], 1.2f, 0, -0.2f, 3.2f };
            facef(t, 3);
            facef(g, 3);
        }
        facef(fin, 3);
    }
    /* the tail rotor, fixed */
    seg(0.4f, 1.5f, -6.3f, 0.4f, -0.5f, -6.3f);
    C(2);
    /* the skids */
    mseg(1.1f, -2.0f, 1.8f, 1.1f, -2.0f, -1.8f);
    mseg(1.1f, -2.0f, 1.0f, 1.0f, -1.1f, 1.0f);
    mseg(1.1f, -2.0f, -1.0f, 0.9f, -0.9f, -1.0f);
}

/* 1982: a jet fighter with twin tails. */
static void make_jet(void)
{
    /* In full again (2026-10-21; plainer from 0.50.1 to 0.53.0, for a
     * monitor that could not take the load): a nose, the body in two
     * boxes with a ring at the middle, the deltas, the tailplanes, the
     * twin fins, the intakes and a canopy - about 38 lines. */
    int nose, c[4], m[4], t[4];
    C(0);
    nose = V(0, 0, 7.5f);
    c[0] = V(-0.9f, -0.7f, 2.5f); c[1] = V(0.9f, -0.7f, 2.5f); c[2] = V(0.9f, 0.85f, 2.5f); c[3] = V(-0.9f, 0.85f, 2.5f);
    m[0] = V(-1.0f, -0.65f, -1.5f); m[1] = V(1.0f, -0.65f, -1.5f); m[2] = V(1.0f, 0.75f, -1.5f); m[3] = V(-1.0f, 0.75f, -1.5f);
    t[0] = V(-0.9f, -0.5f, -6.0f); t[1] = V(0.9f, -0.5f, -6.0f); t[2] = V(0.9f, 0.6f, -6.0f); t[3] = V(-0.9f, 0.6f, -6.0f);
    cone(c, 4, nose); tube(c, m, 4); loop(m, 4); tube(m, t, 4); loop(t, 4);
    /* the intakes, under the wing roots */
    mseg(1.0f, -0.65f, 1.2f, 1.4f, -0.9f, 1.0f);
    mseg(1.4f, -0.9f, 1.0f, 1.0f, -0.65f, -1.5f);
    C(1);
    /* delta wings */
    mseg(0.9f, -0.3f, 0.5f, 5.5f, -0.3f, -4.5f);
    mseg(5.5f, -0.3f, -4.5f, 0.9f, -0.3f, -5.0f);
    { static const float wing[] = { 0.9f, -0.3f, 0.5f, 5.5f, -0.3f, -4.5f, 0.9f, -0.3f, -5.0f }; mfacef(wing, 3); }
    /* the tailplanes */
    mseg(0.9f, -0.2f, -4.6f, 3.0f, -0.2f, -6.3f);
    mseg(3.0f, -0.2f, -6.3f, 0.9f, -0.2f, -6.0f);
    { static const float tailplane[] = { 0.9f, -0.2f, -4.6f, 3.0f, -0.2f, -6.3f, 0.9f, -0.2f, -6.0f }; mfacef(tailplane, 3); }
    C(2);
    /* twin tails */
    mseg(0.9f, 0.6f, -4.0f, 1.2f, 2.8f, -6.2f);
    mseg(1.2f, 2.8f, -6.2f, 0.9f, 0.6f, -6.0f);
    { static const float fin[] = { 0.9f, 0.6f, -4.0f, 1.2f, 2.8f, -6.2f, 0.9f, 0.6f, -6.0f }; mfacef(fin, 3); }
    C(3);
    /* the canopy: a bubble over the front box */
    seg(0, 0.85f, 4.2f, 0, 1.45f, 2.2f);
    seg(0, 1.45f, 2.2f, 0, 0.8f, -0.2f);
    mseg(0.55f, 0.85f, 3.4f, 0.45f, 1.15f, 1.2f);
}

/* 2001: a flying saucer. */
static void make_ufo(void)
{
    int rim[8], up[8], lo[8], i;
    C(0);
    ring_y(rim, 8, 7.0f, 0);
    loop(rim, 8);
    C(1);
    /* the dome: a ring on four ribs */
    ring_y(up, 8, 3.2f, 1.3f);
    loop(up, 8);
    for (i = 0; i < 8; i += 2) seg(cx(7.0f, i, 8), 0, cz(7.0f, i, 8), cx(3.2f, i, 8), 1.3f, cz(3.2f, i, 8));
    C(2);
    /* the belly */
    ring_y(lo, 8, 3.0f, -1.0f);
    loop(lo, 8);
    for (i = 1; i < 8; i += 2) seg(cx(7.0f, i, 8), 0, cz(7.0f, i, 8), cx(3.0f, i, 8), -1.0f, cz(3.0f, i, 8));
    C(3);
    /* the light on top */
    for (i = 0; i < 8; i += 2) seg(cx(3.2f, i, 8), 1.3f, cz(3.2f, i, 8), 0, 2.4f, 0);
    skin_y(7.0f, 0, 3.2f, 1.3f, 8);
    skin_y(7.0f, 0, 3.0f, -1.0f, 8);
    skin_y(3.2f, 1.3f, 0, 2.4f, 8);
}

/* The 1910 boss: an airship with a gondola and four fins. */
static void make_blimp(void)
{
    static const float zs[5] = { 24, 13, 0, -13, -24 };
    static const float rs[5] = { 3.5f, 6.5f, 7.5f, 6.3f, 3.2f };
    int r[5][6], nose, tail, i;
    C(0);
    for (i = 0; i < 5; i++) { ring(r[i], 6, rs[i], rs[i], zs[i], 0); loop(r[i], 6); }
    C(1);
    /* six meridians, nose to tail, each one stroke */
    for (i = 0; i < 6; i++) {
        float a = 6.2831853f * (float)i / 6.0f;
        int k, prev;
        nose = V(0, 0, 31.0f);
        prev = nose;
        for (k = 0; k < 5; k++) { int p = V(rs[k] * sinf(a), rs[k] * cosf(a), zs[k]); E(prev, p); prev = p; }
        tail = V(0, 0, -31.0f);
        E(prev, tail);
    }
    C(2);
    /* the gondola, hung under the middle */
    box(-1.5f, -10.5f, -3.0f, 1.5f, -8.0f, 5.0f);
    mseg(1.2f, -8.0f, 3.0f, 2.5f, -6.8f, 3.0f);
    mseg(1.2f, -8.0f, -2.0f, 2.5f, -6.8f, -2.0f);
    C(3);
    /* four fins */
    seg(0, 3.2f, -24.0f, 0, 9.0f, -30.0f);  seg(0, 9.0f, -30.0f, 0, 0, -31.0f);
    seg(0, -3.2f, -24.0f, 0, -9.0f, -30.0f); seg(0, -9.0f, -30.0f, 0, 0, -31.0f);
    mseg(3.2f, 0, -24.0f, 9.0f, 0, -30.0f);  mseg(9.0f, 0, -30.0f, 0, 0, -31.0f);
    {
        static const float up[] = { 0, 3.2f, -24.0f, 0, 9.0f, -30.0f, 0, 0, -31.0f }, dn[] = { 0, -3.2f, -24.0f, 0, -9.0f, -30.0f, 0, 0, -31.0f };
        static const float sd[] = { 3.2f, 0, -24.0f, 9.0f, 0, -30.0f, 0, 0, -31.0f };
        int k;
        facef(up, 3); facef(dn, 3); mfacef(sd, 3);
        /* its skin: nose, between the rings, tail */
        skin_z(0, 31.0f, rs[0], zs[0], 6);
        for (k = 0; k < 4; k++) skin_z(rs[k], zs[k], rs[k + 1], zs[k + 1], 6);
        skin_z(rs[4], zs[4], 0, -31.0f, 6);
    }
}

/* The 1940 boss: a twin-engined bomber. */
static void make_bomber(void)
{
    int nose, n[4], m[4], t[4], tail;
    C(0);
    nose = V(0, 0, 11.0f);
    n[0] = V(-1.3f, -1.3f, 8.0f); n[1] = V(1.3f, -1.3f, 8.0f); n[2] = V(1.3f, 1.3f, 8.0f); n[3] = V(-1.3f, 1.3f, 8.0f);
    m[0] = V(-1.5f, -1.5f, 0);    m[1] = V(1.5f, -1.5f, 0);    m[2] = V(1.5f, 1.6f, 0);    m[3] = V(-1.5f, 1.6f, 0);
    t[0] = V(-0.8f, -0.6f, -9.0f); t[1] = V(0.8f, -0.6f, -9.0f); t[2] = V(0.8f, 0.9f, -9.0f); t[3] = V(-0.8f, 0.9f, -9.0f);
    tail = V(0, 0.4f, -11.0f);
    cone(n, 4, nose); loop(n, 4); tube(n, m, 4); loop(m, 4); tube(m, t, 4); loop(t, 4); cone(t, 4, tail);
    C(1);
    /* the wing */
    mseg(1.5f, -0.5f, 3.0f, 15.0f, 0.3f, 0.5f);
    mseg(15.0f, 0.3f, 0.5f, 15.0f, 0.3f, -1.5f);
    mseg(15.0f, 0.3f, -1.5f, 1.5f, -0.5f, -2.5f);
    { static const float wing[] = { 1.5f, -0.5f, 3.0f, 15.0f, 0.3f, 0.5f, 15.0f, 0.3f, -1.5f, 1.5f, -0.5f, -2.5f }; mfacef(wing, 4); }
    { static const float nac[] = { 5.0f, -0.5f, 5.0f, 5.0f, 0.8f, 5.0f, 5.0f, 0.8f, -1.0f, 5.0f, -1.2f, -1.0f }; mfacef(nac, 4); }
    C(2);
    /* the engines, one each side, with a propeller */
    mseg(5.0f, -0.5f, 5.0f, 5.0f, -1.2f, -1.0f);
    mseg(5.0f, 0.8f, 5.0f, 5.0f, 0.8f, -1.0f);
    mseg(5.0f, -0.5f, 5.0f, 5.0f, 0.8f, 5.0f);
    mseg(3.5f, 0.1f, 5.5f, 6.5f, 0.1f, 5.5f);
    mseg(5.0f, -1.4f, 5.5f, 5.0f, 1.6f, 5.5f);
    C(3);
    /* the tail: planes and twin fins */
    mseg(0.8f, 0.4f, -8.0f, 5.5f, 0.4f, -10.0f);
    mseg(5.5f, 0.4f, -10.0f, 0.8f, 0.4f, -10.5f);
    mseg(4.5f, 0.4f, -8.8f, 4.5f, 3.2f, -10.5f);
    mseg(4.5f, 3.2f, -10.5f, 4.5f, 0.4f, -10.3f);
    { static const float tp[] = { 0.8f, 0.4f, -8.0f, 5.5f, 0.4f, -10.0f, 0.8f, 0.4f, -10.5f }; mfacef(tp, 3); }
    { static const float fin[] = { 4.5f, 0.4f, -8.8f, 4.5f, 3.2f, -10.5f, 4.5f, 0.4f, -10.3f }; mfacef(fin, 3); }
}

/* The 1970 boss: a heavy twin-rotor helicopter, rotors by the game. */
static void taper(float x0, float x1, float z0, float z1, float y0, float X0, float X1, float Z0, float Z1, float Y1);

/* The Chinook as it is known at once: a long box of a hull with a blunt
 * glazed nose and a tail that sweeps up to the ramp; a low hump behind
 * the cockpit for the front rotor and a tall pylon at the tail for the
 * rear one, an engine on either side of it; long fuel tanks low along
 * both sides, round windows over them, four wheels. The rotors are the
 * game's (draw_rotors): hubs at (0, 4.6, 7.5) and (0, 6.9, -9). */
static void make_bigheli(void)
{
    /* the hull's sections, nose to tail: half width, floor, roof, where */
    static const float sec[5][4] = {
        { 1.0f, -1.4f, 0.6f, 12.5f }, { 2.2f, -2.3f, 2.0f, 9.5f }, { 2.4f, -2.4f, 2.6f, 7.0f },
        { 2.4f, -2.4f, 2.6f, -7.0f }, { 2.0f, -0.6f, 2.8f, -10.5f } };
    int ring[5][4], i, k;

    C(0);
    for (i = 0; i < 5; i++) {
        ring[i][0] = V(-sec[i][0], sec[i][1], sec[i][3]); ring[i][1] = V(sec[i][0], sec[i][1], sec[i][3]);
        ring[i][2] = V(sec[i][0], sec[i][2], sec[i][3]);  ring[i][3] = V(-sec[i][0], sec[i][2], sec[i][3]);
        loop(ring[i], 4);
        if (i > 0) tube(ring[i - 1], ring[i], 4);
    }
    /* the ribs of the cabin */
    for (k = 0; k < 2; k++) {
        float z = 2.5f - 4.5f * (float)k;
        mseg(2.4f, -2.4f, z, 2.4f, 2.6f, z);
        seg(-2.4f, 2.6f, z, 2.4f, 2.6f, z);
    }

    C(3);
    /* the glass: a post down the middle of the windscreen, a sill, the chin windows, and the round windows of the cabin */
    seg(0, 0.6f, 12.5f, 0, 2.0f, 9.5f);
    mseg(1.0f, -0.4f, 12.5f, 2.2f, 0, 9.5f);
    mseg(2.2f, 0, 9.5f, 2.4f, 0.4f, 7.0f); mseg(2.4f, 0.4f, 7.0f, 2.4f, 2.6f, 7.0f);
    mseg(0.3f, -1.4f, 12.5f, 1.2f, -2.3f, 9.5f);
    for (k = 0; k < 5; k++) {
        float z = 5.0f - 2.6f * (float)k;
        mseg(2.45f, 0.8f, z, 2.45f, 1.5f, z - 0.5f); mseg(2.45f, 1.5f, z - 0.5f, 2.45f, 0.8f, z - 1.0f);
    }

    C(1);
    /* the hump for the front rotor, the tall pylon for the rear one, and their masts */
    taper(-1.4f, 1.4f, 5.0f, 9.5f, 2.3f, -0.9f, 0.9f, 6.4f, 8.6f, 4.2f);
    seg(0, 4.2f, 7.5f, 0, 4.6f, 7.5f);
    taper(-1.7f, 1.7f, -10.5f, -3.5f, 2.6f, -0.8f, 0.8f, -10.2f, -7.8f, 6.4f);
    seg(0, 6.4f, -9.0f, 0, 6.9f, -9.0f);
    /* the fuel tanks, low along both sides */
    box(2.4f, -2.7f, -6.0f, 3.5f, -0.9f, 5.5f);
    box(-3.5f, -2.7f, -6.0f, -2.4f, -0.9f, 5.5f);

    C(2);
    /* an engine on either side of the rear pylon, its mouth forward */
    box(1.9f, 2.9f, -9.6f, 3.3f, 4.3f, -6.0f);
    box(-3.3f, 2.9f, -9.6f, -1.9f, 4.3f, -6.0f);
    mseg(1.9f, 2.9f, -6.0f, 3.3f, 4.3f, -6.0f);
    /* the ramp, let down, and the wheels */
    seg(-2.0f, -0.6f, -10.5f, -1.6f, -2.8f, -13.5f); seg(2.0f, -0.6f, -10.5f, 1.6f, -2.8f, -13.5f);
    seg(-1.6f, -2.8f, -13.5f, 1.6f, -2.8f, -13.5f);
    mseg(3.0f, -2.7f, 4.0f, 3.0f, -3.9f, 4.0f);   mseg(3.0f, -3.9f, 3.3f, 3.0f, -3.9f, 4.7f);
    mseg(3.0f, -2.7f, -5.0f, 3.0f, -3.9f, -5.0f); mseg(3.0f, -3.9f, -5.7f, 3.0f, -3.9f, -4.3f);
}

/* The 1982 boss: a big eight-engined jet bomber. */
static void make_b52(void)
{
    int nose, n[4], m[4], t[4], tail, s;
    C(0);
    nose = V(0, 0, 22.0f);
    n[0] = V(-1.6f, -1.6f, 16.0f); n[1] = V(1.6f, -1.6f, 16.0f); n[2] = V(1.6f, 1.8f, 16.0f); n[3] = V(-1.6f, 1.8f, 16.0f);
    m[0] = V(-1.8f, -1.8f, 0);     m[1] = V(1.8f, -1.8f, 0);     m[2] = V(1.8f, 1.8f, 0);     m[3] = V(-1.8f, 1.8f, 0);
    t[0] = V(-1.2f, -0.8f, -18.0f); t[1] = V(1.2f, -0.8f, -18.0f); t[2] = V(1.2f, 1.2f, -18.0f); t[3] = V(-1.2f, 1.2f, -18.0f);
    tail = V(0, 0.6f, -22.0f);
    cone(n, 4, nose); loop(n, 4); tube(n, m, 4); loop(m, 4); tube(m, t, 4); loop(t, 4); cone(t, 4, tail);
    C(1);
    /* swept wings */
    mseg(1.8f, 1.0f, 6.0f, 26.0f, -0.5f, -9.0f);
    mseg(26.0f, -0.5f, -9.0f, 26.0f, -0.5f, -11.0f);
    mseg(26.0f, -0.5f, -11.0f, 1.8f, 1.0f, -2.0f);
    { static const float wing[] = { 1.8f, 1.0f, 6.0f, 26.0f, -0.5f, -9.0f, 26.0f, -0.5f, -11.0f, 1.8f, 1.0f, -2.0f }; mfacef(wing, 4); }
    /* the tail planes */
    mseg(1.2f, 0.6f, -15.0f, 9.0f, 0.6f, -20.0f);
    mseg(9.0f, 0.6f, -20.0f, 1.2f, 0.6f, -20.5f);
    { static const float tp[] = { 1.2f, 0.6f, -15.0f, 9.0f, 0.6f, -20.0f, 1.2f, 0.6f, -20.5f }; mfacef(tp, 3); }
    C(2);
    /* engine pods: two pairs each side, hung under the wing */
    for (s = 0; s < 2; s++) {
        float x = s ? 17.0f : 8.5f, z = s ? -3.0f : 2.5f;
        mseg(x, -1.8f, z + 3.0f, x, -1.8f, z - 2.0f);
        mseg(x, -3.0f, z + 3.0f, x, -3.0f, z - 2.0f);
        mseg(x, -1.8f, z + 3.0f, x, -3.0f, z + 3.0f);
        mseg(x, -1.8f, z - 2.0f, x, -3.0f, z - 2.0f);
        mseg(x, -1.8f, z + 0.5f, x, 0.2f, z - 1.0f);
        { float pod[] = { x, -1.8f, z + 3.0f, x, -1.8f, z - 2.0f, x, -3.0f, z - 2.0f, x, -3.0f, z + 3.0f }; mfacef(pod, 4); }
    }
    C(3);
    /* the tall fin */
    seg(0, 1.5f, -12.0f, 0, 9.0f, -20.0f);
    seg(0, 9.0f, -20.0f, 0, 1.0f, -21.0f);
    { static const float fin[] = { 0, 1.5f, -12.0f, 0, 9.0f, -20.0f, 0, 1.0f, -21.0f }; facef(fin, 3); }
}

/* The 2001 boss: a mothership, two decks of saucer. */
static void make_mothership(void)
{
    int rim[12], deck[12], lo[8], top[6], i;
    C(0);
    ring_y(rim, 12, 30.0f, 0);
    loop(rim, 12);
    C(1);
    ring_y(deck, 12, 18.0f, 6.0f);
    loop(deck, 12);
    for (i = 0; i < 12; i++) seg(cx(30.0f, i, 12), 0, cz(30.0f, i, 12), cx(18.0f, i, 12), 6.0f, cz(18.0f, i, 12));
    C(2);
    ring_y(lo, 8, 14.0f, -5.0f);
    loop(lo, 8);
    for (i = 0; i < 8; i += 2) seg(cx(30.0f, (i * 3) / 2, 12), 0, cz(30.0f, (i * 3) / 2, 12), cx(14.0f, i, 8), -5.0f, cz(14.0f, i, 8));
    C(3);
    ring_y(top, 6, 7.0f, 11.0f);
    loop(top, 6);
    for (i = 0; i < 6; i++) seg(cx(18.0f, i * 2, 12), 6.0f, cz(18.0f, i * 2, 12), cx(7.0f, i, 6), 11.0f, cz(7.0f, i, 6));
    for (i = 0; i < 6; i += 2) seg(cx(7.0f, i, 6), 11.0f, cz(7.0f, i, 6), 0, 15.0f, 0);
    skin_y(30.0f, 0, 18.0f, 6.0f, 12);
    skin_y(30.0f, 0, 14.0f, -5.0f, 12);
    skin_y(18.0f, 6.0f, 7.0f, 11.0f, 6);
    skin_y(7.0f, 11.0f, 0, 15.0f, 6);
}

/* Two rotor blades, in the x-z plane, radius 1: the game scales and turns it. */
static void make_rotor(void)
{
    seg(-1.0f, 0, 0, 1.0f, 0, 0);
    seg(0, 0, -1.0f, 0, 0, 1.0f);
}

/* A parachutist: the canopy as two arcs over a ring, lines to a figure. */
static void make_parachute(void)
{
    int rim[6], i;
    C(0);
    ring_y(rim, 6, 4.0f, 0);
    loop(rim, 6);
    C(1);
    /* the gores: two arcs over the top, each one stroke */
    {
        int a = V(cx(4.0f, 1, 6), 0, cz(4.0f, 1, 6)), b = V(2.6f, 2.0f, 0), top = V(0, 2.8f, 0);
        int c = V(-2.6f, 2.0f, 0), d = V(cx(4.0f, 4, 6), 0, cz(4.0f, 4, 6));
        E(a, b); E(b, top); E(top, c); E(c, d);
        a = V(cx(4.0f, 0, 6), 0, cz(4.0f, 0, 6)); b = V(0, 2.0f, 2.6f); top = V(0, 2.8f, 0);
        c = V(0, 2.0f, -2.6f); d = V(cx(4.0f, 3, 6), 0, cz(4.0f, 3, 6));
        E(a, b); E(b, top); E(top, c); E(c, d);
    }
    C(2);
    for (i = 0; i < 6; i += 2) seg(cx(4.0f, i, 6), 0, cz(4.0f, i, 6), 0, -5.0f, 0);
    C(3);
    /* the figure */
    seg(0, -5.0f, 0, 0, -7.2f, 0);
    seg(-0.8f, -5.6f, 0, 0.8f, -5.6f, 0);
    seg(-0.7f, -8.5f, 0, 0, -7.2f, 0); seg(0, -7.2f, 0, 0.7f, -8.5f, 0);
}

/* A missile: a spike with four fins. */
static void make_missile(void)
{
    C(0);
    seg(0, 0, 2.0f, 0, 0, -1.5f);
    C(1);
    seg(0, 0, -1.5f, 0.7f, 0, -1.9f);  seg(0, 0, -1.5f, -0.7f, 0, -1.9f);
    seg(0, 0, -1.5f, 0, 0.7f, -1.9f);  seg(0, 0, -1.5f, 0, -0.7f, -1.9f);
}

/* A box whose top is another size than its bottom: the rectangle
 * x0..x1 by z0..z1 at height y0, the rectangle X0..X1 by Z0..Z1 at Y1,
 * and the four edges between. Armour is made of these. */
static void taper(float x0, float x1, float z0, float z1, float y0, float X0, float X1, float Z0, float Z1, float Y1)
{
    int a[4], b[4];
    a[0] = V(x0, y0, z0); a[1] = V(x1, y0, z0); a[2] = V(x1, y0, z1); a[3] = V(x0, y0, z1);
    b[0] = V(X0, Y1, Z0); b[1] = V(X1, Y1, Z0); b[2] = V(X1, Y1, Z1); b[3] = V(X0, Y1, Z1);
    loop(a, 4); loop(b, 4); tube(a, b, 4);
}

/* The robot the mothership unfolds into, standing, facing +z: a machine
 * a man could sit in the chest of, as they are drawn in Japan. A small
 * helmet with a V of two fins over its eyes and a pointed chin; a chest
 * that is wide at the shoulders and narrow at the waist, with vents and
 * a light; a skirt of armour; a pack on its back with two hilts. Its
 * arms and legs are limbs of their own (model_robot_limb), so that the
 * game can aim an arm, shoot it off, and make the legs walk: an arm is
 * wide angular shoulder armour with a pylon standing up from it, an
 * upper arm, and a forearm held forward - the right one with a small
 * twin gun and a rifle under it, the left one with one big cannon; a leg
 * is a thin thigh, a knee plate, a lower leg that flares to the foot.
 * Soles at -28, the crown at 23, the fins to 29. The arms turn about
 * their shoulders (x = +-12, y = 14), the legs about their hips
 * (y = -6): boss.c. */
static unsigned char s_robot_limb[MAX_SEGMENTS];

int model_robot_limb(int segment)
{
    return segment >= 0 && segment < MAX_SEGMENTS ? s_robot_limb[segment] : LIMB_BODY;
}

/* an arm on the side `sx` (1 right, -1 left) */
static void robot_arm(float sx, int big)
{
    C(0);
    if (sx > 0) taper(9.5f, 15.0f, -4.0f, 4.0f, 12.0f, 9.0f, 18.0f, -5.0f, 5.0f, 18.5f);        /* shoulder armour, flaring out */
    else        taper(-15.0f, -9.5f, -4.0f, 4.0f, 12.0f, -18.0f, -9.0f, -5.0f, 5.0f, 18.5f);
    seg(sx * 10.5f, 12.0f, -2.0f, sx * 10.5f, 3.5f, -1.0f); seg(sx * 14.0f, 12.0f, -2.0f, sx * 14.5f, 3.5f, -1.0f);   /* the upper arm */
    seg(sx * 10.5f, 12.0f, 2.0f, sx * 10.0f, 3.5f, 1.5f);   seg(sx * 14.0f, 12.0f, 2.0f, sx * 14.5f, 3.5f, 1.5f);
    if (sx > 0) taper(10.0f, 14.5f, -1.0f, 11.0f, -1.0f, 10.0f, 14.5f, -1.0f, 11.0f, 3.5f);     /* the forearm, held forward */
    else        taper(-14.5f, -10.0f, -1.0f, 11.0f, -1.0f, -14.5f, -10.0f, -1.0f, 11.0f, 3.5f);
    C(1);
    seg(sx * 11.0f, 18.5f, -3.0f, sx * 12.5f, 27.0f, -4.0f); seg(sx * 15.0f, 18.5f, -3.0f, sx * 12.5f, 27.0f, -4.0f);   /* the pylon */
    if (!big) {
        /* the small gun: two barrels out of the forearm, and a rifle under it */
        seg(sx * 12.2f, -2.2f, 2.0f, sx * 12.2f, -2.2f, 18.0f);
        seg(sx * 12.2f, -3.8f, 4.0f, sx * 12.2f, -3.8f, 12.0f); seg(sx * 12.2f, -2.2f, 4.0f, sx * 12.2f, -3.8f, 4.0f);
        seg(sx * 12.2f, -2.2f, 12.0f, sx * 12.2f, -3.8f, 12.0f);
        C(3);
        seg(sx * 11.0f, 2.5f, 11.0f, sx * 11.0f, 2.5f, 14.0f); seg(sx * 13.5f, 0, 11.0f, sx * 13.5f, 0, 14.0f);
        seg(sx * 11.0f, 2.5f, 14.0f, sx * 13.5f, 0, 14.0f);
    } else {
        /* the big gun: one fat barrel under the forearm, far out past the fist, a brake on its mouth */
        float x0 = sx > 0 ? 9.8f : -14.8f, x1 = x0 + 5.0f;
        C(2);
        box(x0, -6.5f, 1.0f, x1, -1.5f, 20.0f);
        C(3);
        seg(x0 - 1.2f, -4.0f, 20.0f, x1 + 1.2f, -4.0f, 20.0f); seg(x0 + 2.5f, -7.7f, 20.0f, x0 + 2.5f, -0.3f, 20.0f);
        seg(x0, -6.5f, 20.0f, x1, -1.5f, 20.0f); seg(x1, -6.5f, 20.0f, x0, -1.5f, 20.0f);
    }
}

static void robot_leg(float sx)
{
    C(0);
    seg(sx * 2.2f, -6.0f, -3.0f, sx * 2.5f, -15.0f, -2.5f); seg(sx * 6.8f, -6.0f, -3.0f, sx * 6.5f, -15.0f, -2.5f);   /* the thigh */
    seg(sx * 2.2f, -6.0f, 3.0f, sx * 2.5f, -15.0f, 3.0f);   seg(sx * 6.8f, -6.0f, 3.0f, sx * 6.5f, -15.0f, 3.0f);
    if (sx > 0) taper(1.5f, 8.0f, -4.5f, 4.0f, -26.0f, 2.5f, 6.5f, -2.5f, 3.0f, -15.0f);        /* the lower leg, flaring to the foot */
    else        taper(-8.0f, -1.5f, -4.5f, 4.0f, -26.0f, -6.5f, -2.5f, -2.5f, 3.0f, -15.0f);
    seg(sx * 2.5f, -13.0f, 3.3f, sx * 6.5f, -13.0f, 3.3f);                                         /* the knee plate */
    seg(sx * 2.5f, -13.0f, 3.3f, sx * 4.5f, -18.0f, 4.8f); seg(sx * 6.5f, -13.0f, 3.3f, sx * 4.5f, -18.0f, 4.8f);
    C(2);
    seg(sx * 1.5f, -26.0f, 4.0f, sx * 2.0f, -28.0f, 9.5f); seg(sx * 8.0f, -26.0f, 4.0f, sx * 7.5f, -28.0f, 9.5f);   /* the foot */
    seg(sx * 2.0f, -28.0f, 9.5f, sx * 7.5f, -28.0f, 9.5f);
    seg(sx * 2.0f, -28.0f, 9.5f, sx * 1.5f, -28.0f, -4.5f); seg(sx * 7.5f, -28.0f, 9.5f, sx * 8.0f, -28.0f, -4.5f);
    seg(sx * 1.5f, -28.0f, -4.5f, sx * 8.0f, -28.0f, -4.5f);
}

static void make_robot(void)
{
    int from, limb, i;

    /* the body */
    C(0);
    taper(-1.6f, 1.6f, -2.0f, 3.4f, 17.0f, -3.0f, 3.0f, -3.0f, 2.4f, 22.5f);             /* the helmet */
    seg(0, 16.0f, 0, 0, 17.0f, 0);                                                        /* the neck */
    C(1);
    taper(-4.5f, 4.5f, -3.5f, 4.5f, 6.0f, -9.5f, 9.5f, -5.0f, 5.5f, 16.0f);              /* the chest: wide above, narrow below */
    mseg(4.5f, 6.0f, 4.5f, 3.5f, 0, 3.8f); mseg(4.5f, 6.0f, -3.5f, 3.5f, 0, -3.5f);       /* the belly */
    box(-5.0f, 8.0f, -9.0f, 5.0f, 17.0f, -5.0f);                                          /* the pack */
    mseg(4.0f, 17.0f, -7.0f, 6.5f, 25.0f, -9.5f);                                         /* its two hilts */
    C(2);
    taper(-7.5f, 7.5f, -4.5f, 5.5f, -6.0f, -5.5f, 5.5f, -4.0f, 4.0f, 0);                  /* the skirt */
    seg(0, 0, 4.0f, 0, -6.0f, 5.5f);                                                      /* split down its front */
    mseg(0, 16.4f, 4.0f, 1.8f, 18.2f, 3.3f);                                              /* the chin */
    seg(0, 21.5f, 2.9f, 0, 24.5f, 2.6f);                                                  /* the crest */
    C(3);
    mseg(0, 21.5f, 3.0f, 7.5f, 29.0f, 0.5f); mseg(0, 21.5f, 3.0f, 4.0f, 23.5f, 2.4f);     /* the V of fins */
    mseg(0.5f, 19.8f, 3.1f, 2.8f, 20.6f, 2.8f);                                           /* the eyes */
    mseg(3.5f, 13.6f, 5.5f, 8.0f, 14.8f, 5.6f); mseg(3.5f, 12.0f, 5.4f, 7.4f, 13.2f, 5.5f);   /* the vents */
    seg(0, 5.5f, 5.3f, 3.5f, 8.5f, 5.3f); seg(3.5f, 8.5f, 5.3f, 0, 11.5f, 5.3f);          /* the light in its chest */
    seg(0, 11.5f, 5.3f, -3.5f, 8.5f, 5.3f); seg(-3.5f, 8.5f, 5.3f, 0, 5.5f, 5.3f);

    /* the limbs, each a run of lines of its own */
    for (limb = LIMB_ARM_R; limb <= LIMB_LEG_L; limb++) {
        from = s_segment_count[s_which];
        s_tag = limb;
        switch (limb) {
        case LIMB_ARM_R: robot_arm(1.0f, 0);  break;
        case LIMB_ARM_L: robot_arm(-1.0f, 1); break;
        case LIMB_LEG_R: robot_leg(1.0f);     break;
        default:         robot_leg(-1.0f);    break;
        }
        for (i = from; i < s_segment_count[s_which] && i < MAX_SEGMENTS; i++) s_robot_limb[i] = (unsigned char)limb;
    }
    s_tag = 0;
}

/* The Chinook's escort: a gunship, as the Cobra is - another thing than
 * the khaki helicopters of the era at a glance: a body as narrow as a
 * knife, two seats one behind the other under a stepped canopy, stub wings
 * with a red rocket pod under each, a gun under its chin, a long tail
 * boom with a fin. Its rotor is the game's (draw_rotors), on the mast at
 * (0, 1.9, -0.4). */
static void make_gunship(void)
{
    int s0[4], s1[4], s2[4], s3[4], nose, tip;
    C(0);
    s0[0] = V(-0.35f, -0.5f, 4.2f); s0[1] = V(0.35f, -0.5f, 4.2f); s0[2] = V(0.35f, 0.3f, 4.2f); s0[3] = V(-0.35f, 0.3f, 4.2f);
    s1[0] = V(-0.6f, -0.9f, 2.5f);  s1[1] = V(0.6f, -0.9f, 2.5f);  s1[2] = V(0.6f, 0.9f, 2.5f);  s1[3] = V(-0.6f, 0.9f, 2.5f);
    s2[0] = V(-0.65f, -0.9f, -1.5f); s2[1] = V(0.65f, -0.9f, -1.5f); s2[2] = V(0.65f, 1.0f, -1.5f); s2[3] = V(-0.65f, 1.0f, -1.5f);
    s3[0] = V(-0.4f, -0.4f, -2.8f); s3[1] = V(0.4f, -0.4f, -2.8f); s3[2] = V(0.4f, 0.6f, -2.8f); s3[3] = V(-0.4f, 0.6f, -2.8f);
    nose = V(0, -0.2f, 5.0f);
    tip = V(0, 0.3f, -8.2f);
    cone(s0, 4, nose); loop(s0, 4); tube(s0, s1, 4); loop(s1, 4); tube(s1, s2, 4); loop(s2, 4); tube(s2, s3, 4); cone(s3, 4, tip);
    seg(0, 1.0f, -0.4f, 0, 1.9f, -0.4f);                                        /* the rotor mast */
    C(3);
    /* the stepped canopy: the gunner ahead and low, the pilot behind and higher */
    seg(0, 0.3f, 4.2f, 0, 1.2f, 2.3f); seg(0, 1.2f, 2.3f, 0, 1.3f, 1.4f); seg(0, 1.3f, 1.4f, 0, 1.6f, 0.9f);
    seg(0, 1.6f, 0.9f, 0, 1.6f, -0.3f); seg(0, 1.6f, -0.3f, 0, 1.0f, -1.3f);
    mseg(0.6f, 0.9f, 2.5f, 0.35f, 1.25f, 1.9f); mseg(0.35f, 1.25f, 1.9f, 0.4f, 1.55f, 0.4f);
    /* the gun under its chin */
    seg(0, -0.9f, 3.0f, 0, -1.25f, 3.4f); seg(0, -1.25f, 3.4f, 0, -1.3f, 4.8f);
    C(1);
    /* stub wings */
    mseg(0.65f, 0, 0.2f, 2.6f, -0.2f, 0.0f); mseg(2.6f, -0.2f, 0.0f, 2.6f, -0.2f, -1.0f); mseg(2.6f, -0.2f, -1.0f, 0.65f, 0, -1.2f);
    { static const float wing[] = { 0.65f, 0, 0.2f, 2.6f, -0.2f, 0.0f, 2.6f, -0.2f, -1.0f, 0.65f, 0, -1.2f }; mfacef(wing, 4); }
    /* the fin, and the tail planes */
    seg(0, 0.4f, -7.0f, 0, 2.1f, -8.4f); seg(0, 2.1f, -8.4f, 0, 0.3f, -8.2f);
    { static const float fin[] = { 0, 0.4f, -7.0f, 0, 2.1f, -8.4f, 0, 0.3f, -8.2f }; facef(fin, 3); }
    mseg(0.2f, 0.35f, -5.4f, 1.4f, 0.35f, -6.0f); mseg(1.4f, 0.35f, -6.0f, 0.2f, 0.35f, -6.3f);
    { static const float tp[] = { 0.2f, 0.35f, -5.4f, 1.4f, 0.35f, -6.0f, 0.2f, 0.35f, -6.3f }; mfacef(tp, 3); }
    /* the tail rotor, fixed */
    seg(0.35f, 1.7f, -8.3f, 0.35f, -0.3f, -8.3f);
    C(2);
    /* the rocket pods, one under each wing */
    box(1.5f, -0.75f, -1.3f, 2.2f, -0.25f, 0.9f);
    box(-2.2f, -0.75f, -1.3f, -1.5f, -0.25f, 0.9f);
    /* skids */
    mseg(0.9f, -1.6f, 1.8f, 0.9f, -1.6f, -1.8f);
    mseg(0.9f, -1.6f, 1.0f, 0.55f, -0.9f, 1.0f);
    mseg(0.9f, -1.6f, -1.0f, 0.55f, -0.9f, -1.0f);
}

int models_init(void)
{
    begin(MODEL_PLAYER);     make_player();     end(MODEL_PLAYER);
    begin(MODEL_BIPLANE);    make_biplane();    end(MODEL_BIPLANE);
    begin(MODEL_MONOPLANE);  make_monoplane();  end(MODEL_MONOPLANE);
    begin(MODEL_HELICOPTER); make_helicopter(); end(MODEL_HELICOPTER);
    begin(MODEL_JET);        make_jet();        end(MODEL_JET);
    begin(MODEL_UFO);        make_ufo();        end(MODEL_UFO);
    begin(MODEL_BLIMP);      make_blimp();      end(MODEL_BLIMP);
    begin(MODEL_BOMBER);     make_bomber();     end(MODEL_BOMBER);
    begin(MODEL_BIGHELI);    make_bigheli();    end(MODEL_BIGHELI);
    begin(MODEL_B52);        make_b52();        end(MODEL_B52);
    begin(MODEL_MOTHERSHIP); make_mothership(); end(MODEL_MOTHERSHIP);
    begin(MODEL_ROTOR);      make_rotor();      end(MODEL_ROTOR);
    begin(MODEL_PARACHUTE);  make_parachute();  end(MODEL_PARACHUTE);
    begin(MODEL_MISSILE);    make_missile();    end(MODEL_MISSILE);
    begin(MODEL_ROBOT);      make_robot();      end(MODEL_ROBOT);
    begin(MODEL_GUNSHIP);    make_gunship();    end(MODEL_GUNSHIP);
    return VP_OK;
}

/* --- asking, and drawing ------------------------------------------------------ */

float model_radius(int which)
{
    return which >= 0 && which < MODEL_COUNT ? s_radius[which] : 0.0f;
}

int model_segments(int which, const float **segments)
{
    if (which < 0 || which >= MODEL_COUNT) { *segments = NULL; return 0; }
    *segments = &s_segments[which][0][0];
    return s_segment_count[which];
}

float model_halfspan(int which)
{
    return which >= 0 && which < MODEL_COUNT ? s_halfspan[which] : 0.0f;
}

void model_colour(int which, int slot, int tint, int *r, int *g, int *b)
{
    const unsigned char *c;
    if (which < 0 || which >= MODEL_COUNT) which = 0;
    slot &= MODEL_COLOURS - 1;
    c = tint == TINT_HEAVY ? s_heavy[slot] : tint == TINT_ESCORT ? s_escort[slot] : s_palette[which][slot];
    *r = c[0]; *g = c[1]; *b = c[2];
    if (tint == TINT_WHITE) { *r = *g = *b = 255; }
    else if (tint == TINT_FIRE) { *r = 255; *g = 110 + 40 * slot; *b = 30; }
}

int model_segment_colour(int which, int segment)
{
    if (which < 0 || which >= MODEL_COUNT || segment < 0 || segment >= s_segment_count[which]) return 0;
    return s_segment_colour[which][segment];
}

void model_draw(int which, const vp_xf *place, int tint)
{
    int slot, r, g, b;
    if (which < 0 || which >= MODEL_COUNT) return;
    if (world_solid()) {
        /* line by line, so that what is behind a building is cut (world.c) */
        int i, last = -1;
        for (i = 0; i < s_segment_count[which]; i++) {
            const float *q = s_segments[which][i];
            if (s_segment_colour[which][i] != last) {
                last = s_segment_colour[which][i];
                model_colour(which, last, tint, &r, &g, &b);
                world_rgb(r, g, b);
            }
            world_line(vp_xf_point(place, vp_v3(q[0], q[1], q[2])), vp_xf_point(place, vp_v3(q[3], q[4], q[5])));
        }
        model_faces(which, place);
        return;
    }
    for (slot = 0; slot < MODEL_COLOURS; slot++) {
        if (!s_mesh[which][slot] || vp3d_mesh_edge_count(s_mesh[which][slot]) == 0) continue;
        model_colour(which, slot, tint, &r, &g, &b);
        world_rgb(r, g, b);
        world_mesh(s_mesh[which][slot], place);
    }
}
