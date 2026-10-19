/* hud.c - the head-up display, in board units (0..4095, y up).
 *
 * Every stroke costs the beam, and this is drawn in every frame: numbers
 * stand without labels, scales have few ticks, and what is not needed in
 * the chase view is left out there.
 */
#include "hud.h"
#include "world.h"

#include <math.h>
#include <stdio.h>

#define CX 2048.0f
#define CY 2048.0f
#define DEG (3.14159265f / 180.0f)

/* the display's own green, its warnings' red */
static void green(void) { world_rgb(0, 255, 120); }
static void red(void)   { world_rgb(255, 60, 40); }

/* The pitch ladder: a rung every ten degrees, each where that angle above
 * or below the horizon really is ahead of the plane, so the ladder tilts
 * and slides as the world does. Only the rungs inside the glass are drawn. */
static void ladder(const hud_state *s)
{
    int deg;
    for (deg = -80; deg <= 80; deg += 10) {
        float a = (float)deg * DEG;
        float cx, cy, rx, ry, dx, dy, len, half, gap;
        vp_vec3 c = vp_v3_add(s->eye, vp_v3_scale(world_forward(s->heading, a), 1000.0f));
        vp_vec3 r = vp_v3_add(s->eye, vp_v3_scale(world_forward(s->heading + 0.05f, a), 1000.0f));

        if (fabsf(a - s->pitch) > 0.42f) continue;              /* far outside the glass */
        if (!world_project(c, &cx, &cy) || !world_project(r, &rx, &ry)) continue;
        if (fabsf(cx - CX) > 620.0f || fabsf(cy - CY) > 880.0f) continue;
        dx = rx - cx; dy = ry - cy;
        len = sqrtf(dx * dx + dy * dy);
        if (len < 1.0f) continue;
        dx /= len; dy /= len;
        half = deg == 0 ? 560.0f : 300.0f;
        gap  = deg == 0 ? 170.0f : 120.0f;
        world_bline(cx - dx * half, cy - dy * half, cx - dx * gap, cy - dy * gap);
        world_bline(cx + dx * gap, cy + dy * gap, cx + dx * half, cy + dy * half);
        if (deg != 0) {     /* its ends point at the horizon */
            float sgn = deg > 0 ? -1.0f : 1.0f;
            world_bline(cx + dx * half, cy + dy * half, cx + dx * half - dy * 45.0f * sgn, cy + dy * half + dx * 45.0f * sgn);
        }
    }
}

/* The heading: ticks every ten degrees sliding past a mark, the four
 * winds by their letters. */
static void heading_tape(const hud_state *s)
{
    const float y = 3080.0f, per_deg = 15.0f;
    float hdg = s->heading / DEG;
    int t;

    while (hdg < 0) hdg += 360.0f;
    while (hdg >= 360.0f) hdg -= 360.0f;
    for (t = 0; t < 36; t++) {
        float d = (float)(t * 10) - hdg, x;
        if (d > 180.0f) d -= 360.0f;
        if (d < -180.0f) d += 360.0f;
        if (fabsf(d) > 34.0f) continue;
        x = CX + d * per_deg;
        if (t % 9 == 0) {
            static const char *const wind[4] = { "N", "E", "S", "W" };
            world_text(wind[t / 9], x, y + 20.0f, 70.0f, VP_TEXT_CENTER);
        } else {
            world_bline(x, y, x, y + (t % 3 == 0 ? 60.0f : 30.0f));
        }
    }
    world_bline(CX - 35.0f, y - 70.0f, CX, y - 20.0f);
    world_bline(CX, y - 20.0f, CX + 35.0f, y - 70.0f);
}

/* A number in a pointed box: the speed points right, the height left. */
static void boxed(float x, float y, int value, int point_right)
{
    char buf[16];
    float w = 330.0f, h = 60.0f, tip = point_right ? x + w / 2.0f + 50.0f : x - w / 2.0f - 50.0f;
    float near_x = point_right ? x + w / 2.0f : x - w / 2.0f, far_x = point_right ? x - w / 2.0f : x + w / 2.0f;

    snprintf(buf, sizeof buf, "%d", value);
    world_bline(tip, y, near_x, y + h);
    world_bline(near_x, y + h, far_x, y + h);
    world_bline(far_x, y + h, far_x, y - h);
    world_bline(far_x, y - h, near_x, y - h);
    world_bline(near_x, y - h, tip, y);
    world_text(buf, x, y - 38.0f, 76.0f, VP_TEXT_CENTER);
}

