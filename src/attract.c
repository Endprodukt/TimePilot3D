/* attract.c - the pages nobody plays: the title, the high scores, who is
 * who, and the initials of a new best pilot. */
#include "game_internal.h"
#include <ctype.h>

/* this file's own, in the order below */
static void big_text(const char *s, float y, float width, float max_height, int r, int g, int b);
static void fit_text(const char *s, float y, float width, float height, int r, int g, int b);
static void wire_box(const vp_xf *at, float x0, float y0, float z0, float x1, float y1, float z1);
static int place_of(const char *control);
static void call_out(vp_vec3 at, const char *word, int lit);
static void howto_shots(const vp_xf *plane, float into);

/* --- attract mode ---------------------------------------------------------------- */

/* Lettering as wide as `width`, with a dimmer copy behind it. */

static void big_text(const char *s, float y, float width, float max_height, int r, int g, int b)
{
    float h, w;
    vp_text_font(VP_FONT_ASTEROIDS);
    w = vp_text_width(s, 100.0f);
    h = w > 0 ? width / w * 100.0f : max_height;
    if (h > max_height) h = max_height;
    world_level(G.bright[BRIGHT_TITLE] * 0.4f);
    world_rgb(r, g, b);
    world_text(s, CX + 22.0f, y - 22.0f, h, VP_TEXT_CENTER);
    world_level(G.bright[BRIGHT_TITLE]);
    world_rgb(r, g, b);
    world_text(s, CX, y, h, VP_TEXT_CENTER);
}

/* The title: the plane on its way through time, for ever, from each era
 * to the next; over it the name. */

void draw_title(void)
{
    float t = (float)G.frames / 60.0f;
    float leg = t / 6.0f;                           /* six seconds from one era to the next */
    int from = (int)leg % ERA_COUNT, to = (from + 1) % ERA_COUNT, i;
    float mix = smooth(leg - floorf(leg)), fx = CX, fy = CY;
    vp_xf place;

    {
        float ex, ey;
        world_tunnel_centre(t * 150.0f, 0.8f, &ex, &ey);
        world_draw_tunnel(t * 150.0f, ex, ey, 0.8f, 0.75f, eras[from].colour, eras[to].colour, mix, &fx, &fy, 0, 0);
    }
    world_level(G.bright[BRIGHT_EFFECTS] * 0.7f);
    world_rgb(255, 255, 255);
    world_draw_clock(fx, fy, 95.0f, t * 0.4f);

    world_pose(vp_v3(0, 0, 0), 0, 0, 0);
    place = world_place(vp_v3(5.0f * sinf(t * 0.9f), -4.5f + 1.5f * sinf(t * 1.4f), 34.0f), 0.15f * sinf(t * 0.6f),
                        0.06f * sinf(t * 1.4f), 0.5f * sinf(t * 0.9f));
    world_level(G.bright[BRIGHT_PLAYER]);
    model_draw(MODEL_PLAYER, &place, TINT_OWN);
    draw_burner(&place, 0.8f);

    logo_draw(CX, 3560, 3300, G.bright[BRIGHT_TITLE]);
    /* the years, the one the plane is bound for lit */
    for (i = 0; i < ERA_COUNT; i++) {
        const float *c = eras[i].colour;
        float k = i == to ? 1.0f : 0.4f;
        text(eras[i].year, CX + ((float)i - 2.0f) * 620.0f, 880, i == to ? 120 : 95, VP_TEXT_CENTER,
             (int)(c[0] * k), (int)(c[1] * k), (int)(c[2] * k));
    }
    text(G.version, 3935, 120, 60, VP_TEXT_RIGHT, 90, 90, 90);        /* which build this is */
    draw_coin_line();
}

/* The high scores, over the land of an era flown across slowly. */

void draw_scores(void)
{
    float t = (float)G.frames / 60.0f;
    int era_index = (G.frames / 600) % ERA_COUNT;
    vp_vec3 eye = vp_v3(40.0f * t, 230.0f, 55.0f * t);

    world_pose(eye, 0.6f, -0.32f, 0);
    G.view_heading = 0.6f;
    world_fade(0.6f);
    world_level(G.bright[BRIGHT_GROUND]);
    world_rgb(0, 200, 90);
    world_draw_ground(eye, G.view_heading, 0);
    world_level(G.bright[BRIGHT_GROUND] * 1.4f);
    city_draw(era_index, eye, t, 1.0f);
    world_fade(1.0f);

    big_text("BEST PILOTS", 3450, 2400, 260, 255, 255, 0);
    world_level(G.bright[BRIGHT_TEXT]);
    world_rgb(0, 255, 255);
    vp_text_font(VP_FONT_ASTEROIDS);
    vp_hiscore_draw(world_dvg(), G.table, G.table_count, CX, 3050, 120, G.rank, (double)G.frames / 60.0);
    draw_coin_line();
}

/* A line of text as high as `height`, less if it would be wider than `width`. */

static void fit_text(const char *s, float y, float width, float height, int r, int g, int b)
{
    float w;
    vp_text_font(VP_FONT_ASTEROIDS);
    w = vp_text_width(s, height);
    text(s, CX, y, w > width ? height * width / w : height, VP_TEXT_CENTER, r, g, b);
}

/* Who is who: each era's enemy turning in its colour, with its year and
 * what it is worth; and how the game is flown. */

