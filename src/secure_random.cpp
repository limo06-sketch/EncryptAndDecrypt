// secure_random.cpp
#include "secure_random.h"
#include <vector>
#include <stdexcept>
#include <type_traits>
#include <limits>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#ifdef __linux__
#include <sys/random.h>
#endif
#endif

// 确保在旧系统上也能编译 O_CLOEXEC
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

bool SecureRandom::generate(uint8_t* buffer, size_t size) {
    if (!buffer || size == 0) {
        return false;
    }

#ifdef _WIN32
    // Windows: 使用 BCryptGenRandom
    NTSTATUS status = BCryptGenRandom(
        nullptr,
        buffer,
        static_cast<ULONG>(size),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );
    return status == 0;

#else
#ifdef __linux__
    // 现代 Linux: 优先使用 getrandom 系统调用 (避免文件描述符耗尽问题)
    size_t total_read = 0;
    while (total_read < size) {
        ssize_t n = getrandom(buffer + total_read, size - total_read, 0);
        if (n < 0) {
            if (errno == EINTR) continue; // 被信号中断，重试
            break; // 其他错误，降级回退到 /dev/urandom
        }
        total_read += n;
    }
    if (total_read == size) {
        return true;
    }
#endif

    // Unix-like 回退方案: 使用 /dev/urandom
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    size_t urandom_read = 0;
    while (urandom_read < size) {
        ssize_t n = read(fd, buffer + urandom_read, size - urandom_read);
        if (n <= 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return false;
        }
        urandom_read += n;
    }

    close(fd);
    return true;
#endif
}

std::vector<uint8_t> SecureRandom::generateVector(size_t size) {
    std::vector<uint8_t> result;
    if (size == 0) {
        return result;
    }

    result.resize(size);
    // 安全修复：失败时必须抛出异常，绝不能返回空向量或全0
    if (!generate(result.data(), size)) {
        throw std::runtime_error("SecureRandom: Failed to gather entropy from OS.");
    }
    return result;
}

uint64_t SecureRandom::generate64() {
    uint64_t result = 0;
    if (!generate(reinterpret_cast<uint8_t*>(&result), sizeof(result))) {
        throw std::runtime_error("SecureRandom: Failed to gather entropy from OS.");
    }
    return result;
}

uint32_t SecureRandom::generate32() {
    uint32_t result = 0;
    if (!generate(reinterpret_cast<uint8_t*>(&result), sizeof(result))) {
        throw std::runtime_error("SecureRandom: Failed to gather entropy from OS.");
    }
    return result;
}

template<typename T>
T SecureRandom::generateRange(T min, T max) {
    if (max <= min) {
        return min;
    }

    // 安全修复 1: 使用无符号类型计算跨度，防止有符号整数溢出 (未定义行为)
    using UnsignedT = typename std::make_unsigned<T>::type;
    UnsignedT u_min = static_cast<UnsignedT>(min);
    UnsignedT u_max = static_cast<UnsignedT>(max);
    UnsignedT range = u_max - u_min;

    // 安全修复 2: 动态计算掩码 (Bitmask)
    // 找到大于等于 range 的最小全 1 掩码 (例如 range 是 5 [101]，掩码就是 7 [111])
    UnsignedT mask = 0;
    UnsignedT temp = range;
    while (temp > 0) {
        mask = (mask << 1) | 1;
        temp >>= 1;
    }

    // 安全修复 3: 拒绝采样 (Rejection Sampling) 消除模偏差
    UnsignedT random_val = 0;
    do {
        // 直接生成目标类型大小的随机字节
        if (!generate(reinterpret_cast<uint8_t*>(&random_val), sizeof(random_val))) {
            throw std::runtime_error("SecureRandom: Failed to gather entropy from OS.");
        }

        // 应用掩码，截断多余的高位，节省重试次数
        random_val &= mask;

        // 如果生成的值超出了公平范围，则丢弃并重新生成
    } while (random_val > range);

    // 转换回原类型并加上基准值
    return static_cast<T>(u_min + random_val);
}

// 显式实例化常用类型
template uint8_t SecureRandom::generateRange<uint8_t>(uint8_t, uint8_t);
template uint16_t SecureRandom::generateRange<uint16_t>(uint16_t, uint16_t);
template uint32_t SecureRandom::generateRange<uint32_t>(uint32_t, uint32_t);
template uint64_t SecureRandom::generateRange<uint64_t>(uint64_t, uint64_t);
template int8_t SecureRandom::generateRange<int8_t>(int8_t, int8_t);
template int16_t SecureRandom::generateRange<int16_t>(int16_t, int16_t);
template int32_t SecureRandom::generateRange<int32_t>(int32_t, int32_t);
template int64_t SecureRandom::generateRange<int64_t>(int64_t, int64_t);