// Created by Jacob Hodgkins
#include "analysis/StatefulAnalyzer.h"
#include "analysis/PacmanCertifiedBoundaries.h"
#include "analysis/PacmanCertifiedInlineRanges.h"
#include "semantic/SemanticCatalog.h"
#include "semantic/PacmanSemanticFamilies.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <sys/stat.h>

namespace msrip::analysis {
namespace {

std::uint16_t addLength(std::uint16_t pc, std::size_t length) {
    return static_cast<std::uint16_t>(pc + static_cast<std::uint16_t>(length));
}

std::string stateName(bool enabled) { return enabled ? "ENABLED" : "DISABLED"; }

struct PatchRedirectEvidence { std::uint16_t sourcePc; std::uint16_t target; };
inline constexpr std::array<PatchRedirectEvidence, 27> kPatchRedirects = {{
    {0x241C,0x946A},{0x0413,0x3E5C},{0x100B,0x3678},{0x2108,0x3435},
    {0x2803,0x955E},{0x274B,0x9561},{0x244B,0x947C},{0x248A,0x9481},
    {0x168C,0x869C},{0x2781,0x9561},{0x229A,0x3469},{0x19AD,0x8818},
    {0x24DD,0x9580},{0x16D9,0x86C5},{0x2CC1,0x9797},{0x0C21,0x9524},
    {0x2472,0x94EC},{0x2060,0x366F},{0x2D62,0x364E},{0x24B4,0x9504},
    {0x16B1,0x86B1},{0x27BB,0x9559},{0x0EAD,0x86EE},{0x21A1,0x344F},
    {0x24F9,0x95C3},{0x16FA,0x86D9},{0x2BF4,0x8793}
}};

} // namespace

double AnalysisSummary::classifiedPercent() const {
    if (canonicalProgramBytes == 0) return 0.0;
    return static_cast<double>(classifiedBytes()) * 100.0 / static_cast<double>(canonicalProgramBytes);
}

StatefulAnalyzer::StatefulAnalyzer(const mspacman::DaughterboardImages& images)
    : images_(images), scratch_(0x10000, 0) {
    for (auto& state : logicalOwners_) state.fill(-1);
    for (auto& state : hardData_) state.fill(false);
}

std::string StatefulAnalyzer::hex4(std::uint16_t v) {
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << static_cast<unsigned>(v);
    return ss.str();
}

std::string StatefulAnalyzer::hex2(std::uint8_t v) {
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(v);
    return ss.str();
}

std::string StatefulAnalyzer::csvQuote(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char c : value) {
        if (c == '"') escaped.push_back('"');
        escaped.push_back(c);
    }
    escaped.push_back('"');
    return escaped;
}

const char* StatefulAnalyzer::sourceName(CanonicalSource source) {
    switch (source) {
        case CanonicalSource::Pacman6E: return "pacman.6e";
        case CanonicalSource::Pacman6F: return "pacman.6f";
        case CanonicalSource::Pacman6H: return "pacman.6h";
        case CanonicalSource::Pacman6J: return "pacman.6j";
        case CanonicalSource::U5: return "u5";
        case CanonicalSource::U6: return "u6";
        case CanonicalSource::U7: return "u7";
        case CanonicalSource::None: break;
    }
    return "none";
}

std::size_t StatefulAnalyzer::sourceSize(CanonicalSource source) {
    switch (source) {
        case CanonicalSource::Pacman6E:
        case CanonicalSource::Pacman6F:
        case CanonicalSource::Pacman6H:
        case CanonicalSource::Pacman6J:
        case CanonicalSource::U6:
        case CanonicalSource::U7: return 0x1000;
        case CanonicalSource::U5: return 0x0800;
        case CanonicalSource::None: break;
    }
    return 0;
}

std::size_t StatefulAnalyzer::sourceBaseIndex(CanonicalSource source) {
    switch (source) {
        case CanonicalSource::Pacman6E: return 0x0000;
        case CanonicalSource::Pacman6F: return 0x1000;
        case CanonicalSource::Pacman6H: return 0x2000;
        case CanonicalSource::Pacman6J: return 0x3000;
        case CanonicalSource::U5: return 0x4000;
        case CanonicalSource::U6: return 0x4800;
        case CanonicalSource::U7: return 0x5800;
        case CanonicalSource::None: break;
    }
    return kCanonicalProgramBytes;
}

StatefulAnalyzer::Provenance StatefulAnalyzer::provenanceFor(std::uint16_t address, bool effectiveDecoderEnabled) const {
    Provenance p;
    auto setBase = [&](std::uint16_t baseOffset, bool alias) {
        const std::uint16_t bank = static_cast<std::uint16_t>(baseOffset >> 12);
        p.source = static_cast<CanonicalSource>(static_cast<int>(CanonicalSource::Pacman6E) + bank);
        p.decodedOffset = static_cast<std::uint16_t>(baseOffset & 0x0fff);
        p.canonicalIndex = sourceBaseIndex(p.source) + p.decodedOffset;
        p.valid = true;
        p.alias = alias;
    };

    if (!effectiveDecoderEnabled) {
        if (address < 0x4000) setBase(address, false);
        else if (address >= 0x8000 && address < 0xc000) setBase(static_cast<std::uint16_t>(address - 0x8000), true);
        return p;
    }

    if (address < 0x3000) {
        for (const auto& patch : mspacman::DaughterboardCodec::patchRegions()) {
            if (address >= patch.destination && address < static_cast<std::uint16_t>(patch.destination + 8u)) {
                p.source = CanonicalSource::U5;
                p.decodedOffset = static_cast<std::uint16_t>((patch.source - 0x8000u) + (address - patch.destination));
                p.canonicalIndex = sourceBaseIndex(p.source) + p.decodedOffset;
                p.valid = p.decodedOffset < sourceSize(p.source);
                p.alias = true;
                return p;
            }
        }
        setBase(address, false);
        return p;
    }
    if (address < 0x4000) {
        p.source = CanonicalSource::U7;
        p.decodedOffset = static_cast<std::uint16_t>(address - 0x3000);
        p.canonicalIndex = sourceBaseIndex(p.source) + p.decodedOffset;
        p.valid = true;
        return p;
    }
    if (address >= 0x8000 && address < 0x8800) {
        p.source = CanonicalSource::U5;
        p.decodedOffset = static_cast<std::uint16_t>(address - 0x8000);
        p.canonicalIndex = sourceBaseIndex(p.source) + p.decodedOffset;
        p.valid = true;
        return p;
    }
    if (address >= 0x8800 && address < 0x9000) {
        p.source = CanonicalSource::U6;
        p.decodedOffset = static_cast<std::uint16_t>(0x0800u + address - 0x8800u);
        p.canonicalIndex = sourceBaseIndex(p.source) + p.decodedOffset;
        p.valid = true;
        return p;
    }
    if (address >= 0x9000 && address < 0x9800) {
        p.source = CanonicalSource::U6;
        p.decodedOffset = static_cast<std::uint16_t>(address - 0x9000);
        p.canonicalIndex = sourceBaseIndex(p.source) + p.decodedOffset;
        p.valid = true;
        return p;
    }
    if (address >= 0x9800 && address < 0xa000) {
        setBase(static_cast<std::uint16_t>(0x1800u + address - 0x9800u), true);
        return p;
    }
    if (address >= 0xa000 && address < 0xb000) {
        setBase(static_cast<std::uint16_t>(0x2000u + address - 0xa000u), true);
        return p;
    }
    if (address >= 0xb000 && address < 0xc000) {
        setBase(static_cast<std::uint16_t>(0x3000u + address - 0xb000u), true);
        return p;
    }
    return p;
}

bool StatefulAnalyzer::decodeAt(std::uint16_t pc, bool decoderEnabled, DecodedStateInstruction& out) const {
    constexpr std::size_t speculativeBytes = 16;
    mspacman::DaughterboardMemory speculative(images_);
    speculative.reset(decoderEnabled);
    std::array<bool, speculativeBytes> speculativeMapped{};

    for (std::size_t i = 0; i < speculativeBytes; ++i) {
        const std::uint32_t wide = static_cast<std::uint32_t>(pc) + static_cast<std::uint32_t>(i);
        if (wide > 0xffffu) break;
        std::uint8_t value = 0;
        const std::uint16_t address = static_cast<std::uint16_t>(wide);
        speculativeMapped[i] = speculative.readRom(address, mspacman::AccessType::OpcodeFetch, value);
        scratch_[address] = value;
    }
    if (!speculativeMapped[0]) return false;

    out = DecodedStateInstruction{};
    out.entryDecoderEnabled = decoderEnabled;
    out.instruction = disassembler_.decode(scratch_, pc);
    const std::size_t length = out.instruction.length();
    if (length == 0 || length > speculativeBytes) return false;
    for (std::size_t i = 0; i < length; ++i) if (!speculativeMapped[i]) return false;

    mspacman::DaughterboardMemory actual(images_);
    actual.reset(decoderEnabled);
    out.bytes.reserve(length);
    std::vector<std::uint8_t> actualBytes;
    actualBytes.reserve(length);
    bool previousState = decoderEnabled;
    for (std::size_t i = 0; i < length; ++i) {
        const std::uint16_t address = static_cast<std::uint16_t>(pc + static_cast<std::uint16_t>(i));
        std::uint8_t value = 0;
        const bool mapped = actual.readRom(address, mspacman::AccessType::OpcodeFetch, value);
        const bool effectiveState = actual.decoderEnabled();
        if (effectiveState != previousState) out.decoderTransitioned = true;
        previousState = effectiveState;
        out.bytes.push_back({address, value, effectiveState, mapped});
        actualBytes.push_back(value);
    }
    out.instruction.bytes = actualBytes;

    for (const auto& ref : out.instruction.memoryRefs) {
        if (ref.access == disasm::RefAccess::Address) continue;
        if (ref.access == disasm::RefAccess::Read || ref.access == disasm::RefAccess::ReadWrite) {
            std::uint8_t value = 0;
            const bool before = actual.decoderEnabled();
            const bool mapped = actual.readRom(ref.address, mspacman::AccessType::DataRead, value);
            if (actual.decoderEnabled() != before) out.decoderTransitioned = true;
            if (mapped) out.romDataReads.push_back({ref.address, actual.decoderEnabled(), true});
        }
        if (ref.access == disasm::RefAccess::Write || ref.access == disasm::RefAccess::ReadWrite) {
            const bool before = actual.decoderEnabled();
            actual.writeTrap(ref.address, mspacman::AccessType::DataWrite);
            if (actual.decoderEnabled() != before) out.decoderTransitioned = true;
        }
    }
    out.exitDecoderEnabled = actual.decoderEnabled();
    return true;
}

bool StatefulAnalyzer::canFetch(std::uint16_t pc, bool decoderEnabled) const {
    mspacman::DaughterboardMemory mem(images_);
    mem.reset(decoderEnabled);
    std::uint8_t value = 0;
    return mem.readRom(pc, mspacman::AccessType::OpcodeFetch, value);
}

bool StatefulAnalyzer::readWordForData(std::uint16_t address, bool decoderEnabled, std::uint16_t& value, bool& exitDecoderEnabled) const {
    if (address == 0xffffu) return false;
    mspacman::DaughterboardMemory mem(images_);
    mem.reset(decoderEnabled);
    std::uint8_t lo = 0, hi = 0;
    if (!mem.readRom(address, mspacman::AccessType::DataRead, lo)) return false;
    if (!mem.readRom(static_cast<std::uint16_t>(address + 1u), mspacman::AccessType::DataRead, hi)) return false;
    value = static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
    exitDecoderEnabled = mem.decoderEnabled();
    return true;
}

bool StatefulAnalyzer::markInlineData(std::uint16_t start, std::size_t length, bool decoderEnabled,
                                      bool& exitDecoderEnabled, std::string& error) {
    if (static_cast<std::uint32_t>(start) + length > 0x10000u) {
        error = "Inline data range wraps address space.";
        return false;
    }
    mspacman::DaughterboardMemory mem(images_);
    mem.reset(decoderEnabled);
    for (std::size_t i = 0; i < length; ++i) {
        const std::uint16_t address = static_cast<std::uint16_t>(start + static_cast<std::uint16_t>(i));
        std::uint8_t value = 0;
        if (!mem.readRom(address, mspacman::AccessType::DataRead, value)) {
            error = "Inline data byte at $" + hex4(address) + " is not ROM-mapped.";
            return false;
        }
        const bool effectiveState = mem.decoderEnabled();
        const std::size_t stateIndex = effectiveState ? 1u : 0u;
        if (logicalOwners_[stateIndex][address] >= 0) {
            error = "Proven inline data overlaps decoded code at $" + hex4(address) + " in " + stateName(effectiveState) + " state.";
            return false;
        }
        hardData_[stateIndex][address] = true;
        const Provenance provenance = provenanceFor(address, effectiveState);
        if (!provenance.valid || provenance.canonicalIndex >= dataRefs_.size()) {
            error = "Unable to map inline data byte at $" + hex4(address) + " to canonical provenance.";
            return false;
        }
        ++dataRefs_[provenance.canonicalIndex];
    }
    exitDecoderEnabled = mem.decoderEnabled();
    return true;
}

bool StatefulAnalyzer::targetIsCompatibleWithCertifiedBase(std::uint16_t address, bool decoderEnabled) const {
    mspacman::DaughterboardMemory mem(images_);
    mem.reset(decoderEnabled);
    std::uint8_t value = 0;
    if (!mem.readRom(address, mspacman::AccessType::OpcodeFetch, value)) return false;
    const Provenance p = provenanceFor(address, mem.decoderEnabled());
    if (!p.valid) return false;
    if (p.canonicalIndex < 0x4000u) return certifiedBaseStartMask_[p.canonicalIndex];
    return true;
}

bool StatefulAnalyzer::baseInlineRangeIsCertifiedNonCode(std::uint16_t start, std::size_t length,
                                                           bool decoderEnabled) const {
    if (static_cast<std::uint32_t>(start) + length > 0x10000u) return false;
    mspacman::DaughterboardMemory mem(images_);
    mem.reset(decoderEnabled);
    for (std::size_t i = 0; i < length; ++i) {
        const std::uint16_t address = static_cast<std::uint16_t>(start + static_cast<std::uint16_t>(i));
        std::uint8_t value = 0;
        if (!mem.readRom(address, mspacman::AccessType::DataRead, value)) return false;
        const Provenance p = provenanceFor(address, mem.decoderEnabled());
        if (!p.valid) return false;
        if (p.canonicalIndex < 0x4000u && certifiedBaseCodeMask_[p.canonicalIndex]) return false;
    }
    return true;
}

bool StatefulAnalyzer::requireReachedInstruction(std::uint16_t pc, const std::string& mnemonic,
                                                     const std::string& operands, std::string& error) const {
    const auto it = instructions_.find({pc, true});
    if (it == instructions_.end()) {
        error = "Structured-data proof requires unreached instruction $" + hex4(pc) + ".";
        return false;
    }
    if (it->second.instruction.mnemonic != mnemonic || it->second.instruction.operands != operands) {
        error = "Structured-data instruction proof mismatch at $" + hex4(pc) + ": expected " +
            mnemonic + " " + operands + ", got " + it->second.instruction.text();
        return false;
    }
    return true;
}

bool StatefulAnalyzer::markStructuredDataRange(std::uint16_t start, std::size_t length, const std::string& kind,
                                                const std::string& proof, std::string& error) {
    if (length == 0 || static_cast<std::uint32_t>(start) + length > 0x10000u) {
        error = "Invalid structured-data range at $" + hex4(start) + ".";
        return false;
    }
    std::size_t canonicalBytes = 0;
    std::set<std::size_t> seen;
    for (std::size_t i = 0; i < length; ++i) {
        const std::uint16_t address = static_cast<std::uint16_t>(start + static_cast<std::uint16_t>(i));
        const Provenance p = provenanceFor(address, true);
        if (!p.valid || p.canonicalIndex < 0x4000u || p.canonicalIndex >= kCanonicalProgramBytes) {
            error = "Structured daughterboard DATA at $" + hex4(address) + " did not map to U5/U6/U7 canonical storage.";
            return false;
        }
        if (codeRefs_[p.canonicalIndex] != 0) {
            error = "Structured daughterboard DATA conflicts with CODE at $" + hex4(address) +
                " (" + sourceName(p.source) + " decoded offset $" + hex4(p.decodedOffset) + ").";
            return false;
        }
        if (seen.insert(p.canonicalIndex).second) ++canonicalBytes;
    }
    for (std::size_t i = 0; i < length; ++i) {
        const std::uint16_t address = static_cast<std::uint16_t>(start + static_cast<std::uint16_t>(i));
        const Provenance p = provenanceFor(address, true);
        dataRefs_[p.canonicalIndex] = std::max<std::size_t>(dataRefs_[p.canonicalIndex], 1u);
        structuredDataMask_[p.canonicalIndex] = true;
    }
    structuredDataSpans_.push_back({kind, start, static_cast<std::uint16_t>(start + static_cast<std::uint16_t>(length)),
                                    canonicalBytes, proof});
    return true;
}

