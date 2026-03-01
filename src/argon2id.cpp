#include "argon2id.h"
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <cmath>

// ARGON2ID版本
static constexpr uint32_t ARGON2_VERSION = 0x13;

// 安全内存清零
void Argon2id::secureMemZero(void* ptr, size_t len) {
    if (ptr == nullptr) return;
    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    while (len--) {
        *p++ = 0;
    }
}

// 从字节数组加载64位小端整数
static inline uint64_t load64_le(const uint8_t* p) {
    return static_cast<uint64_t>(p[0]) |
           (static_cast<uint64_t>(p[1]) << 8) |
           (static_cast<uint64_t>(p[2]) << 16) |
           (static_cast<uint64_t>(p[3]) << 24) |
           (static_cast<uint64_t>(p[4]) << 32) |
           (static_cast<uint64_t>(p[5]) << 40) |
           (static_cast<uint64_t>(p[6]) << 48) |
           (static_cast<uint64_t>(p[7]) << 56);
}

// 存储64位小端整数
static inline void store64_le(uint8_t* p, uint64_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
    p[4] = static_cast<uint8_t>(v >> 32);
    p[5] = static_cast<uint8_t>(v >> 40);
    p[6] = static_cast<uint8_t>(v >> 48);
    p[7] = static_cast<uint8_t>(v >> 56);
}

// 初始化哈希
std::vector<uint8_t> Argon2id::initialHash(
    const std::vector<uint8_t>& password,
    const std::vector<uint8_t>& salt,
    const Parameters& params,
    const std::vector<uint8_t>& associated_data) {

    // H_init = Blake2b-512(params || password || salt || data)
    std::vector<uint8_t> H_init_input;
    H_init_input.reserve(256 + password.size() + salt.size() + associated_data.size());

    // 编码参数（32字节）
    auto append_u32_le = [&H_init_input](uint32_t val) {
        H_init_input.push_back(val & 0xFF);
        H_init_input.push_back((val >> 8) & 0xFF);
        H_init_input.push_back((val >> 16) & 0xFF);
        H_init_input.push_back((val >> 24) & 0xFF);
    };

    append_u32_le(params.parallelism);
    append_u32_le(params.output_length);
    append_u32_le(params.memory_cost);
    append_u32_le(params.time_cost);
    append_u32_le(ARGON2_VERSION);
    append_u32_le(2); // Type: Argon2id
    append_u32_le(static_cast<uint32_t>(password.size()));
    H_init_input.insert(H_init_input.end(), password.begin(), password.end());

    append_u32_le(static_cast<uint32_t>(salt.size()));
    H_init_input.insert(H_init_input.end(), salt.begin(), salt.end());

    append_u32_le(0); // secret size = 0
    append_u32_le(static_cast<uint32_t>(associated_data.size()));
    H_init_input.insert(H_init_input.end(), associated_data.begin(), associated_data.end());

    return Blake2b::hash(H_init_input.data(), H_init_input.size(), 64);
}

// 扩展初始哈希到内存块
static std::vector<uint8_t> hashWithCounter(
    const std::vector<uint8_t>& hash,
    uint32_t counter) {
    
    std::vector<uint8_t> input = hash;
    input.resize(hash.size() + 4);
    
    uint8_t* p = input.data() + hash.size();
    p[0] = counter & 0xFF;
    p[1] = (counter >> 8) & 0xFF;
    p[2] = (counter >> 16) & 0xFF;
    p[3] = (counter >> 24) & 0xFF;

    return Blake2b::hash(input.data(), input.size(), 64);
}

// 初始化内存矩阵的第一列
static void fillFirstBlocks(
    std::vector<std::vector<uint64_t>>& memory,
    uint32_t memory_blocks,
    uint32_t lane_count,
    const std::vector<uint8_t>& initial_hash) {

    uint32_t block_index = 0;

    // 为每条Lane生成初始块
    for (uint32_t lane = 0; lane < lane_count; lane++) {
        for (uint32_t block = 0; block < 2; block++) {
            std::vector<uint8_t> hash = hashWithCounter(initial_hash, lane * 2 + block);
            
            // 将hash作为两个64位块填入内存
            for (size_t i = 0; i < 8; i++) {
                memory[lane * 2 + block][i] = load64_le(hash.data() + i * 8);
            }
        }
    }
}

// 伪随机数生成函数（用于选择参考块）
static uint64_t indexAlpha(
    const std::vector<uint64_t>& prev_block,
    uint64_t pseudo_random) {

    uint32_t x = (pseudo_random >> 32) & 0xFFFFFFFFU;
    uint32_t y = pseudo_random & 0xFFFFFFFFU;

    return ((uint64_t)x * y) >> 32;
}

