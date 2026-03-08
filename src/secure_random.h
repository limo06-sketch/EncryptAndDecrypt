// secure_random.h
#ifndef SECURE_RANDOM_H
#define SECURE_RANDOM_H

#include <cstdint>
#include <vector>
#include <cstddef>

/**
 * 跨平台的密码学安全随机数生成器
 * 支持: Windows, Linux, macOS, BSD等标准C++平台
 * 使用操作系统提供的密码学安全随机数源
 */
class SecureRandom {
public:
    SecureRandom() = default;
    ~SecureRandom() = default;

    // 禁止拷贝和移动
    SecureRandom(const SecureRandom&) = delete;
    SecureRandom& operator=(const SecureRandom&) = delete;
    SecureRandom(SecureRandom&&) = delete;
    SecureRandom& operator=(SecureRandom&&) = delete;

    /**
     * 生成密码学安全的随机字节
     * @param buffer 输出缓冲区
     * @param size 需要生成的字节数
     * @return 成功返回true，失败返回false
     */
    bool generate(uint8_t* buffer, size_t size);

    /**
     * 生成密码学安全的随机字节向量
     * @param size 需要生成的字节数
     * @return 包含随机字节的向量，失败抛出异常
     */
    std::vector<uint8_t> generateVector(size_t size);

    /**
     * 生成密码学安全的64位随机整数
     * @return 随机的uint64_t值，失败抛出异常
     */
    uint64_t generate64();

    /**
     * 生成密码学安全的32位随机整数
     * @return 随机的uint32_t值，失败抛出异常
     */
    uint32_t generate32();

    /**
     * 生成指定范围内的随机整数 (均匀分布，无模偏差)
     * @param min 最小值
     * @param max 最大值
     * @return 范围内的随机整数，失败抛出异常
     */
    template<typename T>
    T generateRange(T min, T max);
};

#endif // SECURE_RANDOM_H