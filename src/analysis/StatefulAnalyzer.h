#pragma once
// Created by Jacob Hodgkins

#include "disasm/Z80Disassembler.h"
#include "mspacman/Daughterboard.h"

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace msrip::analysis {

struct AnalysisSummary {
    std::size_t canonicalProgramBytes = 0;
    std::size_t codeBytes = 0;
    std::size_t dataOnlyBytes = 0;
    std::size_t dualUseBytes = 0;
    std::size_t unknownBytes = 0;
    std::size_t statefulInstructions = 0;
    std::size_t uniqueInstructionAddresses = 0;
    std::size_t executionStatesVisited = 0;
    std::size_t cfgEdges = 0;
    std::size_t indirectTransfers = 0;
    std::size_t unmappedEntries = 0;
    std::size_t overlapConflicts = 0;
    std::size_t stackOverflows = 0;
    std::size_t roots = 0;
    std::size_t decoderTransitionInstructions = 0;
    std::size_t rst20DispatchTables = 0;
    std::size_t rst20DispatchEntries = 0;
    std::size_t rst28InlineSites = 0;
    std::size_t rst30InlineSites = 0;
    std::size_t inlineFiveByteCallSites = 0;
    std::size_t verifiedPatchRedirectRoots = 0;
    std::size_t structuredDaughterboardDataBytes = 0;
    std::size_t u7DifferentialEligibleBytes = 0;
    std::size_t u7DifferentialTransferredBytes = 0;
    std::size_t u7DifferentialRejectedIsolatedBytes = 0;
    std::size_t u7DifferentialAcceptedRuns = 0;
    std::size_t residualClosureCodeBytes = 0;
    std::size_t residualClosureDataBytes = 0;
    std::size_t certifiedBaseInstructionStarts = 0;
    std::size_t certifiedBaseCodeBytes = 0;
    std::size_t certifiedBaseDataBytes = 0;
    std::size_t certifiedBaseBoundaryConflicts = 0;
    std::size_t certifiedBaseClassificationConflicts = 0;
    std::size_t baseUnknownBytes = 0;
    std::size_t u5UnknownBytes = 0;
    std::size_t u6UnknownBytes = 0;
    std::size_t u7UnknownBytes = 0;

    std::size_t classifiedBytes() const { return codeBytes + dataOnlyBytes; }
    double classifiedPercent() const;
};

class StatefulAnalyzer {
public:
    explicit StatefulAnalyzer(const mspacman::DaughterboardImages& images);

    bool run(std::string& error);
    bool exportReports(const std::string& outputDir, std::string& error) const;
    const AnalysisSummary& summary() const { return summary_; }

private:
    struct StateKey {
        std::uint16_t pc = 0;
        bool decoderEnabled = true;
        bool operator<(const StateKey& other) const {
            return std::tie(pc, decoderEnabled) < std::tie(other.pc, other.decoderEnabled);
        }
    };

    struct ExecState {
        std::uint16_t pc = 0;
        bool decoderEnabled = true;
        std::vector<std::uint16_t> returnStack;
        bool operator<(const ExecState& other) const {
            return std::tie(pc, decoderEnabled, returnStack) < std::tie(other.pc, other.decoderEnabled, other.returnStack);
        }
    };

    struct ByteObservation {
        std::uint16_t address = 0;
        std::uint8_t value = 0;
        bool decoderEnabled = true; // effective state after the address trap is applied
        bool mapped = false;
    };

    struct DataObservation {
        std::uint16_t address = 0;
        bool decoderEnabled = true;
        bool mapped = false;
    };

    struct DecodedStateInstruction {
        disasm::Instruction instruction;
        bool entryDecoderEnabled = true;
        bool exitDecoderEnabled = true;
        std::vector<ByteObservation> bytes;
        std::vector<DataObservation> romDataReads;
        bool decoderTransitioned = false;
    };

    enum class CanonicalSource { Pacman6E, Pacman6F, Pacman6H, Pacman6J, U5, U6, U7, None };

    struct Provenance {
        CanonicalSource source = CanonicalSource::None;
        std::uint16_t decodedOffset = 0;
        std::size_t canonicalIndex = 0;
        bool valid = false;
        bool alias = false;
    };

    struct DispatchEntry {
        std::uint16_t entryAddress = 0;
        std::uint16_t target = 0;
        bool targetDecoderEnabled = true;
    };

    struct DispatchTable {
        StateKey site;
        std::uint16_t start = 0;
        std::uint16_t end = 0;
        std::string boundaryReason;
        std::vector<DispatchEntry> entries;
    };

    struct U7DifferentialSpan {
        std::uint16_t decodedStart = 0;
        std::uint16_t decodedEndExclusive = 0;
        bool accepted = false;
    };

    struct ResidualClosureSpan {
        std::string kind;
        std::uint16_t logicalStart = 0;
        std::uint16_t logicalEndExclusive = 0;
        std::size_t newlyClassifiedBytes = 0;
        bool code = false;
        std::string proof;
    };

    struct StructuredDataSpan {
        std::string kind;
        std::uint16_t logicalStart = 0;
        std::uint16_t logicalEndExclusive = 0;
        std::size_t canonicalBytes = 0;
        std::string proof;
    };

    struct Edge {
        std::uint16_t fromPc = 0;
        bool fromState = true;
        std::uint16_t toPc = 0;
        bool toState = true;
        std::string kind;
        bool operator<(const Edge& other) const {
            return std::tie(fromPc, fromState, toPc, toState, kind) <
                   std::tie(other.fromPc, other.fromState, other.toPc, other.toState, other.kind);
        }
    };

    static constexpr std::size_t kCanonicalProgramBytes = 0x6800;
    static constexpr std::size_t kMaxReturnStack = 32;
    static constexpr std::size_t kMaxExecStates = 500000;

    bool decodeAt(std::uint16_t pc, bool decoderEnabled, DecodedStateInstruction& out) const;
    bool canFetch(std::uint16_t pc, bool decoderEnabled) const;
    bool readWordForData(std::uint16_t address, bool decoderEnabled, std::uint16_t& value, bool& exitDecoderEnabled) const;
    bool markInlineData(std::uint16_t start, std::size_t length, bool decoderEnabled, bool& exitDecoderEnabled, std::string& error);
    bool resolveRst20(const ExecState& state, const DecodedStateInstruction& decoded, std::string& error);
    bool prepareCertifiedBaseEvidence(std::string& error);
    bool applyCertifiedBaseEvidence(std::string& error);
    bool targetIsCompatibleWithCertifiedBase(std::uint16_t address, bool decoderEnabled) const;
    bool baseInlineRangeIsCertifiedNonCode(std::uint16_t start, std::size_t length, bool decoderEnabled) const;
    bool recoverStructuredDaughterboardData(std::string& error);
    bool applyU7CertifiedNonCodeDifferential(std::string& error);
    bool applyResidualClosure(std::string& error);
    bool markResidualLogicalRange(std::uint16_t start, std::size_t length, bool code, const std::string& kind,
                               const std::string& proof, std::string& error);
    bool markResidualCanonicalRange(CanonicalSource source, std::uint16_t decodedStart, std::size_t length, bool code,
                                 const std::string& kind, const std::string& proof, std::string& error);
    bool markStructuredDataRange(std::uint16_t start, std::size_t length, const std::string& kind,
                                 const std::string& proof, std::string& error);
    bool requireReachedInstruction(std::uint16_t pc, const std::string& mnemonic,
                                   const std::string& operands, std::string& error) const;
    Provenance provenanceFor(std::uint16_t address, bool effectiveDecoderEnabled) const;
    static const char* sourceName(CanonicalSource source);
    static std::size_t sourceSize(CanonicalSource source);
    static std::size_t sourceBaseIndex(CanonicalSource source);
    static std::string hex4(std::uint16_t v);
    static std::string hex2(std::uint8_t v);
    static std::string csvQuote(const std::string& value);

    void seed(std::uint16_t pc, bool decoderEnabled, const std::string& name);
    void enqueue(const ExecState& state);
    void addEdge(std::uint16_t fromPc, bool fromState, std::uint16_t toPc, bool toState, const std::string& kind);
    void observeInstruction(const DecodedStateInstruction& decoded, std::string& error);
    void rebuildSummary();

    const mspacman::DaughterboardImages& images_;
    disasm::Z80Disassembler disassembler_;
    mutable std::vector<std::uint8_t> scratch_;
    std::vector<ExecState> worklist_;
    std::size_t workIndex_ = 0;
    std::set<ExecState> visitedExecStates_;
    std::map<StateKey, DecodedStateInstruction> instructions_;
    std::set<Edge> edges_;
    std::map<StateKey, std::string> roots_;
    std::array<std::array<int, 0x10000>, 2> logicalOwners_{};
    std::array<std::array<bool, 0x10000>, 2> hardData_{};
    std::array<std::size_t, kCanonicalProgramBytes> codeRefs_{};
    std::array<std::size_t, kCanonicalProgramBytes> dataRefs_{};
    std::array<std::size_t, kCanonicalProgramBytes> preU7DiffCodeRefs_{};
    std::array<std::size_t, kCanonicalProgramBytes> preU7DiffDataRefs_{};
    std::array<std::size_t, kCanonicalProgramBytes> preResidualCodeRefs_{};
    std::array<std::size_t, kCanonicalProgramBytes> preResidualDataRefs_{};
    std::array<bool, kCanonicalProgramBytes> structuredDataMask_{};
    std::vector<StructuredDataSpan> structuredDataSpans_;
    std::vector<U7DifferentialSpan> u7DifferentialSpans_;
    std::vector<ResidualClosureSpan> residualClosureSpans_;
    std::set<StateKey> indirectTransfers_;
    std::map<StateKey, DispatchTable> dispatchTables_;
    std::set<StateKey> rst28InlineSites_;
    std::set<StateKey> rst30InlineSites_;
    std::set<StateKey> inlineFiveByteCallSites_;
    std::set<StateKey> unmappedEntries_;
    std::vector<std::string> overlapDetails_;
    std::array<bool, 0x4000> certifiedBaseCodeMask_{};
    std::array<bool, 0x4000> certifiedBaseStartMask_{};
    std::vector<std::string> certifiedBaseBoundaryConflictDetails_;
    std::vector<std::string> certifiedBaseClassificationConflictDetails_;
    std::size_t stackOverflows_ = 0;
    AnalysisSummary summary_;
};

} // namespace msrip::analysis
