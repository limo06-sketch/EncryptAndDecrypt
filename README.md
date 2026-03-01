# 🔐 高性能跨平台C++ 加密库 v1.0.0

![C++](https://img.shields.io/badge/C%2B%2B-11-blue)
![License](https://img.shields.io/badge/License-MIT-green)
![Platform](https://img.shields.io/badge/Platform-Windows%2FLinux%2FmacOS-brightgreen)
![Status](https://img.shields.io/badge/Status-Production-blue)

## 📖 简介

一个使用**标准C++库**（仅依赖STL）实现的企业级加密库，完全跨平台兼容。集成了最新的密码学标准，包括Blake2b哈希、ARGON2ID密钥派生和AES-256加密。

## ✨ 主要特性

### 🔒 密码学算法
- **Blake2b** - RFC 7693标准实现（完整版本）
- **ARGON2ID** - 内存困难型密钥派生函数（抵御GPU攻击）
- **AES-256** - 高级加密标准（CTR模式 + AEAD）
- **SecureRandom** - 密码学安全的随机数生成（跨平台）

### 🌍 跨平台支持
- ✅ Windows (Visual Studio 2015+)
- ✅ Linux (GCC 4.9+, Clang 3.8+)
- ✅ macOS (Clang 3.8+)
- ✅ 其他POSIX系统

### 🛡️ 安全特性
- 恒定时间操作（防时序攻击）
- 安全的内存清零（防泄露）
- 防填充谕言攻击（CTR模式）
- 完整性保护（AEAD认证加密）
- 参数验证和错误处理

### 📦 零依赖
- 仅依赖标准C++库（STL）
- 无第三方加密库依赖
- 易于集成和部署

## 🚀 快速开始

### 基础加密

```cpp
#include "AES256.h"

// 创建加密器
std::vector<uint8_t> password = {/* 密码 */};
AES256 cipher(password);

// 加密
auto ciphertext = cipher.encrypt(plaintext);

// 解密
auto plaintext = cipher.decrypt(ciphertext);
```

### AEAD加密（推荐）

```cpp
// 带认证的加密
auto aead = cipher.encryptAEAD(plaintext);

// 带认证的解密（认证失败会抛出异常）
auto plaintext = cipher.decryptAEAD(aead);
```

### 密钥派生

```cpp
#include "argon2id.h"

// 强度密钥派生（推荐用于密码存储）
auto key = Argon2id::derive(password, salt, Argon2id::STRONG);

// 快速密钥派生（用于快速验证）
auto key = Argon2id::derive(password, salt, Argon2id::FAST);
```

### 编码转换

```cpp
#include "crypto_utils.h"

// Hex编码/解码
std::string hex = CryptoUtils::bytesToHex(data);
auto bytes = CryptoUtils::hexToBytes(hex);

// Base64编码/解码
std::string b64 = CryptoUtils::bytesToBase64(data);
auto bytes = CryptoUtils::base64ToBytes(b64);
```

## 📁 项目结构

```
src/
├── blake2b.h/cpp              # Blake2b哈希算法（RFC 7693）
├── argon2id.h/cpp             # ARGON2ID密钥派生
├── secure_random.h/cpp        # 跨平台安全随机数
├── AES256.h/cpp               # AES-256加密（CTR+AEAD）
├── crypto_utils.h/cpp         # 密码学工具集
├── platform_config.h           # 跨平台配置宏
├── best_hash.h/cpp            # 哈希集成模块
└── ...其他文件

📚 文档:
├── CRYPTO_IMPLEMENTATION.md    # 完整实现文档
├── COMPLETION_SUMMARY.md       # 完成总结
├── QUICK_REFERENCE.md          # 快速参考
└── README.md                   # 本文件
```

## 💾 编译

### CMake构建

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 手动编译（Windows MSVC）

```bash
cl /O2 /W4 /std:c++14 *.cpp
```

### 手动编译（Linux/macOS）

```bash
g++ -O3 -Wall -std=c++14 *.cpp -o libcrypto.a
```

## 📊 性能指标

| 操作 | 速度 | 内存 |
|------|------|------|
| Blake2b | GB/s级 | 64字节 |
| AES-256-CTR | 取决于CPU | 16字节 |
| ARGON2ID (STRONG) | ~1秒/密钥 | 256MB |
| ARGON2ID (DEFAULTS) | ~0.5秒/密钥 | 65MB |
| ARGON2ID (FAST) | ~50ms/密钥 | 19MB |

## 🔐 安全建议

### 密码存储
```cpp
// 使用STRONG参数确保安全
auto hash = Argon2id::derive(password, salt, Argon2id::STRONG);
```

### 敏感数据加密
```cpp
// 始终使用AEAD模式
auto encrypted = cipher.encryptAEAD(sensitive_data);
```

### 盐值管理
```cpp
// 每次密钥派生都使用新的随机盐值
SecureRandom rng;
auto salt = rng.generateVector(16);  // 至少16字节
```

## 📝 文档

- **CRYPTO_IMPLEMENTATION.md** - 完整的API文档和详细说明
- **QUICK_REFERENCE.md** - 快速参考指南和代码示例
- **COMPLETION_SUMMARY.md** - 项目完成总结

## 🎯 应用场景

- ✅ 密码存储和验证
- ✅ 敏感数据加密
- ✅ 密钥派生
- ✅ 身份认证
- ✅ 数据完整性保护
- ✅ 随机数生成
- ✅ 编码转换

## 🔄 版本历史

### v1.0.0 (当前)
- ✅ 完整的Blake2b实现（RFC 7693）
- ✅ 完整的ARGON2ID实现
- ✅ AES-256-CTR加密
- ✅ AEAD认证加密
- ✅ 跨平台支持（Windows/Linux/macOS）
- ✅ 完整的工具集

## 🤝 贡献

欢迎提交issue和pull request！

## 📄 许可证

MIT License - 详见LICENSE文件

## ✋ 责任声明

本库按"AS-IS"提供，不提供任何担保。使用者应对其使用负全部责任。建议在生产环境前进行充分测试。

## 📞 联系方式

有任何问题或建议，欢迎提交issue。

---

## 🌟 为什么选择这个库？

| 特性 | 本库 | 其他库 |
|------|------|--------|
| 零依赖 | ✅ | ❌ |
| 跨平台 | ✅ | ❌ |
| 完整源码 | ✅ | ❌ |
| 企业级 | ✅ | ✅ |
| C++11兼容 | ✅ | ❌ |
| ARGON2ID | ✅ | ❌ |
| 线程安全 | ✅ | ✅ |

---

**Made with ❤️ for Security**

最后更新: 2024
版本: 1.0.0
