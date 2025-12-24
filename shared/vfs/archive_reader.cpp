#include "archive_reader.hpp"

#include <algorithm>
#include <cstring>

namespace shared::vfs {

ArchiveReader::ArchiveReader() = default;

ArchiveReader::~ArchiveReader() {
    close();
}

ArchiveReader::ArchiveReader(ArchiveReader&& other) noexcept
    : archivePath_(std::move(other.archivePath_))
    , entries_(std::move(other.entries_))
    , pathIndex_(std::move(other.pathIndex_))
    , file_(other.file_) {
    other.file_ = nullptr;
}

ArchiveReader& ArchiveReader::operator=(ArchiveReader&& other) noexcept {
    if (this != &other) {
        close();
        archivePath_ = std::move(other.archivePath_);
        entries_ = std::move(other.entries_);
        pathIndex_ = std::move(other.pathIndex_);
        file_ = other.file_;
        other.file_ = nullptr;
    }
    return *this;
}

bool ArchiveReader::open(const std::filesystem::path& archivePath) {
    close();

    const std::string pathStr = archivePath.string();
    file_ = std::fopen(pathStr.c_str(), "rb");
    if (!file_) {
        return false;
    }

    archivePath_ = archivePath;

    if (!read_header() || !read_toc()) {
        close();
        return false;
    }

    return true;
}

void ArchiveReader::close() {
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
    entries_.clear();
    pathIndex_.clear();
    archivePath_.clear();
}

bool ArchiveReader::is_open() const {
    return file_ != nullptr;
}

bool ArchiveReader::read_header() {
    PakHeader header;
    if (std::fread(&header, sizeof(header), 1, file_) != 1) {
        return false;
    }

    if (header.magic != PAK_MAGIC) {
        return false;
    }

    if (header.version != PAK_VERSION) {
        return false;
    }

    if (std::fseek(file_, static_cast<long>(header.tocOffset), SEEK_SET) != 0) {
        return false;
    }

    entries_.reserve(header.entryCount);
    pathIndex_.reserve(header.entryCount);

    return true;
}

bool ArchiveReader::read_toc() {
    // File position should already be at TOC from read_header.
    // Re-read header to get entry count.
    std::fseek(file_, 0, SEEK_SET);

    PakHeader header;
    if (std::fread(&header, sizeof(header), 1, file_) != 1) {
        return false;
    }

    std::fseek(file_, static_cast<long>(header.tocOffset), SEEK_SET);

    for (std::uint32_t i = 0; i < header.entryCount; ++i) {
        PakTocEntry tocEntry;
        if (std::fread(&tocEntry, sizeof(tocEntry), 1, file_) != 1) {
            return false;
        }

        if (tocEntry.pathLength > PAK_MAX_PATH_LENGTH) {
            return false;
        }

        std::string path(tocEntry.pathLength, '\0');
        if (tocEntry.pathLength > 0) {
            if (std::fread(path.data(), 1, tocEntry.pathLength, file_) != tocEntry.pathLength) {
                return false;
            }
        }

        FileEntry entry;
        entry.name = std::move(path);
        entry.offset = tocEntry.offset;
        entry.size = tocEntry.size;

        pathIndex_[entry.name] = entries_.size();
        entries_.push_back(std::move(entry));
    }

    return true;
}

bool ArchiveReader::has_file(const std::string& path) const {
    return pathIndex_.find(path) != pathIndex_.end();
}

const ArchiveReader::FileEntry* ArchiveReader::get_entry(const std::string& path) const {
    auto it = pathIndex_.find(path);
    if (it == pathIndex_.end()) {
        return nullptr;
    }
    return &entries_[it->second];
}

std::optional<std::vector<std::uint8_t>> ArchiveReader::extract(const std::string& path) {
    const FileEntry* entry = get_entry(path);
    if (!entry) {
        return std::nullopt;
    }

    if (!is_open()) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> data(entry->size);
    if (entry->size == 0) {
        return data;  // Empty file.
    }

    if (std::fseek(file_, static_cast<long>(entry->offset), SEEK_SET) != 0) {
        return std::nullopt;
    }

    if (std::fread(data.data(), 1, entry->size, file_) != entry->size) {
        return std::nullopt;
    }

    return data;
}

std::vector<std::string> ArchiveReader::list_directory(const std::string& dirPath) const {
    std::vector<std::string> result;

    // Normalize directory path.
    std::string prefix = dirPath;
    if (!prefix.empty() && prefix.back() != '/') {
        prefix += '/';
    }
    // Root directory: empty prefix.
    if (prefix == "/") {
        prefix.clear();
    }

    const std::size_t prefixLen = prefix.length();

    for (const auto& entry : entries_) {
        // Check if entry is under this directory.
        if (entry.name.length() <= prefixLen) {
            continue;
        }
        if (prefixLen > 0 && entry.name.compare(0, prefixLen, prefix) != 0) {
            continue;
        }

        // Get the part after the prefix.
        std::string_view remainder(entry.name.data() + prefixLen, entry.name.length() - prefixLen);

        // Find first slash to determine if it's a direct child.
        auto slashPos = remainder.find('/');

        std::string childName;
        if (slashPos == std::string_view::npos) {
            // Direct child file.
            childName = std::string(remainder);
        } else {
            // Nested item; extract parent dir name with trailing slash.
            childName = std::string(remainder.substr(0, slashPos + 1));
        }

        // Avoid duplicates.
        if (std::find(result.begin(), result.end(), childName) == result.end()) {
            result.push_back(std::move(childName));
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

} // namespace shared::vfs
