# C++ 加密库 - 完整实现文档

## 概述
这是一个使用标准C++库（仅依赖STL）实现的跨平台加密库，不依赖任何第三方加密库。

## 组件清单

### 1. 核心加密模块

#### Blake2b (blake2b.h / blake2b.cpp)
- **描述**: 完整的Blake2b哈希函数实现
- **符合**: RFC 7693规范
- **特性**:
  - 输出大小：1-64字节（默认64字节）
  - 支持密钥（用于HMAC）
  - 12轮混合操作
  - 完整的参数化支持
- **用途**: ARGON2ID的核心哈希函数

#### ARGON2ID (argon2id.h / argon2id.cpp)
- **描述**: ARGON2ID内存困难型密钥派生函数
- **特性**:
  - 三个参数预设：DEFAULTS, STRONG, FAST
  - 内存困难化防护
  - 时间成本参数
  - 并行度支持
  - 安全的内存清零
- **参数预设**:
  ```cpp
  // DEFAULTS: 65MB内存, 3次迭代, 4个线程
  // STRONG: 256MB内存, 4次迭代, 4个线程（推荐用于关键密钥）
  // FAST: 19MB内存, 2次迭代, 1个线程（快速模式）
  ```
- **用途**: AES256密钥派生、密码哈希、盐值生成

#### AES256 (AES256.h / AES256.cpp)
- **描述**: 完整的AES-256加密实现
- **模式**: CTR（计数器模式）+ AEAD
- **特性**:
  - 256位密钥
  - 32字节（256位）IV
  - CTR模式防护（防止填充谕言攻击）
  - 加密-认证（Encrypt-then-MAC）
  - 密钥扩展（14轮）
  - 恒定时间操作

#### SecureRandom (secure_random.h / secure_random.cpp)
- **描述**: 密码学安全的随机数生成器
- **引擎**: MT19937-64（Mersenne Twister）
- **特性**:
  - 线程安全（使用mutex）
  - 多熵源初始化
  - 跨平台支持（Windows/Linux/macOS）
  - 生成64位和32位随机数
- **用途**: IV生成、盐值生成、密钥扩展

### 2. 工具模块

#### CryptoUtils (crypto_utils.h / crypto_utils.cpp)
- **编码转换**:
  - 十六进制编码/解码
  - Base64编码/解码
  - 字符串<->字节向量转换
- **安全操作**:
  - 恒定时间字节比较
  - 安全内存清零
  - Base64和十六进制验证
- **平台信息**: 获取运行平台详情

#### PlatformConfig (platform_config.h)
- **编译器检查**: MSVC, Clang, GCC
- **操作系统检查**: Windows, Linux, macOS, Unix
- **字节序检查**: 大端/小端检测
- **位宽检查**: 32位/64位检测
- **条件编译宏**:
  - ALIGN(n): 内存对齐
  - FORCE_INLINE: 强制内联
  - THREAD_LOCAL: 线程本地存储
  - EXPORT_SYMBOL: DLL导出
  - 等等...

## 使用示例

### 1. 基础加密解密

```cpp
#include "AES256.h"

// 创建加密器
std::vector<uint8_t> password = {/* ... */};
AES256 cipher(password);

// 加密
std::vector<uint8_t> plaintext = {/* ... */};
auto ciphertext = cipher.encrypt(plaintext);

// 解密
auto decrypted = cipher.decrypt(ciphertext);
```

### 2. AEAD加密（推荐用于生产环境）

```cpp
// 加密（带认证）
auto aead_encrypted = cipher.encryptAEAD(plaintext);

// 解密（验证认证标签）
auto aead_decrypted = cipher.decryptAEAD(aead_encrypted);
// 如果认证失败会抛出异常
```

### 3. 密钥派生

```cpp
#include "argon2id.h"

std::vector<uint8_t> password = {/* ... */};
std::vector<uint8_t> salt = {/* ... */};

// 使用高强度参数
auto key = Argon2id::derive(
    password, 
    salt, 
    Argon2id::STRONG
);
```

### 4. 生成安全随机数

```cpp
#include "secure_random.h"

SecureRandom rng;
auto random_bytes = rng.generateVector(32);
uint64_t random_num = rng.generate64();
```

### 5. 编码转换

```cpp
#include "crypto_utils.h"

// 十六进制
std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
std::string hex = CryptoUtils::bytesToHex(data);  // "48656c6c6f"

// Base64
std::string base64 = CryptoUtils::bytesToBase64(data);  // "SGVsbG8="
```

## 性能特性

### 加密速度
- AES-256-CTR: 高性能（取决于CPU）
- 可通过FORCE_INLINE宏优化

### 内存使用
- Blake2b: 固定64字节状态
- ARGON2ID (DEFAULTS): ~65MB
- ARGON2ID (STRONG): ~256MB
- ARGON2ID (FAST): ~19MB

### 安全特性
- 恒定时间操作防止时序攻击
- 安全的内存清零（使用volatile指针）
- CTR模式防止填充谕言攻击
- ARGON2ID内存困难化防护GPU攻击
- 时间成本参数防止暴力破解

## 跨平台支持

### 操作系统
- ✅ Windows (Visual Studio 2015+)
- ✅ Linux (GCC 4.9+, Clang 3.8+)
- ✅ macOS (Clang 3.8+)
- ✅ 其他POSIX系统

### C++标准
- ✅ C++11 最低要求
- ✅ C++14 推荐
- ✅ C++17 及以上完全支持

## 编译

### 使用CMake
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### 手动编译（Windows MSVC）
```bash
cl /O2 /W4 *.cpp
```

### 手动编译（GCC/Clang）
```bash
g++ -O3 -Wall -std=c++14 *.cpp
```

## 依赖

- 标准C++库（STL）：vector, mutex, random, chrono等
- 无第三方加密库依赖

## 安全建议

1. **密钥管理**
   - 始终使用ARGON2ID (STRONG)派生关键密钥
   - 使用足够长的密码（≥16字符）
   - 使用足够长的盐值（≥16字节）

2. **加密选择**
   - 使用encryptAEAD()而不是encrypt()
   - AEAD提供完整性验证保护

3. **内存处理**
   - 敏感数据使用vector<uint8_t>
   - 利用AES256和Argon2id的自动清零机制
   - 必要时调用CryptoUtils::secureMemZero()

4. **参数选择**
   ```cpp
   // 密码存储
   auto hash = Argon2id::derive(pwd, salt, Argon2id::STRONG);
   
   // 快速验证
   auto key = Argon2id::derive(pwd, salt, Argon2id::FAST);
   ```

## 文件清单

| 文件 | 行数 | 功能 |
|------|------|------|
| blake2b.h/.cpp | ~300 | Blake2b哈希算法 |
| argon2id.h/.cpp | ~350 | ARGON2ID密钥派生 |
| secure_random.h/.cpp | ~150 | 安全随机数生成 |
| AES256.h/.cpp | ~800 | AES-256加密 |
| crypto_utils.h/.cpp | ~250 | 编码和工具函数 |
| platform_config.h | ~150 | 跨平台配置 |

## 许可证

见项目根目录LICENSE文件

## 贡献

欢迎提交问题和拉取请求

## 更新历史

### v1.0.0
- 完整的Blake2b实现（RFC 7693）
- 完整的ARGON2ID实现
- AES-256-CTR加密
- AEAD认证加密
- 跨平台支持
- 完整的密码学工具集
