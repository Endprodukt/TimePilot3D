/* controls.c - the yoke read directly through SDL, see controls.h. */
#include <math.h>
#include "controls.h"
#include "vpbind.h"

#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BUTTONS 32
#define PREFIX      "yoke_button"

static SDL_Joystick *s_yoke;
static unsigned char s_now[MAX_BUTTONS], s_was[MAX_BUTTONS];
static int           s_buttons;
static unsigned      s_steps;

static int says_yoke(const char *name)
{
    char low[128];
    size_t i;
    if (!name) return 0;
    for (i = 0; name[i] && i + 1 < sizeof low; i++) low[i] = (char)tolower((unsigned char)name[i]);
    low[i] = 0;
    return strstr(low, "yoke") != NULL;
}

/* Look for the yoke now and then: it may be plugged in later. vp_input
 * has the joystick subsystem running and its events pumped. */
static void find_yoke(void)
{
    int i, n = SDL_NumJoysticks();
    for (i = 0; i < n; i++) {
        if (!says_yoke(SDL_JoystickNameForIndex(i))) continue;
        s_yoke = SDL_JoystickOpen(i);
        if (s_yoke) {
            s_buttons = SDL_JoystickNumButtons(s_yoke);
            if (s_buttons > MAX_BUTTONS) s_buttons = MAX_BUTTONS;
            return;
        }
    }
}

void controls_update(void)
{
    int i;

    if (s_yoke && !SDL_JoystickGetAttached(s_yoke)) {
        SDL_JoystickClose(s_yoke);
        s_yoke = NULL;
        s_buttons = 0;
    }
    if (!s_yoke && s_steps++ % 120 == 0) find_yoke();
    memcpy(s_was, s_now, sizeof s_was);
    memset(s_now, 0, sizeof s_now);
    if (!s_yoke) return;
    for (i = 0; i < s_buttons; i++) s_now[i] = SDL_JoystickGetButton(s_yoke, i) ? 1 : 0;
}

int controls_yoke_present(void) { return s_yoke != NULL; }

static int s_use_yoke = 1;

void controls_use_yoke(int on) { s_use_yoke = on; }

void controls_menu(vp_input *in, float *x, float *y, int *accept)
{
    *x = (float)vpb_menu_way_x();       /* the arrows, any stick, hat or yoke */
    *y = (float)vpb_menu_way_y();
    *accept = vpb_menu_confirm(in);     /* START 1, Enter, the keypad's Enter */
}

int controls_held(vp_input *in, const char *name)    { (void)in; return vpb_held(name); }
int controls_pressed(vp_input *in, const char *name) { (void)in; return vpb_pressed(name); }

void controls_axes(vp_input *in, float *x, float *y)
{
    if (s_yoke && s_use_yoke) {         /* its whole travel, and nothing else */
        *x = (float)SDL_JoystickGetAxis(s_yoke, 0) / 32767.0f;
        *y = (float)SDL_JoystickGetAxis(s_yoke, 1) / 32767.0f;
    } else {                            /* a stick, read directly; its switches, a hat or the arrows */
        int stick = vpb_stick(), wx = vpb_menu_way_x(), wy = vpb_menu_way_y();
        (void)in;
        *x = stick >= 0 ? vpb_joy_axis(stick, 0) : 0.0f;
        *y = stick >= 0 ? vpb_joy_axis(stick, 1) : 0.0f;
        if (wx) *x = (float)wx;
        if (wy) *y = (float)wy;
    }
    if (*x < -1) *x = -1;
    if (*x > 1) *x = 1;
    if (*y < -1) *y = -1;
    if (*y > 1) *y = 1;
}

int controls_captured(char *name, size_t size)
{
    int i;
    for (i = 0; i < s_buttons; i++) {
        if (s_now[i] && !s_was[i]) {
            snprintf(name, size, PREFIX "%d", i + 1);
            return 1;
        }
    }
    return 0;
}

int controls_any_yoke_button(void)
{
    int i;
    for (i = 0; i < s_buttons; i++) if (s_now[i]) return 1;
    return 0;
}
