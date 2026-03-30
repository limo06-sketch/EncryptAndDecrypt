#include <iostream>
#include <type_traits>
#include <map>
#include <cmath>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <thread>
#include <array>
#include <climits>
#include "AES256.h"
#include <atomic>
#include <fstream>
#include <sstream>
#include "best_hash.h"
#include <random>
#include <limits>   
#include <cstring>
#include <cstdint>
#include <iomanip>

// Platform-specific headers for secure password input and environment variables
#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>  // 用于GetEnvironmentVariableA
#else
    #include <termios.h>
    #include <unistd.h>
#endif

using namespace std;

// Stored AEAD blob for verification (IV||ciphertext||MAC)
static std::vector<uint8_t> stored_correct_aead;

/**
 * @brief 获取当前系统时间的字符串表示
 * @return std::string 格式化的当前时间字符串（格式：YYYY/MM/DD HH:MM:SS）
 */
static std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_val = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    std::tm tm = {};

#ifdef _WIN32
    // Windows 版本
    localtime_s(&tm, &time_val);
#else
    // Linux/macOS 版本
    localtime_r(&time_val, &tm);
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
 * @brief 高精度安全忙等待时钟函数
 * @param milliseconds 等待时间（毫秒）
 * @param mode 自旋模式：1-高精度自旋, 2-混合自旋, 3-节能自旋, 4-智能切换(默认)
 */
