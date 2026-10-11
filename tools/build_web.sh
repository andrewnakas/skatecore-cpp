#!/bin/sh
# Build the web demos. Needs Emscripten (em++) on the PATH.
#   site/            the full Skate 3 runtime (src/s3/** + web/s3/host.cpp -> skate3.js + skate3.wasm)
#   site/lite/       the small "lite" engine (src/*.cpp + web/host.cpp -> skatecore.js + skatecore.wasm)
#   site/s3/         redirect to the root (the old address of the full-runtime demo)
# The cleanroom pack and maps are fetched at run time from the cleanroom site; nothing from the pack is bundled.
set -e
cd "$(dirname "$0")/.."
OUT=${1:-site}
JOBS=${JOBS:-4}  # each em++ -O2 job of the game sources needs ~1 GB
mkdir -p "$OUT/lite" "$OUT/s3"

# Lite engine.
em++ -std=c++17 -O3 -fno-exceptions -fno-rtti -Iinclude src/*.cpp web/host.cpp \
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=SkateCore -sENVIRONMENT=web \
  -sALLOW_MEMORY_GROWTH=1 -sFILESYSTEM=0 \
  -sEXPORTED_FUNCTIONS=_malloc,_free,_demo_set_world,_demo_spawn,_demo_step,_demo_pose,_demo_poll_event,_demo_event_name,_demo_event_points,_demo_event_value,_demo_current_trick,_demo_bail,_demo_rail_count,_demo_rail_points \
  -sEXPORTED_RUNTIME_METHODS=HEAPF32,HEAP32,UTF8ToString \
  -o "$OUT/lite/skatecore.js"
cp web/index.html web/main.js web/park.js web/style.css "$OUT/lite/"

# Full runtime: ~690 translation units, compiled in parallel by mk/web.mk (objects cached in build/wasm_obj;
# set EM_COMPILER_WRAPPER=ccache to share them across clean checkouts).
start=$(date +%s)
make -j"$JOBS" --no-print-directory s3-wasm S3_WASM_OUT="$OUT"
echo "s3 wasm: $(( $(date +%s) - start )) s with -j$JOBS"
cp web/s3/index.html web/s3/main.js web/s3/style.css "$OUT/"
cat > "$OUT/s3/index.html" <<'HTML'
<!doctype html>
<meta charset="utf-8">
<title>skatecore</title>
<meta http-equiv="refresh" content="0; url=../">
<script>location.replace('../' + location.search + location.hash);</script>
<p><a href="../">skatecore moved to the site root</a></p>
HTML
rm -f "$OUT/s3/skate3.js" "$OUT/s3/skate3.wasm" "$OUT/s3/main.js" "$OUT/s3/style.css" "$OUT/skatecore.js" "$OUT/skatecore.wasm" "$OUT/park.js"
touch "$OUT/.nojekyll"
ls -la "$OUT" "$OUT/lite"