// 填充内存矩阵
void Argon2id::fillMemory(
    std::vector<std::vector<uint64_t>>& memory,
    uint32_t memory_blocks,
    uint32_t time_cost,
    uint32_t parallelism) {

    constexpr size_t BLOCK_SIZE_U64 = 128; // ARGON2_BLOCK_SIZE / 8

    // 简化的填充过程：使用多轮迭代
    for (uint32_t pass = 0; pass < time_cost; pass++) {
        for (uint32_t lane = 0; lane < parallelism; lane++) {
            for (uint32_t block = 1; block < memory_blocks; block++) {
                uint32_t prev_block = block - 1;

                // 直接内联fillBlock的操作
                if (prev_block >= memory.size() || block >= memory.size()) {
                    continue;
                }

                if (memory[prev_block].size() != BLOCK_SIZE_U64 || memory[block].size() != BLOCK_SIZE_U64) {
                    continue;
                }

                std::vector<uint64_t> block_tmp(BLOCK_SIZE_U64);

                // 异或混合：当前块 = 当前块 XOR 前一块
                for (size_t i = 0; i < BLOCK_SIZE_U64; i++) {
                    block_tmp[i] = memory[block][i] ^ memory[prev_block][i];
                }

                // Blake2b轮函数应用到块
                std::vector<uint8_t> block_bytes(BLOCK_SIZE_U64 * 8);
                for (size_t i = 0; i < BLOCK_SIZE_U64; i++) {
                    store64_le(block_bytes.data() + i * 8, block_tmp[i]);
                }

                auto hash_result = Blake2b::hash(block_bytes.data(), block_bytes.size(), 64);

                // 更新内存块
                for (size_t i = 0; i < std::min((size_t)BLOCK_SIZE_U64, hash_result.size() / 8); i++) {
                    memory[block][i] ^= load64_le(hash_result.data() + i * 8);
                }

                secureMemZero(block_tmp.data(), block_tmp.size() * 8);
            }
        }
    }
}

// 最终化哈希
std::vector<uint8_t> Argon2id::finalizeHash(
    const std::vector<std::vector<uint64_t>>& memory,
    uint32_t output_length) {

    if (memory.empty()) {
        throw std::invalid_argument("Memory matrix is empty");
    }

    // 取最后一块的内容
    std::vector<uint8_t> final_block;
    final_block.reserve(memory[0].size() * 8);

    for (uint64_t val : memory.back()) {
        final_block.resize(final_block.size() + 8);
        store64_le(final_block.data() + final_block.size() - 8, val);
    }

    // 使用Blake2b进行最终哈希
    auto result = Blake2b::hash(final_block.data(), final_block.size(), output_length);
    secureMemZero(final_block.data(), final_block.size());

    return result;
}

std::vector<uint8_t> Argon2id::derive(
    const std::vector<uint8_t>& password,
    const std::vector<uint8_t>& salt,
    const Parameters& params,
    const std::vector<uint8_t>& associated_data) {

    // 参数验证
    if (password.empty()) {
        throw std::invalid_argument("Password cannot be empty");
    }
    if (salt.size() < 8) {
        throw std::invalid_argument("Salt must be at least 8 bytes");
    }
    if (params.memory_cost < 8 || params.memory_cost > 4294967295UL) {
        throw std::invalid_argument("Invalid memory cost");
    }
    if (params.time_cost < 1) {
        throw std::invalid_argument("Invalid time cost");
    }
    if (params.parallelism < 1) {
        throw std::invalid_argument("Invalid parallelism");
    }
    if (params.output_length < 4 || params.output_length > 4294967295UL) {
        throw std::invalid_argument("Invalid output length");
    }

    // 初始化哈希
    std::vector<uint8_t> init_hash = initialHash(password, salt, params, associated_data);

    // 计算内存块数量
    uint32_t memory_blocks = std::max(8u, params.memory_cost);
    std::vector<std::vector<uint64_t>> memory(memory_blocks);

    // 初始化每个内存块（128 x 64位 = 1024字节）
    constexpr size_t BLOCK_SIZE_U64 = 128;
    for (uint32_t i = 0; i < memory_blocks; i++) {
        memory[i].resize(BLOCK_SIZE_U64, 0);
    }

    // 填充初始块
    fillFirstBlocks(memory, memory_blocks, params.parallelism, init_hash);

    // 填充内存矩阵
    fillMemory(memory, memory_blocks, params.time_cost, params.parallelism);

    // 最终化并返回结果
    std::vector<uint8_t> output = finalizeHash(memory, params.output_length);

    // 安全清零中间数据
    for (auto& block : memory) {
        secureMemZero(block.data(), block.size() * sizeof(uint64_t));
    }

    return output;
}

std::vector<uint8_t> Argon2id::deriveString(
    const std::string& password,
    const std::string& salt,
    uint32_t output_length) {

    std::vector<uint8_t> pwd(password.begin(), password.end());
    std::vector<uint8_t> slt(salt.begin(), salt.end());

    Parameters params = DEFAULTS;
    params.output_length = output_length;

    return derive(pwd, slt, params);
}