bool StatefulAnalyzer::recoverStructuredDaughterboardData(std::string& error) {
    // ---------------- U5 animation pointer table + bytecode programs ----------------
    // The reached routine at $361F indexes $81F0 by C and copies 12 bytes. Eight
    // independently reached C constants cover offsets $00..$54 in $0C-byte steps,
    // proving a 0x60-byte table (48 little-endian pointers).
    if (!requireReachedInstruction(0x361F, "LD", "HL,$81F0", error) ||
        !requireReachedInstruction(0x3628, "LD", "BC,$000C", error) ||
        !requireReachedInstruction(0x362B, "LDIR", "", error)) return false;
    const std::array<std::pair<std::uint16_t,std::string>,8> animationOffsets = {{
        {0x344A,"C,$00"},{0x3464,"C,$0C"},{0x347E,"C,$18"},{0x3483,"C,$24"},
        {0x3488,"C,$30"},{0x348D,"C,$3C"},{0x3492,"C,$48"},{0x3497,"C,$54"}
    }};
    for (const auto& item : animationOffsets)
        if (!requireReachedInstruction(item.first, "LD", item.second, error)) return false;

    const auto& rom = images_.decoderEnabled;
    std::set<std::uint16_t> scriptStarts;
    for (std::uint16_t a = 0x81F0; a < 0x8250; a = static_cast<std::uint16_t>(a + 2u)) {
        const std::uint16_t target = static_cast<std::uint16_t>(rom[a] | (static_cast<std::uint16_t>(rom[a+1]) << 8));
        if (target < 0x8250 || target >= 0x8614) {
            error = "Animation pointer table target escaped validated U5 script domain at $" + hex4(a) +
                " -> $" + hex4(target) + ".";
            return false;
        }
        scriptStarts.insert(target);
    }
    if (scriptStarts.size() != 21u || *scriptStarts.begin() != 0x8250 || *scriptStarts.rbegin() != 0x8604) {
        error = "Animation pointer table structural cardinality/endpoints failed.";
        return false;
    }

    const auto commandLength = [](std::uint8_t op) -> std::size_t {
        switch (op) {
            case 0xF0: return 4; // move/loop: dx,dy,color
            case 0xF1: return 3; // set position: y,x
            case 0xF2: return 2; // set N
            case 0xF3: return 3; // set sprite-sequence pointer
            case 0xF5: return 2; // play sound
            case 0xF6: case 0xF7: case 0xF8: case 0xFF: return 1;
            default: return 0;
        }
    };

    std::set<std::uint16_t> spriteSequenceStarts;
    std::vector<std::uint16_t> orderedScripts(scriptStarts.begin(), scriptStarts.end());
    if (orderedScripts.front() != 0x8250 || rom[0x8250] != 0xFF) {
        error = "Animation null-script sentinel at $8250 failed.";
        return false;
    }
    for (std::size_t i = 1; i < orderedScripts.size(); ++i) {
        const std::uint16_t start = orderedScripts[i];
        const std::uint16_t expectedEnd = (i + 1u < orderedScripts.size()) ? orderedScripts[i+1u] : 0x8614;
        std::uint16_t pc = start;
        bool ended = false;
        while (pc < expectedEnd) {
            const std::uint8_t op = rom[pc];
            const std::size_t len = commandLength(op);
            if (len == 0 || static_cast<std::uint32_t>(pc) + len > expectedEnd) {
                error = "Animation bytecode grammar failed at $" + hex4(pc) + ".";
                return false;
            }
            if (op == 0xF3) {
                const std::uint16_t sprite = static_cast<std::uint16_t>(rom[pc+1] |
                    (static_cast<std::uint16_t>(rom[pc+2]) << 8));
                if (sprite < 0x8614 || sprite >= 0x869C) {
                    error = "Animation sprite-sequence pointer escaped U5 data domain at $" + hex4(pc) + ".";
                    return false;
                }
                spriteSequenceStarts.insert(sprite);
            }
            pc = static_cast<std::uint16_t>(pc + static_cast<std::uint16_t>(len));
            if (op == 0xFF) { ended = true; break; }
        }
        if (!ended || pc != expectedEnd) {
            error = "Animation script did not terminate exactly at the next pointer boundary from $" + hex4(start) + ".";
            return false;
        }
    }
    if (!markStructuredDataRange(0x81F0, 0x8614u - 0x81F0u, "u5_animation_pointer_and_bytecode",
        "Reached $361F 12-byte indexed copy; eight C offsets prove 0x60 pointer table; all 20 non-null scripts parse contiguously under recovered opcode grammar.", error)) return false;

    if (spriteSequenceStarts.size() != 19u) {
        error = "Animation sprite-sequence reference cardinality changed unexpectedly.";
        return false;
    }
    for (const std::uint16_t start : spriteSequenceStarts) {
        std::uint16_t end = start;
        while (end < 0x869C && rom[end] != 0xFF) ++end;
        if (end >= 0x869C) {
            error = "Sprite sequence at $" + hex4(start) + " has no in-domain $FF terminator.";
            return false;
        }
        ++end; // include terminator
        if (!markStructuredDataRange(start, static_cast<std::size_t>(end-start), "u5_animation_sprite_sequence",
            "Referenced by parsed F3 animation bytecode; $FF-terminated sequence remains within $8614-$869B.", error)) return false;
    }

    // ---------------- U5 fruit descriptor table ----------------
    // Selector proof yields exactly indices 0..6, and 3*A indexes three-byte records.
    if (!requireReachedInstruction(0x8762, "CP", "$07", error) ||
        !requireReachedInstruction(0x8766, "LD", "B,$07", error) ||
        !requireReachedInstruction(0x876A, "AND", "$1F", error) ||
        !requireReachedInstruction(0x8770, "LD", "HL,$879D", error) ||
        !requireReachedInstruction(0x8774, "ADD", "A,A", error) ||
        !requireReachedInstruction(0x8775, "ADD", "A,B", error) ||
        !requireReachedInstruction(0x8776, "RST", "$0010", error)) return false;
    if (!markStructuredDataRange(0x879D, 7u * 3u, "u5_fruit_descriptor_table",
        "Reached selector reduces every used fruit index to 0..6 and indexes $879D with 3*A; each selected record consumes three bytes.", error)) return false;

    // ---------------- U6 maze-index mapping used by daughterboard lookup helper ----------------
    if (!requireReachedInstruction(0x94BD, "LD", "A,($4E13)", error) ||
        !requireReachedInstruction(0x94C1, "CP", "$0D", error) ||
        !requireReachedInstruction(0x94C6, "LD", "HL,$94DF", error) ||
        !requireReachedInstruction(0x94C9, "RST", "$0010", error) ||
        !requireReachedInstruction(0x94CB, "ADD", "A,A", error) ||
        !requireReachedInstruction(0x94CF, "ADD", "HL,BC", error)) return false;
    for (std::uint16_t a=0x94DF; a<0x94EC; ++a) {
        if (rom[a] > 3u) {
            error = "Maze-index mapping value exceeded 0..3 at $" + hex4(a) + ".";
            return false;
        }
    }
    if (!markStructuredDataRange(0x94DF, 13, "u6_level_to_maze_index",
        "Reached $94BD bounds/reduces level index to a 13-byte table at $94DF; all runtime table values validate in maze domain 0..3.", error)) return false;

    // ---------------- U5/U6 fruit route pointer tables and U6 path programs ----------------
    if (!requireReachedInstruction(0x8784, "LD", "HL,$87F8", error) ||
        !requireReachedInstruction(0x87CA, "LD", "HL,$8800", error) ||
        !requireReachedInstruction(0x87CD, "CALL", "$94BD", error) ||
        !requireReachedInstruction(0x87D2, "LD", "A,R", error) ||
        !requireReachedInstruction(0x87D4, "AND", "$03", error) ||
        !requireReachedInstruction(0x87D7, "ADD", "A,A", error) ||
        !requireReachedInstruction(0x87D8, "ADD", "A,A", error) ||
        !requireReachedInstruction(0x87D9, "ADD", "A,B", error)) return false;

    if (!markStructuredDataRange(0x87F8, 8, "u5_fruit_entry_table_pointers",
        "Reached $8784 forwards $87F8 to $87CD/$94BD; validated maze-index domain 0..3 selects four little-endian pointers.", error)) return false;
    if (!markStructuredDataRange(0x8800, 8, "u6_fruit_exit_table_pointers",
        "Reached $87CA forwards $8800 to $87CD/$94BD; validated maze-index domain 0..3 selects four little-endian pointers.", error)) return false;

    std::set<std::uint16_t> descriptorTables;
    for (const std::uint16_t pointerTable : {static_cast<std::uint16_t>(0x87F8), static_cast<std::uint16_t>(0x8800)}) {
        for (std::size_t i=0; i<4; ++i) {
            const std::uint16_t a = static_cast<std::uint16_t>(pointerTable + static_cast<std::uint16_t>(i*2u));
            const std::uint16_t table = static_cast<std::uint16_t>(rom[a] | (static_cast<std::uint16_t>(rom[a+1]) << 8));
            const Provenance tp = provenanceFor(table, true);
            if (!tp.valid || tp.source != CanonicalSource::U6) {
                error = "Fruit route descriptor pointer did not resolve to U6: $" + hex4(a) + " -> $" + hex4(table) + ".";
                return false;
            }
            descriptorTables.insert(table);
        }
    }
    if (descriptorTables.size() != 8u) {
        error = "Fruit route descriptor pointer tables did not resolve to eight distinct maze entry/exit tables.";
        return false;
    }

    // The $87D2-$87DA sequence proves four records selected at strides 0,5,10,15.
    // $8720-$8743 proves the first two bytes are a packed path pointer and byte
    // selection is floor(counter/4), so ceil(counter/4) bytes are consumed.
    if (!requireReachedInstruction(0x8720, "LD", "A,(HL)", error) ||
        !requireReachedInstruction(0x872B, "LD", "HL,($4C42)", error) ||
        !requireReachedInstruction(0x872E, "RST", "$0010", error)) return false;
    for (const std::uint16_t table : descriptorTables) {
        if (!markStructuredDataRange(table, 20, "u6_fruit_route_descriptor_table",
            "Random selector masks to 0..3 and multiplies by five; four five-byte records are therefore addressable.", error)) return false;
        for (std::size_t record=0; record<4; ++record) {
            const std::uint16_t a = static_cast<std::uint16_t>(table + static_cast<std::uint16_t>(record*5u));
            const std::uint16_t path = static_cast<std::uint16_t>(rom[a] | (static_cast<std::uint16_t>(rom[a+1]) << 8));
            const std::uint8_t count = rom[a+2];
            const std::size_t packedBytes = (static_cast<std::size_t>(count) + 3u) / 4u;
            const Provenance pp = provenanceFor(path, true);
            if (count == 0 || packedBytes == 0 || !pp.valid || pp.source != CanonicalSource::U6) {
                error = "Fruit route record at $" + hex4(a) + " has invalid U6 packed-path pointer/count.";
                return false;
            }
            if (!markStructuredDataRange(path, packedBytes, "u6_fruit_route_packed_steps",
                "Descriptor count is consumed four 2-bit direction steps per byte by reached fruit movement routine.", error)) return false;
        }
    }

    // Fixed fallback/exit path at $8808 with counter $1D.
    if (!requireReachedInstruction(0x87ED, "LD", "HL,$8808", error) ||
        !requireReachedInstruction(0x87F3, "LD", "A,$1D", error)) return false;
    if (!markStructuredDataRange(0x8808, (0x1Du + 3u) / 4u, "u6_fruit_route_fixed_fallback",
        "Reached fallback stores path $8808 and counter $1D; packed path consumer uses four 2-bit steps per byte.", error)) return false;

    // ---------------- structured-data analysis: fruit subpixel delta table ----------------
    // Reached $8701 copies the fruit substep phase at $4C41 into B and RST $18
    // indexes two bytes per B from $8841.  The only reached writers initialize the
    // phase to $1F or replace it with (path_2bit & 3)<<4; the per-frame low-nibble
    // cadence keeps the reachable phase in $00..$3F.  This proves 64 two-byte
    // signed delta vectors = 128 bytes.
    if (!requireReachedInstruction(0x8701, "LD", "A,($4C41)", error) ||
        !requireReachedInstruction(0x8704, "LD", "B,A", error) ||
        !requireReachedInstruction(0x8705, "LD", "HL,$8841", error) ||
        !requireReachedInstruction(0x8708, "RST", "$0018", error) ||
        !requireReachedInstruction(0x8716, "AND", "$0F", error) ||
        !requireReachedInstruction(0x873C, "LD", "A,$03", error) ||
        !requireReachedInstruction(0x873E, "AND", "C", error) ||
        !requireReachedInstruction(0x873F, "RLCA", "", error) ||
        !requireReachedInstruction(0x8740, "RLCA", "", error) ||
        !requireReachedInstruction(0x8741, "RLCA", "", error) ||
        !requireReachedInstruction(0x8742, "RLCA", "", error) ||
        !requireReachedInstruction(0x8743, "LD", "($4C41),A", error) ||
        !requireReachedInstruction(0x87E7, "LD", "A,$1F", error) ||
        !requireReachedInstruction(0x87E9, "LD", "($4C41),A", error)) return false;
    if (!markStructuredDataRange(0x8841, 128, "u6_fruit_subpixel_delta_vectors",
        "Reached fruit mover bounds phase to $00..$3F and RST $18 consumes one two-byte signed delta vector per phase.", error)) return false;

    // ---------------- structured-data analysis: U6 maze rendering streams ----------------
    // $241F consumes a byte stream from BC: zero terminates; a non-negative byte is
    // a run/skip count followed by one tile byte; a negative byte is itself the tile
    // written at the next video position.  $946A selects one of four U6 stream pointers.
    if (!requireReachedInstruction(0x241C, "CALL", "$946A", error) ||
        !requireReachedInstruction(0x241F, "LD", "A,(BC)", error) ||
        !requireReachedInstruction(0x2420, "AND", "A", error) ||
        !requireReachedInstruction(0x2421, "RET", "Z", error) ||
        !requireReachedInstruction(0x2422, "JP", "M,$242C", error) ||
        !requireReachedInstruction(0x242A, "INC", "BC", error) ||
        !requireReachedInstruction(0x242B, "LD", "A,(BC)", error) ||
        !requireReachedInstruction(0x2444, "INC", "BC", error) ||
        !requireReachedInstruction(0x2445, "JP", "$241F", error) ||
        !requireReachedInstruction(0x946A, "LD", "HL,$9474", error) ||
        !requireReachedInstruction(0x946D, "CALL", "$94BD", error)) return false;
    if (!markStructuredDataRange(0x9474, 8, "u6_maze_stream_pointer_table",
        "Reached $946A forwards a four-entry little-endian pointer table to the validated maze-index helper.", error)) return false;
    for (std::size_t i=0; i<4; ++i) {
        const std::uint16_t pa = static_cast<std::uint16_t>(0x9474u + i*2u);
        const std::uint16_t start = static_cast<std::uint16_t>(rom[pa] | (static_cast<std::uint16_t>(rom[pa+1]) << 8));
        const Provenance sp = provenanceFor(start, true);
        if (!sp.valid || sp.source != CanonicalSource::U6) {
            error = "Maze stream pointer escaped U6 at $" + hex4(pa) + ".";
            return false;
        }
        std::uint16_t pc = start;
        std::size_t commands = 0;
        for (;;) {
            if (pc >= 0x9800u || ++commands > 1024u) {
                error = "Maze stream failed to terminate in U6 from $" + hex4(start) + ".";
                return false;
            }
            const std::uint8_t op = rom[pc++];
            if (op == 0) break;
            if ((op & 0x80u) == 0) {
                if (pc >= 0x9800u) { error = "Maze stream truncated after skip command."; return false; }
                ++pc; // tile byte consumed after positive skip count
            }
        }
        if (!markStructuredDataRange(start, static_cast<std::size_t>(pc-start), "u6_maze_draw_stream",
            "Reached renderer $241F-$2445 consumes the selected stream under zero-terminated skip/tile grammar.", error)) return false;
    }

    // ---------------- U6 pellet-offset tables ----------------
    // The two verified manual-return continuations execute the same 30x8 nested loop.
    // IY is advanced exactly once per inner iteration, proving 240 bytes per selected map.
    if (!requireReachedInstruction(0x2453, "LD", "D,$00", error) ||
        !requireReachedInstruction(0x2455, "LD", "B,$1E", error) ||
        !requireReachedInstruction(0x2457, "LD", "C,$08", error) ||
        !requireReachedInstruction(0x245C, "LD", "E,(IY+0)", error) ||
        !requireReachedInstruction(0x2465, "INC", "IY", error) ||
        !requireReachedInstruction(0x246C, "DEC", "B", error) ||
        !requireReachedInstruction(0x2492, "LD", "D,$00", error) ||
        !requireReachedInstruction(0x2494, "LD", "B,$1E", error) ||
        !requireReachedInstruction(0x2496, "LD", "C,$08", error) ||
        !requireReachedInstruction(0x2498, "LD", "E,(IY+0)", error) ||
        !requireReachedInstruction(0x24A7, "INC", "IY", error) ||
        !requireReachedInstruction(0x9485, "LD", "HL,$9499", error) ||
        !requireReachedInstruction(0x9488, "CALL", "$94BD", error) ||
        !requireReachedInstruction(0x948B, "LD", "IY,$0000", error) ||
        !requireReachedInstruction(0x948F, "ADD", "IY,BC", error)) return false;
    if (!markStructuredDataRange(0x9499, 8, "u6_pellet_offset_pointer_table",
        "Reached pellet setup forwards a four-entry pointer table to the maze-index helper; renderer advances IY 30*8 times.", error)) return false;
    for (std::size_t i=0; i<4; ++i) {
        const std::uint16_t pa = static_cast<std::uint16_t>(0x9499u + i*2u);
        const std::uint16_t start = static_cast<std::uint16_t>(rom[pa] | (static_cast<std::uint16_t>(rom[pa+1]) << 8));
        const Provenance sp = provenanceFor(start, true);
        const Provenance ep = provenanceFor(static_cast<std::uint16_t>(start + 239u), true);
        if (!sp.valid || !ep.valid || sp.source != CanonicalSource::U6 || ep.source != CanonicalSource::U6) {
            error = "Pellet offset table escaped U6 at $" + hex4(start) + ".";
            return false;
        }
        if (!markStructuredDataRange(start, 240, "u6_pellet_offset_table",
            "Reached base renderer consumes exactly 30 groups of 8 IY bytes for the selected maze.", error)) return false;
    }

    // ---------------- U6 one-record/per-maze tables ----------------
    if (!requireReachedInstruction(0x94A2, "LD", "HL,$94B5", error) ||
        !requireReachedInstruction(0x94A5, "CALL", "$94BD", error) ||
        !requireReachedInstruction(0x94A8, "LD", "A,(BC)", error)) return false;
    if (!markStructuredDataRange(0x94B5, 8, "u6_pellet_count_pointer_table",
        "Reached completion check selects one of four maze-specific byte pointers.", error)) return false;
    for (std::size_t i=0; i<4; ++i) {
        const std::uint16_t pa = static_cast<std::uint16_t>(0x94B5u + i*2u);
        const std::uint16_t target = static_cast<std::uint16_t>(rom[pa] | (static_cast<std::uint16_t>(rom[pa+1]) << 8));
        if (!markStructuredDataRange(target, 1, "u6_pellet_count_value",
            "Reached $94A8 dereferences exactly one byte from the maze-selected pointer.", error)) return false;
    }

    if (!requireReachedInstruction(0x9564, "LD", "HL,$9578", error) ||
        !requireReachedInstruction(0x9567, "CALL", "$94BD", error) ||
        !requireReachedInstruction(0x956E, "AND", "$06", error) ||
        !requireReachedInstruction(0x9570, "RST", "$0010", error) ||
        !requireReachedInstruction(0x9572, "INC", "HL", error) ||
        !requireReachedInstruction(0x9573, "LD", "D,(HL)", error)) return false;
    if (!markStructuredDataRange(0x9578, 8, "u6_ghost_destination_pointer_table",
        "Reached selector chooses one of four maze tables, then masks random offset to 0,2,4,6 and reads a word.", error)) return false;
    for (std::size_t i=0; i<4; ++i) {
        const std::uint16_t pa = static_cast<std::uint16_t>(0x9578u + i*2u);
        const std::uint16_t target = static_cast<std::uint16_t>(rom[pa] | (static_cast<std::uint16_t>(rom[pa+1]) << 8));
        if (!markStructuredDataRange(target, 8, "u6_ghost_destination_quadrants",
            "Reached random selector reads four little-endian destination words at offsets 0,2,4,6.", error)) return false;
    }

    if (!requireReachedInstruction(0x94EC, "LD", "HL,$951C", error) ||
        !requireReachedInstruction(0x94EF, "CALL", "$94BD", error) ||
        !requireReachedInstruction(0x94F7, "LD", "C,(HL)", error) ||
        !requireReachedInstruction(0x94F8, "INC", "HL", error) ||
        !requireReachedInstruction(0x94F9, "LD", "B,(HL)", error) ||
        !requireReachedInstruction(0x9500, "AND", "E", error) ||
        !requireReachedInstruction(0x9501, "JR", "NZ,$94F7", error)) return false;
    if (!markStructuredDataRange(0x951C, 8, "u6_power_pellet_pointer_table",
        "Reached draw/save routines select one of four maze-specific four-word energizer tables.", error)) return false;
    for (std::size_t i=0; i<4; ++i) {
        const std::uint16_t pa = static_cast<std::uint16_t>(0x951Cu + i*2u);
        const std::uint16_t target = static_cast<std::uint16_t>(rom[pa] | (static_cast<std::uint16_t>(rom[pa+1]) << 8));
        if (!markStructuredDataRange(target, 8, "u6_power_pellet_screen_addresses",
            "Reached loop reads four successive little-endian screen addresses; DE low bits terminate after four words.", error)) return false;
    }

    // ---------------- U6 tunnel, color and small graphic tables ----------------
    if (!requireReachedInstruction(0x959B, "LD", "HL,$95AE", error) ||
        !requireReachedInstruction(0x959E, "ADD", "HL,BC", error) ||
        !requireReachedInstruction(0x959F, "LD", "A,(HL)", error) ||
        !requireReachedInstruction(0x9593, "CP", "$15", error)) return false;
    if (!markStructuredDataRange(0x95AE, 21, "u6_maze_color_by_level",
        "Reached color selector normalizes board number into 0..20 before indexing one byte at $95AE+A.", error)) return false;

    if (!requireReachedInstruction(0x95CB, "LD", "HL,$95DF", error) ||
        !requireReachedInstruction(0x95CE, "CALL", "$94BD", error) ||
        !requireReachedInstruction(0x95D4, "LD", "A,(BC)", error) ||
        !requireReachedInstruction(0x95D5, "INC", "BC", error) ||
        !requireReachedInstruction(0x95D6, "AND", "A", error) ||
        !requireReachedInstruction(0x95D7, "JP", "Z,$2534", error)) return false;
    if (!markStructuredDataRange(0x95DF, 4, "u6_tunnel_table_pointers",
        "Reached levels 0..2 map through the validated maze selector to two little-endian tunnel-list pointers.", error)) return false;
    for (std::size_t i=0; i<2; ++i) {
        const std::uint16_t pa = static_cast<std::uint16_t>(0x95DFu + i*2u);
        const std::uint16_t start = static_cast<std::uint16_t>(rom[pa] | (static_cast<std::uint16_t>(rom[pa+1]) << 8));
        std::uint16_t pc = start;
        while (pc < 0x9800u && rom[pc] != 0) ++pc;
        if (pc >= 0x9800u) { error = "Tunnel list failed to terminate."; return false; }
        ++pc;
        if (!markStructuredDataRange(start, static_cast<std::size_t>(pc-start), "u6_tunnel_slow_tile_list",
            "Reached $95D4 consumes one byte per tunnel tile until zero terminator.", error)) return false;
    }

    if (!requireReachedInstruction(0x960D, "LD", "HL,$9616", error) ||
        !requireReachedInstruction(0x9610, "CALL", "$9627", error) ||
        !requireReachedInstruction(0x9627, "LD", "A,(HL)", error) ||
        !requireReachedInstruction(0x9628, "CP", "$FF", error) ||
        !requireReachedInstruction(0x9638, "INC", "HL", error)) return false;
    {
        std::uint16_t pc = 0x9616;
        std::size_t records = 0;
        while (pc < 0x9800u && rom[pc] != 0xFFu) {
            pc = static_cast<std::uint16_t>(pc + 4u);
            if (++records > 32u) { error = "Ms. Pac-Man bonus graphic record list did not terminate."; return false; }
        }
        if (pc >= 0x9800u || records != 4u) { error = "Ms. Pac-Man bonus graphic record cardinality changed."; return false; }
        ++pc;
        if (!markStructuredDataRange(0x9616, static_cast<std::size_t>(pc-0x9616u), "u6_bonus_mspac_graphic_records",
            "Reached $9627 consumes four-byte color/graphic/address records until $FF sentinel.", error)) return false;
    }

    // ---------------- U6/U7 sound-song pointer tables and bytecode ----------------
    // The base sound engine reads a byte at (HL); bytes <$F0 are note/event bytes.
    // $F0 consumes a following little-endian pointer, $F1-$F4 consume one parameter,
    // $F5-$FE consume none, and $FF executes the stop handler.
    if (!requireReachedInstruction(0x2CDA, "LD", "HL,$967D", error) ||
        !requireReachedInstruction(0x2CF3, "LD", "HL,$968D", error) ||
        !requireReachedInstruction(0x97BE, "LD", "HL,$9685", error) ||
        !requireReachedInstruction(0x2D72, "LD", "A,(HL)", error) ||
        !requireReachedInstruction(0x2D7A, "CP", "$F0", error) ||
        !requireReachedInstruction(0x2D82, "AND", "$0F", error) ||
        !requireReachedInstruction(0x2F55, "LD", "L,(IX+6)", error) ||
        !requireReachedInstruction(0x2F5B, "LD", "A,(HL)", error) ||
        !requireReachedInstruction(0x2F60, "LD", "A,(HL)", error) ||
        !requireReachedInstruction(0x2F6B, "LD", "A,(HL)", error) ||
        !requireReachedInstruction(0x2F7D, "LD", "A,(HL)", error) ||
        !requireReachedInstruction(0x2F8F, "LD", "A,(HL)", error) ||
        !requireReachedInstruction(0x2FA1, "LD", "A,(HL)", error)) return false;
    for (const std::uint16_t table : {static_cast<std::uint16_t>(0x967D), static_cast<std::uint16_t>(0x9685), static_cast<std::uint16_t>(0x968D)}) {
        if (!markStructuredDataRange(table, 8, "u6_song_pointer_table",
            "Reached three-channel sound setup forwards four little-endian song pointers to the base sound engine.", error)) return false;
    }
    std::set<std::uint16_t> songStarts;
    for (const std::uint16_t table : {static_cast<std::uint16_t>(0x967D), static_cast<std::uint16_t>(0x9685), static_cast<std::uint16_t>(0x968D)}) {
        for (std::size_t i=0; i<4; ++i) {
            const std::uint16_t pa = static_cast<std::uint16_t>(table + i*2u);
            songStarts.insert(static_cast<std::uint16_t>(rom[pa] | (static_cast<std::uint16_t>(rom[pa+1]) << 8)));
        }
    }
    for (const std::uint16_t start : songStarts) {
        Provenance sp = provenanceFor(start, true);
        if (!sp.valid || (sp.source != CanonicalSource::U6 && sp.source != CanonicalSource::U7)) {
            error = "Song pointer escaped daughterboard program storage at $" + hex4(start) + ".";
            return false;
        }
        std::uint16_t pc = start;
        std::size_t tokens = 0;
        bool ended = false;
        while (pc < 0x9800u || (pc >= 0x3000u && pc < 0x4000u)) {
            const Provenance pp = provenanceFor(pc, true);
            if (!pp.valid || (pp.source != CanonicalSource::U6 && pp.source != CanonicalSource::U7)) break;
            const std::uint8_t op = rom[pc];
            std::size_t len = 1;
            if (op == 0xF0u) len = 3;
            else if (op >= 0xF1u && op <= 0xF4u) len = 2;
            for (std::size_t j=0; j<len; ++j) {
                const Provenance bp = provenanceFor(static_cast<std::uint16_t>(pc+j), true);
                if (!bp.valid || (bp.source != CanonicalSource::U6 && bp.source != CanonicalSource::U7)) {
                    error = "Song bytecode parameter escaped daughterboard storage.";
                    return false;
                }
            }
            pc = static_cast<std::uint16_t>(pc + static_cast<std::uint16_t>(len));
            if (++tokens > 2048u) { error = "Song bytecode safety limit exceeded."; return false; }
            if (op == 0xFFu) { ended = true; break; }
        }
        if (!ended) { error = "Song stream failed to terminate from $" + hex4(start) + "."; return false; }
        if (!markStructuredDataRange(start, static_cast<std::size_t>(pc-start), "daughterboard_song_bytecode",
            "Referenced by reached channel song table; byte lengths are proven by reached $2D72/$2F55-$2FAD handlers and terminate at $FF.", error)) return false;
    }

    // ---------------- structured-data analysis: U7 startup/self-test data ----------------
    if (!requireReachedInstruction(0x3042, "LD", "SP,$3154", error) ||
        !requireReachedInstruction(0x3047, "POP", "HL", error) ||
        !requireReachedInstruction(0x3048, "POP", "DE", error) ||
        !requireReachedInstruction(0x30A5, "CP", "$44", error) ||
        !requireReachedInstruction(0x30AD, "JP", "NZ,$3045", error)) return false;
    if (!markStructuredDataRange(0x3154, 24, "u7_ram_test_records",
        "Reached self-test loads SP=$3154 and pops six four-byte address/mask/count records before its exact terminal condition.", error)) return false;
    if (!requireReachedInstruction(0x310C, "LD", "HL,($316C)", error) ||
        !requireReachedInstruction(0x3113, "LD", "HL,($316E)", error) ||
        !requireReachedInstruction(0x311A, "LD", "HL,($3170)", error) ||
        !requireReachedInstruction(0x311F, "LD", "HL,($3172)", error)) return false;
    if (!markStructuredDataRange(0x316C, 8, "u7_self_test_error_character_pairs",
        "Four reached absolute word loads consume the complete $316C-$3173 character-pair table.", error)) return false;

    // Bonus-score text table: DE selects one of three two-byte records.
    if (!requireReachedInstruction(0x320F, "LD", "HL,$32F9", error) ||
        !requireReachedInstruction(0x3212, "ADD", "HL,DE", error) ||
        !requireReachedInstruction(0x3213, "LD", "A,(HL)", error) ||
        !requireReachedInstruction(0x3217, "INC", "HL", error) ||
        !requireReachedInstruction(0x3218, "LD", "A,(HL)", error)) return false;
    if (!markStructuredDataRange(0x32F9, 6, "u7_bonus_score_text_pairs",
        "Reached bonus renderer selects one of three two-character records at offsets 0,2,4.", error)) return false;

    // Direction vectors: all reached users mask/store orientation in domain 0..3 and
    // RST $18 indexes two bytes per orientation, proving the first four vectors.
    if (!requireReachedInstruction(0x1F07, "LD", "HL,$32FF", error) ||
        !requireReachedInstruction(0x1F13, "RST", "$0018", error) ||
        !requireReachedInstruction(0x2933, "LD", "IX,$32FF", error) ||
        !requireReachedInstruction(0x2937, "ADD", "IX,DE", error)) return false;
    if (!markStructuredDataRange(0x32FF, 8, "u7_direction_delta_vectors",
        "Reached orientation consumers index four signed little-endian direction deltas with 2*orientation, orientation domain 0..3.", error)) return false;

    // Difficulty table.  Reached $0716 indexes the certified base table $0796 in
    // six-byte records; $0814 is the next certified instruction start, so there are
    // exactly 21 selector records.  Their first bytes are all <=6.  $0727-$0736
    // multiplies that byte by exactly 42 and indexes $330F; $3435 is reached code,
    // independently bounding seven 42-byte difficulty records.
    if (!requireReachedInstruction(0x0716, "LD", "IX,$0796", error) ||
        !requireReachedInstruction(0x0724, "LD", "A,(IX+0)", error) ||
        !requireReachedInstruction(0x0727, "ADD", "A,A", error) ||
        !requireReachedInstruction(0x0728, "LD", "B,A", error) ||
        !requireReachedInstruction(0x072B, "LD", "C,A", error) ||
        !requireReachedInstruction(0x072E, "ADD", "A,C", error) ||
        !requireReachedInstruction(0x072F, "ADD", "A,B", error) ||
        !requireReachedInstruction(0x0733, "LD", "HL,$330F", error) ||
        !requireReachedInstruction(0x0736, "ADD", "HL,DE", error) ||
        instructions_.find({0x0814,true}) == instructions_.end() || instructions_.find({0x3435,true}) == instructions_.end()) {
        error = "Difficulty-table consumer/boundary proof failed.";
        return false;
    }
    if ((0x0814u-0x0796u) % 6u != 0 || (0x3435u-0x330Fu) % 42u != 0 || (0x3435u-0x330Fu)/42u != 7u) {
        error = "Difficulty-table structural dimensions changed.";
        return false;
    }
    for (std::uint16_t a=0x0796; a<0x0814; a=static_cast<std::uint16_t>(a+6u)) {
        if (rom[a] > 6u) { error = "Difficulty selector exceeded seven-record domain."; return false; }
    }
    if (!markStructuredDataRange(0x330F, 7u*42u, "u7_difficulty_records",
        "Reached selector multiplies validated 0..6 difficulty index by 42; next reached code at $3435 exactly bounds seven records.", error)) return false;

    // ---------------- U7 text pointer table + exact record grammar ----------------
    if (!requireReachedInstruction(0x2C5E, "LD", "HL,$36A5", error) ||
        !requireReachedInstruction(0x2C61, "RST", "$0018", error) ||
        !requireReachedInstruction(0x2C62, "LD", "E,(HL)", error) ||
        !requireReachedInstruction(0x2C63, "INC", "HL", error) ||
        !requireReachedInstruction(0x2C64, "LD", "D,(HL)", error) ||
        !requireReachedInstruction(0x2C84, "LD", "A,(HL)", error) ||
        !requireReachedInstruction(0x2C85, "CP", "$2F", error) ||
        !requireReachedInstruction(0x2C8C, "INC", "HL", error) ||
        !requireReachedInstruction(0x2C95, "LD", "A,(HL)", error) ||
        !requireReachedInstruction(0x2C97, "JP", "M,$2CA4", error)) return false;
    const std::uint16_t firstText = static_cast<std::uint16_t>(rom[0x36A5] | (static_cast<std::uint16_t>(rom[0x36A6]) << 8));
    if (firstText != 0x3713u || firstText <= 0x36A5u || ((firstText-0x36A5u) & 1u) != 0) {
        error = "Text pointer table self-boundary proof failed.";
        return false;
    }
    const std::size_t textPointerBytes = static_cast<std::size_t>(firstText-0x36A5u);
    if (!markStructuredDataRange(0x36A5, textPointerBytes, "u7_text_record_pointer_table",
        "Reached DrawText indexes two-byte entries; first entry points exactly to $3713, self-bounding the pointer table at that address.", error)) return false;
    std::set<std::uint16_t> textStarts;
    for (std::size_t off=0; off<textPointerBytes; off+=2) {
        const std::uint16_t pa = static_cast<std::uint16_t>(0x36A5u + off);
        const std::uint16_t target = static_cast<std::uint16_t>(rom[pa] | (static_cast<std::uint16_t>(rom[pa+1]) << 8));
        const Provenance tp = provenanceFor(target, true);
        // The indirect table also contains a few aliases into the middle of text
        // records and three small literal values.  A full DrawText record begins
        // with a screen-buffer offset: the high byte may use bit 7 for direction,
        // while bits 2..6 must be clear for the 1 KiB screen/color buffer domain.
        if (tp.valid && tp.source == CanonicalSource::U7 && target >= 0x3713u && target < 0x3E5Cu &&
            (rom[static_cast<std::uint16_t>(target + 1u)] & 0x7Cu) == 0u) textStarts.insert(target);
    }
    if (textStarts.size() < 45u) { error = "Text pointer table yielded unexpectedly few full in-U7 records."; return false; }
    for (const std::uint16_t start : textStarts) {
        std::uint16_t p = static_cast<std::uint16_t>(start + 2u); // screen offset word
        std::size_t chars = 0;
        while (p < 0x4000u && rom[p] != 0x2Fu) { ++p; ++chars; if (chars > 64u) break; }
        if (p >= 0x4000u || rom[p] != 0x2Fu || chars > 64u) {
            error = "Text record character terminator proof failed at $" + hex4(start) + ".";
            return false;
        }
        ++p; // delimiter
        if (p >= 0x4000u) { error = "Text record missing color payload."; return false; }
        const std::size_t colorBytes = (rom[p] & 0x80u) ? 1u : chars;
        const std::uint32_t endWide = static_cast<std::uint32_t>(p) + colorBytes;
        if (endWide > 0x4000u) { error = "Text record color payload escaped U7."; return false; }
        if (!markStructuredDataRange(start, static_cast<std::size_t>(endWide-start), "u7_text_record",
            "Referenced by DrawText pointer table as a full record (validated screen-offset domain); reached renderer scans characters to $2F then consumes either one high-bit fill color or one color byte per character.", error)) return false;
    }

    // ---------------- U7 sound support tables ----------------
    if (!requireReachedInstruction(0x2DC2, "LD", "HL,$3BB0", error) ||
        !requireReachedInstruction(0x2DC5, "RST", "$0010", error) ||
        !requireReachedInstruction(0x2DCA, "AND", "$1F", error) ||
        !requireReachedInstruction(0x2DCE, "AND", "$0F", error) ||
        !requireReachedInstruction(0x2DD0, "LD", "HL,$3BB8", error) ||
        !requireReachedInstruction(0x2DD3, "RST", "$0010", error)) return false;
    if (!markStructuredDataRange(0x3BB0, 8, "u7_sound_duration_table",
        "Reached sound engine masks note timing index to 0..7 before RST $10 lookup.", error)) return false;
    if (!markStructuredDataRange(0x3BB8, 16, "u7_sound_frequency_table",
        "Reached sound engine masks pitch index to 0..15 before RST $10 lookup.", error)) return false;

    // ---------------- U7 marquee address table ----------------
    if (!requireReachedInstruction(0x3EDE, "LD", "IX,$3F81", error) ||
        !requireReachedInstruction(0x3ED4, "AND", "$0F", error) ||
        !requireReachedInstruction(0x3EDA, "RES", "0,C", error) ||
        !requireReachedInstruction(0x3EE6, "ADD", "IX,BC", error) ||
        !requireReachedInstruction(0x3F10, "LD", "L,(IX+80)", error) ||
        !requireReachedInstruction(0x3F13, "LD", "H,(IX+81)", error) ||
        !requireReachedInstruction(0x3F76, "LD", "L,(IX+82)", error) ||
        !requireReachedInstruction(0x3F79, "LD", "H,(IX+83)", error)) return false;
    // A is masked to 0..15 and C is forced even.  The odd branch indexes base+$00,$10...$50;
    // the even branch adjusts by -2..+12 and then indexes through +$53.  Enumerating those
    // accesses proves the exact union $3F80-$3FE1 (97 bytes).
    if (!markStructuredDataRange(0x3F80, 0x61, "u7_marquee_screen_address_table",
        "Reached marquee routine bounds phase to 0..15 and reads six/twelve little-endian address lanes through IX offsets up to $53; exact accessed union is $3F80-$3FE0.", error)) return false;

    return true;
}

