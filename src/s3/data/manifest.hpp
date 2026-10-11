// `private/game.json` asset manifest (skate-data manifest.rs, GameAssets/AssetError) and the
// standalone input recorder JSONL (input_recording.rs).
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "result.hpp"
#include "vfs.hpp"

namespace s3::data {

struct GameAssets
{
	uint32_t version = 0;
	std::string character_scene;
	std::string initial_animation;
	std::string action_graph;
	std::string motion_graph;

	// Reads `<root>/private/game.json` and checks the referenced files exist in the VFS.
	static Result<GameAssets> load(const Vfs &vfs, const std::string &root);
	static Result<GameAssets> parse(std::string_view json);
};

// skate_core::input::xbox::XboxState (raw pad words).
struct XboxState
{
	uint16_t buttons = 0;
	std::array<uint8_t, 2> triggers{};
	std::array<int16_t, 2> left{};
	std::array<int16_t, 2> right{};
};

//! Standalone recorder JSONL. Capture timestamps are host observations, not
//! original-game simulation frames. Replay holds the latest observed raw state.
struct RecordingSample
{
	uint32_t schema = 0;
	uint64_t t_us = 0;
	uint64_t sample = 0;
	bool connected = false;
	std::optional<uint32_t> packet;
	uint16_t buttons = 0;
	std::array<uint8_t, 2> triggers{};
	std::array<int16_t, 2> left{};
	std::array<int16_t, 2> right{};
	XboxState raw_state() const { return XboxState{buttons, triggers, left, right}; }
};

class Recording
{
public:
	static Result<Recording> load(const Vfs &vfs, const std::string &path);
	static Result<Recording> read(const std::string &text);
	const std::vector<RecordingSample> &samples() const { return samples_; }
	uint64_t duration_us() const { return samples_.back().t_us - samples_.front().t_us; }
	/// Zero is the first captured sample. No axis interpolation, invented edges,
	/// or rescaling by sample count; the final sample remains until playback ends.
	const RecordingSample &at_elapsed_us(uint64_t elapsed) const;

private:
	std::vector<RecordingSample> samples_;
};

} // namespace s3::data