void draw_cast(void)
{
    float t = (float)G.frames / 60.0f;
    char buf[48];
    int i;

    world_pose(vp_v3(0, 0, 0), 0, 0, 0);
    big_text("ENEMIES OF EVERY AGE", 3500, 3000, 200, 255, 255, 0);
    for (i = 0; i < ERA_COUNT; i++) {
        float x = ((float)i - 2.0f) * 33.0f, s = 11.0f / model_radius(eras[i].model), sx, sy;
        vp_xf place = vp_xf_mul2(vp_xf_translate(x, 24.0f, 150.0f),
                      vp_xf_mul2(vp_xf_rot_y(t * 0.9f + (float)i), vp_xf_mul2(vp_xf_rot_x(-0.25f), vp_xf_scale(s, s, s))));
        obj shown;

        memset(&shown, 0, sizeof shown);
        shown.model = eras[i].model;
        shown.spin = t * 20.0f;
        world_level(G.bright[BRIGHT_ENEMIES]);
        model_draw(eras[i].model, &place, TINT_OWN);
        draw_rotors(&shown, &place);
        if (!world_project(vp_v3(x, 7.0f, 150.0f), &sx, &sy)) continue;
        text(eras[i].year, sx, sy, 110, VP_TEXT_CENTER, (int)eras[i].colour[0], (int)eras[i].colour[1], (int)eras[i].colour[2]);
        snprintf(buf, sizeof buf, "%d", eras[i].points);
        text(buf, sx, sy - 170.0f, 80, VP_TEXT_CENTER, 255, 255, 255);
    }
    fit_text("THE BIG RED AND YELLOW ONES  1500", 1900, 3500, 105, 255, 200, 0);
    fit_text("FIVE IN FORMATION  2000", 1700, 3500, 105, 120, 255, 120);
    fit_text("PARACHUTES  1000 UP TO 5000", 1500, 3500, 105, 255, 255, 255);
    fit_text("ITEMS - FLY THROUGH GATES, SHOOT DEPOTS", 1300, 3500, 105, 80, 255, 255);
    fit_text("GUNS ON THE GROUND  300", 1100, 3500, 105, 255, 90, 90);
    fit_text("THE BOSS OPENS THE TIME TUNNEL", 900, 3500, 105, 255, 80, 80);
    draw_coin_line();
}

/* The items: each one turning in its colour, with what it does; under them
 * where they come from - a gate the plane flies through, a depot shot to
 * pieces - each as it stands on the ground, with the mark that shows it.
 * In attract mode after who is who, and after how to play (`tutorial`). */

