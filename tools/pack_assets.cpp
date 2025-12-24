// pack_assets - CLI tool for packing game assets into .pak archives.
//
// Usage:
//   pack_assets --input <dir> --output <file.pak> [options]
//
// Options:
//   --input, -i <dir>     Source directory containing assets.
//   --output, -o <file>   Output .pak file path.
//   --exclude <pattern>   Pattern for files to exclude (can be repeated).
//   --verbose, -v         Print files being added.
//   --help, -h            Show this help message.

#include "vfs/archive_writer.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Options {
    fs::path inputDir;
    fs::path outputFile;
    std::vector<std::string> excludePatterns;
    bool verbose{false};
};

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " --input <dir> --output <file.pak> [options]\n"
              << "\n"
              << "Options:\n"
              << "  --input, -i <dir>     Source directory containing assets.\n"
              << "  --output, -o <file>   Output .pak file path.\n"
              << "  --exclude <pattern>   Pattern for files to exclude (can be repeated).\n"
              << "  --verbose, -v         Print files being added.\n"
              << "  --help, -h            Show this help message.\n";
}

bool parse_args(int argc, char* argv[], Options& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return false;
        } else if (arg == "--input" || arg == "-i") {
            if (++i >= argc) {
                std::cerr << "Error: --input requires a directory path.\n";
                return false;
            }
            opts.inputDir = argv[i];
        } else if (arg == "--output" || arg == "-o") {
            if (++i >= argc) {
                std::cerr << "Error: --output requires a file path.\n";
                return false;
            }
            opts.outputFile = argv[i];
        } else if (arg == "--exclude") {
            if (++i >= argc) {
                std::cerr << "Error: --exclude requires a pattern.\n";
                return false;
            }
            opts.excludePatterns.push_back(argv[i]);
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return false;
        }
    }

    if (opts.inputDir.empty()) {
        std::cerr << "Error: --input is required.\n";
        return false;
    }
    if (opts.outputFile.empty()) {
        std::cerr << "Error: --output is required.\n";
        return false;
    }

    return true;
}

bool matches_pattern(const std::string& filename, const std::string& pattern) {
    if (pattern.empty()) {
        return false;
    }

    if (pattern[0] == '*') {
        std::string suffix = pattern.substr(1);
        if (filename.length() >= suffix.length()) {
            return filename.compare(filename.length() - suffix.length(), suffix.length(), suffix) == 0;
        }
        return false;
    }

    return filename == pattern;
}

bool should_exclude(const std::string& relativePath, const std::vector<std::string>& patterns) {
    fs::path p(relativePath);
    std::string filename = p.filename().string();

    for (const auto& pattern : patterns) {
        if (matches_pattern(filename, pattern) || matches_pattern(relativePath, pattern)) {
            return true;
        }
    }
    return false;
}

int main(int argc, char* argv[]) {
    Options opts;
    if (!parse_args(argc, argv, opts)) {
        return 1;
    }

    std::error_code ec;
    if (!fs::is_directory(opts.inputDir, ec) || ec) {
        std::cerr << "Error: Input directory does not exist: " << opts.inputDir << "\n";
        return 1;
    }

    struct FileEntry {
        fs::path absolutePath;
        std::string archivePath;
    };
    std::vector<FileEntry> files;

    for (const auto& entry : fs::recursive_directory_iterator(opts.inputDir, ec)) {
        if (ec) {
            std::cerr << "Error iterating directory: " << ec.message() << "\n";
            return 1;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        fs::path relativePath = fs::relative(entry.path(), opts.inputDir, ec);
        if (ec) {
            continue;
        }

        std::string archivePath = relativePath.generic_string();

        if (should_exclude(archivePath, opts.excludePatterns)) {
            if (opts.verbose) {
                std::cout << "Excluding: " << archivePath << "\n";
            }
            continue;
        }

        files.push_back({entry.path(), archivePath});
    }

    if (files.empty()) {
        std::cerr << "Error: No files to pack.\n";
        return 1;
    }

    std::sort(files.begin(), files.end(),
              [](const FileEntry& a, const FileEntry& b) { return a.archivePath < b.archivePath; });

    fs::path outputDir = opts.outputFile.parent_path();
    if (!outputDir.empty() && !fs::exists(outputDir, ec)) {
        fs::create_directories(outputDir, ec);
        if (ec) {
            std::cerr << "Error: Cannot create output directory: " << ec.message() << "\n";
            return 1;
        }
    }

    // Create archive.
    shared::vfs::ArchiveWriter writer;
    if (!writer.begin(opts.outputFile)) {
        std::cerr << "Error: Cannot create output file: " << opts.outputFile << "\n";
        return 1;
    }

    std::uint64_t totalSize = 0;

    for (const auto& file : files) {
        if (!writer.add_file_from_disk(file.archivePath, file.absolutePath)) {
            std::cerr << "Error: Failed to add file: " << file.archivePath << "\n";
            writer.cancel();
            return 1;
        }

        auto size = fs::file_size(file.absolutePath, ec);
        if (!ec) {
            totalSize += size;
        }

        if (opts.verbose) {
            std::cout << file.archivePath << "\n";
        }
    }

    if (!writer.finalize()) {
        std::cerr << "Error: Failed to finalize archive.\n";
        writer.cancel();
        return 1;
    }

    // Report.
    auto outputSize = fs::file_size(opts.outputFile, ec);
    if (ec) {
        outputSize = 0;
    }

    std::cout << "Packed " << writer.file_count() << " files into " << opts.outputFile << "\n";
    std::cout << "  Input size:  " << (totalSize / 1024) << " KB\n";
    std::cout << "  Output size: " << (outputSize / 1024) << " KB\n";

    return 0;
}
