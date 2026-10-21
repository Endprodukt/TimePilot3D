/* setup.c - the first start: how the cabinet is flown, laid out once (setup.h).
 *
 * Nothing is known yet but START: every page goes on with START 1 (or
 * Enter), the pages after the buttons are known also with fire. The yoke,
 * the stick and the arrows choose and set, as in the service menu.
 */
#include "setup.h"
#include "controls.h"
#include "vpbind.h"
#include "sound.h"
#include "world.h"

#include <stdio.h>
#include <string.h>

enum { P_CHOOSE, P_YOKE_BUTTONS, P_YOKE_LAYOUT, P_STICK_BUTTONS, P_STICK_LAYOUT };

#define SENS_MIN       25               /* hundredths, as in the service menu */
#define SENS_MAX       300
#define REPEAT_DELAY   24               /* a held direction: frames before it repeats, and between */
#define REPEAT_EVERY   4

/* The yoke's places, in the order they are asked for (service_settings'
 * yoke_layout), and what the recommended layout puts on each. */
static const char *const place_names[4] = {
    "FRONT RIGHT YOKE TRIGGER", "FRONT LEFT YOKE TRIGGER", "RIGHT THUMB BUTTON", "LEFT THUMB BUTTON"
};
static const char *const place_does[4] = { "FIRE", "LOOK BACK", "FASTER", "SLOWER" };

/* The stick's buttons, in the order they are asked for. */
static const int stick_order[SERVICE_BUTTONS] = { SERVICE_FIRE, SERVICE_REAR, SERVICE_FASTER, SERVICE_SLOWER };
static const char *const button_does[SERVICE_BUTTONS] = { "FIRE", "FASTER", "SLOWER", "LOOK BACK" };   /* by SERVICE_ */

static struct {
    service_settings now;
    int   page, choice, step;
    int   armed;                        /* what brought the page has been let go */
    int   got[4];                       /* the yoke's buttons pressed so far */
    int   sens;                         /* hundredths */
    int   held_h;
    vpb_layout lay;
    unsigned frames;
    int   cheat;                        /* opened by setup_cheat_page: drawn as if a yoke were there */
} S;

/* On to a page: whatever is held now must be let go before it counts there. */
static void turn_to(int page)
{
    S.page = page;
    S.step = 0;
    S.armed = 0;
    S.held_h = 0;
    if (page == P_STICK_BUTTONS) {
        vpb_layout_moved(&S.lay);
        vpb_layout_begin(&S.lay, S.now.button[stick_order[0]]);
    }
}

void setup_open(const service_settings *now)
{
    memset(&S, 0, sizeof S);
    S.now = *now;
    S.now.reset_scores = 0;
    S.sens = (int)(now->yoke_sensitivity * 100.0f + 0.5f);
    if (S.sens < SENS_MIN) S.sens = SENS_MIN;
    if (S.sens > SENS_MAX) S.sens = SENS_MAX;
    S.choice = controls_yoke_present() ? 0 : now->control_stick;
    turn_to(P_CHOOSE);
}

const service_settings *setup_get(void)
{
    S.now.yoke_sensitivity = (float)S.sens / 100.0f;
    return &S.now;
}

/* Every button at its standard for the kind of control chosen. */
static void standard_buttons(void)
{
    int b;
    for (b = 0; b < SERVICE_BUTTONS; b++)
        snprintf(S.now.button[b], sizeof S.now.button[b], "%s", service_standard(&S.now, b));
}

/* START 1 or Enter; once the buttons are known, fire as well. */
static int go_on(vp_input *in, int fire_too)
{
    int held = vpb_menu_confirm_held(in) || (fire_too && vpb_held(S.now.button[SERVICE_FIRE]));
    if (!S.armed) { S.armed = !held; return 0; }
    return vpb_menu_confirm(in) || (fire_too && vpb_pressed(S.now.button[SERVICE_FIRE]));
}

