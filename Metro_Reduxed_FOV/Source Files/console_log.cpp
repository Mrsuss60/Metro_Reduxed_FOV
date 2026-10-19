#include "console_log.h"
#include "globals.h"
#include <mutex>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Console {

static FILE* s_pConOut = nullptr;
static FILE* s_pConIn = nullptr;
static std::ofstream s_logFile;
static std::mutex s_logMutex;
static bool s_initialized = false;

static std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm bt;
    localtime_s(&bt, &timer);

    std::ostringstream oss;
    oss << "[" << std::setfill('0')
        << std::setw(2) << bt.tm_hour << ":"
        << std::setw(2) << bt.tm_min << ":"
        << std::setw(2) << bt.tm_sec << "]";
    return oss.str();
}

void Init() {
    std::lock_guard<std::mutex> lock(s_logMutex);
    if (s_initialized) return;

    if (g_EnableConsole) {
        AllocConsole();
        SetConsoleTitleA("Metro Redux FOV Mod");

        freopen_s(&s_pConOut, "CONOUT$", "w", stdout);
        freopen_s(&s_pConOut, "CONOUT$", "w", stderr);
        freopen_s(&s_pConIn, "CONIN$", "r", stdin);
    }

    s_logFile.open("MetroReduxedFOV.log", std::ios::out | std::ios::trunc);
    s_initialized = true;
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(s_logMutex);
    if (!s_initialized) return;

    if (s_logFile.is_open()) {
        s_logFile.close();
    }

    if (g_EnableConsole) {
        if (s_pConOut) { fclose(s_pConOut); s_pConOut = nullptr; }
        if (s_pConIn) { fclose(s_pConIn); s_pConIn = nullptr; }
        FreeConsole();
    }

    s_initialized = false;
}

void Print(const char* tag, const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(s_logMutex);
    if (!s_initialized) return;

    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    std::string ts = GetTimestamp();

    if (g_EnableConsole) {
        printf("%s %s: %s\n", ts.c_str(), tag, buffer);
        fflush(stdout);
    }

    if (s_logFile.is_open()) {
        s_logFile << ts << " " << tag << ": " << buffer << std::endl;
        s_logFile.flush();
    }
}

void Fov(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Print("fov", "%s", buffer);
}

void Hook(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Print("hook", "%s", buffer);
}

void Warn(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Print("warn", "%s", buffer);
}

void Error(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Print("error", "%s", buffer);
}

void Info(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Print("fov", "%s", buffer);
}

void Success(const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Print("fov", "%s", buffer);
}

} 
