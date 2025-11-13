#pragma once

#include "lockey/lockey.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace chain {

    // Helper functions for string/vector conversion
    inline std::vector<unsigned char> stringToVector(const std::string &str) {
        return std::vector<unsigned char>(str.begin(), str.end());
    }

    inline std::string vectorToString(const std::vector<unsigned char> &vec) {
        return std::string(vec.begin(), vec.end());
    }

    // Base64 encoding implementation (compatible with OpenSSL BIO)
    inline std::string base64Encode(const std::vector<unsigned char> &data) {
        const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string encoded;

        for (size_t i = 0; i < data.size(); i += 3) {
            uint32_t temp = 0;
            for (size_t j = 0; j < 3; ++j) {
                temp <<= 8;
                if (i + j < data.size()) {
                    temp |= data[i + j];
                }
            }

            for (int k = 3; k >= 0; --k) {
                encoded += chars[(temp >> (6 * k)) & 0x3F];
            }
        }

        // Add padding
        size_t pad = data.size() % 3;
        if (pad) {
            for (size_t i = 0; i < 3 - pad; ++i) {
                encoded[encoded.length() - 1 - i] = '=';
            }
        }

        return encoded;
    }

    // Base64 decoding implementation
    inline std::vector<unsigned char> base64Decode(const std::string &encoded) {
        const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::vector<unsigned char> decoded;

        std::string cleanInput = encoded;
        // Remove padding for processing
        while (!cleanInput.empty() && cleanInput.back() == '=') {
            cleanInput.pop_back();
        }

        for (size_t i = 0; i < cleanInput.size(); i += 4) {
            uint32_t temp = 0;
            int validChars = 0;

            for (size_t j = 0; j < 4 && i + j < cleanInput.size(); ++j) {
                size_t pos = chars.find(cleanInput[i + j]);
                if (pos != std::string::npos) {
                    temp |= pos << (6 * (3 - j));
                    validChars++;
                }
            }

            // Extract bytes based on valid characters
            if (validChars >= 2)
                decoded.push_back((temp >> 16) & 0xFF);
            if (validChars >= 3)
                decoded.push_back((temp >> 8) & 0xFF);
            if (validChars >= 4)
                decoded.push_back(temp & 0xFF);
        }

        return decoded;
    }

    // Simple EVP_PKEY placeholder that stores actual Lockey public key
    struct EVP_PKEY {
        std::vector<uint8_t> public_key;
        lockey::Lockey::Algorithm algorithm;
    };

    // Load public key from PEM - extract the actual hex-encoded public key
    inline EVP_PKEY *loadPublicKeyFromPEM(const std::string &pemPublic) {
        EVP_PKEY *key = new EVP_PKEY();
        key->algorithm = lockey::Lockey::Algorithm::Ed25519;

        // Extract the base64 content between BEGIN and END
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
            if (inKey && !line.empty()) {
                base64Data += line;
            }
        }

        // Decode the base64 to get the hex string, then convert hex to binary
        if (!base64Data.empty()) {
            try {
                auto hexStringBytes = base64Decode(base64Data);
                std::string hexString(hexStringBytes.begin(), hexStringBytes.end());

                // Convert hex string to binary using Lockey's from_hex
                key->public_key = lockey::Lockey::from_hex(hexString);
            } catch (...) {
                // If parsing fails, create empty key vector
                key->public_key.clear();
            }
        }

        return key;
    }

    // Verify signature using the same crypto instance
    inline bool verify(EVP_PKEY *pubkey, const std::string &data, const std::vector<unsigned char> &signature) {
        try {
            if (pubkey->public_key.empty()) {
                return false;
            }

            lockey::Lockey crypto(pubkey->algorithm);
            std::vector<uint8_t> dataVec(data.begin(), data.end());
            std::vector<uint8_t> sigVec(signature.begin(), signature.end());

            auto result = crypto.verify(dataVec, sigVec, pubkey->public_key);
            return result.success;
        } catch (...) {
            return false;
        }
    }

    // Verify signature using PEM string directly (alternative interface)
    inline bool verify(const std::string &pemPublic, const std::string &data,
                       const std::vector<unsigned char> &signature) {
        EVP_PKEY *pubkey = loadPublicKeyFromPEM(pemPublic);
        bool result = verify(pubkey, data, signature);
        delete pubkey;
        return result;
    }

    // Encrypt with public key
    inline std::vector<unsigned char> encrypt(EVP_PKEY *pubkey, const std::string &plaintextStr) {
        if (pubkey->public_key.empty()) {
            throw std::runtime_error("Invalid public key for encryption");
        }

        lockey::Lockey crypto(pubkey->algorithm);
        std::vector<uint8_t> plaintext(plaintextStr.begin(), plaintextStr.end());

        auto result = crypto.encrypt_asymmetric(plaintext, pubkey->public_key);
        if (!result.success) {
            throw std::runtime_error("Encryption failed: " + result.error_message);
        }

        return std::vector<unsigned char>(result.data.begin(), result.data.end());
    }

    // Encrypt using PEM string directly (alternative interface)
    inline std::vector<unsigned char> encrypt(const std::string &pemPublic, const std::string &plaintextStr) {
        EVP_PKEY *pubkey = loadPublicKeyFromPEM(pemPublic);
        auto result = encrypt(pubkey, plaintextStr);
        delete pubkey;
        return result;
    }

    // Crypto class for private key operations
    class Crypto {
      public:
        inline Crypto(const std::string &keyFile)
            : algorithm_(lockey::Lockey::Algorithm::Ed25519), crypto_(algorithm_) {
            // Generate a new keypair (in real implementation, would load from file)
            keypair_ = crypto_.generate_keypair();
            if (!keypair_.private_key.empty()) {
                hasKeypair_ = true;
            }
        }

        inline std::vector<unsigned char> sign(const std::string &data) {
            if (!hasKeypair_) {
                throw std::runtime_error("No private key available for signing");
            }

            std::vector<uint8_t> dataVec(data.begin(), data.end());
            auto result = crypto_.sign(dataVec, keypair_.private_key);

            if (!result.success) {
                throw std::runtime_error("Signing failed: " + result.error_message);
            }

            return std::vector<unsigned char>(result.data.begin(), result.data.end());
        }

        inline std::vector<unsigned char> decrypt(const std::vector<unsigned char> &ciphertext) {
            if (!hasKeypair_) {
                throw std::runtime_error("No private key available for decryption");
            }

            std::vector<uint8_t> ciphertextVec(ciphertext.begin(), ciphertext.end());
            auto result = crypto_.decrypt_asymmetric(ciphertextVec, keypair_.private_key);

            if (!result.success) {
                throw std::runtime_error("Decryption failed: " + result.error_message);
            }

            return std::vector<unsigned char>(result.data.begin(), result.data.end());
        }

        inline std::string getPublicHalf() {
            if (!hasKeypair_) {
                throw std::runtime_error("No keypair available");
            }

            // Create a PEM format that contains the hex-encoded public key as base64
            std::string hexKey = lockey::Lockey::to_hex(keypair_.public_key);
            std::string base64Key = base64Encode(std::vector<unsigned char>(hexKey.begin(), hexKey.end()));

            std::ostringstream pem;
            pem << "-----BEGIN PUBLIC KEY-----\n";
            // Split base64 into 64-character lines
            for (size_t i = 0; i < base64Key.length(); i += 64) {
                pem << base64Key.substr(i, 64) << "\n";
            }
            pem << "-----END PUBLIC KEY-----\n";

            return pem.str();
        }

        // Get the raw public key for direct use (Lockey-style)
        inline std::vector<uint8_t> getPublicKeyRaw() {
            if (!hasKeypair_) {
                throw std::runtime_error("No keypair available");
            }
            return keypair_.public_key;
        }

      private:
        lockey::Lockey::Algorithm algorithm_;
        lockey::Lockey crypto_;
        lockey::Lockey::KeyPair keypair_;
        bool hasKeypair_ = false;
    };

} // namespace chain
