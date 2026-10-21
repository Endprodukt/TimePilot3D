/* city.h - what stands on the ground: every era has its own buildings.
 *
 * Nothing is stored. The ground is cut into cells; whether a cell holds a
 * building, which one and how big follows from the cell's place and the
 * era, the same every time it is asked. So the land goes on for ever and
 * is the same when the plane comes back.
 */
#ifndef TP3D_CITY_H
#define TP3D_CITY_H

#include <vectorpie/vp.h>

/* Once, after world_init. */
void city_init(void);

/* The buildings round the eye that the view can see, in the current
 * colour. `time` (seconds) turns what turns; `grow` 0..1 is how far they
 * have risen out of the ground (1 = standing). */
void city_draw(int era, vp_vec3 eye, float time, float grow);

/* How far from the eye buildings are drawn, in metres (at most 920). */
void city_set_reach(float reach);

/* Is a ball of this radius at `p` in a building? */
int city_hit(int era, vp_vec3 p, float radius);

/* How high the building is that stands within `margin` metres of (x, z);
 * 0 where there is none. For what has to fly over. */
float city_top(int era, float x, float z, float margin);

/* --- special places ------------------------------------------------------------
 *
 * A few cells hold something other than a house:
 *   a GATE   two arches to fly through: whoever does gets an item;
 *   a DEPOT  a store to shoot to pieces: an item comes out of it
 *            (which item is the game's to say, and is chance);
 *   a GUN    an emplacement that shoots at what flies past, and can be shot.
 * The game says what has become of one (city_mark); that is forgotten when
 * the era changes (city_reset). */
enum { CITY_NONE = 0, CITY_GATE, CITY_DEPOT, CITY_GUN };
#define GATE_MIDDLE 20.0f       /* metres up: the middle of a gate's way through (its city_site `at`) */
#define GATE_TAKE   22.0f       /* this near that middle, flying through, the item is won */
enum { CITY_SPENT = 1, CITY_GONE = 2 };     /* a gate flown through; a depot or gun destroyed */

typedef struct city_site {
    int     kind;           /* CITY_ */
    int     cx, cz;         /* its cell: its name for city_mark */
    int     spent;          /* a gate already flown through */
    vp_vec3 at;             /* a gate: the middle of the way through; else the top of it */
} city_site;

/* The special places within `reach` of the eye; gives back how many (at most `max`). */
int city_sites(int era, vp_vec3 eye, float reach, city_site *out, int max);

/* The special place whose building is within `radius` of p; 0 if none. */
int city_site_at(int era, vp_vec3 p, float radius, city_site *out);

/* One special place (CITY_ kind) by itself, its foot at the origin of
 * `at`, in its own colour: for the pages that explain it. */
void city_draw_site(int kind, const vp_xf *at);

void city_mark(int cx, int cz, int how);
void city_reset(void);

#endif /* TP3D_CITY_H */
