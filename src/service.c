/* service.c - the service menu: game.ini's settings, changed on the machine.
 *
 * Worked with what the cabinet has, after the Tempest 2000 port's menu.
 * The yoke (or up / down) goes through the rows; fire (or START 1) takes a
 * row, the yoke (or left / right) then sets it, fire lets it go again.
 * Left / right set a row without taking it first. Nothing is taken over
 * until SAVE. The game's rules, the controls and the picture each have a
 * page of their own.
 */
#include "service.h"
#include "controls.h"
#include "vpbind.h"
#include "sound.h"
#include "world.h"

#include <SDL.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum {
    R_VOLUME, R_EFFECTS, R_ENGINE, R_MUSIC,
    R_LIVES, R_DIFFICULTY, R_BONUS, R_KILLS, R_START, R_FREE, R_ATTRACT, R_RESET, R_LEVELSEL, R_INVINCIBLE, R_CONTINUE,
    R_SENS, R_DEADZONE, R_INVERT, R_VIEW, R_AIM, R_FIRE, R_FASTER, R_SLOWER, R_REAR, R_MENUKEY, R_SETUP, R_FACTORY,
    R_SOLID, R_PICTURE, R_BEAM, R_MONITOR, R_LAYOUT, R_CONTROL, R_SAVE, R_CANCEL, R_GAME, R_CONTROLS, R_VIDEO, R_BACK,
    R_BRIGHT, R_BRIGHT_LAST = R_BRIGHT + SERVICE_BRIGHTS - 1, ROWS
};

enum { PAGE_MAIN, PAGE_GAME, PAGE_CONTROLS, PAGE_VIDEO, PAGE_MONITOR };
static const int main_rows[] = {
    R_GAME, R_CONTROLS, R_VIDEO, R_MONITOR, R_VOLUME, R_EFFECTS, R_ENGINE, R_MUSIC, R_FACTORY, R_CANCEL, R_SAVE
};
static const int monitor_rows[] = { R_PICTURE, R_BEAM, R_BACK };
static const int game_rows[] = {
    R_LIVES, R_DIFFICULTY, R_BONUS, R_KILLS, R_START, R_FREE, R_ATTRACT, R_RESET,
#if DEBUG_LEVEL_SELECT
    R_LEVELSEL,
#endif
    R_CONTINUE, R_INVINCIBLE, R_BACK
};
static const int control_rows[] = {
    R_CONTROL, R_SENS, R_DEADZONE, R_INVERT, R_VIEW, R_AIM, R_FIRE, R_FASTER, R_SLOWER, R_REAR, R_LAYOUT, R_MENUKEY, R_SETUP, R_BACK
};
static const int video_rows[] = {
    R_BRIGHT, R_BRIGHT + 1, R_BRIGHT + 2, R_BRIGHT + 3, R_BRIGHT + 4, R_BRIGHT + 5,
    R_BRIGHT + 6, R_BRIGHT + 7, R_SOLID, R_BACK
};
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

#define SENS_MIN       25               /* hundredths */
#define SENS_MAX       300
#define REPEAT_DELAY   24               /* a held control: frames before it repeats, and between */
#define REPEAT_EVERY   4
#define YOKE_ON        0.5f             /* how far the yoke must go to count as a press */

static const char *const row_names[ROWS] = {
    "MASTER VOLUME", "EFFECTS VOLUME", "ENGINE VOLUME", "MUSIC VOLUME",
    "PLANES PER GAME", "DIFFICULTY", "FIRST BONUS PLANE", "ENEMIES PER ERA", "START IN ERA", "FREE PLAY", "ATTRACT SOUND",
    "RESET HIGH SCORES", "LEVEL SELECT", "INVINCIBLE", "CONTINUE",
    "YOKE SENSITIVITY", "YOKE DEAD ZONE", "INVERT PITCH", "VIEW AT START", "AIM ASSIST",
    "FIRE BUTTON", "FASTER BUTTON", "SLOWER BUTTON", "LOOK BACK BUTTON", "MENU BUTTON", "CONTROL SETUP", "FACTORY RESET AND RESTART",
    "SOLID OBJECTS", "PICTURE SIZE", "BEAM LIMIT", "MONITOR", "WHERE THE BUTTONS ARE", "FLOWN WITH", "SAVE", "CANCEL", "GAME", "CONTROLS", "VIDEO", "BACK",
    "GROUND, BUILDINGS", "CLOUD BRIGHTNESS", "ENEMY BRIGHTNESS", "PLANE, COCKPIT",
    "SHOT BRIGHTNESS", "EFFECT BRIGHTNESS", "TEXT BRIGHTNESS", "TITLE BRIGHTNESS"
};

