#pragma once
// Created by Jacob Hodgkins

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace msrip {

struct RomFile {
    std::string name;
    std::string sourcePath;
    std::vector<std::uint8_t> data;
    std::uint32_t crc32 = 0;
    std::string sha256;
};

struct CanonicalRomDescriptor {
    std::string name;
    std::size_t size = 0;
    std::uint32_t crc32 = 0;
    std::string sha256;
    std::string resourceClass;
};

struct ValidationItem {
    CanonicalRomDescriptor expected;
    std::size_t actualSize = 0;
    std::uint32_t actualCrc32 = 0;
    std::string actualSha256;
    bool present = false;
    bool valid = false;
};

class RomSet {
public:
    bool load(const std::string& path, std::string& error);
    void clear();

    const RomFile* find(const std::string& name) const;
    const std::map<std::string, RomFile>& files() const { return files_; }
    const std::string& sourcePath() const { return sourcePath_; }
    bool loaded() const { return loaded_; }

    std::vector<ValidationItem> validationItems() const;
    bool validateCanonical(std::string& error) const;
    std::vector<std::uint8_t> basePacmanProgram() const;
    std::vector<std::uint8_t> graphics() const;
    std::vector<std::uint8_t> normalizedSetBytes() const;

    static const std::vector<CanonicalRomDescriptor>& canonicalManifest();

private:
    bool loadDirectory(const std::string& path, std::string& error);
    bool loadZip(const std::string& path, std::string& error);
    void addFile(const std::string& displayName, const std::string& sourcePath, std::vector<std::uint8_t>&& data);

    bool loaded_ = false;
    std::string sourcePath_;
    std::map<std::string, RomFile> files_;
};

} // namespace msrip
