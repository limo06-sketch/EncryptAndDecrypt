# 👨‍💻 开发者指南

## 目录结构说明

### 核心加密模块

#### blake2b.h / blake2b.cpp
**用途**: Blake2b密码学哈希算法

**关键函数**:
```cpp
static std::vector<uint8_t> hash(
    const void* input,
    size_t input_len,
    size_t output_len = HASH_SIZE,
    const void* key = nullptr,
    size_t key_len = 0
);
```

**内部实现**:
- `init()` - 初始化状态
- `update()` - 处理输入数据
- `final()` - 生成最终哈希
- `compress()` - 核心压缩函数
- `g()` - Blake2b的G函数（列混合和对角混合）

**关键常量**:
- `BLOCK_SIZE = 128` - 块大小
- `HASH_SIZE = 64` - 默认输出大小
- `IV[8]` - 初始化向量
- `SIGMA[12][16]` - 消息调度排列

---

#### argon2id.h / argon2id.cpp
**用途**: ARGON2ID内存困难型密钥派生函数

**关键函数**:
```cpp
static std::vector<uint8_t> derive(
    const std::vector<uint8_t>& password,
    const std::vector<uint8_t>& salt,
    const Parameters& params = DEFAULTS,
    const std::vector<uint8_t>& associated_data = {}
);
```

**参数结构**:
```cpp
struct Parameters {
    uint32_t memory_cost;      // 内存成本（单位：KiB）
    uint32_t time_cost;        // 时间成本（迭代次数）
    uint32_t parallelism;      // 并行度（线程数）
    uint32_t output_length;    // 输出长度（字节）
    uint8_t version;           // 版本号
};
```

**内部实现**:
- `initialHash()` - 生成初始哈希
- `fillFirstBlocks()` - 填充初始内存块
- `fillMemory()` - 多轮内存填充
- `finalizeHash()` - 生成最终输出

**内存块大小**: 128 x 64位 = 1024字节

---

#### secure_random.h / secure_random.cpp
**用途**: 密码学安全的随机数生成

**关键函数**:
```cpp
void generate(uint8_t* buffer, size_t size);
std::vector<uint8_t> generateVector(size_t size);
uint64_t generate64();
uint32_t generate32();
```

**引擎**: MT19937-64 (Mersenne Twister)

**熵源**:
1. std::random_device（主要）
2. 高分辨率时间戳
3. 线程ID

---

#### AES256.h / AES256.cpp
**用途**: AES-256加密（CTR模式 + AEAD）

**关键函数**:
```cpp
std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext);
std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext);
std::vector<uint8_t> encryptAEAD(const std::vector<uint8_t>& plaintext);
std::vector<uint8_t> decryptAEAD(const std::vector<uint8_t>& aead);
```

**内部实现**:
- `keyExpansion()` - 密钥扩展（14轮）
- `encryptBlock()` / `decryptBlock()` - AES核心
- `subBytes()` / `invSubBytes()` - 字节替换
- `shiftRows()` / `invShiftRows()` - 行移
- `mixColumns()` / `invMixColumns()` - 列混合
- `addRoundKey()` - 轮密钥加
- `deriveKey()` - ARGON2ID密钥派生

**密钥派生参数**:
- 使用ARGON2ID::STRONG
- 输出64字节（32字节加密密钥 + 32字节MAC密钥）

---

#### crypto_utils.h / crypto_utils.cpp
**用途**: 密码学工具集

**主要功能**:
1. **编码转换**
   - `bytesToHex()` / `hexToBytes()`
   - `bytesToBase64()` / `base64ToBytes()`
   - `bytesToString()` / `stringToBytes()`

2. **安全操作**
   - `constantTimeCompare()` - 恒定时间比较
   - `secureMemZero()` - 安全内存清零

3. **验证函数**
   - `isValidHex()` - 验证十六进制字符串
   - `isValidBase64()` - 验证Base64字符串

4. **信息获取**
   - `getPlatformInfo()` - 获取平台信息

---

#### platform_config.h
**用途**: 跨平台配置和检查宏

**编译器检查**:
- `COMPILER_MSVC`
- `COMPILER_GCC`
- `COMPILER_CLANG`

**操作系统检查**:
- `OS_WINDOWS`
- `OS_LINUX`
- `OS_MACOS`
- `OS_UNIX`

**有用的宏**:
- `FORCE_INLINE` - 强制内联
- `ALIGN(n)` - 内存对齐
- `THREAD_LOCAL` - 线程本地存储
- `EXPORT_SYMBOL` - 符号导出

---

## 关键设计决策

### 1. 字节序处理
```cpp
// 小端加载/存储（Intel/AMD处理器标准）
static inline uint64_t load64_le(const uint8_t* p);
static inline void store64_le(uint8_t* p, uint64_t v);
```

### 2. 恒定时间操作
```cpp
// 时间比较（防时序攻击）
bool constantTimeCompare(const std::vector<uint8_t>& a, 
                         const std::vector<uint8_t>& b);
```

### 3. 安全内存清零
```cpp
// 使用volatile指针防止编译器优化
volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
```

