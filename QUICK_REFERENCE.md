# 快速参考指南 (Quick Reference)

## 🔐 加密/解密

### 基础用法
```cpp
#include "AES256.h"

// 创建加密器
std::vector<uint8_t> password = {/* ... */};
AES256 cipher(password);

// 加密
auto plaintext = CryptoUtils::stringToBytes("Hello");
auto ciphertext = cipher.encrypt(plaintext);

// 解密
auto decrypted = cipher.decrypt(ciphertext);
std::string result = CryptoUtils::bytesToString(decrypted);
```

### AEAD（推荐）
```cpp
// 带认证的加密
auto aead = cipher.encryptAEAD(plaintext);

// 带认证的解密
auto plaintext = cipher.decryptAEAD(aead);  // 认证失败会抛出异常
```

## 🔑 密钥派生

### 三种参数选择
```cpp
#include "argon2id.h"

// 1. 强度参数（推荐用于密码存储）
auto key = Argon2id::derive(pwd, salt, Argon2id::STRONG);

// 2. 默认参数（平衡）
auto key = Argon2id::derive(pwd, salt, Argon2id::DEFAULTS);

// 3. 快速参数（快速验证）
auto key = Argon2id::derive(pwd, salt, Argon2id::FAST);
```

### 参数对比
| 参数 | 内存 | 迭代 | 线程 | 用途 |
|------|------|------|------|------|
| STRONG | 256MB | 4 | 4 | 密码存储 |
| DEFAULTS | 65MB | 3 | 4 | 通用密钥派生 |
| FAST | 19MB | 2 | 1 | 快速验证 |

## 📝 编码转换

### Hex编码
```cpp
#include "crypto_utils.h"

// 字节 → 十六进制
std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
std::string hex = CryptoUtils::bytesToHex(data);
// 结果: "48656c6c6f"

// 十六进制 → 字节
auto bytes = CryptoUtils::hexToBytes(hex);
```

### Base64编码
```cpp
// 字节 → Base64
std::string b64 = CryptoUtils::bytesToBase64(data);
// 结果: "SGVsbG8="

// Base64 → 字节
auto bytes = CryptoUtils::base64ToBytes(b64);
```

### 字符串转换
```cpp
// 字符串 → 字节
auto bytes = CryptoUtils::stringToBytes("Hello");

// 字节 → 字符串
std::string str = CryptoUtils::bytesToString(bytes);
```

## 🎲 随机数生成

### 生成随机字节
```cpp
#include "secure_random.h"

SecureRandom rng;
auto random_bytes = rng.generateVector(32);  // 32个随机字节
```

### 生成随机整数
```cpp
uint64_t random64 = rng.generate64();
uint32_t random32 = rng.generate32();
```

## 🔒 安全操作

### 恒定时间比较
```cpp
std::vector<uint8_t> hash1 = {/* ... */};
std::vector<uint8_t> hash2 = {/* ... */};

if (CryptoUtils::constantTimeCompare(hash1, hash2)) {
    // 相等
}
```

### 安全清零内存
```cpp
std::vector<uint8_t> sensitive_data = {/* ... */};
CryptoUtils::secureMemZero(sensitive_data.data(), sensitive_data.size());
```

## 📊 Blake2b哈希

### 计算哈希
```cpp
#include "blake2b.h"

std::vector<uint8_t> data = {/* ... */};

// 默认（64字节）
auto hash = Blake2b::hash(data);

// 自定义输出大小
auto hash32 = Blake2b::hash(data, 32);

// 带密钥（用于HMAC）
std::vector<uint8_t> key = {/* ... */};
auto mac = Blake2b::hash(data, 32, key);
```

## 🌍 跨平台检查

### 检测平台
```cpp
#include "platform_config.h"

#if defined(OS_WINDOWS)
    // Windows特定代码
#elif defined(OS_LINUX)
    // Linux特定代码
#elif defined(OS_MACOS)
    // macOS特定代码
#endif
```

