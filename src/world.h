/* world.h - the view, the brightness, the sky and the ground.
 *
 * World space is the SDK's: x right (east), y up, z forward (north), in
 * metres, left-handed. A heading of 0 flies along +z, a positive heading
 * turns right. Everything here draws through the SDK's 3D view in the
 * colour set by world_rgb, which the brightness of world_level dims.
 */
#ifndef TP3D_WORLD_H
#define TP3D_WORLD_H

#include <vectorpie/vp.h>

#define GROUND_Y      0.0f
#define WORLD_FAR     2600.0f       /* nothing beyond is drawn */

/* Once. `vertical` = a 3:4 monitor. */
void world_init(int vertical);
int  world_vertical(void);

/* Each frame, before drawing. */
void world_begin(vp_dvg *d);

/* How hard the last frames were on the beam: the way it had to go, lit
 * and dark together, against what the board draws in a frame without
 * falling behind. 1 is the limit; past it the board holds the program up
 * and the picture stutters, so whatever is only for show is cut back as
 * this comes near 1. (Measured on the cabinet's USB-DVG at 40 frames a
 * second: it blocks from about 300,000 board units a frame.) */
float world_load(void);

/* The beam travel a frame that counts as full load (world_load 1), board
 * units: the service menu's BEAM LIMIT. Lower, and the game thins out its
 * effects, ground and sky sooner. */
void world_set_load_limit(float units);
vp_dvg *world_dvg(void);
vp3d_view *world_view(void);

/* The camera: at `pos`, turned by heading (about y, right positive), pitch
 * (up positive) and roll (radians). */
void world_pose(vp_vec3 pos, float heading, float pitch, float roll);

/* A placement for a model flown the same way. */
vp_xf world_place(vp_vec3 pos, float heading, float pitch, float roll);

/* The direction a heading and pitch point along. */
vp_vec3 world_forward(float heading, float pitch);

/* How bright what is drawn from now on is, 0..1. */
void world_level(float level);
void world_rgb(int r, int g, int b);

/* Everything dimmed by this on top, 0..1, until told otherwise: a scene
 * fading out or in. Unlike world_level it goes all the way to black. */
void world_fade(float fade);

/* Lines and circles in board units (0..4095, y up), current colour. A
 * circle is made round on the tube: its height follows the screen's shape. */
void world_bline(float x0, float y0, float x1, float y1);
void world_bcircle(float cx, float cy, float r, int n);

/* The way through time is a tunnel that snakes. Where its middle is,
 * `s` metres along it, in metres to the side and up; `amp` makes the
 * bends wider (1 = as designed). */
#define TUNNEL_RADIUS 110.0f        /* to the corners of its rings */
#define TUNNEL_STEP   60.0f         /* metres from ring to ring */
void world_tunnel_centre(float s, float amp, float *x, float *y);

/* The tunnel as seen from `dist` metres into it, by an eye (eye_x, eye_y)
 * metres from the line its middle bends about: rings coming at the
 * viewer, each turned a little further, coloured from `from` to `to` as
 * `mix` goes from 0 to 1 (each red, green, blue 0..255). Gives back where
 * its far end is on the board.
 * With `target` over 0 the next three rings from `ahead` metres on carry
 * a small green ring of that radius in their middle: where to fly. */
void world_draw_tunnel(float dist, float eye_x, float eye_y, float amp, float bright,
                       const float from[3], const float to[3], float mix, float *far_x, float *far_y,
                       float target, float ahead);

/* A clock face, its hands `turns` turns of the hour hand on. */
void world_draw_clock(float cx, float cy, float r, float turns);

/* Drawing, in the current colour, dimmed by fog. */
void world_mesh(const vp3d_mesh *m, const vp_xf *place);
void world_line(vp_vec3 a, vp_vec3 b);

/* Solid buildings (the setting `occlusion`): with it on, the 3D lines of
 * a frame are kept and cut where a face that is nearer covers them; the
 * faces are given with world_face (corners in order, a flat convex face,
 * up to 8). world_flush draws what is kept: before the clip of the board
 * changes, and at the end of the frame. */
void world_set_solid(int on);
int  world_solid(void);
void world_face(const vp_vec3 *corners, int count);
/* The faces given after this are one thing's (it makes the cutting fast). */
void world_group(void);
/* While on, lines are drawn at once and never cut: what must always be
 * seen, as the marks on a boss's weak points. */
void world_set_nocut(int on);
/* While on, faces given are the cockpit's: they hide even what is never
 * cut (world_set_nocut), and lines drawn now are never cut by anything. */
void world_set_front(int on);
void world_flush(void);
/* Is this point behind a face given so far this frame? For what is put on
 * the board at a place in the world (a letter, a number) and so is not cut. */
int  world_hidden(vp_vec3 p);

/* A short line that stands alone: a spark, a shot, a piece of something
 * burst. These are not drawn at once but kept, and world_dashes draws
 * them all in the order that takes the beam the shortest way from each to
 * the next. Scattered over the screen in the order they happen to be
 * stored, their dark jumps cost the beam more than everything they light. */
void world_dash(vp_vec3 a, vp_vec3 b);
void world_dashes(void);
int  world_project(vp_vec3 p, float *sx, float *sy);   /* board units; 0 if behind */

/* The other way: `n` points given in board units (x, y pairs), as world
 * points `depth` metres in front of the camera that land just there. For
 * what sits in front of the eye and moves with it (the cockpit). */
void world_board_points(const float *board, int n, float depth, vp_vec3 *out);

/* The ground: a grid round the eye, and the horizon. `coarse` draws every
 * second line only, for when the beam has too much to do. */
void world_draw_ground(vp_vec3 eye, float heading, int coarse);

/* The clouds that float past, the same ones for ever (they repeat). */
void world_draw_clouds(vp_vec3 eye);

/* 2001: rocks drifting where the clouds were, turning slowly. */
void world_draw_asteroids(vp_vec3 eye, float time);

/* The time warp: streaks from the middle of the screen, `t` seconds in. */
void world_draw_warp(float t, unsigned seed);

/* Text in board units (0..4095, y up), current colour, the arcade font. */
void world_text(const char *s, float x, float y, float height, int align);

#endif /* TP3D_WORLD_H */
