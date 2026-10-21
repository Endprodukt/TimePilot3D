/* setup.h - the first start: how the cabinet is flown, laid out once.
 *
 * When the game has never been set up (save/settings.ini has no
 * setup_done = yes) it begins here instead of on its title: it asks whether
 * the cabinet has a yoke or a joystick and buttons, and walks through
 * laying them out. With the yoke: the four buttons pressed one after the
 * other (where they are), then the recommended layout. With a joystick:
 * the four buttons, then a test flight in which the buttons set the
 * stick's sensitivity (game.c, start_practice). Without a yoke only the
 * joystick can be taken. The service menu (CONTROLS, CONTROL SETUP)
 * begins it again.
 */
#ifndef TP3D_SETUP_H
#define TP3D_SETUP_H

#include <vectorpie/vp.h>
#include "service.h"

/* Begin, from its first question, on these settings. */
void setup_open(const service_settings *now);

/* One step: SERVICE_OPEN while it goes on, SERVICE_SAVED when it is done
 * and setup_get has the settings to take over. */
int setup_update(vp_input *in);

const service_settings *setup_get(void);

/* The frame must have begun (world_begin). */
void setup_draw(void);

/* For testing: straight to a page (1 the yoke's buttons, 2 its layout,
 * 3 the stick's buttons, 4 their layout). */
void setup_cheat_page(int page);

#endif /* TP3D_SETUP_H */