void draw_items_page(int tutorial)
{
    static const char *const does[ITEM_COUNT][2] = {
        { "TAKES", "HITS" }, { "", "HOMING" }, { "SECONDS", "" }, { "FULL", "ENERGY" }, { "FOOL", "MISSILES" }
    };
    float t = (float)G.frames / 60.0f, gate_turn = 0.9f + 0.25f * sinf(t * 0.4f), u;
    char buf[64];
    vp_xf gate, depot;
    int i;

    world_pose(vp_v3(0, 0, 0), 0, 0, 0);
    big_text(tutorial ? "GET ITEMS" : "ITEMS", 3500, 2200, 200, 255, 255, 0);
    for (i = 0; i < ITEM_COUNT; i++) {
        vp_vec3 at = vp_v3(((float)i - 2.0f) * 30.0f, 36.0f, 150.0f);
        const unsigned char *c = item_rgb[i];
        float sx, sy;
        world_level(G.bright[BRIGHT_EFFECTS]);
        draw_item(at, i, 5.5f, t * 1.5f + (float)i);
        if (!world_project(vp_v3(at.x, at.y - 9.0f, at.z), &sx, &sy)) continue;
        text(item_names[i], sx, sy, 62, VP_TEXT_CENTER, c[0], c[1], c[2]);
        /* what it does, the number from the game's own rules */
        if (i == ITEM_SHIELD) snprintf(buf, sizeof buf, "%s %d %s", does[i][0], SHIELD_HITS, does[i][1]);
        else if (i == ITEM_MISSILES) snprintf(buf, sizeof buf, "%d %s", MISSILE_LOAD, does[i][1]);
        else if (i == ITEM_TRIPLE) snprintf(buf, sizeof buf, "%d %s", (int)TRIPLE_TIME, does[i][0]);
        else snprintf(buf, sizeof buf, "%s %s", does[i][0], does[i][1]);
        text(buf, sx, sy - 110.0f, 50, VP_TEXT_CENTER, 200, 200, 200);
        if (i == ITEM_FLARES) text("FROM 1970", sx, sy - 200.0f, 50, VP_TEXT_CENTER, 200, 200, 200);
    }

    /* a gate, and the plane flying through it now and then */
    gate = vp_xf_mul2(vp_xf_translate(-40.0f, -16.0f, 150.0f), vp_xf_mul2(vp_xf_rot_y(gate_turn), vp_xf_scale(0.42f, 0.42f, 0.42f)));
    world_level(G.bright[BRIGHT_GROUND] * 1.6f);
    city_draw_site(CITY_GATE, &gate);
    u = fmodf(t, 4.0f) / 2.5f;                  /* through it in two and a half seconds, then a pause */
    world_level(G.bright[BRIGHT_EFFECTS]);
    if (u < 0.55f || ((G.frames / 10) & 1)) draw_mystery(vp_xf_point(&gate, vp_v3(0, 17.0f, 0)), 3.0f, t * 2.0f);
    if (u < 1.0f) {
        vp_vec3 at = vp_xf_point(&gate, vp_v3(0, 17.0f, -90.0f + 180.0f * u));
        vp_xf plane = vp_xf_mul2(world_place(at, gate_turn, 0, 0), vp_xf_scale(0.42f, 0.42f, 0.42f));
        world_level(G.bright[BRIGHT_PLAYER]);
        model_draw(MODEL_PLAYER, &plane, TINT_OWN);
    }

    /* a depot, shot at */
    depot = vp_xf_mul2(vp_xf_translate(34.0f, -16.0f, 150.0f), vp_xf_mul2(vp_xf_rot_y(0.5f + 0.2f * t), vp_xf_scale(0.55f, 0.55f, 0.55f)));
    world_level(G.bright[BRIGHT_GROUND] * 1.6f);
    city_draw_site(CITY_DEPOT, &depot);
    world_level(G.bright[BRIGHT_EFFECTS]);
    if ((G.frames / 12) & 1) draw_mystery(vp_xf_point(&depot, vp_v3(0, 37.0f, 0)), 2.5f, t * 2.0f);
    world_rgb(255, 255, 160);
    for (i = 0; i < 3; i++) {                   /* shots going in */
        float k = fmodf(t * 2.2f + (float)i / 3.0f, 1.0f);
        vp_vec3 from = vp_v3(22.0f, 6.0f, 95.0f), to = vp_xf_point(&depot, vp_v3(0, 14.0f, 0));
        vp_vec3 a = vp_v3_add(from, vp_v3_scale(vp_v3_sub(to, from), k));
        vp_vec3 b = vp_v3_add(from, vp_v3_scale(vp_v3_sub(to, from), k + 0.06f));
        world_line(a, b);
    }
    {
        float sx, sy;
        if (world_project(vp_v3(-40.0f, -21.0f, 150.0f), &sx, &sy)) {
            text("FLY THROUGH", sx, sy, 70, VP_TEXT_CENTER, 255, 170, 40);
            text("A GATE", sx, sy - 110.0f, 70, VP_TEXT_CENTER, 255, 170, 40);
        }
        if (world_project(vp_v3(34.0f, -21.0f, 150.0f), &sx, &sy)) {
            snprintf(buf, sizeof buf, "SHOOT A DEPOT %d TIMES", DEPOT_HITS);
            text(buf, sx, sy, 70, VP_TEXT_CENTER, 255, 255, 255);
            text("THEN FLY INTO THE ITEM", sx, sy - 110.0f, 70, VP_TEXT_CENTER, 255, 255, 255);
        }
    }
    /* and the parachutists: energy, and a bonus for one shot by mistake and caught all the same */
    fit_text("PARACHUTES - FLY INTO THEM FOR ENERGY", 1000, 3500, 80, 255, 255, 255);
    snprintf(buf, sizeof buf, "SHOT ONE BY MISTAKE - CATCH HIM AS HE FALLS  %d", CHUTE_CATCH_POINTS);
    fit_text(buf, 890, 3500, 80, 255, 255, 255);
    snprintf(buf, sizeof buf, "THE ? SHOWS WHERE   EVERY ITEM %d", ITEM_POINTS);
    fit_text(buf, 740, 3500, 90, 80, 255, 255);
    draw_coin_line();
}

/* --- how to play ------------------------------------------------------------------
 *
 * The yoke, in the round, and the player's own plane beside it doing what
 * the yoke does. First the yoke is turned, then pushed and pulled; then it
 * turns round to show its front, where the two triggers are, and back
 * again for the two buttons under the thumbs - each with what it does in
 * this cabinet, as the service menu has them laid. Last the roll, and the
 * START button.
 *
 * The yoke has the shape of the arcade flight yokes of the eighties, and
 * nobody's sign on it: a box, a cone out of each side of it, and on each
 * cone a grip that leans in at its top - a round button for the thumb
 * high on its back, a trigger on its front. The box turns with the hands
 * as a wheel does; the grips tilt on the cones' axis. */

/* Fire's turn: the burst the plane shoots, as tracers from both guns
 * going away ahead of it (in the plane's own frame; its nose is +z). */
static void howto_shots(const vp_xf *plane, float into)
{
    int i, side;
    world_level(G.bright[BRIGHT_SHOTS]);
    world_rgb(255, 255, 160);
    for (i = 0; i < HOWTO_SHOTS; i++) {
        float age = into - HOWTO_SHOT_AT - HOWTO_SHOT_EVERY * (float)i, z;
        if (age < 0 || age > 0.6f) continue;
        z = 6.0f + 90.0f * age;
        for (side = -1; side <= 1; side += 2)
            world_line(vp_xf_point(plane, vp_v3(1.6f * (float)side, -0.2f, z)), vp_xf_point(plane, vp_v3(1.6f * (float)side, -0.2f, z + 14.0f)));
    }
}

/* What how to play is heard with: nothing all the way through but the
 * burst of fire's turn and the afterburner lighting at faster's. Once a
 * step, with the time `t` how to play has been on (the same on either
 * page, the yoke's and the stick's). */
void howto_sounds(float t)
{
    float fire = HOWTO_BUTTONS + HOWTO_SHOT_AT, faster = HOWTO_BUTTONS + 2.0f * HOWTO_EACH;
    int i;
    for (i = 0; i < HOWTO_SHOTS; i++) {
        float at = fire + HOWTO_SHOT_EVERY * (float)i;
        if (t >= at && t - DT < at) sound_fx(FX_SHOT);
    }
    if (t >= faster && t - DT < faster) sound_fx(FX_BURNER);
}

