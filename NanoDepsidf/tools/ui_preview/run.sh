#!/bin/sh
# Renders every Pixel UI screen from the firmware's own drawing code into a PNG contact sheet.
#   tools/ui_preview/run.sh [out.png]     (default: tools/ui_preview/out/preview.png)
# Needs a host C++ compiler and the tools venv (Pillow) for the PPM -> PNG step.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src"
OUT=${1:-"$HERE/out/preview.png"}
BUILD="$HERE/out/build"
mkdir -p "$BUILD/fonts" "$(dirname "$OUT")"

# Copy (not include-path) the sources: a quoted #include resolves next to the including file
# first, which would pick up the real lgfx_config.hpp from src/ instead of the stub.
cp "$SRC"/ui_gfx.cpp "$SRC"/ui_gfx.hpp "$SRC"/ui_screens.cpp "$SRC"/ui_screens.hpp \
   "$SRC"/ui_fx.cpp "$SRC"/ui_fx.hpp "$SRC"/ui_shape.cpp "$SRC"/ui_shape.hpp "$SRC"/menu.h "$SRC"/pd_status.h "$SRC"/sysmon.h "$SRC"/ui_state.h "$SRC"/haptic_params.h \
   "$SRC"/audio_trigger.h "$SRC"/boot_mode.h "$BUILD/"
cp "$SRC"/fonts/*.cpp "$SRC"/fonts/*.h "$BUILD/fonts/"
mkdir -p "$BUILD/icons"
cp "$SRC"/app_profiles/icons/*.c "$SRC"/app_profiles/icons/*.h "$BUILD/icons/"
cp "$HERE"/stub/*.hpp "$BUILD/"
mkdir -p "$BUILD/class/hid" "$BUILD/app_profiles/icons"
cp "$HERE"/stub/class/hid/hid.h "$BUILD/class/hid/"
cp "$SRC"/ui_cards.cpp "$SRC"/ui_cards.hpp "$BUILD/"
cp "$SRC"/app_colors.c "$SRC"/app_colors.h "$BUILD/"
cc -std=gnu11 -O2 -I"$BUILD" -c -o "$BUILD/app_colors.o" "$BUILD/app_colors.c"
cp "$SRC"/app_profiles/app_profile.h "$SRC"/app_profiles/figma.c "$SRC"/app_profiles/plasticity.c "$SRC"/app_profiles/onshape.c "$SRC"/app_profiles/agents.c "$BUILD/app_profiles/"
cp "$SRC"/app_profiles/icons/app_icons.h "$BUILD/app_profiles/icons/"
# Profile data is C (compound literals), so it builds as C and links in.
cc -std=gnu11 -O2 -I"$BUILD" -c -o "$BUILD/figma.o" "$BUILD/app_profiles/figma.c"
cc -std=gnu11 -O2 -I"$BUILD" -c -o "$BUILD/plasticity.o" "$BUILD/app_profiles/plasticity.c"
cc -std=gnu11 -O2 -I"$BUILD" -c -o "$BUILD/onshape.o" "$BUILD/app_profiles/onshape.c"
cc -std=gnu11 -O2 -I"$BUILD" -c -o "$BUILD/agents.o" "$BUILD/app_profiles/agents.c"

# The Figma test icon, converted exactly as send_icon.py sends it, for the "uploaded icon" tile.
"$HERE/../.venv/bin/python" -c "
import sys; sys.path.insert(0, sys.argv[1])
from send_icon import fit_icon, to_rgb565_be
from PIL import Image
open(sys.argv[3], 'wb').write(to_rgb565_be(fit_icon(Image.open(sys.argv[2]))))" \
    "$HERE/.." "$HERE/../icons/figma_pixel_48.png" "$BUILD/icon.raw"

c++ -std=c++17 -O2 -I"$BUILD" -o "$BUILD/preview" "$HERE/preview.cpp" \
    "$BUILD"/ui_gfx.cpp "$BUILD"/ui_screens.cpp "$BUILD"/ui_fx.cpp "$BUILD"/ui_shape.cpp "$BUILD"/ui_cards.cpp "$BUILD"/fonts/*.cpp "$BUILD/figma.o" "$BUILD/plasticity.o" "$BUILD/onshape.o" "$BUILD/agents.o" "$BUILD/app_colors.o" \
    -x c++ "$BUILD"/icons/*.c
ICON_RAW="$BUILD/icon.raw" "$BUILD/preview" > "$BUILD/preview.ppm"
"$HERE/../.venv/bin/python" -c "from PIL import Image; import sys; Image.open(sys.argv[1]).save(sys.argv[2])" \
    "$BUILD/preview.ppm" "$OUT"
echo "wrote $OUT"
