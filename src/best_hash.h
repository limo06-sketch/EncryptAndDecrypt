#ifndef BEST_HASH_H_
#define BEST_HASH_H_

#include <string>
#include <vector>
#include <cstdint>
#include <type_traits>
#include <sstream>
#include <functional>
#include <random>
#include <chrono>
#include <mutex>
#include "argon2id.h"

class SecureHash {
private:
    // 核心哈希算法常量（基于SHA3设计理念）
    static constexpr uint64_t RC[24] = {
        0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
        0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
        0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
        0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
        0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
        0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
        0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
        0x8000000000008080, 0x0000000080000001, 0x8000000080008008
    };

    static constexpr int ROUNDS = 24;
    static constexpr int STATE_SIZE = 25;
    static constexpr int BLOCK_SIZE = 136;
    static constexpr int OUTPUT_SIZE = 64;

    // 动态盐值 - 声明为静态成员变量
    static uint64_t DYNAMIC_SALT[8];
    static bool salt_initialized;
    static std::mutex salt_mutex;

    // 核心哈希函数声明
    static void keccakF1600(uint64_t state[STATE_SIZE]);
    static void spongeAbsorb(uint64_t state[STATE_SIZE], const uint8_t* data, size_t len);
    static void spongeSqueeze(uint64_t state[STATE_SIZE], uint8_t* output, size_t len);
    static void processBlock(uint64_t state[STATE_SIZE], const uint8_t* block);

    // 辅助函数 (保持内联)
    static inline uint64_t rotl64(uint64_t x, int y) {
        return (x << y) | (x >> (64 - y));
    }

    // 盐值管理函数声明
    static void initializeSalt();
    static const uint64_t* getDynamicSalt();

    // 安全内存清理声明
    static void secureMemZero(void* ptr, size_t len);

    // 增强密钥派生函数声明 - 已弃用，使用keyDerivationArgon2id代替
    static void keyStrengthening(uint64_t state[STATE_SIZE], const std::vector<uint8_t>& data);

    // 使用ARGON2ID的密钥派生函数
    static std::vector<uint8_t> keyDerivationArgon2id(
        const std::vector<uint8_t>& password,
        const std::vector<uint8_t>& salt,
        size_t output_length = 32,
        const Argon2id::Parameters& params = Argon2id::DEFAULTS
    );

    // 序列化模板 (在头文件中实现，因为它是模板)
    template<typename T>
    static std::vector<uint8_t> serialize(const T& data) {
        if constexpr (std::is_arithmetic_v<T>) {
            std::vector<uint8_t> result(sizeof(T));
            std::memcpy(result.data(), &data, sizeof(T));
            return result;
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return std::vector<uint8_t>(data.begin(), data.end());
        }
        else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            return data;
        }
        else {
            std::ostringstream oss;
            oss << data;
            std::string str = oss.str();
            return std::vector<uint8_t>(str.begin(), str.end());
        }
    }

public:
    // 主哈希函数模板声明
    template<typename T>
    static std::string hash(const T& data);

    // 返回原始字节形式的哈希（binary), 输出长度以字节为单位
    static std::vector<uint8_t> hashBytes(const std::vector<uint8_t>& data, size_t out_len = 32);

    // 基于 SecureHash 的 HMAC 实现（简化版本）
    static std::vector<uint8_t> hmac(const std::vector<uint8_t>& key, const std::vector<uint8_t>& message, size_t out_len = 32);

    // 验证函数声明
    template<typename T>
    static bool verify(const T& data, const std::string& hash);

    // 快速哈希版本声明
    template<typename T>
    static std::string hashFast(const T& data);
};

// 模板函数的实现也必须放在头文件中
template<typename T>
std::string SecureHash::hash(const T& data) {
    try {
        std::vector<uint8_t> serialized = serialize(data);
        if (serialized.empty()) {
            return std::string(128, '0');
        }

        uint64_t state[STATE_SIZE] = { 0 };
        spongeAbsorb(state, reinterpret_cast<const uint8_t*>(getDynamicSalt()), 8 * sizeof(uint64_t));

        uint64_t data_len = static_cast<uint64_t>(serialized.size());
        spongeAbsorb(state, reinterpret_cast<const uint8_t*>(&data_len), sizeof(data_len));
        spongeAbsorb(state, serialized.data(), serialized.size());

        keyStrengthening(state, serialized);
        keccakF1600(state);

        uint8_t hashResult[OUTPUT_SIZE] = { 0 };
        spongeSqueeze(state, hashResult, OUTPUT_SIZE);

        std::string result;
        result.reserve(OUTPUT_SIZE * 2);
        static const char hexChars[] = "0123456789abcdef";

        for (int i = 0; i < OUTPUT_SIZE; i++) {
            result.push_back(hexChars[(hashResult[i] >> 4) & 0x0F]);
            result.push_back(hexChars[hashResult[i] & 0x0F]);
        }

        secureMemZero(state, sizeof(state));
        secureMemZero(hashResult, sizeof(hashResult));
        return result;

    }
    catch (const std::exception& e) {
        throw std::runtime_error("SecureHash error: " + std::string(e.what()));
    }
}

template<typename T>
bool SecureHash::verify(const T& data, const std::string& hash) {
    std::string computedHash = SecureHash::hash(data);
    if (computedHash.length() != hash.length()) {
        return false;
    }

    unsigned char diff = 0;
    for (size_t i = 0; i < computedHash.length(); i++) {
        diff |= computedHash[i] ^ hash[i];
    }
    return diff == 0;
}

template<typename T>
std::string SecureHash::hashFast(const T& data) {
    try {
        std::vector<uint8_t> serialized = serialize(data);
        if (serialized.empty()) {
            return std::string(128, '0');
        }

        uint64_t state[STATE_SIZE] = { 0 };
        spongeAbsorb(state, reinterpret_cast<const uint8_t*>(getDynamicSalt()), 8 * sizeof(uint64_t));

        uint64_t data_len = static_cast<uint64_t>(serialized.size());
        spongeAbsorb(state, reinterpret_cast<const uint8_t*>(&data_len), sizeof(data_len));
        spongeAbsorb(state, serialized.data(), serialized.size());

        keccakF1600(state);

        uint8_t hashResult[OUTPUT_SIZE] = { 0 };
        spongeSqueeze(state, hashResult, OUTPUT_SIZE);

        std::string result;
        result.reserve(OUTPUT_SIZE * 2);
        static const char hexChars[] = "0123456789abcdef";

        for (int i = 0; i < OUTPUT_SIZE; i++) {
            result.push_back(hexChars[(hashResult[i] >> 4) & 0x0F]);
            result.push_back(hexChars[hashResult[i] & 0x0F]);
        }

        secureMemZero(state, sizeof(state));
        secureMemZero(hashResult, sizeof(hashResult));
        return result;

    }
    catch (...) {
        return std::string(128, '0');
    }
}

#endif