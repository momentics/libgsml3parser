// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "gsml3parser/logger.h"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace gsml3parser {

// ── Thread-local state ──────────────────────────────────────────────────

static thread_local LogLevel tlsLogLevel = LogLevel::INFO;
static thread_local LogCallback tlsLogCallback;

LogLevel getLogLevel() {
    return tlsLogLevel;
}

void setLogLevel(LogLevel level) {
    tlsLogLevel = level;
}

void setLogCallback(LogCallback cb) {
    tlsLogCallback = std::move(cb);
}

// ── Helpers ─────────────────────────────────────────────────────────────

static const char* levelName(LogLevel level) {
    switch (level) {
        case LogLevel::EMERG:   return "EMERG";
        case LogLevel::ALERT:   return "ALERT";
        case LogLevel::CRIT:    return "CRIT";
        case LogLevel::ERR:     return "ERR";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::NOTICE:  return "NOTICE";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::DEBUG:   return "DEBUG";
        default:                return "???";
    }
}

static void emitStderr(LogLevel level, const char* file, int line, const char* fmt, va_list ap) {
    static std::mutex logMutex;
    std::lock_guard<std::mutex> lock(logMutex);

    time_t now = time(nullptr);
    struct tm tmBuf;
#ifdef _WIN32
    localtime_s(&tmBuf, &now);
#else
    localtime_r(&now, &tmBuf);
#endif

    fprintf(stderr, "[%04d-%02d-%02dT%02d:%02d:%02d] [%s] [%s:%d] ",
            tmBuf.tm_year + 1900, tmBuf.tm_mon + 1, tmBuf.tm_mday,
            tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec,
            levelName(level), file ? file : "?", line);

    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

// ── Public API ──────────────────────────────────────────────────────────

void logMessage(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (static_cast<int>(level) > static_cast<int>(tlsLogLevel)) return;

    va_list ap;
    va_start(ap, fmt);

    // Buffer the formatted message so we can pass it to the callback.
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    const char* msg;
    std::string heapMsg;
    if (n < 0 || static_cast<size_t>(n) >= sizeof(buf)) {
        // Message too long for inline buffer — allocate on heap.
        va_start(ap, fmt);
        heapMsg.resize(static_cast<size_t>(n) + 1);
        vsnprintf(heapMsg.data(), heapMsg.size(), fmt, ap);
        va_end(ap);
        msg = heapMsg.c_str();
    } else {
        msg = buf;
    }

    if (tlsLogCallback) {
        tlsLogCallback(level, file, line, msg);
    } else {
        // Re-open va_list for stderr output.
        va_list ap2;
        va_start(ap2, fmt);
        emitStderr(level, file, line, fmt, ap2);
        va_end(ap2);
    }
}

} // namespace gsml3parser
