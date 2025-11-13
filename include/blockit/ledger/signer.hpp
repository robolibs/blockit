#pragma once

#include <algorithm>
#include <fstream>
#include <lockey/lockey.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace blockit::ledger {

    inline std::vector<unsigned char> stringToVector(const std::string &str) { return {str.begin(), str.end()}; }
    inline std::string vectorToString(const std::vector<unsigned char> &vec) { return {vec.begin(), vec.end()}; }

    inline std::string base64Encode(const std::vector<unsigned char> &data) {
        const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string encoded;
        for (size_t i = 0; i < data.size(); i += 3) {
            uint32_t temp = 0;
            for (size_t j = 0; j < 3; ++j) {
                temp <<= 8;
                if (i + j < data.size())
                    temp |= data[i + j];
            }
            for (int k = 3; k >= 0; --k)
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
            uint32_t temp = 0;
            int validChars = 0;
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
        std::vector<uint8_t> public_key;
        lockey::Lockey::Algorithm algorithm;
    };

    inline EVP_PKEY *loadPublicKeyFromPEM(const std::string &pemPublic) {
        EVP_PKEY *key = new EVP_PKEY();
        key->algorithm = lockey::Lockey::Algorithm::Ed25519;
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
                key->public_key = lockey::Lockey::from_hex(hexString);
            } catch (...) {
                key->public_key.clear();
            }
        }
        return key;
    }

    inline bool verify(EVP_PKEY *pubkey, const std::string &data, const std::vector<unsigned char> &signature) {
        try {
            if (pubkey->public_key.empty())
                return false;
            lockey::Lockey crypto(pubkey->algorithm);
            std::vector<uint8_t> dataVec(data.begin(), data.end());
            std::vector<uint8_t> sigVec(signature.begin(), signature.end());
            auto result = crypto.verify(dataVec, sigVec, pubkey->public_key);
            return result.success;
        } catch (...) {
            return false;
        }
    }

    inline bool verify(const std::string &pemPublic, const std::string &data,
                       const std::vector<unsigned char> &signature) {
        EVP_PKEY *pubkey = loadPublicKeyFromPEM(pemPublic);
        bool result = verify(pubkey, data, signature);
        delete pubkey;
        return result;
    }

    class Crypto {
      public:
        inline Crypto(const std::string &keyFile)
            : algorithm_(lockey::Lockey::Algorithm::Ed25519), crypto_(algorithm_) {
            keypair_ = crypto_.generate_keypair();
            if (!keypair_.private_key.empty())
                hasKeypair_ = true;
        }
        inline std::vector<unsigned char> sign(const std::string &data) {
            if (!hasKeypair_)
                throw std::runtime_error("No private key available for signing");
            std::vector<uint8_t> dataVec(data.begin(), data.end());
            auto result = crypto_.sign(dataVec, keypair_.private_key);
            if (!result.success)
                throw std::runtime_error("Signing failed: " + result.error_message);
            return std::vector<unsigned char>(result.data.begin(), result.data.end());
        }
        inline std::vector<unsigned char> decrypt(const std::vector<unsigned char> &ciphertext) {
            if (!hasKeypair_)
                throw std::runtime_error("No private key available for decryption");
            std::vector<uint8_t> ciphertextVec(ciphertext.begin(), ciphertext.end());
            auto result = crypto_.decrypt_asymmetric(ciphertextVec, keypair_.private_key);
            if (!result.success)
                throw std::runtime_error("Decryption failed: " + result.error_message);
            return std::vector<unsigned char>(result.data.begin(), result.data.end());
        }
        inline std::string getPublicHalf() {
            if (!hasKeypair_)
                throw std::runtime_error("No keypair available");
            std::string hexKey = lockey::Lockey::to_hex(keypair_.public_key);
            std::string base64Key = base64Encode(std::vector<unsigned char>(hexKey.begin(), hexKey.end()));
            std::ostringstream pem;
            pem << "-----BEGIN PUBLIC KEY-----\n";
            for (size_t i = 0; i < base64Key.length(); i += 64)
                pem << base64Key.substr(i, 64) << "\n";
            pem << "-----END PUBLIC KEY-----\n";
            return pem.str();
        }
        inline std::vector<uint8_t> getPublicKeyRaw() {
            if (!hasKeypair_)
                throw std::runtime_error("No keypair available");
            return keypair_.public_key;
        }

      private:
        lockey::Lockey::Algorithm algorithm_;
        lockey::Lockey crypto_;
        lockey::Lockey::KeyPair keypair_;
        bool hasKeypair_ = false;
    };

} // namespace blockit::ledger