/* The twelve edges of a box. */
static void wire_box(const vp_xf *at, float x0, float y0, float z0, float x1, float y1, float z1)
{
    vp_vec3 c[8];
    int i;
    for (i = 0; i < 8; i++) c[i] = vp_xf_point(at, vp_v3(i & 1 ? x1 : x0, i & 2 ? y1 : y0, i & 4 ? z1 : z0));
    for (i = 0; i < 8; i++) {
        if (!(i & 1)) world_line(c[i], c[i | 1]);
        if (!(i & 2)) world_line(c[i], c[i | 2]);
        if (!(i & 4)) world_line(c[i], c[i | 4]);
    }
    if (world_solid()) {
        /* its six faces: what is behind it is not seen through it */
        static const int f[6][4] = { { 0, 1, 3, 2 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 3, 7, 5 } };
        world_group();
        for (i = 0; i < 6; i++) {
            vp_vec3 q[4];
            q[0] = c[f[i][0]]; q[1] = c[f[i][1]]; q[2] = c[f[i][2]]; q[3] = c[f[i][3]];
            world_face(q, 4);
        }
    }
}

/* A disc of `n` corners as a face, round `centre` in the plane of `u`
 * and `v` (both in the frame of `at`). */
static void disc_face(const vp_xf *at, vp_vec3 centre, vp_vec3 u, vp_vec3 v, float r, int n)
{
    vp_vec3 q[8];
    int i;
    if (!world_solid()) return;
    if (n > 8) n = 8;
    for (i = 0; i < n; i++) {
        float a = 6.2831853f * (float)i / (float)n;
        q[i] = vp_xf_point(at, vp_v3_add(centre, vp_v3_add(vp_v3_scale(u, r * cosf(a)), vp_v3_scale(v, r * sinf(a)))));
    }
    world_group();
    world_face(q, n);
}

/* Where on the yoke a control laid in the service menu sits: one of
 * YOKE_PLACES, or -1 when it is not one of the yoke's four (another
 * button, or not on the yoke at all). By the yoke's own layout, which the
 * service menu has asked for. */
static int place_of(const char *control)
{
    int number = 0, i;
    if (sscanf(control, "yoke_button%d", &number) != 1) return -1;
    for (i = 0; i < YOKE_PLACES; i++) if (G.yoke_layout[i] == number) return i;
    return -1;
}

/* A control on the yoke, named: a line from it out to the side it is on,
 * and the word at the line's end. */
static void call_out(vp_vec3 at, const char *word, int lit)
{
    float sx, sy, tx;
    if (!word || !world_project(at, &sx, &sy)) return;
    tx = sx < 1150.0f ? sx - 300.0f : sx + 300.0f;
    world_level(G.bright[BRIGHT_TEXT]);
    if (lit) world_rgb(255, 230, 0); else world_rgb(110, 110, 110);
    world_bline(sx, sy, tx, sy + 330.0f);
    text(word, tx, sy + 360.0f, 110, VP_TEXT_CENTER, lit ? 255 : 110, lit ? 230 : 110, lit ? 0 : 110);
}

