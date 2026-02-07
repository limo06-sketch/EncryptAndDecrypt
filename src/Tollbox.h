#include <iostream>
#include <type_traits>
#include <map>
#include <cmath>
#include <vector>
#include <chrono>
#include <thread>
#include <array>
#include <climits>
#include "AES256.h"
#include <atomic>
#include <fstream>
#include <string>
#include "best_hash.h"
#include <random>
#include <limits>   
#include <cstring>
#include <cstdint>
using namespace std;

// Stored AEAD blob for verification (IV||ciphertext||MAC)
static std::vector<uint8_t> stored_correct_aead;

/**
 * @brief 获取当前系统时间的字符串表示
 * @return std::string 格式化的当前时间字符串（格式：YYYY/MM/DD HH:MM:SS）
 */
static std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    std::tm tm = {};

#ifdef _WIN32
    // Windows 版本
    localtime_s(&tm, &time_t);
#else
    // Linux/macOS 版本
    localtime_r(&time_t, &tm);
#endif

    ss << std::put_time(&tm, "%Y/%m/%d %H:%M:%S");
    return ss.str();
}

/**
 * @brief 安全字符串比较函数，防止计时攻击
 * @tparam T 字符串类型（std::string, std::string_view, const char* 等）
 * @param known_str 已知的字符串（如正确的密码哈希）
 * @param user_str 用户提供的字符串
 * @return bool 两个字符串内容是否完全相等
 *
 * @details 使用恒定时间比较算法，无论字符串匹配程度如何，比较时间基本恒定
 *          防止攻击者通过比较时间差推测字符串内容[6,7](@ref)
 */
template<typename T1, typename T2>
static bool yes_no(const T1& known_str, const T2& user_str) {
    // 转换为string_view避免不必要的拷贝，支持多种字符串类型
    std::string_view known_sv(known_str);
    std::string_view user_sv(user_str);

    // 获取长度信息
    size_t known_len = known_sv.length();
    size_t user_len = user_sv.length();

    // 恒定时间比较：始终比较最大长度，避免基于长度的时序信息泄露
    size_t max_len = (known_len > user_len) ? known_len : user_len;

    // 使用位运算累积比较结果，避免短路求值
    unsigned char result = 0;

    // 比较每个字符，使用异或操作符
    for (size_t i = 0; i < max_len; ++i) {
        // 如果索引超出字符串长度，使用0（空字符）进行比较
        unsigned char known_byte = (i < known_len) ?
            static_cast<unsigned char>(known_sv[i]) : 0;
        unsigned char user_byte = (i < user_len) ?
            static_cast<unsigned char>(user_sv[i]) : 0;

        // 异或操作：相同字符结果为0，不同字符结果为非0
        // 使用位或累积差异，任何差异都会使result变为非0
        result |= (known_byte ^ user_byte);
    }

    // 只有所有字符都相同且长度相等时，result才为0
    return (result == 0) && (known_len == user_len);
}

/**
 * @brief 严禁侵权，盗版必究
 * @brief 高精度安全忙等待时钟函数
 * @param nanoseconds 等待时间（纳秒）
 * @param mode 自旋模式：1-高精度自旋, 2-混合自旋, 3-节能自旋, 4-智能切换(默认)
 */
static void sleep(int milliseconds, int mode = 4) {
    auto start = std::chrono::high_resolution_clock::now();
    auto ns_duration = std::chrono::nanoseconds(milliseconds * 1000000LL);

    switch (mode) {
    case 1: // 高精度自旋
        while (std::chrono::high_resolution_clock::now() - start < ns_duration) {
            // 纯忙等待，最高精度
        }
        break;

    case 2: // 混合自旋
        while (std::chrono::high_resolution_clock::now() - start < ns_duration) {
            auto remaining = ns_duration - (std::chrono::high_resolution_clock::now() - start);
            if (remaining > std::chrono::microseconds(100)) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(1000)); // 短暂休眠降低CPU
            }
        }
        break;

    case 3: // 节能自旋
        while (std::chrono::high_resolution_clock::now() - start < ns_duration) {
            auto remaining = ns_duration - (std::chrono::high_resolution_clock::now() - start);
            if (remaining > std::chrono::milliseconds(1)) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
            else if (remaining > std::chrono::microseconds(100)) {
                std::this_thread::yield(); // 让出CPU时间片
            }
            // 最后100微秒忙等待保证精度
        }
        break;

    case 4: // 智能切换（默认）
    default:
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        while (elapsed < ns_duration) {
            auto remaining = ns_duration - elapsed;

            if (remaining > std::chrono::milliseconds(10)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            else if (remaining > std::chrono::milliseconds(1)) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            else if (remaining > std::chrono::microseconds(100)) {
                std::this_thread::yield();
            }
            // 最后100微秒使用高精度自旋

            elapsed = std::chrono::high_resolution_clock::now() - start;
        }
        break;
    }
}

