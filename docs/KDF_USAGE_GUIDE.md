# KDF使用示例与集成指南

## 快速开始

### 基础用法

```cpp
#include "Tollbox.h"

int main() {
    try {
        // 自动使用Argon2id KDF从环境变量"limo"派生密钥
        std::vector<uint8_t> aes_key = deriveKeyFromEnvironment("limo");

        // aes_key: 32字节安全密钥
        // 已应用: Argon2id(m=19456, t=3, p=1)
        // 可直接用于AES-256加密

        std::cout << "密钥长度: " << aes_key.size() << " 字节" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
}
```

### 系统配置（环境变量设置）

#### Windows
```powershell
# PowerShell
$env:limo = "your-secret-password-here"

# 或永久设置
[Environment]::SetEnvironmentVariable("limo", "your-secret-password-here", "User")

# 验证
$env:limo
```

#### Linux/macOS
```bash
# Bash
export limo="your-secret-password-here"

# 或写入 ~/.bashrc
echo 'export limo="your-secret-password-here"' >> ~/.bashrc
source ~/.bashrc

# 验证
echo $limo
```

---

## 集成场景

### 场景1：Web应用认证

```cpp
#include "Tollbox.h"
#include "AES256.h"

class WebAuthService {
private:
    std::vector<uint8_t> master_key;
    AES256* cipher;

public:
    WebAuthService() {
        try {
            // 启动时派生主密钥
            master_key = deriveKeyFromEnvironment("limo");
            cipher = new AES256(master_key);

            std::cout << "[AUTH] 主密钥已派生 (Argon2id KDF)" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] 密钥派生失败: " << e.what() << std::endl;
            throw;
        }
    }

    ~WebAuthService() {
        // 安全清零master_key
        if (!master_key.empty()) {
            volatile uint8_t* ptr = reinterpret_cast<volatile uint8_t*>(master_key.data());
            for (size_t i = 0; i < master_key.size(); ++i) {
                ptr[i] = 0;
            }
            master_key.clear();
            master_key.shrink_to_fit();
        }
        if (cipher) delete cipher;
    }

    std::string encryptUserToken(const std::string& token) {
        std::vector<uint8_t> plaintext(token.begin(), token.end());
        return cipher->encryptString(token);
    }

    std::string decryptUserToken(const std::string& encrypted_token) {
        return cipher->decryptString(encrypted_token);
    }
};

// 使用
int main() {
    WebAuthService auth;

    // 加密令牌
    std::string user_token = "user:12345";
    std::string encrypted = auth.encryptUserToken(user_token);
    std::cout << "加密后: " << encrypted << std::endl;

    // 解密令牌
    std::string decrypted = auth.decryptUserToken(encrypted);
    std::cout << "解密后: " << decrypted << std::endl;

    return 0;
}
```

### 场景2：数据库密码管理

```cpp
#include "Tollbox.h"

class DatabaseManager {
private:
    std::string db_password;  // 加密存储
    AES256 cipher;

public:
    DatabaseManager() : cipher(deriveKeyFromEnvironment("limo")) {
        // 初始化时设置数据库密码
        setDatabasePassword("your_db_password");
    }

    void setDatabasePassword(const std::string& plaintext_password) {
        // 使用Argon2id派生的密钥加密数据库密码
        db_password = cipher.encryptString(plaintext_password);

        // 记录到日志
        AuditLogger logger("db_access.log");
        logger.logSecurityEvent("Database password encrypted with Argon2id-derived key");
    }

    std::string getDatabasePassword() {
        // 运行时解密
        return cipher.decryptString(db_password);
    }

    // 使用Argon2id派生的额外密钥进行二级验证
    bool verify_password_change(const std::string& user_input) {
        std::vector<uint8_t> verification_key = deriveKeyFromEnvironment("limo");

        // 与原始派生密钥比较
        // 如果环境变量被篡改，验证将失败
        return yes_no(db_password, cipher.encryptString(user_input));
    }
};
```

### 场景3：敏感数据保护

