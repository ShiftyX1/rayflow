// BedWars Asset Packing Tool
// Packs game assets into a .pak archive for release builds.

#include "engine/vfs/archive_writer.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Asset directories to pack
static const std::vector<std::string> kAssetDirs = {
    "textures",
    "shaders",
    "fonts",
    "ui",
    "models",
    "sounds",
    "scripts"
};

// File extensions to include (empty = all)
static const std::vector<std::string> kIncludeExtensions = {
    ".png", ".jpg", ".jpeg", ".bmp", ".tga",    // Images
    ".vs", ".fs", ".glsl",                       // Shaders
    ".ttf", ".otf",                              // Fonts
    ".xml", ".css", ".json",                     // Data
    ".obj", ".gltf", ".glb",                     // Models
    ".wav", ".ogg", ".mp3",                      // Audio
    ".lua"                                       // Scripts
};

bool should_include(const fs::path& path) {
    if (kIncludeExtensions.empty()) {
        return true;
    }
    
    std::string ext = path.extension().string();
    // Convert to lowercase for comparison
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    
    for (const auto& allowed : kIncludeExtensions) {
        if (ext == allowed) {
            return true;
        }
    }
    return false;
}

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  -i, --input <dir>    Input directory (default: current directory)\n";
    std::cout << "  -o, --output <file>  Output PAK file (default: assets.pak)\n";
    std::cout << "  --exclude <dir>      Exclude a top-level directory (can be repeated)\n";
    std::cout << "  --prefix <prefix>    Prefix to prepend to archive paths\n";
    std::cout << "  -v, --verbose        Verbose output\n";
    std::cout << "  -h, --help           Show this help\n";
    std::cout << "\n";
    std::cout << "Packs game assets from the following directories:\n";
    for (const auto& dir : kAssetDirs) {
        std::cout << "  " << dir << "/\n";
    }
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program << " --input staging --output assets.pak --exclude scripts\n";
    std::cout << "  " << program << " --input staging/scripts --output scripts.pak --prefix scripts\n";
}

int main(int argc, char* argv[]) {
    fs::path inputDir = fs::current_path();
    fs::path outputFile = "assets.pak";
    bool verbose = false;
    std::vector<std::string> excludeDirs;
    std::string prefix;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        else if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
            inputDir = argv[++i];
        }
        else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            outputFile = argv[++i];
        }
        else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        }
        else if (arg == "--exclude" && i + 1 < argc) {
            excludeDirs.push_back(argv[++i]);
        }
        else if (arg == "--prefix" && i + 1 < argc) {
            prefix = argv[++i];
            // Ensure prefix ends with /
            if (!prefix.empty() && prefix.back() != '/') {
                prefix += '/';
            }
        }
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Validate input directory
    if (!fs::exists(inputDir)) {
        std::cerr << "Error: Input directory does not exist: " << inputDir << "\n";
        return 1;
    }
    
    std::cout << "BedWars Asset Packer\n";
    std::cout << "Input:  " << inputDir << "\n";
    std::cout << "Output: " << outputFile << "\n";
    if (!prefix.empty()) {
        std::cout << "Prefix: " << prefix << "\n";
    }
    if (!excludeDirs.empty()) {
        std::cout << "Exclude:";
        for (const auto& e : excludeDirs) std::cout << " " << e;
        std::cout << "\n";
    }
    std::cout << "\n";
    
    engine::vfs::ArchiveWriter writer;
    
    if (!writer.begin(outputFile)) {
        std::cerr << "Error: Failed to create output file: " << outputFile << "\n";
        return 1;
    }
    
    std::size_t fileCount = 0;
    std::size_t totalSize = 0;
    
    // Build effective asset dirs list (excluding excluded dirs)
    std::vector<std::string> effectiveDirs;
    for (const auto& dir : kAssetDirs) {
        bool excluded = false;
        for (const auto& ex : excludeDirs) {
            if (dir == ex) {
                excluded = true;
                if (verbose) {
                    std::cout << "Excluding: " << dir << "/\n";
                }
                break;
            }
        }
        if (!excluded) {
            effectiveDirs.push_back(dir);
        }
    }
    
    for (const auto& dirName : effectiveDirs) {
        fs::path assetDir = inputDir / dirName;
        
        if (!fs::exists(assetDir)) {
            if (verbose) {
                std::cout << "Skipping (not found): " << dirName << "/\n";
            }
            continue;
        }
        
        std::cout << "Packing: " << dirName << "/\n";
        
        std::error_code ec;
        for (const auto& entry : fs::recursive_directory_iterator(assetDir, ec)) {
            if (ec) {
                std::cerr << "Warning: Error iterating directory: " << ec.message() << "\n";
                break;
            }
            
            if (!entry.is_regular_file()) {
                continue;
            }
            
            if (!should_include(entry.path())) {
                if (verbose) {
                    std::cout << "  Skipping: " << entry.path().filename() << " (unsupported extension)\n";
                }
                continue;
            }
            
            // Get path relative to input directory
            fs::path relativePath = fs::relative(entry.path(), inputDir, ec);
            if (ec) {
                std::cerr << "Warning: Failed to get relative path for: " << entry.path() << "\n";
                continue;
            }
            
            // Convert to forward slashes for archive path
            std::string archivePath = prefix + relativePath.generic_string();
            
            if (verbose) {
                std::cout << "  Adding: " << archivePath << "\n";
            }
            
            if (!writer.add_file_from_disk(archivePath, entry.path())) {
                std::cerr << "Error: Failed to add file: " << entry.path() << "\n";
                writer.cancel();
                return 1;
            }
            
            fileCount++;
            totalSize += entry.file_size(ec);
        }
    }
    
    // Fallback: if none of kAssetDirs existed, pack all files in inputDir directly
    // This is used e.g. for scripts.pak where --input points to the scripts/ subfolder
    if (fileCount == 0) {
        std::cout << "No known asset directories found — packing all files in input directory\n";
        
        std::error_code ec;
        for (const auto& entry : fs::recursive_directory_iterator(inputDir, ec)) {
            if (ec) {
                std::cerr << "Warning: Error iterating directory: " << ec.message() << "\n";
                break;
            }
            
            if (!entry.is_regular_file()) {
                continue;
            }
            
            if (!should_include(entry.path())) {
                if (verbose) {
                    std::cout << "  Skipping: " << entry.path().filename() << " (unsupported extension)\n";
                }
                continue;
            }
            
            fs::path relativePath = fs::relative(entry.path(), inputDir, ec);
            if (ec) {
                std::cerr << "Warning: Failed to get relative path for: " << entry.path() << "\n";
                continue;
            }
            
            std::string archivePath = prefix + relativePath.generic_string();
            
            if (verbose) {
                std::cout << "  Adding: " << archivePath << "\n";
            }
            
            if (!writer.add_file_from_disk(archivePath, entry.path())) {
                std::cerr << "Error: Failed to add file: " << entry.path() << "\n";
                writer.cancel();
                return 1;
            }
            
            fileCount++;
            totalSize += entry.file_size(ec);
        }
    }
    
    if (!writer.finalize()) {
        std::cerr << "Error: Failed to finalize archive\n";
        return 1;
    }
    
    // Get output file size
    std::error_code ec;
    auto pakSize = fs::file_size(outputFile, ec);
    
    std::cout << "\n";
    std::cout << "Done!\n";
    std::cout << "Files packed: " << fileCount << "\n";
    std::cout << "Total size:   " << (totalSize / 1024) << " KB\n";
    std::cout << "PAK size:     " << (pakSize / 1024) << " KB\n";
    
    return 0;
}