bool StatefulAnalyzer::resolveRst20(const ExecState& state, const DecodedStateInstruction& decoded, std::string& error) {
    const StateKey key{decoded.instruction.address, decoded.entryDecoderEnabled};
    if (dispatchTables_.find(key) != dispatchTables_.end()) return true;
    const std::uint16_t base = addLength(decoded.instruction.address, decoded.instruction.length());
    constexpr std::size_t maxEntries = 256;

    struct Word { std::uint16_t target = 0; bool targetState = true; };
    std::vector<Word> words;
    bool sawInvalid = false;
    std::size_t invalidIndex = 0;
    for (std::size_t i = 0; i < maxEntries; ++i) {
        const std::uint32_t wide = static_cast<std::uint32_t>(base) + static_cast<std::uint32_t>(i * 2u);
        if (wide + 1u > 0xffffu) { sawInvalid = true; invalidIndex = i; break; }

        // A Pac-Man-certified instruction byte cannot simultaneously be inline table
        // payload when this logical read still resolves to the byte-identical physical
        // base board. This prevents pointer-shaped code bytes from growing false tables.
        if (!baseInlineRangeIsCertifiedNonCode(static_cast<std::uint16_t>(wide), 2u, decoded.exitDecoderEnabled)) {
            sawInvalid = true;
            invalidIndex = i;
            break;
        }

        std::uint16_t target = 0;
        bool targetState = decoded.exitDecoderEnabled;
        if (!readWordForData(static_cast<std::uint16_t>(wide), decoded.exitDecoderEnabled, target, targetState) ||
            !canFetch(target, targetState) || !targetIsCompatibleWithCertifiedBase(target, targetState)) {
            sawInvalid = true;
            invalidIndex = i;
            break;
        }
        words.push_back({target, targetState});
    }

    struct Choice { std::size_t count = std::numeric_limits<std::size_t>::max(); int priority = 99; std::string reason; } choice;
    auto consider = [&](std::size_t count, int priority, const std::string& reason) {
        if (count == 0 || count > words.size()) return;
        if (count < choice.count || (count == choice.count && priority < choice.priority)) choice = {count, priority, reason};
    };

    // If this RST $20 itself still executes from the physical base board, prefer the
    // exact address-only table boundary certified by ArcadePacRip. Re-prove that every
    // byte in the proposed range remains base non-code in the current decoder state.
    const Provenance siteProvenance = provenanceFor(decoded.instruction.address, decoded.bytes.empty() ? decoded.entryDecoderEnabled
                                                                                                        : decoded.bytes.front().decoderEnabled);
    if (siteProvenance.valid && siteProvenance.canonicalIndex < 0x4000u &&
        siteProvenance.canonicalIndex == decoded.instruction.address) {
        for (const auto& certified : kPacmanCertifiedRst20Ranges) {
            if (certified.site != decoded.instruction.address || certified.start != base) continue;
            const std::size_t length = static_cast<std::size_t>(certified.endExclusive - certified.start);
            if ((length & 1u) != 0u || !baseInlineRangeIsCertifiedNonCode(certified.start, length, decoded.exitDecoderEnabled)) {
                error = "Certified RST $20 inline range failed Ms. Pac-Man revalidation at $" + hex4(certified.site) + ".";
                return false;
            }
            consider(length / 2u, -1, "certified Pac-Man base inline boundary revalidated on Ms. Pac-Man bytes");
            break;
        }
    }

    // Generic fallback for daughterboard-specific RST $20 sites. The same conservative
    // structural heuristics remain, but physical-base candidate targets must now be
    // certified instruction starts and candidate payload bytes may not consume certified code.
    for (const auto& word : words) {
        if (word.target >= base) {
            const std::uint32_t diff = static_cast<std::uint32_t>(word.target) - base;
            if ((diff & 1u) == 0u) consider(diff / 2u, 1, "first aligned local handler target");
        }
    }
    for (const auto& kv : instructions_) {
        if (kv.first.pc <= base || kv.first.decoderEnabled != decoded.exitDecoderEnabled) continue;
        const std::uint32_t diff = static_cast<std::uint32_t>(kv.first.pc) - base;
        if ((diff & 1u) == 0u) consider(diff / 2u, 0, "previously proven code boundary");
        break;
    }
    if (sawInvalid && invalidIndex > 0) consider(invalidIndex, 3, "first certified-invalid pointer/table word");
    if (choice.count == std::numeric_limits<std::size_t>::max()) return false;

    const std::uint16_t end = static_cast<std::uint16_t>(base + static_cast<std::uint16_t>(choice.count * 2u));
    for (std::size_t i = 0; i < choice.count; ++i) {
        const std::uint16_t target = words[i].target;
        if (target >= base && target < end) return false;
    }

    DispatchTable table;
    table.site = key;
    table.start = base;
    table.end = end;
    table.boundaryReason = choice.reason;
    for (std::size_t i = 0; i < choice.count; ++i) {
        const std::uint16_t entryAddress = static_cast<std::uint16_t>(base + static_cast<std::uint16_t>(i * 2u));
        bool markedExitState = decoded.exitDecoderEnabled;
        if (!markInlineData(entryAddress, 2, decoded.exitDecoderEnabled, markedExitState, error)) return false;
        if (markedExitState != words[i].targetState) {
            error = "RST $20 table state proof disagrees at $" + hex4(entryAddress) + ".";
            return false;
        }
        table.entries.push_back({entryAddress, words[i].target, words[i].targetState});
        addEdge(decoded.instruction.address, state.decoderEnabled, words[i].target, words[i].targetState, "rst20_dispatch");
        enqueue({words[i].target, words[i].targetState, state.returnStack});
    }
    dispatchTables_.emplace(key, std::move(table));
    return true;
}


