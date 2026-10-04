// Created by Jacob Hodgkins
#include "rom/RomSet.h"
#include "crypto/Hash.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace msrip {
namespace {

std::string lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::string baseName(const std::string& path) {
    const std::size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? path : path.substr(p + 1);
}

bool endsWithInsensitive(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return lower(s.substr(s.size() - suffix.size())) == lower(suffix);
}

std::string shellQuote(const std::string& s) {
#ifdef _WIN32
    std::string out = "\"";
    for (char c : s) out += (c == '\"') ? "\\\"" : std::string(1, c);
    return out + "\"";
#else
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
#endif
}

FILE* openPipe(const std::string& command, bool binary) {
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
    if (pipe && binary) _setmode(_fileno(pipe), _O_BINARY);
    return pipe;
#else
    (void)binary;
    return popen(command.c_str(), "r");
#endif
}

int closePipe(FILE* pipe) {
#ifdef _WIN32
    return _pclose(pipe);
#else
    return pclose(pipe);
#endif
}

bool readFile(const std::filesystem::path& path, std::vector<std::uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

void appendU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>((value >> (i * 8u)) & 0xffu));
}

} // namespace

const std::vector<CanonicalRomDescriptor>& RomSet::canonicalManifest() {
    static const std::vector<CanonicalRomDescriptor> manifest = {
        {"pacman.6e",0x1000,0xC1E6AB10u,"fe1c3234df345855d30728637f361f79472cabfe2a892a7567c63eaf31a4217b","main_board_program"},
        {"pacman.6f",0x1000,0x1A6FB2D4u,"09a723c9f84790e9019633e37761cfa4e9d7ab6db14f6fdb12738f51fec11065","main_board_program"},
        {"pacman.6h",0x1000,0xBCDD1BEBu,"69347409739b64ed9d9b19713de0bc66627bd137687de649796b9d2ef88ed8e6","main_board_program"},
        {"pacman.6j",0x1000,0x817D94E3u,"03ee523c210e87fb8dd1d925b092ad269fdd753b5b7a20b3757b0ceee5f18679","main_board_program"},
        {"u5",0x0800,0xF45FBBCDu,"9418acd93bf2f859bdd78a342fbbed04ec628b84e6e6c22c3258903d534d6cde","daughterboard_program"},
        {"u6",0x1000,0xA90E7000u,"90e12618b54e22fa3c42c4b5e0c0b3279facd80ceeb1d8d4648b8f42321fb8fd","daughterboard_program"},
        {"u7",0x1000,0xC82CD714u,"1866f07add83e63156a23284ed1e39dfa88de70e7944dfbd9db655d2b8de3e22","daughterboard_program"},
        {"5e",0x1000,0x5C281D01u,"effa5d95c0c3b3d04d41dbeb315370b9666a97036dd0f1a81037ba06b1fec9d9","character_graphics"},
        {"5f",0x1000,0x615AF909u,"6bd821cce05d7d6cb40b2dc5967abb41e8b2dadeae27eac4b235b3f55ee759dd","sprite_graphics"},
        {"82s123.7f",0x0020,0x2FC650BDu,"48fe0b01d68e3d702019ca715f7266c8e3261c769509b281720f53ca0a1cc8fb","palette_prom"},
        {"82s126.4a",0x0100,0x3EB3A8E4u,"ef8f7a3b0c10f787d9cc1cbc5cc266fcc1afadb24c3b4d610fe252b9c3df1d76","color_lookup_prom"},
        {"82s126.1m",0x0100,0xA9CC86BFu,"8e723ad91e46ef1a186b2ed3c99a8bf1c571786bc7ceae2b367cbfc80857a394","waveform_prom"},
        {"82s126.3m",0x0100,0x77245B66u,"8c34002652e587aa19a77bff9040d870af18b4b2fe5c5f0ed962899386e0e751","sound_timing_control_prom"}
    };
    return manifest;
}

void RomSet::clear() {
    loaded_ = false;
    sourcePath_.clear();
    files_.clear();
}

void RomSet::addFile(const std::string& displayName, const std::string& sourcePath, std::vector<std::uint8_t>&& data) {
    RomFile rf;
    rf.name = baseName(displayName);
    rf.sourcePath = sourcePath;
    rf.crc32 = crypto::crc32(data);
    rf.sha256 = crypto::sha256Hex(data);
    rf.data = std::move(data);
    const std::string key = lower(rf.name);
    if (files_.find(key) == files_.end()) files_[key] = std::move(rf);
}

bool RomSet::load(const std::string& path, std::string& error) {
    clear();
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path input(path);

    if (!fs::exists(input, ec) || ec) {
        error = "Path does not exist: " + path;
        return false;
    }

    bool ok = false;
    if (fs::is_directory(input, ec) && !ec) {
        ok = loadDirectory(path, error);
    } else if (fs::is_regular_file(input, ec) && !ec && endsWithInsensitive(path, ".zip")) {
        ok = loadZip(path, error);
    } else {
        error = "MsPacmanRipper accepts a ROM directory or .zip archive.";
    }

    if (ok) { loaded_ = true; sourcePath_ = path; }
    return ok;
}