void draw_howto(void)
{
    /* when what is shown: turning, pushing, the four buttons one after the other, the roll, START */
    enum { FIRE, REAR, FASTER, SLOWER, DOES };
    const float T_PUSH = 5.0f, T_BUTTONS = HOWTO_BUTTONS, EACH = HOWTO_EACH, T_ROLL = T_BUTTONS + 4.0f * EACH, T_START = T_ROLL + 4.5f;
    static const char *const does[DOES] = { "FIRE", "LOOK BACK", "FASTER", "SLOWER" };
    static const char *const place_words[YOKE_PLACES] = { "RIGHT TRIGGER", "LEFT TRIGGER", "RIGHT THUMB BUTTON", "LEFT THUMB BUTTON" };
    const char *const laid[DOES] = { G.btn_fire, G.btn_rear, G.btn_faster, G.btn_slower };
    float t = G.state_time;
    /* what the hands do: the yoke turned (-1..1), pushed (-1..1), and the plane's roll right round at the end */
    float turn = t < T_PUSH ? sinf(PI2 * t / 2.5f) : t > T_ROLL && t < T_START ? 1.0f : 0;
    float push = t >= T_PUSH && t < T_BUTTONS ? sinf(PI2 * (t - T_PUSH) / 2.5f) : 0;
    float roll_round = t > T_ROLL ? PI2 * smooth((t - T_ROLL - 0.6f) / 3.0f) : 0;
    int at[DOES], word_at[YOKE_PLACES], lit[YOKE_PLACES] = { 0, 0, 0, 0 };
    int step = t >= T_BUTTONS && t < T_ROLL ? (int)((t - T_BUTTONS) / EACH) : -1, rolling = t >= T_ROLL && t < T_START;
    int buttons = t >= T_BUTTONS && t < T_START, k, j;
    float front = 0, was = 0, yaw;
    vp_xf base, box, grips, plane;
    char where[48];
    const char *line1 = "", *line2 = "";

    /* what is laid where: each thing a button does, and each place's word */
    for (k = 0; k < YOKE_PLACES; k++) word_at[k] = -1;
    for (k = 0; k < DOES; k++) {
        at[k] = place_of(laid[k]);
        if (at[k] >= 0) word_at[at[k]] = k;
    }
    if (step > SLOWER) step = SLOWER;
    if (step >= 0 && at[step] >= 0) lit[at[step]] = 1;
    if (rolling) { if (at[FASTER] >= 0) lit[at[FASTER]] = 1; if (at[SLOWER] >= 0) lit[at[SLOWER]] = 1; }
    /* How the yoke is looked at: from the pilot's side, a little from the
     * left and above, or from before it when what is shown is a trigger.
     * It turns round only when the next thing shown is on its other side. */
    for (k = 0; k < 6; k++) {
        float begins = k < 4 ? T_BUTTONS + EACH * (float)k : k == 4 ? T_ROLL : T_START;
        int what = k < 4 ? k : FASTER;
        float want = k == 5 || at[what] < 0 ? 0.0f : at[what] <= YOKE_LEFT_TRIGGER ? 1.0f : 0.0f;
        if (t < begins) break;
        front = was + (want - was) * smooth((t - begins) / 1.1f);
        was = want;
    }
    yaw = 0.4f + (PI - 0.8f) * front;

    world_pose(vp_v3(0, 0, 0), 0, 0, 0);
    big_text("HOW TO PLAY", 3500, 2400, 260, 255, 255, 0);

    /* the yoke */
    base = vp_xf_mul2(vp_xf_translate(-30.0f, 8.0f, 150.0f),
           vp_xf_mul2(vp_xf_rot_x(0.3f), vp_xf_mul2(vp_xf_rot_y(yaw), vp_xf_scale(3.0f, 3.0f, 3.0f))));
    box = vp_xf_mul2(base, vp_xf_rot_z(-turn * 0.6f));
    grips = vp_xf_mul2(box, vp_xf_mul2(vp_xf_translate(0, 0, 3.5f), vp_xf_rot_x(push * 0.5f)));
    world_level(G.bright[BRIGHT_PLAYER]);
    world_rgb(150, 170, 150);
    wire_box(&box, -5.0f, -4.2f, 0, 5.0f, 4.2f, 7.0f);
    for (k = -1; k <= 1; k += 2) {
        float side = (float)k;
        /* this grip's two: the trigger on its front, the thumb's button on its back. x > 0 is the pilot's right. */
        int trigger = k > 0 ? YOKE_RIGHT_TRIGGER : YOKE_LEFT_TRIGGER, thumb = k > 0 ? YOKE_RIGHT_THUMB : YOKE_LEFT_THUMB;
        vp_xf grip = vp_xf_mul2(grips, vp_xf_mul2(vp_xf_translate(9.6f * side, 0, 0), vp_xf_rot_z(0.22f * side)));
        vp_vec3 a[6], b[6];
        world_level(G.bright[BRIGHT_PLAYER]);
        /* the cone out of the box's side, wider towards the grip */
        world_rgb(120, 130, 170);
        for (j = 0; j < 6; j++) {
            float an = PI2 * (float)j / 6.0f;
            a[j] = vp_xf_point(&grips, vp_v3(5.0f * side, 1.1f * cosf(an), 1.1f * sinf(an)));
            b[j] = vp_xf_point(&grips, vp_v3(8.3f * side, 2.3f * cosf(an), 2.3f * sinf(an)));
        }
        for (j = 0; j < 6; j++) { world_line(a[j], a[(j + 1) % 6]); world_line(b[j], b[(j + 1) % 6]); if (!(j & 1)) world_line(a[j], b[j]); }
        if (world_solid()) {
            /* the cone's sides and its two ends */
            world_group();
            for (j = 0; j < 6; j++) {
                vp_vec3 q[4];
                q[0] = a[j]; q[1] = a[(j + 1) % 6]; q[2] = b[(j + 1) % 6]; q[3] = b[j];
                world_face(q, 4);
            }
            world_face(a, 6);
            world_face(b, 6);
        }
        /* the grip */
        world_rgb(70, 150, 255);
        wire_box(&grip, -1.3f, -6.0f, -1.4f, 1.3f, 6.0f, 1.4f);
        /* the trigger, on its front */
        if (lit[trigger] && ((G.frames / 6) & 1)) world_rgb(255, 255, 255); else world_rgb(255, 90, 60);
        wire_box(&grip, -0.7f, 1.4f, 1.4f, 0.7f, 3.6f, 2.7f);
        /* the thumb's button, round, high on its back */
        if (lit[thumb] && ((G.frames / 6) & 1)) world_rgb(255, 255, 255); else world_rgb(255, 210, 0);
        for (j = 0; j < 8; j++) {
            float a0 = PI2 * (float)j / 8.0f, a1 = PI2 * (float)(j + 1) / 8.0f;
            world_line(vp_xf_point(&grip, vp_v3(0.9f * cosf(a0), 4.5f + 0.9f * sinf(a0), -1.75f)),
                       vp_xf_point(&grip, vp_v3(0.9f * cosf(a1), 4.5f + 0.9f * sinf(a1), -1.75f)));
            if (!(j & 1)) world_line(vp_xf_point(&grip, vp_v3(0.9f * cosf(a0), 4.5f + 0.9f * sinf(a0), -1.75f)),
                                     vp_xf_point(&grip, vp_v3(0.9f * cosf(a0), 4.5f + 0.9f * sinf(a0), -1.4f)));
        }
        disc_face(&grip, vp_v3(0, 4.5f, -1.75f), vp_v3(1, 0, 0), vp_v3(0, 1, 0), 0.9f, 8);
        /* their words: of the side that is in view */
        if (buttons && front > 0.6f && word_at[trigger] >= 0)
            call_out(vp_xf_point(&grip, vp_v3(0, 2.5f, 2.7f)), does[word_at[trigger]], lit[trigger]);
        if (buttons && front < 0.4f && word_at[thumb] >= 0)
            call_out(vp_xf_point(&grip, vp_v3(0, 4.5f, -1.75f)), does[word_at[thumb]], lit[thumb]);
    }

    /* the plane, seen from behind and a little above, doing what the yoke does */
    {
        float s = 23.0f / model_radius(MODEL_PLAYER);
        float pitch = (G.invert_pitch ? push : -push) * 0.7f;
        plane = vp_xf_mul2(vp_xf_translate(42.0f, 6.0f, 150.0f),
                vp_xf_mul2(vp_xf_rot_x(0.3f),
                vp_xf_mul2(vp_xf_rot_y(t < T_PUSH ? turn * 0.35f : 0),
                vp_xf_mul2(vp_xf_rot_x(-pitch), vp_xf_mul2(vp_xf_rot_z(-turn * (rolling ? 0 : 0.7f) - roll_round), vp_xf_scale(s, s, s))))));
        world_level(G.bright[BRIGHT_PLAYER]);
        model_draw(MODEL_PLAYER, &plane, TINT_OWN);
        if (step == FASTER || step == SLOWER || rolling) draw_burner(&plane, step == FASTER ? 1.0f : step == SLOWER ? 0.12f : 0.6f);
        if (step == FIRE) howto_shots(&plane, t - T_BUTTONS);
    }

    /* what is being shown, in words: for a button what it does, and where it is */
    where[0] = 0;
    if (step >= 0) {
        if (at[step] >= 0) {
            snprintf(where, sizeof where, "%s", place_words[at[step]]);
        } else {                        /* not one of the yoke's four: by its name in the service menu */
            size_t i;
            for (i = 0; laid[step][i] && i + 1 < sizeof where; i++)
                where[i] = laid[step][i] == '_' ? ' ' : (char)toupper((unsigned char)laid[step][i]);
            where[i] = 0;
        }
    }
    if (t < T_PUSH) { line1 = "TURN THE YOKE"; line2 = "THE PLANE TURNS WITH IT"; }
    else if (t < T_BUTTONS) { line1 = G.invert_pitch ? "PUSH TO CLIMB - PULL TO DIVE" : "PUSH TO DIVE - PULL TO CLIMB"; line2 = "ALL THE WAY ROUND MAKES A LOOP"; }
    else if (step >= 0) { line1 = does[step]; line2 = where; }
    else if (rolling) { line1 = "FASTER AND SLOWER TOGETHER"; line2 = "NOW THE YOKE ROLLS THE PLANE"; }
    else { line1 = "START CHANGES THE VIEW"; line2 = "HUD ONLY, COCKPIT, FROM BEHIND"; }
    fit_text(line1, 1150, 3500, 170, 255, 255, 255);
    fit_text(line2, 930, 3500, 105, 0, 255, 190);
    if (step == FASTER) fit_text("LET GO AND THE SPEED GOES BACK TO THE MIDDLE", 760, 3300, 80, 0, 200, 150);
    if (step == REAR) fit_text("FOR AS LONG AS IT IS HELD", 760, 3300, 80, 0, 200, 150);
    draw_coin_line();
}

