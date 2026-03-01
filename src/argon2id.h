#ifndef ARGON2ID_H
#define ARGON2ID_H

#include <vector>
#include <cstdint>
#include <string>
#include "blake2b.h"

class Argon2id {
public:
    // Argon2id参数结构体
    struct Parameters {
        uint32_t memory_cost;      // 内存成本（单位：KiB）
        uint32_t time_cost;        // 时间成本（迭代次数）
        uint32_t parallelism;      // 并行度（线程数）
        uint32_t output_length;    // 输出长度（字节）
        uint8_t version;           // 算法版本（0x13为v1.3）
    };

    // 默认参数预设
    static constexpr Parameters DEFAULTS = {
        65540,      // memory_cost: 64MiB
        3,          // time_cost: 3次迭代
        4,          // parallelism: 4线程
        32,         // output_length: 32字节
        0x13        // version: 1.3
    };

    // 强密码参数预设
    static constexpr Parameters STRONG = {
        262144,     // memory_cost: 256MiB
        4,          // time_cost: 4次迭代
        4,          // parallelism: 4线程
        64,         // output_length: 64字节
        0x13
    };

    // 快速参数预设（用于非关键场景）
    static constexpr Parameters FAST = {
        19456,      // memory_cost: 19MiB
        2,          // time_cost: 2次迭代
        1,          // parallelism: 1线程
        32,         // output_length: 32字节
        0x13
    };

    /**
     * Argon2id密钥派生函数
     * @param password 密码
     * @param salt 盐值（建议至少16字节）
     * @param params Argon2id参数
     * @param associated_data 关联数据（可选，用于AEAD）
     * @return 派生的密钥
     */
    static std::vector<uint8_t> derive(
        const std::vector<uint8_t>& password,
        const std::vector<uint8_t>& salt,
        const Parameters& params = DEFAULTS,
        const std::vector<uint8_t>& associated_data = {}
    );

    /**
     * 简化的字符串密钥派生
     * @param password 密码字符串
     * @param salt 盐字符串
     * @param output_length 输出长度
     * @return 派生的密钥
     */
    static std::vector<uint8_t> deriveString(
        const std::string& password,
        const std::string& salt,
        uint32_t output_length = 32
    );

    // 安全内存清零（用于内部和外部使用）
    static void secureMemZero(void* ptr, size_t len);

private:

    // Blake2b哈希（Argon2的核心哈希函数）
    static std::vector<uint8_t> blake2b(
        const void* input,
        size_t input_len,
        size_t output_len
    );

    // Argon2内存块混合函数
    static void fillBlock(
        std::vector<std::vector<uint64_t>>& memory,
        uint32_t block_index,
        uint32_t segment_index,
        uint32_t pseudo_random_index
    );

    // Argon2填充内存矩阵
    static void fillMemory(
        std::vector<std::vector<uint64_t>>& memory,
        uint32_t memory_blocks,
        uint32_t time_cost,
        uint32_t parallelism
    );

    // 初始化哈希
    static std::vector<uint8_t> initialHash(
        const std::vector<uint8_t>& password,
        const std::vector<uint8_t>& salt,
        const Parameters& params,
        const std::vector<uint8_t>& associated_data
    );

    // 最终化哈希
    static std::vector<uint8_t> finalizeHash(
        const std::vector<std::vector<uint64_t>>& memory,
        uint32_t output_length
    );
};

#endif // ARGON2ID_H
