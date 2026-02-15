#include "best_hash.h"
#include <cstring>
#include <stdexcept>

// 静态成员变量定义
uint64_t SecureHash::DYNAMIC_SALT[8] = { 0 };
bool SecureHash::salt_initialized = false;
std::mutex SecureHash::salt_mutex;

// 盐值初始化函数实现
void SecureHash::initializeSalt() {
    std::lock_guard<std::mutex> lock(salt_mutex);
    if (salt_initialized) return;

    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::mt19937_64 gen(static_cast<uint64_t>(seed) ^
        std::random_device{}() ^
        (reinterpret_cast<uintptr_t>(&seed)));

    for (int i = 0; i < 8; ++i) {
        DYNAMIC_SALT[i] = gen();
    }
    salt_initialized = true;
}

// 返回原始字节形式的哈希（截断或扩展到 out_len）

// 返回原始字节形式的哈希（截断或扩展到 out_len）
std::vector<uint8_t> SecureHash::hashBytes(const std::vector<uint8_t>& data, size_t out_len) {
    uint64_t state[STATE_SIZE] = {0};
    spongeAbsorb(state, reinterpret_cast<const uint8_t*>(getDynamicSalt()), 8 * sizeof(uint64_t));

    uint64_t data_len = static_cast<uint64_t>(data.size());
    spongeAbsorb(state, reinterpret_cast<const uint8_t*>(&data_len), sizeof(data_len));
    spongeAbsorb(state, data.data(), data.size());

    keccakF1600(state);

    size_t full_out = ((out_len + 7) / 8) * 8; // round up to 8
    std::vector<uint8_t> out(full_out);
    spongeSqueeze(state, out.data(), full_out);

    std::vector<uint8_t> result(out.begin(), out.begin() + out_len);
    secureMemZero(state, sizeof(state));
    secureMemZero(out.data(), out.size());
    return result;
}

// 简单 HMAC: key XOR ipad/opad + hash
std::vector<uint8_t> SecureHash::hmac(const std::vector<uint8_t>& key, const std::vector<uint8_t>& message, size_t out_len) {
    const size_t block_size = 136; // sponge rate used as block
    std::vector<uint8_t> K(block_size, 0);
    if (!key.empty()) {
        for (size_t i = 0; i < std::min(K.size(), key.size()); ++i) K[i] = key[i];
    }

    std::vector<uint8_t> ipad(block_size), opad(block_size);
    for (size_t i = 0; i < block_size; ++i) {
        ipad[i] = K[i] ^ 0x36;
        opad[i] = K[i] ^ 0x5c;
    }

    // inner = H(ipad || message)
    std::vector<uint8_t> inner_in;
    inner_in.reserve(ipad.size() + message.size());
    inner_in.insert(inner_in.end(), ipad.begin(), ipad.end());
    inner_in.insert(inner_in.end(), message.begin(), message.end());
    std::vector<uint8_t> inner_hash = hashBytes(inner_in, out_len);

    // outer = H(opad || inner)
    std::vector<uint8_t> outer_in;
    outer_in.reserve(opad.size() + inner_hash.size());
    outer_in.insert(outer_in.end(), opad.begin(), opad.end());
    outer_in.insert(outer_in.end(), inner_hash.begin(), inner_hash.end());
    std::vector<uint8_t> mac = hashBytes(outer_in, out_len);

    secureMemZero(K.data(), K.size());
    secureMemZero(ipad.data(), ipad.size());
    secureMemZero(opad.data(), opad.size());
    secureMemZero(inner_hash.data(), inner_hash.size());

    return mac;
}

// 获取动态盐值函数实现
const uint64_t* SecureHash::getDynamicSalt() {
    if (!salt_initialized) {
        initializeSalt();
    }
    return DYNAMIC_SALT;
}

// 安全内存清零函数实现
void SecureHash::secureMemZero(void* ptr, size_t len) {
    if (ptr == nullptr) return;
    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    while (len--) {
        *p++ = 0;
    }
}

