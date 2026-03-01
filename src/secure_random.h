#ifndef SECURE_RANDOM_H
#define SECURE_RANDOM_H

#include <cstdint>
#include <vector>
#include <random>
#include <mutex>

/**
 * 跨平台的密码学安全随机数生成器
 * 支持: Windows, Linux, macOS, BSD等标准C++平台
 */
class SecureRandom {
public:
    SecureRandom();
    ~SecureRandom();

    /**
     * 生成安全随机字节
     * @param buffer 输出缓冲区
     * @param size 需要生成的字节数
     */
    void generate(uint8_t* buffer, size_t size);

    /**
     * 生成安全随机字节向量
     * @param size 需要生成的字节数
     * @return 包含随机字节的向量
     */
    std::vector<uint8_t> generateVector(size_t size);

    /**
     * 生成随机的64位整数
     * @return 随机的uint64_t值
     */
    uint64_t generate64();

    /**
     * 生成随机的32位整数
     * @return 随机的uint32_t值
     */
    uint32_t generate32();

private:
    // 内部状态
    std::random_device rd;
    std::mt19937_64 rng;
    std::mutex mtx;

    // 初始化随机数生成器
    void initialize();
};

#endif // SECURE_RANDOM_H