static const char *const difficulty_names[DIFFICULTY_COUNT] = { "EASY", "NORMAL", "HARD", "ACE" };
static const char *const view_names[VIEW_COUNT] = { "HUD ONLY", "COCKPIT", "CHASE" };

static struct {
    service_settings now;
    int   sens;                         /* hundredths */
    int   page, cursor, row;
    int   editing, capture_armed;
    vpb_layout lay;                     /* laying out a button (vpbind.h) */
    int   layout_step;                  /* asking where the yoke's buttons are: 0 not, 1..4 the place asked for */
    int   layout_new[4];
    int   held_v, held_h;               /* frames a direction has been held */
    int   preview;                      /* frames until a volume's row sounds again */
    int   engine;                       /* the engine is running for its row */
    int   music;                        /* and the music for its */
    int   tab;
    int   factory_armed;                /* FACTORY RESET taken once: the next START does it */
    unsigned frames;
} S;

static const int *page_rows(int *count)
{
    if (S.page == PAGE_GAME) { *count = COUNT(game_rows); return game_rows; }
    if (S.page == PAGE_CONTROLS) { *count = COUNT(control_rows); return control_rows; }
    if (S.page == PAGE_VIDEO) { *count = COUNT(video_rows); return video_rows; }
    if (S.page == PAGE_MONITOR) { *count = COUNT(monitor_rows); return monitor_rows; }
    *count = COUNT(main_rows);
    return main_rows;
}

/* What opens the menu: F2 (the cabinet's key for every game's menu, never
 * laid out - vpbind.c) always, so that nobody is locked out, and whatever
 * MENU BUTTON has besides. */
static char s_menu_key[128] = "key_F2";

void service_menu_key(const char *controls)
{
    snprintf(s_menu_key, sizeof s_menu_key, "%s", controls && controls[0] ? controls : "key_F2");
}

int service_hotkey(void)
{
    int tab, hit;

    vpb_update();
    tab = vpb_key_held(SDL_SCANCODE_F2) || vpb_held(s_menu_key);
    hit = tab && !S.tab;

    S.tab = tab;
    return hit;
}

void service_open(const service_settings *now)
{
    int tab = S.tab, i;

    memset(&S, 0, sizeof S);
    S.tab = tab;
    S.now = *now;
    S.now.reset_scores = 0;
    for (i = 0; i < SERVICE_BUTTONS; i++) vpb_normalize(S.now.button[i], sizeof S.now.button[i]);
    if (!S.now.menu_key[0]) snprintf(S.now.menu_key, sizeof S.now.menu_key, "key_F2");
    S.sens = (int)(now->yoke_sensitivity * 100.0f + 0.5f);
    if (S.sens < SENS_MIN) S.sens = SENS_MIN;
    if (S.sens > SENS_MAX) S.sens = SENS_MAX;
    S.row = main_rows[0];
}

const service_settings *service_get(void)
{
    S.now.yoke_sensitivity = (float)S.sens / 100.0f;
    return &S.now;
}

static void hear(void)
{
    sound_volumes((float)S.now.volume / 100.0f, (float)S.now.effects_volume / 100.0f,
                  (float)S.now.engine_volume / 100.0f, (float)S.now.music_volume / 100.0f);
}

/* A row that holds a number: where it is, its range, and how much one
 * step changes it. */
