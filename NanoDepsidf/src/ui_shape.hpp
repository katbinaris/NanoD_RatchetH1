#pragma once
// APP-mode micro-interaction: a small 3D shape in the Main Screen's middle band that follows
// the knob -- nested copies for zoom (an endless loop), a one-for-one turn for orbit, a
// wrapping row over a scrolling floor for pan, an amber flash for a tap. Direct port of the
// approved preview (https://claude.ai/artifact/9x5URCUiMgJPqHDDtFeZWj, round 3 "Selected
// face", isometric 2:1): same projection, same scenes, same numbers, so the device matches it.
// Stateless: display_task.cpp owns yaw / zoom / pan and the settle / flash timing.

#include <stdint.h>

namespace ui {

enum ShapeKind : uint8_t { SHAPE_CUBE = 0, SHAPE_PYRAMID, SHAPE_OCTA };
enum ShapeScene : uint8_t { SCENE_ZOOM = 0, SCENE_ORBIT, SCENE_PAN };
// R3C: grey edges, sparse-filled top face with white edges, faint hidden dashes.
// R2C: white edges, dashed hidden edges, hollow corner grips, checkered nearest face.
// R4A: R2C reworked against 1px glitches -- an even 2px pen, solid dark hidden edges, a plain
// dark nearest face (nothing screen-fixed to shimmer), 6x6 grips.
enum ShapeStyle : uint8_t { STYLE_SELECTED_FACE = 0, STYLE_CAD_GRIPS, STYLE_THICK };

struct ShapeView {
    ShapeKind shape;
    ShapeStyle style;
    ShapeScene scene;
    float yaw;   // radians; 45 deg + k*90 deg is the clean 2:1 isometric pose
    float zoom;  // nested-copy phase: +1 = one doubling
    float pan;   // px of floor travel
    bool flash;  // amber flash (a tap action)
};

// Draws the scene into the band y 55..127 (clipped there).
void shape_scene(const ShapeView &v);

} // namespace ui