/* Without a yoke there is nothing to choose: only the joystick can be
 * taken (2026-10-21: the cursor could go up to YOKE, and then nothing
 * could be done with it). */
static int yoke_there(void) { return controls_yoke_present() || S.cheat; }

static void choose(vp_input *in)
{
    int way = vpb_menu_up() ? -1 : vpb_menu_down() ? 1 : 0;
    if (!yoke_there()) S.choice = 1;
    else if (way && S.choice != (way > 0)) { S.choice = way > 0; sound_fx(FX_TINK); }
    if (!go_on(in, 0)) return;
    sound_fx(FX_PICKUP);
    S.now.control_stick = S.choice;
    standard_buttons();
    turn_to(S.choice ? P_STICK_BUTTONS : P_YOKE_BUTTONS);
}

/* The yoke's four places, pressed one after the other; a button that has
 * its place already does not count again. START goes back one. */
static void yoke_buttons(vp_input *in)
{
    char name[32];
    int number = 0, i;

    if (vpb_menu_confirm(in)) {
        sound_fx(FX_TINK);
        if (S.step == 0) turn_to(P_CHOOSE); else S.step--;
        return;
    }
    if (!S.armed) { S.armed = !controls_any_yoke_button(); return; }
    if (!controls_captured(name, sizeof name) || sscanf(name, "yoke_button%d", &number) != 1) return;
    for (i = 0; i < S.step; i++) if (S.got[i] == number) return;
    S.got[S.step] = number;
    S.armed = 0;
    if (++S.step < 4) { sound_fx(FX_TINK); return; }
    for (i = 0; i < 4; i++) S.now.yoke_layout[i] = S.got[i];
    standard_buttons();                 /* fire on the right trigger ... (service_standard) */
    sound_fx(FX_PICKUP);
    turn_to(P_YOKE_LAYOUT);
}

/* The stick's four, each laid out as in the service menu (vpbind.h): the
 * button pressed, or START 1 to keep the standard. */
static void stick_buttons(vp_input *in)
{
    int b = stick_order[S.step];
    int got = vpb_layout_step(&S.lay, in, S.now.button[b], sizeof S.now.button[b], service_standard(&S.now, b));

    if (got == VPB_WAITING) return;
    if (++S.step < SERVICE_BUTTONS) {
        sound_fx(FX_TINK);
        vpb_layout_moved(&S.lay);       /* the next replaces what that one has */
        vpb_layout_begin(&S.lay, S.now.button[stick_order[S.step]]);
        return;
    }
    sound_fx(FX_PICKUP);
    turn_to(P_STICK_LAYOUT);
}

int setup_update(vp_input *in)
{
    S.frames++;
    switch (S.page) {
    case P_CHOOSE:        choose(in); break;
    case P_YOKE_BUTTONS:  yoke_buttons(in); break;
    case P_STICK_BUTTONS: stick_buttons(in); break;
    case P_YOKE_LAYOUT:
    case P_STICK_LAYOUT:
        if (go_on(in, 1)) { sound_fx(FX_PICKUP); return SERVICE_SAVED; }
        break;
    }
    return SERVICE_OPEN;
}

void setup_cheat_page(int page)
{
    static const int pages[] = { P_CHOOSE, P_YOKE_BUTTONS, P_YOKE_LAYOUT, P_STICK_BUTTONS, P_STICK_LAYOUT };
    if (page < 0 || page > 4) return;
    S.now.control_stick = page >= 3;
    standard_buttons();
    turn_to(pages[page]);
    S.cheat = 1;
    if (page == 1) S.step = 1, S.got[0] = 1;     /* one place taken already: how it looks */
}

/* --- drawing (board units, y up) ---------------------------------------------- */

#define CX 2048.0f

static void text(const char *s, float x, float y, float height, int align, int r, int g, int b)
{
    world_rgb(r, g, b);
    world_text(s, x, y, height, align);
}

