#pragma once
// Created by Jacob Hodgkins
// Address/name-only reference to the frozen Pac-Man 381-family semantic ledger. No ROM bytes are embedded.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace msrip::semantic {
struct PacmanSemanticFamily {
    const char* stableId;
    unsigned wave;
    const char* provenanceStableId;
    const char* familyName;
    std::uint16_t entryPC;
    std::vector<std::uint16_t> sourcePCs;
};
class PacmanSemanticFamilies {
public:
    static const std::vector<PacmanSemanticFamily>& records();
    static constexpr std::size_t CertifiedFamilies=381;
    static constexpr std::size_t CertifiedInstructionStarts=5414;
};
}
