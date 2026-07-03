#include "Guid.h"

#include <cstdint>
#include <cstdio>
#include <random>

std::string GenerateGuid()
{
    static std::mt19937_64 generator = [] {
        std::random_device device;
        const std::uint64_t seed = (static_cast<std::uint64_t>(device()) << 32) ^ device();
        return std::mt19937_64(seed);
    }();

    const std::uint64_t high = generator();
    const std::uint64_t low = generator();

    // UUID v4 layout: version nibble 4, variant bits 10xx.
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer),
                  "%08x-%04x-4%03x-%04x-%012llx",
                  static_cast<unsigned int>(high >> 32),
                  static_cast<unsigned int>((high >> 16) & 0xFFFFu),
                  static_cast<unsigned int>(high & 0x0FFFu),
                  static_cast<unsigned int>(0x8000u | ((low >> 48) & 0x3FFFu)),
                  static_cast<unsigned long long>(low & 0xFFFFFFFFFFFFull));
    return std::string(buffer);
}