static void rect(float x0, float y0, float x1, float y1)
{
    world_bline(x0, y0, x1, y0); world_bline(x1, y0, x1, y1);
    world_bline(x1, y1, x0, y1); world_bline(x0, y1, x0, y0);
}

static int blink(void) { return (S.frames / 15) & 1; }

/* The yoke as the pilot sees it, centred on (cx, cy): the box in the
 * middle, a grip on each side, each grip's thumb button on top and its
 * trigger on the front, round the grip's inner side. `ask` is the place
 * asked for now (blinks), `done` how many have been pressed (green, with
 * the number each reported); `words` names each place's function. */
static void draw_yoke(float cx, float cy, int ask, int done, const char *const words[4])
{
    int k;

    world_rgb(150, 170, 150);
    rect(cx - 260, cy - 220, cx + 260, cy + 160);           /* the box */
    world_bline(cx, cy - 220, cx, cy - 520);                 /* the column */
    for (k = -1; k <= 1; k += 2) {
        float s = (float)k, gx = cx + 820.0f * s;
        int trigger = k > 0 ? 0 : 1, thumb = k > 0 ? 2 : 3, p;
        world_rgb(150, 170, 150);
        world_bline(cx + 260.0f * s, cy, gx - 110.0f * s, cy);         /* the arm */
        rect(gx - 110, cy - 450, gx + 110, cy + 260);                   /* the grip */
        for (p = 0; p < 2; p++) {
            int place = p ? thumb : trigger;
            int lit = place == ask && blink(), taken = place < done;
            float lx = gx, ly = p ? cy + 520.0f : cy - 640.0f;
            if (lit) world_rgb(255, 230, 0); else if (taken) world_rgb(0, 255, 0); else world_rgb(150, 170, 150);
            if (p) world_bcircle(gx, cy + 320.0f, 60.0f, 12);                         /* the thumb's button, on top */
            else rect(gx - 110.0f * s - 70.0f * s, cy - 40, gx - 110.0f * s, cy + 140);  /* the trigger, in front */
            if (place == ask) text(blink() ? "PRESS" : "", lx, ly, 100, VP_TEXT_CENTER, 255, 230, 0);
            else if (taken && !words) {
                char n[16];
                snprintf(n, sizeof n, "%d", S.got[place]);
                text(n, lx, ly, 100, VP_TEXT_CENTER, 0, 255, 0);
            }
            if (words) text(words[place], lx, ly, 100, VP_TEXT_CENTER, 255, 255, 255);
        }
    }
}

static void help(const char *one, const char *two)
{
    if (one) text(one, CX, 300, 80, VP_TEXT_CENTER, 0, 150, 0);
    if (two) text(two, CX, 170, 80, VP_TEXT_CENTER, 0, 150, 0);
}