/* An upright bar with a mark where the value is. */
static void gauge(float x, float y0, float y1, float value)
{
    float y = y0 + (y1 - y0) * (value < 0 ? 0 : value > 1.0f ? 1.0f : value);
    world_bline(x, y0, x, y1);
    world_bline(x - 30.0f, y0, x + 30.0f, y0);
    world_bline(x - 30.0f, y1, x + 30.0f, y1);
    world_bline(x - 55.0f, y + 35.0f, x, y);
    world_bline(x, y, x - 55.0f, y - 35.0f);
}

/* The energy left: a frame, and as much of it filled as is left; green,
 * then yellow, then red and blinking. */
static void energy_bar(const hud_state *s, float y)
{
    const float x0 = CX - 520.0f, x1 = CX + 520.0f, h = 34.0f;
    float e = s->energy < 0 ? 0 : s->energy > 1.0f ? 1.0f : s->energy;
    float fill = x0 + (x1 - x0) * e;
    int i;

    world_rgb(70, 110, 140);
    world_bline(x0, y - h, x1, y - h);
    world_bline(x1, y - h, x1, y + h);
    world_bline(x1, y + h, x0, y + h);
    world_bline(x0, y + h, x0, y - h);
    if (e <= 0) return;
    if (e < 0.25f) { if ((s->frames / 8) & 1) return; red(); }
    else if (e < 0.5f) world_rgb(255, 220, 0);
    else green();
    for (i = -1; i <= 1; i++)       /* three strokes fill it */
        world_bline(x0 + 8.0f, y + (float)i * 14.0f, fill - 8.0f > x0 + 8.0f ? fill - 8.0f : x0 + 8.0f, y + (float)i * 14.0f);
}

static void target_box(const hud_state *s)
{
    float x = s->target_x, y = s->target_y;
    float r = 9000.0f / (s->target_range < 60.0f ? 60.0f : s->target_range);
    char buf[16];

    if (r < 55.0f) r = 55.0f;
    if (r > 170.0f) r = 170.0f;
    if (s->locked) {            /* the guns will follow it: a diamond */
        red();
        world_bline(x - r, y, x, y + r);
        world_bline(x, y + r, x + r, y);
        world_bline(x + r, y, x, y - r);
        world_bline(x, y - r, x - r, y);
    } else {                    /* corners only */
        float c = r * 0.45f;
        world_rgb(255, 220, 0);
        world_bline(x - r, y + r - c, x - r, y + r); world_bline(x - r, y + r, x - r + c, y + r);
        world_bline(x + r - c, y + r, x + r, y + r); world_bline(x + r, y + r, x + r, y + r - c);
        world_bline(x + r, y - r + c, x + r, y - r); world_bline(x + r, y - r, x + r - c, y - r);
        world_bline(x - r + c, y - r, x - r, y - r); world_bline(x - r, y - r, x - r, y - r + c);
    }
    snprintf(buf, sizeof buf, "%d", (int)s->target_range);
    world_text(buf, x, y - r - 90.0f, 55.0f, VP_TEXT_CENTER);
}

/* Which way the nearest enemy is, when none is to be seen. */
static void arrow(const hud_state *s)
{
    float c = cosf(s->arrow_angle), n = sinf(s->arrow_angle);
    float x = CX + c * 640.0f, y = CY + n * 640.0f;
    world_rgb(255, 220, 0);
    world_bline(x - c * 70.0f - n * 45.0f, y - n * 70.0f + c * 45.0f, x, y);
    world_bline(x, y, x - c * 70.0f + n * 45.0f, y - n * 70.0f - c * 45.0f);
}

/* The boss is always pointed at: in sight, red corners round it; out of
 * sight, a big red arrow on a ring round the middle, with its name. */
