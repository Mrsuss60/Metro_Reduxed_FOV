#include "hook.h"
#include "console_log.h"
#include <cstring>
#include <climits>

namespace {

enum OpcodeFlags : uint32_t {
    FLAG_NONE       = 0,
    FLAG_MODRM      = 1 << 0,
    FLAG_DATA_IMM8  = 1 << 1,
    FLAG_DATA_IMM16 = 1 << 2,
    FLAG_DATA_IMM32 = 1 << 3,
    FLAG_DATA_IMM64 = 1 << 4,
    FLAG_DATA_REL8  = 1 << 5,
    FLAG_DATA_REL32 = 1 << 6,
    FLAG_EXT_IMM8   = 1 << 7
};

}

size_t GetInstructionLength(const uint8_t* code) {
    if (!code) return 0;

    const uint8_t* p = code;
    bool hasRex = false;
    uint8_t rex = 0;
    bool has66 = false;
    bool has67 = false;

    while (true) {
        uint8_t b = *p;

        if (b == 0x66) {
            has66 = true;
            p++;
        }
        else if (b == 0x67) {
            has67 = true;
            p++;
        }
        else if (b == 0xF0 || b == 0xF2 || b == 0xF3 ||
                 b == 0x2E || b == 0x36 || b == 0x3E || b == 0x26 || b == 0x64 || b == 0x65) {
            p++;
        }
        else if ((b & 0xF0) == 0x40) {
            hasRex = true;
            rex = b;
            p++;
        }
        else {
            break;
        }

        if ((size_t)(p - code) >= 15) return 0;
    }

    uint8_t opcode = *p++;
    bool isTwoByte = false;
    bool isThreeByte = false;
    uint8_t threeBytePrefix = 0;

    if (opcode == 0x0F) {
        isTwoByte = true;
        opcode = *p++;
        if (opcode == 0x38 || opcode == 0x3A) {
            isThreeByte = true;
            threeBytePrefix = opcode;
            opcode = *p++;
        }
    }

    bool hasModRm = false;
    int immSize = 0;

    if (isThreeByte) {
        hasModRm = true;
        if (threeBytePrefix == 0x3A) immSize = 1;
    }
    else if (isTwoByte) {
        if ((opcode >= 0x80 && opcode <= 0x8F)) {
            immSize = 4;
        }
        else if (opcode == 0xBA) {
            hasModRm = true;
            immSize = 1;
        }
        else if (opcode == 0x70 || opcode == 0xA4 || opcode == 0xAC || opcode == 0xC2 || opcode == 0xC4 || opcode == 0xC5 || opcode == 0xC6) {
            hasModRm = true;
            immSize = 1;
        }
        else if (opcode == 0xA2 || opcode == 0x31 || opcode == 0x05 || opcode == 0x0B) {
            hasModRm = false;
        }
        else {
            hasModRm = true;
        }
    }
    else {
        switch (opcode) {
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
        case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
        case 0x98: case 0x99: case 0x9C: case 0x9D: case 0x9E: case 0x9F:
        case 0xC3: case 0xCB: case 0xC9: case 0xCC: case 0xCD: case 0xCE: case 0xCF:
        case 0xEC: case 0xED: case 0xEE: case 0xEF:
        case 0xF4: case 0xF5: case 0xF8: case 0xF9: case 0xFA: case 0xFB: case 0xFC: case 0xFD:
            break;

        case 0x04: case 0x0C: case 0x14: case 0x1C: case 0x24: case 0x2C: case 0x34: case 0x3C:
        case 0x6A: case 0xA8:
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
        case 0xEB: case 0xE3:
            immSize = 1;
            break;

        case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
            immSize = 1;
            break;

        case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
            if (hasRex && (rex & 0x08)) {
                immSize = 8;
            } else if (has66) {
                immSize = 2;
            } else {
                immSize = 4;
            }
            break;

        case 0x05: case 0x0D: case 0x15: case 0x1D: case 0x25: case 0x2D: case 0x35: case 0x3D:
        case 0x68: case 0xA9: case 0xE8: case 0xE9:
            immSize = has66 ? 2 : 4;
            break;

        case 0x80: case 0x82: case 0x83:
        case 0xC0: case 0xC1:
        case 0xC6:
        case 0x6B:
            hasModRm = true;
            immSize = 1;
            break;

        case 0x81:
        case 0xC7:
        case 0x69:
            hasModRm = true;
            immSize = has66 ? 2 : 4;
            break;

        case 0xF6:
        case 0xF7: {
            hasModRm = true;
            uint8_t modrm = *p;
            uint8_t reg = (modrm >> 3) & 7;
            if (reg == 0) {
                immSize = (opcode == 0xF6) ? 1 : (has66 ? 2 : 4);
            }
            break;
        }

        default:
            hasModRm = true;
            break;
        }
    }

    if (hasModRm) {
        uint8_t modrm = *p++;
        uint8_t mod = (modrm >> 6) & 3;
        uint8_t rm = modrm & 7;

        bool hasSib = false;
        uint8_t sibBase = 0;

        if (mod != 3 && rm == 4) {
            hasSib = true;
            uint8_t sib = *p++;
            sibBase = sib & 7;
        }

        if (mod == 0) {
            if (rm == 5 || (hasSib && sibBase == 5)) {
                p += 4;
            }
        }
        else if (mod == 1) {
            p += 1;
        }
        else if (mod == 2) {
            p += 4;
        }
    }

    p += immSize;

    size_t length = (size_t)(p - code);
    return (length <= 15) ? length : 0;
}

