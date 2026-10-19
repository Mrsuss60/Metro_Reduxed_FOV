#include "g_memory.h"
#include <sstream>

bool PatchMemory(void* address, const void* data, size_t size) {
    if (!address || !data || size == 0) return false;
    DWORD old;

    DWORD protect = PAGE_READWRITE;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(address, &mbi, sizeof(mbi)) &&
        (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {
        protect = PAGE_EXECUTE_READWRITE;
    }

    if (VirtualProtect(address, size, protect, &old)) {
        memcpy(address, data, size);
        DWORD dummy;
        VirtualProtect(address, size, old, &dummy);
        FlushInstructionCache(GetCurrentProcess(), address, size);
        return true;
    }
    return false;
}

bool NopMemory(void* address, size_t size) {
    if (!address || size == 0) return false;
    std::vector<BYTE> nops(size, 0x90);
    return PatchMemory(address, nops.data(), size);
}

uintptr_t FindPattern(uintptr_t base, size_t size, const std::string& pattern, const std::string& mask) {
    auto DataCompare = [](const BYTE* data, const BYTE* maskBytes, const char* maskStr) {
        for (; *maskStr; ++maskStr, ++data, ++maskBytes) {
            if (*maskStr == 'x' && *data != *maskBytes) return false;
        }
        return true;
    };

    std::vector<BYTE> bytes;
    for (size_t i = 0; i < pattern.length(); i += 3) {
        bytes.push_back((BYTE)strtol(pattern.substr(i, 2).c_str(), nullptr, 16));
    }

    for (uintptr_t i = 0; i < size - mask.length(); i++) {
        if (DataCompare((BYTE*)(base + i), bytes.data(), mask.c_str())) {
            return base + i;
        }
    }
    return 0;
}

uintptr_t FindPatternIDA(uintptr_t base, size_t size, const char* pattern) {
    if (!base || !pattern || size == 0) return 0;

    std::vector<int> patternBytes;
    std::istringstream stream(pattern);
    std::string byteStr;

    while (stream >> byteStr) {
        if (byteStr == "?" || byteStr == "??") {
            patternBytes.push_back(-1); // Wildcard
        } else {
            patternBytes.push_back(std::stoi(byteStr, nullptr, 16));
        }
    }

    if (patternBytes.empty() || size < patternBytes.size()) return 0;

    const BYTE* scanBase = reinterpret_cast<const BYTE*>(base);
    const size_t scanLen = size - patternBytes.size();

    for (size_t i = 0; i < scanLen; ++i) {
        bool found = true;
        for (size_t j = 0; j < patternBytes.size(); ++j) {
            if (patternBytes[j] != -1 && scanBase[i + j] != static_cast<BYTE>(patternBytes[j])) {
                found = false;
                break;
            }
        }
        if (found) {
            return base + i;
        }
    }

    return 0;
}

uintptr_t FindStringRef(uintptr_t base, size_t size, const char* str) {
    if (!base || !str || size == 0) return 0;
    size_t strLen = strlen(str) + 1;

    uintptr_t strAddr = 0;
    const BYTE* scanBase = reinterpret_cast<const BYTE*>(base);
    for (size_t i = 0; i < size - strLen; ++i) {
        if (memcmp(scanBase + i, str, strLen) == 0) {
            strAddr = base + i;
            break;
        }
    }
    if (!strAddr) return 0;

    // RIP-relative reference: lea reg, [rip+disp32]
    for (size_t i = 0; i < size - 7; ++i) {
        if ((scanBase[i] == 0x48 || scanBase[i] == 0x4C) && scanBase[i + 1] == 0x8D) {
            uint8_t modrm = scanBase[i + 2];
            if ((modrm & 0xC7) == 0x05) {
                int32_t disp = *reinterpret_cast<const int32_t*>(scanBase + i + 3);
                uintptr_t refTarget = base + i + 7 + disp;
                if (refTarget == strAddr) {
                    return base + i;
                }
            }
        }
    }

    return 0;
}