#include <catch2/catch_test_macros.hpp>

#include "vfs/archive_reader.hpp"
#include "vfs/archive_writer.hpp"
#include "vfs/vfs.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Helper to create a temporary test PAK archive.
class TestArchive {
public:
    explicit TestArchive(const fs::path& path) : path_(path) {}

    ~TestArchive() {
        std::error_code ec;
        fs::remove(path_, ec);
    }

    bool create(const std::vector<std::pair<std::string, std::string>>& files) {
        shared::vfs::ArchiveWriter writer;
        if (!writer.begin(path_)) {
            return false;
        }

        for (const auto& [name, content] : files) {
            std::vector<std::uint8_t> data(content.begin(), content.end());
            if (!writer.add_file(name, data)) {
                writer.cancel();
                return false;
            }
        }

        return writer.finalize();
    }

    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

// Helper to create temporary loose files.
class TempDir {
public:
    explicit TempDir(const std::string& prefix = "vfs_test") {
        path_ = fs::temp_directory_path() / (prefix + "_" + std::to_string(std::rand()));
        fs::create_directories(path_);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    const fs::path& path() const { return path_; }

    void create_file(const std::string& relativePath, const std::string& content) {
        fs::path fullPath = path_ / relativePath;
        fs::create_directories(fullPath.parent_path());
        std::ofstream ofs(fullPath, std::ios::binary);
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

private:
    fs::path path_;
};

TEST_CASE("ArchiveReader can open and read ZIP files", "[vfs][archive]") {
    TempDir tempDir;
    TestArchive archive(tempDir.path() / "test.pak");

    REQUIRE(archive.create({
        {"hello.txt", "Hello, World!"},
        {"textures/block.png", "PNG_DATA_HERE"},
        {"shaders/test.vs", "#version 330\nvoid main() {}"},
    }));

    shared::vfs::ArchiveReader reader;

    SECTION("open succeeds for valid archive") {
        REQUIRE(reader.open(archive.path()));
        REQUIRE(reader.is_open());
    }

    SECTION("open fails for non-existent file") {
        REQUIRE_FALSE(reader.open(tempDir.path() / "nonexistent.pak"));
        REQUIRE_FALSE(reader.is_open());
    }

    SECTION("entries are indexed correctly") {
        REQUIRE(reader.open(archive.path()));
        REQUIRE(reader.entries().size() == 3);
        REQUIRE(reader.has_file("hello.txt"));
        REQUIRE(reader.has_file("textures/block.png"));
        REQUIRE(reader.has_file("shaders/test.vs"));
        REQUIRE_FALSE(reader.has_file("nonexistent.txt"));
    }

    SECTION("extract returns file contents") {
        REQUIRE(reader.open(archive.path()));

        auto data = reader.extract("hello.txt");
        REQUIRE(data.has_value());
        std::string content(data->begin(), data->end());
        REQUIRE(content == "Hello, World!");
    }

    SECTION("list_directory returns direct children") {
        REQUIRE(reader.open(archive.path()));

        auto root = reader.list_directory("");
        REQUIRE(root.size() == 3);
        REQUIRE(std::find(root.begin(), root.end(), "hello.txt") != root.end());
        REQUIRE(std::find(root.begin(), root.end(), "textures/") != root.end());
        REQUIRE(std::find(root.begin(), root.end(), "shaders/") != root.end());

        auto textures = reader.list_directory("textures");
        REQUIRE(textures.size() == 1);
        REQUIRE(textures[0] == "block.png");
    }
}

TEST_CASE("VFS initialization and shutdown", "[vfs]") {
    TempDir tempDir;

    SECTION("init/shutdown cycle") {
        REQUIRE_FALSE(shared::vfs::is_initialized());

        shared::vfs::init(tempDir.path());
        REQUIRE(shared::vfs::is_initialized());
        REQUIRE(shared::vfs::get_game_dir() == tempDir.path());

        shared::vfs::shutdown();
        REQUIRE_FALSE(shared::vfs::is_initialized());
    }
}

TEST_CASE("VFS reads loose files", "[vfs]") {
    TempDir tempDir;
    tempDir.create_file("test.txt", "Loose file content");
    tempDir.create_file("subdir/nested.txt", "Nested content");

    shared::vfs::init(tempDir.path());

    SECTION("read_file finds loose file") {
        auto data = shared::vfs::read_file("test.txt");
        REQUIRE(data.has_value());
        std::string content(data->begin(), data->end());
        REQUIRE(content == "Loose file content");
    }

    SECTION("read_text_file returns string") {
        auto content = shared::vfs::read_text_file("subdir/nested.txt");
        REQUIRE(content.has_value());
        REQUIRE(*content == "Nested content");
    }

    SECTION("exists returns true for existing files") {
        REQUIRE(shared::vfs::exists("test.txt"));
        REQUIRE(shared::vfs::exists("subdir/nested.txt"));
        REQUIRE_FALSE(shared::vfs::exists("nonexistent.txt"));
    }

    SECTION("stat returns file info") {
        auto st = shared::vfs::stat("test.txt");
        REQUIRE(st.has_value());
        REQUIRE(st->size == 18);  // "Loose file content"
        REQUIRE_FALSE(st->is_directory);
        REQUIRE_FALSE(st->from_archive);
    }

    SECTION("list_dir returns directory contents") {
        auto entries = shared::vfs::list_dir("");
        REQUIRE(std::find(entries.begin(), entries.end(), "test.txt") != entries.end());
        REQUIRE(std::find(entries.begin(), entries.end(), "subdir/") != entries.end());
    }

    SECTION("resolve_loose_path returns filesystem path") {
        auto loosePath = shared::vfs::resolve_loose_path("test.txt");
        REQUIRE(loosePath.has_value());
        REQUIRE(*loosePath == tempDir.path() / "test.txt");

        auto missing = shared::vfs::resolve_loose_path("nonexistent.txt");
        REQUIRE_FALSE(missing.has_value());
    }

    shared::vfs::shutdown();
}

TEST_CASE("VFS reads from mounted archives", "[vfs]") {
    TempDir tempDir;
    TestArchive archive(tempDir.path() / "test.pak");

    REQUIRE(archive.create({
        {"archived.txt", "Content from archive"},
        {"data/config.json", "{\"key\": \"value\"}"},
    }));

    shared::vfs::init(tempDir.path());
    REQUIRE(shared::vfs::mount("test.pak"));

    SECTION("read_file finds archived file") {
        auto data = shared::vfs::read_file("archived.txt");
        REQUIRE(data.has_value());
        std::string content(data->begin(), data->end());
        REQUIRE(content == "Content from archive");
    }

    SECTION("exists returns true for archived files") {
        REQUIRE(shared::vfs::exists("archived.txt"));
        REQUIRE(shared::vfs::exists("data/config.json"));
    }

    SECTION("stat shows file is from archive") {
        auto st = shared::vfs::stat("archived.txt");
        REQUIRE(st.has_value());
        REQUIRE(st->from_archive);
    }

    shared::vfs::shutdown();
}

TEST_CASE("VFS loose files override archived files", "[vfs]") {
    TempDir tempDir;

    // Create archive with a file.
    TestArchive archive(tempDir.path() / "test.pak");
    REQUIRE(archive.create({
        {"override.txt", "ARCHIVE VERSION"},
    }));

    // Create loose file with same name.
    tempDir.create_file("override.txt", "LOOSE VERSION");

    shared::vfs::init(tempDir.path());
    REQUIRE(shared::vfs::mount("test.pak"));

    // Loose file should take priority.
    auto content = shared::vfs::read_text_file("override.txt");
    REQUIRE(content.has_value());
    REQUIRE(*content == "LOOSE VERSION");

    // Stat should show it's not from archive.
    auto st = shared::vfs::stat("override.txt");
    REQUIRE(st.has_value());
    REQUIRE_FALSE(st->from_archive);

    shared::vfs::shutdown();
}

TEST_CASE("VFS LooseOnly flag ignores archives", "[vfs]") {
    TempDir tempDir;

    TestArchive archive(tempDir.path() / "test.pak");
    REQUIRE(archive.create({
        {"archived_only.txt", "Should not be found"},
    }));

    shared::vfs::init(tempDir.path(), shared::vfs::InitFlags::LooseOnly);

    // Mount should succeed silently but not actually load archive.
    REQUIRE(shared::vfs::mount("test.pak"));

    // File from archive should not be found.
    REQUIRE_FALSE(shared::vfs::exists("archived_only.txt"));

    shared::vfs::shutdown();
}

TEST_CASE("VFS path normalization", "[vfs]") {
    TempDir tempDir;
    tempDir.create_file("dir/file.txt", "content");

    shared::vfs::init(tempDir.path());

    SECTION("handles leading slash") {
        auto data = shared::vfs::read_file("/dir/file.txt");
        REQUIRE(data.has_value());
    }

    SECTION("handles backslashes") {
        auto data = shared::vfs::read_file("dir\\file.txt");
        REQUIRE(data.has_value());
    }

    SECTION("handles double slashes") {
        auto data = shared::vfs::read_file("dir//file.txt");
        REQUIRE(data.has_value());
    }

    shared::vfs::shutdown();
}
