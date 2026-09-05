#pragma once
// Created by Jacob Hodgkins

#include <cstdint>
#include <string>
#include <vector>

namespace msrip::semantic {

struct SemanticSymbol {
    std::uint16_t address=0;
    bool decoderEnabled=true;
    std::string name;
    std::string kind;
    std::string evidence;
};

class SemanticCatalog {
public:
    static const std::vector<SemanticSymbol>& symbols();
    static const SemanticSymbol* find(std::uint16_t address,bool decoderEnabled=true);
};

} // namespace msrip::semantic