static int *row_number(int row, int *lo, int *hi, int *step)
{
    *lo = 0; *hi = 100; *step = 1;
    switch (row) {
    case R_SENS:       *lo = SENS_MIN; *hi = SENS_MAX; *step = 5; return &S.sens;
    case R_DEADZONE:   *hi = 50; return &S.now.yoke_deadzone;
    case R_AIM:        *step = 5; return &S.now.aim_assist;
    case R_VOLUME:     return &S.now.volume;
    case R_EFFECTS:    return &S.now.effects_volume;
    case R_ENGINE:     return &S.now.engine_volume;
    case R_MUSIC:      return &S.now.music_volume;
    case R_LIVES:      *lo = 1; *hi = 9; return &S.now.lives;
    case R_DIFFICULTY: *hi = DIFFICULTY_COUNT - 1; return &S.now.difficulty;
    case R_VIEW:       *hi = VIEW_COUNT - 1; return &S.now.view;
    case R_BONUS:      *lo = 5000; *hi = 100000; *step = 5000; return &S.now.bonus_plane;
    case R_KILLS:      *lo = 8; *hi = 96; *step = 8; return &S.now.kills_per_era;
    case R_START:      *hi = 4; return &S.now.start_era;
    case R_PICTURE:    *lo = 60; *hi = 100; *step = 2; return &S.now.picture_size;
    case R_BEAM:       *lo = 100; *hi = 250; *step = 10; return &S.now.beam_limit;
    }
    if (row >= R_BRIGHT && row <= R_BRIGHT_LAST) return &S.now.brightness[row - R_BRIGHT];
    return NULL;
}

static int *row_switch(int row)
{
    switch (row) {
    case R_ATTRACT: return &S.now.attract_sound;
    case R_FREE:    return &S.now.free_play;
    case R_INVERT:  return &S.now.invert_pitch;
    case R_SOLID:   return &S.now.occlusion;
    case R_CONTROL: return &S.now.control_stick;
    case R_RESET:   return &S.now.reset_scores;
    case R_LEVELSEL: return &S.now.level_select;
    case R_INVINCIBLE: return &S.now.invincible;
    case R_CONTINUE: return &S.now.continues;
    }
    return NULL;
}

static int is_button(int row) { return (row >= R_FIRE && row <= R_REAR) || row == R_MENUKEY; }
static int button_index(int row) { return row - R_FIRE; }
static const char *standard_of(int b);

/* A row that lays out controls: what it has, and its standard. */
static char *laid_of(int row, const char **standard)
{
    if (row == R_MENUKEY) { if (standard) *standard = "key_F2"; return S.now.menu_key; }
    if (standard) *standard = standard_of(button_index(row));
    return S.now.button[button_index(row)];
}

/* A button's standard: on the yoke where WHERE THE BUTTONS ARE says (fire
 * the right trigger, look back the left, faster and slower the thumbs); with
 * a stick buttons 1 to 4 of it and the keys AdvanceMAME gives those. */
const char *service_standard(const service_settings *v, int b)
{
    static const char *const stick[SERVICE_BUTTONS] = {
        "key_Left_Ctrl+joy1_button1", "key_Left_Alt+joy1_button2", "key_Space+joy1_button3", "key_Left_Shift+joy1_button4"
    };
    static const int place[SERVICE_BUTTONS] = { 0, 2, 3, 1 };      /* fire, faster, slower, rear */
    static char yoke[16];
    if (v->control_stick) return stick[b];
    snprintf(yoke, sizeof yoke, "yoke_button%d", v->yoke_layout[place[b]]);
    return yoke;
}

static const char *standard_of(int b) { return service_standard(&S.now, b); }

/* A number shown as a slider, not as its figure alone. */
static int is_slider(int row)
{
    return row == R_DEADZONE || row == R_AIM || row == R_PICTURE || row == R_BEAM || (row >= R_VOLUME && row <= R_MUSIC)
        || (row >= R_BRIGHT && row <= R_BRIGHT_LAST);
}

