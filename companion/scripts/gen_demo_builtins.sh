#!/bin/sh
# The demo knob's built-in profiles (src/demo_builtins.json): the firmware's own, icons included,
# as profile_json.c writes them, in the registry's order (app_profiles.c). Rerun after a
# built-in profile or icon changes, then retake the screenshots.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
FW="$HERE/../../NanoDepsidf"
TMP=$(mktemp -d)
"$FW/tools/profile_json_test/run.sh" "$TMP" > /dev/null
ORDER=$(sed -n '/app_profile_t \*const/,/};/p' "$FW/src/app_profiles/app_profiles.c" | grep -o '&app_profile_[a-z0-9_]*' | sed 's/&app_profile_//')
python3 - "$TMP" "$HERE/../src/demo_builtins.json" $ORDER <<'EOF'
import json, sys
tmp, out, ids = sys.argv[1], sys.argv[2], sys.argv[3:]
json.dump([json.load(open(f"{tmp}/{i}.json")) for i in ids], open(out, "w"), separators=(",", ":"))
print(f"{out}: {', '.join(ids)}")
EOF
rm -rf "$TMP"
