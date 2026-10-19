#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>

class InlineHook {
public:
    InlineHook();
    ~InlineHook();

    InlineHook(const InlineHook&) = delete;
    InlineHook& operator=(const InlineHook&) = delete;

    InlineHook(InlineHook&& other) noexcept;
    InlineHook& operator=(InlineHook&& other) noexcept;

    bool Install(void* pTarget, void* pHookFunction);
    bool Uninstall();
    bool Remove() { return Uninstall(); }

    bool IsInstalled() const { return m_installed; }
    void* GetTarget() const { return m_pTarget; }
    void* GetTrampoline() const { return m_pTrampoline; }

    template<typename T>
    T GetOriginal() const {
        return reinterpret_cast<T>(m_pTrampoline);
    }

private:
    void* m_pTarget = nullptr;
    void* m_pHookFunction = nullptr;
    uint8_t* m_pTrampoline = nullptr;
    size_t m_stolenLen = 0;
    std::vector<uint8_t> m_originalBytes;
    bool m_installed = false;

    static void* AllocateNear(void* target, size_t size);
};

size_t GetInstructionLength(const uint8_t* code);
