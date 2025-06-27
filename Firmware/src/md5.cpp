#include "Md5.h"
#include "mbedtls/md5.h"
#include <sstream>
#include <iomanip>

String ToMD5(String& input) {
    std::string Input = std::string(input.c_str());
    return String(ToMD5(Input).c_str());
}

std::string ToMD5(const std::string& input) {
    unsigned char digest[16]; // MD5 produces a 16-byte hash

    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts_ret(&ctx);
    mbedtls_md5_update_ret(&ctx, (const unsigned char*)input.c_str(), input.size());
    mbedtls_md5_finish_ret(&ctx, digest);
    mbedtls_md5_free(&ctx);

    std::ostringstream oss;
    for (int i = 0; i < 16; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }

    return oss.str();
}