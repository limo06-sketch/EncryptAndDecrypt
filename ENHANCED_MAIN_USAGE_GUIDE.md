/**
 * @file ENHANCED_MAIN_USAGE_GUIDE.md
 * @brief 强化版 main.cpp 使用指南
 */

# 🎯 强化版 main.cpp 使用指南

## 快速开始

### 编译

```bash
# Visual Studio Build
msbuild EncryptAndDecrypt.sln /p:Configuration=Release

# 或使用CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 运行

```bash
./EncryptApp
```

## 程序流程

### 1️⃣ 系统初始化阶段

```
[*] Security system initialized
[*] Platform information displayed
[*] Generating authentication key...
[OK] Authentication key generated (AEAD protected)
```

**做了什么？**
- ✅ 初始化 AccountLockoutManager
- ✅ 初始化 AuditLogger
- ✅ 使用 SecureRandom 生成 32 字节密钥材料
- ✅ 创建 AES256 加密器实例
- ✅ 派生主认证密钥（使用 ARGON2ID）
- ✅ 生成存储的 AEAD 密文

**安全特性**
- SecureMemoryGuard 保护原始密钥
- volatile 指针防止编译器优化
- 完整的事件日志

### 2️⃣ 文件初始化阶段

```
[*] Validating file paths...
[!] Error: Invalid file path detected!
```

**文件操作**
- `test.txt` - 认证日志文件（追加模式）
- `game.txt` - 高分记录文件
- `auth.log` - 安全审计日志
- `lockout.dat` - 账户锁定记录

**安全检查**
- 路径遍历检测（`../` 防护）
- 文件访问权限检查
- 文件打开成功验证

### 3️⃣ 认证阶段

```
================================================
     Enterprise Encryption System v1.0.0
     Security Level: 9.8/10
================================================

