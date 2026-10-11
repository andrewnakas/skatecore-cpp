#!/bin/sh
# Build the web demo: the skate library + web/host.cpp -> site/skatecore.js + skatecore.wasm,
# and copy the page. Needs Emscripten (em++) on the PATH.
set -e
cd "$(dirname "$0")/.."
OUT=${1:-site}
mkdir -p "$OUT"
em++ -std=c++17 -O3 -fno-exceptions -fno-rtti -Iinclude src/*.cpp web/host.cpp \
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=SkateCore -sENVIRONMENT=web \
  -sALLOW_MEMORY_GROWTH=1 -sFILESYSTEM=0 \
  -sEXPORTED_FUNCTIONS=_malloc,_free,_demo_set_world,_demo_spawn,_demo_step,_demo_pose,_demo_poll_event,_demo_event_name,_demo_event_points,_demo_event_value,_demo_current_trick,_demo_bail,_demo_rail_count,_demo_rail_points \
  -sEXPORTED_RUNTIME_METHODS=HEAPF32,HEAP32,UTF8ToString \
  -o "$OUT/skatecore.js"
cp web/index.html web/main.js web/park.js web/style.css "$OUT/"
touch "$OUT/.nojekyll"
ls -la "$OUT"
