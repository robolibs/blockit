#pragma once

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <keylock/keylock.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <blockit/common/error.hpp>

namespace blockit::ledger {

    inline std::vector<unsigned char> stringToVector(const std::string &str) { return {str.begin(), str.end()}; }

    inline std::string vectorToString(const std::vector<unsigned char> &vec) { return {vec.begin(), vec.end()}; }

    inline std::string base64Encode(const std::vector<unsigned char> &data) {
        const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string encoded;
        for (size_t i = 0; i < data.size(); i += 3) {
            dp::u32 temp = 0;
            for (size_t j = 0; j < 3; ++j) {
                temp <<= 8;
                if (i + j < data.size())
                    temp |= data[i + j];
            }
            for (dp::i32 k = 3; k >= 0; --k)
                encoded += chars[(temp >> (6 * k)) & 0x3F];
        }
        size_t pad = data.size() % 3;
        if (pad)
            for (size_t i = 0; i < 3 - pad; ++i)
                encoded[encoded.length() - 1 - i] = '=';
        return encoded;
    }

    inline std::vector<unsigned char> base64Decode(const std::string &encoded) {
        const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::vector<unsigned char> decoded;
        std::string cleanInput = encoded;
        while (!cleanInput.empty() && cleanInput.back() == '=')
            cleanInput.pop_back();
        for (size_t i = 0; i < cleanInput.size(); i += 4) {
            dp::u32 temp = 0;
            dp::i32 validChars = 0;
            for (size_t j = 0; j < 4 && i + j < cleanInput.size(); ++j) {
                size_t pos = chars.find(cleanInput[i + j]);
                if (pos != std::string::npos) {
                    temp |= pos << (6 * (3 - j));
                    validChars++;
                }
            }
            if (validChars >= 2)
                decoded.push_back((temp >> 16) & 0xFF);
            if (validChars >= 3)
                decoded.push_back((temp >> 8) & 0xFF);
            if (validChars >= 4)
                decoded.push_back(temp & 0xFF);
        }
        return decoded;
    }

    struct EVP_PKEY {
        std::vector<dp::u8> public_key;
        keylock::Algorithm algorithm;
    };

    inline dp::Result<EVP_PKEY *, dp::Error> loadPublicKeyFromPEM(const std::string &pemPublic) {
        EVP_PKEY *key = new EVP_PKEY();
        key->algorithm = keylock::Algorithm::Ed25519;
        std::string base64Data;
        std::istringstream iss(pemPublic);
        std::string line;
        bool inKey = false;
        while (std::getline(iss, line)) {
            if (line.find("BEGIN") != std::string::npos) {
                inKey = true;
                continue;
            }
            if (line.find("END") != std::string::npos) {
                break;
            }
            if (inKey && !line.empty())
                base64Data += line;
        }
        if (!base64Data.empty()) {
            try {
                auto hexStringBytes = base64Decode(base64Data);
                std::string hexString(hexStringBytes.begin(), hexStringBytes.end());
                key->public_key = keylock::keylock::from_hex(hexString);
            } catch (...) {
                delete key;
                return dp::Result<EVP_PKEY *, dp::Error>::err(deserialization_failed("Failed to decode public key"));
            }
        }
        if (key->public_key.empty()) {
            delete key;
            return dp::Result<EVP_PKEY *, dp::Error>::err(dp::Error::invalid_argument("Empty or invalid public key"));
        }
        return dp::Result<EVP_PKEY *, dp::Error>::ok(key);
    }

