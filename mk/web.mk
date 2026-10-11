# Stage W2: the full runtime (src/s3/core, data, game) + web/s3/host.cpp in WebAssembly.
# tools/build_web.sh runs `make -j<cpus> s3-wasm` (objects in build/wasm_obj, -MMD tracked).
S3_WASM_CXX ?= em++
S3_WASM_FLAGS ?= -std=c++17 -O2 -ffp-contract=off -fexceptions -Isrc
S3_WASM_OBJDIR ?= build/wasm_obj
S3_WASM_OUT ?= site
S3_WASM_SRC := $(sort $(shell find src/s3/core src/s3/game -name '*.cpp' 2>/dev/null) $(wildcard src/s3/data/*.cpp)) \
	web/s3/host.cpp
S3_WASM_OBJ := $(patsubst %.cpp,$(S3_WASM_OBJDIR)/%.o,$(S3_WASM_SRC))
S3_WASM_EXPORT_LIST := _malloc _free _s3_buffer _s3_load_pack _s3_add_file _s3_error _s3_file_count _s3_read_file \
	_s3_map_load _s3_map_name _s3_map_version _s3_map_vertex_count _s3_map_index_count _s3_map_positions \
	_s3_map_normals _s3_map_uvs _s3_map_uv1s _s3_map_indices _s3_map_group_count _s3_map_groups \
	_s3_map_material_count _s3_map_material_stride _s3_map_materials _s3_map_material_name \
	_s3_map_texture_count _s3_map_texture_name _s3_map_texture_info _s3_map_texture_rgba _s3_map_spawn \
	_s3_map_spawn_moved _s3_map_environment_count _s3_map_environment _s3_map_light_count \
	_s3_map_light_stride _s3_map_lights _s3_map_rail_count _s3_map_collision_count _s3_rig_load \
	_s3_rig_bone_name _s3_rig_bone_parent _s3_sim_load _s3_sim_step _s3_sim_set_aspect _s3_sim_time_step \
	_s3_sim_view _s3_sim_bone_count _s3_sim_bones _s3_sim_board _s3_sim_state_name _s3_sim_motion_state \
	_s3_sim_action_state _s3_sim_clip _s3_sim_trick_name _s3_sim_trick_identifier _s3_sim_score_name \
	_s3_sim_events
S3_WASM_COMMA := ,
S3_WASM_EMPTY :=
S3_WASM_SPACE := $(S3_WASM_EMPTY) $(S3_WASM_EMPTY)
S3_WASM_EXPORTS = $(subst $(S3_WASM_SPACE),$(S3_WASM_COMMA),$(strip $(S3_WASM_EXPORT_LIST)))

$(S3_WASM_OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(S3_WASM_CXX) $(S3_WASM_FLAGS) -MMD -MP -c $< -o $@

-include $(S3_WASM_OBJ:.o=.d)

$(S3_WASM_OUT)/skate3.js: $(S3_WASM_OBJ)
	@mkdir -p $(dir $@)
	@echo "link $@ ($(words $(S3_WASM_OBJ)) objects)"
	@$(S3_WASM_CXX) $(S3_WASM_FLAGS) $(S3_WASM_OBJ) \
	  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=Skate3 -sENVIRONMENT=web \
	  -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=4GB -sINITIAL_MEMORY=128MB -sSTACK_SIZE=16MB -sFILESYSTEM=0 \
	  -sEXPORTED_FUNCTIONS=$(S3_WASM_EXPORTS) \
	  -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPU32,HEAPF32,UTF8ToString,stringToUTF8,lengthBytesUTF8 \
	  -o $@

s3-wasm: $(S3_WASM_OUT)/skate3.js
.PHONY: s3-wasm
