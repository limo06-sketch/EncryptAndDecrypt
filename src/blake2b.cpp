#include "blake2b.h"
#include <cstring>
#include <stdexcept>

// Blake2b初始化向量定义已移到头文件

// 右旋转内联实现
static inline uint64_t blake2b_rotr64(uint64_t x, int shift) {
    return (x >> shift) | (x << (64 - shift));
}

// 加载64位大端整数
inline uint64_t Blake2b::load64(const void* src) {
    const uint8_t* p = static_cast<const uint8_t*>(src);
    return static_cast<uint64_t>(p[0])       |
           (static_cast<uint64_t>(p[1]) << 8) |
           (static_cast<uint64_t>(p[2]) << 16) |
           (static_cast<uint64_t>(p[3]) << 24) |
           (static_cast<uint64_t>(p[4]) << 32) |
           (static_cast<uint64_t>(p[5]) << 40) |
           (static_cast<uint64_t>(p[6]) << 48) |
           (static_cast<uint64_t>(p[7]) << 56);
}

// 存储64位大端整数
inline void Blake2b::store64(void* dst, uint64_t v) {
    uint8_t* p = static_cast<uint8_t*>(dst);
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
    p[4] = static_cast<uint8_t>(v >> 32);
    p[5] = static_cast<uint8_t>(v >> 40);
    p[6] = static_cast<uint8_t>(v >> 48);
    p[7] = static_cast<uint8_t>(v >> 56);
}

// 右旋转
inline uint64_t Blake2b::rotr64(uint64_t x, int shift) {
    return blake2b_rotr64(x, shift);
}

// 初始化Blake2b状态
void Blake2b::init(State* state, size_t outlen, const void* key, size_t keylen) {
    if (outlen < 1 || outlen > MAX_OUTPUT_SIZE) {
        throw std::invalid_argument("Blake2b: invalid output length");
    }
    if (keylen > MAX_KEY_SIZE) {
        throw std::invalid_argument("Blake2b: key too long");
    }

    // 复制初始值
    std::memcpy(state->h, IV, sizeof(IV));

    // 参数块：处理outlen和keylen
    state->h[0] ^= 0x01010000 ^ (static_cast<uint64_t>(keylen) << 8) ^ outlen;

    state->t[0] = 0;
    state->t[1] = 0;
    state->f[0] = 0;
    state->f[1] = 0;
    state->buflen = 0;
    state->outlen = outlen;

    // 如果有密钥，先处理密钥块
    if (keylen > 0) {
        uint8_t block[BLOCK_SIZE] = { 0 };
        std::memcpy(block, key, keylen);
        update(state, block, BLOCK_SIZE);
    }
}

// Blake2b的G函数
static inline void g(uint64_t* v, size_t a, size_t b, size_t c, size_t d, uint64_t x, uint64_t y) {
    v[a] = v[a] + v[b] + x;
    v[d] = blake2b_rotr64(v[d] ^ v[a], 32);
    v[c] = v[c] + v[d];
    v[b] = blake2b_rotr64(v[b] ^ v[c], 24);
    v[a] = v[a] + v[b] + y;
    v[d] = blake2b_rotr64(v[d] ^ v[a], 16);
    v[c] = v[c] + v[d];
    v[b] = blake2b_rotr64(v[b] ^ v[c], 63);
}

// 压缩函数
void Blake2b::compress(State* state, const uint8_t* block) {
    uint64_t v[16];
    uint64_t m[16];

    // 初始化工作向量
    for (int i = 0; i < 8; i++) {
        v[i] = state->h[i];
    }
    std::memcpy(v + 8, IV, 64);

    // 加载消息块
    for (int i = 0; i < 16; i++) {
        m[i] = load64(block + i * 8);
    }

    // 执行12轮混合
    for (int round = 0; round < 12; round++) {
        const uint64_t* s = SIGMA[round];

        // 列混合
        g(v, 0, 4,  8, 12, m[s[ 0]], m[s[ 1]]);
        g(v, 1, 5,  9, 13, m[s[ 2]], m[s[ 3]]);
        g(v, 2, 6, 10, 14, m[s[ 4]], m[s[ 5]]);
        g(v, 3, 7, 11, 15, m[s[ 6]], m[s[ 7]]);

        // 对角混合
        g(v, 0, 5, 10, 15, m[s[ 8]], m[s[ 9]]);
        g(v, 1, 6, 11, 12, m[s[10]], m[s[11]]);
        g(v, 2, 7,  8, 13, m[s[12]], m[s[13]]);
        g(v, 3, 4,  9, 14, m[s[14]], m[s[15]]);
    }

    // 更新哈希值
    for (int i = 0; i < 8; i++) {
        state->h[i] ^= v[i] ^ v[i + 8];
    }

    // 清零消息块
    std::memset(m, 0, sizeof(m));
}

// 更新
void Blake2b::update(State* state, const void* input, size_t inlen) {
    const uint8_t* in = static_cast<const uint8_t*>(input);

    while (inlen > 0) {
        size_t left = BLOCK_SIZE - state->buflen;
        size_t fill = (inlen < left) ? inlen : left;

        std::memcpy(state->buf + state->buflen, in, fill);
        state->buflen += fill;
        in += fill;
        inlen -= fill;

        if (state->buflen == BLOCK_SIZE) {
            // 更新计数器
            state->t[0] += BLOCK_SIZE;
            if (state->t[0] < BLOCK_SIZE) {
                state->t[1]++;
            }

            compress(state, state->buf);
            state->buflen = 0;
        }
    }
}

// 最终化
void Blake2b::final(State* state, void* out) {
    // 更新最后的计数
    state->t[0] += state->buflen;
    if (state->t[0] < static_cast<uint64_t>(state->buflen)) {
        state->t[1]++;
    }

    // 设置最后一块标志
    state->f[0] = ~state->f[0];

    // 填充最后一块
    std::memset(state->buf + state->buflen, 0, BLOCK_SIZE - state->buflen);

    compress(state, state->buf);

    // 输出哈希值
    for (size_t i = 0; i < state->outlen; i++) {
        static_cast<uint8_t*>(out)[i] = (state->h[i >> 3] >> (8 * (i & 7))) & 0xFF;
    }
}

// 主哈希函数
std::vector<uint8_t> Blake2b::hash(
    const void* input,
    size_t input_len,
    size_t output_len,
    const void* key,
    size_t key_len) {

    if (output_len < 1 || output_len > MAX_OUTPUT_SIZE) {
        throw std::invalid_argument("Blake2b: invalid output length");
    }

    State state;
    init(&state, output_len, key, key_len);

    if (input_len > 0) {
        update(&state, input, input_len);
    }

    std::vector<uint8_t> out(output_len);
    final(&state, out.data());

    return out;
}
