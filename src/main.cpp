// Created by Jacob Hodgkins
#include "analysis/StatefulAnalyzer.h"
#include "mspacman/Daughterboard.h"
#include "rom/RomSet.h"
#include "Version.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>

namespace {

void printUsage(const char* argv0) {
    std::cout
        << "MsPacmanRipper " << msrip::kVersion << " — complete Ms. Pac-Man arcade ROM disassembler\n"
        << "Created by Jacob Hodgkins\n\n"
        << "Usage:\n"
        << "  " << argv0 << " <mspacman.zip|ROM-directory> <output-folder>\n\n"
        << "The input must be the canonical 13-file Ms. Pac-Man set.\n"
        << "The output contains the full program disassembly plus structured graphics,\n"
        << "color, audio, manifests, and exact complete-board reconstruction data.\n";
}

bool loadCanonical(const std::string& path, msrip::RomSet& roms) {
    std::string error;
    if (!roms.load(path, error)) {
        std::cerr << "ERROR: " << error << "\n";
        return false;
    }
    if (!roms.validateCanonical(error)) {
        std::cerr << "ERROR: canonical Ms. Pac-Man validation failed:\n" << error;
        return false;
    }
    return true;
}

std::filesystem::path findProjectRoot(const char* argv0) {
    namespace fs = std::filesystem;
    std::error_code ec;

    const fs::path cwd = fs::current_path(ec);
    if (!ec && fs::is_regular_file(cwd / "scripts" / "export_full_disassembly.py", ec))
        return cwd;

    ec.clear();
    const fs::path exe = fs::absolute(fs::path(argv0), ec);
    if (!ec) {
        const fs::path parent = exe.parent_path();
        if (fs::is_regular_file(parent / "scripts" / "export_full_disassembly.py", ec))
            return parent;
        ec.clear();
        if (fs::is_regular_file(parent.parent_path() / "scripts" / "export_full_disassembly.py", ec))
            return parent.parent_path();
    }
    return {};
}

std::string shellQuote(const std::string& s) {
#ifdef _WIN32
    std::string out = "\"";
    for (char c : s) out += (c == '\"') ? "\\\"" : std::string(1, c);
    return out + "\"";
#else
    std::string out = "'";
    for (char c : s) out += (c == '\'') ? "'\\''" : std::string(1, c);
    return out + "'";
#endif
}

bool commandAvailable(const char* name) {
#ifdef _WIN32
    const std::string cmd = std::string(name) + " --version >NUL 2>&1";
#else
    const std::string cmd = std::string(name) + " --version >/dev/null 2>&1";
#endif
    return std::system(cmd.c_str()) == 0;
}

bool writeBinary(const std::filesystem::path& path, const std::uint8_t* data, std::size_t size) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(out);
}

int internalAnalyze(const std::string& romPath, const std::string& outputDir) {
    msrip::RomSet roms;
    if (!loadCanonical(romPath, roms)) return 1;

    msrip::mspacman::DaughterboardImages images;
    std::string error;
    if (!msrip::mspacman::DaughterboardCodec::buildImages(roms, images, error)) {
        std::cerr << "ERROR: " << error << "\n";
        return 1;
    }

    msrip::analysis::StatefulAnalyzer analyzer(images);
    if (!analyzer.run(error)) {
        std::cerr << "ERROR: stateful analysis failed: " << error << "\n";
        return 1;
    }
    if (!analyzer.exportReports(outputDir, error)) {
        std::cerr << "ERROR: unable to export internal analysis: " << error << "\n";
        return 1;
    }
    return 0;
}