bool StatefulAnalyzer::applyU7CertifiedNonCodeDifferential(std::string& error) {
    // U7 differential analysis: U7 is the daughterboard replacement for logical $3000-$3FFF.
    // Reuse Pac-Man non-code ownership only for bytes that are *still UNKNOWN*,
    // byte-identical to the corresponding physical Pac-Man $3000-$3FFF byte, and
    // certified non-code by the frozen 5,414-boundary Pac-Man ledger.  To avoid
    // crediting isolated one-byte coincidences, only contiguous eligible runs of
    // length >= 2 are accepted.
    preU7DiffCodeRefs_ = codeRefs_;
    preU7DiffDataRefs_ = dataRefs_;
    u7DifferentialSpans_.clear();

    constexpr std::size_t u7Base = 0x5800u;
    std::array<bool,0x1000> eligible{};
    for (std::size_t off = 0; off < 0x1000u; ++off) {
        const std::size_t canonical = u7Base + off;
        const std::size_t baseAddr = 0x3000u + off;
        if (codeRefs_[canonical] != 0 || dataRefs_[canonical] != 0) continue;
        if (certifiedBaseCodeMask_[baseAddr]) continue;
        if (images_.decoderEnabled[baseAddr] != images_.decoderDisabled[baseAddr]) continue;
        eligible[off] = true;
    }

    std::size_t off = 0;
    while (off < eligible.size()) {
        if (!eligible[off]) { ++off; continue; }
        const std::size_t start = off;
        while (off < eligible.size() && eligible[off]) ++off;
        const std::size_t length = off - start;
        const bool accepted = length >= 2u;
        u7DifferentialSpans_.push_back({static_cast<std::uint16_t>(start), static_cast<std::uint16_t>(off), accepted});
        if (!accepted) continue;
        for (std::size_t i = start; i < off; ++i) {
            const std::size_t canonical = u7Base + i;
            if (codeRefs_[canonical] != 0 || dataRefs_[canonical] != 0) {
                error = "U7 differential attempted to overwrite pre-existing ownership at decoded offset $" +
                    hex4(static_cast<std::uint16_t>(i)) + ".";
                return false;
            }
            dataRefs_[canonical] = 1;
        }
    }
    return true;
}


bool StatefulAnalyzer::markResidualLogicalRange(std::uint16_t start, std::size_t length, bool code,
                                               const std::string& kind, const std::string& proof,
                                               std::string& error) {
    if (length == 0 || static_cast<std::uint32_t>(start) + length > 0x10000u) {
        error = "Invalid residual ownership closure residual range at $" + hex4(start) + ".";
        return false;
    }
    std::set<std::size_t> seen;
    std::size_t newly = 0;
    for (std::size_t i = 0; i < length; ++i) {
        const std::uint16_t address = static_cast<std::uint16_t>(start + static_cast<std::uint16_t>(i));
        const Provenance p = provenanceFor(address, true);
        if (!p.valid || p.canonicalIndex < 0x4000u || p.canonicalIndex >= kCanonicalProgramBytes) {
            error = "residual ownership closure residual range at $" + hex4(address) + " did not resolve to daughterboard storage.";
            return false;
        }
        if (!seen.insert(p.canonicalIndex).second) continue;
        const bool knownCode = codeRefs_[p.canonicalIndex] != 0;
        const bool knownData = dataRefs_[p.canonicalIndex] != 0;
        if (knownCode || knownData) {
            if (code && knownData && !knownCode) {
                error = "residual ownership closure CODE proof conflicts with pre-existing DATA at $" + hex4(address) + ".";
                return false;
            }
            continue;
        }
        if (code) codeRefs_[p.canonicalIndex] = 1;
        else dataRefs_[p.canonicalIndex] = 1;
        ++newly;
    }
    if (newly != 0) residualClosureSpans_.push_back({kind, start,
        static_cast<std::uint16_t>(start + static_cast<std::uint16_t>(length)), newly, code, proof});
    return true;
}

bool StatefulAnalyzer::markResidualCanonicalRange(CanonicalSource source, std::uint16_t decodedStart,
                                                 std::size_t length, bool code, const std::string& kind,
                                                 const std::string& proof, std::string& error) {
    if (source == CanonicalSource::None || decodedStart + length > sourceSize(source)) {
        error = "Invalid residual ownership closure canonical residual range.";
        return false;
    }
    std::size_t newly = 0;
    const std::size_t base = sourceBaseIndex(source);
    for (std::size_t i = 0; i < length; ++i) {
        const std::size_t index = base + decodedStart + i;
        const bool knownCode = codeRefs_[index] != 0;
        const bool knownData = dataRefs_[index] != 0;
        if (knownCode || knownData) {
            if (code && knownData && !knownCode) {
                error = "residual ownership closure canonical CODE proof conflicts with pre-existing DATA.";
                return false;
            }
            continue;
        }
        if (code) codeRefs_[index] = 1;
        else dataRefs_[index] = 1;
        ++newly;
    }
    std::uint16_t logicalStart = decodedStart;
    if (source == CanonicalSource::U5) logicalStart = static_cast<std::uint16_t>(0x8000u + decodedStart);
    else if (source == CanonicalSource::U7) logicalStart = static_cast<std::uint16_t>(0x3000u + decodedStart);
    else if (source == CanonicalSource::U6) {
        logicalStart = decodedStart < 0x0800u ? static_cast<std::uint16_t>(0x9000u + decodedStart)
                                             : static_cast<std::uint16_t>(0x8800u + decodedStart - 0x0800u);
    }
    if (newly != 0) residualClosureSpans_.push_back({kind, logicalStart,
        static_cast<std::uint16_t>(logicalStart + static_cast<std::uint16_t>(length)), newly, code, proof});
    return true;
}

bool StatefulAnalyzer::applyResidualClosure(std::string& error) {
    // residual ownership closure is deliberately a residual adjudication stage. Preserve the exact
    // certified U7 differential analysis ledger, then allow only fail-closed, explicitly bounded rules
    // to change UNKNOWN bytes. Previously classified bytes are never relabeled here.
    preResidualCodeRefs_ = codeRefs_;
    preResidualDataRefs_ = dataRefs_;
    residualClosureSpans_.clear();
    const auto& rom = images_.decoderEnabled;

    auto requireBytes = [&](std::uint16_t start, const std::vector<std::uint8_t>& expected,
                            const std::string& label) -> bool {
        if (static_cast<std::uint32_t>(start) + expected.size() > rom.size()) {
            error = label + " escaped logical ROM.";
            return false;
        }
        for (std::size_t i = 0; i < expected.size(); ++i) {
            if (rom[start+i] != expected[i]) {
                error = label + " byte proof failed at $" + hex4(static_cast<std::uint16_t>(start+i)) + ".";
                return false;
            }
        }
        return true;
    };

    // U5 $8000-$81EF is the physical 40x8 patch staging area. Credit only bytes
    // that remain UNKNOWN after U7 differential analysis. Exact hardware source slots are payload
    // storage; complement gaps are required to be all $FF before being called padding.
    std::array<bool,0x1F0> patchSlot{};
    for (const auto& patch : mspacman::DaughterboardCodec::patchRegions()) {
        if (patch.source < 0x8000u || patch.source + 8u > 0x81F0u) {
            error = "residual ownership closure patch source escaped U5 staging area.";
            return false;
        }
        for (std::uint16_t n=0; n<8; ++n) patchSlot[patch.source - 0x8000u + n] = true;
        if (!markResidualLogicalRange(patch.source, 8, false, "u5_patch_payload_storage",
            "Exact daughterboard 40x8 source-slot map; only bytes still UNKNOWN after U7 differential analysis are credited.", error)) return false;
    }
    std::size_t g=0;
    while (g<patchSlot.size()) {
        if (patchSlot[g]) { ++g; continue; }
        const std::size_t begin=g;
        while (g<patchSlot.size() && !patchSlot[g]) ++g;
        for (std::size_t i=begin; i<g; ++i) {
            if (rom[0x8000u+i] != 0xFFu) {
                error = "U5 patch staging complement is not all $FF at $" + hex4(static_cast<std::uint16_t>(0x8000u+i)) + ".";
                return false;
            }
        }
        if (!markResidualLogicalRange(static_cast<std::uint16_t>(0x8000u+begin), g-begin, false,
            "u5_patch_staging_gap_padding",
            "Maximal complement gap between exact 8-byte hardware patch-source slots inside $8000-$81EF; canonical bytes are all $FF.", error)) return false;
    }

    if (rom[0x8637] != 0xFF || rom[0x865B] != 0xFF) { error = "U5 residual sprite sequence terminator proof failed."; return false; }
    if (!markResidualLogicalRange(0x862F,9,false,"u5_unreferenced_sprite_sequence",
        "One of 21 back-to-back FF-terminated sprite-code siblings in the independently reparsed $8614-$869B family.",error) ||
        !markResidualLogicalRange(0x8653,9,false,"u5_unreferenced_sprite_sequence",
        "One of 21 back-to-back FF-terminated sprite-code siblings in the independently reparsed $8614-$869B family.",error)) return false;
    if (!markResidualLogicalRange(0x87B2,3,false,"u5_unused_eighth_fruit_descriptor",
        "Three-byte sibling immediately after the seven selector-reachable fruit descriptors; selector is explicitly bounded to 0..6; next reached code starts at $87B5.",error)) return false;

    const std::array<std::pair<std::uint16_t,std::uint16_t>,9> u6Gaps = {{{0x9015,0x9018},{0x9108,0x9109},
        {0x92E9,0x92EC},{0x93DC,0x93F9},{0x8B2B,0x8B2C},{0x8B4E,0x8B4F},{0x8D24,0x8D27},{0x8E3F,0x8E40},{0x97C4,0x97D0}}};
    for (const auto& r : u6Gaps) if (!markResidualLogicalRange(r.first, r.second-r.first, false,
        "u6_bounded_structure_padding",
        "Exact bounded gap between independently proved neighboring U6 structures; only still-UNKNOWN bytes are credited.", error)) return false;
    static const std::string gccMessage = "GENERAL COMPUTER  CORPORATION   Hello, Nakamura!";
    if (gccMessage.size()!=48u || !std::equal(gccMessage.begin(),gccMessage.end(),rom.begin()+0x97D0)) {
        error = "U6 physical GCC developer message proof failed."; return false;
    }
    if (!markResidualLogicalRange(0x97D0,48,false,"u6_physical_developer_message",
        "Exact 48-byte printable string at the physical end of decoded U6; independently bounded by ROM end $9800 and preceded by 12 $FF bytes.",error) ||
        !markResidualLogicalRange(0x9169,6,false,"u6_unreferenced_packed_route_fragment",
        "Exact six-byte hole inside the descriptor-bounded packed fruit-route pool: referenced paths end at $9169 and resume at $916F; maze stream begins $9179. Retained as dead/unreferenced route DATA, not generic padding.",error)) return false;

    // Three byte-complete dead-code islands. Their exact canonical bytes are pinned
    // here; the independent residual ownership closure verifier decodes the same islands separately.
    if (!requireBytes(0x367F,{0x3A,0x08,0x4D,0xE6,0x0F,0xCB,0x3F,0xCB,0x3F,0x2F,0x1E,0x1C,0x83,0xFE,0x18,0x20,0x02,0x3E,0x36,0x32,0x0A,0x4C,0xC9},"dead code $367F") ||
        !markResidualLogicalRange(0x367F,0x17,true,"u7_bounded_dead_code_367f",
        "Locally decoded byte-complete CFG; one internal conditional branch; terminal RET at $3695; no external edge or DATA collision.",error) ||
        !requireBytes(0x39E0,{0x3D,0x4F,0x21,0x00,0x4D,0xD7,0xEB,0x79,0x21,0xF2,0x39,0xD7,0x12,0x23,0x13,0x7E,0x12,0xC9},"dead code $39E0") ||
        !markResidualLogicalRange(0x39E0,0x12,true,"u7_bounded_dead_code_39e0",
        "Byte-complete local CFG ending RET at $39F1; only returning RST $10 table-lookup intrinsics leave the range.",error) ||
        !requireBytes(0x3A00,{0x3E,0x01,0x32,0x14,0x4E,0xC9},"dead code $3A00") ||
        !markResidualLogicalRange(0x3A00,6,true,"u7_bounded_dead_code_3a00",
        "Three-instruction dead helper: LD A,$01; LD ($4E14),A; RET; byte-complete and immediately followed by established DATA.",error)) return false;

    // Live consumers prove these legacy U7 record banks.
    if (!requireReachedInstruction(0x2BF9,"LD","DE,$3B08",error) ||
        !requireReachedInstruction(0x2BFD,"LD","C,$07",error)) return false;
    if (!markResidualLogicalRange(0x3B08,0x28,false,"u7_fruit_history_sprite_color_pairs",
        "Reached/decoded fruit-history renderer uses two-byte sprite/color pairs at $3B08; capped source offset plus seven-pair loop bounds the live window, with one final sibling pair completing the block at the $3B30 sound-table boundary.",error) ||
        !markResidualLogicalRange(0x3B40,0x40,false,"u7_sound_effect_record_bank_ch2",
        "Channel-2 effect base is $3B40; sound engine selects among eight bits and copies exact 8-byte parameter records, proving the complete eight-record bank.",error) ||
        !markResidualLogicalRange(0x3B80,0x30,false,"u7_sound_effect_record_bank_ch3",
        "Channel-3 effect base is $3B80 and uses the same 8-byte parameter-record engine; six non-overlapping records end at the proved $3BB0 timing table.",error)) return false;

    const std::array<std::tuple<std::uint16_t,std::uint16_t,const char*>,9> textResidual = {{
        {0x3816,0x3817,"u7_text_bank_separator"},{0x3817,0x3823,"u7_dead_text_record_3817"},
        {0x38D5,0x38DC,"u7_dead_text_record_38d5"},{0x38DC,0x38DD,"u7_text_bank_separator"},
        {0x38DD,0x38EA,"u7_dead_text_record_38dd"},{0x3D32,0x3D3A,"u7_dead_text_record_3d32"},
        {0x3D53,0x3D57,"u7_text_bank_separator"},{0x3DC4,0x3DC6,"u7_text_bank_separator"},
        {0x3E4F,0x3E5A,"u7_dead_text_record_3e4f"}}};
    for (const auto& r : textResidual) if (!markResidualLogicalRange(std::get<0>(r),std::get<1>(r)-std::get<0>(r),false,
        std::get<2>(r),"Independently bounded legacy DrawText-format sibling/separator inside the U7 text bank; only still-UNKNOWN bytes are credited.",error)) return false;

    if (!markResidualLogicalRange(0x39F2,0x0E,false,"u7_dead_lookup_pair_table_39f2",
        "Dead routine $39E0 explicitly indexes $39F2 through RST $10 then consumes the indexed byte and following byte; next independently decoded code starts $3A00.",error)) return false;

    if (!std::equal(rom.begin()+0x3FD0,rom.begin()+0x3FE0,rom.begin()+0x3FE0)) {
        error = "Duplicate marquee-tail proof failed."; return false;
    }
    if (!markResidualLogicalRange(0x3FE0,0x10,false,"u7_duplicate_marquee_tail",
        "Exact 16-byte duplicate of $3FD0-$3FDF; only previously UNKNOWN bytes are credited.",error) ||
        !markResidualCanonicalRange(CanonicalSource::U7,0x0FF0,8,false,"u7_decoder_trap_hidden_physical_storage",
        "Physical U7 bytes in the exact $3FF0-$3FF7 decoder-disable trap; hardware switches to Pac-Man view before any read/fetch, so these bytes are unreachable dead storage.",error)) return false;
    if (rom[0x3E8A]!=0xC9 || rom[0x3F7F]!=0xD0) { error = "Final one-byte boundary filler proof failed."; return false; }
    if (!markResidualLogicalRange(0x3E8A,1,false,"u7_rst20_table_to_code_padding",
        "Exactly one byte between the 17-word RST $20 table $3E68-$3E89 and reached code at $3E8B; canonical byte is $C9 and has no ownership/reference.",error) ||
        !markResidualLogicalRange(0x3F7F,1,false,"u7_code_to_marquee_table_padding",
        "Single unreachable byte after reached unconditional RET at $3F7E and before proved marquee table start $3F80; canonical byte $D0.",error)) return false;

    // Freeze the residual delta contract: exactly 47 CODE + 610 DATA bytes and no
    // non-UNKNOWN pre-residual ownership closure byte may have changed ownership class.
    std::size_t newCode=0,newData=0;
    for (std::size_t i=0;i<kCanonicalProgramBytes;++i) {
        const bool preC=preResidualCodeRefs_[i]!=0, preD=preResidualDataRefs_[i]!=0;
        const bool postC=codeRefs_[i]!=0, postD=dataRefs_[i]!=0;
        if (preC || preD) {
            if (preC!=postC || preD!=postD) { error="residual ownership closure modified pre-existing ownership."; return false; }
            continue;
        }
        if (postC && postD) { error="residual ownership closure created dual-use ownership."; return false; }
        if (postC) ++newCode; else if (postD) ++newData;
    }
    if (newCode!=47u || newData!=610u) {
        error = "residual ownership closure residual delta mismatch: CODE="+std::to_string(newCode)+" DATA="+std::to_string(newData)+".";
        return false;
    }
    for (std::size_t i=0;i<kCanonicalProgramBytes;++i) {
        if (codeRefs_[i]==0 && dataRefs_[i]==0) { error="residual ownership closure left UNKNOWN canonical storage at index "+std::to_string(i)+"."; return false; }
    }
    return true;
}

