#include "secure_random.h"
#include <chrono>
#include <thread>
#include <cstring>

SecureRandom::SecureRandom() {
    initialize();
}

SecureRandom::~SecureRandom() {
    // 清零任何敏感数据（如果需要）
}

void SecureRandom::initialize() {
    // 使用多个熵源初始化Mersenne Twister引擎
    std::vector<uint32_t> seeds;
    
    // 尝试从std::random_device获取熵
    // 注意：std::random_device的质量因实现而异
    try {
        for (int i = 0; i < 4; i++) {
            seeds.push_back(rd());
        }
    } catch (...) {
        // 如果random_device失败，使用时间戳作为备选
        auto now = std::chrono::high_resolution_clock::now();
        uint64_t timestamp = now.time_since_epoch().count();
        seeds.push_back(static_cast<uint32_t>(timestamp));
        seeds.push_back(static_cast<uint32_t>(timestamp >> 32));
    }

    // 使用线程ID增加熵
    uint32_t thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    seeds.push_back(thread_id);

    // 初始化MT引擎
    std::seed_seq seq(seeds.begin(), seeds.end());
    rng.seed(seq);
}

void SecureRandom::generate(uint8_t* buffer, size_t size) {
    if (buffer == nullptr || size == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mtx);

    // 使用MT19937-64生成64位随机数，然后分解为字节
    size_t full_words = size / 8;
    size_t remaining_bytes = size % 8;

    for (size_t i = 0; i < full_words; i++) {
        uint64_t random_word = rng();
        std::memcpy(buffer + i * 8, &random_word, 8);
    }

    // 处理剩余的字节
    if (remaining_bytes > 0) {
        uint64_t last_word = rng();
        std::memcpy(buffer + full_words * 8, &last_word, remaining_bytes);
    }
}

std::vector<uint8_t> SecureRandom::generateVector(size_t size) {
    std::vector<uint8_t> result(size);
    generate(result.data(), size);
    return result;
}

uint64_t SecureRandom::generate64() {
    std::lock_guard<std::mutex> lock(mtx);
    return rng();
}

uint32_t SecureRandom::generate32() {
    std::lock_guard<std::mutex> lock(mtx);
    return static_cast<uint32_t>(rng() >> 32);
}