/* How to play, for a cabinet flown with a stick and buttons: the same
 * page, the same order - the stick moved to the sides, then up and down,
 * then each of the four buttons with what it does, the roll, START. On a
 * panel seen from the player's side: the stick with its ball, and the
 * four buttons in a row, numbered as advmame.rc numbers them. */
void draw_howto_stick(void)
{
    enum { FIRE, REAR, FASTER, SLOWER, DOES };
    const float T_PUSH = 5.0f, T_BUTTONS = HOWTO_BUTTONS, EACH = HOWTO_EACH, T_ROLL = T_BUTTONS + 4.0f * EACH, T_START = T_ROLL + 4.5f;
    static const char *const does[DOES] = { "FIRE", "LOOK BACK", "FASTER", "SLOWER" };
    const char *const laid[DOES] = { G.btn_fire, G.btn_rear, G.btn_faster, G.btn_slower };
    float t = G.state_time;
    /* a stick is all or nothing: over, back, over the other way */
    float turn = t < T_PUSH ? (sinf(PI2 * t / 2.5f) > 0.3f ? 1.0f : sinf(PI2 * t / 2.5f) < -0.3f ? -1.0f : 0)
               : t > T_ROLL && t < T_START ? 1.0f : 0;
    float push = t >= T_PUSH && t < T_BUTTONS ? (sinf(PI2 * (t - T_PUSH) / 2.5f) > 0.3f ? 1.0f : sinf(PI2 * (t - T_PUSH) / 2.5f) < -0.3f ? -1.0f : 0) : 0;
    float lean = t < T_PUSH ? sinf(PI2 * t / 2.5f) : turn, nod = t >= T_PUSH && t < T_BUTTONS ? sinf(PI2 * (t - T_PUSH) / 2.5f) : 0;
    float roll_round = t > T_ROLL ? PI2 * smooth((t - T_ROLL - 0.6f) / 3.0f) : 0;
    int step = t >= T_BUTTONS && t < T_ROLL ? (int)((t - T_BUTTONS) / EACH) : -1, rolling = t >= T_ROLL && t < T_START;
    int at[DOES], lit[4] = { 0, 0, 0, 0 }, k, j;
    vp_xf panel, plane;
    vp_vec3 foot, tip;
    char where[48];
    const char *line1 = "", *line2 = "";

    /* which button does what: advmame.rc's p1_button1 to 4 are the panel's four */
    for (k = 0; k < DOES; k++) {
        int number = 0;
        at[k] = sscanf(laid[k], "p1_button%d", &number) == 1 && number >= 1 && number <= 4 ? number - 1 : -1;
    }
    if (step > SLOWER) step = SLOWER;
    if (step >= 0 && at[step] >= 0) lit[at[step]] = 1;
    if (rolling) { if (at[FASTER] >= 0) lit[at[FASTER]] = 1; if (at[SLOWER] >= 0) lit[at[SLOWER]] = 1; }

    world_pose(vp_v3(0, 0, 0), 0, 0, 0);
    big_text("HOW TO PLAY", 3500, 2400, 260, 255, 255, 0);

    /* the panel, tilted to the eye */
    panel = vp_xf_mul2(vp_xf_translate(-27.0f, 0.0f, 150.0f), vp_xf_mul2(vp_xf_rot_x(-0.85f), vp_xf_scale(2.6f, 2.6f, 2.6f)));
    world_level(G.bright[BRIGHT_PLAYER]);
    world_rgb(150, 170, 150);
    wire_box(&panel, -15.0f, -1.2f, -7.0f, 15.0f, 0, 7.0f);
    /* the stick: a collar on the panel, the shaft, the ball - leaning the way it is held */
    foot = vp_xf_point(&panel, vp_v3(-9.0f, 0, 0));
    tip = vp_xf_point(&panel, vp_v3(-9.0f + 7.5f * sinf(lean * 0.45f), 7.5f * cosf(lean * 0.45f) * cosf(nod * 0.45f), 7.5f * sinf(nod * 0.45f)));
    world_rgb(120, 130, 170);
    for (j = 0; j < 8; j++) {
        float a0 = PI2 * (float)j / 8.0f, a1 = PI2 * (float)(j + 1) / 8.0f;
        world_line(vp_xf_point(&panel, vp_v3(-9.0f + 2.2f * cosf(a0), 0.1f, 2.2f * sinf(a0))),
                   vp_xf_point(&panel, vp_v3(-9.0f + 2.2f * cosf(a1), 0.1f, 2.2f * sinf(a1))));
    }
    disc_face(&panel, vp_v3(-9.0f, 0.1f, 0), vp_v3(1, 0, 0), vp_v3(0, 0, 1), 2.2f, 8);
    world_rgb(200, 200, 215);
    world_line(foot, tip);
    world_rgb(255, 90, 60);
    {
        /* the ball: three rings round the tip */
        const float r = 2.0f * 2.6f;
        for (k = 0; k < 3; k++) {
            vp_vec3 prev = vp_v3(0, 0, 0);
            for (j = 0; j <= 8; j++) {
                float an = PI2 * (float)j / 8.0f, c = r * cosf(an), n = r * sinf(an);
                vp_vec3 p = vp_v3_add(tip, k == 0 ? vp_v3(c, n, 0) : k == 1 ? vp_v3(c, 0, n) : vp_v3(0, c, n));
                if (j) world_line(prev, p);
                prev = p;
            }
        }
        if (world_solid()) {
            /* the ball: its three rings as discs, which is solid enough */
            vp_xf at = vp_xf_translate(tip.x, tip.y, tip.z);
            disc_face(&at, vp_v3(0, 0, 0), vp_v3(1, 0, 0), vp_v3(0, 1, 0), r, 8);
            disc_face(&at, vp_v3(0, 0, 0), vp_v3(1, 0, 0), vp_v3(0, 0, 1), r, 8);
            disc_face(&at, vp_v3(0, 0, 0), vp_v3(0, 1, 0), vp_v3(0, 0, 1), r, 8);
        }
    }
    /* the four buttons: a ring on the panel and a cap over it, down when it is the one shown */
    for (k = 0; k < 4; k++) {
        float x = 0.5f + 4.3f * (float)k, cap = lit[k] && ((G.frames / 10) & 1) ? 0.25f : 0.9f, sx, sy;
        char number[2] = { (char)('1' + k), 0 };
        world_level(G.bright[BRIGHT_PLAYER]);
        for (j = 0; j < 8; j++) {
            float a0 = PI2 * (float)j / 8.0f, a1 = PI2 * (float)(j + 1) / 8.0f;
            world_rgb(120, 130, 170);
            world_line(vp_xf_point(&panel, vp_v3(x + 1.6f * cosf(a0), 0.05f, 1.6f * sinf(a0))),
                       vp_xf_point(&panel, vp_v3(x + 1.6f * cosf(a1), 0.05f, 1.6f * sinf(a1))));
            if (lit[k]) world_rgb(255, 255, 255); else world_rgb(255, 210, 0);
            world_line(vp_xf_point(&panel, vp_v3(x + 1.2f * cosf(a0), cap, 1.2f * sinf(a0))),
                       vp_xf_point(&panel, vp_v3(x + 1.2f * cosf(a1), cap, 1.2f * sinf(a1))));
        }
        disc_face(&panel, vp_v3(x, cap, 0), vp_v3(1, 0, 0), vp_v3(0, 0, 1), 1.2f, 8);
        if (world_project(vp_xf_point(&panel, vp_v3(x, 0, -3.6f)), &sx, &sy))
            text(number, sx, sy - 40.0f, 80, VP_TEXT_CENTER, 150, 150, 150);
        /* what it does, over it, when it is the one shown; the second of two a line higher */
        if (lit[k] && world_project(vp_xf_point(&panel, vp_v3(x, 0.9f, 2.0f)), &sx, &sy)) {
            int what = step >= 0 ? step : at[FASTER] == k ? FASTER : SLOWER;
            float up = rolling && what == SLOWER ? 420.0f : 250.0f;
            world_level(G.bright[BRIGHT_TEXT]);
            world_rgb(255, 230, 0);
            world_bline(sx, sy + 60.0f, sx, sy + up - 20.0f);
            text(does[what], sx, sy + up, 110, VP_TEXT_CENTER, 255, 230, 0);
        }
    }

    /* the plane, seen from behind and a little above, doing what the stick does */
    {
        float s = 23.0f / model_radius(MODEL_PLAYER);
        float pitch = (G.invert_pitch ? nod : -nod) * 0.7f;
        plane = vp_xf_mul2(vp_xf_translate(42.0f, 6.0f, 150.0f),
                vp_xf_mul2(vp_xf_rot_x(0.3f),
                vp_xf_mul2(vp_xf_rot_y(t < T_PUSH ? lean * 0.35f : 0),
                vp_xf_mul2(vp_xf_rot_x(-pitch), vp_xf_mul2(vp_xf_rot_z(-lean * (rolling ? 0 : 0.7f) - roll_round), vp_xf_scale(s, s, s))))));
        world_level(G.bright[BRIGHT_PLAYER]);
        model_draw(MODEL_PLAYER, &plane, TINT_OWN);
        if (step == FASTER || step == SLOWER || rolling) draw_burner(&plane, step == FASTER ? 1.0f : step == SLOWER ? 0.12f : 0.6f);
        if (step == FIRE) howto_shots(&plane, t - T_BUTTONS);
    }

    /* in words */
    where[0] = 0;
    if (step >= 0) {
        if (at[step] >= 0) {
            snprintf(where, sizeof where, "BUTTON %d", at[step] + 1);
        } else {
            size_t i;
            for (i = 0; laid[step][i] && i + 1 < sizeof where; i++)
                where[i] = laid[step][i] == '_' ? ' ' : (char)toupper((unsigned char)laid[step][i]);
            where[i] = 0;
        }
    }
    if (t < T_PUSH) { line1 = "MOVE THE STICK LEFT AND RIGHT"; line2 = "THE PLANE TURNS WITH IT"; }
    else if (t < T_BUTTONS) { line1 = G.invert_pitch ? "UP TO CLIMB - DOWN TO DIVE" : "UP TO DIVE - DOWN TO CLIMB"; line2 = "ALL THE WAY ROUND MAKES A LOOP"; }
    else if (step >= 0) { line1 = does[step]; line2 = where; }
    else if (rolling) { line1 = "FASTER AND SLOWER TOGETHER"; line2 = "NOW THE STICK ROLLS THE PLANE"; }
    else { line1 = "START CHANGES THE VIEW"; line2 = "HUD ONLY, COCKPIT, FROM BEHIND"; }
    fit_text(line1, 1150, 3500, 170, 255, 255, 255);
    fit_text(line2, 930, 3500, 105, 0, 255, 190);
    if (step == FASTER) fit_text("LET GO AND THE SPEED GOES BACK TO THE MIDDLE", 760, 3300, 80, 0, 200, 150);
    if (step == REAR) fit_text("FOR AS LONG AS IT IS HELD", 760, 3300, 80, 0, 200, 150);
    (void)turn; (void)push;
    draw_coin_line();
}

