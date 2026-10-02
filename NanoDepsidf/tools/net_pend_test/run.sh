#!/bin/sh
# Builds and runs the WiFi handshake-pool host test with the Mac's compiler.
set -e
cd "$(dirname "$0")/../.."
OUT="${TMPDIR:-/tmp}/net_pend_test"
cc -std=gnu11 -Wall -Werror -O1 -g -fsanitize=undefined -I src tools/net_pend_test/test.c -o "$OUT"
"$OUT"
