#pragma once

#include <chrono>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "serializer.hpp"
#include "signer.hpp"

namespace blockit::ledger {

    using namespace std::chrono;

    struct Timestamp {
        int32_t sec;
        uint32_t nanosec;
        inline Timestamp() : sec(0), nanosec(0) {}
        inline Timestamp(int32_t s, uint32_t ns) : sec(s), nanosec(ns) {}
        inline std::string serialize() const {
            return std::string("{") + "\"sec\": " + std::to_string(sec) + ", \"nanosec\": " + std::to_string(nanosec) +
                   "}";
        }
        inline static Timestamp deserialize(const std::string &data) {
            Timestamp result;
            size_t sec_pos = data.find("\"sec\": ") + 7;
            size_t sec_end = data.find(',', sec_pos);
            result.sec = std::stoi(data.substr(sec_pos, sec_end - sec_pos));
            size_t nanosec_pos = data.find("\"nanosec\": ") + 11;
            size_t nanosec_end = data.find('}', nanosec_pos);
            result.nanosec = static_cast<uint32_t>(std::stoul(data.substr(nanosec_pos, nanosec_end - nanosec_pos)));
            return result;
        }
        inline std::vector<uint8_t> serializeBinary() const {
            std::vector<uint8_t> buffer;
            BinarySerializer::writeUint32(buffer, static_cast<uint32_t>(sec));
            BinarySerializer::writeUint32(buffer, nanosec);
            return buffer;
        }
        inline static Timestamp deserializeBinary(const std::vector<uint8_t> &data) {
            Timestamp result;
            size_t offset = 0;
            result.sec = static_cast<int32_t>(BinarySerializer::readUint32(data, offset));
            result.nanosec = BinarySerializer::readUint32(data, offset);
            return result;
        }
    };

    template <typename T> class has_to_string {
      private:
        template <typename U> static auto test(int) -> decltype(std::declval<U>().to_string(), std::true_type{});
        template <typename> static std::false_type test(...);

      public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };

    template <typename T> class Transaction {
        static_assert(has_to_string<T>::value, "Type T must have a 'to_string() const' method");

      public:
        Timestamp timestamp_{};
        int16_t priority_{};
        std::string uuid_{};
        T function_{};
        std::vector<unsigned char> signature_{};
        Transaction() = default;
        inline Transaction(std::string uuid, T function, int16_t priority = 100) {
            priority_ = priority;
            uuid_ = std::move(uuid);
            function_ = std::move(function);
            timestamp_.sec = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
            timestamp_.nanosec =
                duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count() % 1000000000;
        }
        inline void signTransaction(std::shared_ptr<Crypto> privateKey_) { signature_ = privateKey_->sign(toString()); }
        inline bool isValid() const {
            if (uuid_.empty() || function_.to_string().empty() || signature_.empty() || priority_ < 0 ||
                priority_ > 255)
                return false;
            return true;
        }
        inline std::string toString() const {
            std::stringstream ss;
            ss << timestamp_.sec << timestamp_.nanosec << priority_ << uuid_ << function_.to_string();
            return ss.str();
        }

        inline std::string serialize() const { return serializeJson(); }
        inline std::vector<uint8_t> serializeBinary() const {
            std::vector<uint8_t> buffer;
            BinarySerializer::writeUint32(buffer, static_cast<uint32_t>(timestamp_.sec));
            BinarySerializer::writeUint32(buffer, timestamp_.nanosec);
            BinarySerializer::writeInt16(buffer, priority_);
            BinarySerializer::writeString(buffer, uuid_);
            auto functionData = TypeSerializer<T>::serializeBinary(function_);
            BinarySerializer::writeBytes(buffer, functionData);
            BinarySerializer::writeBytes(buffer, signature_);
            return buffer;
        }
        inline std::string serializeJson() const {
            std::stringstream ss;
            ss << '{';
            ss << "\"uuid\": \"" << JsonSerializer::escapeJson(uuid_) << "\",";
            ss << "\"timestamp\": {\"sec\": " << timestamp_.sec << ", \"nanosec\": " << timestamp_.nanosec << "},";
            ss << "\"priority\": " << priority_ << ",";
            ss << "\"function\": " << TypeSerializer<T>::serializeJson(function_) << ",";
            std::string signature_b64 = base64Encode(signature_);
            ss << "\"signature\": \"" << signature_b64 << "\"";
            ss << '}';
            return ss.str();
        }
        static Transaction<T> deserialize(const std::string &data) { return deserializeJson(data); }
        template <typename U = T>
        static Transaction<T> deserialize(
            const std::vector<uint8_t> &data, SerializationFormat format = SerializationFormat::BINARY,
            typename std::enable_if<TypeSerializer<U>::supportsBinary() || TypeSerializer<U>::supportsJson()>::type * =
                0) {
            if (format == SerializationFormat::BINARY)
                return deserializeBinary(data);
            else {
                std::string jsonStr(data.begin(), data.end());
                return deserializeJson(jsonStr);
            }
        }
        static Transaction<T> deserializeAuto(const std::vector<uint8_t> &data) {
            if (data.size() >= 4) {
                uint32_t magic = *reinterpret_cast<const uint32_t *>(data.data());
                if (magic == BinaryHeader::MAGIC_NUMBER)
                    return deserializeBinary(data);
            }
            if (!data.empty() && data[0] == '{') {
                std::string jsonStr(data.begin(), data.end());
                return deserializeJson(jsonStr);
            }
            return deserializeBinary(data);
        }
        static Transaction<T> deserializeBinary(const std::vector<uint8_t> &data) {
            Transaction<T> result;
            size_t offset = 0;
            result.timestamp_.sec = static_cast<int32_t>(BinarySerializer::readUint32(data, offset));
            result.timestamp_.nanosec = BinarySerializer::readUint32(data, offset);
            result.priority_ = BinarySerializer::readInt16(data, offset);
            result.uuid_ = BinarySerializer::readString(data, offset);
            auto functionData = BinarySerializer::readBytes(data, offset);
            result.function_ = TypeSerializer<T>::deserializeBinary(functionData);
            result.signature_ = BinarySerializer::readBytesToUChar(data, offset);
            return result;
        }
        static Transaction<T> deserializeJson(const std::string &data) {
            Transaction<T> result;
            result.uuid_ = JsonSerializer::extractJsonValue(data, "uuid");
            std::string timestampJson = JsonSerializer::extractJsonValue(data, "timestamp");
            result.timestamp_.sec = std::stoi(JsonSerializer::extractJsonValue(timestampJson, "sec"));
            result.timestamp_.nanosec =
                static_cast<uint32_t>(std::stoul(JsonSerializer::extractJsonValue(timestampJson, "nanosec")));
            result.priority_ = static_cast<int16_t>(std::stoi(JsonSerializer::extractJsonValue(data, "priority")));
            std::string functionJson = JsonSerializer::extractJsonValue(data, "function");
            result.function_ = TypeSerializer<T>::deserializeJson(functionJson);
            std::string signature_b64 = JsonSerializer::extractJsonValue(data, "signature");
            result.signature_ = base64Decode(signature_b64);
            return result;
        }
    };

} // namespace blockit::ledger
