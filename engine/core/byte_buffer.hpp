#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

// ============================================================================
// ByteWriter - Serialize data to bytes
// ============================================================================

class ByteWriter {
public:
    ByteWriter() = default;
    explicit ByteWriter(std::size_t reserve) { data_.reserve(reserve); }

    // --- Primitives ---
    
    void write_u8(std::uint8_t v) {
        data_.push_back(v);
    }
    
    void write_u16(std::uint16_t v) {
        data_.push_back(static_cast<std::uint8_t>(v & 0xFF));
        data_.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    }
    
    void write_u32(std::uint32_t v) {
        data_.push_back(static_cast<std::uint8_t>(v & 0xFF));
        data_.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        data_.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        data_.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    }
    
    void write_u64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            data_.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF));
        }
    }
    
    void write_i32(std::int32_t v) {
        write_u32(static_cast<std::uint32_t>(v));
    }
    
    void write_f32(float v) {
        std::uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        write_u32(bits);
    }
    
    void write_bool(bool v) {
        write_u8(v ? 1 : 0);
    }

    // --- Strings ---
    
    void write_string(std::string_view s) {
        if (s.size() > 0xFFFF) {
            throw std::runtime_error("String too long for serialization");
        }
        write_u16(static_cast<std::uint16_t>(s.size()));
        data_.insert(data_.end(), s.begin(), s.end());
    }

    // --- Raw bytes ---
    
    void write_bytes(std::span<const std::uint8_t> bytes) {
        data_.insert(data_.end(), bytes.begin(), bytes.end());
    }

    // --- Access ---
    
    std::span<const std::uint8_t> data() const { return data_; }
    std::vector<std::uint8_t> take() { return std::move(data_); }
    void clear() { data_.clear(); }

private:
    std::vector<std::uint8_t> data_;
};

// ============================================================================
// ByteReader - Deserialize data from bytes
// ============================================================================

class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> data)
        : data_(data), pos_(0) {}

    // --- Primitives ---
    
    std::uint8_t read_u8() {
        check_remaining(1);
        return data_[pos_++];
    }
    
    std::uint16_t read_u16() {
        check_remaining(2);
        std::uint16_t v = static_cast<std::uint16_t>(data_[pos_])
                       | (static_cast<std::uint16_t>(data_[pos_ + 1]) << 8);
        pos_ += 2;
        return v;
    }
    
    std::uint32_t read_u32() {
        check_remaining(4);
        std::uint32_t v = static_cast<std::uint32_t>(data_[pos_])
                       | (static_cast<std::uint32_t>(data_[pos_ + 1]) << 8)
                       | (static_cast<std::uint32_t>(data_[pos_ + 2]) << 16)
                       | (static_cast<std::uint32_t>(data_[pos_ + 3]) << 24);
        pos_ += 4;
        return v;
    }
    
    std::uint64_t read_u64() {
        check_remaining(8);
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<std::uint64_t>(data_[pos_ + i]) << (i * 8);
        }
        pos_ += 8;
        return v;
    }
    
    std::int32_t read_i32() {
        return static_cast<std::int32_t>(read_u32());
    }
    
    float read_f32() {
        std::uint32_t bits = read_u32();
        float v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }
    
    bool read_bool() {
        return read_u8() != 0;
    }

    // --- Strings ---
    
    std::string read_string() {
        std::uint16_t len = read_u16();
        check_remaining(len);
        std::string s(reinterpret_cast<const char*>(data_.data() + pos_), len);
        pos_ += len;
        return s;
    }

    // --- Raw bytes ---
    
    std::span<const std::uint8_t> read_bytes(std::size_t count) {
        check_remaining(count);
        auto span = data_.subspan(pos_, count);
        pos_ += count;
        return span;
    }

    // --- State ---
    
    std::size_t position() const { return pos_; }
    std::size_t remaining() const { return data_.size() - pos_; }
    bool at_end() const { return pos_ >= data_.size(); }

private:
    void check_remaining(std::size_t need) {
        if (pos_ + need > data_.size()) {
            throw std::runtime_error("ByteReader: not enough data");
        }
    }

    std::span<const std::uint8_t> data_;
    std::size_t pos_;
};

} // namespace engine
