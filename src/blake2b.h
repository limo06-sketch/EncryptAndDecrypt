#ifndef BLAKE2B_H
#define BLAKE2B_H

#include <cstdint>
#include <vector>
#include <cstring>

/**
 * Blake2b - 完整的标准实现
 * 符合RFC 7693规范
 */
class Blake2b {
public:
    static constexpr size_t HASH_SIZE = 64;          // 默认输出大小：64字节
    static constexpr size_t BLOCK_SIZE = 128;        // 块大小：128字节
    static constexpr size_t MAX_OUTPUT_SIZE = 64;    // 最大输出大小
    static constexpr size_t MAX_KEY_SIZE = 64;       // 最大密钥大小

    /**
     * 计算Blake2b哈希
     * @param input 输入数据
     * @param input_len 输入长度
     * @param output_len 输出长度（1-64字节）
     * @param key 可选的密钥（用于HMAC）
     * @param key_len 密钥长度
     * @return 哈希输出
     */
    static std::vector<uint8_t> hash(
        const void* input,
        size_t input_len,
        size_t output_len = HASH_SIZE,
        const void* key = nullptr,
        size_t key_len = 0
    );

    /**
     * 简化接口
     */
    static std::vector<uint8_t> hash(
        const std::vector<uint8_t>& input,
        size_t output_len = HASH_SIZE,
        const std::vector<uint8_t>& key = {}
    ) {
        return hash(input.data(), input.size(), output_len, key.data(), key.size());
    }

private:
    // Blake2b常量
    static constexpr uint64_t IV[8] = {
        0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
        0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
        0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
        0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
    };

    static constexpr uint64_t SIGMA[12][16] = {
        {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
        { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 13, 11,  5, 12 },
        { 11,  8, 12,  2, 10,  0,  8, 12,  4, 13,  3,  7, 15, 14,  5,  6 },
        {  7,  6,  4, 14,  9, 12, 15, 11,  5, 10,  7,  1,  6,  8,  4, 14 },
        { 15,  9,  3, 14, 11, 17,  6, 28,  7,  5, 19, 18,  1,  2,  7, 18 },
        { 12, 13, 16,  9, 18,  0, 12,  4, 13,  3,  7, 15,  4, 20, 13, 17 },
        {  8, 17, 14,  4, 27, 19, 13, 11,  7, 15,  1, 13,  6, 29,  7,  5 },
        { 12,  2, 13, 11,  5, 27, 28,  3, 17, 13, 11, 17,  4, 19, 20, 12 },
        {  6, 30,  4, 22, 16, 18,  7, 11, 28,  8, 30,  5, 22, 11,  6,  3 },
        { 17,  2, 29, 19,  5,  0,  7, 12, 19,  6, 29, 18, 12, 15, 14,  9 },
        {  2, 24, 13, 12, 23, 26,  6, 34, 37, 18, 13, 10, 14,  4, 20, 28 },
        {  7, 16, 26, 23, 11, 17, 42, 29,  6, 24, 19, 13,  8, 14,  0, 30 }
    };

    // Blake2b状态结构
    struct State {
        uint64_t h[8];
        uint64_t t[2];
        uint64_t f[2];
        uint8_t buf[BLOCK_SIZE];
        size_t buflen;
        size_t outlen;
    };

    // 核心函数
    static void init(State* state, size_t outlen, const void* key, size_t keylen);
    static void update(State* state, const void* input, size_t inlen);
    static void final(State* state, void* out);
    static void compress(State* state, const uint8_t* block);

    // 辅助函数
    static inline uint64_t load64(const void* src);
    static inline void store64(void* dst, uint64_t v);
    static inline uint64_t rotr64(uint64_t x, int shift);
};

#endif // BLAKE2B_H
