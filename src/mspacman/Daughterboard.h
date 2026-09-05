#pragma once
// Created by Jacob Hodgkins

#include "rom/RomSet.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace msrip::mspacman {

struct PatchRegion {
    std::uint16_t destination = 0;
    std::uint16_t source = 0;
};

struct DaughterboardImages {
    std::array<std::uint8_t, 0x10000> decoderDisabled{};
    std::array<std::uint8_t, 0x10000> decoderEnabled{};
    std::array<bool, 0x10000> romMappedDisabled{};
    std::array<bool, 0x10000> romMappedEnabled{};
};

struct EncodedAuxRoms {
    std::vector<std::uint8_t> u5;
    std::vector<std::uint8_t> u6;
    std::vector<std::uint8_t> u7;
};

enum class AccessType { OpcodeFetch, DataRead, DataWrite };

class DaughterboardCodec {
public:
    static const std::array<PatchRegion, 40>& patchRegions();
    static const std::array<std::uint16_t, 7>& decoderDisableTrapStarts();
    static std::uint16_t decoderEnableTrapStart();

    static bool buildImages(const RomSet& roms, DaughterboardImages& out, std::string& error);
    static bool encodeAuxFromEnabledImage(const DaughterboardImages& images, EncodedAuxRoms& out, std::string& error);
    static bool verifyAuxRoundTrip(const RomSet& roms, std::string& error);
};

class DaughterboardMemory {
public:
    explicit DaughterboardMemory(const DaughterboardImages& images);
    void reset(bool decoderEnabled = true);
    bool decoderEnabled() const { return decoderEnabled_; }

    bool readRom(std::uint16_t address, AccessType type, std::uint8_t& value);
    void writeTrap(std::uint16_t address, AccessType type);

private:
    void applyTrap(std::uint16_t address);
    const DaughterboardImages& images_;
    bool decoderEnabled_ = true;
};

} // namespace msrip::mspacman
