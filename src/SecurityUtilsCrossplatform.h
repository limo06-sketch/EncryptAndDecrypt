#ifndef SECURITY_UTILS_CROSSPLATFORM_H
#define SECURITY_UTILS_CROSSPLATFORM_H

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <chrono>
#include <mutex>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

// ============================================================================
// Input Handler
// ============================================================================

class InputHandler {
public:
    static std::string getInput() {
        std::string input;
        std::getline(std::cin, input);
        return input;
    }
    
    static void clearInputBuffer() {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
    }
};

// ============================================================================
// Input Validator
// ============================================================================

class InputValidator {
public:
    static constexpr size_t MIN_PASSWORD_LENGTH = 8;
    static constexpr size_t MAX_PASSWORD_LENGTH = 128;
    
    static std::string validatePassword(std::string password) {
        // Remove trailing newlines
        while (!password.empty() && 
               (password.back() == '\n' || password.back() == '\r')) {
            password.pop_back();
        }
        
        // Length check
        if (password.empty()) {
            throw std::invalid_argument("Password cannot be empty");
        }
        if (password.length() < MIN_PASSWORD_LENGTH) {
            std::string msg = "Password too short (minimum " + 
                            std::to_string(MIN_PASSWORD_LENGTH) + " characters)";
            throw std::invalid_argument(msg);
        }
        if (password.length() > MAX_PASSWORD_LENGTH) {
            std::string msg = "Password too long (maximum " + 
                            std::to_string(MAX_PASSWORD_LENGTH) + " characters)";
            throw std::invalid_argument(msg);
        }
        
        // Character set check (only ASCII printable)
        for (unsigned char c : password) {
            if (c < 32 || c > 126) {
                throw std::invalid_argument("Password contains invalid characters");
            }
        }
        
        // No null characters
        if (password.find('\0') != std::string::npos) {
            throw std::invalid_argument("Password contains null character");
        }
        
        return password;
    }
    
    static bool validateFilePath(const std::string& path) {
        // Prevent path traversal
        if (path.find("..") != std::string::npos) {
            return false;
        }
        
        if (path.empty()) {
            return false;
        }
        
        return true;
    }
};

// ============================================================================
// Secure File Handler
// ============================================================================

class SecureFileHandler {
public:
    static void createSecureFile(const std::string& path, 
                                 std::ios_base::openmode flags = std::ios::app) {
        if (!InputValidator::validateFilePath(path)) {
            throw std::invalid_argument("Unsafe file path");
        }
        
        std::ofstream file(path, flags);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + path);
        }
        file.close();
    }
    
    static void secureDelete(const std::string& path, int iterations = 3) {
        std::ifstream check(path);
        if (!check) {
            throw std::runtime_error("File not found: " + path);
        }
        check.seekg(0, std::ios::end);
        size_t file_size = check.tellg();
        check.close();
        
        if (file_size == 0) {
            fs::remove(path);
            return;
        }
        
        std::vector<uint8_t> buffer(file_size);
        
        for (int iter = 0; iter < iterations; ++iter) {
            for (size_t j = 0; j < file_size; ++j) {
                buffer[j] = static_cast<uint8_t>((static_cast<unsigned long long>(iter) * 255) ^ j);
            }
            
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            file.write((const char*)buffer.data(), file_size);
            file.flush();
            file.close();
        }
        
        std::fill(buffer.begin(), buffer.end(), 0);
        fs::remove(path);
    }
};

// ============================================================================
// Authentication Manager
// ============================================================================

class AuthenticationManager {
private:
    static constexpr int MAX_ATTEMPTS = 3;
    static constexpr int LOCKOUT_DURATION_SECONDS = 600;
    static constexpr const char* LOCKOUT_FILE = "auth_lockout.dat";
    
    struct LockoutRecord {
        std::chrono::steady_clock::time_point lockout_time{}; // 默认初始化
        int failed_count{0}; // 确保已初始化，消除 C26495
    };
    
    std::map<std::string, LockoutRecord> lockout_records;
    std::mutex mutex;
    std::string lockout_data_dir;
    
    static std::string getDeviceFingerprint() {
        static std::string fingerprint = "default_device";
        return fingerprint;
    }
    
public:
    explicit AuthenticationManager(const std::string& data_dir = ".") 
        : lockout_data_dir(data_dir) {
        loadLockoutState();
    }
    
    bool isLocked() {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::string fingerprint = getDeviceFingerprint();
        
        auto it = lockout_records.find(fingerprint);
        if (it != lockout_records.end()) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.lockout_time).count();
            
