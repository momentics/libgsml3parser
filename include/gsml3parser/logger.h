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

#pragma once

#include <cstddef>
#include <cstdio>
#include <functional>
#include <string>

namespace gsml3parser {

/**
 * Simple logging levels.  Set GSML3PARSER_LOG_LEVEL environment variable
 * to control the threshold (0=EMERG … 7=DEBUG).  Default: 6 (INFO).
 *
 * Each thread maintains its own log level (thread_local).
 */
enum class LogLevel : int {
    EMERG   = 0,
    ALERT   = 1,
    CRIT    = 2,
    ERR     = 3,
    WARNING = 4,
    NOTICE  = 5,
    INFO    = 6,
    DEBUG   = 7
};

/**
 * Return the current thread's log level threshold.
 */
LogLevel getLogLevel();

/**
 * Set the current thread's log level threshold.
 */
void setLogLevel(LogLevel level);

/**
 * Callback signature for custom log backends.
 *
 * @param level  Log severity.
 * @param file   Source file (may be nullptr).
 * @param line   Source line number (0 if unknown).
 * @param msg    Formatted message string (null-terminated).
 */
using LogCallback = std::function<void(LogLevel level, const char* file, int line, const char* msg)>;

/**
 * Install a custom log callback for the current thread.
 * When set, logMessage() invokes the callback instead of writing to stderr.
 * Pass nullptr to revert to the default stderr backend.
 */
void setLogCallback(LogCallback cb);

/**
 * Log a message at the given level.  Disabled if level exceeds the current
 * thread's threshold.  Thread-safe (mutex-protected for stderr output).
 */
void logMessage(LogLevel level, const char* file, int line, const char* fmt, ...);

} // namespace gsml3parser

// ── Macros ──────────────────────────────────────────────────────────────

#define GSML3PARSER_LOG(level, ...) \
    do { \
        if (static_cast<int>(level) <= static_cast<int>(gsml3parser::getLogLevel())) \
            gsml3parser::logMessage(level, __FILE__, __LINE__, __VA_ARGS__); \
    } while (0)

#define GSML3PARSER_LOG_EMERG(...)  GSML3PARSER_LOG(gsml3parser::LogLevel::EMERG, __VA_ARGS__)
#define GSML3PARSER_LOG_ALERT(...)  GSML3PARSER_LOG(gsml3parser::LogLevel::ALERT, __VA_ARGS__)
#define GSML3PARSER_LOG_CRIT(...)   GSML3PARSER_LOG(gsml3parser::LogLevel::CRIT, __VA_ARGS__)
#define GSML3PARSER_LOG_ERR(...)    GSML3PARSER_LOG(gsml3parser::LogLevel::ERR, __VA_ARGS__)
#define GSML3PARSER_LOG_WARN(...)   GSML3PARSER_LOG(gsml3parser::LogLevel::WARNING, __VA_ARGS__)
#define GSML3PARSER_LOG_NOTICE(...) GSML3PARSER_LOG(gsml3parser::LogLevel::NOTICE, __VA_ARGS__)
#define GSML3PARSER_LOG_INFO(...)   GSML3PARSER_LOG(gsml3parser::LogLevel::INFO, __VA_ARGS__)
#define GSML3PARSER_LOG_DEBUG(...)  GSML3PARSER_LOG(gsml3parser::LogLevel::DEBUG, __VA_ARGS__)


