#ifndef AES256_H
#define AES256_H

#include <vector>
#include <string>
#include <cstdint>
#include <array>
#include <random>
#include <chrono>
#include <stdexcept>
#include <memory>
#include <algorithm>

class AES256 {
public:
    explicit AES256(const std::vector<uint8_t>& key);
    ~AES256();

    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext);
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext);

    std::string encryptString(const std::string& plaintext);
    std::string decryptString(const std::string& ciphertext);
    // AEAD: Encrypt-then-MAC using SecureHash-based HMAC (MAC is appended as hex string)
    std::vector<uint8_t> encryptAEAD(const std::vector<uint8_t>& plaintext);
    std::vector<uint8_t> decryptAEAD(const std::vector<uint8_t>& aead);

private:
    static const size_t BLOCK_SIZE = 16;
    static const size_t KEY_SIZE = 32;
    static const size_t ROUNDS = 14;
    static const size_t IV_SIZE = 32; // 256-bit IV as requested

    // Core AES components
    static const uint8_t SBOX[256];
    static const uint8_t INV_SBOX[256];
    static const uint32_t RCON[10];

    std::vector<uint8_t> key;
    std::vector<uint8_t> macKey;
    std::vector<uint32_t> roundKeys;

    // Enhanced security components
    class SecureRandom {
    private:
        std::random_device rd; // use std::random_device as CSPRNG source when available

    public:
        SecureRandom();
        void generate(uint8_t* buffer, size_t size);
        std::vector<uint8_t> generateVector(size_t size);
    };

    // Core cryptographic operations
    void keyExpansion(const std::vector<uint8_t>& key);
    std::vector<uint8_t> generateSecureIV();
    void constantTimeMemZero(void* ptr, size_t len);
    bool constantTimeCompare(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);

    // AES core transformations
    void subBytes(std::vector<uint8_t>& state);
    void shiftRows(std::vector<uint8_t>& state);
    void mixColumns(std::vector<uint8_t>& state);
    void addRoundKey(std::vector<uint8_t>& state, size_t round);

    void invSubBytes(std::vector<uint8_t>& state);
    void invShiftRows(std::vector<uint8_t>& state);
    void invMixColumns(std::vector<uint8_t>& state);

    // Block operations (CTR mode for padding oracle protection)
    void encryptBlock(std::vector<uint8_t>& block);
    void decryptBlock(std::vector<uint8_t>& block);

    // Utility functions
    uint8_t gmul(uint8_t a, uint8_t b);
    void padData(std::vector<uint8_t>& data);
    void unpadData(std::vector<uint8_t>& data);
    uint32_t subWord(uint32_t word);
    uint32_t rotWord(uint32_t word);

    // Security enhancement: derive key with salt
    std::vector<uint8_t> deriveKey(const std::vector<uint8_t>& password);

    // 新增：CTR模式辅助函数
    void incrementCounter(std::vector<uint8_t>& counter);
};

#endif // AES256_H