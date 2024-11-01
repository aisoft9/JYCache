#include "common.h"

#include <openssl/evp.h>

namespace HybridCache {

bool EnableLogging = true;

void split(const std::string& str, const char delim,
           std::vector<std::string>& items) {
    std::istringstream iss(str);
    std::string tmp;
    while (std::getline(iss, tmp, delim)) {
        if (!tmp.empty()) {
            items.emplace_back(std::move(tmp));
        }
    }
}

std::string base64(const unsigned char* input, size_t length) {
    static constexpr char base[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

    std::string result;
    result.reserve(((length + 3 - 1) / 3) * 4 + 1);

    unsigned char parts[4];
    size_t rpos;
    for(rpos = 0; rpos < length; rpos += 3){
        parts[0] = (input[rpos] & 0xfc) >> 2;
        parts[1] = ((input[rpos] & 0x03) << 4) | ((((rpos + 1) < length ? input[rpos + 1] : 0x00) & 0xf0) >> 4);
        parts[2] = (rpos + 1) < length ? (((input[rpos + 1] & 0x0f) << 2) | ((((rpos + 2) < length ? input[rpos + 2] : 0x00) & 0xc0) >> 6)) : 0x40;
        parts[3] = (rpos + 2) < length ? (input[rpos + 2] & 0x3f) : 0x40;

        result += base[parts[0]];
        result += base[parts[1]];
        result += base[parts[2]];
        result += base[parts[3]];
    }

    return result;
}

std::string md5(const std::string& str) {
    std::array<unsigned char, 16> binary;
    unsigned int digestlen = static_cast<unsigned int>(binary.size());

    const EVP_MD* md    = EVP_get_digestbyname("md5");
    EVP_MD_CTX*   mdctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(mdctx, md, nullptr);
    EVP_DigestUpdate(mdctx, str.c_str(), str.length());
    EVP_DigestFinal_ex(mdctx, binary.data(), &digestlen);
    EVP_MD_CTX_destroy(mdctx);

    return base64(binary.data(), binary.size());
}

}  // namespace HybridCache
