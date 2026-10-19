#pragma once
#include <windows.h>
#include <cstdint>
#include <cstdio>

namespace Console {
    void Init();
    void Shutdown();

    void Print(const char* tag, const char* fmt, ...);
    void Fov(const char* fmt, ...);
    void Hook(const char* fmt, ...);
    void Warn(const char* fmt, ...);
    void Error(const char* fmt, ...);

    void Info(const char* fmt, ...);
    void Success(const char* fmt, ...);
}
