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

# Full-runtime demo (stage W1+): the s3::data loaders + web/s3/host.cpp -> site/s3/skate3.js + skate3.wasm.
# The pack and maps are fetched at run time from the cleanroom site; nothing from the pack is bundled.
S3_EXPORTS=_malloc,_free,_s3_buffer,_s3_load_pack,_s3_add_file,_s3_error,_s3_file_count,_s3_read_file,\
_s3_map_load,_s3_map_name,_s3_map_version,_s3_map_vertex_count,_s3_map_index_count,_s3_map_positions,\
_s3_map_normals,_s3_map_uvs,_s3_map_uv1s,_s3_map_indices,_s3_map_group_count,_s3_map_groups,_s3_map_material_count,\
_s3_map_material_stride,_s3_map_materials,_s3_map_material_name,_s3_map_texture_count,_s3_map_texture_name,\
_s3_map_texture_info,_s3_map_texture_rgba,_s3_map_spawn,_s3_map_spawn_moved,_s3_map_environment_count,\
_s3_map_environment,_s3_map_light_count,_s3_map_light_stride,_s3_map_lights,_s3_map_rail_count,\
_s3_map_collision_count,_s3_rig_load,_s3_rig_bone_name,_s3_rig_bone_parent
mkdir -p "$OUT/s3"
em++ -std=c++17 -O2 -ffp-contract=off -fexceptions -Isrc src/s3/data/*.cpp web/s3/host.cpp \
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=Skate3 -sENVIRONMENT=web \
  -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=2GB -sINITIAL_MEMORY=64MB -sFILESYSTEM=0 \
  -sEXPORTED_FUNCTIONS=$S3_EXPORTS \
  -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPU32,HEAPF32,UTF8ToString,stringToUTF8,lengthBytesUTF8 \
  -o "$OUT/s3/skate3.js"
cp web/s3/index.html web/s3/main.js web/s3/style.css "$OUT/s3/"
ls -la "$OUT" "$OUT/s3"
