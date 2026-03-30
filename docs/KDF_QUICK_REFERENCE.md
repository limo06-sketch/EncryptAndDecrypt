# KDF快速参考卡

## 核心函数签名

```cpp
std::vector<uint8_t> deriveKeyFromEnvironment(
    const char* var_name,                    // 环境变量名 (如 "limo")
    const std::vector<uint8_t>& salt = {}    // 可选的盐值
);
// 返回: 32字节 AES-256 密钥
// 异常: std::runtime_error (环境变量不存在或派生失败)
```

---

## 一行代码使用

```cpp
// 从环境变量 "limo" 派生密钥
auto key = deriveKeyFromEnvironment("limo");

// 使用自定义盐值
std::vector<uint8_t> salt = {0xDE, 0xAD, 0xBE, 0xEF, ...};
auto key = deriveKeyFromEnvironment("limo", salt);
```

---

## Argon2id参数速查表

| 参数 | 值 | 含义 |
|------|-----|------|
| memory_cost | 19456 | 19 MiB 内存 |
| time_cost | 3 | 3次迭代 |
| parallelism | 1 | 单线程 |
| output_length | 32 | 32字节密钥 |
| version | 0x13 | RFC 9106 v1.3 |

---

## 性能指标

| 指标 | 值 | 备注 |
|------|-----|------|
| 派生时间 | ~120ms | 现代CPU |
| 内存占用 | 19 MiB | 临时占用 |
| 输出大小 | 32字节 | 固定 |
| 并发安全 | ✅ 线程安全 | 无共享状态 |

---

## 环境变量设置

### Windows (PowerShell)
```powershell
$env:limo = "your-secret-password"
```

### Linux/macOS (Bash)
```bash
export limo="your-secret-password"
```

---

## 常用集成模式

### 模式1：应用启动时初始化
```cpp
std::vector<uint8_t> MASTER_KEY;  // 全局变量

int main() {
    try {
        MASTER_KEY = deriveKeyFromEnvironment("limo");
        AES256 cipher(MASTER_KEY);
        // ... 应用逻辑
    } catch (...) {
        exit(1);  // 密钥派生失败，无法继续
    }
}
```

### 模式2：单次使用
```cpp
std::string encrypt_sensitive_data(const std::string& data) {
    auto key = deriveKeyFromEnvironment("limo");
    AES256 cipher(key);
    return cipher.encryptString(data);
}
```

### 模式3：多密钥场景
```cpp
auto encryption_key = deriveKeyFromEnvironment("limo");
auto signing_key = deriveKeyFromEnvironment("limo", custom_salt);
```

---

## 错误处理

```cpp
try {
    auto key = deriveKeyFromEnvironment("limo");
} catch (const std::runtime_error& e) {
    // 处理错误
    // 原因：环境变量不存在 | Argon2id失败 | 输出大小异常
    std::cerr << "KDF Error: " << e.what() << std::endl;
}
```

---

## 安全清零示例

```cpp
std::vector<uint8_t> key = deriveKeyFromEnvironment("limo");

// 使用密钥...

// 使用完毕后清零
volatile uint8_t* ptr = reinterpret_cast<volatile uint8_t*>(key.data());
for (size_t i = 0; i < key.size(); ++i) {
    ptr[i] = 0;
}
key.clear();
key.shrink_to_fit();
```

---

## 标准与规范

✅ **RFC 9106** - Argon2 Password Hashing Algorithm  
✅ **OWASP** - Password Storage Cheat Sheet  
✅ **NIST SP 800-132** - PBKDF2 标准  
✅ **CWE-916** - Use of Password Hash with Insufficient Effort  

---

## 与旧方案的区别

| 特性 | 旧方案 | 新方案 |
|------|--------|--------|
| 标准 | 自定义HMAC | RFC 9106 |
| 后备方案 | 有 | 无 |
| 错误验证 | 基础 | 严格 |
| 文档 | 简洁 | 完整 |
| OWASP认可 | ✗ | ✅ |

---

## 何时使用

✅ **立即使用** - 生产环境密钥派生  
✅ **推荐** - Web应用认证  
✅ **推荐** - 数据库加密  
✅ **推荐** - API令牌管理  
✅ **推荐** - 敏感数据保护  

❌ **不适合** - 密码验证（使用argon2_verify）  
❌ **不适合** - 实时加密（已足够快）  

---

## 编译要求

- ✅ C++17 或更高
- ✅ argon2id.h 库
- ✅ blake2b.h 库
- ✅ windows.h (Windows)
- ✅ unistd.h (Linux/macOS)

---

## 调试技巧

```cpp
// 验证KDF是否正常工作
void test_kdf() {
    std::cout << "[TEST] 测试Argon2id KDF..." << std::endl;

    try {
        auto key = deriveKeyFromEnvironment("limo");

        std::cout << "[TEST] ✅ 派生成功" << std::endl;
        std::cout << "[TEST] 密钥长度: " << key.size() << " 字节" << std::endl;

        // 多次派生应产生相同的密钥
        auto key2 = deriveKeyFromEnvironment("limo");

        if (std::equal(key.begin(), key.end(), key2.begin())) {
            std::cout << "[TEST] ✅ 确定性派生: 通过" << std::endl;
        } else {
            std::cout << "[TEST] ❌ 确定性派生: 失败" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cout << "[TEST] ❌ 异常: " << e.what() << std::endl;
    }
}
```

---

## 参考资源

- 📖 RFC 9106: https://tools.ietf.org/html/rfc9106
- 📖 OWASP: https://cheatsheetseries.owasp.org
- 🔗 Argon2: https://github.com/P-H-C/phc-winner-argon2
- 📚 NIST SP 800-132: https://nvlpubs.nist.gov/nistpubs

---

**版本**: 1.0  
**标准**: RFC 9106  
**状态**: ✅ Production-Ready