void draw_initials(void)
{
    char one[2] = { 0, 0 }, buf[32];
    int i;

    {
        float ex, ey, dist = (float)G.frames * 1.5f;
        world_tunnel_centre(dist, 0.8f, &ex, &ey);
        world_draw_tunnel(dist, ex, ey, 0.8f, 0.35f, eras[G.era].colour, eras[(G.era + 1) % ERA_COUNT].colour, 0.5f, NULL, NULL, 0, 0);
    }
    big_text("GREAT FLYING", 3300, 2600, 240, 255, 255, 0);
    text(G.players == 2 ? (G.over_player ? "PLAYER 2 - ENTER YOUR INITIALS" : "PLAYER 1 - ENTER YOUR INITIALS")
                        : "ENTER YOUR INITIALS", CX, 2900, 130, VP_TEXT_CENTER, 255, 255, 255);
    snprintf(buf, sizeof buf, "%d", G.score);
    text(buf, CX, 2600, 160, VP_TEXT_CENTER, 0, 255, 0);
    for (i = 0; i < 3; i++) {
        float x = CX - 400 + 400.0f * (float)i;
        one[0] = i <= G.initial_pos ? G.initials[i] : ' ';
        if (i == G.initial_pos && ((G.frames / 10) & 1)) one[0] = ' ';
        text(one, x, 1900, 300, VP_TEXT_CENTER, 255, 255, 255);
        world_rgb(0, 200, 255);
        world_bline(x - 130, 1820, x + 130, 1820);
    }
    text("YOKE - LETTER    FIRE OR START - NEXT", CX, 1300, 90, VP_TEXT_CENTER, 0, 200, 0);
}