/**
 * @brief 生成指定范围内的军事级真随机数
 * @tparam T 生成的整数类型（如 int, uint32_t, long long 等）
 * @param min 范围下限（包含）
 * @param max 范围上限（包含）
 * @return 返回 [min, max] 范围内的随机整数
 */
template<typename T>
T military_grade_random_range(T min, T max) {
    if (min > max) std::swap(min, max);
    using U = std::uint64_t;
    U range = static_cast<U>(max) - static_cast<U>(min) + 1ULL;

    std::random_device rd;

    // build 64-bit random value from random_device
    while (true) {
        U x = 0ULL;
        // combine multiple rd() outputs to fill 64 bits
        for (int i = 0; i < 2; ++i) {
            x = (x << 32) | static_cast<U>(rd());
        }

        // rejection sampling to avoid modulo bias
        constexpr U max_val = std::numeric_limits<U>::max();
        U limit = max_val - (max_val % range);
        if (x <= limit) {
            return static_cast<T>(static_cast<U>(min) + (x % range));
        }
        // otherwise retry
    }
}

// 生成真随机密钥的函数
// @return vector<uint8_t> 包含32个随机字节的AES-256密钥
static std::vector<uint8_t> generateRandomKey() {
    std::random_device rd; // may be non-deterministic on some platforms
    std::vector<uint8_t> randomKey(32);
    for (size_t i = 0; i < randomKey.size(); ++i) {
        // combine multiple rd() calls if rd() has limited range
        unsigned int v = rd();
        randomKey[i] = static_cast<uint8_t>(v & 0xFF);
    }
    return randomKey;
}

static string guess(const long long pu, long long u)
{
    if (u == pu)
    {
        return "true";
    }
    else {
        if (u > pu)
        {
            return "big";
        }
        else {
            return "small";
        }
    }
}

/**
 * @brief 安全获取整数输入（带验证和范围检查）
 * @param min 最小值（包含）
 * @param max 最大值（包含）
 * @param prompt 提示信息
 * @return 返回有效的整数，如果输入无效则重复提示
 */
static long long getSafeInput(long long min, long long max, const string& prompt) {
    long long input;
    while (true) {
        clog << prompt;
        if (cin >> input) {
            if (input >= min && input <= max) {
                cin.ignore(10000, '\n');  // 清理输入缓冲区
                return input;
            }
            else {
                clog << "\x1b[31m输入超出范围，请输入 " << min << " 到 " << max << " 之间的数字\x1b[36m" << endl;
            }
        }
        else {
            cin.clear();  // 清除错误标志
            cin.ignore(10000, '\n');  // 丢弃无效输入
            clog << "\x1b[31m输入无效，请输入整数\x1b[36m" << endl;
        }
    }
}

static size_t game()
{
    clog << "请输入难度: 1-简单(1-100) 2-中等(1-1000) 3-困难(1-10000)" << endl;
    auto level = getSafeInput(1, 3, "请选择难度 [1/2/3] >> ");

    long long max_num = 100;
    switch (level)
    {
    case 1:
        max_num = 100;
        break;
    case 2:
        max_num = 1000;
        break;
    case 3:
        max_num = 10000;
        break;
    default:
        max_num = 100;
        break;
    }

    long long pu = military_grade_random_range<long long>(1, max_num);
    clog << "已生成随机数，请开始猜测(范围1~" << max_num << ")" << endl;

    size_t cnt = 0;

    while (true)
    {
        auto u = getSafeInput(1, max_num, "输入你的猜测 >> ");

        string res = guess(pu, u);
        if (res == "true")
        {
            clog << "\x1b[32m恭喜你，猜对了！答案就是：" << pu << "\x1b[36m" << endl;
            ++cnt;
            break;
        }
        else if (res == "big")
        {
            clog << "\x1b[33m你猜的数字太大了，请重新输入\x1b[36m" << endl;
            ++cnt;
        }
        else if (res == "small")
        {
            clog << "\x1b[34m你猜的数字太小了，请重新输入\x1b[36m" << endl;
            ++cnt;
        }
    }
    return cnt;
}