bool StatefulAnalyzer::prepareCertifiedBaseEvidence(std::string& error) {
    certifiedBaseCodeMask_.fill(false);
    certifiedBaseStartMask_.fill(false);
    certifiedBaseBoundaryConflictDetails_.clear();
    certifiedBaseClassificationConflictDetails_.clear();

    std::vector<std::uint8_t> base(0x4000);
    for (std::size_t i = 0; i < base.size(); ++i) base[i] = images_.decoderDisabled[i];

    std::size_t codeBytes = 0;
    for (const auto& boundary : kPacmanCertifiedBoundaries) {
        const std::size_t pc = boundary.pc;
        const std::size_t length = boundary.length;
        if (length == 0 || pc + length > base.size()) {
            certifiedBaseBoundaryConflictDetails_.push_back("boundary range escaped 16-KiB base at $" + hex4(boundary.pc));
            continue;
        }
        const auto decoded = disassembler_.decode(base, boundary.pc);
        if (decoded.length() != length) {
            certifiedBaseBoundaryConflictDetails_.push_back("decoder length mismatch at $" + hex4(boundary.pc) +
                ": evidence=" + std::to_string(length) + " decoded=" + std::to_string(decoded.length()));
            continue;
        }
        if (certifiedBaseStartMask_[pc]) {
            certifiedBaseBoundaryConflictDetails_.push_back("duplicate certified instruction start at $" + hex4(boundary.pc));
            continue;
        }
        certifiedBaseStartMask_[pc] = true;
        for (std::size_t n = 0; n < length; ++n) {
            const std::size_t index = pc + n;
            if (certifiedBaseCodeMask_[index]) {
                certifiedBaseBoundaryConflictDetails_.push_back("overlapping certified instruction ownership at $" +
                    hex4(static_cast<std::uint16_t>(index)));
                continue;
            }
            certifiedBaseCodeMask_[index] = true;
            ++codeBytes;
        }
    }

    const std::size_t dataBytes = 0x4000u - codeBytes;
    if (kPacmanCertifiedBoundaries.size() != 5414u || codeBytes != 11601u || dataBytes != 4783u) {
        std::ostringstream ss;
        ss << "certified Pac-Man boundary denominator mismatch: starts=" << kPacmanCertifiedBoundaries.size()
           << " code=" << codeBytes << " noncode=" << dataBytes;
        certifiedBaseBoundaryConflictDetails_.push_back(ss.str());
    }
    if (!certifiedBaseBoundaryConflictDetails_.empty()) {
        error = "Certified Pac-Man boundary preparation failed: " + certifiedBaseBoundaryConflictDetails_.front();
        return false;
    }
    return true;
}

bool StatefulAnalyzer::applyCertifiedBaseEvidence(std::string& error) {
    certifiedBaseClassificationConflictDetails_.clear();

    // Audit the recovered Ms. Pac-Man execution evidence before importing base ownership.
    // A byte that ArcadePacRip proved non-code may not silently become physical base-board CODE here.
    // Enabled-state patch aliases map to U5 provenance and therefore do not trigger this check.
    for (std::size_t i = 0; i < 0x4000u; ++i) {
        if (!certifiedBaseCodeMask_[i] && codeRefs_[i] != 0) {
            certifiedBaseClassificationConflictDetails_.push_back("stateful physical-base CODE contradicts certified non-code at $" +
                hex4(static_cast<std::uint16_t>(i)));
        }
    }
    if (!certifiedBaseClassificationConflictDetails_.empty()) {
        error = "Certified Pac-Man classification audit failed: " + certifiedBaseClassificationConflictDetails_.front();
        return false;
    }

    // Credit only the physical base-board storage object. Logical enabled overlays/U7 retain their own provenance.
    for (std::size_t i = 0; i < 0x4000u; ++i) {
        if (certifiedBaseCodeMask_[i]) {
            if (codeRefs_[i] == 0) codeRefs_[i] = 1;
        } else {
            if (dataRefs_[i] == 0) dataRefs_[i] = 1;
        }
    }
    return true;
}

void StatefulAnalyzer::seed(std::uint16_t pc, bool decoderEnabled, const std::string& name) {
    const StateKey key{pc, decoderEnabled};
    roots_.emplace(key, name);
    enqueue({pc, decoderEnabled, {}});
}

void StatefulAnalyzer::enqueue(const ExecState& state) {
    if (visitedExecStates_.find(state) == visitedExecStates_.end()) worklist_.push_back(state);
}

void StatefulAnalyzer::addEdge(std::uint16_t fromPc, bool fromState, std::uint16_t toPc, bool toState, const std::string& kind) {
    edges_.insert({fromPc, fromState, toPc, toState, kind});
}

void StatefulAnalyzer::observeInstruction(const DecodedStateInstruction& decoded, std::string& error) {
    const StateKey key{decoded.instruction.address, decoded.entryDecoderEnabled};
    const int ownerId = static_cast<int>(decoded.instruction.address);
    for (const auto& byte : decoded.bytes) {
        if (!byte.mapped) continue;
        const std::size_t stateIndex = byte.decoderEnabled ? 1u : 0u;
        if (hardData_[stateIndex][byte.address]) {
            std::ostringstream ss;
            ss << "code overlaps proven inline data at $" << hex4(byte.address) << " in " << stateName(byte.decoderEnabled) << " state";
            overlapDetails_.push_back(ss.str());
        }
        int& existing = logicalOwners_[stateIndex][byte.address];
        if (existing >= 0 && existing != ownerId) {
            std::ostringstream ss;
            ss << "logical instruction overlap at $" << hex4(byte.address) << " in " << stateName(byte.decoderEnabled)
               << " state; existing owner=" << existing << " new owner=" << ownerId;
            overlapDetails_.push_back(ss.str());
        } else {
            existing = ownerId;
        }
        const Provenance p = provenanceFor(byte.address, byte.decoderEnabled);
        if (!p.valid || p.canonicalIndex >= codeRefs_.size()) {
            error = "Unable to map executing logical byte $" + hex4(byte.address) + " to canonical provenance.";
            return;
        }
        ++codeRefs_[p.canonicalIndex];
    }
    for (const auto& data : decoded.romDataReads) {
        const Provenance p = provenanceFor(data.address, data.decoderEnabled);
        if (p.valid && p.canonicalIndex < dataRefs_.size()) ++dataRefs_[p.canonicalIndex];
    }
    instructions_.emplace(key, decoded);
}

bool StatefulAnalyzer::run(std::string& error) {
    worklist_.clear();
    workIndex_ = 0;
    visitedExecStates_.clear();
    instructions_.clear();
    edges_.clear();
    roots_.clear();
    for (auto& state : logicalOwners_) state.fill(-1);
    for (auto& state : hardData_) state.fill(false);
    codeRefs_.fill(0);
    dataRefs_.fill(0);
    preU7DiffCodeRefs_.fill(0);
    preU7DiffDataRefs_.fill(0);
    structuredDataMask_.fill(false);
    structuredDataSpans_.clear();
    u7DifferentialSpans_.clear();
    indirectTransfers_.clear();
    dispatchTables_.clear();
    rst28InlineSites_.clear();
    rst30InlineSites_.clear();
    inlineFiveByteCallSites_.clear();
    unmappedEntries_.clear();
    overlapDetails_.clear();
    certifiedBaseCodeMask_.fill(false);
    certifiedBaseStartMask_.fill(false);
    certifiedBaseBoundaryConflictDetails_.clear();
    certifiedBaseClassificationConflictDetails_.clear();
    stackOverflows_ = 0;
    summary_ = AnalysisSummary{};

    if (!prepareCertifiedBaseEvidence(error)) return false;

    const std::array<std::uint16_t, 8> vectors = {0x0000,0x0008,0x0010,0x0018,0x0020,0x0028,0x0030,0x0038};
    for (const auto vector : vectors) seed(vector, true, "z80_vector_" + hex4(vector));
    seed(0x0038, false, "im1_vector_from_disabled");
    seed(mspacman::DaughterboardCodec::decoderEnableTrapStart(), false, "decoder_enable_trap");

    // Canonical startup loads I=$3F and the hardware interrupt vector supplies $FA.
    // Reading $3FFA/$3FFB is inside the decoder-enable trap, so the vector is read
    // from the Ms. Pac-Man view and resolves to $3000 in this canonical set.
    const std::uint16_t irqEntry = static_cast<std::uint16_t>(images_.decoderEnabled[0x3ffa] |
        (static_cast<std::uint16_t>(images_.decoderEnabled[0x3ffb]) << 8));
    if (irqEntry != 0xffffu) seed(irqEntry, true, "derived_im2_irq_entry");
    // Audited direct redirects embedded in enabled patch bytes. Each
    // source instruction is independently decoded from the canonical Ms. Pac-Man
    // logical image and must resolve to the expected daughterboard target before the
    // target is accepted as a discovery root. The manifest contains addresses only.
    for (const auto& evidence : kPatchRedirects) {
        bool sourceInsidePatch = false;
        for (const auto& patch : mspacman::DaughterboardCodec::patchRegions()) {
            if (evidence.sourcePc >= patch.destination && evidence.sourcePc < static_cast<std::uint16_t>(patch.destination + 8u)) {
                sourceInsidePatch = true;
                break;
            }
        }
        DecodedStateInstruction source;
        if (!sourceInsidePatch || !decodeAt(evidence.sourcePc, true, source) || source.instruction.target < 0 ||
            static_cast<std::uint16_t>(source.instruction.target) != evidence.target ||
            (source.instruction.flow != disasm::FlowKind::Jump && source.instruction.flow != disasm::FlowKind::Call) ||
            !canFetch(evidence.target, source.exitDecoderEnabled)) {
            error = "Patch redirect evidence failed at $" + hex4(evidence.sourcePc) +
                " -> expected $" + hex4(evidence.target);
            return false;
        }
        const Provenance first = source.bytes.empty() ? Provenance{} : provenanceFor(source.bytes.front().address, source.bytes.front().decoderEnabled);
        if (!first.valid || first.source != CanonicalSource::U5 || !first.alias) {
            error = "Patch redirect source at $" + hex4(evidence.sourcePc) + " did not execute from the U5 overlay alias.";
            return false;
        }
        seed(evidence.target, source.exitDecoderEnabled, "verified_patch_redirect_" + hex4(evidence.sourcePc));
    }

    // structured-data analysis: the pellet setup patch uses a deliberate PUSH HL / RET trampoline
    // to return into the byte-identical base renderer at $2453 or $2492.  Our abstract
    // return stack cannot infer a runtime PUSH of an arbitrary register value, so prove
    // both continuation constants and the common trampoline bytes before seeding them.
    {
        DecodedStateInstruction a, b, c, d;
        if (!decodeAt(0x947C, true, a) || a.instruction.mnemonic != "LD" || a.instruction.operands != "HL,$2453" ||
            !decodeAt(0x9481, true, b) || b.instruction.mnemonic != "LD" || b.instruction.operands != "HL,$2492" ||
            !decodeAt(0x9484, true, c) || c.instruction.mnemonic != "PUSH" || c.instruction.operands != "HL" ||
            !decodeAt(0x9498, true, d) || d.instruction.mnemonic != "RET") {
            error = "Pellet-renderer manual-return trampoline proof failed.";
            return false;
        }
        seed(0x2453, true, "verified_pellet_draw_return_947c");
        seed(0x2492, true, "verified_pellet_scan_return_9481");
    }

    while (workIndex_ < worklist_.size()) {
        if (visitedExecStates_.size() >= kMaxExecStates) {
            error = "Stateful CFG exceeded safety limit of " + std::to_string(kMaxExecStates) + " execution states.";
            return false;
        }
        const ExecState state = worklist_[workIndex_++];
        if (!visitedExecStates_.insert(state).second) continue;

        const StateKey stateKey{state.pc, state.decoderEnabled};
        DecodedStateInstruction decoded;
        auto cached = instructions_.find(stateKey);
        if (cached != instructions_.end()) {
            decoded = cached->second;
        } else {
            if (!decodeAt(state.pc, state.decoderEnabled, decoded)) {
                unmappedEntries_.insert(stateKey);
                continue;
            }
            observeInstruction(decoded, error);
            if (!error.empty()) return false;
        }

        const auto& in = decoded.instruction;
        const std::uint16_t next = addLength(in.address, in.length());
        const bool exitState = decoded.exitDecoderEnabled;

        auto queueTarget = [&](std::uint16_t target, const std::vector<std::uint16_t>& stack, const std::string& kind) {
            addEdge(in.address, state.decoderEnabled, target, exitState, kind);
            enqueue({target, exitState, stack});
        };

        switch (in.flow) {
            case disasm::FlowKind::Normal:
            case disasm::FlowKind::Halt:
                queueTarget(next, state.returnStack, in.flow == disasm::FlowKind::Halt ? "halt_resume" : "fallthrough");
                break;

            case disasm::FlowKind::Jump:
            case disasm::FlowKind::RelativeJump:
                if (in.indirect || in.target < 0) {
                    if (in.address != 0x0027 && in.address != 0x0064) indirectTransfers_.insert(stateKey);
                } else {
                    queueTarget(static_cast<std::uint16_t>(in.target), state.returnStack,
                                in.flow == disasm::FlowKind::RelativeJump ? "relative_jump" : "jump");
                }
                if (in.conditional) queueTarget(next, state.returnStack, "branch_not_taken");
                break;

            case disasm::FlowKind::Call: {
                if (in.conditional) queueTarget(next, state.returnStack, "call_not_taken");

                // Byte-proven Pac-Man-family inline-call convention retained by this
                // canonical Ms. Pac-Man set. CALL $2BCD at $2B70 pushes $2B73; the
                // callee POPs that pointer, consumes exactly five inline bytes, then
                // arranges a return to $2B78. Both decoder views are byte-identical
                // across the call site/callee prefix, so this is control-format proof,
                // not inherited semantic naming.
                const bool inlineConvention = in.address == 0x2b70 && in.target == 0x2bcd &&
                    images_.decoderEnabled[0x2b70] == 0xcd && images_.decoderEnabled[0x2b71] == 0xcd && images_.decoderEnabled[0x2b72] == 0x2b &&
                    images_.decoderEnabled[0x2bcd] == 0xe1 && images_.decoderEnabled[0x2bce] == 0x5e &&
                    images_.decoderEnabled[0x2bcf] == 0x23 && images_.decoderEnabled[0x2bd0] == 0x56 &&
                    std::equal(images_.decoderEnabled.begin() + 0x2b70, images_.decoderEnabled.begin() + 0x2bd1,
                               images_.decoderDisabled.begin() + 0x2b70);
                if (inlineConvention) {
                    bool continuationState = exitState;
                    if (!markInlineData(next, 5, exitState, continuationState, error)) return false;
                    inlineFiveByteCallSites_.insert(stateKey);
                    if (state.returnStack.size() >= kMaxReturnStack) {
                        ++stackOverflows_;
                        addEdge(in.address, state.decoderEnabled, static_cast<std::uint16_t>(next + 5u), continuationState, "inline_call_stack_limit_continuation");
                        enqueue({static_cast<std::uint16_t>(next + 5u), continuationState, state.returnStack});
                        break;
                    }
                    std::vector<std::uint16_t> pushed = state.returnStack;
                    pushed.push_back(static_cast<std::uint16_t>(next + 5u));
                    addEdge(in.address, state.decoderEnabled, 0x2bcd, continuationState, "inline_five_byte_call");
                    enqueue({0x2bcd, continuationState, pushed});
                    break;
                }

                if (in.target < 0) {
                    indirectTransfers_.insert(stateKey);
                    queueTarget(next, state.returnStack, "unresolved_call_return_assumption");
                    break;
                }
                const std::uint16_t target = static_cast<std::uint16_t>(in.target);
                if (!canFetch(target, exitState)) {
                    queueTarget(next, state.returnStack, "external_call_return_assumption");
                    break;
                }
                if (state.returnStack.size() >= kMaxReturnStack) {
                    ++stackOverflows_;
                    queueTarget(next, state.returnStack, "stack_limit_return_assumption");
                    break;
                }
                std::vector<std::uint16_t> pushed = state.returnStack;
                pushed.push_back(next);
                queueTarget(target, pushed, "call");
                break;
            }

            case disasm::FlowKind::Restart: {
                if (in.target == 0x20) {
                    if (resolveRst20(state, decoded, error)) break;
                    if (!error.empty()) return false;
                }
                if (in.target == 0x28) {
                    bool continuationState = exitState;
                    if (!markInlineData(next, 2, exitState, continuationState, error)) return false;
                    rst28InlineSites_.insert(stateKey);
                    addEdge(in.address, state.decoderEnabled, static_cast<std::uint16_t>(next + 2u), continuationState, "rst28_inline_continuation");
                    enqueue({static_cast<std::uint16_t>(next + 2u), continuationState, state.returnStack});
                    break;
                }
                if (in.target == 0x30) {
                    bool continuationState = exitState;
                    if (!markInlineData(next, 3, exitState, continuationState, error)) return false;
                    rst30InlineSites_.insert(stateKey);
                    addEdge(in.address, state.decoderEnabled, static_cast<std::uint16_t>(next + 3u), continuationState, "rst30_inline_continuation");
                    enqueue({static_cast<std::uint16_t>(next + 3u), continuationState, state.returnStack});
                    break;
                }
                if (in.target < 0) {
                    indirectTransfers_.insert(stateKey);
                    break;
                }
                if (state.returnStack.size() >= kMaxReturnStack) {
                    ++stackOverflows_;
                    queueTarget(next, state.returnStack, "rst_stack_limit_return_assumption");
                    break;
                }
                std::vector<std::uint16_t> pushed = state.returnStack;
                pushed.push_back(next);
                queueTarget(static_cast<std::uint16_t>(in.target), pushed, "restart");
                break;
            }

            case disasm::FlowKind::Return:
                if (in.conditional) queueTarget(next, state.returnStack, "return_not_taken");
                if (!state.returnStack.empty()) {
                    std::vector<std::uint16_t> popped = state.returnStack;
                    const std::uint16_t target = popped.back();
                    popped.pop_back();
                    queueTarget(target, popped, "return");
                }
                break;
        }
    }

    if (!recoverStructuredDaughterboardData(error)) return false;
    if (!applyCertifiedBaseEvidence(error)) return false;
    if (!applyU7CertifiedNonCodeDifferential(error)) return false;
    if (!applyResidualClosure(error)) return false;
    rebuildSummary();

    if (!overlapDetails_.empty()) {
        error = "Stateful ownership overlap audit failed: " + overlapDetails_.front();
        return false;
    }
    if (stackOverflows_ != 0) {
        error = "Stateful call-stack analysis hit its safety limit " + std::to_string(stackOverflows_) + " time(s).";
        return false;
    }
    return true;
}

