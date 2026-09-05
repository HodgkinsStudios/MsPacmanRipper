// Created by Jacob Hodgkins
#include "mspacman/Daughterboard.h"

#include <algorithm>
#include <sstream>

namespace msrip::mspacman {
namespace {

template <std::size_t N>
std::uint16_t permuteAddress(std::uint16_t value, const std::array<int, N>& outputSourceBits) {
    std::uint16_t out = 0;
    for (std::size_t outIndex = 0; outIndex < N; ++outIndex) {
        const int srcBit = outputSourceBits[outIndex];
        const std::size_t outBit = N - 1u - outIndex;
        out |= static_cast<std::uint16_t>(((value >> srcBit) & 1u) << outBit);
    }
    return out;
}

std::uint8_t permute8(std::uint8_t value, const std::array<int, 8>& outputSourceBits) {
    std::uint8_t out = 0;
    for (std::size_t outIndex = 0; outIndex < 8; ++outIndex) {
        const int srcBit = outputSourceBits[outIndex];
        const std::size_t outBit = 7u - outIndex;
        out |= static_cast<std::uint8_t>(((value >> srcBit) & 1u) << outBit);
    }
    return out;
}

std::array<int, 8> inversePermutation(const std::array<int, 8>& decode) {
    std::array<int, 8> inverse{};
    for (std::size_t outputIndex = 0; outputIndex < decode.size(); ++outputIndex) {
        const int inputBit = decode[outputIndex];
        const int outputBit = 7 - static_cast<int>(outputIndex);
        inverse[7 - inputBit] = outputBit;
    }
    return inverse;
}

constexpr std::array<int,8> kDataDecode = {0,4,5,7,6,3,2,1};
constexpr std::array<int,11> kU5Address = {8,7,5,9,10,6,3,4,2,1,0};
constexpr std::array<int,11> kU6Address = {3,7,9,10,8,6,5,4,2,1,0};
constexpr std::array<int,12> kU7Address = {11,3,7,9,10,8,6,5,4,2,1,0};

std::uint8_t decodeData(std::uint8_t v) { return permute8(v, kDataDecode); }
std::uint8_t encodeData(std::uint8_t v) {
    static const std::array<int,8> inv = inversePermutation(kDataDecode);
    return permute8(v, inv);
}

bool inEightByteRegion(std::uint16_t address, std::uint16_t start) {
    return address >= start && address < static_cast<std::uint16_t>(start + 8u);
}

} // namespace

const std::array<PatchRegion, 40>& DaughterboardCodec::patchRegions() {
    static const std::array<PatchRegion, 40> patches = {{
        {0x0410,0x8008},{0x08e0,0x81d8},{0x0a30,0x8118},{0x0bd0,0x80d8},
        {0x0c20,0x8120},{0x0e58,0x8168},{0x0ea8,0x8198},{0x1000,0x8020},
        {0x1008,0x8010},{0x1288,0x8098},{0x1348,0x8048},{0x1688,0x8088},
        {0x16b0,0x8188},{0x16d8,0x80c8},{0x16f8,0x81c8},{0x19a8,0x80a8},
        {0x19b8,0x81a8},{0x2060,0x8148},{0x2108,0x8018},{0x21a0,0x81a0},
        {0x2298,0x80a0},{0x23e0,0x80e8},{0x2418,0x8000},{0x2448,0x8058},
        {0x2470,0x8140},{0x2488,0x8080},{0x24b0,0x8180},{0x24d8,0x80c0},
        {0x24f8,0x81c0},{0x2748,0x8050},{0x2780,0x8090},{0x27b8,0x8190},
        {0x2800,0x8028},{0x2b20,0x8100},{0x2b30,0x8110},{0x2bf0,0x81d0},
        {0x2cc0,0x80d0},{0x2cd8,0x80e0},{0x2cf0,0x81e0},{0x2d60,0x8160}
    }};
    return patches;
}

const std::array<std::uint16_t, 7>& DaughterboardCodec::decoderDisableTrapStarts() {
    static const std::array<std::uint16_t, 7> traps = {0x0038,0x03b0,0x1600,0x2120,0x3ff0,0x8000,0x97f0};
    return traps;
}

std::uint16_t DaughterboardCodec::decoderEnableTrapStart() { return 0x3ff8; }

bool DaughterboardCodec::buildImages(const RomSet& roms, DaughterboardImages& out, std::string& error) {
    std::string validationError;
    if (!roms.validateCanonical(validationError)) { error = "Canonical validation failed:\n" + validationError; return false; }
    const auto base = roms.basePacmanProgram();
    const RomFile* u5 = roms.find("u5");
    const RomFile* u6 = roms.find("u6");
    const RomFile* u7 = roms.find("u7");
    if (base.size() != 0x4000 || !u5 || !u6 || !u7) { error = "Required program ROMs unavailable."; return false; }

    out = DaughterboardImages{};

    // Decoder disabled: original Pac-Man 16 KiB plus A15-not-decoded mirrors at 8000-BFFF.
    for (std::size_t i = 0; i < 0x4000; ++i) {
        out.decoderDisabled[i] = base[i];
        out.romMappedDisabled[i] = true;
        out.decoderDisabled[0x8000+i] = base[i];
        out.romMappedDisabled[0x8000+i] = true;
    }

    // Decoder enabled: preserved lower Pac-Man code, decrypted U7 replacement, decrypted U5/U6 upper code,
    // and documented main-board mirrors.
    for (std::size_t i = 0; i < 0x3000; ++i) {
        out.decoderEnabled[i] = base[i];
        out.romMappedEnabled[i] = true;
    }
    for (std::uint16_t i = 0; i < 0x1000; ++i) {
        const std::uint16_t phys = permuteAddress<12>(i, kU7Address);
        out.decoderEnabled[0x3000u+i] = decodeData(u7->data[phys]);
        out.romMappedEnabled[0x3000u+i] = true;
    }
    for (std::uint16_t i = 0; i < 0x0800; ++i) {
        const std::uint16_t p5 = permuteAddress<11>(i, kU5Address);
        const std::uint16_t p6 = permuteAddress<11>(i, kU6Address);
        out.decoderEnabled[0x8000u+i] = decodeData(u5->data[p5]);
        out.decoderEnabled[0x8800u+i] = decodeData(u6->data[0x0800u+p6]);
        out.decoderEnabled[0x9000u+i] = decodeData(u6->data[p6]);
        out.decoderEnabled[0x9800u+i] = base[0x1800u+i];
        out.romMappedEnabled[0x8000u+i] = true;
        out.romMappedEnabled[0x8800u+i] = true;
        out.romMappedEnabled[0x9000u+i] = true;
        out.romMappedEnabled[0x9800u+i] = true;
    }
    for (std::size_t i = 0; i < 0x1000; ++i) {
        out.decoderEnabled[0xa000u+i] = base[0x2000u+i];
        out.decoderEnabled[0xb000u+i] = base[0x3000u+i];
        out.romMappedEnabled[0xa000u+i] = true;
        out.romMappedEnabled[0xb000u+i] = true;
    }

    for (const PatchRegion& patch : patchRegions()) {
        for (std::uint16_t i = 0; i < 8; ++i)
            out.decoderEnabled[patch.destination+i] = out.decoderEnabled[patch.source+i];
    }
    return true;
}

bool DaughterboardCodec::encodeAuxFromEnabledImage(const DaughterboardImages& images, EncodedAuxRoms& out, std::string& error) {
    out.u5.assign(0x0800, 0);
    out.u6.assign(0x1000, 0);
    out.u7.assign(0x1000, 0);

    // Patch destinations are aliases of bytes stored in the first 0x1F0 bytes of decoded U5.
    // Therefore only canonical daughterboard source ranges are encoded here; patched low-bank aliases are not independent storage.
    for (std::uint16_t i = 0; i < 0x1000; ++i) {
        const std::uint16_t phys = permuteAddress<12>(i, kU7Address);
        if (phys >= out.u7.size()) { error = "U7 inverse address permutation escaped ROM."; return false; }
        out.u7[phys] = encodeData(images.decoderEnabled[0x3000u+i]);
    }
    for (std::uint16_t i = 0; i < 0x0800; ++i) {
        const std::uint16_t p5 = permuteAddress<11>(i, kU5Address);
        const std::uint16_t p6 = permuteAddress<11>(i, kU6Address);
        if (p5 >= out.u5.size() || p6 >= 0x0800u) { error = "U5/U6 inverse address permutation escaped ROM."; return false; }
        out.u5[p5] = encodeData(images.decoderEnabled[0x8000u+i]);
        out.u6[0x0800u+p6] = encodeData(images.decoderEnabled[0x8800u+i]);
        out.u6[p6] = encodeData(images.decoderEnabled[0x9000u+i]);
    }
    return true;
}

bool DaughterboardCodec::verifyAuxRoundTrip(const RomSet& roms, std::string& error) {
    DaughterboardImages images;
    if (!buildImages(roms, images, error)) return false;
    EncodedAuxRoms encoded;
    if (!encodeAuxFromEnabledImage(images, encoded, error)) return false;
    const RomFile* u5 = roms.find("u5");
    const RomFile* u6 = roms.find("u6");
    const RomFile* u7 = roms.find("u7");
    if (!u5 || !u6 || !u7) { error = "Aux ROMs missing after validation."; return false; }
    if (encoded.u5 != u5->data || encoded.u6 != u6->data || encoded.u7 != u7->data) {
        std::ostringstream ss;
        ss << "Daughterboard inverse round trip mismatch:";
        if (encoded.u5 != u5->data) ss << " U5";
        if (encoded.u6 != u6->data) ss << " U6";
        if (encoded.u7 != u7->data) ss << " U7";
        error = ss.str();
        return false;
    }
    return true;
}

DaughterboardMemory::DaughterboardMemory(const DaughterboardImages& images) : images_(images) {}

void DaughterboardMemory::reset(bool decoderEnabled) { decoderEnabled_ = decoderEnabled; }

void DaughterboardMemory::applyTrap(std::uint16_t address) {
    for (const std::uint16_t start : DaughterboardCodec::decoderDisableTrapStarts()) {
        if (inEightByteRegion(address, start)) { decoderEnabled_ = false; return; }
    }
    if (inEightByteRegion(address, DaughterboardCodec::decoderEnableTrapStart())) decoderEnabled_ = true;
}

bool DaughterboardMemory::readRom(std::uint16_t address, AccessType, std::uint8_t& value) {
    applyTrap(address);
    if (decoderEnabled_) {
        if (!images_.romMappedEnabled[address]) return false;
        value = images_.decoderEnabled[address];
    } else {
        if (!images_.romMappedDisabled[address]) return false;
        value = images_.decoderDisabled[address];
    }
    return true;
}

void DaughterboardMemory::writeTrap(std::uint16_t address, AccessType) { applyTrap(address); }

} // namespace msrip::mspacman
