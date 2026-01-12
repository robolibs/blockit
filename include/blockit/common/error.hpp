#pragma once

#include <datapod/datapod.hpp>
#include <shared_mutex>

namespace blockit {

    // ===========================================
    // Blockit-specific error codes (100+)
    // ===========================================

    constexpr dp::u32 ERR_CHAIN_EMPTY = 100;
    constexpr dp::u32 ERR_INVALID_BLOCK = 101;
    constexpr dp::u32 ERR_DUPLICATE_TX = 102;
    constexpr dp::u32 ERR_UNAUTHORIZED = 103;
    constexpr dp::u32 ERR_HASH_FAILED = 104;
    constexpr dp::u32 ERR_NOT_INITIALIZED = 105;
    constexpr dp::u32 ERR_SIGNING_FAILED = 106;
    constexpr dp::u32 ERR_VERIFICATION_FAILED = 107;
    constexpr dp::u32 ERR_INVALID_TRANSACTION = 108;
    constexpr dp::u32 ERR_CAPABILITY_MISSING = 109;
    constexpr dp::u32 ERR_MERKLE_EMPTY = 110;
    constexpr dp::u32 ERR_SERIALIZATION_FAILED = 111;
    constexpr dp::u32 ERR_DESERIALIZATION_FAILED = 112;

    // ===========================================
    // Error factory functions
    // ===========================================

    inline dp::Error chain_empty(const dp::String &msg = "Chain is empty") { return dp::Error{ERR_CHAIN_EMPTY, msg}; }

    inline dp::Error invalid_block(const dp::String &msg = "Invalid block") {
        return dp::Error{ERR_INVALID_BLOCK, msg};
    }

    inline dp::Error duplicate_tx(const dp::String &msg = "Duplicate transaction") {
        return dp::Error{ERR_DUPLICATE_TX, msg};
    }

    inline dp::Error unauthorized(const dp::String &msg = "Unauthorized participant") {
        return dp::Error{ERR_UNAUTHORIZED, msg};
    }

    inline dp::Error hash_failed(const dp::String &msg = "Hash computation failed") {
        return dp::Error{ERR_HASH_FAILED, msg};
    }

    inline dp::Error not_initialized(const dp::String &msg = "Not initialized") {
        return dp::Error{ERR_NOT_INITIALIZED, msg};
    }

    inline dp::Error signing_failed(const dp::String &msg = "Signing operation failed") {
        return dp::Error{ERR_SIGNING_FAILED, msg};
    }

    inline dp::Error verification_failed(const dp::String &msg = "Verification failed") {
        return dp::Error{ERR_VERIFICATION_FAILED, msg};
    }

    inline dp::Error invalid_transaction(const dp::String &msg = "Invalid transaction") {
        return dp::Error{ERR_INVALID_TRANSACTION, msg};
    }

    inline dp::Error capability_missing(const dp::String &msg = "Missing required capability") {
        return dp::Error{ERR_CAPABILITY_MISSING, msg};
    }

    inline dp::Error merkle_empty(const dp::String &msg = "Merkle tree is empty") {
        return dp::Error{ERR_MERKLE_EMPTY, msg};
    }

    inline dp::Error serialization_failed(const dp::String &msg = "Serialization failed") {
        return dp::Error{ERR_SERIALIZATION_FAILED, msg};
    }

    inline dp::Error deserialization_failed(const dp::String &msg = "Deserialization failed") {
        return dp::Error{ERR_DESERIALIZATION_FAILED, msg};
    }

} // namespace blockit
