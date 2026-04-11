#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

inline constexpr int kIndexWidth = 128;
inline constexpr int kIndexHeight = 64;
inline constexpr std::size_t kIndexBytes = kIndexWidth * kIndexHeight;

[[nodiscard]] std::vector<std::uint8_t> trans_to_index_view(std::span<const std::uint8_t> raw_block);

}  // namespace dj1000