/* Change the row's value by `steps`. */
static void change(int row, int steps)
{
    int lo, hi, step, *number = row_number(row, &lo, &hi, &step), *onoff = row_switch(row);

    if (!steps) return;
    if (is_button(row)) {               /* none, or its standard */
        const char *standard;
        char *has = laid_of(row, &standard);
        vpb_layout_toggle(&S.lay, has, sizeof S.now.menu_key, standard);
        sound_fx(FX_TINK);
        return;
    }
    if (onoff) {
        *onoff = !*onoff;
        sound_fx(FX_TINK);
        if (row == R_CONTROL) {
            /* The other kind of control: its usual buttons go with it, where
             * the ones laid belong to the kind that was. A stick's are
             * buttons 1 to 4; a yoke's are its triggers and thumb buttons. */
            int b, of_yoke = 0;
            for (b = 0; b < SERVICE_BUTTONS; b++) if (strstr(S.now.button[b], "yoke_")) of_yoke++;
            if (S.now.control_stick == !!of_yoke)
                for (b = 0; b < SERVICE_BUTTONS; b++) snprintf(S.now.button[b], sizeof S.now.button[b], "%s", standard_of(b));
        }
        return;
    }
    if (!number) return;
    *number += steps * step;
    if (*number < lo) *number = lo;
    if (*number > hi) *number = hi;
    if (row >= R_VOLUME && row <= R_MUSIC) hear();
    else if (!is_slider(row)) sound_fx(FX_TINK);
}

/* The menu is silent but for what a volume's row is the volume of: the
 * engine on the engine's row, an explosion now and then on the effects',
 * both on the master's, the attract's music on the music's. */
static void sound_for_row(void)
{
    int engine = S.row == R_VOLUME || S.row == R_ENGINE, music = S.row == R_MUSIC;

    if (music != S.music) {
        S.music = music;
        sound_music(music ? MUSIC_ATTRACT : MUSIC_NONE);
    }

    if (engine != S.engine) {
        S.engine = engine;
        sound_engine(engine ? 3 : -1);
        if (engine) sound_engine_load(0.4f);
    }
    if (S.preview > 0) { S.preview--; return; }
    if (S.row == R_EFFECTS || S.row == R_VOLUME) {
        sound_fx(FX_EXPLODE);
        S.preview = 60;
    }
}

static void turn_to(int page)
{
    int count;

    vpb_layout_moved(&S.lay);
    S.page = page;
    S.cursor = 0;
    S.row = page_rows(&count)[0];
}

/* A direction held down: once, then again and again. */
static int repeated(int held)
{
    if (!held) return 0;
    if (held == 1) return 1;
    return held > REPEAT_DELAY && (held - REPEAT_DELAY) % REPEAT_EVERY == 0;
}

/* Where the yoke's buttons are: asked one place after the other - right
 * trigger, left trigger, right thumb, left thumb - and the button pressed
 * for each is that place's. All four, or none: START leaves it as it was. */
static const char *const place_names[4] = { "RIGHT TRIGGER", "LEFT TRIGGER", "RIGHT THUMB", "LEFT THUMB" };

static void capture_layout(vp_input *in)
{
    char name[32];
    int number = 0, i;

    if (vpb_menu_confirm(in)) { S.layout_step = 0; return; }
    if (!S.capture_armed) { S.capture_armed = !controls_any_yoke_button(); return; }
    if (!controls_captured(name, sizeof name) || sscanf(name, "yoke_button%d", &number) != 1) return;
    for (i = 0; i < S.layout_step - 1; i++) if (S.layout_new[i] == number) return;        /* that one has its place already */
    S.layout_new[S.layout_step - 1] = number;
    S.capture_armed = 0;
    sound_fx(FX_TINK);
    if (++S.layout_step <= 4) return;
    for (i = 0; i < 4; i++) S.now.yoke_layout[i] = S.layout_new[i];
    S.layout_step = 0;
    sound_fx(FX_PICKUP);
}

/* Laying out a button: vpbind.h. */
static void capture(vp_input *in)
{
    const char *standard;
    char *has = laid_of(S.row, &standard);
    int got = vpb_layout_step(&S.lay, in, has, sizeof S.now.menu_key, standard);
    if (got == VPB_CHANGED) sound_fx(FX_PICKUP);
    else if (got == VPB_LEFT) sound_fx(FX_TINK);
}

