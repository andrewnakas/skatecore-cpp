// Port of skate-game/src/physics/biped_air/publication.rs
//! Recovered output adapter; switches with the complete lifecycle migration.
#include "s3/game/physics/biped_air/publication.hpp"

namespace s3::game::physics::biped_air::publication {

void publish(::s3::player::offboard::biped_air::recovered::OffBoardOutput output,
             ::s3::player::input_phase::OffBoardOutputFields& off_board) {
    //82D30300 writes OffBoard, not the overlapping on-board Air packet.
    //Common Biped cadence80/state84 publication remains coordinator-owned.
    off_board.scalar_32 = output.scalar_32;
    off_board.vector_64 = rs::to_bits(output.vector_64);
    off_board.scalar_92 = output.scalar_92;
    off_board.vector_96 = rs::to_bits(output.vector_96);
    off_board.word_144 = output.word_144;
    off_board.scalar_148 = output.scalar_148;
    off_board.scalar_152 = output.scalar_152;
    off_board.scalar_156 = output.scalar_156;
    off_board.vector_160 = rs::to_bits(output.vector_160);
    off_board.vector_176 = rs::to_bits(output.vector_176);
    off_board.vector_192 = rs::to_bits(output.vector_192);
    off_board.vector_208 = rs::to_bits(output.vector_208);
    off_board.vector_224 = rs::to_bits(output.vector_224);
    off_board.vector_240 = rs::to_bits(output.vector_240);
    off_board.flag_320 = static_cast<std::uint8_t>(output.flag_320);
    off_board.flag_328 = static_cast<std::uint8_t>(output.flag_328);
    off_board.trajectory_valid_331 = static_cast<std::uint8_t>(output.flag_331);
}

}  // namespace s3::game::physics::biped_air::publication