### 4. 线程安全
```cpp
// SecureRandom使用mutex保护
std::lock_guard<std::mutex> lock(mtx);
```

## 添加新功能的指南

### 添加新算法

1. **创建头文件** (e.g., `src/NewAlgo.h`)
```cpp
#ifndef NEW_ALGO_H
#define NEW_ALGO_H

#include <vector>
#include <cstdint>
#include "platform_config.h"  // 包含平台配置

class NewAlgo {
public:
    // 公开接口
    static std::vector<uint8_t> process(const std::vector<uint8_t>& input);
    
private:
    // 私有实现
    static void coreAlgorithm(/* ... */);
};

#endif
```

2. **实现源文件** (e.g., `src/NewAlgo.cpp`)
```cpp
#include "NewAlgo.h"
#include <cstring>
#include <stdexcept>

// 实现...
```

3. **编写单元测试**
```cpp
#include "NewAlgo.h"

void test_NewAlgo() {
    auto result = NewAlgo::process(input);
    assert(result == expected);
}
```

4. **更新文档**
   - 在CRYPTO_IMPLEMENTATION.md中添加说明
   - 在QUICK_REFERENCE.md中添加示例

## 性能优化建议

### 1. 避免不必要的内存分配
```cpp
// 不好
for (int i = 0; i < n; i++) {
    std::vector<uint8_t> temp(1024);  // 每次分配
    // 使用temp
}

// 好
std::vector<uint8_t> temp(1024);
for (int i = 0; i < n; i++) {
    // 重用temp
}
```

### 2. 使用引用避免复制
```cpp
// 不好
void process(std::vector<uint8_t> data);  // 复制

// 好
void process(const std::vector<uint8_t>& data);  // 引用
```

### 3. 预分配内存容量
```cpp
std::vector<uint8_t> result;
result.reserve(expected_size);  // 避免多次重新分配
```

### 4. 使用move语义
```cpp
return std::move(result);  // C++11+
```

## 安全最佳实践

### 1. 参数验证
```cpp
if (password.empty()) {
    throw std::invalid_argument("Password cannot be empty");
}
```

### 2. 边界检查
```cpp
if (index >= data.size()) {
    throw std::out_of_range("Index out of bounds");
}
```

### 3. 资源清理
```cpp
// 使用RAII自动清理
std::vector<uint8_t> sensitive;
// ... 使用 ...
// 离开作用域时自动调用析构函数
```

### 4. 异常安全
```cpp
try {
    auto result = operation();
    return result;
} catch (const std::exception& e) {
    // 清理资源
    throw;  // 重新抛出
}
```

## 调试建议

### 1. 启用调试符号
```bash
g++ -g -O0 -Wall *.cpp  # 调试构建
```

### 2. 内存检查（valgrind）
```bash
valgrind --leak-check=full ./program
```

### 3. 地址清理器（AddressSanitizer）
```bash
g++ -fsanitize=address -g *.cpp
```

### 4. 输出中间结果
```cpp
#ifdef DEBUG
    std::cerr << "Debug info: " << value << std::endl;
#endif
```

## 编译器兼容性

### MSVC (Windows)
```cpp
#pragma warning(disable: 4996)  // 禁用不安全警告
```

### GCC (Linux)
```cpp
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
```

### Clang (macOS)
```cpp
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-result"
```

## 版本控制建议

### 提交消息格式
```
[类型] 简短描述

详细说明
- 实现细节1
- 实现细节2

关闭: #123
```

类型:
- feat: 新功能
- fix: 修复
- docs: 文档
- refactor: 重构
- perf: 性能优化
- test: 测试

### 分支管理
- `master` - 稳定版本
- `develop` - 开发分支
- `feature/*` - 功能分支
- `bugfix/*` - 修复分支

## 继续改进方向

### 短期（1-2个月）
- [ ] 添加HMAC-SHA256
- [ ] 添加单元测试框架
- [ ] 性能基准测试

### 中期（3-6个月）
- [ ] 椭圆曲线加密 (ECDSA)
- [ ] RSA支持
- [ ] 密钥存储机制

### 长期（6-12个月）
- [ ] TLS/SSL实现
- [ ] X.509证书支持
- [ ] 硬件安全模块集成

## 常见陷阱及解决方案

| 问题 | 原因 | 解决方案 |
|------|------|---------|
| 密钥泄露 | 未清零内存 | 使用secureMemZero() |
| 时序攻击 | 依赖于数据的执行时间 | 使用恒定时间操作 |
| 弱随机数 | 不合适的RNG | 使用SecureRandom |
| 填充谕言 | CBC模式下的问题 | 使用CTR或GCM |
| 内存溢出 | 边界检查不足 | 添加边界验证 |

## 资源链接

- [RFC 7693 - Blake2](https://tools.ietf.org/html/rfc7693)
- [ARGON2 论文](https://www.password-hashing.net)
- [FIPS 197 - AES](https://csrc.nist.gov/publications/detail/fips/197/final)
- [OWASP 密码学备忘单](https://cheatsheetseries.owasp.org)

---

**最后更新**: 2024
**维护者**: 安全团队
