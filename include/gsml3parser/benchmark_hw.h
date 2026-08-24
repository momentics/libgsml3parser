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

/// Benchmark hardware identification (unified for all benchmark tests/examples).
///
/// Performance numbers depend on CPU/RAM/OS, so every benchmark test and
/// example prints the hardware it ran on. This header is the single, unified
/// source of that information: hardwareInfo()/hardwareId() detect the machine
/// dynamically at runtime (first call) and cache the result for the process
/// lifetime, so all benchmark tests in one run report the identical data.
///
/// Reported fields: CPU brand, base clock (MHz), sockets, physical cores,
/// logical processors, L1/L2/L3 cache sizes, total RAM (GB), populated
/// memory slots, configured memory clock (MHz), and OS.
///
/// hardwareId() format:
///   "<brand> | base <MHz> MHz | <S> socket(s), <C> cores, <L> logical
///    | L1 <...> | L2 <...> | L3 <...> | <RAM> GB RAM[, <slots> slots @ <MHz>]
///    | <OS>"
///
/// Sources (all runtime, no external dependencies):
///   Windows: CPUID (brand/topology/caches), registry (base speed, OS
///            version), GetPhysicallyInstalledSystemMemory (RAM), raw SMBIOS
///            table via GetSystemFirmwareTable (memory slots/frequency).
///   Linux:   /proc/cpuinfo, /sys/devices/system/cpu/*/topology, /sys cpufreq
///            (base speed), /sys cpu cache index files, /proc/meminfo (RAM),
///            /sys edac (memory slots, best effort), /etc/os-release (OS).
///
/// NOTE: <windows.h> is deliberately NOT included: it pollutes the
/// preprocessor with legacy macros (e.g. nb30.h defines DEREGISTERED), which
/// breaks C++ identifiers in translation units that include this header.
/// Instead, minimal extern "C" declarations matching the SDK signatures are
/// used, so the header is redeclaration-compatible with <windows.h>.
///
/// This is a benchmarking utility, not part of the zero-allocation hot path;
/// it may allocate (std::string, std::vector).
#pragma once

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <intrin.h>
#elif defined(__linux__)
#include <cctype>
#include <dirent.h>
#include <set>
#include <utility>
#endif

#if defined(_WIN32)
// HKEY must match winreg.h (pointer to an incomplete struct) for
// redeclaration compatibility.
struct HKEY__;
typedef struct HKEY__* HKEY;

#if defined(_M_IX86)
#define BHW_CALL __stdcall
#else
#define BHW_CALL
#endif
#endif

