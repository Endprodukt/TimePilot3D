/* models.h - the game's shapes, built in code as wireframe meshes.
 *
 * Model space: +x right, +y up, +z forward (the way the thing flies), in
 * metres. Each is drawn as a wireframe (every edge, see-through), the look
 * of the vector arcade games, so no face winding has to be right.
 */
#ifndef TP3D_MODELS_H
#define TP3D_MODELS_H

#include <vectorpie/vp.h>

enum {
    MODEL_PLAYER = 0,   /* the player's fighter */
    MODEL_BIPLANE,      /* 1910 */
    MODEL_MONOPLANE,    /* 1940 */
    MODEL_HELICOPTER,   /* 1970, without its rotors */
    MODEL_JET,          /* 1982 */
    MODEL_UFO,          /* 2001 */
    MODEL_BLIMP,        /* the 1910 boss */
    MODEL_BOMBER,       /* the 1940 boss */
    MODEL_BIGHELI,      /* the 1970 boss, without its rotors */
    MODEL_B52,          /* the 1982 boss */
    MODEL_MOTHERSHIP,   /* the 2001 boss */
    MODEL_ROTOR,        /* two blades, turned by the game */
    MODEL_PARACHUTE,    /* a parachutist */
    MODEL_MISSILE,
    MODEL_ROBOT,        /* what the 2001 boss turns into */
    MODEL_GUNSHIP,      /* the 1970 boss's escort, without its rotor */
    MODEL_COUNT
};

/* Every model has up to this many colours of its own. */
#define MODEL_COLOURS 4

/* How a model is coloured when it is drawn. */
enum {
    TINT_OWN = 0,   /* its own colours */
    TINT_WHITE,     /* all white: it has just been hit */
    TINT_FIRE,      /* burning */
    TINT_HEAVY,     /* red and yellow: one of the big ones that cross every era */
    TINT_ESCORT     /* ice blue and white: a boss's escort, not one of the swarm */
};

/* Build them all once. Returns VP_OK. */
int models_init(void);

/* Draw a model where `place` puts it, in its colours (dimmed by the
 * world's brightness and fog as everything is). */
void model_draw(int which, const vp_xf *place, int tint);

/* The radius of the smallest sphere round the model, for collisions. */
float model_radius(int which);

/* The model's lines as plain end points, six floats each (x, y, z of the
 * one end, then of the other): gives back how many. For a wreck drawn
 * with a part missing, and for the pieces a burst throws about. */
int model_segments(int which, const float **segments);

/* Which of the model's colours one of those lines has, and what that
 * colour is (red, green, blue 0..255) under a tint. */
int  model_segment_colour(int which, int segment);
void model_colour(int which, int slot, int tint, int *r, int *g, int *b);

/* The robot is made of limbs that move on their own: which one a line of
 * MODEL_ROBOT belongs to. */
enum { LIMB_BODY, LIMB_ARM_R, LIMB_ARM_L, LIMB_LEG_R, LIMB_LEG_L };
int model_robot_limb(int segment);

/* A model's faces (for solid drawing): how many; one of them, its
 * corners (up to 8, in the model's units) and its tag (the robot's limb);
 * and all of them given to world_face where `place` puts the model. */
int  model_face_count(int which);
int  model_face(int which, int i, const float (**corners)[3], int *tag);
void model_faces(int which, const vp_xf *place);

/* How far the model reaches to either side of its middle (x). */
float model_halfspan(int which);

#endif /* TP3D_MODELS_H */
