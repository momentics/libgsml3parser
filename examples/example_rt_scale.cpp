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

// example_rt_scale.cpp — real-time scale verification.
//
// Creates 1,000,000 subscriber sessions, then runs 1000 event-loop ticks
// of 10 ms each (tickAllTimers + tickAllProcedures + 500 parsed L3
// messages per tick = 50K msg/s aggregate) and verifies the real-time
// budget: no tick may exceed its 10 ms period (audit SCALE).

#include <algorithm>  // std::min / std::max for tick statistics
#include <array>      // std::array<TimerExpiry, 1024>
#include <chrono>
#include <climits>
#include <cinttypes>  // PRId64 — portable int64_t printf (Linux CI: int64_t = long)
#include <cstdio>
#include <cstring>
#include <span>
#include <vector>

#include <gsml3parser/parser.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/stack/subscriber_registry.h>

using namespace gsml3parser;

#ifdef _WIN32
// NOMINMAX: windows.h defines min/max macros that would break std::min/std::max.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
// GetProcessMemoryInfo lives in Psapi.dll — link it explicitly,
// otherwise MSVC fails with LNK2019 (unresolved external).
#pragma comment(lib, "psapi.lib")
static double peakWorkingSetGB() {
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(),
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        return -1.0;
    }
    return static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0 * 1024.0);
}
#else
static double peakWorkingSetGB() {
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return -1.0;
    char line[256];
    double gb = -1.0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmPeak:", 7) == 0) {
            long kb = 0;
            if (sscanf(line + 7, "%ld", &kb) == 1) gb = static_cast<double>(kb) / (1024.0 * 1024.0);
            break;
        }
    }
    fclose(f);
    return gb;
}
#endif

int main() {
    constexpr uint32_t N = 1'000'000;
    constexpr int TICKS = 1000;
    constexpr int MSGS_PER_TICK = 500;
    constexpr int64_t PERIOD_NS = 10'000'000; // 10 ms tick period

    ShardedSubscriberRegistry<32> reg;
    reg.reserve(N);
    auto t0 = std::chrono::steady_clock::now();
    for (uint32_t i = 1; i <= N; ++i) {
        if (!reg.createByTMSI(i)) {
            std::printf("FAIL: create session #%u\n", i);
            return 1;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    double createMs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0;
    std::printf("create 1M sessions: %.1f ms\n", createMs);

    std::vector<SubscriberSession*> sessions(N);
    for (uint32_t i = 0; i < N; ++i) sessions[i] = reg.findByTMSI(i + 1);

    // Pre-encode one L3 message (Channel Release) for the tick loop.
    ParsedMessage pm{RRM{L3ChannelRelease::builder().cause(RRCause::Normal_Event).build()}};
    auto wireOpt = writeL3Bytes(pm);
    if (!wireOpt) { std::printf("FAIL: writeL3Bytes\n"); return 1; }
    std::vector<uint8_t> wire = *wireOpt;

    int64_t tickMin = INT64_MAX, tickMax = 0;
    int64_t overruns = 0;
    int64_t totalNs = 0;
    auto loopStart = std::chrono::steady_clock::now();
    for (int t = 0; t < TICKS; ++t) {
        auto tickStart = std::chrono::steady_clock::now();
        std::array<TimerExpiry, 1024> expired{};
        reg.tickAllTimers(std::chrono::milliseconds(10), expired);
        reg.tickAllProcedures(std::chrono::milliseconds(10));
        int base = (t * MSGS_PER_TICK) % static_cast<int>(N);
        for (int m = 0; m < MSGS_PER_TICK; ++m) {
            SubscriberSession* s = sessions[static_cast<size_t>(base + m) % N];
            auto parsed = parseL3(std::span<const uint8_t>(wire.data(), wire.size()));
            if (parsed) s->procedures.feed(*parsed, s, {});
        }
        auto tickEnd = std::chrono::steady_clock::now();
        int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(tickEnd - tickStart).count();
        tickMin = std::min(tickMin, ns);
        tickMax = std::max(tickMax, ns);
        totalNs += ns;
        if (ns > PERIOD_NS) ++overruns;
    }
    auto loopEnd = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration_cast<std::chrono::microseconds>(loopEnd - loopStart).count() / 1e6;
    double totalMsgs = static_cast<double>(TICKS) * MSGS_PER_TICK;

    std::printf("tick: avg %.3f ms, min %.3f ms, max %.3f ms, overruns %" PRId64 "/%d\n",
                totalNs / 1e6 / TICKS, tickMin / 1e6, tickMax / 1e6, overruns, TICKS);
    std::printf("throughput: %.2f M msg/s\n", totalMsgs / elapsedSec / 1e6);
    double peakGB = peakWorkingSetGB();
    std::printf("peak working set: %.2f GB\n", peakGB);

    if (overruns > 0) {
        std::printf("FAIL: %" PRId64 " tick(s) exceeded the 10 ms period\n", overruns);
        return 1;
    }
    std::printf("RT SCALE OK\n");
    return 0;
}