namespace gsml3parser::benchmark {

/// Snapshot of the detected hardware.
struct HardwareInfo {
    std::string cpuBrand;         ///< e.g. "Intel(R) Core(TM) i9-9900K CPU @ 3.60GHz".
    unsigned long baseFreqMhz{0}; ///< Base clock in MHz (0 = unknown).
    unsigned sockets{0};          ///< Number of CPU sockets.
    unsigned cores{0};            ///< Physical cores, total across all sockets.
    unsigned logical{0};          ///< Logical processors (hardware concurrency).
    unsigned long l1DataBytes{0}; ///< L1 data cache per core (0 = unknown).
    unsigned long l1InstrBytes{0};///< L1 instruction cache per core (0 = unknown).
    unsigned long l2Bytes{0};     ///< L2 cache per core, or per socket when l2Shared.
    bool l2Shared{false};         ///< True when L2 is shared (reported per socket).
    unsigned long l3Bytes{0};     ///< L3 cache per socket (0 = unknown).
    unsigned long totalRamGb{0};  ///< Total physical RAM in whole GB (0 = unknown).
    unsigned ramSlots{0};         ///< Populated memory slots (0 = unknown).
    unsigned ramTotalSlots{0};    ///< Memory slots on the board (0 = unknown).
    unsigned long ramFreqMhz{0};  ///< Configured memory clock in MHz (0 = unknown).
    std::string os;               ///< e.g. "Windows 10.0 (build 19044)".
};

namespace detail {

#if defined(_WIN32)
// ── Minimal Win32 declarations (see header NOTE) ─────────────────────
// The registry functions live in advapi32.dll; pull it in so translation
// units that include this header link without an explicit dependency.
#pragma comment(lib, "advapi32.lib")
extern "C" {
// kernel32.dll — total physical memory in kilobytes (Vista+).
__declspec(dllimport) int BHW_CALL GetPhysicallyInstalledSystemMemory(unsigned long long* totalMemoryKb);
// kernel32.dll — raw SMBIOS/ACPI firmware table (signature 'RSMB').
__declspec(dllimport) unsigned int BHW_CALL GetSystemFirmwareTable(unsigned long signature, unsigned long tableId, void* buffer, unsigned long bufferSize);
// kernel32.dll (winreg) — registry access (base speed, OS version).
__declspec(dllimport) long BHW_CALL RegOpenKeyExW(HKEY key, const wchar_t* subKey, unsigned long options, unsigned long sam, HKEY* result);
__declspec(dllimport) long BHW_CALL RegQueryValueExW(HKEY key, const wchar_t* valueName, unsigned long* reserved, unsigned long* type, unsigned char* data, unsigned long* size);
__declspec(dllimport) long BHW_CALL RegCloseKey(HKEY key);
}

// CPU brand string from CPUID leaves 0x80000002..0x80000004 (48 chars max).
// Also extracts the base clock when the brand ends with "@ X.XXGHz".
inline std::string cpuBrandString(HardwareInfo& h) {
#if defined(_M_X64) || defined(_M_IX86)
    int info[4] = {0, 0, 0, 0};
    __cpuidex(info, 0x80000000, 0);
    if (info[0] < 0x80000004) return "";
    char brand[49] = {};
    __cpuidex(info, 0x80000002, 0);
    std::memcpy(brand + 0, info, 16);
    __cpuidex(info, 0x80000003, 0);
    std::memcpy(brand + 16, info, 16);
    __cpuidex(info, 0x80000004, 0);
    std::memcpy(brand + 32, info, 16);
    std::string s(brand);
    // Intel pads the brand string with spaces; trim both ends.
    const auto first = s.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(' ');
    s = s.substr(first, last - first + 1);
    const auto at = s.rfind("@ ");
    const auto ghz = s.rfind("GHz");
    if (at != std::string::npos && ghz != std::string::npos && ghz > at) {
        const double ghzValue = std::strtod(s.c_str() + at + 2, nullptr);
        if (ghzValue > 0.0) {
            h.baseFreqMhz = static_cast<unsigned long>(ghzValue * 1000.0 + 0.5);
        }
    }
    return s;
#else
    (void)h;
    return "";
#endif
}

// Sockets/cores/logical. Logical from hardware_concurrency (reliable). SMT
// from CPUID leaf 1. Sockets/cores from CPUID 0x0B only when internally
// consistent with the logical count (guards against emulated/masked CPUID in
// VMs); otherwise derived from SMT assuming a single socket.
inline void readCpuTopology(HardwareInfo& h) {
    h.logical = std::thread::hardware_concurrency();
    if (h.logical == 0) h.logical = 1;
    bool smt = false;
#if defined(_M_X64) || defined(_M_IX86)
    int info[4] = {0, 0, 0, 0};
    __cpuidex(info, 1, 0);
    smt = ((info[2] >> 28) & 1) != 0;
    __cpuidex(info, 0, 0);
    if (info[0] >= 0x0B) {
        unsigned threadsPerCore = 0, coresPerSocket = 0;
        for (unsigned sub = 0; sub < 8; ++sub) {
            __cpuidex(info, 0x0B, sub);
            const int levelType = info[2] & 0xFF;  // 1=SMT, 2=Core
            if (levelType == 0) break;
            if (levelType == 1 && info[0] > 0) {
                threadsPerCore = static_cast<unsigned>(info[0]);
            } else if (levelType == 2 && info[0] > 0) {
                coresPerSocket = static_cast<unsigned>(info[0]);
            }
        }
        if (threadsPerCore > 0 && coresPerSocket > 0) {
            const unsigned perSocket = coresPerSocket * threadsPerCore;
            if (perSocket > 0 && h.logical % perSocket == 0) {
                h.sockets = h.logical / perSocket;
                h.cores = h.sockets * coresPerSocket;
                return;
            }
        }
    }
#endif
    // Fallback / VM path: derive cores from SMT, assume a single socket.
    const unsigned threadsPerCore = smt ? 2u : 1u;
    h.sockets = 1;
    h.cores = h.logical / threadsPerCore;
    if (h.cores == 0) h.cores = 1;
}

// L1/L2/L3 sizes from CPUID leaf 0x04 (Deterministic Cache Parameters).
// The subleaf count and each descriptor are validated; emulated/masked CPUID
// in VMs often returns garbage, in which case the fields stay 0 ("n/a").
inline void readCaches(HardwareInfo& h) {
#if defined(_M_X64) || defined(_M_IX86)
    int info[4] = {0, 0, 0, 0};
    __cpuidex(info, 4, 0);
    const int subleaves = info[0];
    if (subleaves <= 0 || subleaves > 32) return;  // invalid count
    for (int sub = 0; sub < subleaves; ++sub) {
        __cpuidex(info, 4, static_cast<unsigned>(sub));
        const int type = (info[0] >> 5) & 0x7;     // 0=Data, 1=Instruction, 2=Unified
        const int level = (info[0] >> 10) & 0x7;   // 1, 2, 3
        if (level < 1 || level > 3) continue;      // invalid descriptor
        const int index = (info[0] >> 22) & 0x1F;  // 0=private, N=shared by N cores
        const int sets = (info[1] & 0xFF) + 1;
        const int partitions = ((info[1] >> 8) & 0x1FF) + 1;
        const int ways = ((info[1] >> 16) & 0x7F) + 1;
        int lineSize = (info[2] >> 24) & 0xFF;     // bytes (0 on very old CPUs)
        if (lineSize == 0) lineSize = ((info[2] & 0xFF) + 1) * 64;
        const unsigned long size = static_cast<unsigned long>(sets) *
                                   static_cast<unsigned long>(partitions) *
                                   static_cast<unsigned long>(ways) *
                                   static_cast<unsigned long>(lineSize);
        if (size == 0 || size > (1ul << 30)) continue;  // sanity: <= 1 GB
        if (level == 1 && type == 0) {
            h.l1DataBytes = size;
        } else if (level == 1 && type == 1) {
            h.l1InstrBytes = size;
        } else if (level == 1 && type == 2) {
            if (h.l1DataBytes == 0) h.l1DataBytes = size;  // unified L1
        } else if (level == 2) {
            h.l2Bytes = size;
            h.l2Shared = index != 0;  // shared group: the size is per group/socket
        } else if (level == 3) {
            if (h.l3Bytes == 0) h.l3Bytes = size;  // all cores report the same shared size
        }
    }
#endif
}

// HKEY_LOCAL_MACHINE pseudo-handle. winreg.h defines it as
// ((HKEY)(ULONG_PTR)((LONG)0x80000002)); on 64-bit the (LONG) value is
// sign-extended, so the handle is 0xFFFFFFFF80000002 there.
inline HKEY localMachineKey() {
#if defined(_M_X64) || defined(_M_ARM64)
    return reinterpret_cast<HKEY>(0xFFFFFFFF80000002ull);
#else
    return reinterpret_cast<HKEY>(0x80000002ull);
#endif
}

inline HKEY openLmSubKey(const wchar_t* subKey) {
    HKEY hk = nullptr;
    if (RegOpenKeyExW(localMachineKey(), subKey, 0, 0x20019u /* KEY_READ */, &hk) != 0) {
        return nullptr;
    }
    return hk;
}

// Read a REG_DWORD value under HKLM\<subKey> (0 on failure).
inline unsigned long registryDword(const wchar_t* subKey, const wchar_t* value) {
    HKEY hk = openLmSubKey(subKey);
    if (!hk) return 0;
    unsigned long val = 0;
    unsigned long type = 0;
    unsigned long size = sizeof(val);
    const long status = RegQueryValueExW(hk, value, nullptr, &type,
                                         reinterpret_cast<unsigned char*>(&val), &size);
    RegCloseKey(hk);
    return (status == 0 && type == 4 /* REG_DWORD */) ? val : 0;
}

// Read a REG_SZ value under HKLM\<subKey> (empty string on failure).
inline std::string registryString(const wchar_t* subKey, const wchar_t* value) {
    HKEY hk = openLmSubKey(subKey);
    if (!hk) return "";
    wchar_t buf[256] = {};
    unsigned long type = 0;
    unsigned long size = sizeof(buf);
    const long status = RegQueryValueExW(hk, value, nullptr, &type,
                                         reinterpret_cast<unsigned char*>(buf), &size);
    RegCloseKey(hk);
    if (status != 0 || type != 1 /* REG_SZ */) return "";
    std::string out;
    const wchar_t* s = buf;
    while (*s && static_cast<unsigned short>(*s) < 128) {
        out += static_cast<char>(*s);
        ++s;
    }
    return out;
}

// Base clock fallback: "~MHz" of the first processor in the registry.
inline unsigned long registryBaseFreqMhz() {
    return registryDword(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"~MHz");
}

inline std::string osNameWindows() {
    const std::wstring subKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    // Prefer the numeric major/minor DWORDs over the "CurrentVersion" string,
    // which can be stale/inconsistent on some images.
    const unsigned long major = registryDword(subKey.c_str(), L"CurrentMajorVersionNumber");
    const unsigned long minor = registryDword(subKey.c_str(), L"CurrentMinorVersionNumber");
    const std::string build = registryString(subKey.c_str(), L"CurrentBuildNumber");
    std::string s = "Windows";
    if (major > 0) {
        s += " " + std::to_string(major) + "." + std::to_string(minor);
    }
    if (!build.empty()) s += " (build " + build + ")";
    return s;
}

// Total physical RAM (kernel32, Vista+).
inline void readTotalRam(HardwareInfo& h) {
    unsigned long long totalKb = 0;
    if (GetPhysicallyInstalledSystemMemory(&totalKb) != 0 && totalKb > 0) {
        h.totalRamGb = static_cast<unsigned long>(totalKb / (1024ull * 1024ull));
    }
}

// Raw SMBIOS 'RSMB' signature (the MSVC 'RSMB' multi-char constant).
constexpr unsigned long kRsmbSignature = 0x52534D42;

// Memory slots + configured clock from the raw SMBIOS table (type 16/17).
// Handles three layouts: SMBIOS 3.x entry point (anchor "_SM_3._"), SMBIOS
// 2.x entry point (anchor "_SM_"), and the raw 'RSMB' table (8-byte header,
// structure table at offset 8). Field offsets are chosen by range validation
// rather than the (sometimes unreliable) SMBIOS version.
inline void readSmbiosMemory(HardwareInfo& h) {
    const unsigned long tableSize = GetSystemFirmwareTable(kRsmbSignature, 0, nullptr, 0);
    if (tableSize < 16) return;
    std::vector<unsigned char> buf(tableSize);
    if (GetSystemFirmwareTable(kRsmbSignature, 0, buf.data(), tableSize) != tableSize) return;

    const unsigned char* table = nullptr;
    unsigned long tableLen = 0;
    if (std::memcmp(buf.data(), "_SM_3.", 6) == 0) {
        table = buf.data() + 0x12;
        tableLen = static_cast<unsigned long>(buf[0x10]) |
                   (static_cast<unsigned long>(buf[0x11]) << 8);
    } else if (std::memcmp(buf.data(), "_SM_", 4) == 0) {
        table = buf.data() + 0x13;
        tableLen = static_cast<unsigned long>(buf[0x11]) |
                   (static_cast<unsigned long>(buf[0x12]) << 8);
    } else {
        // Raw 'RSMB' table: 8-byte header, structure table at offset 8.
        table = buf.data() + 0x08;
        tableLen = tableSize - 0x08;
    }
    const unsigned char* end = buf.data() + tableSize;
    if (tableLen > 0) {
        const unsigned char* byLen = table + tableLen;
        if (byLen < end) end = byLen;
    }

    const unsigned char* p = table;
    while (p + 4 <= end) {
        const unsigned char type = p[0];
        const unsigned char fmtLen = p[1];
        if (fmtLen < 4 || type > 127) break;  // malformed structure
        // The formatted area is followed by NUL-terminated strings that end
        // with a double NUL; the next structure starts right after it.
        const unsigned char* s = p + fmtLen;
        while (s + 1 < end && !(s[0] == 0 && s[1] == 0)) ++s;
        if (s + 2 > end) break;
        const unsigned char* next = s + 2;

        if (type == 17) {  // Memory Device
            // Populated if any size field (3.x 4-byte or 2.x 2-byte) is non-zero.
            bool populated = false;
            if (fmtLen >= 12) {
                const unsigned long size4 = p[8] |
                    (static_cast<unsigned long>(p[9]) << 8) |
                    (static_cast<unsigned long>(p[10]) << 16) |
                    (static_cast<unsigned long>(p[11]) << 24);
                if (size4 != 0) populated = true;
            }
            if (!populated && fmtLen >= 10 && (p[8] | p[9]) != 0) {
                populated = true;
            }
            if (populated) {
                ++h.ramSlots;
                // Configured memory clock in MHz: try candidate offsets and take
                // the first reasonable value (100..10000 MHz).
                const unsigned long candidates[3] = {
                    (fmtLen >= 23) ? (p[21] | (static_cast<unsigned long>(p[22]) << 8)) : 0,
                    (fmtLen >= 16) ? (p[14] | (static_cast<unsigned long>(p[15]) << 8)) : 0,
                    (fmtLen >= 14) ? (p[12] | (static_cast<unsigned long>(p[13]) << 8)) : 0,
                };
                for (const unsigned long c : candidates) {
                    if (c >= 100 && c <= 10000) {
                        if (c > h.ramFreqMhz) h.ramFreqMhz = c;
                        break;
                    }
                }
            }
        } else if (type == 16) {  // Physical Memory Array
            // Number of memory devices (best effort: 3.x then 2.x offset).
            unsigned long total = 0;
            if (fmtLen >= 11) total = p[9] | (static_cast<unsigned long>(p[10]) << 8);
            if ((total < 1 || total > 64) && fmtLen >= 9) {
                total = p[7] | (static_cast<unsigned long>(p[8]) << 8);
            }
            if (total >= 1 && total <= 64) h.ramTotalSlots = total;
        }
        p = next;
    }
}

inline HardwareInfo detectHardware() {
    HardwareInfo h;
    h.cpuBrand = cpuBrandString(h);
    readCpuTopology(h);
    readCaches(h);
    if (h.baseFreqMhz == 0) h.baseFreqMhz = registryBaseFreqMhz();
    readTotalRam(h);
    readSmbiosMemory(h);
    h.os = osNameWindows();
    return h;
}

#else  // !defined(_WIN32)

#if defined(__linux__)
// Read a single-line sysfs/procfs file, trimmed (empty on failure).
inline std::string readSysLine(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return "";
    char buf[256];
    const bool ok = std::fgets(buf, sizeof(buf), f) != nullptr;
    std::fclose(f);
    if (!ok) return "";
    std::string s(buf);
    const auto last = s.find_last_not_of(" \t\r\n");
    return last == std::string::npos ? "" : s.substr(0, last + 1);
}

// Parse a sysfs cache size like "32K", "1M", "8G".
inline unsigned long parseCacheSize(const std::string& s) {
    unsigned long v = 0;
    size_t i = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + static_cast<unsigned long>(s[i] - '0');
        ++i;
    }
    if (i < s.size()) {
        switch (s[i]) {
            case 'K': case 'k': v *= 1024ul; break;
            case 'M': case 'm': v *= 1024ul * 1024ul; break;
            case 'G': case 'g': v *= 1024ul * 1024ul * 1024ul; break;
            default: break;
        }
    }
    return v;
}

inline HardwareInfo detectHardware() {
    HardwareInfo h;
    // Brand + logical count from /proc/cpuinfo.
    std::FILE* f = std::fopen("/proc/cpuinfo", "r");
    if (f) {
        char buf[512];
        while (std::fgets(buf, sizeof(buf), f)) {
            if (std::strncmp(buf, "processor", 9) == 0) {
                ++h.logical;
            } else if (h.cpuBrand.empty() &&
                       (std::strncmp(buf, "model name", 10) == 0 ||
                        std::strncmp(buf, "Hardware", 8) == 0 ||
                        std::strncmp(buf, "Model", 5) == 0)) {
                const char* colon = std::strchr(buf, ':');
                if (colon) {
                    std::string v = colon + 1;
                    const auto first = v.find_first_not_of(" \t");
                    if (first != std::string::npos) {
                        v = v.substr(first);
                        const auto last = v.find_last_not_of(" \t\r\n");
                        if (last != std::string::npos) v = v.substr(0, last + 1);
                        if (!v.empty()) h.cpuBrand = v;
                    }
                }
            }
        }
        std::fclose(f);
    }
    if (h.logical == 0) h.logical = std::thread::hardware_concurrency();
    if (h.logical == 0) h.logical = 1;

    // Sockets/cores from /sys topology: unique package IDs and unique
    // (package, core_id) pairs.
    std::set<int> packages;
    std::set<std::pair<int, int>> cores;
    DIR* dir = opendir("/sys/devices/system/cpu");
    if (dir) {
        while (const dirent* e = readdir(dir)) {
            if (std::strncmp(e->d_name, "cpu", 3) != 0) continue;
            if (!std::isdigit(static_cast<unsigned char>(e->d_name[3]))) continue;
            const std::string base =
                std::string("/sys/devices/system/cpu/") + e->d_name + "/topology/";
            const std::string pkg = readSysLine(base + "physical_package_id");
            const std::string coreId = readSysLine(base + "core_id");
            if (pkg.empty() || coreId.empty()) continue;
            const int p = std::atoi(pkg.c_str());
            const int cid = std::atoi(coreId.c_str());
            packages.insert(p);
            cores.insert({p, cid});
        }
        closedir(dir);
    }
    h.sockets = packages.empty() ? 1 : static_cast<unsigned>(packages.size());
    h.cores = cores.empty() ? h.logical / h.sockets : static_cast<unsigned>(cores.size());
    if (h.cores == 0) h.cores = 1;

    // Base speed: cpufreq base_frequency (kHz); fallback: current "cpu MHz".
    const std::string bf = readSysLine("/sys/devices/system/cpu/cpu0/cpufreq/base_frequency");
    if (!bf.empty()) {
        const long khz = std::atol(bf.c_str());
        if (khz > 0) h.baseFreqMhz = static_cast<unsigned long>(khz / 1000);
    }
    if (h.baseFreqMhz == 0) {
        std::FILE* f2 = std::fopen("/proc/cpuinfo", "r");
        if (f2) {
            char buf[512];
            while (std::fgets(buf, sizeof(buf), f2)) {
                if (std::strncmp(buf, "cpu MHz", 7) == 0) {
                    h.baseFreqMhz = static_cast<unsigned long>(std::atof(buf + 7));
                    break;
                }
            }
            std::fclose(f2);
        }
    }

    // Caches from /sys/devices/system/cpu/cpu0/cache/indexN/.
    for (int idx = 0; idx < 16; ++idx) {
        const std::string base =
            "/sys/devices/system/cpu/cpu0/cache/index" + std::to_string(idx) + "/";
        const std::string levelStr = readSysLine(base + "level");
        if (levelStr.empty()) break;
        const int level = std::atoi(levelStr.c_str());
        const unsigned long size = parseCacheSize(readSysLine(base + "size"));
        const std::string type = readSysLine(base + "type");
        if (level == 1) {
            if (type == "Instruction") h.l1InstrBytes = size;
            else h.l1DataBytes = size;  // Data or unified L1
        } else if (level == 2) {
            h.l2Bytes = size;  // private per core on x86
        } else if (level == 3) {
            if (h.l3Bytes == 0) h.l3Bytes = size;  // shared: full size per socket
        }
    }

    // Total RAM from /proc/meminfo.
    std::FILE* mf = std::fopen("/proc/meminfo", "r");
    if (mf) {
        char buf[256];
        while (std::fgets(buf, sizeof(buf), mf)) {
            if (std::strncmp(buf, "MemTotal:", 9) == 0) {
                const long kb = std::strtol(buf + 9, nullptr, 10);
                if (kb > 0) h.totalRamGb = static_cast<unsigned long>(kb / (1024 * 1024));
                break;
            }
        }
        std::fclose(mf);
    }

    // Populated DIMM slots via edac sysfs (best effort; often unavailable).
    DIR* mcDir = opendir("/sys/devices/system/edac/mc");
    if (mcDir) {
        while (const dirent* mc = readdir(mcDir)) {
            if (std::strncmp(mc->d_name, "mc", 2) != 0) continue;
            if (!std::isdigit(static_cast<unsigned char>(mc->d_name[2]))) continue;
            const std::string csPath =
                std::string("/sys/devices/system/edac/mc/") + mc->d_name + "/csrow";
            DIR* csDir = opendir(csPath.c_str());
            if (!csDir) continue;
            while (const dirent* cs = readdir(csDir)) {
                if (std::strncmp(cs->d_name, "csrow", 5) != 0) continue;
                const std::string dimmPath = csPath + "/" + cs->d_name + "/dimm";
                DIR* dimmDir = opendir(dimmPath.c_str());
                if (!dimmDir) continue;
                while (const dirent* d = readdir(dimmDir)) {
                    if (std::strncmp(d->d_name, "dimm", 4) != 0) continue;
                    const std::string sz =
                        readSysLine(dimmPath + "/" + d->d_name + "/dimm_size");
                    if (!sz.empty() && std::atol(sz.c_str()) > 0) ++h.ramSlots;
                }
                closedir(dimmDir);
            }
            closedir(csDir);
        }
        closedir(mcDir);
    }

    // OS: PRETTY_NAME from /etc/os-release (fallback: uname).
    std::FILE* of = std::fopen("/etc/os-release", "r");
    std::string os;
    if (of) {
        char buf[512];
        while (std::fgets(buf, sizeof(buf), of)) {
            if (std::strncmp(buf, "PRETTY_NAME=", 14) == 0) {
                os = buf + 14;
                const auto last = os.find_last_not_of(" \t\r\n\"");
                if (last != std::string::npos) os = os.substr(0, last + 1);
                break;
            }
        }
        std::fclose(of);
    }
    if (os.empty()) {
        utsname u{};
        if (uname(&u) == 0) {
            os = std::string(u.sysname) + " " + u.release;
        } else {
            os = "unknown OS";
        }
    }
    h.os = os;
    return h;
}

#else  // neither _WIN32 nor __linux__

inline HardwareInfo detectHardware() {
    HardwareInfo h;
    h.logical = std::thread::hardware_concurrency();
    if (h.logical == 0) h.logical = 1;
    h.sockets = 1;
    h.cores = h.logical;
    h.os = "unknown OS";
    return h;
}

#endif  // __linux__

#endif  // _WIN32

// Format a byte count as "64K", "256K", "16M", "1G" ("n/a" for 0).
inline std::string fmtBytes(unsigned long bytes) {
    if (bytes == 0) return "n/a";
    if (bytes >= (1ul << 30) && bytes % (1ul << 30) == 0) {
        return std::to_string(bytes / (1ul << 30)) + "G";
    }
    if (bytes >= (1ul << 20) && bytes % (1ul << 20) == 0) {
        return std::to_string(bytes / (1ul << 20)) + "M";
    }
    if (bytes >= 1024ul && bytes % 1024ul == 0) {
        return std::to_string(bytes / 1024ul) + "K";
    }
    return std::to_string(bytes) + "B";
}

} // namespace detail