static void simple_progress(int current, int total, int width = 20) {
    // 计算进度
    float progress = static_cast<float>(current) / total;
    int percent = static_cast<int>(progress * 100);
    int pos = static_cast<int>(width * progress);

    // 显示进度条
    std::cout << "[";
    for (int i = 0; i < width; ++i) {
        if (i < pos) std::cout << "#";
        else std::cout << " ";
    }
    std::cout << "] " << percent << "%\r";
    std::cout.flush();
}

// 编译时字符串加密函数
template<size_t N>
consteval auto compile_time_encrypt(const char(&str)[N]) {
    // 使用多层异或混淆
    constexpr uint8_t xor_keys[] = { 0xA5, 0x5A, 0x33, 0xCC, 0xF0, 0x0F };
    constexpr size_t keys_count = sizeof(xor_keys);

    std::array<uint8_t, N> result{};

    // 多层混淆加密
    for (size_t i = 0; i < N - 1; ++i) {
        uint8_t val = static_cast<uint8_t>(str[i]);

        // 第1层: 基于位置的异或
        val ^= xor_keys[i % keys_count];
        // 第2层: 字节内位旋转
        val = (val << 3) | (val >> 5);
        // 第3层: 与常量运算
        val += static_cast<uint8_t>(i * 0x37) & 0xFF;
        // 第4层: 位取反
        val = ~val;
        // 第5层: 再次异或
        val ^= xor_keys[(i + 3) % keys_count];

        result[i] = val;
    }
    result[N - 1] = 0;  // 保持字符串结束符

    return result;
}

/**
 * @brief 计算密钥的熵值并返回强度描述
 * @param key 待测试的密钥字符串
 * @return string 熵值强度描述
 */
static std::string calculateKeyEntropy(const std::string& key) {
    if (key.empty()) {
        return "弱"; // 空密钥直接返回弱
    }

    // 统计每个字符出现的频率[7](@ref)
    std::map<char, int> frequencyMap;
    for (char c : key) {
        frequencyMap[c]++;
    }

    // 计算香农熵[7](@ref)
    double entropy = 0.0;
    size_t keyLength = key.length();

    for (const auto& pair : frequencyMap) {
        double probability = static_cast<double>(pair.second) / keyLength;
        if (probability > 0) {
            entropy -= probability * log2(probability);
        }
    }

    // 根据熵值范围返回对应的强度描述[1,4](@ref)
    int entropyScore = static_cast<int>(entropy * 10); // 放大10倍便于分级

    if (entropyScore >= 91) return "极强";
    if (entropyScore >= 61) return "强";
    if (entropyScore >= 41) return "有点强";
    if (entropyScore >= 21) return "中等";
    if (entropyScore >= 11) return "有点意思";
    return "弱";
}


// 运行时解密函数
template<size_t N>
std::string runtime_decrypt(const std::array<uint8_t, N>& encrypted) {
    constexpr uint8_t xor_keys[] = { 0xA5, 0x5A, 0x33, 0xCC, 0xF0, 0x0F };
    constexpr size_t keys_count = sizeof(xor_keys);

    char buffer[N]{};

    // 反向解密操作
    for (size_t i = 0; i < N - 1; ++i) {
        uint8_t val = encrypted[i];

        // 反向第5层: 异或
        val ^= xor_keys[(i + 3) % keys_count];
        // 反向第4层: 位取反
        val = ~val;
        // 反向第3层: 与常量运算
        val -= static_cast<uint8_t>(i * 0x37) & 0xFF;
        // 反向第2层: 字节内位旋转
        val = (val >> 3) | (val << 5);
        // 反向第1层: 基于位置的异或
        val ^= xor_keys[i % keys_count];

        buffer[i] = static_cast<char>(val);
    }
    buffer[N - 1] = '\0';

    return std::string(buffer);
}

static std::string get_secure_string() {
    // 在编译时加密字符串
    constexpr auto encrypted = compile_time_encrypt("limo");


    // 运行时解密
    return runtime_decrypt(encrypted);
}

// 安全清理函数：用于在敏感数据使用后清理内存
static void secure_clean(std::string& str) {
    if (str.empty()) return;

    // 使用可写缓冲 &str[0]，并通过 volatile 指针清零以防被优化
    char* data = &str[0];
    volatile char* p = data;
    for (size_t i = 0; i < str.size(); ++i) {
        p[i] = 0;
    }
    str.clear();
    str.shrink_to_fit();
}