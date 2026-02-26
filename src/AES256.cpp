#include "AES256.h"
#include "best_hash.h"
#include <cstring>
#include <iterator>
#include <functional>
#include <thread>

// AES S-box
const uint8_t AES256::SBOX[256] = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
    0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
    0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
    0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
};

// Inverse S-box
const uint8_t AES256::INV_SBOX[256] = {
    0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38, 0xBF, 0x40, 0xA3, 0x9E, 0x81, 0xF3, 0xD7, 0xFB,
    0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87, 0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB,
    0x54, 0x7B, 0x94, 0x32, 0xA6, 0xC2, 0x23, 0x3D, 0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E,
    0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2, 0x76, 0x5B, 0xA2, 0x49, 0x6D, 0x8B, 0xD1, 0x25,
    0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92,
    0x6C, 0x70, 0x48, 0x50, 0xFD, 0xED, 0xB9, 0xDA, 0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84,
    0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A, 0xF7, 0xE4, 0x58, 0x05, 0xB8, 0xB3, 0x45, 0x06,
    0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02, 0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B,
    0x3A, 0x91, 0x11, 0x41, 0x4F, 0x67, 0xDC, 0xEA, 0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73,
    0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85, 0xE2, 0xF9, 0x37, 0xE8, 0x1C, 0x75, 0xDF, 0x6E,
    0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89, 0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B,
    0xFC, 0x56, 0x3E, 0x4B, 0xC6, 0xD2, 0x79, 0x20, 0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4,
    0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31, 0xB1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xEC, 0x5F,
    0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D, 0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF,
    0xA0, 0xE0, 0x3B, 0x4D, 0xAE, 0x2A, 0xF5, 0xB0, 0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26, 0xE1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0C, 0x7D
};

// Round constants
const uint32_t AES256::RCON[10] = {
    0x01000000, 0x02000000, 0x04000000, 0x08000000,
    0x10000000, 0x20000000, 0x40000000, 0x80000000,
    0x1B000000, 0x36000000
};

// SecureRandom implementation - 改进的可靠性版本
AES256::SecureRandom::SecureRandom() {
    // std::random_device initialization - no explicit seeding needed
}

void AES256::SecureRandom::generate(uint8_t* buffer, size_t size) {
    static std::random_device rd;  // 单一全局实例以提高熵

    for (size_t i = 0; i < size; ++i) {
        // 多次调用std::random_device以增加熵
        unsigned int v1 = rd();
        unsigned int v2 = rd();
        unsigned int v3 = rd();

        // 混合多个随机值
        unsigned int mixed = (v1 ^ v2) + v3;
        buffer[i] = static_cast<uint8_t>((mixed >> (i % 4)) & 0xFF);
    }
}

std::vector<uint8_t> AES256::SecureRandom::generateVector(size_t size) {
    std::vector<uint8_t> result(size);
    generate(result.data(), size);
    return result;
}

// Constructor
AES256::AES256(const std::vector<uint8_t>& key) {
    if (key.empty()) {
        throw std::invalid_argument("密钥不能为空");
    }

    // 生成随机盐值（8字节）
    SecureRandom rng;
    this->salt = rng.generateVector(8);

    // 使用随机盐值进行密钥派生
    std::vector<uint8_t> master = deriveKey(key, this->salt);

    if (master.size() < KEY_SIZE * 2) {
        // ensure we have space to derive encKey + macKey
        master.resize(KEY_SIZE * 2, 0);
    }

    // split master into encryption key and mac key
    this->key.assign(master.begin(), master.begin() + KEY_SIZE);
    this->macKey.assign(master.begin() + KEY_SIZE, master.begin() + KEY_SIZE * 2);

    if (this->key.size() != KEY_SIZE) this->key.resize(KEY_SIZE, 0);
    if (this->macKey.size() != KEY_SIZE) this->macKey.resize(KEY_SIZE, 0);

    keyExpansion(this->key);

    // clear master
    constantTimeMemZero(master.data(), master.size());
}

AES256::~AES256() {
    constantTimeMemZero(key.data(), key.size());
    constantTimeMemZero(macKey.data(), macKey.size());
    constantTimeMemZero(salt.data(), salt.size());
    constantTimeMemZero(roundKeys.data(), roundKeys.size() * sizeof(uint32_t));
}