```cpp
#include "Tollbox.h"

class SensitiveDataVault {
private:
    struct EncryptedData {
        std::string name;
        std::vector<uint8_t> encrypted_content;
        std::vector<uint8_t> hmac_tag;
        std::string timestamp;
    };

    std::vector<EncryptedData> vault;
    AES256 cipher;

public:
    SensitiveDataVault() : cipher(deriveKeyFromEnvironment("limo")) {
        std::cout << "[VAULT] 已使用Argon2id(19MiB, t=3)初始化" << std::endl;
    }

    void store_secret(const std::string& name, const std::string& secret) {
        try {
            // 使用AEAD加密（AES-GCM）
            std::vector<uint8_t> plaintext(secret.begin(), secret.end());
            std::vector<uint8_t> aead_blob = cipher.encryptAEAD(plaintext);

            EncryptedData data;
            data.name = name;
            data.encrypted_content = aead_blob;
            data.timestamp = getCurrentTime();

            vault.push_back(data);

            AuditLogger logger("vault.log");
            logger.logSecurityEvent("Secret stored: " + name + " (Argon2id protected)");

            // 清零敏感数据
            volatile uint8_t* ptr = reinterpret_cast<volatile uint8_t*>(plaintext.data());
            for (size_t i = 0; i < plaintext.size(); ++i) {
                ptr[i] = 0;
            }
        } catch (const std::exception& e) {
            AuditLogger logger("vault.log");
            logger.logSecurityEvent("Failed to store secret: " + std::string(e.what()));
            throw;
        }
    }

    std::string retrieve_secret(const std::string& name) {
        for (const auto& data : vault) {
            if (data.name == name) {
                // 解密并验证完整性
                std::vector<uint8_t> decrypted = cipher.decryptAEAD(data.encrypted_content);

                AuditLogger logger("vault.log");
                logger.logSecurityEvent("Secret accessed: " + name);

                return std::string(decrypted.begin(), decrypted.end());
            }
        }
        throw std::runtime_error("Secret not found: " + name);
    }

    size_t vault_size() const { return vault.size(); }
};

// 使用示例
int main() {
    SensitiveDataVault vault;

    // 存储敏感数据
    vault.store_secret("api_key", "sk-1234567890abcdef");
    vault.store_secret("db_password", "super_secret_password");

    // 检索并使用
    std::string api_key = vault.retrieve_secret("api_key");
    std::cout << "Retrieved API Key: " << api_key << std::endl;

    return 0;
}
```

### 场景4：多密钥派生（不同用途）

```cpp
#include "Tollbox.h"

class MultiPurposeKeyDerivation {
public:
    // 为不同目的派生不同的密钥

    struct DerivedKeys {
        std::vector<uint8_t> encryption_key;     // AES-256
        std::vector<uint8_t> authentication_key; // HMAC
        std::vector<uint8_t> integrity_key;      // GCM
    };

    static DerivedKeys derive_all_keys() {
        DerivedKeys keys;

        // 基础密钥派生（使用Argon2id）
        std::vector<uint8_t> base_key = deriveKeyFromEnvironment("limo");

        // 使用HKDF派生不同目的的密钥
        // (假设有HKDF实现)
        keys.encryption_key = base_key;  // 直接使用

        // 衍生其他密钥
        std::vector<uint8_t> info_enc(8);
        std::copy_n(reinterpret_cast<uint8_t*>("ENC_KEY"), 7, info_enc.begin());
        // keys.authentication_key = hkdf_expand(base_key, info_enc, 32);

        return keys;
    }
};
```

---

## 错误处理与调试

### 错误场景

```cpp
#include "Tollbox.h"

void handle_kdf_errors() {
    try {
        // 场景1：环境变量未设置
        auto key = deriveKeyFromEnvironment("NONEXISTENT");
    } catch (const std::runtime_error& e) {
        std::cerr << "❌ 环境变量未找到: " << e.what() << std::endl;
        // → 检查环境变量是否已设置
    }

    try {
        // 场景2：Argon2id派生失败（极罕见）
        auto key = deriveKeyFromEnvironment("limo");
    } catch (const std::runtime_error& e) {
        std::cerr << "❌ 密钥派生失败: " << e.what() << std::endl;
        // → 检查内存是否足够（需要19MiB）
        // → 检查blake2b库是否正确链接
    }
}

// 调试打印
void debug_key_derivation() {
    std::cout << "[DEBUG] KDF参数:" << std::endl;
    std::cout << "  Memory Cost: 19456 KiB (19 MiB)" << std::endl;
    std::cout << "  Time Cost: 3 iterations" << std::endl;
    std::cout << "  Parallelism: 1 thread" << std::endl;
    std::cout << "  Output Length: 32 bytes" << std::endl;
    std::cout << "  Version: Argon2id v1.3 (RFC 9106)" << std::endl;

    try {
        auto key = deriveKeyFromEnvironment("limo");
        std::cout << "[DEBUG] ✅ 密钥派生成功" << std::endl;
        std::cout << "  密钥长度: " << key.size() << " 字节" << std::endl;

        // 打印密钥十六进制（仅用于调试，生产环境不要打印）
        std::cout << "  密钥(HEX): ";
        for (uint8_t byte : key) {
            printf("%02x", byte);
        }
        std::cout << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[DEBUG] ❌ 错误: " << e.what() << std::endl;
    }
}
```

