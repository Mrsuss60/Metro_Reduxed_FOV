#pragma once
#include <windows.h>
#include <vector>
#include <string>

bool PatchMemory(void* address, const void* data, size_t size);
bool NopMemory(void* address, size_t size);
uintptr_t FindPattern(uintptr_t base, size_t size, const std::string& pattern, const std::string& mask);