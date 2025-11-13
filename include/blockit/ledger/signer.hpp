#pragma once

#include <algorithm>
#include <fstream>
#include <lockey/lockey.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace blockit::ledger {

    std::vector<unsigned char> stringToVector(const std::string &str);
    std::string vectorToString(const std::vector<unsigned char> &vec);

    std::string base64Encode(const std::vector<unsigned char> &data);
    std::vector<unsigned char> base64Decode(const std::string &encoded);

    struct EVP_PKEY {
        std::vector<uint8_t> public_key;
        lockey::Lockey::Algorithm algorithm;
    };

    EVP_PKEY *loadPublicKeyFromPEM(const std::string &pemPublic);

    bool verify(EVP_PKEY *pubkey, const std::string &data, const std::vector<unsigned char> &signature);

    bool verify(const std::string &pemPublic, const std::string &data, const std::vector<unsigned char> &signature);

    class Crypto {
      public:
        Crypto(const std::string &keyFile);
        std::vector<unsigned char> sign(const std::string &data);
        std::vector<unsigned char> decrypt(const std::vector<unsigned char> &ciphertext);
        std::string getPublicHalf();
        std::vector<uint8_t> getPublicKeyRaw();

      private:
        lockey::Lockey::Algorithm algorithm_;
        lockey::Lockey crypto_;
        lockey::Lockey::KeyPair keypair_;
        bool hasKeypair_ = false;
    };

} // namespace blockit::ledger