// Enhanced key derivation (PBKDF2-style implementation)
std::vector<uint8_t> AES256::deriveKey(const std::vector<uint8_t>& password, 
                                        const std::vector<uint8_t>& salt) {
    const size_t iterations = 500000;  // 增强的迭代次数
    std::vector<uint8_t> derivedKey(KEY_SIZE * 2, 0);  // 派生两个密钥（加密密钥+MAC密钥）

    // 初始化：使用HMAC-SHA3风格的混合
    std::vector<uint8_t> U(KEY_SIZE, 0);

    // 第一轮混合：password + salt
    for (size_t j = 0; j < KEY_SIZE; j++) {
        uint8_t p = password[j % password.size()];
        uint8_t s = salt[j % salt.size()];
        U[j] = (p ^ s);
    }

    // 主迭代循环 - PBKDF2风格
    for (size_t i = 0; i < iterations; i++) {
        // 轮转并应用非线性变换
        for (size_t j = 0; j < KEY_SIZE; j++) {
            uint8_t u_val = U[j];
            uint8_t p = password[j % password.size()];
            uint8_t s = salt[i % salt.size()];

            // 应用多重混合函数
            u_val ^= ((p + i) & 0xFF);
            u_val = ((u_val << 1) | (u_val >> 7));  // 左旋转
            u_val ^= ((s + j) & 0xFF);

            // 非线性S盒变换（基于AES S-box思想）
            u_val = SBOX[u_val];

            // 累积到派生密钥
            derivedKey[j] ^= u_val;
            if (j + KEY_SIZE < KEY_SIZE * 2) {
                derivedKey[j + KEY_SIZE] ^= (u_val ^ p ^ s);
            }

            U[j] = u_val;
        }

        // 每1000轮进行一次额外混合
        if (i % 1000 == 0) {
            for (size_t j = 0; j < KEY_SIZE - 1; j++) {
                U[j] ^= U[j + 1];
            }
            U[KEY_SIZE - 1] ^= U[0];
        }
    }

    // 清零敏感数据
    constantTimeMemZero(U.data(), U.size());

    return derivedKey;
}

// Constant-time memory zeroing
void AES256::constantTimeMemZero(void* ptr, size_t len) {
    if (ptr == nullptr) return;

    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    while (len--) {
        *p++ = 0;
    }
}

// Constant-time comparison
bool AES256::constantTimeCompare(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) return false;

    uint8_t result = 0;
    for (size_t i = 0; i < a.size(); i++) {
        result |= a[i] ^ b[i];
    }

    return result == 0;
}

// Generate secure 256-bit IV
std::vector<uint8_t> AES256::generateSecureIV() {
    SecureRandom rng;
    return rng.generateVector(IV_SIZE);
}

// Counter increment function
void AES256::incrementCounter(std::vector<uint8_t>& counter) {
    for (int i = BLOCK_SIZE - 1; i >= 0; i--) {
        if (++counter[i] != 0) break;
    }
}

// Key expansion
void AES256::keyExpansion(const std::vector<uint8_t>& key) {
    roundKeys.resize(4 * (ROUNDS + 1));

    // Copy initial key
    for (size_t i = 0; i < 8; i++) {
        roundKeys[i] = (key[4 * i] << 24) | (key[4 * i + 1] << 16) |
            (key[4 * i + 2] << 8) | key[4 * i + 3];
    }

    // Enhanced key expansion
    for (size_t i = 8; i < 4 * (ROUNDS + 1); i++) {
        uint32_t temp = roundKeys[i - 1];

        if (i % 8 == 0) {
            temp = subWord(rotWord(temp)) ^ RCON[i / 8 - 1];
        }
        else if (i % 8 == 4) {
            temp = subWord(temp);
        }

        // Additional transformation
        temp ^= (temp >> 16) ^ (temp << 16);
        roundKeys[i] = roundKeys[i - 8] ^ temp;
    }
}

// AES core transformations
void AES256::subBytes(std::vector<uint8_t>& state) {
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        state[i] = SBOX[state[i]];
    }
}

void AES256::invSubBytes(std::vector<uint8_t>& state) {
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        state[i] = INV_SBOX[state[i]];
    }
}

void AES256::shiftRows(std::vector<uint8_t>& state) {
    uint8_t temp = state[1];
    state[1] = state[5]; state[5] = state[9];
    state[9] = state[13]; state[13] = temp;

    std::swap(state[2], state[10]);
    std::swap(state[6], state[14]);

    temp = state[15];
    state[15] = state[11]; state[11] = state[7];
    state[7] = state[3]; state[3] = temp;
}

void AES256::invShiftRows(std::vector<uint8_t>& state) {
    uint8_t temp = state[13];
    state[13] = state[9]; state[9] = state[5];
    state[5] = state[1]; state[1] = temp;

    std::swap(state[2], state[10]);
    std::swap(state[6], state[14]);

    temp = state[3];
    state[3] = state[7]; state[7] = state[11];
    state[11] = state[15]; state[15] = temp;
}