    inline dp::Result<bool, dp::Error> verify(EVP_PKEY *pubkey, const std::string &data,
                                              const std::vector<unsigned char> &signature) {
        try {
            if (!pubkey || pubkey->public_key.empty()) {
                return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("Invalid or empty public key"));
            }
            keylock::keylock crypto(pubkey->algorithm);
            std::vector<dp::u8> dataVec(data.begin(), data.end());
            std::vector<dp::u8> sigVec(signature.begin(), signature.end());
            auto result = crypto.verify(dataVec, sigVec, pubkey->public_key);
            return dp::Result<bool, dp::Error>::ok(result.success);
        } catch (const std::exception &e) {
            return dp::Result<bool, dp::Error>::err(verification_failed(dp::String(e.what())));
        }
    }

    inline dp::Result<bool, dp::Error> verify(const std::string &pemPublic, const std::string &data,
                                              const std::vector<unsigned char> &signature) {
        auto key_result = loadPublicKeyFromPEM(pemPublic);
        if (!key_result.is_ok()) {
            return dp::Result<bool, dp::Error>::err(key_result.error());
        }
        EVP_PKEY *pubkey = key_result.value();
        auto result = verify(pubkey, data, signature);
        delete pubkey;
        return result;
    }

    class Crypto {
      public:
        inline Crypto(const std::string &keyFile) : algorithm_(keylock::Algorithm::Ed25519), crypto_(algorithm_) {
            keypair_ = crypto_.generate_keypair();
            if (!keypair_.private_key.empty())
                hasKeypair_ = true;
            (void)keyFile; // API compatibility - keyFile ignored, generates new keypair
        }

        inline dp::Result<std::vector<unsigned char>, dp::Error> sign(const std::string &data) {
            if (!hasKeypair_) {
                return dp::Result<std::vector<unsigned char>, dp::Error>::err(
                    signing_failed("No private key available for signing"));
            }
            try {
                std::vector<dp::u8> dataVec(data.begin(), data.end());
                auto result = crypto_.sign(dataVec, keypair_.private_key);
                if (!result.success) {
                    return dp::Result<std::vector<unsigned char>, dp::Error>::err(
                        signing_failed(dp::String(result.error_message.c_str())));
                }
                return dp::Result<std::vector<unsigned char>, dp::Error>::ok(
                    std::vector<unsigned char>(result.data.begin(), result.data.end()));
            } catch (const std::exception &e) {
                return dp::Result<std::vector<unsigned char>, dp::Error>::err(signing_failed(dp::String(e.what())));
            }
        }

        inline dp::Result<std::vector<unsigned char>, dp::Error> decrypt(const std::vector<unsigned char> &ciphertext) {
            if (!hasKeypair_) {
                return dp::Result<std::vector<unsigned char>, dp::Error>::err(
                    dp::Error::permission_denied("No private key available for decryption"));
            }
            try {
                std::vector<dp::u8> ciphertextVec(ciphertext.begin(), ciphertext.end());
                auto result = crypto_.decrypt_asymmetric(ciphertextVec, keypair_.private_key);
                if (!result.success) {
                    return dp::Result<std::vector<unsigned char>, dp::Error>::err(
                        dp::Error::io_error(dp::String(result.error_message.c_str())));
                }
                return dp::Result<std::vector<unsigned char>, dp::Error>::ok(
                    std::vector<unsigned char>(result.data.begin(), result.data.end()));
            } catch (const std::exception &e) {
                return dp::Result<std::vector<unsigned char>, dp::Error>::err(
                    dp::Error::io_error(dp::String(e.what())));
            }
        }

        inline dp::Result<std::string, dp::Error> getPublicHalf() {
            if (!hasKeypair_) {
                return dp::Result<std::string, dp::Error>::err(dp::Error::invalid_argument("No keypair available"));
            }
            try {
                std::string hexKey = keylock::keylock::to_hex(keypair_.public_key);
                std::string base64Key = base64Encode(std::vector<unsigned char>(hexKey.begin(), hexKey.end()));
                std::ostringstream pem;
                pem << "-----BEGIN PUBLIC KEY-----\n";
                for (size_t i = 0; i < base64Key.length(); i += 64)
                    pem << base64Key.substr(i, 64) << "\n";
                pem << "-----END PUBLIC KEY-----\n";
                return dp::Result<std::string, dp::Error>::ok(pem.str());
            } catch (const std::exception &e) {
                return dp::Result<std::string, dp::Error>::err(serialization_failed(dp::String(e.what())));
            }
        }

        inline dp::Result<std::vector<dp::u8>, dp::Error> getPublicKeyRaw() {
            if (!hasKeypair_) {
                return dp::Result<std::vector<dp::u8>, dp::Error>::err(
                    dp::Error::invalid_argument("No keypair available"));
            }
            return dp::Result<std::vector<dp::u8>, dp::Error>::ok(keypair_.public_key);
        }

        inline bool hasKeypair() const { return hasKeypair_; }

      private:
        keylock::Algorithm algorithm_;
        keylock::keylock crypto_;
        keylock::KeyPair keypair_;
        bool hasKeypair_ = false;
    };

} // namespace blockit::ledger
