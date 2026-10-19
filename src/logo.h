/* logo.h - the game's name as on its marquee: "TimePilot" in fat letters,
 * yellow above and red below, and under it a blue "3D" with depth between
 * striped wings. */
#ifndef TP3D_LOGO_H
#define TP3D_LOGO_H

/* Drawn in board units, centred on `cx`, the top of the capitals at
 * `top`, `width` wide; `bright` 0..1. */
void logo_draw(float cx, float top, float width, float bright);

#endif /* TP3D_LOGO_H */