static void sleep(int milliseconds, int mode = 4) {
    auto start = std::chrono::high_resolution_clock::now();
    auto ns_duration = std::chrono::nanoseconds(static_cast<long long>(milliseconds) * 1000000LL);

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
    default: {
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
        U max_val = static_cast<U>(~0ULL);  // Max value for U
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

// ============================================================================
// 编译时动态密钥生成系统 - 防彩虹表
// ============================================================================

/**
 * @brief 编译时伪随机数生成器（基于__TIME__和__DATE__）
 * 每次编译时生成完全不同的密钥序列
 */
class CompileTimeRNG {
private:
    // 简单的LCG算法常数
    static constexpr uint32_t LCG_A = 1664525U;
    static constexpr uint32_t LCG_C = 1013904223U;
    static constexpr uint32_t LCG_M = 4294967296ULL;

    /**
     * @brief 编译时哈希函数，基于__TIME__和__DATE__
     * 将时间戳转换为伪随机种子
     */
    static constexpr uint32_t compile_time_seed() {
        // __TIME__ 格式: "HH:MM:SS"
        // __DATE__ 格式: "Mon DD YYYY"
        const char* time_str = __TIME__;
        const char* date_str = __DATE__;

        uint32_t hash = 5381;

        // 对时间字符串进行哈希
        for (size_t i = 0; time_str[i]; ++i) {
            hash = ((hash << 5) + hash) + static_cast<uint32_t>(time_str[i]);
        }

        // 对日期字符串进行哈希
        for (size_t i = 0; date_str[i]; ++i) {
            hash = ((hash << 5) + hash) + static_cast<uint32_t>(date_str[i]);
        }

        return hash ^ 0xDEADBEEF;
    }

    /**
     * @brief 生成编译时伪随机数
     * 使用LCG（线性同余生成器）
     */
    static constexpr uint32_t lcg_next(uint32_t& state) {
        state = (state * LCG_A + LCG_C);
        return state;
    }

public:
    /**
     * @brief 生成动态的密钥表数组
     * @tparam N 要生成的密钥个数
     * @return std::array<uint8_t, N> 包含N个伪随机密钥字节
     */
    template<size_t N>
    static constexpr std::array<uint8_t, N> generate_keys() {
        std::array<uint8_t, N> keys{};
        uint32_t state = compile_time_seed();

        for (size_t i = 0; i < N; ++i) {
            keys[i] = static_cast<uint8_t>((lcg_next(state) >> 8) & 0xFF);
        }

        return keys;
    }

    /**
     * @brief 生成动态的费舍尔-耶茨S-Box
     * 每次编译生成完全不同的非线性替换表
     * @return std::array<uint8_t, 256> 伪随机排列的256元素数组
     */
    static constexpr std::array<uint8_t, 256> generate_sbox() {
        std::array<uint8_t, 256> sbox{};

        // 初始化为0-255
        for (size_t i = 0; i < 256; ++i) {
            sbox[i] = static_cast<uint8_t>(i);
        }

        // 费舍尔-耶茨洗牌算法（编译时版本）
        uint32_t state = compile_time_seed();
        for (size_t i = 255; i > 0; --i) {
            // 生成随机索引
            uint32_t rand_val = lcg_next(state);
            size_t j = (rand_val ^ (rand_val >> 16)) % (i + 1);

            // 交换
            uint8_t temp = sbox[i];
            sbox[i] = sbox[j];
            sbox[j] = temp;
        }

        return sbox;
    }

    /**
     * @brief 生成反向S-Box（用于解密）
     * @param sbox 原始的S-Box表
     * @return std::array<uint8_t, 256> 反向S-Box
     */
    static constexpr std::array<uint8_t, 256> generate_inv_sbox(const std::array<uint8_t, 256>& sbox) {
        std::array<uint8_t, 256> inv_sbox{};

        for (size_t i = 0; i < 256; ++i) {
            inv_sbox[sbox[i]] = static_cast<uint8_t>(i);
        }

        return inv_sbox;
    }
};

// ============================================================================
// 在编译时生成动态的密钥和S-Box
// 每次编译都完全不同，防止彩虹表攻击
// ============================================================================

// 生成10个动态异或密钥
constexpr auto DYNAMIC_XOR_KEYS = CompileTimeRNG::generate_keys<10>();

// 生成8个动态替换密钥
constexpr auto DYNAMIC_SUB_KEYS = CompileTimeRNG::generate_keys<8>();

// 生成6个动态加法密钥
constexpr auto DYNAMIC_ADD_KEYS = CompileTimeRNG::generate_keys<6>();

// 生成动态的S-Box（每次编译都不同）
constexpr auto DYNAMIC_SBOX = CompileTimeRNG::generate_sbox();

// 生成对应的反向S-Box
constexpr auto DYNAMIC_INV_SBOX = CompileTimeRNG::generate_inv_sbox(DYNAMIC_SBOX);

// 编译时字符串加密函数 - 增强型混淆
template<size_t N>
consteval auto compile_time_encrypt(const char(&str)[N]) {
    // 使用编译时动态生成的密钥（每次编译都不同！）
    constexpr auto xor_keys = DYNAMIC_XOR_KEYS;
    constexpr size_t keys_count = xor_keys.size();

    // 使用编译时动态生成的替换密钥
    constexpr auto sub_keys = DYNAMIC_SUB_KEYS;
    constexpr size_t sub_keys_count = sub_keys.size();

    // 使用编译时动态生成的加法密钥
    constexpr auto add_keys = DYNAMIC_ADD_KEYS;
    constexpr size_t add_keys_count = add_keys.size();

    // 使用编译时动态生成的S-Box（每次编译都不同！）
    constexpr auto sbox = DYNAMIC_SBOX;

    std::array<uint8_t, N> result{};

    // 超强多层混淆加密
    for (size_t i = 0; i < N - 1; ++i) {
        uint8_t val = static_cast<uint8_t>(str[i]);

        // ===== 第1层: 基于位置的异或混淆 =====
        val ^= xor_keys[i % keys_count];

        // ===== 第2层: 动态S-Box 替换 (AES风格的非线性变换) =====
        val = sbox[val];

        // ===== 第3层: 简单的左旋转8位（容易反向） =====
        val = (val << 3) | (val >> 5);

        // ===== 第4层: 与多个密钥的混合运算 =====
        val ^= sub_keys[(i * 7) % sub_keys_count];
        val += static_cast<uint8_t>(i * 0x37) & 0xFF;
        val ^= sub_keys[(i * 13) % sub_keys_count];

        // ===== 第5层: 位取反和异或 =====
        val = ~val;
        val ^= add_keys[i % add_keys_count];

        // ===== 第6层: 再次 动态S-Box 替换 =====
        val = sbox[val];

        // ===== 第7层: 最终异或混淆 =====
        val ^= xor_keys[(i + 3) % keys_count];
        val ^= static_cast<uint8_t>((i * i + 0x5A) & 0xFF);

        // ===== 第8层: 非线性加法 =====
        val = (val + static_cast<uint8_t>(0x47 * (i + 1))) & 0xFF;
        val ^= add_keys[(i * 11) % add_keys_count];

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

    // 统计每个字符出现的频率
    std::map<unsigned char, int> frequencyMap;
    for (unsigned char c : key) {
        frequencyMap[c]++;
    }

    // 计算香农熵
    double entropy = 0.0;
    size_t keyLength = key.length();

    for (const auto& pair : frequencyMap) {
        double probability = static_cast<double>(pair.second) / keyLength;
        if (probability > 0) {
            entropy -= probability * std::log2(probability);
        }
    }

    // 根据熵值范围返回对应的强度描述
    int entropyScore = static_cast<int>(entropy * 10); // 放大10倍便于分级

    if (entropyScore >= 91) return "极强";
    if (entropyScore >= 61) return "强";
    if (entropyScore >= 41) return "有点强";
    if (entropyScore >= 21) return "中等";
    if (entropyScore >= 11) return "有点意思";
    return "弱";
}

#if 1
// 运行时解密函数 - 增强型混淆解密
template<size_t N>
std::string runtime_decrypt(const std::array<uint8_t, N>& encrypted) {
    // 必须与 compile_time_encrypt 的动态密钥保持一致
    // 这些是在编译时生成的，每次编译都完全不同
    constexpr auto xor_keys = DYNAMIC_XOR_KEYS;
    constexpr size_t keys_count = xor_keys.size();

    constexpr auto sub_keys = DYNAMIC_SUB_KEYS;
    constexpr size_t sub_keys_count = sub_keys.size();

    constexpr auto add_keys = DYNAMIC_ADD_KEYS;
    constexpr size_t add_keys_count = add_keys.size();

    // 使用编译时动态生成的反向S-Box
    constexpr auto inv_sbox = DYNAMIC_INV_SBOX;

    char buffer[N]{};

    // 反向解密操作 (按倒序反向执行加密的所有步骤)
    for (size_t i = 0; i < N - 1; ++i) {
        uint8_t val = encrypted[i];

        // ===== 反向第8层: 解除非线性加法 =====
        val ^= add_keys[(i * 11) % add_keys_count];
        val = (val - static_cast<uint8_t>(0x47 * (i + 1))) & 0xFF;

        // ===== 反向第7层: 解除最终异或混淆 =====
        val ^= static_cast<uint8_t>((i * i + 0x5A) & 0xFF);
        val ^= xor_keys[(i + 3) % keys_count];

        // ===== 反向第6层: 反向动态S-Box 替换 =====
        val = inv_sbox[val];

        // ===== 反向第5层: 解除位取反和异或 =====
        val ^= add_keys[i % add_keys_count];
        val = ~val;

        // ===== 反向第4层: 解除与多个密钥的混合运算 =====
        val ^= sub_keys[(i * 13) % sub_keys_count];
        val -= static_cast<uint8_t>(i * 0x37) & 0xFF;
        val ^= sub_keys[(i * 7) % sub_keys_count];

        // ===== 反向第3层: 解除左旋转8位 =====
        val = (val >> 3) | (val << 5);

        // ===== 反向第2层: 反向动态S-Box 替换 =====
        val = inv_sbox[val];

        // ===== 反向第1层: 解除基于位置的异或 =====
        val ^= xor_keys[i % keys_count];

        buffer[i] = static_cast<char>(val);
    }
    buffer[N - 1] = '\0';

    return std::string(buffer);
}

// ============================================================================
// 安全清理函数：用于在敏感数据使用后清理内存
// ============================================================================

/**
 * @brief 安全清理std::string，防止内存dump
 * 使用volatile指针确保编译器不优化掉清零操作
 */
static void secure_clean(std::string& str) {
    if (str.empty()) return;

    // 使用可写缓冲 &str[0]，并通过 volatile 指针清零以防被优化
    volatile char* p = reinterpret_cast<volatile char*>(&str[0]);
    for (size_t i = 0; i < str.size(); ++i) {
        p[i] = 0;
    }
    str.clear();
    str.shrink_to_fit();
}

// ============================================================================
// 企业级安全：环境变量获取 + AEAD加密 + Argon2id密钥推导
// ============================================================================

/**
 * @brief 企业级环境变量读取（防内存dump、防窥视）
 * 使用volatile指针确保编译器不优化掉清零操作
 * @param var_name 环境变量名称
 * @return 环境变量值，如果不存在返回空字符串
 */
static std::string getEnvironmentVariableSecure(const char* var_name) {
    if (!var_name) return "";

    std::string result;

#ifdef _WIN32
    // Windows: 使用GetEnvironmentVariable防止不安全的getenv
    char buffer[2048];
    DWORD len = GetEnvironmentVariableA(var_name, buffer, sizeof(buffer));

    if (len == 0 || len > sizeof(buffer) - 1) {
        // 错误或缓冲区太小
        volatile char* vbuf = reinterpret_cast<volatile char*>(buffer);
        for (size_t i = 0; i < sizeof(buffer); ++i) {
            vbuf[i] = 0;
        }
        return "";
    }

    result.assign(buffer, len);

    // 安全清零缓冲区 - 使用volatile防止被编译器优化
    volatile char* vbuf = reinterpret_cast<volatile char*>(buffer);
    for (size_t i = 0; i < sizeof(buffer); ++i) {
        vbuf[i] = 0;
    }
#else
    // Linux/macOS: getenv + 立即复制 + 清零
    const char* env_val = std::getenv(var_name);
    if (env_val) {
        result.assign(env_val);
        // 注意：Linux的getenv返回静态内存，不应清零
    } else {
        return "";
    }
#endif

    return result;
}

/**
 * @brief 从环境变量安全推导加密密钥（标准Argon2id KDF）
 * 
 * 使用RFC 9106标准Argon2id算法进行密钥推导，达到企业级安全标准。
 * 参数采用OWASP推荐的强化值：m=19456KiB, t=3, p=1
 * 
 * @param var_name 环境变量名（如"limo"）
 * @param salt 可选的盐值，如果为空则自动生成编译时盐+动态混入
 * @return 标准化的32字节AES-256加密密钥
 * @throws std::runtime_error 如果环境变量不存在
 */
static std::vector<uint8_t> deriveKeyFromEnvironment(
    const char* var_name, 
    const std::vector<uint8_t>& salt = {}) {

    // 步骤1：安全读取环境变量
    std::string env_value = getEnvironmentVariableSecure(var_name);
    if (env_value.empty()) {
        throw std::runtime_error("Environment variable '" + std::string(var_name) + "' not found");
    }

    // 步骤2：转换为字节向量用于Argon2id
    std::vector<uint8_t> password(env_value.begin(), env_value.end());

    // 步骤3：构建高熵盐值
    std::vector<uint8_t> actual_salt = salt;
    if (actual_salt.empty()) {
        // 使用编译时常量盐基础
        constexpr uint8_t COMPILE_TIME_SALT[] = {
            0x4c, 0x69, 0x6d, 0x6f, 0x53, 0x65, 0x63, 0x75,  // "LimoSecu"
            0x72, 0x65, 0x4b, 0x65, 0x79, 0x44, 0x65, 0x72   // "reKeyDer"
        };
        actual_salt.assign(COMPILE_TIME_SALT, 
                          COMPILE_TIME_SALT + sizeof(COMPILE_TIME_SALT));

        // 混入编译时DYNAMIC_XOR_KEYS增加多样性
        for (size_t i = 0; i < 8 && i < DYNAMIC_XOR_KEYS.size(); ++i) {
            actual_salt[i] ^= DYNAMIC_XOR_KEYS[i];
        }
    }

    // 步骤4：配置企业级Argon2id参数（OWASP推荐）
    // 参考：https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html
    Argon2id::Parameters kdf_params{
        19456,      // memory_cost: 19 MiB（内存代价，KiB单位）
        3,          // time_cost: 3次迭代（时间代价）
        1,          // parallelism: 单线程（更安全，防止侧通道）
        32,         // output_length: 32字节（AES-256密钥长度）
        0x13        // version: Argon2id v1.3
    };

    // 步骤5：使用标准Argon2id进行密钥派生
    // RFC 9106标准实现，包含：
    // - memory-hard函数（防GPU/ASIC攻击）
    // - time-cost迭代（防快速穷举）
    // - salt混淆（防彩虹表）
    std::vector<uint8_t> derived_key = Argon2id::derive(
        password,           // 原始密码（来自环境变量）
        actual_salt,        // 高熵盐值
        kdf_params          // 企业级参数
    );

    // 步骤6：验证密钥派生成功
    if (derived_key.empty() || derived_key.size() != 32) {
        throw std::runtime_error("Argon2id key derivation failed or invalid output size");
    }

    // 步骤7：安全清零原始密码（防内存dump）
    // 使用volatile指针确保编译器不优化掉清零操作
    volatile uint8_t* pwd_ptr = reinterpret_cast<volatile uint8_t*>(password.data());
    for (size_t i = 0; i < password.size(); ++i) {
        pwd_ptr[i] = 0;
    }
    password.clear();
    password.shrink_to_fit();

    // 步骤8：清零环境变量字符串
    secure_clean(env_value);

    // 步骤9：返回安全派生的32字节密钥
    return derived_key;
}

/**
 * @brief 企业级安全字符串存储结构
 * 格式: [IV(32)] || [Ciphertext] || [GCM_TAG(16)]
 */
struct SecureStringBlob {
    std::vector<uint8_t> iv;
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> auth_tag;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> result;
        result.insert(result.end(), iv.begin(), iv.end());
        result.insert(result.end(), ciphertext.begin(), ciphertext.end());
        result.insert(result.end(), auth_tag.begin(), auth_tag.end());
        return result;
    }

    static SecureStringBlob deserialize(const std::vector<uint8_t>& blob) {
        SecureStringBlob result;
        size_t offset = 0;

        // 提取IV (32字节)
        if (offset + 32 <= blob.size()) {
            result.iv.assign(blob.begin() + offset, blob.begin() + offset + 32);
            offset += 32;
        }

        // 提取Auth Tag (16字节，从末尾)
        if (blob.size() >= offset + 16) {
            result.auth_tag.assign(
                blob.begin() + blob.size() - 16,
                blob.end()
            );

            // 提取Ciphertext (中间部分)
            result.ciphertext.assign(
                blob.begin() + offset,
                blob.begin() + blob.size() - 16
            );
        }

        return result;
    }
};

/**
 * @brief 生成企业级安全存储的值
 * 编译时需要指定存储值（现在从环境变量运行时读取）
 * @param env_var_name 存储的值的环境变量名（如 "limo"）
 * @param key_var_name 加密密钥的环境变量名（如 "SECURE_KEY"）
 * @return 解密后的原始值
 */
static std::string get_secure_string() {
    try {
        // 从环境变量"limo"读取要保护的值
        std::string protected_value = getEnvironmentVariableSecure("limo");
        if (protected_value.empty()) {
            throw std::runtime_error("Environment variable 'limo' not configured");
        }

        // 为了企业级安全，我们使用带AEAD的加密存储
        // 这里实现完整的加密->存储->验证流程

        // 步骤1：从环境变量推导密钥
        std::vector<uint8_t> key = deriveKeyFromEnvironment("limo");

        // 步骤2：创建AES256实例进行AEAD加密
        AES256 cipher(key);

        // 步骤3：将值转换为字节
        std::vector<uint8_t> plaintext(protected_value.begin(), protected_value.end());

        // 步骤4：使用AEAD加密（包含完整性验证）
        std::vector<uint8_t> aead_blob = cipher.encryptAEAD(plaintext);

        // 步骤5：验证解密（完整性检查）
        std::vector<uint8_t> decrypted = cipher.decryptAEAD(aead_blob);

        // 步骤6：确认解密结果与原值一致（防篡改验证）
        if (decrypted.size() != plaintext.size() || 
            !std::equal(decrypted.begin(), decrypted.end(), plaintext.begin())) {
            throw std::runtime_error("AEAD verification failed - possible tampering detected");
        }

        // 步骤7：返回解密值
        std::string result(decrypted.begin(), decrypted.end());

        // 步骤8：安全清零所有敏感数据
        // 使用volatile指针清零key
        volatile uint8_t* key_ptr = reinterpret_cast<volatile uint8_t*>(key.data());
        for (size_t i = 0; i < key.size(); ++i) {
            key_ptr[i] = 0;
        }
        key.clear();
        key.shrink_to_fit();

        // 清零plaintext
        volatile uint8_t* plain_ptr = reinterpret_cast<volatile uint8_t*>(plaintext.data());
        for (size_t i = 0; i < plaintext.size(); ++i) {
            plain_ptr[i] = 0;
        }
        plaintext.clear();
        plaintext.shrink_to_fit();

        // 清零decrypted
        volatile uint8_t* dec_ptr = reinterpret_cast<volatile uint8_t*>(decrypted.data());
        for (size_t i = 0; i < decrypted.size(); ++i) {
            dec_ptr[i] = 0;
        }
        decrypted.clear();
        decrypted.shrink_to_fit();

        // 清零aead_blob
        volatile uint8_t* aead_ptr = reinterpret_cast<volatile uint8_t*>(aead_blob.data());
        for (size_t i = 0; i < aead_blob.size(); ++i) {
            aead_ptr[i] = 0;
        }
        aead_blob.clear();
        aead_blob.shrink_to_fit();

        // 清零protected_value（std::string）
        secure_clean(protected_value);

        return result;

    } catch (const std::exception& e) {
        // 企业日志记录安全异常
        std::cerr << "\x1b[31m[SECURITY] get_secure_string failed: " 
                  << e.what() << "\x1b[0m" << std::endl;

        // 不返回任何敏感信息，抛出异常
        throw std::runtime_error("Secure string retrieval failed - check environment variables");
    }
}

#endif
// ============================================================================
// 跨平台隐藏密码输入函数 (支持无限长度)
// ============================================================================

/**
 * @brief 跨平台隐藏密码输入函数
 * 
 * 特点:
 * ✓ Windows: 使用 conio.h (_getch)
 * ✓ Linux/macOS: 使用 termios.h (tcgetattr/tcsetattr)
 * ✓ 支持无限长度密码
 * ✓ 不显示输入字符
 * ✓ 显示输入进度（点号）
 * ✓ 支持退格删除
 * 
 * @return std::string 用户输入的密码
 */
static std::string getSecurePassword() {
    std::string password;
    password.reserve(64); // 预分配内存，减少老电脑的分配开销

    // --- 关键步骤 1：强制刷新输出缓冲区 ---
    // Release 模式下不 flush 提示符会“隐身”
    std::cout.flush();

#ifdef _WIN32
    // Windows 版使用原生 _getch()
    while (true) {
        int ch = _getch();

        // 识别 Enter 键 (Windows 下可能是 13)
        if (ch == 13 || ch == 10) {
            std::cout << std::endl;
            break;
        }
        // 识别 Backspace (8)
        else if (ch == 8) {
            if (!password.empty()) {
                password.pop_back();
                // 经典的控制台擦除序列
                std::cout << "\b \b" << std::flush;
            }
        }
        // 过滤不可见字符（如方向键、功能键），只接收 ASCII 可打印字符
        else if (ch >= 32 && ch <= 126) {
            password.push_back(static_cast<char>(ch));
            std::cout << '*' << std::flush;
        }
    }
#else
    // Linux/POSIX 版使用 termios 禁用回显
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO | ICANON); // 禁用回显和行缓冲
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    char ch;
    while (read(STDIN_FILENO, &ch, 1) == 1) {
        if (ch == '\n' || ch == '\r') {
            std::cout << std::endl;
            break;
        }
        else if (ch == 127 || ch == 8) { // 处理退格
            if (!password.empty()) {
                password.pop_back();
                std::cout << "\b \b" << std::flush;
            }
        }
        else if (ch >= 32 && ch <= 126) {
            password.push_back(ch);
            std::cout << '*' << std::flush;
        }
    }
    // 恢复终端设置
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif

    return password;
}

// ============================================================================
// Production-level Security Enhancement - Argon2 Edition
// ============================================================================

// Salt generation function
static std::vector<uint8_t> generateSalt(size_t length = 16) {
    std::vector<uint8_t> salt(length);
    std::random_device rd;
    for (size_t i = 0; i < length; ++i) {
        salt[i] = static_cast<uint8_t>(rd() & 0xFF);
    }
    return salt;
}

// Secure memory cleanup wrapper (RAII)
class SecureMemoryGuard {
private:
    std::vector<uint8_t>* data;

public:
    explicit SecureMemoryGuard(std::vector<uint8_t>* ptr) : data(ptr) {}

    ~SecureMemoryGuard() {
        if (data) {
            std::fill(data->begin(), data->end(), 0);
            data->clear();
            data->shrink_to_fit();
        }
    }

    SecureMemoryGuard(const SecureMemoryGuard&) = delete;
    SecureMemoryGuard& operator=(const SecureMemoryGuard&) = delete;
};

// Account lockout manager
class AccountLockoutManager {
private:
    std::string lockout_file;
    static constexpr int MAX_FAILED_ATTEMPTS = 3;
    static constexpr long long LOCKOUT_DURATION_SECONDS = 600;  // 10 minutes

    struct LockoutRecord {
        int failed_count;
        std::chrono::system_clock::time_point lockout_time;
    };

public:
    explicit AccountLockoutManager(const std::string& data_dir = ".") {
        lockout_file = data_dir + "/lockout.dat";
    }

    bool isAccountLocked() {
        std::ifstream file(lockout_file, std::ios::binary);
        if (!file) {
            return false;  // File not found, account not locked
        }

        int failed_count = 0;
        long long lockout_time_us = 0;

        file.read(reinterpret_cast<char*>(&failed_count), sizeof(failed_count));
        file.read(reinterpret_cast<char*>(&lockout_time_us), sizeof(lockout_time_us));
        file.close();

        if (failed_count < MAX_FAILED_ATTEMPTS) {
            return false;
        }

        auto lockout_tp = std::chrono::system_clock::time_point(
            std::chrono::microseconds(lockout_time_us));
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lockout_tp).count();

        if (elapsed >= LOCKOUT_DURATION_SECONDS) {
            // Lockout expired, clear record
            std::remove(lockout_file.c_str());
            return false;
        }

        return true;
    }

    int getRemainingLockoutTime() const {
        std::ifstream file(lockout_file, std::ios::binary);
        if (!file) return 0;

        int failed_count = 0;
        long long lockout_time_us = 0;

        file.read(reinterpret_cast<char*>(&failed_count), sizeof(failed_count));
        file.read(reinterpret_cast<char*>(&lockout_time_us), sizeof(lockout_time_us));
        file.close();

        if (failed_count < MAX_FAILED_ATTEMPTS) {
            return 0;
        }

        auto lockout_tp = std::chrono::system_clock::time_point(
            std::chrono::microseconds(lockout_time_us));
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lockout_tp).count();

        return max(0ll, LOCKOUT_DURATION_SECONDS - elapsed);
    }

    void recordFailedAttempt() const {
        std::ifstream file(lockout_file, std::ios::binary);
        int failed_count = 0;
        long long lockout_time_us = 0;

        if (file) {
            file.read(reinterpret_cast<char*>(&failed_count), sizeof(failed_count));
            file.read(reinterpret_cast<char*>(&lockout_time_us), sizeof(lockout_time_us));
            file.close();
        }

        failed_count++;

        std::ofstream outfile(lockout_file, std::ios::binary | std::ios::trunc);
        if (!outfile) return;

        auto now = std::chrono::system_clock::now();
        lockout_time_us = now.time_since_epoch().count() / 1000;  // Convert to microseconds

        outfile.write(reinterpret_cast<const char*>(&failed_count), sizeof(failed_count));
        outfile.write(reinterpret_cast<const char*>(&lockout_time_us), sizeof(lockout_time_us));
        outfile.close();
    }

    void recordSuccessfulAttempt() {
        // Clear failed record
        std::remove(lockout_file.c_str());
    }

    int getFailedAttempts() const {
        std::ifstream file(lockout_file, std::ios::binary);
        if (!file) return 0;

        int failed_count = 0;
        file.read(reinterpret_cast<char*>(&failed_count), sizeof(failed_count));
        file.close();

        return min(failed_count, MAX_FAILED_ATTEMPTS);
    }
};