/// Detects the hardware this process runs on.
///
/// Detection happens once, on the first call (function-local static cache);
/// every subsequent call — from any benchmark test or example in the same
/// run — returns the identical data, so all results are attributed to the
/// same machine.
[[nodiscard]] inline const HardwareInfo& hardwareInfo() {
    static const HardwareInfo info = detail::detectHardware();
    return info;
}

/// Formats the detected hardware as a single attribution string.
/// See the header comment for the exact format.
[[nodiscard]] inline std::string hardwareId() {
    static const std::string id = [] {
        const HardwareInfo& h = hardwareInfo();
        std::string s;
        s += h.cpuBrand.empty() ? "unknown CPU" : h.cpuBrand;
        s += " | base ";
        s += h.baseFreqMhz > 0 ? std::to_string(h.baseFreqMhz) + " MHz"
                               : std::string("unknown");
        s += " | " + std::to_string(h.sockets) + " socket" + (h.sockets == 1 ? "" : "s")
           + ", " + std::to_string(h.cores) + " core" + (h.cores == 1 ? "" : "s")
           + ", " + std::to_string(h.logical) + " logical";
        s += " | L1 ";
        if (h.l1DataBytes > 0 && h.l1InstrBytes > 0) {
            s += detail::fmtBytes(h.l1DataBytes) + " D + " +
                 detail::fmtBytes(h.l1InstrBytes) + " I/core";
        } else if (h.l1DataBytes > 0 || h.l1InstrBytes > 0) {
            s += detail::fmtBytes(h.l1DataBytes > 0 ? h.l1DataBytes : h.l1InstrBytes)
                 + "/core";
        } else {
            s += "n/a";
        }
        s += " | L2 ";
        if (h.l2Bytes > 0) {
            s += detail::fmtBytes(h.l2Bytes) + (h.l2Shared ? "/socket" : "/core");
        } else {
            s += "n/a";
        }
        s += " | L3 ";
        if (h.l3Bytes > 0) {
            s += detail::fmtBytes(h.l3Bytes) + "/socket";
        } else {
            s += "n/a";
        }
        s += " | " + (h.totalRamGb > 0 ? std::to_string(h.totalRamGb) + " GB RAM"
                                       : std::string("RAM n/a"));
        if (h.ramSlots > 0) {
            s += ", " + std::to_string(h.ramSlots) + " slot" + (h.ramSlots == 1 ? "" : "s");
            if (h.ramTotalSlots > 0 && h.ramTotalSlots != h.ramSlots) {
                s += "/" + std::to_string(h.ramTotalSlots);
            }
            if (h.ramFreqMhz > 0) {
                s += " @ " + std::to_string(h.ramFreqMhz) + " MHz";
            }
        }
        s += " | " + h.os;
        return s;
    }();
    return id;
}

/// Prints the hardware ID to stdout (single line).
/// Used by benchmark tests and examples to attribute their results to the
/// machine that produced them.
inline void printHardwareId() {
    std::printf("%s\n", hardwareId().c_str());
}

} // namespace gsml3parser::benchmark

#if defined(_WIN32)
#undef BHW_CALL
#endif