uint8_t AES256::gmul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) p ^= a;
        bool hi_bit_set = a & 0x80;
        a <<= 1;
        if (hi_bit_set) a ^= 0x1B;
        b >>= 1;
    }
    return p;
}

void AES256::mixColumns(std::vector<uint8_t>& state) {
    for (size_t i = 0; i < 4; i++) {
        uint8_t a0 = state[4 * i], a1 = state[4 * i + 1],
            a2 = state[4 * i + 2], a3 = state[4 * i + 3];

        state[4 * i] = gmul(0x02, a0) ^ gmul(0x03, a1) ^ a2 ^ a3;
        state[4 * i + 1] = a0 ^ gmul(0x02, a1) ^ gmul(0x03, a2) ^ a3;
        state[4 * i + 2] = a0 ^ a1 ^ gmul(0x02, a2) ^ gmul(0x03, a3);
        state[4 * i + 3] = gmul(0x03, a0) ^ a1 ^ a2 ^ gmul(0x02, a3);
    }
}

void AES256::invMixColumns(std::vector<uint8_t>& state) {
    for (size_t i = 0; i < 4; i++) {
        uint8_t a0 = state[4 * i], a1 = state[4 * i + 1],
            a2 = state[4 * i + 2], a3 = state[4 * i + 3];

        state[4 * i] = gmul(0x0e, a0) ^ gmul(0x0b, a1) ^ gmul(0x0d, a2) ^ gmul(0x09, a3);
        state[4 * i + 1] = gmul(0x09, a0) ^ gmul(0x0e, a1) ^ gmul(0x0b, a2) ^ gmul(0x0d, a3);
        state[4 * i + 2] = gmul(0x0d, a0) ^ gmul(0x09, a1) ^ gmul(0x0e, a2) ^ gmul(0x0b, a3);
        state[4 * i + 3] = gmul(0x0b, a0) ^ gmul(0x0d, a1) ^ gmul(0x09, a2) ^ gmul(0x0e, a3);
    }
}

void AES256::addRoundKey(std::vector<uint8_t>& state, size_t round) {
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        size_t word = i / 4;
        size_t byte = i % 4;
        uint8_t roundKeyByte = (roundKeys[4 * round + word] >> (24 - 8 * byte)) & 0xFF;
        state[i] ^= roundKeyByte;
    }
}

void AES256::padData(std::vector<uint8_t>& data) {
    size_t padLen = BLOCK_SIZE - (data.size() % BLOCK_SIZE);
    data.insert(data.end(), padLen, static_cast<uint8_t>(padLen));
}

void AES256::unpadData(std::vector<uint8_t>& data) {
    if (data.empty()) return;

    uint8_t padLen = data.back();
    if (padLen > BLOCK_SIZE || padLen == 0) {
        throw std::runtime_error("无效的填充数据");
    }

    for (size_t i = data.size() - padLen; i < data.size(); i++) {
        if (data[i] != padLen) {
            throw std::runtime_error("无效的填充数据");
        }
    }

    data.resize(data.size() - padLen);
}

uint32_t AES256::subWord(uint32_t word) {
    return (SBOX[(word >> 24) & 0xFF] << 24) |
        (SBOX[(word >> 16) & 0xFF] << 16) |
        (SBOX[(word >> 8) & 0xFF] << 8) |
        SBOX[word & 0xFF];
}

uint32_t AES256::rotWord(uint32_t word) {
    return (word << 8) | (word >> 24);
}

void AES256::encryptBlock(std::vector<uint8_t>& block) {
    addRoundKey(block, 0);

    for (size_t round = 1; round < ROUNDS; round++) {
        subBytes(block);
        shiftRows(block);
        mixColumns(block);
        addRoundKey(block, round);
    }

    subBytes(block);
    shiftRows(block);
    addRoundKey(block, ROUNDS);
}

void AES256::decryptBlock(std::vector<uint8_t>& block) {
    addRoundKey(block, ROUNDS);
    invShiftRows(block);
    invSubBytes(block);

    for (int round = ROUNDS - 1; round > 0; round--) {
        addRoundKey(block, round);
        invMixColumns(block);
        invShiftRows(block);
        invSubBytes(block);
    }

    addRoundKey(block, 0);
}