InlineHook::InlineHook() = default;

InlineHook::~InlineHook() {
    Uninstall();
}

InlineHook::InlineHook(InlineHook&& other) noexcept {
    *this = std::move(other);
}

InlineHook& InlineHook::operator=(InlineHook&& other) noexcept {
    if (this != &other) {
        Uninstall();
        m_pTarget = other.m_pTarget;
        m_pHookFunction = other.m_pHookFunction;
        m_pTrampoline = other.m_pTrampoline;
        m_stolenLen = other.m_stolenLen;
        m_originalBytes = std::move(other.m_originalBytes);
        m_installed = other.m_installed;

        other.m_pTarget = nullptr;
        other.m_pHookFunction = nullptr;
        other.m_pTrampoline = nullptr;
        other.m_stolenLen = 0;
        other.m_installed = false;
    }
    return *this;
}

void* InlineHook::AllocateNear(void* target, size_t size) {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    const uintptr_t gran = sysInfo.dwAllocationGranularity;

    uintptr_t minAddr = (uintptr_t)sysInfo.lpMinimumApplicationAddress;
    uintptr_t maxAddr = (uintptr_t)sysInfo.lpMaximumApplicationAddress;
    uintptr_t start = (uintptr_t)target;

    uintptr_t lowBound = (start > 0x70000000ULL) ? (start - 0x70000000ULL) : minAddr;
    uintptr_t highBound = (start + 0x70000000ULL < maxAddr) ? (start + 0x70000000ULL) : maxAddr;

    for (uintptr_t addr = (start & ~(gran - 1)); addr > lowBound; addr -= gran) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) break;
        if (mbi.State == MEM_FREE) {
            void* mem = VirtualAlloc((LPVOID)addr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (mem) return mem;
        }
        if ((uintptr_t)mbi.AllocationBase < gran) break;
        addr = (uintptr_t)mbi.AllocationBase;
    }

    for (uintptr_t addr = ((start + gran) & ~(gran - 1)); addr < highBound; addr += gran) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) break;
        if (mbi.State == MEM_FREE) {
            void* mem = VirtualAlloc((LPVOID)addr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (mem) return mem;
        }
        addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    }

    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