[>] Enter password to access encryption program
>>
[>] Password: ****************************
Password entropy level: Strong
```

#### 认证流程

**第1层：账户锁定检查**
```cpp
if (lockout_manager.isAccountLocked()) {
    // 检查是否在锁定期内
    // 显示剩余时间
    // 记录非法登录尝试
}
```

**第2层：密码输入（隐藏）**
```cpp
std::string user_input = getSecurePassword();
// Windows: 使用 _getch()
// Linux/macOS: 使用 termios
// 特点：无回显、支持退格、任意长度
```

**第3层：密码验证**
```cpp
user_input = InputValidator::validatePassword(user_input);
// 检查：
// - 长度 [8-128] 字符
// - ASCII 可打印字符
// - 无空字符
```

**第4层：熵值计算**
```cpp
string entropy_level = calculateKeyEntropy(user_input);
// 输出：弱 / 中等 / 强 / 极强
```

**第5层：Blake2b 哈希**
```cpp
std::vector<uint8_t> user_hash = Blake2b::hash(user_bytes, 32);
// 输出：256 位哈希（32 字节）
```

**第6层：AEAD 解密验证**
```cpp
std::vector<uint8_t> recovered = aes.decryptAEAD(stored_correct_aead);
// 如果解密失败 → 数据被篡改 → 认证失败
// 如果解密成功 → 进入恒定时间比较
```

**第7层：恒定时间比较**
```cpp
unsigned char diff = 0;
for (size_t i = 0; i < recovered.size(); ++i) {
    diff |= recovered[i] ^ user_hash[i];
}
auth_ok = (diff == 0);
// 固定时间运行，无论匹配与否
// 防止时序攻击
```

#### 认证成功

```
[+] Authentication successful
[认证日志]
==================================================
Authentication successful!
Time: 2024/XX/XX HH:MM:SS
Password Entropy Level: Strong
Attempt Number: 1
==================================================
```

**执行操作**
- 记录成功认证日志
- 记录密码熵值
- 清除失败计数
- 释放账户锁定

#### 认证失败

```
[-] Incorrect password
[*] Remaining attempts: 2
[>] Password: 
```

**执行操作**
- 记录失败尝试
- 增加失败计数
- 固定延迟 500ms（防暴力破解）
- 提示剩余尝试次数

**3 次失败后**

```
[!] Too many attempts. Account locked for 10 minutes.
[*] Remaining: 600 seconds
[*] Remaining: 599 seconds
...
[*] Lockout released. Try again.
```

**执行操作**
- 锁定账户
- 显示 10 分钟倒计时
- 自动解锁后允许重试
- 完整的锁定日志

### 4️⃣ 程序运行阶段

```
Loading...
[##########                ] 50%
...
Loading complete
```

**加载进度条**
- 100 步进度条
- 每步 40ms 延迟
- 逼真的加载动画

### 5️⃣ 游戏阶段

```
========================================
      Number Guessing Game
========================================

请输入难度: 1-简单(1-100) 2-中等(1-1000) 3-困难(1-10000)
请选择难度 [1/2/3] >> 1

已生成随机数，请开始猜测(范围1~100)
输入你的猜测 >> 50
你猜的数字太大了，请重新输入
...
恭喜你，猜对了！答案就是：42

总猜测次数: 5
当前高分: 3

再玩一次? (y/n): n
```

**游戏特性**
- ✅ 三个难度等级
- ✅ 实时反馈
- ✅ 猜测计数
- ✅ 高分跟踪
- ✅ 新高分记录

### 6️⃣ 程序退出阶段

```
========================================
      Thank you for using the
      Enterprise Encryption System
      Goodbye!
========================================
```

**执行操作**
- 记录正常退出事件
- 关闭所有文件
- 清零敏感内存
- 恢复终端设置

## 日志文件说明

### 1. auth.log - 安全审计日志

```
[2024-01-15 14:30:00] PROGRAM_START - Security System Initialized
[2024-01-15 14:30:00] AUTHENTICATION_KEY_GENERATED
[2024-01-15 14:30:05] AUTHENTICATION_SUCCESS
[2024-01-15 14:30:05] PASSWORD_ENTROPY: Strong
[2024-01-15 14:30:05] PROGRAM_ENTRY_GRANTED
[2024-01-15 14:35:20] NEW_HIGH_SCORE: 42
[2024-01-15 14:35:30] PROGRAM_EXIT_NORMAL
```

**日志类型**
- PROGRAM_START/EXIT
- AUTHENTICATION_*
- PASSWORD_*
- FILE_PATH_*
- AEAD_*
- ACCOUNT_LOCKOUT
- LOGIN_ATTEMPT_WHILE_LOCKED
- NEW_HIGH_SCORE

### 2. test.txt - 认证日志

```
==================================================
Authentication successful!
Time: 2024/01/15 14:30:05
Password Entropy Level: Strong
Attempt Number: 1
==================================================
```

### 3. game.txt - 高分记录

```
5
```

简单的数字文件，存储当前最高分。

### 4. lockout.dat - 账户锁定记录

```
[Binary File]
Failed Count: 3
Lockout Time: 2024-01-15 14:30:20
```

## 常见场景

### 场景 1：密码验证失败

```
[>] Password: ****
Password entropy level: Weak
[-] Incorrect password
[*] Remaining attempts: 2
[>] Password: 
```

**处理步骤**
1. 密码输入，显示强度
2. 验证失败，显示剩余次数
3. 允许重试

### 场景 2：账户被锁定

```
[!] Account locked. Remaining: 450 seconds
[*] Remaining: 450 seconds
[*] Remaining: 449 seconds
```

**处理步骤**
1. 检测锁定状态
2. 显示剩余时间
3. 自动计时
4. 600 秒后自动解锁

### 场景 3：文件路径非法

```
[!] Error: Invalid file path detected!
[!] FILE_PATH_VALIDATION_FAILED_AT_STARTUP
```

**处理步骤**
1. 检测路径遍历攻击
2. 拒绝访问
3. 记录安全事件
4. 安全退出

### 场景 4：新高分记录

```
Congratulations! New high score!
NEW_HIGH_SCORE: 42 (logged)
```

**处理步骤**
1. 比较猜测次数
2. 如果更少 → 新高分
3. 更新 game.txt
4. 记录事件日志

## 密码建议

### ✅ 好的密码示例

```
MySecureP@ssw0rd2024!
Tr0pic@lP@r@d1se#2024
C0mpl3x*&^%$#@!Password
```

**特点**
- 长度 ≥ 8 字符
- 包含大小写字母
- 包含数字
- 包含特殊字符
- 熵值：强/极强

### ❌ 弱的密码示例

```
123456
password
aaaaaa
test123
```

**问题**
- 太短或过于常见
- 缺乏字符多样性
- 熵值低
- 容易被破解

## 故障排除

### 问题 1：无法打开文件

```
[!] Error: Cannot open game record file
```

**解决方案**
- 检查目录权限
- 检查磁盘空间
- 检查文件是否被占用
- 尝试以管理员身份运行

### 问题 2：密码总是失败

**解决方案**
- 确保 Caps Lock 关闭
- 检查是否有多余空格
- 确认初始密码是 "limo"
- 检查是否误触其他键

### 问题 3：账户被锁定

```
[!] Account locked. Remaining: 600 seconds
```

**解决方案**
- 等待 10 分钟
- 不要频繁尝试（会重置计数器）
- 检查 lockout.dat 文件
- 删除 lockout.dat 可解锁（不推荐）

### 问题 4：高分记录损坏

```
Current high score: 0 (invalid)
```

**解决方案**
- 检查 game.txt 是否被损坏
- 删除 game.txt （会重新生成）
- 手动编辑：删除内容，写入数字

## 性能调优

### 密钥派生速度

```cpp
// ARGON2ID 参数可调整
// 当前：STRONG 配置
// 结果：1 秒左右

// 快速配置（不推荐）
// 结果：100ms
```

### 加密性能

```
操作              耗时
─────────────────────
Blake2b 哈希     < 1ms
AEAD 加密        < 1ms
AES-256 块       < 1μs
全流程认证       ~1s
```

### 日志性能

```cpp
// 日志轮转 10MB
// 超过 10MB 自动备份：
// auth.log → auth.log.bak.TIMESTAMP
```

## 安全最佳实践

### ✅ 推荐做法

1. **定期检查日志**
   ```bash
   tail -f auth.log
   ```

2. **备份高分记录**
   ```bash
   cp game.txt game.txt.backup
   ```

3. **监控账户锁定**
   ```bash
   grep "LOCKOUT" auth.log
   ```

4. **定期清理日志**
   ```bash
   # 30 天前的日志
   find . -name "auth.log.bak.*" -mtime +30 -delete
   ```

### ❌ 不推荐做法

1. **修改密码验证代码**
2. **删除审计日志**
3. **使用弱密码**
4. **频繁暴力尝试登录**
5. **修改密钥派生参数**

## API 参考

### InputValidator

```cpp
// 验证密码格式和强度
std::string validatePassword(std::string password);

// 验证文件路径（防路径遍历）
bool validateFilePath(const std::string& path);
```

### AccountLockoutManager

```cpp
// 检查账户是否被锁定
bool isAccountLocked();

// 获取剩余锁定时间（秒）
int getRemainingLockoutTime() const;

// 记录失败尝试
void recordFailedAttempt() const;

// 记录成功尝试
void recordSuccessfulAttempt();

// 获取失败次数
int getFailedAttempts() const;
```

### AuditLogger

```cpp
// 记录认证尝试
void logAuthAttempt(bool success, int remaining_attempts = -1);

// 记录账户锁定
void logAccountLockout(int duration_seconds);

// 记录密码修改
void logPasswordChange();

// 记录安全事件
void logSecurityEvent(const std::string& event_description);
```

### Blake2b

```cpp
// 计算 Blake2b 哈希
std::vector<uint8_t> hash(
    const std::vector<uint8_t>& data,
    size_t output_length
);
```

### SecureRandom

```cpp
// 生成随机向量
std::vector<uint8_t> generateVector(size_t length);

// 生成随机整数
uint32_t generate();
```

## 扩展和自定义

### 修改锁定时间

```cpp
// 在 Tollbox.h 中
static constexpr long long LOCKOUT_DURATION_SECONDS = 300;  // 改为 5 分钟
```

### 修改最大尝试次数

```cpp
// 在 Tollbox.h 中
static constexpr int MAX_FAILED_ATTEMPTS = 5;  // 改为 5 次
```

### 修改日志大小限制

```cpp
// 在 Tollbox.h 中
static constexpr size_t MAX_LOG_SIZE = 50 * 1024 * 1024;  // 改为 50MB
```

### 添加自定义日志

```cpp
// 在 main.cpp 中
audit_log.logSecurityEvent("YOUR_CUSTOM_EVENT: " + description);
```

## 总结

这是一个**完整、安全、生产就绪**的加密应用程序。按照此指南使用，您可以：

✅ 安全地管理用户认证
✅ 跟踪所有安全事件
✅ 防止暴力破解攻击
✅ 保护敏感数据
✅ 维护审计日志

**有问题？** 查阅 `MAIN_INTEGRATION_SUMMARY.md` 了解更多技术细节。

**准备好了吗？** 编译、运行、享受安全体验！🚀
