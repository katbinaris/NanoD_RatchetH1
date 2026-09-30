#!/bin/sh
# Builds and runs the profile JSON host test with the Mac's compiler. Needs managed_components/
# (any firmware build fetches it). Optional: a folder to write each built-in's JSON into.
set -e
cd "$(dirname "$0")/../.."
OUT="${TMPDIR:-/tmp}/profile_json_test"
cc -std=gnu11 -Wall -Wno-unused-function -O1 -g -fsanitize=undefined -Wno-deprecated-declarations \
    -I src -I managed_components/espressif__cjson/cJSON \
    -I managed_components/espressif__tinyusb/src -I tools/profile_json_test \
    tools/profile_json_test/test.c src/app_profiles/*.c src/app_profiles/icons/*.c \
    managed_components/espressif__cjson/cJSON/cJSON.c -o "$OUT"
"$OUT" "$@"
