#include "dshow-base.hpp"
#include "log.hpp"
#include "dshowcapture.hpp"

#include <cstdarg>
#include <cstdio>

namespace DShow {

void* logParam = nullptr;
static LogCallback logCallback = nullptr;

void SetLogCallback(LogCallback callback, void* param) {
    logCallback = callback;
    logParam = param;
}

static void Log(LogType type, const wchar_t* format, va_list args) {
    wchar_t str[4096];
    vswprintf_s(str, 4096, format, args);
    if (logCallback) {
        logCallback(type, str, logParam);
    }
}

void Error(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    Log(LogType::Error, format, args);
    va_end(args);
}

void Warning(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    Log(LogType::Warning, format, args);
    va_end(args);
}

void Info(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    Log(LogType::Info, format, args);
    va_end(args);
}

void Debug(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    Log(LogType::Debug, format, args);
    va_end(args);
}

void ErrorHR(const wchar_t* str, HRESULT hr) {
    Error(L"%s (0x%08lX)", str, hr);
}

void WarningHR(const wchar_t* str, HRESULT hr) {
    Warning(L"%s (0x%08lX)", str, hr);
}

void InfoHR(const wchar_t* str, HRESULT hr) {
    Info(L"%s (0x%08lX)", str, hr);
}

void DebugHR(const wchar_t* str, HRESULT hr) {
    Debug(L"%s (0x%08lX)", str, hr);
}

} // namespace DShow
