#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace blockit::ledger {

    // Enhanced SFINAE detection for serialization capabilities
    template <typename T> class has_binary_serialize {
        template <typename U>
        static auto test_binary_serialize(int) -> decltype(std::declval<U>().serializeBinary(), std::true_type{});
        template <typename> static std::false_type test_binary_serialize(...);

        template <typename U>
        static auto test_binary_deserialize(int)
            -> decltype(U::deserializeBinary(std::declval<std::vector<uint8_t>>()), std::true_type{});
        template <typename> static std::false_type test_binary_deserialize(...);

      public:
        static constexpr bool has_serialize_method = decltype(test_binary_serialize<T>(0))::value;
        static constexpr bool has_deserialize_method = decltype(test_binary_deserialize<T>(0))::value;
        static constexpr bool value = has_serialize_method && has_deserialize_method;
    };

    template <typename T> class has_json_serialize {
        template <typename U>
        static auto test_json_serialize(int) -> decltype(std::declval<U>().serialize(), std::true_type{});
        template <typename> static std::false_type test_json_serialize(...);

        template <typename U>
        static auto test_json_deserialize(int)
            -> decltype(U::deserialize(std::declval<std::string>()), std::true_type{});
        template <typename> static std::false_type test_json_deserialize(...);

      public:
        static constexpr bool has_serialize_method = decltype(test_json_serialize<T>(0))::value;
        static constexpr bool has_deserialize_method = decltype(test_json_deserialize<T>(0))::value;
        static constexpr bool value = has_serialize_method && has_deserialize_method;
    };

    // Serialization format enum
    enum class SerializationFormat {
        BINARY, // Default
        JSON
    };

    // Binary serialization utility class
    class BinarySerializer {
      public:
        // Write operations
        static void writeUint8(std::vector<uint8_t> &buffer, uint8_t value) { buffer.push_back(value); }

        static void writeUint16(std::vector<uint8_t> &buffer, uint16_t value) {
            // Use little-endian for consistency across platforms
            buffer.push_back(static_cast<uint8_t>(value & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        }

        static void writeUint32(std::vector<uint8_t> &buffer, uint32_t value) {
            // Use little-endian for consistency across platforms
            buffer.push_back(static_cast<uint8_t>(value & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        }

        static void writeInt16(std::vector<uint8_t> &buffer, int16_t value) {
            writeUint16(buffer, static_cast<uint16_t>(value));
        }

        static void writeDouble(std::vector<uint8_t> &buffer, double value) {
            const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&value);
            buffer.insert(buffer.end(), bytes, bytes + sizeof(double));
        }

        static void writeString(std::vector<uint8_t> &buffer, const std::string &str) {
            writeUint32(buffer, static_cast<uint32_t>(str.length()));
            buffer.insert(buffer.end(), str.begin(), str.end());
        }

        static void writeBytes(std::vector<uint8_t> &buffer, const std::vector<uint8_t> &data) {
            writeUint32(buffer, static_cast<uint32_t>(data.size()));
            buffer.insert(buffer.end(), data.begin(), data.end());
        }

        // Read operations
        static uint8_t readUint8(const std::vector<uint8_t> &buffer, size_t &offset) {
            if (offset >= buffer.size()) {
                throw std::runtime_error("Buffer underflow reading uint8");
            }
            return buffer[offset++];
        }

        static uint16_t readUint16(const std::vector<uint8_t> &buffer, size_t &offset) {
            if (offset + 2 > buffer.size()) {
                throw std::runtime_error("Buffer underflow reading uint16");
            }
            uint16_t value = static_cast<uint16_t>(buffer[offset]) | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
            offset += 2;
            return value;
        }

        static uint32_t readUint32(const std::vector<uint8_t> &buffer, size_t &offset) {
            if (offset + 4 > buffer.size()) {
                throw std::runtime_error("Buffer underflow reading uint32");
            }
            uint32_t value = static_cast<uint32_t>(buffer[offset]) | (static_cast<uint32_t>(buffer[offset + 1]) << 8) |
                             (static_cast<uint32_t>(buffer[offset + 2]) << 16) |
                             (static_cast<uint32_t>(buffer[offset + 3]) << 24);
            offset += 4;
            return value;
        }

        static int16_t readInt16(const std::vector<uint8_t> &buffer, size_t &offset) {
            return static_cast<int16_t>(readUint16(buffer, offset));
        }

        static double readDouble(const std::vector<uint8_t> &buffer, size_t &offset) {
            if (offset + sizeof(double) > buffer.size()) {
                throw std::runtime_error("Buffer underflow reading double");
            }
            double value;
            std::memcpy(&value, buffer.data() + offset, sizeof(double));
            offset += sizeof(double);
            return value;
        }

        static std::string readString(const std::vector<uint8_t> &buffer, size_t &offset) {
            uint32_t length = readUint32(buffer, offset);
            if (offset + length > buffer.size()) {
                throw std::runtime_error("Buffer underflow reading string");
            }
            std::string result(reinterpret_cast<const char *>(buffer.data() + offset), length);
            offset += length;
            return result;
        }

        static std::vector<uint8_t> readBytes(const std::vector<uint8_t> &buffer, size_t &offset) {
            uint32_t length = readUint32(buffer, offset);
            if (offset + length > buffer.size()) {
                throw std::runtime_error("Buffer underflow reading bytes");
            }
            std::vector<uint8_t> result(buffer.begin() + offset, buffer.begin() + offset + length);
            offset += length;
            return result;
        }

        static std::vector<unsigned char> readBytesToUChar(const std::vector<uint8_t> &buffer, size_t &offset) {
            uint32_t length = readUint32(buffer, offset);
            if (offset + length > buffer.size()) {
                throw std::runtime_error("Buffer underflow reading bytes");
            }
            std::vector<unsigned char> result(buffer.begin() + offset, buffer.begin() + offset + length);
            offset += length;
            return result;
        }

        // CRC32 checksum calculation
        static uint32_t calculateCRC32(const std::vector<uint8_t> &data) {
            uint32_t crc = 0xFFFFFFFF;
            for (uint8_t byte : data) {
                crc ^= byte;
                for (int i = 0; i < 8; i++) {
                    crc = (crc >> 1) ^ (0xEDB88320 * (crc & 1));
                }
            }
            return ~crc;
        }
    };

    // JSON serialization utilities
    class JsonSerializer {
      public:
        static std::string escapeJson(const std::string &str) {
            std::string result;
            for (char c : str) {
                switch (c) {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    result += c;
                    break;
                }
            }
            return result;
        }

        static std::string extractJsonValue(const std::string &json, const std::string &key) {
            std::string searchPattern = "\"" + key + "\": ";
            size_t start = json.find(searchPattern);
            if (start == std::string::npos) {
                throw std::runtime_error("Key not found: " + key);
            }
            start += searchPattern.length();

            // Handle different value types
            if (json[start] == '"') {
                // String value
                start++; // Skip opening quote
                size_t end = json.find("\"", start);
                return json.substr(start, end - start);
            } else if (json[start] == '{') {
                // Object value - find matching brace
                int braceCount = 1;
                size_t end = start + 1;
                while (end < json.length() && braceCount > 0) {
                    if (json[end] == '{')
                        braceCount++;
                    else if (json[end] == '}')
                        braceCount--;
                    end++;
                }
                return json.substr(start, end - start);
            } else {
                // Numeric or boolean value
                size_t end = json.find_first_of(",}", start);
                return json.substr(start, end - start);
            }
        }
    };

    // Binary format header
    struct BinaryHeader {
        static constexpr uint32_t MAGIC_NUMBER = 0x424C4B54; // "BLKT"
        static constexpr uint16_t VERSION = 1;

        uint32_t magic;
        uint16_t version;
        uint32_t data_length;
        uint32_t checksum;

        BinaryHeader() : magic(MAGIC_NUMBER), version(VERSION), data_length(0), checksum(0) {}

        void serialize(std::vector<uint8_t> &buffer) const {
            BinarySerializer::writeUint32(buffer, magic);
            BinarySerializer::writeUint16(buffer, version);
            BinarySerializer::writeUint32(buffer, data_length);
            BinarySerializer::writeUint32(buffer, checksum);
        }

        static BinaryHeader deserialize(const std::vector<uint8_t> &buffer, size_t &offset) {
            BinaryHeader header;
            header.magic = BinarySerializer::readUint32(buffer, offset);
            header.version = BinarySerializer::readUint16(buffer, offset);
            header.data_length = BinarySerializer::readUint32(buffer, offset);
            header.checksum = BinarySerializer::readUint32(buffer, offset);

            if (header.magic != MAGIC_NUMBER) {
                throw std::runtime_error("Invalid binary format magic number");
            }
            if (header.version != VERSION) {
                throw std::runtime_error("Unsupported binary format version");
            }

            return header;
        }
    };

    // Unified type serialization helper
    template <typename T> class TypeSerializer {
      public:
        // Serialize T to binary format
        static std::vector<uint8_t> serializeBinary(const T &obj) {
            if constexpr (has_binary_serialize<T>::value) {
                return obj.serializeBinary();
            } else {
                // Fallback: serialize to_string() as binary
                std::vector<uint8_t> buffer;
                BinarySerializer::writeString(buffer, obj.to_string());
                return buffer;
            }
        }

        // Deserialize T from binary format
        static T deserializeBinary(const std::vector<uint8_t> &data) {
            if constexpr (has_binary_serialize<T>::value) {
                return T::deserializeBinary(data);
            } else {
                // Fallback: This is limited - cannot reconstruct T from just to_string()
                // The type should implement binary serialization for full functionality
                throw std::runtime_error("Type does not support binary deserialization");
            }
        }

        // Serialize T to JSON format
        static std::string serializeJson(const T &obj) {
            if constexpr (has_json_serialize<T>::value) {
                return obj.serialize();
            } else {
                // Fallback: use to_string() as JSON string value
                return "\"" + JsonSerializer::escapeJson(obj.to_string()) + "\"";
            }
        }

        // Deserialize T from JSON format
        static T deserializeJson(const std::string &data) {
            if constexpr (has_json_serialize<T>::value) {
                return T::deserialize(data);
            } else {
                // Fallback: This is limited - cannot reconstruct T from just to_string()
                // The type should implement JSON serialization for full functionality
                throw std::runtime_error("Type does not support JSON deserialization");
            }
        }

        // Check if type supports binary serialization
        static constexpr bool supportsBinary() { return has_binary_serialize<T>::value; }

        // Check if type supports JSON serialization
        static constexpr bool supportsJson() { return has_json_serialize<T>::value; }
    };

} // namespace blockit::ledger