// Enhanced audit logger class
class AuditLogger {
private:
    std::ofstream log_file;
    std::string log_path;
    static constexpr size_t MAX_LOG_SIZE = 10 * 1024 * 1024;  // 10 MB

    void ensureOpen() {
        if (!log_file.is_open()) {
            log_file.open(log_path, std::ios::app);
        }
    }

    void rotateIfNeeded() {
        log_file.seekp(0, std::ios::end);
        auto pos = log_file.tellp();
        if (pos != std::ofstream::pos_type(-1) &&
            static_cast<std::streamoff>(pos) > static_cast<std::streamoff>(MAX_LOG_SIZE)) {
            log_file.close();

            auto now = std::chrono::system_clock::now();
            auto time_val = std::chrono::system_clock::to_time_t(now);
            std::ostringstream backup_name;
            backup_name << log_path << ".bak." << time_val;

            std::rename(log_path.c_str(), backup_name.str().c_str());
            log_file.open(log_path, std::ios::app);
        }
    }

public:
    explicit AuditLogger(const std::string& path) : log_path(path) {
        ensureOpen();
    }

    ~AuditLogger() {
        if (log_file.is_open()) {
            log_file.close();
        }
    }

    void logAuthAttempt(bool success, int remaining_attempts = -1) {
        ensureOpen();
        rotateIfNeeded();

        auto time_str = getCurrentTime();
        log_file << "[" << time_str << "] AUTH_ATTEMPT: " 
                 << (success ? "SUCCESS" : "FAILED");
        if (remaining_attempts >= 0) {
            log_file << " (Remaining: " << remaining_attempts << ")";
        }
        log_file << std::endl;
        log_file.flush();
    }

    void logAccountLockout(int duration_seconds) {
        ensureOpen();
        rotateIfNeeded();

        auto time_str = getCurrentTime();
        log_file << "[" << time_str << "] ACCOUNT_LOCKOUT: " 
                 << "Locked for " << duration_seconds << " seconds" << std::endl;
        log_file.flush();
    }

    void logPasswordChange() {
        ensureOpen();
        rotateIfNeeded();

        auto time_str = getCurrentTime();
        log_file << "[" << time_str << "] PASSWORD_CHANGED" << std::endl;
        log_file.flush();
    }

    void logSecurityEvent(const std::string& event_description) {
        ensureOpen();
        rotateIfNeeded();

        auto time_str = getCurrentTime();
        log_file << "[" << time_str << "] SECURITY_EVENT: " << event_description << std::endl;
        log_file.flush();
    }
};