int service_update(vp_input *in, int hotkey)
{
    int accept, left, right, up, down, count;
    float yx, yy;
    const int *rows = page_rows(&count);

    S.frames++;
    sound_for_row();
    if (hotkey) return SERVICE_CANCELLED;
    if (S.lay.capturing) { capture(in); return SERVICE_OPEN; }
    if (S.layout_step) { capture_layout(in); return SERVICE_OPEN; }

    /* The menu's own controls, whatever is laid out: the yoke, the stick,
     * the arrow keys; START 1, Enter and the keypad's Enter say yes. */
    controls_menu(in, &yx, &yy, &accept);
    left  = yx < -YOKE_ON;
    right = yx > YOKE_ON;
    vpb_layout_filter(&S.lay, &left, &right);
    if (is_button(S.row) && !S.editing
        && (vpb_key_pressed(SDL_SCANCODE_DELETE) || vpb_key_pressed(SDL_SCANCODE_KP_PERIOD))) {
        const char *standard;
        char *has = laid_of(S.row, &standard);
        vpb_layout_standard(&S.lay, has, sizeof S.now.menu_key, standard);
        sound_fx(FX_PICKUP);
        return SERVICE_OPEN;
    }
    up    = yy < -YOKE_ON;
    down  = yy > YOKE_ON;
    S.held_h = left != right ? S.held_h + 1 : 0;
    S.held_v = up != down ? S.held_v + 1 : 0;
    if (repeated(S.held_h)) change(S.row, right ? 1 : -1);

    if (S.editing) {
        if (accept) { S.editing = 0; sound_fx(FX_TINK); }
        return SERVICE_OPEN;
    }
    if (repeated(S.held_v)) {
        vpb_layout_moved(&S.lay);
        S.cursor += down ? 1 : -1;
        if (S.cursor < 0) S.cursor = 0;
        if (S.cursor >= count) S.cursor = count - 1;
        S.row = rows[S.cursor];
        S.preview = 0;
        S.factory_armed = 0;
    }
    if (!accept) return SERVICE_OPEN;
    if (S.row == R_SAVE) return SERVICE_SAVED;
    if (S.row == R_CANCEL) return SERVICE_CANCELLED;
    if (S.row == R_SETUP) return SERVICE_SETUP;
    if (S.row == R_FACTORY) {           /* asked twice: it cannot be undone */
        if (S.factory_armed) return SERVICE_FACTORY;
        S.factory_armed = 1;
        sound_fx(FX_WARN);
        return SERVICE_OPEN;
    }
    sound_fx(FX_TINK);
    if (S.row == R_GAME) turn_to(PAGE_GAME);
    else if (S.row == R_CONTROLS) turn_to(PAGE_CONTROLS);
    else if (S.row == R_VIDEO) turn_to(PAGE_VIDEO);
    else if (S.row == R_MONITOR) turn_to(PAGE_MONITOR);
    else if (S.row == R_BACK) turn_to(PAGE_MAIN);
    else if (is_button(S.row)) vpb_layout_begin(&S.lay, laid_of(S.row, NULL));
    else if (S.row == R_LAYOUT) { S.layout_step = 1; S.capture_armed = 0; }
    else if (row_switch(S.row)) change(S.row, 1);
    else S.editing = 1;
    return SERVICE_OPEN;
}

/* --- drawing (board units, y up) ---------------------------------------------- */

static void text(const char *s, float x, float y, float height, int align, int r, int g, int b)
{
    world_rgb(r, g, b);
    world_text(s, x, y, height, align);
}

static void diamond(float x, float y, float r)
{
    vp_dvg *d = world_dvg();
    vp_moveto(d, (int)(x - r), (int)y);
    vp_lineto(d, (int)x, (int)(y + r));
    vp_lineto(d, (int)(x + r), (int)y);
    vp_lineto(d, (int)x, (int)(y - r));
    vp_lineto(d, (int)(x - r), (int)y);
}

/* A slider: the part set bright, the rest dim, a knob where they meet. */
static void slider(float x, float y, float width, float part, int lit, float level)
{
    float knob = x + width * part;
    int bright = (int)((lit ? 255.0f : 150.0f) * level);

    world_rgb(0, bright, bright);
    world_bline(x, y - 40, x, y + 40);
    world_bline(x, y, knob, y);
    world_rgb(40, 60, 110);
    world_bline(knob, y, x + width, y);
    world_bline(x + width, y - 40, x + width, y + 40);
    if (lit) world_rgb(255, 255, 0); else world_rgb(200, 200, 200);
    diamond(knob, y, 45);
}