bool RomSet::loadDirectory(const std::string& path, std::string& error) {
    namespace fs = std::filesystem;
    std::error_code ec;

    for (fs::directory_iterator it(fs::path(path), ec), end; !ec && it != end; it.increment(ec)) {
        const fs::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }

        std::vector<std::uint8_t> data;
        if (readFile(entry.path(), data)) {
            addFile(entry.path().filename().string(), entry.path().string(), std::move(data));
        }
    }

    if (ec) {
        error = "Unable to read ROM directory: " + ec.message();
        return false;
    }
    if (files_.empty()) { error = "ROM directory contained no readable files."; return false; }
    return true;
}

bool RomSet::loadZip(const std::string& path, std::string& error) {
#ifdef _WIN32
    // Modern supported Windows versions ship bsdtar as tar.exe. Use it directly so
    // the Windows port needs no third-party ZIP library or Unix compatibility layer.
    const std::string listCmd = "tar -tf " + shellQuote(path) + " 2>NUL";
#else
    const std::string listCmd = "unzip -Z1 " + shellQuote(path) + " 2>/dev/null";
#endif

    FILE* pipe = openPipe(listCmd, false);
    if (!pipe) {
#ifdef _WIN32
        error = "Unable to launch Windows tar.exe to read the ZIP archive.";
#else
        error = "Unable to launch unzip. Install Ubuntu package 'unzip'.";
#endif
        return false;
    }

    std::vector<std::string> names;
    char line[4096];
    while (std::fgets(line, sizeof(line), pipe)) {
        std::string n(line);
        while (!n.empty() && (n.back() == '\n' || n.back() == '\r')) n.pop_back();
        if (!n.empty() && n.back() != '/') names.push_back(n);
    }
    const int listRc = closePipe(pipe);
    if (listRc != 0 || names.empty()) { error = "Could not read ZIP archive."; return false; }

    for (const std::string& name : names) {
#ifdef _WIN32
        const std::string cmd = "tar -xOf " + shellQuote(path) + " -- " + shellQuote(name) + " 2>NUL";
#else
        const std::string cmd = "unzip -p " + shellQuote(path) + " " + shellQuote(name) + " 2>/dev/null";
#endif
        FILE* fp = openPipe(cmd, true);
        if (!fp) continue;

        std::vector<std::uint8_t> data;
        std::uint8_t buf[8192];
        for (;;) {
            const std::size_t got = std::fread(buf, 1, sizeof(buf), fp);
            if (got) data.insert(data.end(), buf, buf + got);
            if (got < sizeof(buf)) break;
        }

        const int rc = closePipe(fp);
        if (rc == 0) addFile(name, path + ":" + name, std::move(data));
    }

    if (files_.empty()) { error = "ZIP was readable but no files could be extracted."; return false; }
    return true;
}

const RomFile* RomSet::find(const std::string& name) const {
    const auto it = files_.find(lower(name));
    return it == files_.end() ? nullptr : &it->second;
}

std::vector<ValidationItem> RomSet::validationItems() const {
    std::vector<ValidationItem> out;
    for (const auto& expected : canonicalManifest()) {
        ValidationItem item;
        item.expected = expected;
        if (const RomFile* f = find(expected.name)) {
            item.present = true;
            item.actualSize = f->data.size();
            item.actualCrc32 = f->crc32;
            item.actualSha256 = f->sha256;
            item.valid = item.actualSize == expected.size && item.actualCrc32 == expected.crc32 && item.actualSha256 == expected.sha256;
        }
        out.push_back(std::move(item));
    }
    return out;
}

bool RomSet::validateCanonical(std::string& error) const {
    if (!loaded_) { error = "No ROM set loaded."; return false; }
    std::ostringstream problems;
    bool ok = true;
    for (const ValidationItem& item : validationItems()) {
        if (!item.valid) {
            ok = false;
            problems << item.expected.name << ": ";
            if (!item.present) problems << "missing";
            else problems << "identity mismatch";
            problems << "\n";
        }
    }
    if (files_.size() != canonicalManifest().size()) {
        ok = false;
        problems << "archive member count: expected " << canonicalManifest().size() << ", found " << files_.size() << "\n";
    }
    if (!ok) error = problems.str();
    return ok;
}

std::vector<std::uint8_t> RomSet::basePacmanProgram() const {
    std::vector<std::uint8_t> out;
    for (const char* n : {"pacman.6e","pacman.6f","pacman.6h","pacman.6j"}) {
        const RomFile* f = find(n);
        if (!f) return {};
        out.insert(out.end(), f->data.begin(), f->data.end());
    }
    return out;
}

std::vector<std::uint8_t> RomSet::graphics() const {
    std::vector<std::uint8_t> out;
    for (const char* n : {"5e","5f"}) {
        const RomFile* f = find(n);
        if (!f) return {};
        out.insert(out.end(), f->data.begin(), f->data.end());
    }
    return out;
}

std::vector<std::uint8_t> RomSet::normalizedSetBytes() const {
    std::vector<std::string> names;
    names.reserve(canonicalManifest().size());
    for (const auto& d : canonicalManifest()) names.push_back(lower(d.name));
    std::sort(names.begin(), names.end());

    std::vector<std::uint8_t> out;
    for (const std::string& n : names) {
        const RomFile* f = find(n);
        if (!f) return {};
        out.insert(out.end(), n.begin(), n.end());
        out.push_back(0);
        appendU64(out, static_cast<std::uint64_t>(f->data.size()));
        out.insert(out.end(), f->data.begin(), f->data.end());
    }
    return out;
}

} // namespace msrip
