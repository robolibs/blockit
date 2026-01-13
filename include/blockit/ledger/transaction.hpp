#pragma once

#include <chrono>
#include <datapod/datapod.hpp>
#include <memory>
#include <sstream>
#include <type_traits>

#include "signer.hpp"

namespace blockit {

    using namespace std::chrono;

    struct Timestamp {
        dp::i32 sec{0};
        dp::u32 nanosec{0};

        Timestamp() = default;
        Timestamp(dp::i32 s, dp::u32 ns) : sec(s), nanosec(ns) {}

        auto members() { return std::tie(sec, nanosec); }
        auto members() const { return std::tie(sec, nanosec); }
    };

    template <typename T> class has_to_string {
      private:
        template <typename U> static auto test(int) -> decltype(std::declval<U>().to_string(), std::true_type{});
        template <typename> static std::false_type test(...);

      public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };

    template <typename T> class has_members {
      private:
        template <typename U> static auto test(int) -> decltype(std::declval<U>().members(), std::true_type{});
        template <typename> static std::false_type test(...);

      public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };

    template <typename T> class Transaction {
        static_assert(has_to_string<T>::value, "Type T must have a 'to_string() const' method");

      public:
        Timestamp timestamp_{};
        dp::i16 priority_{};
        dp::String uuid_{};
        T function_{};
        dp::Vector<dp::u8> signature_{};

        Transaction() = default;

        inline Transaction(const std::string &uuid, T function, dp::i16 priority = 100) {
            priority_ = priority;
            uuid_ = dp::String(uuid.c_str());
            function_ = std::move(function);
            timestamp_.sec = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
            timestamp_.nanosec =
                duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count() % 1000000000;
        }

        auto members() { return std::tie(timestamp_, priority_, uuid_, function_, signature_); }
        auto members() const { return std::tie(timestamp_, priority_, uuid_, function_, signature_); }

        inline dp::Result<void, dp::Error> signTransaction(std::shared_ptr<Crypto> privateKey_) {
            auto sign_result = privateKey_->sign(toString());
            if (!sign_result.is_ok()) {
                return dp::Result<void, dp::Error>::err(sign_result.error());
            }
            auto sig = sign_result.value();
            signature_ = dp::Vector<dp::u8>(sig.begin(), sig.end());
            return dp::Result<void, dp::Error>::ok();
        }

        inline dp::Result<bool, dp::Error> isValid() const {
            if (uuid_.empty()) {
                return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("Transaction UUID is empty"));
            }
            if (function_.to_string().empty()) {
                return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("Transaction function is empty"));
            }
            if (signature_.empty()) {
                return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("Transaction signature is empty"));
            }
            if (priority_ < 0 || priority_ > 255) {
                return dp::Result<bool, dp::Error>::err(
                    dp::Error::invalid_argument("Transaction priority out of range"));
            }
            return dp::Result<bool, dp::Error>::ok(true);
        }

        inline std::string toString() const {
            std::stringstream ss;
            ss << timestamp_.sec << timestamp_.nanosec << priority_ << std::string(uuid_.c_str())
               << function_.to_string();
            return ss.str();
        }

        // Serialize to binary using datapod
        inline dp::ByteBuf serialize() const {
            auto &self = const_cast<Transaction<T> &>(*this);
            return dp::serialize<dp::Mode::WITH_VERSION>(self);
        }

        // Deserialize from binary using datapod
        static dp::Result<Transaction<T>, dp::Error> deserialize(const dp::ByteBuf &data) {
            try {
                auto result = dp::deserialize<dp::Mode::WITH_VERSION, Transaction<T>>(data);
                return dp::Result<Transaction<T>, dp::Error>::ok(std::move(result));
            } catch (const std::exception &e) {
                return dp::Result<Transaction<T>, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
            }
        }

        static dp::Result<Transaction<T>, dp::Error> deserialize(const dp::u8 *data, dp::usize size) {
            try {
                auto result = dp::deserialize<dp::Mode::WITH_VERSION, Transaction<T>>(data, size);
                return dp::Result<Transaction<T>, dp::Error>::ok(std::move(result));
            } catch (const std::exception &e) {
                return dp::Result<Transaction<T>, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
            }
        }
    };

} // namespace blockit
