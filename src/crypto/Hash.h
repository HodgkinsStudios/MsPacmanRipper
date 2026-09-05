#pragma once
// Created by Jacob Hodgkins

#include <cstdint>
#include <string>
#include <vector>

namespace msrip::crypto {

std::uint32_t crc32(const std::vector<std::uint8_t>& data);
std::string sha256Hex(const std::vector<std::uint8_t>& data);

} // namespace msrip::crypto