// Keccak-f[1600]置换函数实现
void SecureHash::keccakF1600(uint64_t state[STATE_SIZE]) {
    for (int round = 0; round < ROUNDS; round++) {
        uint64_t C[5]{}, D[5]{};
        for (int i = 0; i < 5; i++) {
            C[i] = state[i] ^ state[i + 5] ^ state[i + 10] ^ state[i + 15] ^ state[i + 20];
        }
        for (int i = 0; i < 5; i++) {
            D[i] = C[(i + 4) % 5] ^ rotl64(C[(i + 1) % 5], 1);
        }
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                state[i + 5 * j] ^= D[i];
            }
        }

        uint64_t temp = state[1];
        int x = 1, y = 0;
        for (int i = 0; i < 24; i++) {
            int newX = y;
            int newY = (2 * x + 3 * y) % 5;
            uint64_t temp2 = state[newX + 5 * newY];
            state[newX + 5 * newY] = rotl64(temp, (i + 1) * (i + 2) / 2 % 64);
            temp = temp2;
            x = newX;
            y = newY;
        }

        for (int j = 0; j < 5; j++) {
            uint64_t temp[5]{};
            for (int i = 0; i < 5; i++) {
                temp[i] = state[i + 5 * j];
            }
            for (int i = 0; i < 5; i++) {
                state[i + 5 * j] = temp[i] ^ ((~temp[(i + 1) % 5]) & temp[(i + 2) % 5]);
            }
        }

        state[0] ^= RC[round];
    }
}

// 处理数据块函数实现
void SecureHash::processBlock(uint64_t state[STATE_SIZE], const uint8_t* block) {
    for (int i = 0; i < BLOCK_SIZE / 8; i++) {
        uint64_t value = 0;
        std::memcpy(&value, block + i * 8, sizeof(uint64_t));
        state[i] ^= value;
    }
    keccakF1600(state);
}

// 海绵结构吸收阶段实现
void SecureHash::spongeAbsorb(uint64_t state[STATE_SIZE], const uint8_t* data, size_t len) {
    size_t blockCount = len / BLOCK_SIZE;

    for (size_t i = 0; i < blockCount; i++) {
        processBlock(state, data + i * BLOCK_SIZE);
    }

    size_t remaining = len % BLOCK_SIZE;
    if (remaining > 0) {
        uint8_t lastBlock[BLOCK_SIZE] = { 0 };
        std::memcpy(lastBlock, data + blockCount * BLOCK_SIZE, remaining);
        lastBlock[remaining] = 0x06;
        lastBlock[BLOCK_SIZE - 1] |= 0x80;
        processBlock(state, lastBlock);
    }
}

// 海绵结构挤压阶段实现
void SecureHash::spongeSqueeze(uint64_t state[STATE_SIZE], uint8_t* output, size_t len) {
    size_t outputBlocks = len / 64;

    for (size_t i = 0; i < outputBlocks; i++) {
        std::memcpy(output + i * 64, state, 64);
        if (i < outputBlocks - 1) {
            keccakF1600(state);
        }
    }

    size_t remaining = len % 64;
    if (remaining > 0) {
        std::memcpy(output + outputBlocks * 64, state, remaining);
    }
}

// 增强密钥派生函数实现
void SecureHash::keyStrengthening(uint64_t state[STATE_SIZE], const std::vector<uint8_t>& data) {
    constexpr uint32_t ITERATIONS = 1000;

    for (uint32_t i = 0; i < ITERATIONS; ++i) {
        uint64_t strengthening_data[3] = {
            i,
            getDynamicSalt()[i % 8],
            static_cast<uint64_t>(data.size())
        };

        spongeAbsorb(state, reinterpret_cast<const uint8_t*>(strengthening_data),
            sizeof(strengthening_data));

        if (i % 10 == 0) {
            keccakF1600(state);
        }
    }
}