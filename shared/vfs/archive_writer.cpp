#include "archive_writer.hpp"

#include <cstring>
#include <fstream>

namespace shared::vfs {

ArchiveWriter::ArchiveWriter() = default;

ArchiveWriter::~ArchiveWriter() {
    if (file_ && !finalized_) {
        cancel();
    }
}

bool ArchiveWriter::begin(const std::filesystem::path& archivePath) {
    if (file_) {
        return false;
    }

    outputPath_ = archivePath;
    const std::string pathStr = archivePath.string();
    file_ = std::fopen(pathStr.c_str(), "wb");
    if (!file_) {
        return false;
    }

    PakHeader header;
    if (std::fwrite(&header, sizeof(header), 1, file_) != 1) {
        std::fclose(file_);
        file_ = nullptr;
        return false;
    }

    entries_.clear();
    currentOffset_ = PAK_HEADER_SIZE;
    finalized_ = false;
    return true;
}

bool ArchiveWriter::add_file(const std::string& archivePath, const std::vector<std::uint8_t>& data) {
    if (!file_ || finalized_) {
        return false;
    }

    if (!data.empty()) {
        if (std::fwrite(data.data(), 1, data.size(), file_) != data.size()) {
            return false;
        }
    }

    PendingEntry entry;
    entry.path = archivePath;
    entry.offset = currentOffset_;
    entry.size = data.size();
    entries_.push_back(std::move(entry));

    currentOffset_ += data.size();
    return true;
}

bool ArchiveWriter::add_file_from_disk(const std::string& archivePath,
                                        const std::filesystem::path& sourcePath) {
    if (!file_ || finalized_) {
        return false;
    }

    std::ifstream src(sourcePath, std::ios::binary | std::ios::ate);
    if (!src) {
        return false;
    }

    const auto size = static_cast<std::size_t>(src.tellg());
    src.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> data(size);
    if (size > 0) {
        if (!src.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size))) {
            return false;
        }
    }

    return add_file(archivePath, data);
}

bool ArchiveWriter::finalize() {
    if (!file_ || finalized_) {
        return false;
    }

    const std::uint64_t tocOffset = currentOffset_;

    for (const auto& entry : entries_) {
        PakTocEntry tocEntry;
        tocEntry.offset = entry.offset;
        tocEntry.size = entry.size;
        tocEntry.pathLength = static_cast<std::uint32_t>(entry.path.size());

        if (std::fwrite(&tocEntry, sizeof(tocEntry), 1, file_) != 1) {
            return false;
        }

        if (!entry.path.empty()) {
            if (std::fwrite(entry.path.data(), 1, entry.path.size(), file_) != entry.path.size()) {
                return false;
            }
        }
    }

    std::fseek(file_, 0, SEEK_SET);

    PakHeader header;
    header.magic = PAK_MAGIC;
    header.version = PAK_VERSION;
    header.entryCount = static_cast<std::uint32_t>(entries_.size());
    header.tocOffset = tocOffset;

    if (std::fwrite(&header, sizeof(header), 1, file_) != 1) {
        return false;
    }

    std::fclose(file_);
    file_ = nullptr;
    finalized_ = true;
    return true;
}

void ArchiveWriter::cancel() {
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }

    if (!outputPath_.empty()) {
        std::error_code ec;
        std::filesystem::remove(outputPath_, ec);
        outputPath_.clear();
    }

    entries_.clear();
    finalized_ = false;
}

} // namespace shared::vfs