void setup_draw(void)
{
    char buf[128];
    int i;

    world_fade(1.0f);
    world_level(1.0f);
    text("SETUP", CX, 3700, 170, VP_TEXT_CENTER, 255, 255, 0);

    switch (S.page) {
    case P_CHOOSE:
        text("HOW DO YOU CONTROL THE GAME?", CX, 3000, 130, VP_TEXT_CENTER, 255, 255, 255);
        for (i = 0; i < 2; i++) {
            float y = 2300.0f - 450.0f * (float)i;
            int here = S.choice == i, off = i == 0 && !yoke_there();
            if (here) text(">", 900, y, 150, VP_TEXT_RIGHT, 255, 255, 0);
            if (off) {                      /* nothing to take */
                text("YOKE", 1000, y, 150, VP_TEXT_LEFT, 50, 50, 70);
                text("NOT CONNECTED", 1700, y + 20, 80, VP_TEXT_LEFT, 120, 60, 60);
            } else {
                text(i ? "JOYSTICK AND BUTTONS" : "YOKE", 1000, y, 150, VP_TEXT_LEFT, here ? 255 : 60, here ? 255 : 110, 255);
            }
        }
        text(controls_yoke_present() ? "A YOKE IS CONNECTED" : "NO YOKE IS CONNECTED", CX, 1100, 90, VP_TEXT_CENTER, 0, 150, 150);
        help("UP, DOWN - CHOOSE", "START - TAKE");
        break;

    case P_YOKE_BUTTONS:
        if (!controls_yoke_present() && !S.cheat) {
            text("NO YOKE IS CONNECTED", CX, 2800, 130, VP_TEXT_CENTER, 255, 80, 80);
            text("PLUG IT IN, OR GO BACK", CX, 2500, 100, VP_TEXT_CENTER, 255, 255, 255);
            help(NULL, "START - BACK");
            break;
        }
        text("PLEASE PRESS THE", CX, 3150, 110, VP_TEXT_CENTER, 255, 255, 255);
        text(place_names[S.step], CX, 2850, 150, VP_TEXT_CENTER, 255, 230, 0);
        snprintf(buf, sizeof buf, "%d OF 4", S.step + 1);
        text(buf, CX, 2600, 90, VP_TEXT_CENTER, 0, 150, 150);
        draw_yoke(CX, 1450, S.step, S.step, NULL);
        help("PRESS THE BUTTON THAT IS ASKED FOR", "START - BACK");
        break;

    case P_YOKE_LAYOUT:
        text("THE RECOMMENDED LAYOUT IS:", CX, 3150, 120, VP_TEXT_CENTER, 255, 255, 255);
        for (i = 0; i < 4; i++) {
            float y = 2850.0f - 170.0f * (float)i;
            text(place_names[i], 2550, y, 80, VP_TEXT_RIGHT, 60, 110, 255);
            text(place_does[i], 2700, y, 80, VP_TEXT_LEFT, 255, 255, 255);
        }
        draw_yoke(CX, 1450, -1, 0, place_does);
        help("CHANGE IT IN THE SERVICE MENU (F2), CONTROLS", "FIRE OR START - PLAY");
        break;

    case P_STICK_BUTTONS:
    case P_STICK_LAYOUT: {
        const char *said = vpb_layout_said(&S.lay);
        if (S.page == P_STICK_BUTTONS) {
            text("PLEASE PRESS THE BUTTON FOR", CX, 3150, 110, VP_TEXT_CENTER, 255, 255, 255);
            text(button_does[stick_order[S.step]], CX, 2850, 150, VP_TEXT_CENTER, 255, 230, 0);
        } else {
            text("YOUR LAYOUT IS:", CX, 3150, 120, VP_TEXT_CENTER, 255, 255, 255);
        }
        for (i = 0; i < SERVICE_BUTTONS; i++) {
            int b = stick_order[i], here = S.page == P_STICK_BUTTONS && i == S.step;
            float y = 2400.0f - 250.0f * (float)i, size = 100.0f, room = 3900.0f - 1700.0f;
            char all[256];
            text(button_does[b], 1500, y, 100, VP_TEXT_RIGHT, here ? 255 : 60, here ? 255 : 110, 255);
            if (here) {
                if (blink()) text("PRESS A BUTTON", 1700, y, 100, VP_TEXT_LEFT, 255, 255, 0);
                continue;
            }
            if (S.page == P_STICK_BUTTONS && i > S.step) continue;      /* not asked yet */
            vpb_spelled(S.now.button[b], all, sizeof all);
            if ((float)strlen(all) * size * 1.05f > room) size = room / ((float)strlen(all) * 1.05f);
            text(all, 1700, y, size, VP_TEXT_LEFT, 255, 255, 255);
        }
        if (said) text(said, CX, 420, 80, VP_TEXT_CENTER, 255, 80, 80);
        if (S.page == P_STICK_BUTTONS) help("START 1 - KEEP THE STANDARD   START 2 - NONE", "THE STICK MOVES THE PLANE: ONLY BUTTONS HERE");
        else help("NEXT: A TEST FLIGHT TO SET THE STICK'S SENSITIVITY", "FIRE OR START - FLY");
        break;
    }
    }
}