void service_show_monitor(void) { turn_to(PAGE_MONITOR); }

int service_picture_size(void) { return S.now.picture_size >= 60 && S.now.picture_size <= 100 ? S.now.picture_size : 100; }

void service_draw(void)
{
    int count;
    const int *rows = page_rows(&count);
    char value[48];
    int i;

    world_fade(1.0f);
    world_level(1.0f);
    if (S.page == PAGE_MONITOR) {
        /* The picture's edge, to set its size by: a frame round the whole
         * of what the game draws, and a cross through its middle. Where
         * the frame folds over or is cut off, the picture is too big. */
        world_rgb(255, 255, 255);
        world_bline(0, 0, 4095, 0); world_bline(4095, 0, 4095, 4095);
        world_bline(4095, 4095, 0, 4095); world_bline(0, 4095, 0, 0);
        world_rgb(90, 90, 90);
        world_bline(0, 2048, 4095, 2048); world_bline(2048, 0, 2048, 4095);
        text("THE WHITE FRAME IS THE PICTURE'S EDGE", 2048, 2300, 80, VP_TEXT_CENTER, 0, 150, 0);
        text("BEAM LIMIT - LOWER THINS THE PICTURE OUT SOONER", 2048, 2150, 80, VP_TEXT_CENTER, 0, 150, 0);
    }
    text(S.page == PAGE_MONITOR ? "MONITOR" : S.page == PAGE_GAME ? "GAME" : S.page == PAGE_CONTROLS ? "CONTROLS" : S.page == PAGE_VIDEO ? "VIDEO"
                                                                                                       : "SERVICE MENU",
         2048, 3700, 170, VP_TEXT_CENTER, 255, 255, 0);
    for (i = 0; i < count; i++) {
        int   row = rows[i];
        int   here = row == S.row, set = here && (S.editing || S.lay.capturing);
        int   lo, hi, step, *number = row_number(row, &lo, &hi, &step), *onoff = row_switch(row);
        /* a long page (CONTROLS, 14 rows) a little closer, clear of the help at the bottom */
        float y = count > 12 ? 3450.0f - 210.0f * (float)i : 3300.0f - 250.0f * (float)i;
        int   r = here ? 255 : 60, g = here ? 255 : 110, b = 255;

        if (row == R_SAVE || row == R_CANCEL) continue;         /* side by side, below */
        if (here) text(">", 180, y, 100, VP_TEXT_RIGHT, 255, 255, 0);
        text(row_names[row], 240, y, 100, VP_TEXT_LEFT, r, g, b);
        if (set) { r = 255; g = 255; b = 0; }
        if (row == R_FACTORY) {             /* the name is long: its word further right */
            if (S.factory_armed && here) { if ((S.frames / 12) & 1) text("SURE?", 3050, y, 100, VP_TEXT_LEFT, 255, 60, 60); }
            else text("START", 3050, y, 100, VP_TEXT_LEFT, r, g, b);
        } else if (row == R_SETUP) {
            text("START", 2300, y, 100, VP_TEXT_LEFT, r, g, b);
        } else if (row == R_GAME || row == R_CONTROLS || row == R_VIDEO || row == R_MONITOR) {
            text("OPEN", 2300, y, 100, VP_TEXT_LEFT, r, g, b);
        } else if (row == R_SENS) {
            snprintf(value, sizeof value, "%d.%02d", *number / 100, *number % 100);
            text(value, 2300, y, 100, VP_TEXT_LEFT, r, g, b);
        } else if (row == R_DIFFICULTY) {
            text(difficulty_names[*number], 2300, y, 100, VP_TEXT_LEFT, r, g, b);
        } else if (row == R_START) {
            static const char *const years[] = { "1910", "1940", "1970", "1982", "2001" };
            text(years[*number % 5], 2300, y, 100, VP_TEXT_LEFT, r, g, b);
        } else if (row == R_CONTROL) {
            text(S.now.control_stick ? "JOYSTICK + BUTTONS" : "YOKE", 2300, y, S.now.control_stick ? 80 : 100, VP_TEXT_LEFT, r, g, b);
        } else if (row == R_VIEW) {
            text(view_names[*number % VIEW_COUNT], 2300, y, 100, VP_TEXT_LEFT, r, g, b);
        } else if (number && is_slider(row)) {
            slider(2300, y + 50, 1100, (float)(*number - lo) / (float)(hi - lo), here,
                   row >= R_BRIGHT && row <= R_BRIGHT_LAST ? (float)(*number < 15 ? 15 : *number) / 100.0f : 1.0f);
            snprintf(value, sizeof value, "%d", *number);
            text(value, 3850, y, 100, VP_TEXT_RIGHT, r, g, b);
        } else if (number) {
            snprintf(value, sizeof value, "%d", *number);
            text(value, 2300, y, 100, VP_TEXT_LEFT, r, g, b);
        } else if (onoff) {
            text(*onoff ? "YES" : "NO", 2300, y, 100, VP_TEXT_LEFT, r, g, b);
        } else if (row == R_LAYOUT) {
            /* further right than the other values: the row's name is long */
            if (S.layout_step && here) {
                snprintf(value, sizeof value, "PRESS %s", place_names[S.layout_step - 1]);
                if ((S.frames / 15) & 1) text(value, 2650, y, 70, VP_TEXT_LEFT, 255, 255, 0);
            } else {
                /* each place's number on its own, well apart: right trigger, left trigger, right thumb, left thumb */
                int k;
                for (k = 0; k < 4; k++) {
                    snprintf(value, sizeof value, "%d", S.now.yoke_layout[k]);
                    text(value, 2700.0f + 380.0f * (float)k, y, 100, VP_TEXT_LEFT, r, g, b);
                }
            }
        } else if (S.lay.capturing && here && ((S.frames / 15) & 1)) {
            text("PRESS A BUTTON", 2300, y, 100, VP_TEXT_LEFT, 255, 255, 0);
        } else if (is_button(row)) {
            /* All its controls; smaller when they would not fit. */
            char all[256];
            float size = 100.0f, room = 4000.0f - 2300.0f;
            vpb_spelled(laid_of(row, NULL), all, sizeof all);
            if (row == R_MENUKEY && strcmp(laid_of(row, NULL), "key_F2")) strncat(all, " + F2", sizeof all - strlen(all) - 1);
            if ((float)strlen(all) * size * 1.05f > room) size = room / ((float)strlen(all) * 1.05f);
            text(all, 2300, y, size, VP_TEXT_LEFT, r, g, b);
        }
    }
    if (S.page == PAGE_MAIN) {
        if (S.row == R_CANCEL) text("CANCEL", 1400, 600, 170, VP_TEXT_CENTER, 255, 255, 255);
        else text("CANCEL", 1400, 620, 120, VP_TEXT_CENTER, 200, 60, 60);
        if (S.row == R_SAVE) text("SAVE", 2700, 600, 170, VP_TEXT_CENTER, 255, 255, 255);
        else text("SAVE", 2700, 620, 120, VP_TEXT_CENTER, 0, 200, 0);
    }
    {
        const char *said = vpb_layout_said(&S.lay);
        const char *one = "YOKE, UP, DOWN - ROW   LEFT, RIGHT - SET", *two = "ENTER, START 1 - TAKE";
        if (said) text(said, 2048, 420, 80, VP_TEXT_CENTER, 255, 80, 80);
        if (S.layout_step) { one = "PRESS THE YOKE'S BUTTON FOR EACH PLACE"; two = "START 1 - LEAVE IT AS IT IS"; }
        else if (S.editing) { one = "YOKE, LEFT, RIGHT - SET"; two = "ENTER, START 1 - DONE"; }
        else if (S.factory_armed && S.row == R_FACTORY) {
            text("SETTINGS, CONTROLS AND HIGH SCORES ARE ALL DELETED", 2048, 420, 80, VP_TEXT_CENTER, 255, 80, 80);
            one = "START 1 AGAIN - RESET AND RESTART"; two = "UP, DOWN - KEEP EVERYTHING";
        }
        else if (S.lay.capturing || is_button(S.row)) vpb_layout_help(&S.lay, &one, &two);
        text(one, 2048, 300, 80, VP_TEXT_CENTER, 0, 150, 0);
        text(two, 2048, 170, 80, VP_TEXT_CENTER, 0, 150, 0);
    }
}