void StatefulAnalyzer::rebuildSummary() {
    summary_ = AnalysisSummary{};
    summary_.canonicalProgramBytes = kCanonicalProgramBytes;
    for (std::size_t i = 0; i < kCanonicalProgramBytes; ++i) {
        if (codeRefs_[i] != 0) {
            ++summary_.codeBytes;
            if (dataRefs_[i] != 0) ++summary_.dualUseBytes;
        } else if (dataRefs_[i] != 0) {
            ++summary_.dataOnlyBytes;
        } else {
            ++summary_.unknownBytes;
        }
    }
    summary_.statefulInstructions = instructions_.size();
    std::set<std::uint16_t> uniqueAddresses;
    for (const auto& kv : instructions_) uniqueAddresses.insert(kv.first.pc);
    summary_.uniqueInstructionAddresses = uniqueAddresses.size();
    summary_.executionStatesVisited = visitedExecStates_.size();
    summary_.cfgEdges = edges_.size();
    summary_.indirectTransfers = indirectTransfers_.size();
    summary_.unmappedEntries = unmappedEntries_.size();
    summary_.overlapConflicts = overlapDetails_.size();
    summary_.stackOverflows = stackOverflows_;
    summary_.roots = roots_.size();
    for (const auto& kv : instructions_) if (kv.second.decoderTransitioned) ++summary_.decoderTransitionInstructions;
    summary_.rst20DispatchTables = dispatchTables_.size();
    for (const auto& kv : dispatchTables_) summary_.rst20DispatchEntries += kv.second.entries.size();
    summary_.rst28InlineSites = rst28InlineSites_.size();
    summary_.rst30InlineSites = rst30InlineSites_.size();
    summary_.inlineFiveByteCallSites = inlineFiveByteCallSites_.size();
    summary_.verifiedPatchRedirectRoots = kPatchRedirects.size();
    summary_.structuredDaughterboardDataBytes = 0;
    for (const bool v : structuredDataMask_) if (v) ++summary_.structuredDaughterboardDataBytes;
    summary_.u7DifferentialEligibleBytes = 0;
    summary_.u7DifferentialTransferredBytes = 0;
    summary_.u7DifferentialRejectedIsolatedBytes = 0;
    summary_.u7DifferentialAcceptedRuns = 0;
    for (const auto& span : u7DifferentialSpans_) {
        const std::size_t length = static_cast<std::size_t>(span.decodedEndExclusive - span.decodedStart);
        summary_.u7DifferentialEligibleBytes += length;
        if (span.accepted) {
            summary_.u7DifferentialTransferredBytes += length;
            ++summary_.u7DifferentialAcceptedRuns;
        } else {
            summary_.u7DifferentialRejectedIsolatedBytes += length;
        }
    }
    summary_.residualClosureCodeBytes = 0;
    summary_.residualClosureDataBytes = 0;
    for (const auto& span : residualClosureSpans_) {
        if (span.code) summary_.residualClosureCodeBytes += span.newlyClassifiedBytes;
        else summary_.residualClosureDataBytes += span.newlyClassifiedBytes;
    }
    summary_.certifiedBaseInstructionStarts = kPacmanCertifiedBoundaries.size();
    summary_.certifiedBaseCodeBytes = 0;
    for (const bool v : certifiedBaseCodeMask_) if (v) ++summary_.certifiedBaseCodeBytes;
    summary_.certifiedBaseDataBytes = 0x4000u - summary_.certifiedBaseCodeBytes;
    summary_.certifiedBaseBoundaryConflicts = certifiedBaseBoundaryConflictDetails_.size();
    summary_.certifiedBaseClassificationConflicts = certifiedBaseClassificationConflictDetails_.size();
    for (std::size_t i = 0; i < kCanonicalProgramBytes; ++i) {
        if (codeRefs_[i] != 0 || dataRefs_[i] != 0) continue;
        if (i < 0x4000u) ++summary_.baseUnknownBytes;
        else if (i < 0x4800u) ++summary_.u5UnknownBytes;
        else if (i < 0x5800u) ++summary_.u6UnknownBytes;
        else ++summary_.u7UnknownBytes;
    }
}

