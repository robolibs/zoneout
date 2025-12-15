#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace zoneout {

    class UUID {
      public:
        using ByteArray = std::array<uint8_t, 16>;

        UUID() { generate(); }

        explicit UUID(const std::string &str) { fromString(str); }

        explicit UUID(const ByteArray &bytes) : data_(bytes) {}

        void generate() {
            static thread_local std::random_device rd;
            static thread_local std::mt19937_64 gen(rd());
            static thread_local std::uniform_int_distribution<uint64_t> dis;

            // Generate 128 bits of randomness
            uint64_t high = dis(gen);
            uint64_t low = dis(gen);

            // Set version (4) and variant bits according to RFC 4122
            high = (high & 0xFFFFFFFFFFFF0FFFUL) | 0x0000000000004000UL; // Version 4
            low = (low & 0x3FFFFFFFFFFFFFFFUL) | 0x8000000000000000UL;   // Variant 10

            // Pack into byte array
            for (int i = 0; i < 8; ++i) {
                data_[i] = static_cast<uint8_t>(high >> (56 - i * 8));
                data_[i + 8] = static_cast<uint8_t>(low >> (56 - i * 8));
            }
        }

        std::string toString() const {
            std::stringstream ss;
            ss << std::hex << std::setfill('0');

            // Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
            for (size_t i = 0; i < data_.size(); ++i) {
                if (i == 4 || i == 6 || i == 8 || i == 10) {
                    ss << '-';
                }
                ss << std::setw(2) << static_cast<unsigned>(data_[i]);
            }

            return ss.str();
        }

        void fromString(const std::string &str) {
            if (str.length() != 36) {
                throw std::invalid_argument("Invalid UUID string format");
            }

            size_t byte_idx = 0;
            for (size_t i = 0; i < str.length(); i += 2) {
                if (str[i] == '-') {
                    i++;
                }
                if (i + 1 >= str.length()) {
                    throw std::invalid_argument("Invalid UUID string format");
                }

                std::string byte_str = str.substr(i, 2);
                data_[byte_idx++] = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
            }
        }

        bool operator==(const UUID &other) const { return data_ == other.data_; }

        bool operator!=(const UUID &other) const { return !(*this == other); }

        bool operator<(const UUID &other) const { return data_ < other.data_; }

        const ByteArray &bytes() const { return data_; }

        bool isNull() const {
            return std::all_of(data_.begin(), data_.end(), [](uint8_t b) { return b == 0; });
        }

        static UUID null() { return UUID(ByteArray{}); }

      private:
        ByteArray data_;
    };

    // Hash function for UUID to enable use in unordered containers
    struct UUIDHash {
        std::size_t operator()(const UUID &uuid) const {
            const auto &bytes = uuid.bytes();
            std::size_t h1 = 0;
            std::size_t h2 = 0;

            // Simple hash combining
            for (size_t i = 0; i < 8; ++i) {
                h1 = h1 * 31 + bytes[i];
                h2 = h2 * 31 + bytes[i + 8];
            }

            return h1 ^ (h2 << 1);
        }
    };

    // Convenience function for generating UUIDs
    inline UUID generateUUID() { return UUID(); }

    // Convert UUID to/from string
    inline std::string uuidToString(const UUID &uuid) { return uuid.toString(); }

    inline UUID uuidFromString(const std::string &str) { return UUID(str); }

} // namespace zoneout