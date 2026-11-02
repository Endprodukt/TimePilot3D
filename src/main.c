/* Time Pilot 3D for VectorPie: Konami's Time Pilot (1982) as a 3D vector
 * game flown with a yoke, from the cockpit or from behind the plane.
 *
 * Controls, by their advmame.rc names:
 *   p1_stickx / p1_sticky   the yoke: turn, and climb or dive
 *   p1_left / p1_right / p1_up / p1_down   the same on keys or a stick
 *   p1_button1              fire (held: keeps firing)
 *   p1_button2              faster (held: the throttle opens)
 *   p1_button3              slower (held: the throttle closes)
 *   p1_button4              look back (while held)
 *   coin1 / coin2           a credit
 *   start1                  start; in a game: cockpit / chase view
 *   start2                  start for two players taking turns (two credits)
 *   F2 / Tab                the service menu (settings; more in CONTROLS, MENU BUTTON);
 *                           closed again, the game begins anew from its title
 */
#include <vectorpie/vp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"

static void update(vp_game *g, float dt) { game_update(g, dt); }
static void draw(vp_game *g, vp_dvg *d)  { game_draw(g, d); }

int main(int argc, char **argv)
{
    vp_game_desc desc = {0};
    const char *env = getenv("VP_VERTICAL");
    int vertical = env && !strcmp(env, "1");
    int i;

    /* The VectorPie menu starts a game as <name>_dvg or <name>_sdl. */
    if (!getenv("VP_OUTPUT") && argc > 0) {
        size_t n = strlen(argv[0]);
        const char *output = n >= 4 && !strcmp(argv[0] + n - 4, "_sdl") ? "sdl"
                           : n >= 4 && !strcmp(argv[0] + n - 4, "_dvg") ? "dvg" : NULL;
#ifdef _WIN32
        if (output) _putenv_s("VP_OUTPUT", output);
#else
        if (output) setenv("VP_OUTPUT", output, 1);
#endif
    }
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--vertical")) vertical = 1;
    game_init(vertical);
    env = getenv("TP3D_LEVEL");         /* a testing aid: start at this era (1-based) */
    if (env) game_start_level(atoi(env));
    env = getenv("TP3D_KILLS");         /* a testing aid: start each level this far in */
    if (env) game_cheat_kills(atoi(env));
    if (getenv("TP3D_ITEMS")) game_cheat_items();           /* a testing aid: see game.h */
    if (getenv("TP3D_SERVICE")) game_cheat_service();      /* a testing aid: see game.h */
    if (getenv("TP3D_CONTINUE")) game_cheat_continue();    /* a testing aid: see game.h */
    if (getenv("TP3D_SETUP")) game_cheat_setup(atoi(getenv("TP3D_SETUP")));          /* a testing aid: see game.h */
    if (getenv("TP3D_BOSS2")) game_cheat_boss2(atoi(getenv("TP3D_BOSS2")));          /* a testing aid: see game.h */
    if (getenv("TP3D_DEBUGSEL")) game_cheat_debugsel(atoi(getenv("TP3D_DEBUGSEL")));    /* a testing aid: see game.h */
    if (getenv("TP3D_WEAK")) game_cheat_weak();            /* a testing aid: see game.h */
    if (getenv("TP3D_CHUTES")) game_cheat_chutes();        /* a testing aid: see game.h */
    if (getenv("TP3D_STUNT")) game_cheat_stunt();          /* a testing aid: see game.h */
    if (getenv("TP3D_HANDSOFF")) game_tunnel_hands_off();   /* a testing aid: see game.h */
    env = getenv("TP3D_PAGE");          /* a testing aid: the attract page to begin on */
    if (env) game_attract_page(atoi(env));
    env = getenv("TP3D_SELFTEST");      /* "steps[,levels]": see game_selftest */
    if (env) {
        int steps = 1, levels = 5;
        sscanf(env, "%d,%d", &steps, &levels);
        game_selftest(steps, levels);
    }

    desc.title          = "Time Pilot 3D";
    desc.update         = update;
    desc.draw           = draw;
    desc.update_hz      = 60;
    desc.frame_hz       = 60;
    desc.frame_hz_board = 40;
    return vp_run(&desc, argc, argv);
}
