/* controls.h - the yoke, the stick and the buttons, read as they are.
 *
 * Nothing goes by advmame.rc's names: a function's buttons are vpbind.h's
 * (keys, any joystick's buttons, the yoke's as "yoke_button1" ..., the
 * mouse's), the yoke's whole travel and a stick's axes are read directly.
 */
#ifndef TP3D_CONTROLS_H
#define TP3D_CONTROLS_H

#include <vectorpie/vp.h>

/* Once every game step, before anything else asks. */
void controls_update(void);

/* Is a yoke connected, and read directly? */
int controls_yoke_present(void);

/* A function's controls, vpbind.h's text: any of them held / down now. */
int controls_held(vp_input *in, const char *name);
int controls_pressed(vp_input *in, const char *name);      /* down since the last step */

/* The yoke, -1..1 each way: left and forward negative. No dead zone of the
 * game's is applied here. Without a yoke: the analog stick, with the keys
 * and the stick's directions standing in. */
void controls_axes(vp_input *in, float *x, float *y);

/* The game is flown with the yoke (1, the default) or with a stick and
 * buttons (0): then the yoke's axes are not read even when there is one,
 * and controls_axes gives what the stick's switches say. */
void controls_use_yoke(int on);

/* For menus, whatever is laid out: which way is asked for (-1, 0, 1 each
 * way, left and up negative) by the arrows, any stick, hat or the yoke, and
 * whether START 1, Enter or the keypad's Enter has just said yes. */
void controls_menu(vp_input *in, float *x, float *y, int *accept);

/* For laying out the controls: the yoke button that has just been pressed,
 * by its name into `name`; 0 if none. And whether any is held. */
int controls_captured(char *name, size_t size);
int controls_any_yoke_button(void);

#endif /* TP3D_CONTROLS_H */
