# Stage K: skate-core camera/** + skate-game (simref, Bevy-free) camera/** port and its tests.
# Own object dir with -MMD header tracking. Links only the camera sources plus the explicit
# dependency closure below (physics world queries, graph controller, graph runtime, data
# loaders); other stages' in-progress trees are not compiled.
S3_CAM_CXX ?= c++
S3_CAM_CXXFLAGS ?= -std=c++17 -O2 -ffp-contract=off -Wall -Wextra -Wdouble-promotion -Isrc -Itests/s3/core
S3_CAM_OBJDIR ?= build/s3_cam_obj
S3_CAM_BIN ?= $(S3_CAM_OBJDIR)/s3_cam_tests
S3_CAM_OWN_SRC := $(sort $(wildcard src/s3/core/camera/*.cpp src/s3/game/camera/*.cpp src/s3/game/camera.cpp))
# Dependency closure (computed from the camera objects' undefined symbols).
S3_CAM_DEP_SRC := \
	src/s3/core/graph/activation.cpp \
	src/s3/core/graph/controller.cpp \
	src/s3/core/input/angle.cpp \
	src/s3/core/physics/assembly.cpp \
	src/s3/core/physics/board_ground.cpp \
	src/s3/core/physics/board_pose.cpp \
	src/s3/core/physics/board_pose/normalization.cpp \
	src/s3/core/physics/board_pose/part.cpp \
	src/s3/core/physics/board_runtime.cpp \
	src/s3/core/physics/board_step.cpp \
	src/s3/core/physics/board_world.cpp \
	src/s3/core/physics/board_world/broadphase.cpp \
	src/s3/core/physics/board_world/query_index.cpp \
	src/s3/core/physics/board_world/query_metadata.cpp \
	src/s3/core/physics/collision/fixup.cpp \
	src/s3/core/physics/collision/sphere_triangle.cpp \
	src/s3/core/physics/collision/world_contact.cpp \
	src/s3/core/physics/constraint_frames.cpp \
	src/s3/core/physics/construction/authored_transforms.cpp \
	src/s3/core/physics/construction/board_parts.cpp \
	src/s3/core/physics/construction/deck_geometry.cpp \
	src/s3/core/physics/construction/mass_moments.cpp \
	src/s3/core/physics/construction/primitive_mass.cpp \
	src/s3/core/physics/construction/principal_axes.cpp \
	src/s3/core/physics/contact.cpp \
	src/s3/core/physics/contact_feedback/board_reports.cpp \
	src/s3/core/physics/contact_solver.cpp \
	src/s3/core/physics/drive_frames.cpp \
	src/s3/core/physics/drive_solver.cpp \
	src/s3/core/physics/drive_solver/builder.cpp \
	src/s3/core/physics/hook_drive.cpp \
	src/s3/core/physics/hook_drive/frames.cpp \
	src/s3/core/physics/joint_builder.cpp \
	src/s3/core/physics/joint_builder/arithmetic.cpp \
	src/s3/core/physics/joint_records.cpp \
	src/s3/core/physics/mass.cpp \
	src/s3/core/physics/point_force.cpp \
	src/s3/core/physics/rigid_body.cpp \
	src/s3/core/physics/rigid_body/dynamic_update.cpp \
	src/s3/core/physics/solver.cpp \
	src/s3/core/physics/solver/contact.cpp \
	src/s3/core/physics/solver/contact_build.cpp \
	src/s3/core/physics/solver/contact_build/geometry.cpp \
	src/s3/core/physics/solver/contact_build/publication.cpp \
	src/s3/core/physics/solver/drive.cpp \
	src/s3/core/physics/solver/joint.cpp \
	src/s3/core/physics/solver/joint_data.cpp \
	src/s3/core/physics/solver/packed.cpp \
	src/s3/core/physics/solver/packing.cpp \
	src/s3/core/physics/triangle_closest.cpp \
	src/s3/core/physics/triangle_query.cpp \
	src/s3/core/physics/triangle_sweep.cpp \
	src/s3/core/physics/triangle_sweep_roots.cpp \
	src/s3/core/physics/triangle_sweep_walk.cpp \
	src/s3/core/physics/truck_frames.cpp \
	src/s3/core/physics/world_contact/axes.cpp \
	src/s3/core/physics/world_contact/box_feature.cpp \
	src/s3/core/physics/world_contact/buffer.cpp \
	src/s3/core/physics/world_contact/dispatch.cpp \
	src/s3/core/physics/world_contact/face_clip.cpp \
	src/s3/core/physics/world_contact/face_edge.cpp \
	src/s3/core/physics/world_contact/face_face.cpp \
	src/s3/core/physics/world_contact/face_specialized.cpp \
	src/s3/core/physics/world_contact/feature.cpp \
	src/s3/core/physics/world_contact/point_feature.cpp \
	src/s3/core/physics/world_contact/primitive_geometry.cpp \
	src/s3/core/physics/world_contact/primitive_query.cpp \
	src/s3/core/physics/world_contact/projection.cpp \
	src/s3/core/physics/world_contact/reduction.cpp \
	src/s3/core/physics/world_contact/segment_face.cpp \
	src/s3/core/physics/world_contact/segment_prism.cpp \
	src/s3/core/physics/world_contact/triangle_box_sat.cpp \
	src/s3/core/physics/world_contact/triangle_feature.cpp \
	src/s3/core/trigonometry.cpp \
	src/s3/data/attrib_hash.cpp \
	src/s3/data/collections.cpp \
	src/s3/data/json.cpp \
	src/s3/data/result.cpp \
	src/s3/data/state_graph.cpp \
	src/s3/data/vfs.cpp \
	src/s3/game/graph_host/condition_nodes.cpp \
	src/s3/game/graph_runtime.cpp \
	src/s3/game/graph_runtime/disk.cpp
S3_CAM_TEST_SRC := $(sort tests/s3/cam/main.cpp $(wildcard tests/s3/cam/*_test.cpp))
S3_CAM_OBJ := $(patsubst %.cpp,$(S3_CAM_OBJDIR)/%.o,$(S3_CAM_OWN_SRC) $(S3_CAM_DEP_SRC) $(S3_CAM_TEST_SRC))

$(S3_CAM_OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(S3_CAM_CXX) $(S3_CAM_CXXFLAGS) -MMD -MP -c $< -o $@

-include $(S3_CAM_OBJ:.o=.d)

$(S3_CAM_BIN): $(S3_CAM_OBJ)
	@echo "link $@"
	@$(S3_CAM_CXX) $(S3_CAM_CXXFLAGS) $^ -o $@

s3-cam-test: $(S3_CAM_BIN)
	./$(S3_CAM_BIN)

.PHONY: s3-cam-test
S3_TESTS += s3-cam-test