bool InlineHook::Install(void* pTarget, void* pHookFunction) {
    if (!pTarget || !pHookFunction) {
        Console::Error("Invalid target or detour pointer");
        return false;
    }
    if (m_installed) {
        Console::Warn("Hook on 0x%p already installed", pTarget);
        return false;
    }

    m_pTarget = pTarget;
    m_pHookFunction = pHookFunction;

    constexpr size_t MIN_DETOUR_LEN = 14;

    m_stolenLen = 0;
    const uint8_t* pCode = static_cast<const uint8_t*>(m_pTarget);

    while (m_stolenLen < MIN_DETOUR_LEN) {
        size_t len = GetInstructionLength(pCode + m_stolenLen);
        if (len == 0 || m_stolenLen + len > 32) {
            Console::Error("Unable to decode instruction at +0x%zx on 0x%p", m_stolenLen, m_pTarget);
            return false;
        }
        m_stolenLen += len;
    }

    m_originalBytes.assign(pCode, pCode + m_stolenLen);

    size_t trampSize = m_stolenLen + 14 + 16;
    m_pTrampoline = static_cast<uint8_t*>(AllocateNear(m_pTarget, trampSize));
    if (!m_pTrampoline) {
        Console::Error("Failed to allocate trampoline near 0x%p", m_pTarget);
        return false;
    }

    size_t currOffset = 0;
    while (currOffset < m_stolenLen) {
        const uint8_t* srcInstr = pCode + currOffset;
        uint8_t* dstInstr = m_pTrampoline + currOffset;
        size_t instrLen = GetInstructionLength(srcInstr);

        memcpy(dstInstr, srcInstr, instrLen);

        size_t prefixLen = 0;
        while (prefixLen < instrLen) {
            uint8_t b = srcInstr[prefixLen];
            if (b == 0x66 || b == 0x67 || b == 0xF0 || b == 0xF2 || b == 0xF3 ||
                b == 0x2E || b == 0x36 || b == 0x3E || b == 0x26 || b == 0x64 || b == 0x65 ||
                (b & 0xF0) == 0x40) {
                prefixLen++;
            } else {
                break;
            }
        }

        size_t opcodeOffset = prefixLen;
        uint8_t op = srcInstr[opcodeOffset];

        // Relocate call/jmp rel32 (E8 / E9)
        if (op == 0xE8 || op == 0xE9) {
            size_t dispOffset = opcodeOffset + 1;
            if (dispOffset + 4 <= instrLen) {
                int32_t origDisp = *reinterpret_cast<const int32_t*>(srcInstr + dispOffset);
                uintptr_t targetAddr = reinterpret_cast<uintptr_t>(srcInstr) + instrLen + origDisp;
                intptr_t newDisp = static_cast<intptr_t>(targetAddr) - (reinterpret_cast<uintptr_t>(dstInstr) + instrLen);

                if (newDisp >= INT32_MIN && newDisp <= INT32_MAX) {
                    *reinterpret_cast<int32_t*>(dstInstr + dispOffset) = static_cast<int32_t>(newDisp);
                } else {
                    Console::Warn("Relocation out of 32-bit range for 0x%02X at 0x%p", op, (void*)targetAddr);
                }
            }
        }
        else if (op == 0x0F) {
            opcodeOffset++;
            uint8_t op2 = srcInstr[opcodeOffset];
            // Relocate Jcc rel32 (0x0F 0x80 - 0x0F 0x8F)
            if (op2 >= 0x80 && op2 <= 0x8F) {
                size_t dispOffset = opcodeOffset + 1;
                if (dispOffset + 4 <= instrLen) {
                    int32_t origDisp = *reinterpret_cast<const int32_t*>(srcInstr + dispOffset);
                    uintptr_t targetAddr = reinterpret_cast<uintptr_t>(srcInstr) + instrLen + origDisp;
                    intptr_t newDisp = static_cast<intptr_t>(targetAddr) - (reinterpret_cast<uintptr_t>(dstInstr) + instrLen);

                    if (newDisp >= INT32_MIN && newDisp <= INT32_MAX) {
                        *reinterpret_cast<int32_t*>(dstInstr + dispOffset) = static_cast<int32_t>(newDisp);
                    }
                }
            } else {
                // Relocate RIP-relative displacement
                size_t modRmOffset = opcodeOffset + 1;
                if (modRmOffset < instrLen) {
                    uint8_t modrm = srcInstr[modRmOffset];
                    if ((modrm >> 6) == 0 && (modrm & 7) == 5) {
                        size_t dispOffset = modRmOffset + 1;
                        if (dispOffset + 4 <= instrLen) {
                            int32_t origDisp = *reinterpret_cast<const int32_t*>(srcInstr + dispOffset);
                            uintptr_t targetAddr = reinterpret_cast<uintptr_t>(srcInstr) + instrLen + origDisp;
                            intptr_t newDisp = static_cast<intptr_t>(targetAddr) - (reinterpret_cast<uintptr_t>(dstInstr) + instrLen);

                            if (newDisp >= INT32_MIN && newDisp <= INT32_MAX) {
                                *reinterpret_cast<int32_t*>(dstInstr + dispOffset) = static_cast<int32_t>(newDisp);
                            }
                        }
                    }
                }
            }
        }
        else {
            // Relocate RIP-relative displacement
            size_t modRmOffset = opcodeOffset + 1;
            if (modRmOffset < instrLen) {
                uint8_t modrm = srcInstr[modRmOffset];
                if ((modrm >> 6) == 0 && (modrm & 7) == 5) {
                    size_t dispOffset = modRmOffset + 1;
                    if (dispOffset + 4 <= instrLen) {
                        int32_t origDisp = *reinterpret_cast<const int32_t*>(srcInstr + dispOffset);
                        uintptr_t targetAddr = reinterpret_cast<uintptr_t>(srcInstr) + instrLen + origDisp;
                        intptr_t newDisp = static_cast<intptr_t>(targetAddr) - (reinterpret_cast<uintptr_t>(dstInstr) + instrLen);

                        if (newDisp >= INT32_MIN && newDisp <= INT32_MAX) {
                            *reinterpret_cast<int32_t*>(dstInstr + dispOffset) = static_cast<int32_t>(newDisp);
                        }
                    }
                }
            }
        }

        currOffset += instrLen;
    }

    // 14-byte indirect JMP: FF 25 00 00 00 00 [target]
    uint8_t* pJumpBack = m_pTrampoline + m_stolenLen;
    pJumpBack[0] = 0xFF;
    pJumpBack[1] = 0x25;
    pJumpBack[2] = 0x00;
    pJumpBack[3] = 0x00;
    pJumpBack[4] = 0x00;
    pJumpBack[5] = 0x00;
    *reinterpret_cast<uint64_t*>(pJumpBack + 6) = reinterpret_cast<uint64_t>(pCode + m_stolenLen);

    DWORD oldProtect;
    if (!VirtualProtect(m_pTarget, m_stolenLen, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Console::Error("VirtualProtect on 0x%p failed", m_pTarget);
        VirtualFree(m_pTrampoline, 0, MEM_RELEASE);
        m_pTrampoline = nullptr;
        return false;
    }

    uint8_t* pDetour = static_cast<uint8_t*>(m_pTarget);
    pDetour[0] = 0xFF;
    pDetour[1] = 0x25;
    pDetour[2] = 0x00;
    pDetour[3] = 0x00;
    pDetour[4] = 0x00;
    pDetour[5] = 0x00;
    *reinterpret_cast<uint64_t*>(pDetour + 6) = reinterpret_cast<uint64_t>(pHookFunction);

    for (size_t i = MIN_DETOUR_LEN; i < m_stolenLen; ++i) {
        pDetour[i] = 0x90;
    }

    DWORD trampProtect;
    VirtualProtect(m_pTrampoline, trampSize, PAGE_EXECUTE_READ, &trampProtect);
    FlushInstructionCache(GetCurrentProcess(), m_pTrampoline, trampSize);

    VirtualProtect(m_pTarget, m_stolenLen, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), m_pTarget, m_stolenLen);

    Console::Hook("Hook call site @ 0x%p -> 0x%p", m_pTarget, pHookFunction);

    m_installed = true;
    return true;
}

bool InlineHook::Uninstall() {
    if (!m_installed || !m_pTarget) return false;

    DWORD oldProtect;
    if (VirtualProtect(m_pTarget, m_stolenLen, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(m_pTarget, m_originalBytes.data(), m_stolenLen);
        VirtualProtect(m_pTarget, m_stolenLen, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), m_pTarget, m_stolenLen);
        Console::Hook("Restored original bytes @ 0x%p", m_pTarget);
    }

    if (m_pTrampoline) {
        VirtualFree(m_pTrampoline, 0, MEM_RELEASE);
        m_pTrampoline = nullptr;
    }

    m_installed = false;
    m_pTarget = nullptr;
    m_pHookFunction = nullptr;
    m_stolenLen = 0;
    m_originalBytes.clear();

    return true;
}