// Enhanced encryption using CTR mode
std::vector<uint8_t> AES256::encrypt(const std::vector<uint8_t>& plaintext) {
    if (plaintext.empty()) return {};

    std::vector<uint8_t> data = plaintext;
    padData(data);

    // Generate secure 256-bit IV
    std::vector<uint8_t> iv = generateSecureIV();
    std::vector<uint8_t> ciphertext;

    // Use CTR mode for better security
    ciphertext.insert(ciphertext.end(), iv.begin(), iv.end());

    // Use only first BLOCK_SIZE bytes for CTR counter
    std::vector<uint8_t> counter(iv.begin(), iv.begin() + BLOCK_SIZE);

    for (size_t i = 0; i < data.size(); i += BLOCK_SIZE) {
        // Encrypt current counter
        std::vector<uint8_t> encryptedCounter = counter;
        encryptBlock(encryptedCounter);

        // XOR with plaintext block
        size_t block_size = std::min(BLOCK_SIZE, data.size() - i);
        for (size_t j = 0; j < block_size; j++) {
            ciphertext.push_back(data[i + j] ^ encryptedCounter[j]);
        }

        // Increment counter
        incrementCounter(counter);
    }

    return ciphertext;
}

// Enhanced decryption (CTR mode)
std::vector<uint8_t> AES256::decrypt(const std::vector<uint8_t>& ciphertext) {
    if (ciphertext.size() < IV_SIZE) {
        throw std::invalid_argument("密文太短，无法提取IV");
    }

    // Extract IV
    std::vector<uint8_t> iv(ciphertext.begin(), ciphertext.begin() + IV_SIZE);
    std::vector<uint8_t> encryptedData(ciphertext.begin() + IV_SIZE, ciphertext.end());

    std::vector<uint8_t> plaintext;

    // Use only first BLOCK_SIZE bytes for CTR counter
    std::vector<uint8_t> counter(iv.begin(), iv.begin() + BLOCK_SIZE);

    for (size_t i = 0; i < encryptedData.size(); i += BLOCK_SIZE) {
        // Encrypt current counter
        std::vector<uint8_t> encryptedCounter = counter;
        encryptBlock(encryptedCounter);

        // XOR with ciphertext block
        size_t block_size = std::min(BLOCK_SIZE, encryptedData.size() - i);
        for (size_t j = 0; j < block_size; j++) {
            plaintext.push_back(encryptedData[i + j] ^ encryptedCounter[j]);
        }

        // Increment counter
        incrementCounter(counter);
    }

    unpadData(plaintext);
    return plaintext;

}

// AEAD: Encrypt-then-MAC. Output = IV || ciphertext || MAC(bytes)
std::vector<uint8_t> AES256::encryptAEAD(const std::vector<uint8_t>& plaintext) {
    // Encrypt with CTR
    std::vector<uint8_t> ct = encrypt(plaintext);

    // Compute HMAC over IV||ciphertext using SecureHash::hmac with derived macKey
    std::vector<uint8_t> mac = SecureHash::hmac(this->macKey, ct, 32); // 32-byte tag

    // Build output: ct || mac
    std::vector<uint8_t> out;
    out.reserve(ct.size() + mac.size());
    out.insert(out.end(), ct.begin(), ct.end());
    out.insert(out.end(), mac.begin(), mac.end());

    // clear mac temporary
    // (mac will be appended to output and thus not zeroed here)

    return out;
}

std::vector<uint8_t> AES256::decryptAEAD(const std::vector<uint8_t>& aead) {
    if (aead.size() < 32 + IV_SIZE) throw std::runtime_error("AEAD: input too short");

    // Separate mac (last 32 bytes)
    size_t mac_len = 32;
    size_t ct_len = aead.size() - mac_len;

    std::vector<uint8_t> ct(aead.begin(), aead.begin() + ct_len);
    std::vector<uint8_t> mac(aead.begin() + ct_len, aead.end());

    // Verify MAC
    std::vector<uint8_t> expected = SecureHash::hmac(this->macKey, ct, mac_len);
    bool ok = constantTimeCompare(expected, mac);
    // clear sensitive
    constantTimeMemZero(expected.data(), expected.size());

    if (!ok) throw std::runtime_error("AEAD: authentication failed");

    // Decrypt ciphertext (which contains IV||actual ciphertext)
    std::vector<uint8_t> pt = decrypt(ct);

    return pt;
}

// String convenience functions
std::string AES256::encryptString(const std::string& plaintext) {
    std::vector<uint8_t> plainData(plaintext.begin(), plaintext.end());
    auto encrypted = encrypt(plainData);
    return std::string(encrypted.begin(), encrypted.end());
}

std::string AES256::decryptString(const std::string& ciphertext) {
    std::vector<uint8_t> cipherData(ciphertext.begin(), ciphertext.end());
    auto decrypted = decrypt(cipherData);
    return std::string(decrypted.begin(), decrypted.end());
}