static void boss_mark(const hud_state *s)
{
    red();
    if (s->boss_seen) {
        const float r = 210.0f, k = 70.0f;
        float x = s->boss_x, y = s->boss_y;
        world_bline(x - r, y + r - k, x - r, y + r); world_bline(x - r, y + r, x - r + k, y + r);
        world_bline(x + r - k, y + r, x + r, y + r); world_bline(x + r, y + r, x + r, y + r - k);
        world_bline(x + r, y - r + k, x + r, y - r); world_bline(x + r, y - r, x + r - k, y - r);
        world_bline(x - r + k, y - r, x - r, y - r); world_bline(x - r, y - r, x - r, y - r + k);
    } else {
        float c = cosf(s->boss_angle), n = sinf(s->boss_angle);
        float x = CX + c * 820.0f, y = CY + n * 820.0f;
        world_bline(x - c * 130.0f - n * 85.0f, y - n * 130.0f + c * 85.0f, x, y);
        world_bline(x, y, x - c * 130.0f + n * 85.0f, y - n * 130.0f - c * 85.0f);
        world_bline(x - c * 130.0f - n * 85.0f, y - n * 130.0f + c * 85.0f, x - c * 130.0f + n * 85.0f, y - n * 130.0f - c * 85.0f);
        world_text("BOSS", CX + c * 620.0f, CY + n * 620.0f - 35.0f, 70.0f, VP_TEXT_CENTER);
    }
}

/* A laser has the plane: the word, quiet while it takes aim; locked on,
 * big and blinking fast with what to do, and a box closing on the middle. */
static void laser_warning(const hud_state *s)
{
    if (s->laser == 1) {
        world_rgb(170, 60, 40);
        world_text("LASER AIMING", CX, 3050.0f, 90.0f, VP_TEXT_CENTER);
        return;
    }
    if ((s->frames / 3u) & 1u) return;
    red();
    world_text("LASER LOCK", CX, 2560.0f, 170.0f, VP_TEXT_CENTER);
    world_text("BREAK AWAY", CX, 1120.0f, 150.0f, VP_TEXT_CENTER);
    {
        float r = 330.0f, k = 110.0f;
        world_bline(CX - r, CY + r - k, CX - r, CY + r); world_bline(CX - r, CY + r, CX - r + k, CY + r);
        world_bline(CX + r - k, CY + r, CX + r, CY + r); world_bline(CX + r, CY + r, CX + r, CY + r - k);
        world_bline(CX + r, CY - r + k, CX + r, CY - r); world_bline(CX + r, CY - r, CX + r - k, CY - r);
        world_bline(CX - r + k, CY - r, CX - r, CY - r); world_bline(CX - r, CY - r, CX - r, CY - r + k);
    }
}

/* A missile is after the plane: a red double arrow on a ring round the
 * middle shows where it comes from, its distance beside it, and the word
 * over the display. All of it blinks, faster the nearer it is. */
static void missile_warning(const hud_state *s)
{
    float c = cosf(s->missile_angle), n = sinf(s->missile_angle);
    int period = s->missile_range < 150.0f ? 3 : s->missile_range < 350.0f ? 5 : 8, k;
    char buf[16];

    if ((s->frames / (unsigned)period) & 1) return;
    red();
    for (k = 0; k < 2; k++) {
        float r = 520.0f + 80.0f * (float)k, x = CX + c * r, y = CY + n * r;
        world_bline(x - c * 80.0f - n * 60.0f, y - n * 80.0f + c * 60.0f, x, y);
        world_bline(x, y, x - c * 80.0f + n * 60.0f, y - n * 80.0f - c * 60.0f);
    }
    snprintf(buf, sizeof buf, "%d", (int)s->missile_range);
    world_text(buf, CX + c * 400.0f, CY + n * 400.0f - 30.0f, 60.0f, VP_TEXT_CENTER);
    world_text("MISSILE", CX, 2880.0f, 130.0f, VP_TEXT_CENTER);
}

/* The cockpit: the coaming with its hump, the frame of the display's
 * glass, and the canopy's bow at both sides. */
