#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include "platform_config.h"

/**
 * 密码学工具类
 * 提供通用的加密操作、编码转换等功能
 */
class CryptoUtils {
public:
    /**
     * 将字节数组转换为十六进制字符串
     * @param data 输入数据
     * @return 十六进制字符串（小写）
     */
    static std::string bytesToHex(const std::vector<uint8_t>& data);

    /**
     * 将十六进制字符串转换为字节数组
     * @param hex 十六进制字符串
     * @return 字节数组
     */
    static std::vector<uint8_t> hexToBytes(const std::string& hex);

    /**
     * 将字节数组转换为Base64字符串
     * @param data 输入数据
     * @return Base64编码字符串
     */
    static std::string bytesToBase64(const std::vector<uint8_t>& data);

    /**
     * 将Base64字符串转换为字节数组
     * @param base64 Base64编码字符串
     * @return 字节数组
     */
    static std::vector<uint8_t> base64ToBytes(const std::string& base64);

    /**
     * 安全比较两个字节数组（恒定时间）
     * @param a 第一个字节数组
     * @param b 第二个字节数组
     * @return 是否相等
     */
    static bool constantTimeCompare(
        const std::vector<uint8_t>& a,
        const std::vector<uint8_t>& b);

    /**
     * 安全清零内存
     * @param ptr 指针
     * @param len 长度
     */
    static void secureMemZero(void* ptr, size_t len);

    /**
     * 将字节向量转换为字符串（UTF-8）
     * @param data 字节向量
     * @return 字符串
     */
    static std::string bytesToString(const std::vector<uint8_t>& data);

    /**
     * 将字符串转换为字节向量（UTF-8）
     * @param str 字符串
     * @return 字节向量
     */
    static std::vector<uint8_t> stringToBytes(const std::string& str);

    /**
     * 获取平台信息
     * @return 平台描述字符串
     */
    static std::string getPlatformInfo();

    /**
     * 校验Base64字符串的有效性
     * @param str Base64字符串
     * @return 是否有效
     */
    static bool isValidBase64(const std::string& str);

    /**
     * 校验十六进制字符串的有效性
     * @param str 十六进制字符串
     * @return 是否有效
     */
    static bool isValidHex(const std::string& str);

private:
    // Base64字母表
    static constexpr const char BASE64_CHARS[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    /**
     * Base64解码辅助函数
     * @param c Base64字符
     * @return 对应的6位值，无效时返回255
     */
    static inline uint8_t base64CharToValue(char c);

    /**
     * 十六进制字符转值
     * @param c 十六进制字符
     * @return 值（0-15），无效时返回255
     */
    static inline uint8_t hexCharToValue(char c);
};

#endif // CRYPTO_UTILS_H