int internalExportLogical(const std::string& romPath, const std::string& outputDir) {
    namespace fs = std::filesystem;
    msrip::RomSet roms;
    if (!loadCanonical(romPath, roms)) return 1;

    msrip::mspacman::DaughterboardImages images;
    std::string error;
    if (!msrip::mspacman::DaughterboardCodec::buildImages(roms, images, error)) {
        std::cerr << "ERROR: " << error << "\n";
        return 1;
    }

    std::error_code ec;
    fs::create_directories(outputDir, ec);
    if (ec) {
        std::cerr << "ERROR: unable to create internal logical-output directory: " << ec.message() << "\n";
        return 1;
    }

    const fs::path out(outputDir);
    if (!writeBinary(out / "decoder_enabled_64k.bin", images.decoderEnabled.data(), images.decoderEnabled.size()) ||
        !writeBinary(out / "decoder_disabled_64k.bin", images.decoderDisabled.data(), images.decoderDisabled.size())) {
        std::cerr << "ERROR: unable to write internal logical images.\n";
        return 1;
    }

    std::ofstream csv(out / "patch_map.csv");
    if (!csv) {
        std::cerr << "ERROR: unable to write internal patch map.\n";
        return 1;
    }
    csv << "destination_start,destination_end,source_start,source_end\n";
    auto hex4 = [](std::uint16_t v) {
        static constexpr char kHex[] = "0123456789ABCDEF";
        std::string s(4, '0');
        for (int i = 3; i >= 0; --i) { s[static_cast<std::size_t>(i)] = kHex[v & 0xF]; v >>= 4; }
        return s;
    };
    for (const auto& p : msrip::mspacman::DaughterboardCodec::patchRegions()) {
        csv << "0x" << hex4(p.destination) << ",0x" << hex4(static_cast<std::uint16_t>(p.destination + 7))
            << ",0x" << hex4(p.source) << ",0x" << hex4(static_cast<std::uint16_t>(p.source + 7)) << "\n";
    }
    return 0;
}

int runFullDisassemblyExporter(const char* argv0, const std::string& romPath, const std::string& outDir) {
    const auto root = findProjectRoot(argv0);
    if (root.empty()) {
        std::cerr << "ERROR: unable to locate the bundled MsPacmanRipper export pipeline.\n";
        return 1;
    }

    const char* envPython = std::getenv("MSPACMANRIPPER_PYTHON");
    std::string python;
    if (envPython && *envPython) python = envPython;
#ifdef _WIN32
    else if (commandAvailable("python")) python = "python";
    else if (commandAvailable("python3")) python = "python3";
#else
    else if (commandAvailable("python3")) python = "python3";
    else if (commandAvailable("python")) python = "python";
#endif
    else {
        std::cerr << "ERROR: Python 3 is required by the bundled structured-source exporter.\n";
        return 1;
    }

    std::error_code ec;
    const auto exe = std::filesystem::absolute(std::filesystem::path(argv0), ec);
    if (ec) {
        std::cerr << "ERROR: unable to resolve MsPacmanRipper executable path.\n";
        return 1;
    }

    const auto script = root / "scripts" / "export_full_disassembly.py";
    const std::string cmd = shellQuote(python) + " " + shellQuote(script.string()) + " " +
                            shellQuote(root.string()) + " " + shellQuote(exe.string()) + " " +
                            shellQuote(romPath) + " " + shellQuote(outDir);
    const int rc = std::system(cmd.c_str());
    return rc == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    // Private subprocess entry points used only by the bundled full-output exporter.
    if (argc == 4 && std::string(argv[1]) == "--internal-analyze")
        return internalAnalyze(argv[2], argv[3]);
    if (argc == 4 && std::string(argv[1]) == "--internal-export-logical")
        return internalExportLogical(argv[2], argv[3]);

    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        printUsage(argv[0]);
        return 0;
    }
    if (argc == 2 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-V")) {
        std::cout << "MsPacmanRipper " << msrip::kVersion << "\n"
                  << "Created by Jacob Hodgkins\n";
        return 0;
    }
    if (argc != 3) {
        printUsage(argv[0]);
        return argc == 1 ? 0 : 2;
    }

    const std::string romPath = argv[1];
    const std::string outputDir = argv[2];

    msrip::RomSet check;
    if (!loadCanonical(romPath, check)) return 1;

    std::cout << "MsPacmanRipper: canonical 13-file Ms. Pac-Man set validated.\n"
              << "Disassembling complete board ROM/PROM set...\n";
    const int rc = runFullDisassemblyExporter(argv[0], romPath, outputDir);
    if (rc == 0) {
        std::cout << "MsPacmanRipper: COMPLETE -> " << outputDir << "\n";
    }
    return rc;
}