static void cockpit(void)
{
    world_rgb(0, 150, 220);
    world_bline(0, 560, 820, 900);
    world_bline(820, 900, 1400, 1000);
    world_bline(1400, 1000, 2695, 1000);
    world_bline(2695, 1000, 3275, 900);
    world_bline(3275, 900, 4095, 560);
    /* the glass */
    world_rgb(0, 90, 150);
    world_bline(1330, 1000, 1200, 3260);
    world_bline(1200, 3260, 2895, 3260);
    world_bline(2895, 3260, 2765, 1000);
    /* the canopy */
    world_bline(0, 2300, 420, 3500);
    world_bline(420, 3500, 1250, 4095);
    world_bline(4095, 2300, 3675, 3500);
    world_bline(3675, 3500, 2845, 4095);
}

/* Looking back: the picture in a mirror's frame, with its corners cut. */
static void mirror(const hud_state *s)
{
    world_rgb(0, 150, 220);
    world_bline(500, 900, 300, 1200);
    world_bline(300, 1200, 300, 3000);
    world_bline(300, 3000, 500, 3300);
    world_bline(500, 3300, 3595, 3300);
    world_bline(3595, 3300, 3795, 3000);
    world_bline(3795, 3000, 3795, 1200);
    world_bline(3795, 1200, 3595, 900);
    world_bline(3595, 900, 500, 900);
    green();
    world_text("REAR VIEW", CX, 3380.0f, 90.0f, VP_TEXT_CENTER);
    energy_bar(s, s->energy_y);
}

void hud_draw(const hud_state *s)
{
    world_level(s->bright);
    if (s->rear) {
        mirror(s);
    } else if (s->cockpit) {
        if (s->frame) cockpit();
        green();
        /* a hit: the display breaks up for a moment, every other frame gone */
        if (!(s->hurt > 0 && (s->frames & 1))) {
            ladder(s);
            heading_tape(s);
        }
        /* the plane's own mark, and the guns' */
        world_bline(CX - 150.0f, CY, CX - 55.0f, CY);
        world_bline(CX - 55.0f, CY, CX, CY - 55.0f);
        world_bline(CX, CY - 55.0f, CX + 55.0f, CY);
        world_bline(CX + 55.0f, CY, CX + 150.0f, CY);
        if (s->locked) red();
        world_bcircle(CX, CY, 70.0f, 8);
    }
    if (!s->rear) {
        /* round the picture the short way for the beam: the height on the
         * right, the energy below, then the throttle and the speed on the left */
        green();
        boxed(s->cockpit ? 3275.0f : 3575.0f, s->cockpit ? CY : 1500.0f, (int)s->altitude, 0);
        energy_bar(s, s->energy_y);
        green();
        gauge(s->cockpit ? 480.0f : 200.0f, s->cockpit ? 1500.0f : 900.0f, s->cockpit ? 2600.0f : 2100.0f, s->throttle);
        if (s->burner) {            /* lit: said over the throttle, in the flame's colour */
            world_rgb(255, 150, 0);
            world_text("A/B", s->cockpit ? 480.0f : 200.0f, s->cockpit ? 2680.0f : 2180.0f, 70.0f, VP_TEXT_CENTER);
        }
        green();
        boxed(s->cockpit ? 820.0f : 520.0f, s->cockpit ? CY : 1500.0f, (int)(s->speed * 3.6f), 1);

        if (s->target) target_box(s);
        else if (s->arrow) arrow(s);
    }
    if (s->boss) boss_mark(s);
    if (s->laser) laser_warning(s);
    if (s->missile) missile_warning(s);

    world_level(s->bright_text);
    if ((s->frames / 10) & 1) {
        red();
        if (s->stalled) world_text("STALL", CX, 2620.0f, 170.0f, VP_TEXT_CENTER);
        else if (s->pull_up) world_text("PULL UP", CX, 2620.0f, 170.0f, VP_TEXT_CENTER);
        else if (s->energy < 0.25f) world_text("LOW ENERGY", CX, 1400.0f, 110.0f, VP_TEXT_CENTER);
    }
    if (s->hurt > 0) {          /* a hit: the edge of the picture flashes */
        red();
        world_bline(40, 40, 4055, 40);
        world_bline(4055, 40, 4055, 4055);
        world_bline(4055, 4055, 40, 4055);
        world_bline(40, 4055, 40, 40);
    }
}