bool StatefulAnalyzer::exportReports(const std::string& outputDir, std::string& error) const {
    if (mkdir(outputDir.c_str(), 0755) != 0) {
        struct stat st{};
        if (stat(outputDir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            error = "Unable to create analysis output directory: " + outputDir;
            return false;
        }
    }

    struct PacmanFamilyTransferRecord {
        const semantic::PacmanSemanticFamily* family=nullptr;
        std::string status;
        std::size_t instructionBytes=0;
        std::size_t patchIntersectBytes=0;
        std::size_t u7Bytes=0;
        std::size_t enabledByteDifferences=0;
    };
    std::vector<PacmanFamilyTransferRecord> pacmanFamilyTransfers;
    std::map<std::uint16_t,std::string> pacmanCandidateLabels;
    {
        std::array<std::uint8_t,0x10000> certifiedLength{};
        std::set<std::uint16_t> certifiedStarts;
        for (const auto& b : kPacmanCertifiedBoundaries) {
            certifiedLength[b.pc]=b.length;
            certifiedStarts.insert(b.pc);
        }
        const auto& families=semantic::PacmanSemanticFamilies::records();
        if (families.size()!=semantic::PacmanSemanticFamilies::CertifiedFamilies) {
            error="semantic transfer analysis Pac-Man semantic-family denominator mismatch.";
            return false;
        }
        std::set<std::uint16_t> familyStarts;
        std::size_t candidate=0,patched=0,different=0;
        for (const auto& family : families) {
            PacmanFamilyTransferRecord tr; tr.family=&family;
            for (const auto pc : family.sourcePCs) {
                if (!certifiedLength[pc]) {
                    error="semantic transfer analysis semantic family references non-certified instruction start $"+hex4(pc)+".";
                    return false;
                }
                if (!familyStarts.insert(pc).second) {
                    error="semantic transfer analysis semantic family source-PC overlap at $"+hex4(pc)+".";
                    return false;
                }
                const auto length=certifiedLength[pc];
                for (std::uint16_t i=0;i<length;++i) {
                    const auto address=static_cast<std::uint16_t>(pc+i);
                    ++tr.instructionBytes;
                    if (address>=0x3000u && address<0x4000u) ++tr.u7Bytes;
                    if (images_.decoderEnabled[address]!=images_.decoderDisabled[address]) ++tr.enabledByteDifferences;
                    for (const auto& patch : mspacman::DaughterboardCodec::patchRegions()) {
                        if (address>=patch.destination && address<static_cast<std::uint16_t>(patch.destination+8u)) {
                            ++tr.patchIntersectBytes; break;
                        }
                    }
                }
            }
            if (tr.patchIntersectBytes) { tr.status="PATCH_OVERLAY_INTERSECTS"; ++patched; }
            else if (tr.enabledByteDifferences) { tr.status="ENABLED_BYTES_DIFFER"; ++different; }
            else { tr.status="BYTE_EQUIVALENT_UNPATCHED"; ++candidate; pacmanCandidateLabels[family.entryPC]=family.familyName; }
            pacmanFamilyTransfers.push_back(std::move(tr));
        }
        if (familyStarts!=certifiedStarts || familyStarts.size()!=semantic::PacmanSemanticFamilies::CertifiedInstructionStarts) {
            error="semantic transfer analysis semantic-family source-PC partition does not exactly equal the certified 5,414 Pac-Man instruction starts.";
            return false;
        }
        if (candidate!=338u || patched!=42u || different!=1u) {
            error="semantic transfer analysis semantic differential count mismatch: candidate="+std::to_string(candidate)+
                " patched="+std::to_string(patched)+" different="+std::to_string(different)+".";
            return false;
        }
    }

    {
        std::ofstream out(outputDir + "/pacman_semantic_family_transfer.csv");
        if (!out) { error="Unable to write pacman_semantic_family_transfer.csv"; return false; }
        out << "family_id,wave,provenance_stable_id,family_name,entry_pc,instruction_starts,instruction_bytes,patch_intersect_bytes,u7_bytes,enabled_byte_differences,transfer_status\n";
        for (const auto& tr : pacmanFamilyTransfers) {
            const auto& f=*tr.family;
            out << csvQuote(f.stableId) << ',' << f.wave << ',' << csvQuote(f.provenanceStableId) << ',' << csvQuote(f.familyName)
                << ",0x" << hex4(f.entryPC) << ',' << f.sourcePCs.size() << ',' << tr.instructionBytes << ','
                << tr.patchIntersectBytes << ',' << tr.u7Bytes << ',' << tr.enabledByteDifferences << ',' << tr.status << "\n";
        }
    }

    {
        std::ofstream out(outputDir + "/SEMANTIC_TRANSFER_STATUS.txt");
        if (!out) { error="Unable to write SEMANTIC_TRANSFER_STATUS.txt"; return false; }
        std::size_t candidate=0,patched=0,different=0;
        for (const auto& tr:pacmanFamilyTransfers) {
            if (tr.status=="BYTE_EQUIVALENT_UNPATCHED") ++candidate;
            else if (tr.status=="PATCH_OVERLAY_INTERSECTS") ++patched;
            else if (tr.status=="ENABLED_BYTES_DIFFER") ++different;
        }
        std::size_t routineSeeds=0,deadRoutineSeeds=0,dataSeeds=0;
        for (const auto& sym:semantic::SemanticCatalog::symbols()) {
            if (sym.kind=="routine") ++routineSeeds;
            else if (sym.kind=="dead_routine") ++deadRoutineSeeds;
            else if (sym.kind=="data") ++dataSeeds;
        }
        out << "MsPacmanRipper semantic transfer analysis semantic differential status\n"
            << "Pac-Man reference families imported/audited: " << pacmanFamilyTransfers.size() << "/381\n"
            << "Pac-Man reference instruction starts partitioned: 5414/5414\n"
            << "Byte-equivalent unpatched transfer candidates: " << candidate << "\n"
            << "Patch-intersecting families requiring Ms.-specific review: " << patched << "\n"
            << "Unpatched families with changed enabled-view bytes: " << different << "\n"
            << "Ms.-specific live routine semantic seeds: " << routineSeeds << "\n"
            << "Audited dead-routine semantic seeds: " << deadRoutineSeeds << "\n"
            << "Structured data semantic seeds: " << dataSeeds << "\n"
            << "Semantic rule: candidate means byte/boundary equivalent and outside every daughterboard patch window; it is not yet a claim of transitive behavioral equivalence.\n"
            << "FULLY DISASSEMBLED STATUS: " << std::fixed << std::setprecision(6) << summary_.classifiedPercent() << "%\n";
    }

    struct PacmanBehaviorAuditRecord {
        const semantic::PacmanSemanticFamily* family=nullptr;
        std::set<std::string> reasons;
        std::set<std::string> dependencies;
        bool behaviorCertified=false;
    };
    std::vector<PacmanBehaviorAuditRecord> pacmanBehaviorAudits;
    std::map<std::string,std::size_t> pacmanFamilyIndexByStableId;
    std::map<std::uint16_t,std::size_t> pacmanFamilyIndexBySourcePC;
    std::map<std::uint16_t,std::string> pacmanBehaviorLabels;
    {
        const auto& families=semantic::PacmanSemanticFamilies::records();
        for(std::size_t i=0;i<families.size();++i){
            pacmanFamilyIndexByStableId[families[i].stableId]=i;
            for(const auto pc:families[i].sourcePCs) pacmanFamilyIndexBySourcePC[pc]=i;
        }
        std::vector<std::uint8_t> base(0x4000);
        for(std::size_t i=0;i<base.size();++i) base[i]=images_.decoderDisabled[i];
        pacmanBehaviorAudits.resize(families.size());
        auto familyForTarget=[&](std::uint16_t target)->const semantic::PacmanSemanticFamily*{
            const auto it=pacmanFamilyIndexBySourcePC.find(target);
            return it==pacmanFamilyIndexBySourcePC.end()?nullptr:&families[it->second];
        };
        auto addDependency=[&](PacmanBehaviorAuditRecord& audit,const semantic::PacmanSemanticFamily& self,std::uint16_t target,const char* prefix){
            if(target>=0x4000u){audit.reasons.insert(std::string(prefix)+"_TARGET_OUTSIDE_BASE_"+hex4(target));return;}
            const auto* dep=familyForTarget(target);
            if(!dep){audit.reasons.insert(std::string(prefix)+"_TARGET_UNOWNED_"+hex4(target));return;}
            if(std::string(dep->stableId)!=self.stableId) audit.dependencies.insert(dep->stableId);
        };
        auto patchByte=[&](std::uint16_t address){
            for(const auto& patch:mspacman::DaughterboardCodec::patchRegions())
                if(address>=patch.destination && address<static_cast<std::uint16_t>(patch.destination+8u)) return true;
            return false;
        };
        auto parseRomLiterals=[&](const std::string& operands,int target,PacmanBehaviorAuditRecord& audit){
            for(std::size_t i=0;i+4<operands.size();++i){
                if(operands[i]!='$') continue;
                unsigned value=0; bool ok=true;
                for(std::size_t j=1;j<=4;++j){
                    const unsigned char ch=static_cast<unsigned char>(operands[i+j]);
                    if(!std::isxdigit(ch)){ok=false;break;}
                    value*=16u;
                    if(ch>='0'&&ch<='9')value+=ch-'0';
                    else if(ch>='A'&&ch<='F')value+=10u+ch-'A';
                    else value+=10u+ch-'a';
                }
                if(ok && value<0x4000u && (target<0 || value!=static_cast<unsigned>(target))) audit.reasons.insert("ROM_LITERAL_DEPENDENCY");
            }
        };
        for(std::size_t fi=0;fi<families.size();++fi){
            auto& audit=pacmanBehaviorAudits[fi]; audit.family=&families[fi];
            const auto& transfer=pacmanFamilyTransfers[fi];
            if(transfer.status!="BYTE_EQUIVALENT_UNPATCHED") audit.reasons.insert(transfer.status);
            for(const auto pc:families[fi].sourcePCs){
                const auto in=disassembler_.decode(base,pc);
                if(in.indirect) audit.reasons.insert("INDIRECT_CONTROL");
                int target=in.target;
                if(in.flow==disasm::FlowKind::Restart && target>=0){
                    const auto rst=static_cast<std::uint16_t>(target);
                    if(rst==0x20u){
                        const PacmanCertifiedRst20Range* range=nullptr;
                        for(const auto& r:kPacmanCertifiedRst20Ranges)if(r.site==pc){range=&r;break;}
                        if(!range) audit.reasons.insert("RST20_RANGE_MISSING");
                        else{
                            bool changed=false;
                            for(std::uint16_t a=range->start;a<range->endExclusive;++a)
                                if(patchByte(a)||images_.decoderEnabled[a]!=images_.decoderDisabled[a]) changed=true;
                            if(changed) audit.reasons.insert("RST20_INLINE_CHANGED_OR_PATCHED");
                            for(std::uint16_t a=range->start;a<range->endExclusive;a=static_cast<std::uint16_t>(a+2u)){
                                const auto t=static_cast<std::uint16_t>(images_.decoderEnabled[a]|(static_cast<std::uint16_t>(images_.decoderEnabled[a+1u])<<8u));
                                addDependency(audit,families[fi],t,"RST20");
                            }
                        }
                    }else if(rst==0x28u) audit.reasons.insert("INLINE_SCHEDULER_RST_28");
                    else if(rst==0x30u) audit.reasons.insert("INLINE_SCHEDULER_RST_30");
                    else addDependency(audit,families[fi],rst,"CONTROL");
                }else if((in.flow==disasm::FlowKind::Call||in.flow==disasm::FlowKind::Jump||in.flow==disasm::FlowKind::RelativeJump) && target>=0){
                    addDependency(audit,families[fi],static_cast<std::uint16_t>(target),"CONTROL");
                }
                parseRomLiterals(in.operands,target,audit);
            }
        }
        std::set<std::string> safe;
        for(const auto& audit:pacmanBehaviorAudits) if(audit.reasons.empty()) safe.insert(audit.family->stableId);
        bool changed=true;
        while(changed){
            changed=false;
            for(auto it=safe.begin();it!=safe.end();){
                const auto idx=pacmanFamilyIndexByStableId.at(*it);
                bool bad=false; for(const auto& dep:pacmanBehaviorAudits[idx].dependencies)if(!safe.count(dep)){bad=true;break;}
                if(bad){it=safe.erase(it);changed=true;}else ++it;
            }
        }
        for(auto& audit:pacmanBehaviorAudits){
            audit.behaviorCertified=safe.count(audit.family->stableId)!=0;
            if(audit.behaviorCertified) pacmanBehaviorLabels[audit.family->entryPC]=audit.family->familyName;
        }
        if(safe.size()!=177u){error="behavioral semantic analysis behavioral inheritance fixed-point mismatch: "+std::to_string(safe.size())+" != 177.";return false;}
    }

    {
        std::ofstream out(outputDir+"/pacman_behavioral_semantic_audit.csv");
        if(!out){error="Unable to write pacman_behavioral_semantic_audit.csv";return false;}
        out<<"family_id,family_name,entry_pc,transfer_status,behavior_status,local_hold_reasons,dependencies\n";
        for(std::size_t i=0;i<pacmanBehaviorAudits.size();++i){
            const auto& a=pacmanBehaviorAudits[i];
            std::ostringstream reasons,deps;
            for(const auto& x:a.reasons){if(reasons.tellp()>0)reasons<<';';reasons<<x;}
            for(const auto& x:a.dependencies){if(deps.tellp()>0)deps<<';';deps<<x;}
            std::string status;
            if(a.behaviorCertified)status="INHERITED_BEHAVIOR_CERTIFIED";
            else if(pacmanFamilyTransfers[i].status=="BYTE_EQUIVALENT_UNPATCHED")status="INHERITED_LOCAL_BYTES_ONLY";
            else if(pacmanFamilyTransfers[i].status=="PATCH_OVERLAY_INTERSECTS")status="PATCHED_REVIEW_REQUIRED";
            else status="REPLACED_CHANGED_BYTES_REVIEW_REQUIRED";
            out<<csvQuote(a.family->stableId)<<','<<csvQuote(a.family->familyName)<<",0x"<<hex4(a.family->entryPC)<<','
               <<pacmanFamilyTransfers[i].status<<','<<status<<','<<csvQuote(reasons.str())<<','<<csvQuote(deps.str())<<"\n";
        }
    }

    {
        std::ofstream out(outputDir+"/BEHAVIORAL_SEMANTIC_STATUS.txt");
        if(!out){error="Unable to write BEHAVIORAL_SEMANTIC_STATUS.txt";return false;}
        std::size_t certified=0,localOnly=0,patched=0,replaced=0;
        for(std::size_t i=0;i<pacmanBehaviorAudits.size();++i){
            if(pacmanBehaviorAudits[i].behaviorCertified)++certified;
            else if(pacmanFamilyTransfers[i].status=="BYTE_EQUIVALENT_UNPATCHED")++localOnly;
            else if(pacmanFamilyTransfers[i].status=="PATCH_OVERLAY_INTERSECTS")++patched;
            else ++replaced;
        }
        out<<"MsPacmanRipper behavioral semantic analysis behavioral semantic inheritance status\n"
           <<"Pac-Man semantic families: 381\n"
           <<"Inherited behavior certified (fail-closed): "<<certified<<"\n"
           <<"Byte-equivalent local semantics only; deeper dependency review held: "<<localOnly<<"\n"
           <<"Patch-intersecting families: "<<patched<<"\n"
           <<"Changed enabled-view family: "<<replaced<<"\n"
           <<"Behavior-certified reference-family percentage: "<<std::fixed<<std::setprecision(6)<<(100.0*certified/381.0)<<"%\n"
           <<"FULLY DISASSEMBLED STATUS: "<<summary_.classifiedPercent()<<"%\n";
    }

    std::size_t currentRoutineDeadRoutineSeedCount = 0;

    {
        // behavioral semantic analysis: exhaustive physical daughterboard CODE segmentation.
        // These segments are storage-semantic families: maximal contiguous canonical CODE runs,
        // split at U6's logical half boundary. They provide a stable, byte-exhaustive denominator
        // without pretending that every contiguous run is already a fully named runtime routine.
        struct CodeSegmentRow {
            CanonicalSource source=CanonicalSource::None;
            std::uint16_t decodedStart=0;
            std::uint16_t decodedEndExclusive=0;
            std::uint16_t logicalStart=0;
            std::uint16_t logicalEndExclusive=0;
            std::vector<std::uint16_t> patchAliases;
            std::vector<std::string> semanticSeeds;
        };
        std::vector<CodeSegmentRow> segments;
        auto logicalForDecoded=[](CanonicalSource source,std::uint16_t off)->std::uint16_t{
            if(source==CanonicalSource::U5) return static_cast<std::uint16_t>(0x8000u+off);
            if(source==CanonicalSource::U6) return off<0x0800u ? static_cast<std::uint16_t>(0x9000u+off)
                                                               : static_cast<std::uint16_t>(0x8800u+off-0x0800u);
            if(source==CanonicalSource::U7) return static_cast<std::uint16_t>(0x3000u+off);
            return 0;
        };
        const std::array<CanonicalSource,3> daughterSources={CanonicalSource::U5,CanonicalSource::U6,CanonicalSource::U7};
        for(const auto source:daughterSources){
            const std::size_t baseIndex=sourceBaseIndex(source);
            const std::size_t size=sourceSize(source);
            std::size_t off=0;
            while(off<size){
                while(off<size && codeRefs_[baseIndex+off]==0) ++off;
                if(off>=size) break;
                const std::size_t start=off;
                const bool u6HighHalf=source==CanonicalSource::U6 && start>=0x0800u;
                while(off<size && codeRefs_[baseIndex+off]!=0){
                    if(source==CanonicalSource::U6 && ((off>=0x0800u)!=u6HighHalf)) break;
                    ++off;
                }
                CodeSegmentRow row;
                row.source=source;
                row.decodedStart=static_cast<std::uint16_t>(start);
                row.decodedEndExclusive=static_cast<std::uint16_t>(off);
                row.logicalStart=logicalForDecoded(source,row.decodedStart);
                row.logicalEndExclusive=static_cast<std::uint16_t>(row.logicalStart+(row.decodedEndExclusive-row.decodedStart));
                if(source==CanonicalSource::U5){
                    for(const auto& patch:mspacman::DaughterboardCodec::patchRegions()){
                        const auto ps=static_cast<std::uint16_t>(patch.source-0x8000u);
                        const auto pe=static_cast<std::uint16_t>(ps+8u);
                        if(row.decodedStart<pe && ps<row.decodedEndExclusive) row.patchAliases.push_back(patch.destination);
                    }
                }
                for(const auto& sym:semantic::SemanticCatalog::symbols()){
                    if(sym.kind!="routine" && sym.kind!="dead_routine") continue;
                    // A live semantic root must inherit the provenance of the byte that
                    // actually executed, not merely the entry decoder state.  This is
                    // observable at the $3FF8 decoder-enable trap: the entry state is
                    // DISABLED, but the opcode fetch enables the daughterboard before
                    // the returned byte is recorded, so the executing RET is physical
                    // U7 $0FF8.  Dead routines have no reached instruction and retain
                    // the direct logical/state provenance fallback.
                    auto p=provenanceFor(sym.address,sym.decoderEnabled);
                    const auto reached=instructions_.find({sym.address,sym.decoderEnabled});
                    if(reached!=instructions_.end() && !reached->second.bytes.empty())
                        p=provenanceFor(reached->second.bytes.front().address,reached->second.bytes.front().decoderEnabled);
                    if(p.valid && p.source==source && p.decodedOffset>=row.decodedStart && p.decodedOffset<row.decodedEndExclusive)
                        row.semanticSeeds.push_back(sym.name);
                }
                segments.push_back(std::move(row));
            }
        }
        std::size_t u5Segments=0,u6Segments=0,u7Segments=0,u5Code=0,u6Code=0,u7Code=0,seeded=0;
        for(const auto& row:segments){
            const auto bytes=static_cast<std::size_t>(row.decodedEndExclusive-row.decodedStart);
            if(row.source==CanonicalSource::U5){++u5Segments;u5Code+=bytes;}
            else if(row.source==CanonicalSource::U6){++u6Segments;u6Code+=bytes;}
            else if(row.source==CanonicalSource::U7){++u7Segments;u7Code+=bytes;}
            seeded+=row.semanticSeeds.size();
        }
        if(segments.size()!=64u || u5Segments!=28u || u6Segments!=14u || u7Segments!=22u ||
           u5Code!=502u || u6Code!=526u || u7Code!=1613u || seeded<37u){
            error="behavioral semantic analysis daughterboard CODE segmentation mismatch."; return false;
        }
        currentRoutineDeadRoutineSeedCount = seeded;
        std::ofstream out(outputDir+"/daughterboard_code_segments.csv");
        if(!out){error="Unable to write daughterboard_code_segments.csv";return false;}
        out<<"segment_id,source,decoded_start,decoded_end_exclusive,code_bytes,primary_logical_start,primary_logical_end_exclusive,patch_alias_destinations,semantic_seed_names\n";
        std::size_t ordinal=0;
        for(const auto& row:segments){
            std::ostringstream aliases,seeds;
            for(const auto a:row.patchAliases){if(aliases.tellp()>0)aliases<<';';aliases<<"0x"<<hex4(a);}
            for(const auto& n:row.semanticSeeds){if(seeds.tellp()>0)seeds<<';';seeds<<n;}
            out<<"MS_CODE_SEG_"<<std::setfill('0')<<std::setw(3)<<ordinal++<<std::setfill(' ')<<','<<sourceName(row.source)
               <<",0x"<<hex4(row.decodedStart)<<",0x"<<hex4(row.decodedEndExclusive)<<','
               <<(row.decodedEndExclusive-row.decodedStart)<<",0x"<<hex4(row.logicalStart)<<",0x"<<hex4(row.logicalEndExclusive)<<','
               <<csvQuote(aliases.str())<<','<<csvQuote(seeds.str())<<"\n";
        }
    }

    {
        // behavioral semantic analysis: map every Pac-Man family whose enabled behavior is modified to
        // the exact daughterboard patch windows and verified redirect targets that alter it.
        std::array<std::uint8_t,0x10000> certifiedLength{};
        for(const auto& b:kPacmanCertifiedBoundaries) certifiedLength[b.pc]=b.length;
        std::array<int,0x4000> familyByByte{}; familyByByte.fill(-1);
        const auto& families=semantic::PacmanSemanticFamilies::records();
        for(std::size_t fi=0;fi<families.size();++fi){
            for(const auto pc:families[fi].sourcePCs){
                const auto len=certifiedLength[pc];
                for(std::uint16_t i=0;i<len;++i){
                    const auto a=static_cast<std::uint16_t>(pc+i);
                    if(a<0x4000u) familyByByte[a]=static_cast<int>(fi);
                }
            }
        }
        struct ImpactRow {std::size_t fi=0;std::set<std::pair<std::uint16_t,std::uint16_t>> patchPairs;std::vector<std::pair<std::uint16_t,std::uint16_t>> redirects;};
        std::vector<ImpactRow> impacts;
        std::size_t withRedirect=0,redirectEdges=0,patchNoRedirect=0,u7Changed=0,semanticRedirectTargets=0;
        std::set<std::uint16_t> uniqueRedirectTargets;
        for(std::size_t fi=0;fi<pacmanFamilyTransfers.size();++fi){
            if(pacmanFamilyTransfers[fi].status=="BYTE_EQUIVALENT_UNPATCHED") continue;
            ImpactRow row; row.fi=fi;
            const auto& f=*pacmanFamilyTransfers[fi].family;
            for(const auto pc:f.sourcePCs){
                const auto len=certifiedLength[pc];
                for(std::uint16_t i=0;i<len;++i){
                    const auto a=static_cast<std::uint16_t>(pc+i);
                    for(const auto& patch:mspacman::DaughterboardCodec::patchRegions()){
                        if(a>=patch.destination && a<static_cast<std::uint16_t>(patch.destination+8u)){
                            row.patchPairs.emplace(patch.destination,patch.source);
                        }
                    }
                }
            }
            for(const auto& evidence:kPatchRedirects){
                if(evidence.sourcePc<0x4000u && familyByByte[evidence.sourcePc]==static_cast<int>(fi))
                    row.redirects.emplace_back(evidence.sourcePc,evidence.target);
            }
            if(!row.redirects.empty()){++withRedirect;redirectEdges+=row.redirects.size();}
            else if(pacmanFamilyTransfers[fi].status=="PATCH_OVERLAY_INTERSECTS") ++patchNoRedirect;
            else ++u7Changed;
            for(const auto& rr:row.redirects){uniqueRedirectTargets.insert(rr.second);if(semantic::SemanticCatalog::find(rr.second,true))++semanticRedirectTargets;}
            impacts.push_back(std::move(row));
        }
        if(impacts.size()!=43u || withRedirect!=25u || redirectEdges!=27u || patchNoRedirect!=17u || u7Changed!=1u ||
           uniqueRedirectTargets.size()!=26u || semanticRedirectTargets!=27u){
            error="behavioral semantic analysis modified-family impact-map denominator mismatch."; return false;
        }
        std::ofstream out(outputDir+"/pacman_modified_family_impact.csv");
        if(!out){error="Unable to write pacman_modified_family_impact.csv";return false;}
        out<<"family_id,family_name,entry_pc,transfer_status,impact_kind,patch_destinations,patch_sources,verified_redirects,semantic_redirect_targets\n";
        for(const auto& row:impacts){
            const auto& f=*pacmanFamilyTransfers[row.fi].family;
            std::ostringstream pd,ps,rr,sn;
            for(const auto& pair:row.patchPairs){
                if(pd.tellp()>0){pd<<';';ps<<';';}
                pd<<"0x"<<hex4(pair.first); ps<<"0x"<<hex4(pair.second);
            }
            for(const auto& x:row.redirects){if(rr.tellp()>0)rr<<';';rr<<"0x"<<hex4(x.first)<<"->0x"<<hex4(x.second);const auto* sym=semantic::SemanticCatalog::find(x.second,true);if(sn.tellp()>0)sn<<';';sn<<(sym?sym->name:("UNNAMED_"+hex4(x.second)));}
            const std::string kind=pacmanFamilyTransfers[row.fi].status=="ENABLED_BYTES_DIFFER" ? "U7_CHANGED_BYTES" :
                (!row.redirects.empty()?"PATCHED_WITH_VERIFIED_REDIRECT":"PATCHED_INLINE_OR_DATA_ONLY");
            out<<csvQuote(f.stableId)<<','<<csvQuote(f.familyName)<<",0x"<<hex4(f.entryPC)<<','<<pacmanFamilyTransfers[row.fi].status<<','<<kind<<','
               <<csvQuote(pd.str())<<','<<csvQuote(ps.str())<<','<<csvQuote(rr.str())<<','<<csvQuote(sn.str())<<"\n";
        }
        std::ofstream statusOut(outputDir+"/MODIFIED_FAMILY_STATUS.txt");
        if(!statusOut){error="Unable to write MODIFIED_FAMILY_STATUS.txt";return false;}
        statusOut<<"MsPacmanRipper behavioral semantic analysis modified-family / daughterboard segmentation status\n"
                 <<"Modified Pac-Man families mapped: 43/43\n"
                 <<"  patch-intersecting with verified redirect(s): 25\n"
                 <<"  patch-intersecting inline/data-only: 17\n"
                 <<"  changed enabled-view U7 family: 1\n"
                 <<"Verified redirect edges: 27\n"
                 <<"Unique verified redirect targets with semantic names: 26/26\n"
                 <<"Physical daughterboard CODE segments: 64 (U5 28 / U6 14 / U7 22)\n"
                 <<"Physical daughterboard CODE bytes: 2641 (U5 502 / U6 526 / U7 1613)\n"
                 <<"Routine/dead-routine semantic seeds represented in CODE segments: 37/37\n"
                 <<"Current routine/dead-routine semantic seeds represented in CODE segments: "<<currentRoutineDeadRoutineSeedCount<<"/"<<currentRoutineDeadRoutineSeedCount<<"\n"
                 <<"FULLY DISASSEMBLED STATUS: "<<std::fixed<<std::setprecision(6)<<summary_.classifiedPercent()<<"%\n";
    }

    {
        std::ofstream out(outputDir + "/OWNERSHIP_CLOSURE_SUMMARY.txt");
        if (!out) { error = "Unable to write OWNERSHIP_CLOSURE_SUMMARY.txt"; return false; }
        out << "MsPacmanRipper residual ownership closure — Complete Program-Storage Ownership Closure\n"
            << "Created by Jacob Hodgkins\n\n"
            << "Canonical program storage bytes: " << summary_.canonicalProgramBytes << "\n"
            << "CODE bytes: " << summary_.codeBytes << "\n"
            << "DATA-only bytes (audited code/data ownership): " << summary_.dataOnlyBytes << "\n"
            << "CODE+DATA dual-use bytes: " << summary_.dualUseBytes << "\n"
            << "UNKNOWN bytes: " << summary_.unknownBytes << "\n"
            << "Classified bytes: " << summary_.classifiedBytes() << "/" << summary_.canonicalProgramBytes << "\n"
            << std::fixed << std::setprecision(6)
            << "FULLY DISASSEMBLED STATUS: " << summary_.classifiedPercent() << "%\n"
            << "Status definition: strict unique canonical program-byte CODE/DATA ownership; UNKNOWN is never credited.\n"
            << "Human-semantic completion is a later metric and is not implied by this percentage.\n\n"
            << "Stateful instructions: " << summary_.statefulInstructions << "\n"
            << "Unique instruction addresses: " << summary_.uniqueInstructionAddresses << "\n"
            << "Execution states visited (includes call-stack context): " << summary_.executionStatesVisited << "\n"
            << "CFG edges: " << summary_.cfgEdges << "\n"
            << "Roots: " << summary_.roots << "\n"
            << "Instructions with decoder transitions: " << summary_.decoderTransitionInstructions << "\n"
            << "Unresolved indirect transfers: " << summary_.indirectTransfers << "\n"
            << "Unmapped entries encountered: " << summary_.unmappedEntries << "\n"
            << "Instruction overlap conflicts: " << summary_.overlapConflicts << "\n"
            << "Call-stack safety overflows: " << summary_.stackOverflows << "\n"
            << "RST $20 dispatch tables: " << summary_.rst20DispatchTables << "\n"
            << "RST $20 dispatch entries: " << summary_.rst20DispatchEntries << "\n"
            << "RST $28 inline-data sites: " << summary_.rst28InlineSites << "\n"
            << "RST $30 inline-data sites: " << summary_.rst30InlineSites << "\n"
            << "Verified five-byte inline CALL sites: " << summary_.inlineFiveByteCallSites << "\n"
            << "Verified patch redirect roots: " << summary_.verifiedPatchRedirectRoots << "\n"
            << "Structured daughterboard DATA bytes: " << summary_.structuredDaughterboardDataBytes << "\n"
            << "U7 differential eligible bytes: " << summary_.u7DifferentialEligibleBytes << "\n"
            << "U7 differential transferred bytes: " << summary_.u7DifferentialTransferredBytes << "\n"
            << "U7 differential rejected isolated bytes: " << summary_.u7DifferentialRejectedIsolatedBytes << "\n"
            << "U7 differential accepted runs: " << summary_.u7DifferentialAcceptedRuns << "\n"
            << "residual ownership closure residual CODE bytes: " << summary_.residualClosureCodeBytes << "\n"
            << "residual ownership closure residual DATA bytes: " << summary_.residualClosureDataBytes << "\n"
            << "Certified Pac-Man instruction starts: " << summary_.certifiedBaseInstructionStarts << "\n"
            << "Certified base CODE bytes: " << summary_.certifiedBaseCodeBytes << "\n"
            << "Certified base non-code bytes: " << summary_.certifiedBaseDataBytes << "\n"
            << "Certified base boundary conflicts: " << summary_.certifiedBaseBoundaryConflicts << "\n"
            << "Certified base classification conflicts: " << summary_.certifiedBaseClassificationConflicts << "\n"
            << "Base-board UNKNOWN bytes: " << summary_.baseUnknownBytes << "\n"
            << "U5 UNKNOWN bytes: " << summary_.u5UnknownBytes << "\n"
            << "U6 UNKNOWN bytes: " << summary_.u6UnknownBytes << "\n"
            << "U7 UNKNOWN bytes: " << summary_.u7UnknownBytes << "\n";
    }

    {
        std::ofstream out(outputDir + "/patch_redirect_audit.csv");
        if (!out) { error = "Unable to write patch_redirect_audit.csv"; return false; }
        out << "source_pc,target,flow,source_u5_overlay,verified\n";
        for (const auto& evidence : kPatchRedirects) {
            DecodedStateInstruction source;
            const bool decoded = decodeAt(evidence.sourcePc, true, source);
            bool insidePatch = false;
            for (const auto& patch : mspacman::DaughterboardCodec::patchRegions())
                if (evidence.sourcePc >= patch.destination && evidence.sourcePc < static_cast<std::uint16_t>(patch.destination + 8u)) insidePatch = true;
            bool u5Alias = false;
            if (decoded && !source.bytes.empty()) {
                const Provenance p = provenanceFor(source.bytes.front().address, source.bytes.front().decoderEnabled);
                u5Alias = p.valid && p.source == CanonicalSource::U5 && p.alias;
            }
            const bool targetMatch = decoded && source.instruction.target >= 0 &&
                static_cast<std::uint16_t>(source.instruction.target) == evidence.target;
            const bool flowOk = decoded && (source.instruction.flow == disasm::FlowKind::Jump || source.instruction.flow == disasm::FlowKind::Call);
            out << "0x" << hex4(evidence.sourcePc) << ",0x" << hex4(evidence.target) << ","
                << (decoded ? source.instruction.mnemonic : "UNMAPPED") << "," << (insidePatch && u5Alias ? "VERIFIED" : "FAIL")
                << "," << (insidePatch && u5Alias && targetMatch && flowOk ? "VERIFIED" : "FAIL") << "\n";
        }
    }

    {
        std::ofstream out(outputDir + "/u7_differential_evidence.csv");
        if (!out) { error = "Unable to write u7_differential_evidence.csv"; return false; }
        out << "decoded_start,decoded_end_exclusive,length,logical_start,accepted\n";
        for (const auto& span : u7DifferentialSpans_) {
            const std::size_t length = static_cast<std::size_t>(span.decodedEndExclusive - span.decodedStart);
            out << "0x" << hex4(span.decodedStart) << ",0x" << hex4(span.decodedEndExclusive) << ',' << length
                << ",0x" << hex4(static_cast<std::uint16_t>(0x3000u + span.decodedStart)) << ','
                << (span.accepted ? "VERIFIED" : "REJECT_ISOLATED") << "\n";
        }
    }

    {
        std::ofstream out(outputDir + "/ownership_pre_u7_differential.csv");
        if (!out) { error = "Unable to write ownership_pre_u7_differential.csv"; return false; }
        out << "source,decoded_offset,canonical_index,classification,code_observations,data_observations\n";
        const std::array<CanonicalSource, 7> sources = {CanonicalSource::Pacman6E, CanonicalSource::Pacman6F,
            CanonicalSource::Pacman6H, CanonicalSource::Pacman6J, CanonicalSource::U5, CanonicalSource::U6, CanonicalSource::U7};
        for (const auto source : sources) {
            const std::size_t base = sourceBaseIndex(source);
            for (std::size_t offset = 0; offset < sourceSize(source); ++offset) {
                const std::size_t index = base + offset;
                std::string classification = "UNKNOWN";
                if (preU7DiffCodeRefs_[index] != 0) classification = preU7DiffDataRefs_[index] != 0 ? "CODE_DATA" : "CODE";
                else if (preU7DiffDataRefs_[index] != 0) classification = "DATA";
                out << sourceName(source) << ",0x" << hex4(static_cast<std::uint16_t>(offset)) << ',' << index << ','
                    << classification << ',' << preU7DiffCodeRefs_[index] << ',' << preU7DiffDataRefs_[index] << "\n";
            }
        }
    }

    {
        std::ofstream out(outputDir + "/ownership_pre_residual_closure.csv");
        if (!out) { error = "Unable to write ownership_pre_residual_closure.csv"; return false; }
        out << "source,decoded_offset,canonical_index,classification,code_observations,data_observations\n";
        const std::array<CanonicalSource, 7> sources = {CanonicalSource::Pacman6E, CanonicalSource::Pacman6F,
            CanonicalSource::Pacman6H, CanonicalSource::Pacman6J, CanonicalSource::U5, CanonicalSource::U6, CanonicalSource::U7};
        for (const auto source : sources) {
            const std::size_t base = sourceBaseIndex(source);
            for (std::size_t offset = 0; offset < sourceSize(source); ++offset) {
                const std::size_t index = base + offset;
                std::string classification = "UNKNOWN";
                if (preResidualCodeRefs_[index] != 0) classification = preResidualDataRefs_[index] != 0 ? "CODE_DATA" : "CODE";
                else if (preResidualDataRefs_[index] != 0) classification = "DATA";
                out << sourceName(source) << ",0x" << hex4(static_cast<std::uint16_t>(offset)) << ',' << index << ','
                    << classification << ',' << preResidualCodeRefs_[index] << ',' << preResidualDataRefs_[index] << "\n";
            }
        }
    }

    {
        std::ofstream out(outputDir + "/residual_closure_evidence.csv");
        if (!out) { error = "Unable to write residual_closure_evidence.csv"; return false; }
        out << "kind,logical_start,logical_end_exclusive,newly_classified_bytes,classification,proof\n";
        for (const auto& span : residualClosureSpans_) {
            out << csvQuote(span.kind) << ",0x" << hex4(span.logicalStart) << ",0x" << hex4(span.logicalEndExclusive)
                << ',' << span.newlyClassifiedBytes << ',' << (span.code ? "CODE" : "DATA") << ',' << csvQuote(span.proof) << "\n";
        }
    }

    {
        std::ofstream out(outputDir + "/structured_data_evidence.csv");
        if (!out) { error = "Unable to write structured_data_evidence.csv"; return false; }
        out << "kind,logical_start,logical_end_exclusive,canonical_bytes,proof\n";
        for (const auto& span : structuredDataSpans_) {
            out << csvQuote(span.kind) << ",0x" << hex4(span.logicalStart) << ",0x" << hex4(span.logicalEndExclusive)
                << "," << span.canonicalBytes << "," << csvQuote(span.proof) << "\n";
        }
    }

    {
        std::ofstream out(outputDir + "/base_differential_evidence.csv");
        if (!out) { error = "Unable to write base_differential_evidence.csv"; return false; }
        out << "pc,certified_length,decoded_length,verified\n";
        std::vector<std::uint8_t> base(0x4000);
        for (std::size_t i = 0; i < base.size(); ++i) base[i] = images_.decoderDisabled[i];
        for (const auto& boundary : kPacmanCertifiedBoundaries) {
            const auto decoded = disassembler_.decode(base, boundary.pc);
            out << "0x" << hex4(boundary.pc) << ',' << static_cast<unsigned>(boundary.length) << ','
                << decoded.length() << ',' << (decoded.length() == boundary.length ? "VERIFIED" : "FAIL") << "\n";
        }
    }

    {
        std::ofstream out(outputDir + "/overlay_exception_audit.csv");
        if (!out) { error = "Unable to write overlay_exception_audit.csv"; return false; }
        out << "destination,source,base_storage_class,enabled_storage_source,enabled_storage_offset,audit\n";
        for (const auto& patch : mspacman::DaughterboardCodec::patchRegions()) {
            for (std::uint16_t n = 0; n < 8; ++n) {
                const std::uint16_t dst = static_cast<std::uint16_t>(patch.destination + n);
                const std::uint16_t src = static_cast<std::uint16_t>(patch.source + n);
                const Provenance p = provenanceFor(dst, true);
                const bool ok = p.valid && p.source == CanonicalSource::U5;
                out << "0x" << hex4(dst) << ",0x" << hex4(src) << ','
                    << (certifiedBaseCodeMask_[dst] ? "CODE" : "NONCODE") << ','
                    << sourceName(p.source) << ",0x" << hex4(p.decodedOffset) << ',' << (ok ? "VERIFIED" : "FAIL") << "\n";
            }
        }
    }

    {
        std::ofstream out(outputDir + "/instructions.csv");
        if (!out) { error = "Unable to write instructions.csv"; return false; }
        out << "address,entry_decoder,exit_decoder,bytes,mnemonic,operands,flow,conditional,indirect,target,root\n";
        for (const auto& kv : instructions_) {
            const auto& d = kv.second;
            std::ostringstream bytes;
            for (std::size_t i = 0; i < d.instruction.bytes.size(); ++i) {
                if (i) bytes << ' ';
                bytes << hex2(d.instruction.bytes[i]);
            }
            std::string flow;
            switch (d.instruction.flow) {
                case disasm::FlowKind::Normal: flow = "normal"; break;
                case disasm::FlowKind::Call: flow = "call"; break;
                case disasm::FlowKind::Jump: flow = "jump"; break;
                case disasm::FlowKind::RelativeJump: flow = "relative_jump"; break;
                case disasm::FlowKind::Return: flow = "return"; break;
                case disasm::FlowKind::Restart: flow = "restart"; break;
                case disasm::FlowKind::Halt: flow = "halt"; break;
            }
            const auto rootIt = roots_.find(kv.first);
            out << "0x" << hex4(kv.first.pc) << ',' << stateName(kv.first.decoderEnabled) << ',' << stateName(d.exitDecoderEnabled)
                << ',' << csvQuote(bytes.str()) << ',' << csvQuote(d.instruction.mnemonic) << ',' << csvQuote(d.instruction.operands)
                << ',' << flow << ',' << (d.instruction.conditional ? 1 : 0) << ',' << (d.instruction.indirect ? 1 : 0) << ',';
            if (d.instruction.target >= 0) out << "0x" << hex4(static_cast<std::uint16_t>(d.instruction.target));
            out << ',' << csvQuote(rootIt == roots_.end() ? std::string() : rootIt->second) << "\n";
        }
    }

    {
        std::ofstream out(outputDir + "/state_cfg.csv");
        if (!out) { error = "Unable to write state_cfg.csv"; return false; }
        out << "from_address,from_decoder,to_address,to_decoder,edge_kind\n";
        for (const auto& e : edges_)
            out << "0x" << hex4(e.fromPc) << ',' << stateName(e.fromState) << ",0x" << hex4(e.toPc) << ','
                << stateName(e.toState) << ',' << e.kind << "\n";
    }

    {
        std::ofstream out(outputDir + "/ownership.csv");
        if (!out) { error = "Unable to write ownership.csv"; return false; }
        out << "source,decoded_offset,canonical_index,classification,code_observations,data_observations\n";
        const std::array<CanonicalSource, 7> sources = {CanonicalSource::Pacman6E, CanonicalSource::Pacman6F,
            CanonicalSource::Pacman6H, CanonicalSource::Pacman6J, CanonicalSource::U5, CanonicalSource::U6, CanonicalSource::U7};
        for (const auto source : sources) {
            const std::size_t base = sourceBaseIndex(source);
            for (std::size_t offset = 0; offset < sourceSize(source); ++offset) {
                const std::size_t index = base + offset;
                std::string classification = "UNKNOWN";
                if (codeRefs_[index] != 0) classification = dataRefs_[index] != 0 ? "CODE_DATA" : "CODE";
                else if (dataRefs_[index] != 0) classification = "DATA";
                out << sourceName(source) << ",0x" << hex4(static_cast<std::uint16_t>(offset)) << ',' << index << ','
                    << classification << ',' << codeRefs_[index] << ',' << dataRefs_[index] << "\n";
            }
        }
    }

    {
        std::ofstream out(outputDir + "/unknown_regions.csv");
        if (!out) { error = "Unable to write unknown_regions.csv"; return false; }
        out << "source,start_decoded_offset,end_decoded_offset_exclusive,length\n";
        const std::array<CanonicalSource, 7> sources = {CanonicalSource::Pacman6E, CanonicalSource::Pacman6F,
            CanonicalSource::Pacman6H, CanonicalSource::Pacman6J, CanonicalSource::U5, CanonicalSource::U6, CanonicalSource::U7};
        for (const auto source : sources) {
            const std::size_t base = sourceBaseIndex(source);
            std::size_t offset = 0;
            while (offset < sourceSize(source)) {
                const auto isUnknown = [&](std::size_t o) { return codeRefs_[base + o] == 0 && dataRefs_[base + o] == 0; };
                if (!isUnknown(offset)) { ++offset; continue; }
                const std::size_t start = offset;
                while (offset < sourceSize(source) && isUnknown(offset)) ++offset;
                out << sourceName(source) << ",0x" << hex4(static_cast<std::uint16_t>(start)) << ",0x"
                    << hex4(static_cast<std::uint16_t>(offset)) << ',' << (offset - start) << "\n";
            }
        }
    }

    {
        std::ofstream out(outputDir + "/rst20_dispatch_tables.csv");
        if (!out) { error = "Unable to write rst20_dispatch_tables.csv"; return false; }
        out << "site,site_decoder,table_start,table_end_exclusive,entry_index,entry_address,target,target_decoder,boundary_reason\n";
        for (const auto& kv : dispatchTables_) {
            const auto& table = kv.second;
            for (std::size_t i = 0; i < table.entries.size(); ++i) {
                const auto& entry = table.entries[i];
                out << "0x" << hex4(table.site.pc) << ',' << stateName(table.site.decoderEnabled) << ",0x" << hex4(table.start)
                    << ",0x" << hex4(table.end) << ',' << i << ",0x" << hex4(entry.entryAddress) << ",0x" << hex4(entry.target)
                    << ',' << stateName(entry.targetDecoderEnabled) << ',' << csvQuote(table.boundaryReason) << "\n";
            }
        }
    }

    {
        std::ofstream out(outputDir + "/patch_coverage.csv");
        if (!out) { error = "Unable to write patch_coverage.csv"; return false; }
        out << "patch_index,destination_start,destination_end,source_start,source_end,code_bytes,inline_data_bytes,unknown_bytes,instruction_starts\n";
        std::size_t patchIndex = 0;
        for (const auto& patch : mspacman::DaughterboardCodec::patchRegions()) {
            std::size_t code = 0, data = 0, unknown = 0, starts = 0;
            for (std::uint16_t i = 0; i < 8; ++i) {
                const std::uint16_t address = static_cast<std::uint16_t>(patch.destination + i);
                if (logicalOwners_[1][address] >= 0) ++code;
                else if (hardData_[1][address]) ++data;
                else ++unknown;
                if (instructions_.find({address, true}) != instructions_.end()) ++starts;
            }
            out << patchIndex++ << ",0x" << hex4(patch.destination) << ",0x" << hex4(static_cast<std::uint16_t>(patch.destination + 7u))
                << ",0x" << hex4(patch.source) << ",0x" << hex4(static_cast<std::uint16_t>(patch.source + 7u)) << ','
                << code << ',' << data << ',' << unknown << ',' << starts << "\n";
        }
    }

    {
        std::ofstream out(outputDir + "/stateful_disassembly.asm");
        if (!out) { error = "Unable to write stateful_disassembly.asm"; return false; }
        out << "; MsPacmanRipper complete-code analysis stateful disassembly\n"
            << "; Created by Jacob Hodgkins\n"
            << "; Semantic labels are evidence-backed seeds; mechanical state/address labels remain as stable aliases.\n\n";
        for (const auto& kv : instructions_) {
            const auto& d = kv.second;
            if (const auto* sym = semantic::SemanticCatalog::find(kv.first.pc, kv.first.decoderEnabled))
                out << sym->name << ": ; " << sym->evidence << "\n";
            out << (kv.first.decoderEnabled ? "EN_" : "DIS_") << hex4(kv.first.pc) << ":\n    "
                << std::left << std::setw(8) << d.instruction.mnemonic << d.instruction.operands;
            out << " ; ";
            for (std::size_t i = 0; i < d.instruction.bytes.size(); ++i) {
                if (i) out << ' ';
                out << hex2(d.instruction.bytes[i]);
            }
            if (d.entryDecoderEnabled != d.exitDecoderEnabled) out << " | decoder -> " << stateName(d.exitDecoderEnabled);
            out << "\n\n";
        }
    }


    {
        std::ofstream out(outputDir + "/semantic_symbols.csv");
        if (!out) { error = "Unable to write semantic_symbols.csv"; return false; }
        out << "address,decoder_state,name,kind,evidence\n";
        for (const auto& sym : semantic::SemanticCatalog::symbols())
            out << "0x" << hex4(sym.address) << ',' << stateName(sym.decoderEnabled) << ','
                << csvQuote(sym.name) << ',' << csvQuote(sym.kind) << ',' << csvQuote(sym.evidence) << "\n";
    }

    {
        // Complete CODE disassembly coverage: the frozen Pac-Man boundary manifest covers
        // all physical base CODE. Stateful decoded instructions cover all reached
        // daughterboard/overlay CODE, and the three residual ownership closure dead islands are decoded here.
        std::ofstream out(outputDir + "/complete_code_disassembly.asm");
        if (!out) { error = "Unable to write complete_code_disassembly.asm"; return false; }
        out << "; MsPacmanRipper complete-code analysis complete CODE disassembly\n"
            << "; Created by Jacob Hodgkins\n"
            << "; Every canonical CODE byte must be represented by at least one instruction below.\n\n";
        std::array<bool,kCanonicalProgramBytes> covered{};
        std::vector<std::uint8_t> base(0x4000);
        for (std::size_t i=0;i<base.size();++i) base[i]=images_.decoderDisabled[i];
        out << "; ---- Physical Pac-Man base program (certified identical storage) ----\n\n";
        for (const auto& b : kPacmanCertifiedBoundaries) {
            const auto in=disassembler_.decode(base,b.pc);
            if (in.length()!=b.length) { error="complete-code analysis base disassembly length regression at $"+hex4(b.pc)+"."; return false; }
            const auto behaviorLabel=pacmanBehaviorLabels.find(b.pc);
            if(behaviorLabel!=pacmanBehaviorLabels.end())
                out << "PAC_INHERITED_" << behaviorLabel->second << ": ; behavioral semantic analysis fail-closed behavioral inheritance certified\n";
            const auto pacLabel=pacmanCandidateLabels.find(b.pc);
            if (pacLabel!=pacmanCandidateLabels.end() && behaviorLabel==pacmanBehaviorLabels.end())
                out << "PAC_REF_CANDIDATE_" << pacLabel->second << ": ; byte-equivalent, unpatched Pac-Man semantic-family entry; deeper behavioral/data review pending\n";
            out << "BASE_" << hex4(b.pc) << ":\n    " << std::left << std::setw(8) << in.mnemonic << in.operands << " ; ";
            for (std::size_t i=0;i<in.bytes.size();++i){ if(i)out<<' '; out<<hex2(in.bytes[i]); covered[b.pc+i]=true; }
            out << "\n\n";
        }

        out << "; ---- Ms. Pac-Man daughterboard / patch-visible instructions ----\n\n";
        std::set<std::pair<std::uint16_t,bool>> emitted;
        for (const auto& kv : instructions_) {
            const auto& d=kv.second;
            bool daughterboard=false;
            for (const auto& obs:d.bytes) {
                const auto p=provenanceFor(obs.address,obs.decoderEnabled);
                if (p.valid && p.canonicalIndex>=0x4000u) daughterboard=true;
            }
            if (!daughterboard || !emitted.insert({kv.first.pc,kv.first.decoderEnabled}).second) continue;
            if (const auto* sym=semantic::SemanticCatalog::find(kv.first.pc,kv.first.decoderEnabled))
                out << sym->name << ": ; " << sym->evidence << "\n";
            out << (kv.first.decoderEnabled?"MS_EN_":"MS_DIS_") << hex4(kv.first.pc) << ":\n    "
                << std::left << std::setw(8) << d.instruction.mnemonic << d.instruction.operands << " ; ";
            for (std::size_t i=0;i<d.instruction.bytes.size();++i){ if(i)out<<' '; out<<hex2(d.instruction.bytes[i]); }
            out << "\n\n";
            for (const auto& obs:d.bytes) {
                const auto p=provenanceFor(obs.address,obs.decoderEnabled);
                if (p.valid && p.canonicalIndex<kCanonicalProgramBytes && codeRefs_[p.canonicalIndex]!=0) covered[p.canonicalIndex]=true;
            }
        }

        std::vector<std::uint8_t> logical(images_.decoderEnabled.begin(),images_.decoderEnabled.end());
        const std::array<std::pair<std::uint16_t,std::uint16_t>,3> dead={{{0x367F,0x3696},{0x39E0,0x39F2},{0x3A00,0x3A06}}};
        out << "; ---- Audited dead CODE islands ----\n\n";
        for(const auto& r:dead){
            std::uint16_t pc=r.first;
            while(pc<r.second){
                const auto in=disassembler_.decode(logical,pc);
                if(in.length()==0 || static_cast<std::uint32_t>(pc)+in.length()>r.second){ error="complete-code analysis dead-code decode escaped bounded island at $"+hex4(pc)+".";return false; }
                if(const auto* sym=semantic::SemanticCatalog::find(pc,true)) out<<sym->name<<": ; "<<sym->evidence<<"\n";
                out<<"DEAD_"<<hex4(pc)<<":\n    "<<std::left<<std::setw(8)<<in.mnemonic<<in.operands<<" ; ";
                for(std::size_t i=0;i<in.bytes.size();++i){if(i)out<<' ';out<<hex2(in.bytes[i]);const auto p=provenanceFor(static_cast<std::uint16_t>(pc+i),true);if(p.valid&&p.canonicalIndex<kCanonicalProgramBytes&&codeRefs_[p.canonicalIndex]!=0)covered[p.canonicalIndex]=true;}
                out<<"\n\n"; pc=static_cast<std::uint16_t>(pc+in.length());
            }
        }
        std::size_t code=0,coveredCode=0,extra=0;
        for(std::size_t i=0;i<kCanonicalProgramBytes;++i){
            if(codeRefs_[i]!=0){++code;if(covered[i])++coveredCode;}
            else if(covered[i])++extra;
        }
        if(code!=14242u || coveredCode!=code || extra!=0u){
            error="complete-code analysis complete CODE coverage mismatch: code="+std::to_string(code)+" covered="+std::to_string(coveredCode)+" extra="+std::to_string(extra)+".";
            return false;
        }
        std::ofstream cov(outputDir+"/COMPLETE_CODE_COVERAGE.txt");
        if(!cov){error="Unable to write COMPLETE_CODE_COVERAGE.txt";return false;}
        cov<<"MsPacmanRipper complete-code analysis complete CODE coverage: VERIFIED\n"
           <<"Canonical CODE bytes: "<<code<<"\nCovered CODE bytes: "<<coveredCode<<"\nExtra non-CODE bytes represented as instructions: "<<extra<<"\n"
           <<"FULLY DISASSEMBLED STATUS: "<<std::fixed<<std::setprecision(6)<<summary_.classifiedPercent()<<"%\n"
           <<"Semantic seed symbols: "<<semantic::SemanticCatalog::symbols().size()<<"\n";
    }

    if (!overlapDetails_.empty()) {
        std::ofstream out(outputDir + "/overlap_conflicts.txt");
        if (!out) { error = "Unable to write overlap_conflicts.txt"; return false; }
        for (const auto& line : overlapDetails_) out << line << '\n';
    }
    return true;
}

} // namespace msrip::analysis