### 获取平台信息
```cpp
std::string info = CryptoUtils::getPlatformInfo();
// 输出: "Platform: Windows (64-bit) [little-endian] MSVC"
```

## 🐛 错误处理

### 异常捕获
```cpp
try {
    auto key = Argon2id::derive(pwd, salt, Argon2id::STRONG);
} catch (const std::invalid_argument& e) {
    // 参数无效
} catch (const std::exception& e) {
    // 其他错误
}
```

### 输入验证
```cpp
if (!CryptoUtils::isValidHex(str)) {
    // 无效的十六进制字符串
}

if (!CryptoUtils::isValidBase64(str)) {
    // 无效的Base64字符串
}
```

## ⚡ 性能建议

### 优化加密
- 为大数据块使用CTR模式
- 避免频繁创建/销毁AES256对象

### 优化密钥派生
- 对密码存储使用STRONG（只需执行一次）
- 对快速验证使用FAST（可重复执行）
- 使用足够长的盐值（≥16字节）

### 优化内存使用
```cpp
{
    std::vector<uint8_t> temp = {/* ... */};
    // 使用temp
} // 自动销毁，但不安全清零

// 敏感数据应使用：
CryptoUtils::secureMemZero(sensitive.data(), sensitive.size());
```

## 🔄 完整工作流示例

### 密码存储场景
```cpp
#include "AES256.h"
#include "argon2id.h"
#include "secure_random.h"
#include "crypto_utils.h"

// 1. 生成盐值
SecureRandom rng;
auto salt = rng.generateVector(16);

// 2. 派生密钥
auto password = CryptoUtils::stringToBytes(user_password);
auto key = Argon2id::derive(password, salt, Argon2id::STRONG);

// 3. 加密用户数据
AES256 cipher(key);
auto encrypted = cipher.encryptAEAD(user_data);

// 4. 存储：[salt || encrypted_data]
std::vector<uint8_t> stored;
stored.insert(stored.end(), salt.begin(), salt.end());
stored.insert(stored.end(), encrypted.begin(), encrypted.end());

// 保存到数据库或文件...
```

### 解密场景
```cpp
// 1. 读取存储的数据
// 前16字节是盐值，后面是加密数据
auto salt = std::vector<uint8_t>(stored.begin(), stored.begin() + 16);
auto encrypted = std::vector<uint8_t>(stored.begin() + 16, stored.end());

// 2. 重新派生密钥
auto password = CryptoUtils::stringToBytes(user_password);
auto key = Argon2id::derive(password, salt, Argon2id::STRONG);

// 3. 解密
AES256 cipher(key);
try {
    auto plaintext = cipher.decryptAEAD(encrypted);
    // 成功！使用plaintext
} catch (const std::exception& e) {
    // 认证失败，数据被篡改
}
```

## 📋 常见问题

### Q: STRONG、DEFAULTS和FAST的区别？
**A:** 内存和迭代次数不同。STRONG最安全但最慢，FAST最快但安全性最低。

### Q: 为什么要使用AEAD而不是普通加密？
**A:** AEAD提供完整性验证，防止数据被篡改。推荐生产环境使用。

### Q: 盐值需要多长？
**A:** 至少16字节（128位）。更长更好，但不必要超过32字节。

### Q: 密钥需要多长？
**A:** AES-256需要32字节（256位）。ARGON2ID可以输出任意长度。

### Q: 可以多次使用同一盐值吗？
**A:** 不建议。每次密钥派生都应使用新的随机盐值。

### Q: 线程安全吗？
**A:** SecureRandom是线程安全的。AES256不是（每个线程应使用自己的实例）。

## 🚀 性能目标

- Blake2b: 每秒数GB
- AES-256-CTR: 取决于CPU
- ARGON2ID (STRONG): ~1秒/密钥
- ARGON2ID (FAST): ~50-100ms/密钥

---

**提示**: 对于生产环境，始终优先选择安全性而非速度。