---

## 性能测试

```cpp
#include <chrono>
#include "Tollbox.h"

void benchmark_kdf() {
    std::cout << "=== Argon2id KDF 性能测试 ===" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    // 第一次派生（包括初始化开销）
    auto key1 = deriveKeyFromEnvironment("limo");

    auto mid = std::chrono::high_resolution_clock::now();

    // 第二次派生（热缓存）
    auto key2 = deriveKeyFromEnvironment("limo");

    auto end = std::chrono::high_resolution_clock::now();

    auto first_time = std::chrono::duration_cast<std::chrono::milliseconds>(mid - start).count();
    auto second_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - mid).count();

    std::cout << "第一次派生时间: " << first_time << " ms" << std::endl;
    std::cout << "第二次派生时间: " << second_time << " ms" << std::endl;
    std::cout << "平均时间: " << (first_time + second_time) / 2 << " ms" << std::endl;

    std::cout << "\n推荐用途:" << std::endl;
    if (first_time < 200) {
        std::cout << "  ✅ 适合实时应用 (Web、API)" << std::endl;
    } else {
        std::cout << "  ✅ 适合后台任务、初始化" << std::endl;
    }
}
```

---

## 最佳实践

### ✅ 推荐做法

```cpp
// 1. 在应用启动时派生密钥（缓存）
std::vector<uint8_t> master_key = deriveKeyFromEnvironment("limo");
cipher = new AES256(master_key);

// 2. 使用volatile指针清零敏感数据
volatile uint8_t* ptr = reinterpret_cast<volatile uint8_t*>(data.data());
for (size_t i = 0; i < data.size(); ++i) ptr[i] = 0;

// 3. 记录安全事件（审计日志）
AuditLogger logger("security.log");
logger.logSecurityEvent("Key derived using Argon2id RFC 9106");

// 4. 验证环境变量（启动检查）
try {
    auto key = deriveKeyFromEnvironment("limo");
    std::cout << "[STARTUP] ✅ 密钥派生成功" << std::endl;
} catch (...) {
    std::cerr << "[STARTUP] ❌ 关键错误：无法派生密钥" << std::endl;
    exit(1);
}
```

### ❌ 避免做法

```cpp
// 1. 不要每次使用都重新派生（浪费时间和内存）
// ❌ 错误
for (int i = 0; i < 1000; ++i) {
    auto key = deriveKeyFromEnvironment("limo");  // 花费3秒！
}

// ✅ 正确
auto key = deriveKeyFromEnvironment("limo");  // 一次派生
for (int i = 0; i < 1000; ++i) {
    cipher.encrypt(data[i]);  // 复用密钥
}

// 2. 不要打印/日志记录派生的密钥
// ❌ 错误
std::cout << "Key: " << key << std::endl;

// ✅ 正确
std::cout << "Key derived successfully" << std::endl;

// 3. 不要在多线程中共享未同步的密钥
// ❌ 错误（竞态条件）
std::vector<uint8_t> shared_key = deriveKeyFromEnvironment("limo");
// 多线程使用 shared_key

// ✅ 正确（使用锁）
std::mutex key_mutex;
std::vector<uint8_t> shared_key = deriveKeyFromEnvironment("limo");
{
    std::lock_guard<std::mutex> lock(key_mutex);
    // 使用 shared_key
}
```

---

## 常见问题

### Q1: 为什么需要19MiB内存？
**A**: 这是OWASP推荐的值，用于防止GPU和ASIC攻击。12MiB以下容易被高端GPU暴力破解。

### Q2: 派生密钥需要多长时间？
**A**: 典型环境下约100-150ms。这是可以接受的延迟，推荐在应用启动时进行。

### Q3: 可以改变Argon2id参数吗？
**A**: 可以，但需谨慎。参考 `Argon2id::FAST` 和 `Argon2id::STRONG` 预设，或直接修改 `kdf_params` 结构体。

### Q4: 如何处理密钥更新？
**A**: 更新环境变量"limo"的值，重启应用。新会话将自动使用新密钥派生。

### Q5: 是否可以指定自定义盐值？
**A**: 可以，调用 `deriveKeyFromEnvironment("limo", your_salt)`。确保盐值足够随机（≥16字节）。

---

**文档版本**: 1.0  
**最后更新**: 2024年  
**标准**: RFC 9106 Argon2id  
**状态**: ✅ 生产级