            if (elapsed < LOCKOUT_DURATION_SECONDS) {
                auto remaining = LOCKOUT_DURATION_SECONDS - elapsed;
                std::cerr << "Account locked. Remaining time: " << remaining << " seconds\n";
                return true;
            } else {
                lockout_records.erase(it);
                saveLockoutState();
            }
        }
        
        return false;
    }
    
    bool recordFailedAttempt() {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::string fingerprint = getDeviceFingerprint();
        
        if (lockout_records.find(fingerprint) == lockout_records.end()) {
            lockout_records[fingerprint] = {
                std::chrono::steady_clock::now(),
                0
            };
        }
        
        lockout_records[fingerprint].failed_count++;
        
        if (lockout_records[fingerprint].failed_count >= MAX_ATTEMPTS) {
            lockout_records[fingerprint].lockout_time = std::chrono::steady_clock::now();
            saveLockoutState();
            return true;
        }
        
        return false;
    }
    
    void recordSuccessfulAttempt() {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::string fingerprint = getDeviceFingerprint();
        lockout_records[fingerprint].failed_count = 0;
        saveLockoutState();
    }
    
    int getRemainingAttempts() {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::string fingerprint = getDeviceFingerprint();
        auto it = lockout_records.find(fingerprint);
        
        if (it == lockout_records.end()) {
            return MAX_ATTEMPTS;
        }
        
        return MAX_ATTEMPTS - it->second.failed_count;
    }
    
private:
    std::string getLockoutFilePath() const {
        fs::path dir(lockout_data_dir);
        return (dir / LOCKOUT_FILE).string();
    }
    
    void saveLockoutState() {
        try {
            std::string lockout_file = getLockoutFilePath();
            std::ofstream file(lockout_file, std::ios::binary);
            
            if (!file) {
                return;
            }
            
            auto now = std::chrono::steady_clock::now().time_since_epoch();
            long long ticks = now.count();
            
            file.write(reinterpret_cast<const char*>(&ticks), sizeof(ticks));
            file.close();
        } catch (...) {
            // Ignore errors, continue
        }
    }
    
    void loadLockoutState() {
        try {
            std::string lockout_file = getLockoutFilePath();
            std::ifstream file(lockout_file, std::ios::binary);
            
            if (!file) {
                return;
            }
            
            long long ticks = 0;
            file.read(reinterpret_cast<char*>(&ticks), sizeof(ticks));
            file.close();
        } catch (...) {
            // Ignore load errors
        }
    }
};

// ============================================================================
// Audit Log
// ============================================================================

class AuditLog {
private:
    std::ofstream log_file;
    std::mutex log_mutex;
    std::string log_path;
    size_t max_log_size = 1024 * 1024;
    
public:
    explicit AuditLog(const std::string& path) : log_path(path) {
        try {
            log_file.open(path, std::ios::app);
            if (!log_file.is_open()) {
                throw std::runtime_error("Cannot open log file: " + path);
            }
        } catch (const std::exception& e) {
            std::cerr << "Audit log initialization failed: " << e.what() << std::endl;
        }
    }
    
    ~AuditLog() {
        if (log_file.is_open()) {
            log_file.close();
        }
    }
    
    void logAuthSuccess() {
        if (!log_file.is_open()) return;
        
        std::lock_guard<std::mutex> lock(log_mutex);
        checkLogSize();
        log_file << "[AUTH_SUCCESS]" << std::endl;
        log_file.flush();
    }
    
    void logAuthFailure(int attempt_number) {
        if (!log_file.is_open()) return;
        
        std::lock_guard<std::mutex> lock(log_mutex);
        checkLogSize();
        log_file << "[AUTH_FAILURE:ATTEMPT_" << attempt_number << "]" << std::endl;
        log_file.flush();
    }
    
    void logAccountLockout() {
        if (!log_file.is_open()) return;
        
        std::lock_guard<std::mutex> lock(log_mutex);
        checkLogSize();
        log_file << "[ACCOUNT_LOCKED]" << std::endl;
        log_file.flush();
    }
    
    void logEvent(const std::string& event) {
        if (!log_file.is_open()) return;
        
        std::lock_guard<std::mutex> lock(log_mutex);
        checkLogSize();
        log_file << "[" << event << "]" << std::endl;
        log_file.flush();
    }
    
private:
    void checkLogSize() {
        log_file.seekp(0, std::ios::end);
        size_t current_size = log_file.tellp();
        
        if (current_size > max_log_size) {
            rotateLog();
        }
    }
    
    void rotateLog() {
        if (!log_file.is_open()) return;
        
        log_file.close();
        
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream backup_name;
        backup_name << log_path << ".bak." << time_t_now;
        
        try {
            fs::rename(log_path, backup_name.str());
        } catch (...) {
            try {
                fs::remove(log_path);
            } catch (...) {
                // Continue
            }
        }
        
        log_file.open(log_path, std::ios::app);
    }
};

// ============================================================================
// Utility Functions
// ============================================================================

inline void secureMemZero(void* ptr, size_t len) {
    volatile unsigned char* vptr = (volatile unsigned char*)ptr;
    for (size_t i = 0; i < len; ++i) {
        vptr[i] = 0;
    }
}

#endif // SECURITY_UTILS_CROSSPLATFORM_H
