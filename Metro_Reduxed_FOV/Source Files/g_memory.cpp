#include "g_memory.h"

bool PatchMemory(void* address, const void* data, size_t size) {
    if (!address) return false;
    DWORD old;
    if (VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old)) {
        memcpy(address, data, size);
        VirtualProtect(address, size, old, &old);
        return true;
    }
    return false;
}

bool NopMemory(void* address, size_t size) {
    if (!address) return false;
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