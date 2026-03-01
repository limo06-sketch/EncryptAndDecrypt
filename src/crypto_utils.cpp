#include "crypto_utils.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>

constexpr const char CryptoUtils::BASE64_CHARS[];

std::string CryptoUtils::bytesToHex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    for (uint8_t byte : data) {
        oss << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(byte);
    }
    return oss.str();
}

std::vector<uint8_t> CryptoUtils::hexToBytes(const std::string& hex) {
    if (!isValidHex(hex)) {
        throw std::invalid_argument("Invalid hexadecimal string");
    }

    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);

    for (size_t i = 0; i < hex.size(); i += 2) {
        uint8_t high = hexCharToValue(hex[i]);
        uint8_t low = hexCharToValue(hex[i + 1]);
        result.push_back((high << 4) | low);
    }

    return result;
}

std::string CryptoUtils::bytesToBase64(const std::vector<uint8_t>& data) {
    std::string result;
    result.reserve((data.size() + 2) / 3 * 4);

    size_t i = 0;
    while (i < data.size()) {
        uint32_t buf = 0;
        size_t len = std::min(size_t(3), data.size() - i);

        for (size_t j = 0; j < len; j++) {
            buf = (buf << 8) | data[i + j];
        }

        buf <<= (3 - len) * 8;

        result.push_back(BASE64_CHARS[(buf >> 18) & 0x3F]);
        result.push_back(BASE64_CHARS[(buf >> 12) & 0x3F]);

        if (len > 1) {
            result.push_back(BASE64_CHARS[(buf >> 6) & 0x3F]);
        } else {
            result.push_back('=');
        }

        if (len > 2) {
            result.push_back(BASE64_CHARS[buf & 0x3F]);
        } else {
            result.push_back('=');
        }

        i += len;
    }

    return result;
}

std::vector<uint8_t> CryptoUtils::base64ToBytes(const std::string& base64) {
    if (!isValidBase64(base64)) {
        throw std::invalid_argument("Invalid base64 string");
    }

    std::vector<uint8_t> result;
    result.reserve(base64.size() * 3 / 4);

    size_t i = 0;
    while (i < base64.size()) {
        uint8_t c1 = base64CharToValue(base64[i++]);
        uint8_t c2 = base64CharToValue(base64[i++]);
        
        result.push_back((c1 << 2) | (c2 >> 4));

        if (i < base64.size() && base64[i] != '=') {
            uint8_t c3 = base64CharToValue(base64[i++]);
            result.push_back(((c2 & 0x0F) << 4) | (c3 >> 2));

            if (i < base64.size() && base64[i] != '=') {
                uint8_t c4 = base64CharToValue(base64[i++]);
                result.push_back(((c3 & 0x03) << 6) | c4);
            }
        }
    }

    return result;
}

bool CryptoUtils::constantTimeCompare(
    const std::vector<uint8_t>& a,
    const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) {
        return false;
    }

    uint8_t result = 0;
    for (size_t i = 0; i < a.size(); i++) {
        result |= a[i] ^ b[i];
    }

    return result == 0;
}

void CryptoUtils::secureMemZero(void* ptr, size_t len) {
    if (ptr == nullptr) {
        return;
    }

    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    while (len--) {
        *p++ = 0;
    }
}

std::string CryptoUtils::bytesToString(const std::vector<uint8_t>& data) {
    return std::string(data.begin(), data.end());
}

std::vector<uint8_t> CryptoUtils::stringToBytes(const std::string& str) {
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::string CryptoUtils::getPlatformInfo() {
    std::string info = "Platform: ";
    info += PLATFORM_NAME;

#if defined(BITS_64)
    info += " (64-bit)";
#else
    info += " (32-bit)";
#endif

#if defined(LITTLE_ENDIAN)
    info += " [little-endian]";
#endif

#if defined(COMPILER_MSVC)
    info += " MSVC";
#elif defined(COMPILER_CLANG)
    info += " Clang";
#elif defined(COMPILER_GCC)
    info += " GCC";
#endif

    return info;
}

bool CryptoUtils::isValidBase64(const std::string& str) {
    if (str.empty()) {
        return false;
    }

    if (str.size() % 4 != 0) {
        return false;
    }

    for (size_t i = 0; i < str.size(); i++) {
        char c = str[i];
        if (!std::isalnum(c) && c != '+' && c != '/' && c != '=') {
            return false;
        }
        // 检查'='只能出现在末尾
        if (c == '=' && i < str.size() - 2) {
            return false;
        }
    }

    return true;
}

bool CryptoUtils::isValidHex(const std::string& str) {
    if (str.empty() || str.size() % 2 != 0) {
        return false;
    }

    for (char c : str) {
        if (!std::isxdigit(c)) {
            return false;
        }
    }

    return true;
}

inline uint8_t CryptoUtils::base64CharToValue(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return 255;
}

inline uint8_t CryptoUtils::hexCharToValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 255;
}
