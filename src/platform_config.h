#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

/**
 * 跨平台配置头文件
 * 为Windows, Linux, macOS及其他POSIX系统提供统一接口
 */

#include <cstdint>
#include <cstddef>

// 编译器检查
#if defined(_MSC_VER)
    #define PLATFORM_WINDOWS 1
    #define COMPILER_MSVC 1
#elif defined(__clang__)
    #define COMPILER_CLANG 1
#elif defined(__GNUC__)
    #define COMPILER_GCC 1
#endif

// 操作系统检查
#if defined(_WIN32) || defined(_WIN64) || defined(PLATFORM_WINDOWS)
    #define OS_WINDOWS 1
    #define PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
    #define OS_MACOS 1
    #define PLATFORM_NAME "macOS"
#elif defined(__linux__)
    #define OS_LINUX 1
    #define PLATFORM_NAME "Linux"
#elif defined(__unix__)
    #define OS_UNIX 1
    #define PLATFORM_NAME "Unix"
#else
    #define PLATFORM_NAME "Unknown"
#endif

// 字节序检查
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define LITTLE_ENDIAN 1
    #endif
#elif defined(_WIN32)
    #define LITTLE_ENDIAN 1
#elif defined(__APPLE__)
    #define LITTLE_ENDIAN 1
#elif defined(__linux__)
    #define LITTLE_ENDIAN 1
#else
    // 默认假设小端
    #define LITTLE_ENDIAN 1
#endif

// 位宽度检查
#if defined(__LP64__) || defined(_WIN64)
    #define BITS_64 1
#else
    #define BITS_32 1
#endif

// 对齐宏
#if defined(_MSC_VER)
    #define ALIGN(n) __declspec(align(n))
#else
    #define ALIGN(n) __attribute__((aligned(n)))
#endif

// 可能未使用的属性
#if defined(__GNUC__) || defined(__clang__)
    #define UNUSED __attribute__((unused))
#else
    #define UNUSED
#endif

// 强制内联
#if defined(_MSC_VER)
    #define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define FORCE_INLINE inline __attribute__((always_inline))
#else
    #define FORCE_INLINE inline
#endif

// 导出符号（如果编译为DLL/SO）
#if defined(_MSC_VER)
    #define EXPORT_SYMBOL __declspec(dllexport)
    #define IMPORT_SYMBOL __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
    #define EXPORT_SYMBOL __attribute__((visibility("default")))
    #define IMPORT_SYMBOL
#else
    #define EXPORT_SYMBOL
    #define IMPORT_SYMBOL
#endif

// 断言宏（仅在调试模式）
#if defined(_DEBUG) || defined(DEBUG)
    #include <cassert>
    #define CRYPTO_ASSERT(expr) assert(expr)
#else
    #define CRYPTO_ASSERT(expr) ((void)0)
#endif

// C++标准版本检查
#if __cplusplus >= 201703L
    #define CPP_VERSION_17 1
#elif __cplusplus >= 201402L
    #define CPP_VERSION_14 1
#elif __cplusplus >= 201103L
    #define CPP_VERSION_11 1
#endif

// 线程本地存储
#if defined(_MSC_VER)
    #define THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
    #define THREAD_LOCAL __thread
#else
    #define THREAD_LOCAL thread_local
#endif

// 防护编译警告
#if defined(_MSC_VER)
    #pragma warning(disable: 4996)  // 不安全函数警告
    #pragma warning(disable: 4251)  // DLL导出警告
#endif

#endif // PLATFORM_CONFIG_H
