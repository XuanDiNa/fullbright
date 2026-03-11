#include <windows.h>
#include <intrin.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#if __has_include("detours/detours.h")
#include "detours/detours.h"
#define FULLBRIGHT_HAS_DETOURS 1
#else
#define FULLBRIGHT_HAS_DETOURS 0
#endif

namespace {

char const* kDefaultGetGammaPatternShort =
    "48 83 EC 28 48 8B 01 48 8D 54 24 30 41 B8 36 00 00 00 48 8B 40 08 FF 15 ? ? ? ? 48 8B 10 48 8B 42 08 48 8B 88 B8 01 00 00 48 85 C9 74 ? E8 ? ? ? ? 48 83 C4 28 C3 F3 0F 10 42 18";
char const* kDefaultGetGammaPatternLong =
    "48 83 EC ? 80 B9 ? ? ? ? ? 48 8D 54 24 ? 48 8B 01 74 ? 41 B8 ? ? ? ? FF 50 ? 48 8B 10 48 85 D2 74 ? 48 8B 42 ? 48 8B 88 ? ? ? ? 48 85 C9 74 ? E8 ? ? ? ? 48 83 C4 ? C3 F3 0F 10 42 ? 48 83 C4 ? C3 41 B8 ? ? ? ? FF 50 ? 48 8B 10 48 85 D2 75 ? E8 ? ? ? ? CC E8 ? ? ? ? CC CC CC CC CC CC CC CC CC CC F3 0F 11 4C 24";
char const* kFallbackGetGammaPatternLatite =
    "48 83 EC ? 48 8B 01 48 8D 54 ? ? 41 B8 36 00 00 00";
char const* kFallbackGetGammaPatternAltA =
    "48 83 EC ? 48 8B 01 48 8D 54 24 ? 41 B8 36 00 00 00 48 8B";
char const* kFallbackGetGammaPatternAltB =
    "48 83 EC ? 48 8B 01 48 8D 54 ? ? 41 B8 36 00 00 00 48 8B 40 ? FF 15";
char const* kDefaultScreenRenderPattern =
    "48 89 5C 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 BC 24 ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 4C 8B FA";
char const* kDefaultLevelRenderPattern =
    "48 8B C4 48 89 58 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 70 ? 0F 29 78 ? 44 0F 29 40 ? 44 0F 29 48 ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 4D 8B E8 4C 8B E2 4C 8B F9";

std::atomic<bool> g_enabled{true};
std::atomic<bool> g_runtimeEnabled{true};
std::atomic<bool> g_patternScanEnabled{true};
std::atomic<double> g_forceBrightness{10.0};
std::atomic<unsigned long long> g_hookTargetRva{0};
char g_hookPattern[256]{};
std::atomic<int> g_hookPatternOffset{0};
std::atomic<int> g_candidateTrialIndex{0};
std::atomic<unsigned long long> g_callerFilterRva{0};
std::atomic<bool> g_phaseGateEnabled{true};
std::atomic<bool> g_phaseGateFallbackToCaller{false};
char g_phaseScreenPattern[256]{};
char g_phaseLevelPattern[256]{};
char g_toggleHotkey[64] = "F8";
std::atomic<int> g_toggleVirtualKey{VK_F8};
std::atomic<bool> g_toggleNeedCtrl{false};
std::atomic<bool> g_toggleNeedAlt{false};
std::atomic<bool> g_toggleNeedShift{false};
std::atomic<bool> g_toggleHotkeyValid{true};
std::atomic<bool> g_toggleHotkeyWasDown{false};
std::atomic<bool> g_hotkeyThreadStop{false};
HANDLE g_hotkeyThread = nullptr;

std::atomic<bool> g_hookActive{false};
std::atomic<unsigned int> g_hookHitCount{0};
std::atomic<bool> g_directPatchActive{false};
void* g_directPatchAddr = nullptr;
size_t g_directPatchLen = 0;
unsigned char g_directPatchBackup[16]{};
std::atomic<ULONGLONG> g_hookAttachTick{0};
std::atomic<unsigned int> g_noHitCycleCount{0};
std::atomic<unsigned int> g_outlierOriginalSamples{0};
std::atomic<unsigned long long> g_lastNoHitRva{0};
std::atomic<unsigned int> g_sameNoHitRvaCount{0};
std::atomic<unsigned long long> g_rejectedCandidateRva{0};
std::atomic<bool> g_cycleExhaustedLogged{false};
std::atomic<bool> g_watchdogStop{false};
HANDLE g_watchdogThread = nullptr;

char g_jsonPath[MAX_PATH]{};
char g_logPath[MAX_PATH]{};
char g_jsonPathAlt[MAX_PATH]{};
char g_logPathAlt[MAX_PATH]{};

using GetGammaFn = float(__fastcall*)(void*, void*);
GetGammaFn g_getGammaOrig = nullptr;
using ScreenRenderFn = void(__fastcall*)(void*, void*);
using LevelRenderFn = void(__fastcall*)(void*, void*, void*);
ScreenRenderFn g_screenRenderOrig = nullptr;
LevelRenderFn g_levelRenderOrig = nullptr;

std::atomic<bool> g_initialized{false};
HINSTANCE g_moduleHandle = nullptr;
std::atomic<unsigned int> g_callerSampleCount{0};
std::atomic<bool> g_filterObserveLogged{false};
std::atomic<unsigned int> g_phaseScreenHitCount{0};
std::atomic<unsigned int> g_phaseLevelHitCount{0};
std::atomic<bool> g_phaseFallbackApplied{false};
std::atomic<int> g_uiRenderDepth{0};
std::atomic<int> g_worldRenderDepth{0};
std::atomic<bool> g_phaseHooksLogged{false};

bool InstallPhaseHooks();
void UninstallPhaseHooks();
bool InstallGetGammaHook();
void UninstallGetGammaHook();
DWORD WINAPI CandidateWatchdogThread(LPVOID);
void ParseAndApplyToggleHotkey();
DWORD WINAPI HotkeyPollThread(LPVOID);
void ApplyRuntimeEnabled(bool enabled);

void DebugLog(char const* fmt, ...) {
    char buffer[1024]{};
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(buffer, _countof(buffer), _TRUNCATE, fmt, args);
    va_end(args);
    OutputDebugStringA(buffer);

    auto appendFile = [&](char const* path) {
        if (!path || path[0] == '\0') return;
        HANDLE h = CreateFileA(
            path,
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        if (h == INVALID_HANDLE_VALUE) return;
        DWORD written = 0;
        (void)WriteFile(h, buffer, static_cast<DWORD>(strlen(buffer)), &written, nullptr);
        CloseHandle(h);
    };

    appendFile(g_logPath);
    if (g_logPathAlt[0] && _stricmp(g_logPathAlt, g_logPath) != 0) {
        appendFile(g_logPathAlt);
    }
}

void InitPaths(HINSTANCE self) {
    char dllPath[MAX_PATH]{};
    GetModuleFileNameA(self, dllPath, static_cast<DWORD>(_countof(dllPath)));
    char* slash = strrchr(dllPath, '\\');
    if (!slash) return;
    *slash = '\0';
    char subDir[MAX_PATH]{};
    _snprintf_s(subDir, _countof(subDir), _TRUNCATE, "%s\\FullBright", dllPath);
    DWORD subDirAttrs = GetFileAttributesA(subDir);
    bool hasSubDir = subDirAttrs != INVALID_FILE_ATTRIBUTES && (subDirAttrs & FILE_ATTRIBUTE_DIRECTORY);

    if (hasSubDir) {
        _snprintf_s(g_jsonPath, _countof(g_jsonPath), _TRUNCATE, "%s\\FullBright.json", subDir);
        _snprintf_s(g_logPath, _countof(g_logPath), _TRUNCATE, "%s\\FullBright.log", subDir);
        _snprintf_s(g_jsonPathAlt, _countof(g_jsonPathAlt), _TRUNCATE, "%s\\FullBright.json", dllPath);
        _snprintf_s(g_logPathAlt, _countof(g_logPathAlt), _TRUNCATE, "%s\\FullBright.log", dllPath);
    } else {
        _snprintf_s(g_jsonPath, _countof(g_jsonPath), _TRUNCATE, "%s\\FullBright.json", dllPath);
        _snprintf_s(g_logPath, _countof(g_logPath), _TRUNCATE, "%s\\FullBright.log", dllPath);
        g_jsonPathAlt[0] = '\0';
        g_logPathAlt[0] = '\0';
    }
}

bool FileExistsA_(char const* path) {
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool ReadTextFileSmall(char const* path, std::string& out) {
    out.clear();
    FILE* fp = nullptr;
    if (fopen_s(&fp, path, "rb") != 0 || !fp) return false;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0) {
        fclose(fp);
        return false;
    }
    out.resize(static_cast<size_t>(size));
    size_t n = out.empty() ? 0 : fread(&out[0], 1, out.size(), fp);
    fclose(fp);
    out.resize(n);
    return true;
}

bool WriteTextFile(char const* path, char const* text) {
    FILE* fp = nullptr;
    if (fopen_s(&fp, path, "wb") != 0 || !fp) return false;
    size_t len = strlen(text);
    size_t n = fwrite(text, 1, len, fp);
    fclose(fp);
    return n == len;
}

void SaveConfigJson() {
    char buf[1024]{};
    char hookTargetRvaBuf[32] = "0x0";
    _snprintf_s(hookTargetRvaBuf, _countof(hookTargetRvaBuf), _TRUNCATE, "0x%llx", g_hookTargetRva.load(std::memory_order_relaxed));
    char hookPatternOffsetBuf[32] = "0";
    _snprintf_s(hookPatternOffsetBuf, _countof(hookPatternOffsetBuf), _TRUNCATE, "%d", g_hookPatternOffset.load(std::memory_order_relaxed));
    char candidateTrialIndexBuf[32] = "0";
    _snprintf_s(candidateTrialIndexBuf, _countof(candidateTrialIndexBuf), _TRUNCATE, "%d", g_candidateTrialIndex.load(std::memory_order_relaxed));
    char callerFilterRvaBuf[32] = "0x0";
    _snprintf_s(callerFilterRvaBuf, _countof(callerFilterRvaBuf), _TRUNCATE, "0x%llx", g_callerFilterRva.load(std::memory_order_relaxed));
    char phaseGateBuf[8] = "true";
    _snprintf_s(
        phaseGateBuf,
        _countof(phaseGateBuf),
        _TRUNCATE,
        "%s",
        g_phaseGateEnabled.load(std::memory_order_relaxed) ? "true" : "false"
    );
    char phaseGateFallbackBuf[8] = "true";
    _snprintf_s(
        phaseGateFallbackBuf,
        _countof(phaseGateFallbackBuf),
        _TRUNCATE,
        "%s",
        g_phaseGateFallbackToCaller.load(std::memory_order_relaxed) ? "true" : "false"
    );

    _snprintf_s(
        buf,
        _countof(buf),
        _TRUNCATE,
        "{\n"
        "  \"enabled\": %s,\n"
        "  \"hookMode\": \"getGamma\",\n"
        "  \"hookTargetRva\": \"%s\",\n"
        "  \"hookPattern\": \"%s\",\n"
        "  \"hookPatternOffset\": \"%s\",\n"
        "  \"candidateTrialIndex\": \"%s\",\n"
        "  \"callerFilterRva\": \"%s\",\n"
        "  \"phaseGateEnabled\": %s,\n"
        "  \"phaseGateFallbackToCaller\": %s,\n"
        "  \"toggleHotkey\": \"%s\",\n"
        "  \"phaseScreenPattern\": \"%s\",\n"
        "  \"phaseLevelPattern\": \"%s\",\n"
        "  \"patternScanEnabled\": %s,\n"
        "  \"forceBrightness\": %.3f\n"
        "}\n",
        g_enabled.load(std::memory_order_relaxed) ? "true" : "false",
        hookTargetRvaBuf,
        g_hookPattern,
        hookPatternOffsetBuf,
        candidateTrialIndexBuf,
        callerFilterRvaBuf,
        phaseGateBuf,
        phaseGateFallbackBuf,
        g_toggleHotkey,
        g_phaseScreenPattern,
        g_phaseLevelPattern,
        g_patternScanEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        g_forceBrightness.load(std::memory_order_relaxed)
    );
    (void)WriteTextFile(g_jsonPath, buf);
    if (g_jsonPathAlt[0] && _stricmp(g_jsonPathAlt, g_jsonPath) != 0) {
        (void)WriteTextFile(g_jsonPathAlt, buf);
    }
}

void LoadConfigJson() {
    if (!FileExistsA_(g_jsonPath) && g_jsonPathAlt[0] && FileExistsA_(g_jsonPathAlt)) {
        strncpy_s(g_jsonPath, _countof(g_jsonPath), g_jsonPathAlt, _TRUNCATE);
    }

    if (!FileExistsA_(g_jsonPath)) {
        strcpy_s(g_hookPattern, _countof(g_hookPattern), kDefaultGetGammaPatternShort);
        strcpy_s(g_phaseScreenPattern, _countof(g_phaseScreenPattern), kDefaultScreenRenderPattern);
        strcpy_s(g_phaseLevelPattern, _countof(g_phaseLevelPattern), kDefaultLevelRenderPattern);
        SaveConfigJson();
        return;
    }

    std::string txt;
    if (!ReadTextFileSmall(g_jsonPath, txt)) return;

    auto extract = [&](char const* key, char* out, size_t outCap) {
        out[0] = '\0';
        std::string pat = std::string("\"") + key + "\"";
        size_t p = txt.find(pat);
        if (p == std::string::npos) return;
        p = txt.find(':', p);
        if (p == std::string::npos) return;
        ++p;
        while (p < txt.size() && std::isspace(static_cast<unsigned char>(txt[p]))) ++p;
        if (p >= txt.size()) return;
        if (txt[p] == '"') {
            ++p;
            size_t e = txt.find('"', p);
            if (e == std::string::npos) return;
            size_t n = (e > p) ? (e - p) : 0;
            if (n >= outCap) n = outCap - 1;
            memcpy(out, txt.data() + p, n);
            out[n] = '\0';
            return;
        }
        size_t e = p;
        while (e < txt.size() && txt[e] != ',' && txt[e] != '}' && txt[e] != '\r' && txt[e] != '\n') ++e;
        while (e > p && std::isspace(static_cast<unsigned char>(txt[e - 1]))) --e;
        size_t n = (e > p) ? (e - p) : 0;
        if (n >= outCap) n = outCap - 1;
        memcpy(out, txt.data() + p, n);
        out[n] = '\0';
    };

    char enabledBuf[32] = "true";
    char hookModeBuf[64] = "getGamma";
    char hookTargetBuf[64] = "0x0";
    char hookPatternBuf[256] = "";
    char hookPatternOffsetBuf[32] = "0";
    char candidateTrialIndexBuf[32] = "0";
    char callerFilterRvaBuf[64] = "0x0";
    char phaseGateBuf[32] = "true";
    char phaseGateFallbackBuf[32] = "true";
    char toggleHotkeyBuf[64] = "F8";
    char phaseScreenPatternBuf[256] = "";
    char phaseLevelPatternBuf[256] = "";
    char patternBuf[32] = "true";
    char brightnessBuf[64] = "10.0";

    extract("enabled", enabledBuf, _countof(enabledBuf));
    extract("hookMode", hookModeBuf, _countof(hookModeBuf));
    extract("hookTargetRva", hookTargetBuf, _countof(hookTargetBuf));
    extract("hookPattern", hookPatternBuf, _countof(hookPatternBuf));
    extract("hookPatternOffset", hookPatternOffsetBuf, _countof(hookPatternOffsetBuf));
    extract("candidateTrialIndex", candidateTrialIndexBuf, _countof(candidateTrialIndexBuf));
    extract("callerFilterRva", callerFilterRvaBuf, _countof(callerFilterRvaBuf));
    extract("phaseGateEnabled", phaseGateBuf, _countof(phaseGateBuf));
    extract("phaseGateFallbackToCaller", phaseGateFallbackBuf, _countof(phaseGateFallbackBuf));
    extract("toggleHotkey", toggleHotkeyBuf, _countof(toggleHotkeyBuf));
    extract("phaseScreenPattern", phaseScreenPatternBuf, _countof(phaseScreenPatternBuf));
    extract("phaseLevelPattern", phaseLevelPatternBuf, _countof(phaseLevelPatternBuf));
    extract("patternScanEnabled", patternBuf, _countof(patternBuf));
    extract("forceBrightness", brightnessBuf, _countof(brightnessBuf));

    g_enabled.store(strcmp(enabledBuf, "false") != 0 && strcmp(enabledBuf, "0") != 0, std::memory_order_relaxed);
    g_patternScanEnabled.store(strcmp(patternBuf, "false") != 0 && strcmp(patternBuf, "0") != 0, std::memory_order_relaxed);
    g_hookTargetRva.store(strtoull(hookTargetBuf, nullptr, 0), std::memory_order_relaxed);
    g_hookPatternOffset.store(atoi(hookPatternOffsetBuf), std::memory_order_relaxed);
    g_candidateTrialIndex.store(atoi(candidateTrialIndexBuf), std::memory_order_relaxed);
    g_callerFilterRva.store(strtoull(callerFilterRvaBuf, nullptr, 0), std::memory_order_relaxed);
    g_phaseGateEnabled.store(strcmp(phaseGateBuf, "false") != 0 && strcmp(phaseGateBuf, "0") != 0, std::memory_order_relaxed);
    g_phaseGateFallbackToCaller.store(
        strcmp(phaseGateFallbackBuf, "false") != 0 && strcmp(phaseGateFallbackBuf, "0") != 0,
        std::memory_order_relaxed
    );
    strncpy_s(g_toggleHotkey, _countof(g_toggleHotkey), toggleHotkeyBuf, _TRUNCATE);
    g_forceBrightness.store(atof(brightnessBuf), std::memory_order_relaxed);

    if (phaseScreenPatternBuf[0]) {
        strncpy_s(g_phaseScreenPattern, _countof(g_phaseScreenPattern), phaseScreenPatternBuf, _TRUNCATE);
    } else {
        strncpy_s(g_phaseScreenPattern, _countof(g_phaseScreenPattern), kDefaultScreenRenderPattern, _TRUNCATE);
    }
    if (phaseLevelPatternBuf[0]) {
        strncpy_s(g_phaseLevelPattern, _countof(g_phaseLevelPattern), phaseLevelPatternBuf, _TRUNCATE);
    } else {
        strncpy_s(g_phaseLevelPattern, _countof(g_phaseLevelPattern), kDefaultLevelRenderPattern, _TRUNCATE);
    }

    if (_stricmp(hookModeBuf, "getGamma") != 0) {
        DebugLog("[FullBright] only getGamma mode is supported; forcing hookMode=getGamma\n");
    }

    if (hookPatternBuf[0]) {
        strncpy_s(g_hookPattern, _countof(g_hookPattern), hookPatternBuf, _TRUNCATE);
    } else {
        strncpy_s(g_hookPattern, _countof(g_hookPattern), kDefaultGetGammaPatternShort, _TRUNCATE);
    }

    if (g_forceBrightness.load(std::memory_order_relaxed) < 1.0) {
        g_forceBrightness.store(1.0, std::memory_order_relaxed);
    }
    if (g_forceBrightness.load(std::memory_order_relaxed) > 25.0) {
        g_forceBrightness.store(25.0, std::memory_order_relaxed);
    }

    ParseAndApplyToggleHotkey();

    DebugLog(
        "[FullBright] config: enabled=%d hookMode=getGamma hookTargetRva=0x%llx patternScan=%d phaseGate=%d toggleHotkey=%s forceBrightness=%.3f\n",
        g_enabled.load(std::memory_order_relaxed) ? 1 : 0,
        g_hookTargetRva.load(std::memory_order_relaxed),
        g_patternScanEnabled.load(std::memory_order_relaxed) ? 1 : 0,
        g_phaseGateEnabled.load(std::memory_order_relaxed) ? 1 : 0,
        g_toggleHotkey,
        g_forceBrightness.load(std::memory_order_relaxed)
    );
}

std::string ToUpperAscii(char const* s) {
    std::string out;
    if (!s) return out;
    out.assign(s);
    for (char& ch : out) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return out;
}

std::string TrimAscii(std::string s) {
    size_t l = 0;
    while (l < s.size() && std::isspace(static_cast<unsigned char>(s[l]))) ++l;
    size_t r = s.size();
    while (r > l && std::isspace(static_cast<unsigned char>(s[r - 1]))) --r;
    return s.substr(l, r - l);
}

bool ParseToggleHotkeyToken(std::string const& token, int& outVk) {
    if (token.empty()) return false;
    if (token.size() == 1) {
        char c = token[0];
        if (c >= 'A' && c <= 'Z') {
            outVk = static_cast<int>(c);
            return true;
        }
        if (c >= '0' && c <= '9') {
            outVk = static_cast<int>(c);
            return true;
        }
    }

    if (token == "SPACE") {
        outVk = VK_SPACE;
        return true;
    }
    if (token == "TAB") {
        outVk = VK_TAB;
        return true;
    }
    if (token == "ENTER" || token == "RETURN") {
        outVk = VK_RETURN;
        return true;
    }
    if (token == "ESC" || token == "ESCAPE") {
        outVk = VK_ESCAPE;
        return true;
    }
    if (token == "BACKSPACE") {
        outVk = VK_BACK;
        return true;
    }
    if (token == "INSERT") {
        outVk = VK_INSERT;
        return true;
    }
    if (token == "DELETE") {
        outVk = VK_DELETE;
        return true;
    }
    if (token == "HOME") {
        outVk = VK_HOME;
        return true;
    }
    if (token == "END") {
        outVk = VK_END;
        return true;
    }
    if (token == "PGUP" || token == "PAGEUP") {
        outVk = VK_PRIOR;
        return true;
    }
    if (token == "PGDN" || token == "PAGEDOWN") {
        outVk = VK_NEXT;
        return true;
    }
    if (token == "UP") {
        outVk = VK_UP;
        return true;
    }
    if (token == "DOWN") {
        outVk = VK_DOWN;
        return true;
    }
    if (token == "LEFT") {
        outVk = VK_LEFT;
        return true;
    }
    if (token == "RIGHT") {
        outVk = VK_RIGHT;
        return true;
    }

    if (token.size() >= 2 && token[0] == 'F') {
        int n = atoi(token.c_str() + 1);
        if (n >= 1 && n <= 24) {
            outVk = VK_F1 + (n - 1);
            return true;
        }
    }

    if (token.size() == 4 && token[0] == 'N' && token[1] == 'U' && token[2] == 'M' && token[3] >= '0' && token[3] <= '9') {
        outVk = VK_NUMPAD0 + (token[3] - '0');
        return true;
    }
    if (token.size() == 7 && token.rfind("NUMPAD", 0) == 0) {
        char d = token[6];
        if (d >= '0' && d <= '9') {
            outVk = VK_NUMPAD0 + (d - '0');
            return true;
        }
    }

    return false;
}

void ParseAndApplyToggleHotkey() {
    std::string src = ToUpperAscii(g_toggleHotkey);
    src = TrimAscii(src);
    if (src.empty()) {
        src = "F8";
        strncpy_s(g_toggleHotkey, _countof(g_toggleHotkey), src.c_str(), _TRUNCATE);
    }

    bool needCtrl = false;
    bool needAlt = false;
    bool needShift = false;
    int keyVk = 0;

    size_t start = 0;
    while (start < src.size()) {
        size_t plus = src.find('+', start);
        std::string part = src.substr(start, plus == std::string::npos ? std::string::npos : plus - start);
        part = TrimAscii(part);
        if (!part.empty()) {
            if (part == "CTRL" || part == "CONTROL") {
                needCtrl = true;
            } else if (part == "ALT") {
                needAlt = true;
            } else if (part == "SHIFT") {
                needShift = true;
            } else {
                int parsedVk = 0;
                if (!ParseToggleHotkeyToken(part, parsedVk)) {
                    g_toggleHotkeyValid.store(false, std::memory_order_relaxed);
                    g_toggleVirtualKey.store(VK_F8, std::memory_order_relaxed);
                    g_toggleNeedCtrl.store(false, std::memory_order_relaxed);
                    g_toggleNeedAlt.store(false, std::memory_order_relaxed);
                    g_toggleNeedShift.store(false, std::memory_order_relaxed);
                    strncpy_s(g_toggleHotkey, _countof(g_toggleHotkey), "F8", _TRUNCATE);
                    DebugLog("[FullBright] invalid toggleHotkey token '%s'; fallback to F8\n", part.c_str());
                    return;
                }
                keyVk = parsedVk;
            }
        }
        if (plus == std::string::npos) break;
        start = plus + 1;
    }

    if (keyVk == 0) {
        keyVk = VK_F8;
        strncpy_s(g_toggleHotkey, _countof(g_toggleHotkey), "F8", _TRUNCATE);
    }

    g_toggleVirtualKey.store(keyVk, std::memory_order_relaxed);
    g_toggleNeedCtrl.store(needCtrl, std::memory_order_relaxed);
    g_toggleNeedAlt.store(needAlt, std::memory_order_relaxed);
    g_toggleNeedShift.store(needShift, std::memory_order_relaxed);
    g_toggleHotkeyValid.store(true, std::memory_order_relaxed);
}

bool IsVkDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool IsToggleHotkeyDown() {
    if (!g_toggleHotkeyValid.load(std::memory_order_relaxed)) return false;

    int vk = g_toggleVirtualKey.load(std::memory_order_relaxed);
    if (vk <= 0 || !IsVkDown(vk)) return false;

    if (g_toggleNeedCtrl.load(std::memory_order_relaxed) && !(IsVkDown(VK_CONTROL) || IsVkDown(VK_LCONTROL) || IsVkDown(VK_RCONTROL))) {
        return false;
    }
    if (g_toggleNeedAlt.load(std::memory_order_relaxed) && !(IsVkDown(VK_MENU) || IsVkDown(VK_LMENU) || IsVkDown(VK_RMENU))) {
        return false;
    }
    if (g_toggleNeedShift.load(std::memory_order_relaxed) && !(IsVkDown(VK_SHIFT) || IsVkDown(VK_LSHIFT) || IsVkDown(VK_RSHIFT))) {
        return false;
    }

    return true;
}

void ProcessHotkeyToggleEdge() {
    bool down = IsToggleHotkeyDown();
    bool wasDown = g_toggleHotkeyWasDown.exchange(down, std::memory_order_relaxed);
    if (down && !wasDown) {
        bool nowEnabled = !g_runtimeEnabled.load(std::memory_order_relaxed);
        ApplyRuntimeEnabled(nowEnabled);
        SaveConfigJson();
        DebugLog(
            "[FullBright] toggle hotkey (%s) -> enabled=%d\n",
            g_toggleHotkey,
            nowEnabled ? 1 : 0
        );
    }
}

void ApplyRuntimeEnabled(bool enabled) {
    g_enabled.store(enabled, std::memory_order_relaxed);
    g_runtimeEnabled.store(enabled, std::memory_order_relaxed);

    if (enabled) {
        bool phaseReady = InstallPhaseHooks();
        if (g_phaseGateEnabled.load(std::memory_order_relaxed) && !phaseReady) {
            g_phaseGateEnabled.store(false, std::memory_order_relaxed);
        }
        if (!g_hookActive.load(std::memory_order_relaxed)) {
            (void)InstallGetGammaHook();
        }
        g_watchdogStop.store(false, std::memory_order_relaxed);
        if (!g_watchdogThread) {
            g_watchdogThread = CreateThread(nullptr, 0, CandidateWatchdogThread, nullptr, 0, nullptr);
        }
    } else {
        UninstallPhaseHooks();
        UninstallGetGammaHook();
    }
}

DWORD WINAPI HotkeyPollThread(LPVOID) {
    while (!g_hotkeyThreadStop.load(std::memory_order_relaxed)) {
        if (g_initialized.load(std::memory_order_relaxed)) {
            ProcessHotkeyToggleEdge();
        }
        Sleep(25);
    }
    return 0;
}

void* ResolveByRva(unsigned long long rva) {
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!base || rva == 0) return nullptr;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
    if (rva >= nt->OptionalHeader.SizeOfImage) return nullptr;
    return base + rva;
}

unsigned long long CanonicalizeFunctionBeginRva(unsigned long long rva) {
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!base || rva == 0) return rva;
    DWORD64 imageBase = reinterpret_cast<DWORD64>(base);
    DWORD64 addr = imageBase + rva;
    PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(addr, &imageBase, nullptr);
    if (!rf || rf->BeginAddress == 0) return rva;
    return static_cast<unsigned long long>(rf->BeginAddress);
}

bool ParsePattern(char const* pattern, std::vector<int>& bytes) {
    bytes.clear();
    if (!pattern || !pattern[0]) return false;
    char const* p = pattern;
    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        if (p[0] == '?' && p[1] == '?') {
            bytes.push_back(-1);
            p += 2;
            continue;
        }
        if (p[0] == '?') {
            bytes.push_back(-1);
            ++p;
            continue;
        }
        unsigned int v = 0;
        if (sscanf_s(p, "%2x", &v) != 1) return false;
        bytes.push_back(static_cast<int>(v & 0xFF));
        if (std::isxdigit(static_cast<unsigned char>(p[0])) && std::isxdigit(static_cast<unsigned char>(p[1]))) {
            p += 2;
        } else {
            ++p;
        }
    }
    return !bytes.empty();
}

bool ScanTextForPattern(char const* pattern, int offset, unsigned long long& outRva) {
    outRva = 0;
    std::vector<int> pat;
    if (!ParsePattern(pattern, pat)) return false;

    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!base) return false;

    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    auto* sec = IMAGE_FIRST_SECTION(nt);
    IMAGE_SECTION_HEADER* text = nullptr;
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (memcmp(sec[i].Name, ".text", 5) == 0) {
            text = &sec[i];
            break;
        }
    }
    if (!text) return false;

    auto* start = base + text->VirtualAddress;
    size_t size = text->Misc.VirtualSize;
    if (size < pat.size()) return false;

    for (size_t i = 0; i + pat.size() <= size; ++i) {
        bool ok = true;
        for (size_t j = 0; j < pat.size(); ++j) {
            int b = pat[j];
            if (b >= 0 && start[i + j] != static_cast<unsigned char>(b)) {
                ok = false;
                break;
            }
        }
        if (!ok) continue;
        long long posSigned = static_cast<long long>(i) + static_cast<long long>(offset);
        if (posSigned < 0) continue;
        size_t pos = static_cast<size_t>(posSigned);
        if (pos >= size) continue;
        outRva = static_cast<unsigned long long>(text->VirtualAddress + pos);
        return true;
    }
    return false;
}

bool MatchPatternAtRva(char const* pattern, unsigned long long rva) {
    if (!pattern || !pattern[0] || rva == 0) return false;
    std::vector<int> pat;
    if (!ParsePattern(pattern, pat)) return false;
    void* target = ResolveByRva(rva);
    if (!target) return false;
    auto* p = static_cast<unsigned char*>(target);
    for (size_t i = 0; i < pat.size(); ++i) {
        int b = pat[i];
        if (b >= 0 && p[i] != static_cast<unsigned char>(b)) {
            return false;
        }
    }
    return true;
}

bool ResolveGetGammaByFallbackPatterns(unsigned long long& outRva, char const*& outSource) {
    outRva = 0;
    outSource = "";
    struct PatternItem {
        char const* pattern;
        char const* name;
        int offset;
    };
    PatternItem const items[] = {
        {kDefaultGetGammaPatternShort, "default-short", 0},
        {kFallbackGetGammaPatternLatite, "latite-family", 0},
        {kFallbackGetGammaPatternAltA, "alt-a", 0},
        {kFallbackGetGammaPatternAltB, "alt-b", 0},
        {kDefaultGetGammaPatternLong, "default-long", 0},
    };

    for (auto const& it : items) {
        if (ScanTextForPattern(it.pattern, it.offset, outRva)) {
            outSource = it.name;
            return true;
        }
    }
    return false;
}

bool ScanTextForPatternAll(char const* pattern, int offset, std::vector<unsigned long long>& outRvas, size_t maxMatches) {
    outRvas.clear();
    std::vector<int> pat;
    if (!ParsePattern(pattern, pat)) return false;

    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!base) return false;

    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    auto* sec = IMAGE_FIRST_SECTION(nt);
    IMAGE_SECTION_HEADER* text = nullptr;
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (memcmp(sec[i].Name, ".text", 5) == 0) {
            text = &sec[i];
            break;
        }
    }
    if (!text) return false;

    auto* start = base + text->VirtualAddress;
    size_t size = text->Misc.VirtualSize;
    if (size < pat.size()) return false;

    for (size_t i = 0; i + pat.size() <= size; ++i) {
        bool ok = true;
        for (size_t j = 0; j < pat.size(); ++j) {
            int b = pat[j];
            if (b >= 0 && start[i + j] != static_cast<unsigned char>(b)) {
                ok = false;
                break;
            }
        }
        if (!ok) continue;
        long long posSigned = static_cast<long long>(i) + static_cast<long long>(offset);
        if (posSigned < 0) continue;
        size_t pos = static_cast<size_t>(posSigned);
        if (pos >= size) continue;
        outRvas.push_back(static_cast<unsigned long long>(text->VirtualAddress + pos));
        if (outRvas.size() >= maxMatches) break;
    }
    return !outRvas.empty();
}

bool FindGetGammaTargetByLiteralGetterScan(unsigned long long& outRva) {
    outRva = 0;

    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!base) return false;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    auto* sec = IMAGE_FIRST_SECTION(nt);
    IMAGE_SECTION_HEADER* text = nullptr;
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (memcmp(sec[i].Name, ".text", 5) == 0) {
            text = &sec[i];
            break;
        }
    }
    if (!text) return false;

    auto* start = base + text->VirtualAddress;
    size_t size = text->Misc.VirtualSize;
    if (size < 10) return false;

    struct GetterCandidate {
        unsigned long long rva = 0;
        unsigned int score = 0;
        unsigned int disp = 0;
        unsigned int form = 0; // 1=81, 2=80, 3=41, 4=42
    };
    std::vector<GetterCandidate> candidates;
    candidates.reserve(512);

    auto addCandidate = [&](size_t i, unsigned int form, unsigned int disp, size_t len) {
        GetterCandidate c{};
        c.rva = static_cast<unsigned long long>(text->VirtualAddress + i);
        c.disp = disp;
        c.form = form;

        unsigned int score = 0;
        if (disp >= 0x80 && disp <= 0x400) score += 80;
        else if (disp <= 0x800) score += 40;
        if (form == 1 || form == 2) score += 30;
        else score += 15;
        if (i + len + 1 < size && start[i + len] == 0xCC && start[i + len + 1] == 0xCC) score += 20;
        c.score = score;
        candidates.push_back(c);
    };

    for (size_t i = 0; i + 10 <= size; ++i) {
        if (start[i] != 0xF3 || start[i + 1] != 0x0F || start[i + 2] != 0x10) continue;

        if (start[i + 3] == 0x81 && i + 9 <= size && start[i + 8] == 0xC3) {
            unsigned int disp = *reinterpret_cast<unsigned int*>(start + i + 4);
            addCandidate(i, 1, disp, 9);
            continue;
        }
        if (start[i + 3] == 0x80 && i + 9 <= size && start[i + 8] == 0xC3) {
            unsigned int disp = *reinterpret_cast<unsigned int*>(start + i + 4);
            addCandidate(i, 2, disp, 9);
            continue;
        }
        if (start[i + 3] == 0x41 && i + 6 <= size && start[i + 5] == 0xC3) {
            unsigned int disp = static_cast<unsigned int>(start[i + 4]);
            addCandidate(i, 3, disp, 6);
            continue;
        }
        if (start[i + 3] == 0x42 && i + 6 <= size && start[i + 5] == 0xC3) {
            unsigned int disp = static_cast<unsigned int>(start[i + 4]);
            addCandidate(i, 4, disp, 6);
            continue;
        }
    }

    if (candidates.empty()) {
        DebugLog("[FullBright] getGamma literal-getter scan found no candidates\n");
        return false;
    }

    std::sort(candidates.begin(), candidates.end(), [](GetterCandidate const& a, GetterCandidate const& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.form != b.form) return a.form < b.form;
        if (a.disp != b.disp) return a.disp < b.disp;
        return a.rva < b.rva;
    });

    DebugLog("[FullBright] getGamma literal-getter candidates=%zu\n", candidates.size());
    size_t topN = candidates.size() < 12 ? candidates.size() : 12;
    for (size_t i = 0; i < topN; ++i) {
        DebugLog("[FullBright] literal-candidate[%zu] rva=0x%llx score=%u disp=0x%x form=%u\n",
                 i,
                 candidates[i].rva,
                 candidates[i].score,
                 candidates[i].disp,
                 candidates[i].form);
    }

    size_t probeCount = candidates.size() < 24 ? candidates.size() : 24;
    unsigned long long rejectedRva = g_rejectedCandidateRva.load(std::memory_order_relaxed);
    unsigned long long noHitRva = g_lastNoHitRva.load(std::memory_order_relaxed);
    int trial = g_candidateTrialIndex.load(std::memory_order_relaxed);
    if (trial < 0) trial = 0;
    size_t chosen = probeCount == 0 ? 0 : (static_cast<size_t>(trial) % probeCount);
    if (probeCount > 0) {
        size_t attempts = 0;
        while (attempts < probeCount) {
            unsigned long long rva = candidates[chosen].rva;
            bool rejected = (rejectedRva != 0 && rva == rejectedRva) || (noHitRva != 0 && rva == noHitRva);
            if (!rejected) break;
            ++attempts;
            chosen = (chosen + 1) % probeCount;
        }
        if (attempts >= probeCount) {
            DebugLog(
                "[FullBright] literal-getter skip list exhausted (rejected=0x%llx noHit=0x%llx), fallback to chosen index=%zu\n",
                rejectedRva,
                noHitRva,
                chosen
            );
        }
    }
    outRva = candidates[chosen].rva;
    DebugLog("[FullBright] getGamma literal-getter choose index=%zu trial=%d probeCount=%zu rva=0x%llx\n",
             chosen,
             trial,
             probeCount,
             outRva);
    g_candidateTrialIndex.store(trial + 1, std::memory_order_relaxed);
    return true;
}

bool FindGetGammaTargetByScan(unsigned long long& outRva) {
    outRva = 0;
    std::vector<unsigned long long> rawMatches;
    std::vector<unsigned long long> tmp;
    char const* getterPatterns[] = {
        "F3 0F 10 81 ? ? ? ? C3",
        "F3 0F 10 80 ? ? ? ? C3",
        "F3 0F 10 41 ? C3",
        "F3 0F 10 42 ? C3"
    };
    for (auto* pat : getterPatterns) {
        if (ScanTextForPatternAll(pat, 0, tmp, 256)) {
            rawMatches.insert(rawMatches.end(), tmp.begin(), tmp.end());
        }
    }
    if (rawMatches.empty()) return false;

    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!base) return false;
    DWORD64 imageBase = reinterpret_cast<DWORD64>(base);

    struct CandidateScore {
        unsigned int score = 0;
        unsigned int hits = 0;
        unsigned int minSize = 0;
    };
    std::unordered_map<unsigned long long, CandidateScore> scoreMap;

    for (unsigned long long rva : rawMatches) {
        DWORD64 addr = imageBase + rva;
        PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(addr, &imageBase, nullptr);
        if (!rf || rf->BeginAddress == 0 || rf->EndAddress <= rf->BeginAddress) continue;
        unsigned long long begin = static_cast<unsigned long long>(rf->BeginAddress);
        unsigned int fsize = static_cast<unsigned int>(rf->EndAddress - rf->BeginAddress);
        if (fsize < 5 || fsize > 0x100) continue;

        auto& sc = scoreMap[begin];
        sc.hits += 1;
        sc.minSize = (sc.minSize == 0) ? fsize : (std::min)(sc.minSize, fsize);
        sc.score += 20;
        if (fsize <= 0x40) sc.score += 40;
        else if (fsize <= 0x80) sc.score += 20;
        if (fsize >= 0x10 && fsize <= 0x30) sc.score += 10;
    }

    if (scoreMap.empty()) return false;

    unsigned long long best = 0;
    unsigned int bestScore = 0;
    unsigned int secondScore = 0;
    unsigned int bestHits = 0;
    unsigned int bestMinSize = 0xFFFFFFFFu;
    for (auto const& kv : scoreMap) {
        unsigned long long rva = kv.first;
        CandidateScore const& sc = kv.second;
        if (sc.score > bestScore ||
            (sc.score == bestScore && (sc.hits > bestHits ||
                                       (sc.hits == bestHits && (sc.minSize < bestMinSize ||
                                                                (sc.minSize == bestMinSize && rva < best)))))) {
            secondScore = bestScore;
            bestScore = sc.score;
            best = rva;
            bestHits = sc.hits;
            bestMinSize = sc.minSize;
        } else if (sc.score > secondScore) {
            secondScore = sc.score;
        }
    }

    DebugLog("[FullBright] getGamma getter-scan candidates=%zu best=0x%llx score=%u second=%u\n",
             scoreMap.size(), best, bestScore, secondScore);
    if (best == 0) return false;
    if (bestScore < 80 || bestScore <= secondScore + 15) {
        DebugLog("[FullBright] getGamma getter-scan rejected by confidence gate\n");
        return false;
    }
    outRva = best;
    return true;
}

bool FindGetGammaTargetByShapeScan(unsigned long long& outRva) {
    outRva = 0;

    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!base) return false;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    auto* sec = IMAGE_FIRST_SECTION(nt);
    IMAGE_SECTION_HEADER* text = nullptr;
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (memcmp(sec[i].Name, ".text", 5) == 0) {
            text = &sec[i];
            break;
        }
    }
    if (!text) return false;

    auto* start = base + text->VirtualAddress;
    size_t size = text->Misc.VirtualSize;
    if (size < 16) return false;

    DWORD64 imageBase = reinterpret_cast<DWORD64>(base);
    struct CandidateScore {
        unsigned int score = 0;
        unsigned int hits = 0;
        unsigned int minSize = 0;
    };
    std::unordered_map<unsigned long long, CandidateScore> scoreMap;

    for (size_t i = 0; i + 16 <= size; ++i) {
        // movss xmm0, [mem]
        if (start[i] != 0xF3 || start[i + 1] != 0x0F || start[i + 2] != 0x10) continue;
        unsigned char modrm = start[i + 3];
        unsigned char reg = (modrm >> 3) & 0x7;
        unsigned char mod = (modrm >> 6) & 0x3;
        unsigned char rm = modrm & 0x7;
        if (reg != 0) continue; // xmm0
        if (mod == 3) continue; // register source, not memory getter

        size_t insnLen = 4;
        if (rm == 4 && mod != 3) {
            if (i + insnLen >= size) continue;
            ++insnLen; // SIB byte
        }
        if (mod == 0 && rm == 5) {
            insnLen += 4;
        } else if (mod == 1) {
            insnLen += 1;
        } else if (mod == 2) {
            insnLen += 4;
        }
        if (i + insnLen >= size) continue;

        bool hasRetNear = false;
        for (size_t k = i + insnLen; k < i + insnLen + 20 && k < size; ++k) {
            if (start[k] == 0xC3) {
                hasRetNear = true;
                break;
            }
            if (k + 4 < size && start[k] == 0x48 && start[k + 1] == 0x83 && start[k + 2] == 0xC4 && start[k + 4] == 0xC3) {
                hasRetNear = true;
                break;
            }
        }
        if (!hasRetNear) continue;

        unsigned long long rva = static_cast<unsigned long long>(text->VirtualAddress + i);
        DWORD64 addr = imageBase + rva;
        PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(addr, &imageBase, nullptr);
        if (!rf || rf->BeginAddress == 0 || rf->EndAddress <= rf->BeginAddress) continue;

        unsigned long long begin = static_cast<unsigned long long>(rf->BeginAddress);
        unsigned int fsize = static_cast<unsigned int>(rf->EndAddress - rf->BeginAddress);
        if (fsize < 5 || fsize > 0x120) continue;

        auto& sc = scoreMap[begin];
        sc.hits += 1;
        sc.minSize = (sc.minSize == 0) ? fsize : (std::min)(sc.minSize, fsize);
        sc.score += 25;
        if (fsize <= 0x40) sc.score += 45;
        else if (fsize <= 0x80) sc.score += 25;
        if ((modrm & 0x7) == 1) sc.score += 10; // [rcx+disp] preference
    }

    if (scoreMap.empty()) {
        DebugLog("[FullBright] getGamma shape-scan found no candidates\n");
        return false;
    }

    unsigned long long best = 0;
    unsigned int bestScore = 0;
    unsigned int secondScore = 0;
    unsigned int bestHits = 0;
    unsigned int bestMinSize = 0xFFFFFFFFu;
    for (auto const& kv : scoreMap) {
        unsigned long long rva = kv.first;
        CandidateScore const& sc = kv.second;
        if (sc.score > bestScore ||
            (sc.score == bestScore && (sc.hits > bestHits ||
                                       (sc.hits == bestHits && (sc.minSize < bestMinSize ||
                                                                (sc.minSize == bestMinSize && rva < best)))))) {
            secondScore = bestScore;
            bestScore = sc.score;
            best = rva;
            bestHits = sc.hits;
            bestMinSize = sc.minSize;
        } else if (sc.score > secondScore) {
            secondScore = sc.score;
        }
    }

    std::vector<std::pair<unsigned long long, CandidateScore>> ranked;
    ranked.reserve(scoreMap.size());
    for (auto const& kv : scoreMap) ranked.push_back(kv);
    std::sort(ranked.begin(), ranked.end(), [](auto const& a, auto const& b) {
        if (a.second.score != b.second.score) return a.second.score > b.second.score;
        if (a.second.hits != b.second.hits) return a.second.hits > b.second.hits;
        if (a.second.minSize != b.second.minSize) return a.second.minSize < b.second.minSize;
        return a.first < b.first;
    });

    DebugLog("[FullBright] getGamma shape-scan candidates=%zu best=0x%llx score=%u second=%u\n",
             scoreMap.size(), best, bestScore, secondScore);
    size_t topN = ranked.size() < 8 ? ranked.size() : 8;
    for (size_t i = 0; i < topN; ++i) {
        DebugLog("[FullBright] shape-candidate[%zu] rva=0x%llx score=%u hits=%u minSize=0x%x\n",
                 i,
                 ranked[i].first,
                 ranked[i].second.score,
                 ranked[i].second.hits,
                 ranked[i].second.minSize);
    }
    if (best == 0) return false;
    if (bestScore < 180 || bestScore <= secondScore + 8) {
        DebugLog("[FullBright] getGamma shape-scan rejected by confidence gate\n");
        return false;
    }
    outRva = best;
    return true;
}

void __fastcall ScreenRenderHook(void* self, void* ctx) {
    g_uiRenderDepth.fetch_add(1, std::memory_order_relaxed);
    g_phaseScreenHitCount.fetch_add(1, std::memory_order_relaxed);
    if (g_screenRenderOrig) {
        g_screenRenderOrig(self, ctx);
    }
    g_uiRenderDepth.fetch_sub(1, std::memory_order_relaxed);
}

void __fastcall LevelRenderHook(void* self, void* screenCtx, void* renderObj) {
    g_worldRenderDepth.fetch_add(1, std::memory_order_relaxed);
    g_phaseLevelHitCount.fetch_add(1, std::memory_order_relaxed);
    if (g_levelRenderOrig) {
        g_levelRenderOrig(self, screenCtx, renderObj);
    }
    g_worldRenderDepth.fetch_sub(1, std::memory_order_relaxed);
}

bool ResolvePhaseTarget(char const* pattern, unsigned long long& outRva) {
    outRva = 0;
    if (!pattern || !pattern[0]) return false;
    return ScanTextForPattern(pattern, 0, outRva);
}

bool InstallDirectGammaReturnPatch(unsigned long long targetRva) {
    if (targetRva == 0 || g_directPatchActive.load(std::memory_order_relaxed)) return false;
    void* target = ResolveByRva(targetRva);
    if (!target) return false;

    float forced = static_cast<float>(g_forceBrightness.load(std::memory_order_relaxed));
    if (forced < 1.0f) forced = 1.0f;
    if (forced > 25.0f) forced = 25.0f;
    unsigned int bits = 0;
    memcpy(&bits, &forced, sizeof(bits));

    // mov eax, imm32; movd xmm0, eax; ret
    unsigned char patch[10] = {
        0xB8,
        static_cast<unsigned char>(bits & 0xFF),
        static_cast<unsigned char>((bits >> 8) & 0xFF),
        static_cast<unsigned char>((bits >> 16) & 0xFF),
        static_cast<unsigned char>((bits >> 24) & 0xFF),
        0x66,
        0x0F,
        0x6E,
        0xC0,
        0xC3,
    };

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    memcpy(g_directPatchBackup, target, sizeof(patch));
    memcpy(target, patch, sizeof(patch));
    DWORD dummy = 0;
    (void)VirtualProtect(target, sizeof(patch), oldProtect, &dummy);
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    g_directPatchAddr = target;
    g_directPatchLen = sizeof(patch);
    g_directPatchActive.store(true, std::memory_order_relaxed);
    DebugLog(
        "[FullBright] getGamma direct patch applied rva=0x%llx forced=%.3f\n",
        targetRva,
        forced
    );
    return true;
}

void UninstallDirectGammaReturnPatch() {
    if (!g_directPatchActive.load(std::memory_order_relaxed) || !g_directPatchAddr || g_directPatchLen == 0) return;
    DWORD oldProtect = 0;
    if (VirtualProtect(g_directPatchAddr, g_directPatchLen, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(g_directPatchAddr, g_directPatchBackup, g_directPatchLen);
        DWORD dummy = 0;
        (void)VirtualProtect(g_directPatchAddr, g_directPatchLen, oldProtect, &dummy);
        FlushInstructionCache(GetCurrentProcess(), g_directPatchAddr, g_directPatchLen);
    }
    g_directPatchAddr = nullptr;
    g_directPatchLen = 0;
    memset(g_directPatchBackup, 0, sizeof(g_directPatchBackup));
    g_directPatchActive.store(false, std::memory_order_relaxed);
    DebugLog("[FullBright] getGamma direct patch removed\n");
}

bool InstallPhaseHooks() {
#if !FULLBRIGHT_HAS_DETOURS
    return false;
#else
    if (!g_phaseGateEnabled.load(std::memory_order_relaxed)) {
        return false;
    }

    unsigned long long screenRva = 0;
    unsigned long long levelRva = 0;
    bool screenOk = ResolvePhaseTarget(g_phaseScreenPattern, screenRva);
    bool levelOk = ResolvePhaseTarget(g_phaseLevelPattern, levelRva);

    if (!screenOk || !levelOk) {
        DebugLog(
            "[FullBright] phase hooks unresolved screenOk=%d levelOk=%d (gate still uses caller filter fallback)\n",
            screenOk ? 1 : 0,
            levelOk ? 1 : 0
        );
        return false;
    }

    void* screenTarget = ResolveByRva(screenRva);
    void* levelTarget = ResolveByRva(levelRva);
    if (!screenTarget || !levelTarget) {
        DebugLog("[FullBright] phase hooks ResolveByRva failed\n");
        return false;
    }

    g_screenRenderOrig = reinterpret_cast<ScreenRenderFn>(screenTarget);
    g_levelRenderOrig = reinterpret_cast<LevelRenderFn>(levelTarget);

    LONG attachScreen = ERROR_INVALID_FUNCTION;
    LONG attachLevel = ERROR_INVALID_FUNCTION;
    if (DetourTransactionBegin() == NO_ERROR && DetourUpdateThread(GetCurrentThread()) == NO_ERROR) {
        attachScreen = DetourAttach(reinterpret_cast<PVOID*>(&g_screenRenderOrig), reinterpret_cast<PVOID>(ScreenRenderHook));
        attachLevel = DetourAttach(reinterpret_cast<PVOID*>(&g_levelRenderOrig), reinterpret_cast<PVOID>(LevelRenderHook));
        if (attachScreen == NO_ERROR && attachLevel == NO_ERROR) {
            LONG commitRes = DetourTransactionCommit();
            if (commitRes == NO_ERROR) {
                if (!g_phaseHooksLogged.exchange(true, std::memory_order_relaxed)) {
                    DebugLog("[FullBright] phase hooks attached screenRva=0x%llx levelRva=0x%llx\n", screenRva, levelRva);
                }
                return true;
            }
        }
        DetourTransactionAbort();
    }

    g_screenRenderOrig = nullptr;
    g_levelRenderOrig = nullptr;
    DebugLog("[FullBright] phase hooks attach failed screen=%ld level=%ld\n", attachScreen, attachLevel);
    return false;
#endif
}

void UninstallPhaseHooks() {
#if FULLBRIGHT_HAS_DETOURS
    bool hasScreen = g_screenRenderOrig != nullptr;
    bool hasLevel = g_levelRenderOrig != nullptr;
    if (!hasScreen && !hasLevel) return;

    if (DetourTransactionBegin() == NO_ERROR && DetourUpdateThread(GetCurrentThread()) == NO_ERROR) {
        if (hasScreen) {
            (void)DetourDetach(reinterpret_cast<PVOID*>(&g_screenRenderOrig), reinterpret_cast<PVOID>(ScreenRenderHook));
        }
        if (hasLevel) {
            (void)DetourDetach(reinterpret_cast<PVOID*>(&g_levelRenderOrig), reinterpret_cast<PVOID>(LevelRenderHook));
        }
        (void)DetourTransactionCommit();
    }
#endif
    g_screenRenderOrig = nullptr;
    g_levelRenderOrig = nullptr;
    g_uiRenderDepth.store(0, std::memory_order_relaxed);
    g_worldRenderDepth.store(0, std::memory_order_relaxed);
}

float __fastcall GetGammaHook(void* options, void* a2) {
    unsigned int hits = g_hookHitCount.fetch_add(1, std::memory_order_relaxed) + 1;
    uintptr_t retAddr = reinterpret_cast<uintptr_t>(_ReturnAddress());
    uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    unsigned long long callerRva = (retAddr > base) ? static_cast<unsigned long long>(retAddr - base) : 0ULL;
    if (hits == 1) {
        DebugLog("[FullBright] getGamma first hit options=%p a2=%p callerRva=0x%llx\n", options, a2, callerRva);
    }
    float original = 0.0f;
    if (g_getGammaOrig) {
        original = g_getGammaOrig(options, a2);
    }
    unsigned int sampleNo = g_callerSampleCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (sampleNo <= 24) {
        DebugLog(
            "[FullBright] getGamma sample #%u callerRva=0x%llx original=%.3f uiDepth=%d worldDepth=%d\n",
            sampleNo,
            callerRva,
            original,
            g_uiRenderDepth.load(std::memory_order_relaxed),
            g_worldRenderDepth.load(std::memory_order_relaxed)
        );
    }

    bool outlierOriginal = (original > 10.0f || original < 0.0f);
    if (outlierOriginal) {
        unsigned int outlierN = g_outlierOriginalSamples.fetch_add(1, std::memory_order_relaxed) + 1;
        if (outlierN <= 8) {
            DebugLog(
                "[FullBright] getGamma outlier original=%.3f callerRva=0x%llx (count=%u)\n",
                original,
                callerRva,
                outlierN
            );
        }
    }
    if (!g_enabled.load(std::memory_order_relaxed)) {
        return original;
    }

    if (g_phaseGateEnabled.load(std::memory_order_relaxed)) {
        int uiDepth = g_uiRenderDepth.load(std::memory_order_relaxed);
        int worldDepth = g_worldRenderDepth.load(std::memory_order_relaxed);
        if (sampleNo >= 16 && g_phaseGateFallbackToCaller.load(std::memory_order_relaxed) &&
            !g_phaseFallbackApplied.load(std::memory_order_relaxed) &&
            g_phaseScreenHitCount.load(std::memory_order_relaxed) == 0 &&
            g_phaseLevelHitCount.load(std::memory_order_relaxed) == 0) {
            g_phaseFallbackApplied.store(true, std::memory_order_relaxed);
            g_phaseGateEnabled.store(false, std::memory_order_relaxed);
            if (g_callerFilterRva.load(std::memory_order_relaxed) == 0 && original > 0.01f) {
                g_callerFilterRva.store(callerRva, std::memory_order_relaxed);
            }
            SaveConfigJson();
            DebugLog(
                "[FullBright] getGamma phase fallback activated: disable phase gate, callerFilterRva=0x%llx\n",
                g_callerFilterRva.load(std::memory_order_relaxed)
            );
        }
        if (uiDepth > 0 || worldDepth <= 0) {
            if (!g_filterObserveLogged.exchange(true, std::memory_order_relaxed)) {
                DebugLog("[FullBright] getGamma phase gate active: waiting for world-render calls\n");
            }
            return original;
        }
    }

    unsigned long long callerFilter = g_callerFilterRva.load(std::memory_order_relaxed);
    if (callerFilter != 0 && callerRva != callerFilter) {
        return original;
    }
    if (original <= 0.01f || outlierOriginal) {
        return original;
    }
    float forced = static_cast<float>(g_forceBrightness.load(std::memory_order_relaxed));
    if (forced < 1.0f) forced = 1.0f;
    if (forced > 25.0f) forced = 25.0f;
    if (hits == 1) {
        DebugLog("[FullBright] getGamma override forced=%.3f callerRva=0x%llx\n", forced, callerRva);
    }
    return forced;
}

bool InstallGetGammaHook() {
    bool resolvedByLiteralGetter = false;
    bool resolvedByPrimaryPattern = false;
    int trialAtStart = g_candidateTrialIndex.load(std::memory_order_relaxed);
    bool preferHeuristicScans = g_patternScanEnabled.load(std::memory_order_relaxed) && trialAtStart > 0;
    unsigned int noHitCycles = g_noHitCycleCount.load(std::memory_order_relaxed);
    bool preferStructuredScans = g_patternScanEnabled.load(std::memory_order_relaxed) && noHitCycles >= 3;
    if (g_patternScanEnabled.load(std::memory_order_relaxed)) {
        if (trialAtStart > 0 && g_hookTargetRva.load(std::memory_order_relaxed) != 0) {
            DebugLog(
                "[FullBright] getGamma trial mode active: clearing persisted hookTargetRva=0x%llx for next candidate (trial=%d)\n",
                g_hookTargetRva.load(std::memory_order_relaxed),
                trialAtStart
            );
            g_hookTargetRva.store(0, std::memory_order_relaxed);
        }
    }
    if (!g_patternScanEnabled.load(std::memory_order_relaxed) &&
        g_hookTargetRva.load(std::memory_order_relaxed) != 0 &&
        g_hookPattern[0]) {
        unsigned long long explicitRva = g_hookTargetRva.load(std::memory_order_relaxed);
        if (!MatchPatternAtRva(g_hookPattern, explicitRva)) {
            DebugLog(
                "[FullBright] explicit rva 0x%llx does not match hookPattern; attempting safe re-scan\n",
                explicitRva
            );
            unsigned long long scannedRva = 0;
            char const* src = "config-pattern";
            if (ScanTextForPattern(g_hookPattern, g_hookPatternOffset.load(std::memory_order_relaxed), scannedRva) ||
                ResolveGetGammaByFallbackPatterns(scannedRva, src)) {
                g_hookTargetRva.store(scannedRva, std::memory_order_relaxed);
                SaveConfigJson();
                DebugLog("[FullBright] safe re-scan resolved hookTargetRva=0x%llx source=%s\n", scannedRva, src);
            } else {
                DebugLog("[FullBright] safe re-scan failed; skip hook install to avoid crash\n");
                return false;
            }
        }
    }

    if (g_hookTargetRva.load(std::memory_order_relaxed) == 0 &&
        g_patternScanEnabled.load(std::memory_order_relaxed) &&
        g_hookPattern[0] &&
        !preferHeuristicScans) {
        unsigned long long scannedRva = 0;
        if (ScanTextForPattern(g_hookPattern, g_hookPatternOffset.load(std::memory_order_relaxed), scannedRva)) {
            g_hookTargetRva.store(scannedRva, std::memory_order_relaxed);
            resolvedByPrimaryPattern = true;
            SaveConfigJson();
            DebugLog("[FullBright] getGamma pattern resolved hookTargetRva=0x%llx\n", scannedRva);
        } else {
            DebugLog("[FullBright] getGamma primary pattern scan failed\n");
        }
    }

    if (preferHeuristicScans) {
        DebugLog(
            "[FullBright] getGamma trial=%d: prefer heuristic scans (skip primary/fallback signatures)\n",
            trialAtStart
        );
    }

    if (preferStructuredScans) {
        DebugLog(
            "[FullBright] getGamma no-hit cycles=%u: prefer structured scans (getter/shape before literal)\n",
            noHitCycles
        );
    }

    auto tryLiteralGetterScan = [&]() {
        if (g_hookTargetRva.load(std::memory_order_relaxed) != 0 || !g_patternScanEnabled.load(std::memory_order_relaxed)) {
            return;
        }
        unsigned long long scannedRva = 0;
        if (FindGetGammaTargetByLiteralGetterScan(scannedRva)) {
            g_hookTargetRva.store(scannedRva, std::memory_order_relaxed);
            resolvedByLiteralGetter = true;
            DebugLog("[FullBright] getGamma literal-getter resolved hookTargetRva=0x%llx\n", scannedRva);
        } else {
            DebugLog("[FullBright] getGamma literal-getter scan failed\n");
        }
    };

    auto tryGetterScan = [&]() {
        if (g_hookTargetRva.load(std::memory_order_relaxed) != 0 || !g_patternScanEnabled.load(std::memory_order_relaxed)) {
            return;
        }
        unsigned long long scannedRva = 0;
        if (FindGetGammaTargetByScan(scannedRva)) {
            g_hookTargetRva.store(scannedRva, std::memory_order_relaxed);
            DebugLog("[FullBright] getGamma getter-scan resolved hookTargetRva=0x%llx\n", scannedRva);
        } else {
            DebugLog("[FullBright] getGamma getter-scan failed\n");
        }
    };

    auto tryShapeScan = [&]() {
        if (g_hookTargetRva.load(std::memory_order_relaxed) != 0 || !g_patternScanEnabled.load(std::memory_order_relaxed)) {
            return;
        }
        unsigned long long scannedRva = 0;
        if (FindGetGammaTargetByShapeScan(scannedRva)) {
            g_hookTargetRva.store(scannedRva, std::memory_order_relaxed);
            DebugLog("[FullBright] getGamma shape-scan resolved hookTargetRva=0x%llx\n", scannedRva);
        } else {
            DebugLog("[FullBright] getGamma shape-scan failed\n");
        }
    };

    if (g_hookTargetRva.load(std::memory_order_relaxed) == 0 &&
        g_patternScanEnabled.load(std::memory_order_relaxed) &&
        !preferHeuristicScans) {
        unsigned long long scannedRva = 0;
        char const* src = "fallback";
        if (ResolveGetGammaByFallbackPatterns(scannedRva, src)) {
            g_hookTargetRva.store(scannedRva, std::memory_order_relaxed);
            SaveConfigJson();
            DebugLog("[FullBright] getGamma fallback pattern resolved hookTargetRva=0x%llx source=%s\n", scannedRva, src);
        } else {
            DebugLog("[FullBright] getGamma fallback patterns scan failed\n");
        }
    }

    if (preferStructuredScans) {
        tryGetterScan();
        tryShapeScan();
        tryLiteralGetterScan();
    } else {
        tryLiteralGetterScan();
        tryGetterScan();
        tryShapeScan();
    }

    if (g_hookTargetRva.load(std::memory_order_relaxed) == 0) {
        DebugLog("[FullBright] getGamma route selected; pattern scan found no match, set hookTargetRva manually\n");
        return false;
    }

    bool allowCanonicalize = !resolvedByLiteralGetter && !resolvedByPrimaryPattern;
    if (!g_patternScanEnabled.load(std::memory_order_relaxed) &&
        g_hookTargetRva.load(std::memory_order_relaxed) != 0) {
        allowCanonicalize = false;
    }

    if (allowCanonicalize) {
        unsigned long long before = g_hookTargetRva.load(std::memory_order_relaxed);
        unsigned long long canonical = CanonicalizeFunctionBeginRva(before);
        if (canonical != 0 && canonical != before) {
            g_hookTargetRva.store(canonical, std::memory_order_relaxed);
            SaveConfigJson();
            DebugLog("[FullBright] getGamma canonicalized targetRva 0x%llx -> 0x%llx\n", before, canonical);
        }
    } else {
        DebugLog("[FullBright] getGamma skip canonicalize for explicit target\n");
    }

    void* target = ResolveByRva(g_hookTargetRva.load(std::memory_order_relaxed));
    if (!target) {
        DebugLog("[FullBright] getGamma ResolveByRva failed rva=0x%llx\n", g_hookTargetRva.load(std::memory_order_relaxed));
        return false;
    }

#if FULLBRIGHT_HAS_DETOURS
    auto tryAttach = [&](void* targetAddr, LONG& outRes) -> bool {
        g_getGammaOrig = reinterpret_cast<GetGammaFn>(targetAddr);
        outRes = ERROR_INVALID_FUNCTION;
        if (DetourTransactionBegin() == NO_ERROR && DetourUpdateThread(GetCurrentThread()) == NO_ERROR) {
            outRes = DetourAttach(reinterpret_cast<PVOID*>(&g_getGammaOrig), reinterpret_cast<PVOID>(GetGammaHook));
            if (outRes == NO_ERROR) {
                outRes = DetourTransactionCommit();
            } else {
                DetourTransactionAbort();
            }
        }
        return outRes == NO_ERROR;
    };

    LONG attachRes = ERROR_INVALID_FUNCTION;
    unsigned long long effectiveHookRva = g_hookTargetRva.load(std::memory_order_relaxed);
    bool attached = tryAttach(target, attachRes);

    bool explicitTargetMode = !g_patternScanEnabled.load(std::memory_order_relaxed) &&
                              g_hookTargetRva.load(std::memory_order_relaxed) != 0;

    if (!attached && !explicitTargetMode) {
        unsigned long long explicitRva = g_hookTargetRva.load(std::memory_order_relaxed);
        unsigned long long canonicalRva = CanonicalizeFunctionBeginRva(explicitRva);
        if (canonicalRva != 0 && canonicalRva != explicitRva) {
            void* canonicalTarget = ResolveByRva(canonicalRva);
            if (canonicalTarget) {
                DebugLog(
                    "[FullBright] getGamma attach retry with canonical rva=0x%llx (explicit=0x%llx)\n",
                    canonicalRva,
                    explicitRva
                );
                attached = tryAttach(canonicalTarget, attachRes);
                if (attached) {
                    effectiveHookRva = canonicalRva;
                }
            }
        }
    }

    if (attached) {
        g_hookActive.store(true, std::memory_order_relaxed);
        g_hookHitCount.store(0, std::memory_order_relaxed);
        g_outlierOriginalSamples.store(0, std::memory_order_relaxed);
        g_hookAttachTick.store(GetTickCount64(), std::memory_order_relaxed);
        DebugLog("[FullBright] getGamma route attached rva=0x%llx effectiveRva=0x%llx\n",
                 g_hookTargetRva.load(std::memory_order_relaxed),
                 effectiveHookRva);
        return true;
    }

    if (explicitTargetMode) {
        if (InstallDirectGammaReturnPatch(g_hookTargetRva.load(std::memory_order_relaxed))) {
            g_hookActive.store(true, std::memory_order_relaxed);
            return true;
        }
    }

    g_getGammaOrig = nullptr;
    DebugLog(
        "[FullBright] getGamma route attach failed rva=0x%llx code=%ld\n",
        g_hookTargetRva.load(std::memory_order_relaxed),
        attachRes
    );
#else
    DebugLog("[FullBright] getGamma route selected; detours unavailable in this build\n");
#endif
    return false;
}

void UninstallGetGammaHook() {
    if (g_directPatchActive.load(std::memory_order_relaxed)) {
        UninstallDirectGammaReturnPatch();
        g_hookActive.store(false, std::memory_order_relaxed);
        return;
    }
    if (!g_hookActive.load(std::memory_order_relaxed)) return;
#if FULLBRIGHT_HAS_DETOURS
    bool detached = false;
    if (g_getGammaOrig &&
        DetourTransactionBegin() == NO_ERROR &&
        DetourUpdateThread(GetCurrentThread()) == NO_ERROR &&
        DetourDetach(reinterpret_cast<PVOID*>(&g_getGammaOrig), reinterpret_cast<PVOID>(GetGammaHook)) == NO_ERROR &&
        DetourTransactionCommit() == NO_ERROR) {
        detached = true;
    }
    if (!detached) {
        DebugLog("[FullBright] getGamma detach failed; keeping hook state intact\n");
        return;
    }
#endif
    g_getGammaOrig = nullptr;
    g_hookActive.store(false, std::memory_order_relaxed);
}

DWORD WINAPI CandidateWatchdogThread(LPVOID) {
    while (!g_watchdogStop.load(std::memory_order_relaxed)) {
        if (g_patternScanEnabled.load(std::memory_order_relaxed) && g_hookActive.load(std::memory_order_relaxed)) {
            unsigned int hitCount = g_hookHitCount.load(std::memory_order_relaxed);
            unsigned int outlierCount = g_outlierOriginalSamples.load(std::memory_order_relaxed);
            bool shouldRotate = (hitCount == 0) || (outlierCount >= 8);
            if (shouldRotate) {
                ULONGLONG attachTick = g_hookAttachTick.load(std::memory_order_relaxed);
                ULONGLONG now = GetTickCount64();
                if (attachTick != 0 && now > attachTick && now - attachTick >= 5000) {
                    unsigned int cycles = g_noHitCycleCount.load(std::memory_order_relaxed);
                    if (cycles < 8) {
                        unsigned long long currentRva = g_hookTargetRva.load(std::memory_order_relaxed);
                        g_rejectedCandidateRva.store(currentRva, std::memory_order_relaxed);
                        unsigned long long lastRva = g_lastNoHitRva.load(std::memory_order_relaxed);
                        if (currentRva != 0 && currentRva == lastRva) {
                            g_sameNoHitRvaCount.fetch_add(1, std::memory_order_relaxed);
                        } else {
                            g_lastNoHitRva.store(currentRva, std::memory_order_relaxed);
                            g_sameNoHitRvaCount.store(1, std::memory_order_relaxed);
                        }

                        if (g_sameNoHitRvaCount.load(std::memory_order_relaxed) >= 3) {
                            g_patternScanEnabled.store(false, std::memory_order_relaxed);
                            SaveConfigJson();
                            if (!g_cycleExhaustedLogged.exchange(true, std::memory_order_relaxed)) {
                                DebugLog(
                                    "[FullBright] getGamma watchdog: same unresolved rva=0x%llx repeated, disable patternScan to avoid loop\n",
                                    currentRva
                                );
                            }
                            continue;
                        }

                        g_noHitCycleCount.store(cycles + 1, std::memory_order_relaxed);
                        DebugLog(
                            "[FullBright] getGamma watchdog: rotate candidate reason=%s cycle=%u\n",
                            hitCount == 0 ? "no-hit" : "outlier-original",
                            cycles + 1
                        );
                        UninstallGetGammaHook();
                        g_hookTargetRva.store(0, std::memory_order_relaxed);
                        SaveConfigJson();
                        (void)InstallGetGammaHook();
                    } else if (!g_cycleExhaustedLogged.exchange(true, std::memory_order_relaxed)) {
                        DebugLog("[FullBright] getGamma watchdog: candidate cycles exhausted; waiting for restart\n");
                    }
                }
            }
        }
        Sleep(1000);
    }
    return 0;
}

} // namespace

__declspec(dllexport) int FullBrightInit(HINSTANCE self) {
    if (g_initialized.load(std::memory_order_relaxed)) {
        return 0;
    }

    if (!self) {
        self = g_moduleHandle;
    }
    if (!self) {
        HMODULE hm = nullptr;
        if (GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&FullBrightInit),
                &hm)) {
            self = hm;
            g_moduleHandle = hm;
        }
    }
    if (!self) {
        return 0;
    }

    InitPaths(self);
    DebugLog("[FullBright] build=getGamma-only path=%s\n", g_jsonPath);
    LoadConfigJson();
    g_noHitCycleCount.store(0, std::memory_order_relaxed);
    g_outlierOriginalSamples.store(0, std::memory_order_relaxed);
    g_lastNoHitRva.store(0, std::memory_order_relaxed);
    g_sameNoHitRvaCount.store(0, std::memory_order_relaxed);
    g_rejectedCandidateRva.store(0, std::memory_order_relaxed);
    g_cycleExhaustedLogged.store(false, std::memory_order_relaxed);
    g_callerSampleCount.store(0, std::memory_order_relaxed);
    g_filterObserveLogged.store(false, std::memory_order_relaxed);
    g_phaseHooksLogged.store(false, std::memory_order_relaxed);
    g_phaseScreenHitCount.store(0, std::memory_order_relaxed);
    g_phaseLevelHitCount.store(0, std::memory_order_relaxed);
    g_phaseFallbackApplied.store(false, std::memory_order_relaxed);
    g_toggleHotkeyWasDown.store(false, std::memory_order_relaxed);
    g_runtimeEnabled.store(g_enabled.load(std::memory_order_relaxed), std::memory_order_relaxed);
    g_initialized.store(true, std::memory_order_relaxed);

    g_hotkeyThreadStop.store(false, std::memory_order_relaxed);
    if (!g_hotkeyThread) {
        g_hotkeyThread = CreateThread(nullptr, 0, HotkeyPollThread, nullptr, 0, nullptr);
    }

    if (!g_enabled.load(std::memory_order_relaxed)) {
        DebugLog("[FullBright] disabled by config; skip hook install\n");
        return 0;
    }
    bool phaseReady = InstallPhaseHooks();
    if (g_phaseGateEnabled.load(std::memory_order_relaxed) && !phaseReady) {
        DebugLog("[FullBright] phase gate requested but phase hooks unavailable; temporarily disabling gate\n");
        g_phaseGateEnabled.store(false, std::memory_order_relaxed);
    }
    (void)InstallGetGammaHook();
    g_watchdogStop.store(false, std::memory_order_relaxed);
    if (!g_watchdogThread) {
        g_watchdogThread = CreateThread(nullptr, 0, CandidateWatchdogThread, nullptr, 0, nullptr);
    }
    return 0;
}

__declspec(dllexport) int FullBrightTick() {
    if (!g_initialized.load(std::memory_order_relaxed)) {
        return 0;
    }
    ProcessHotkeyToggleEdge();
    return 0;
}

__declspec(dllexport) int FullBrightCleanup() {
    if (!g_initialized.load(std::memory_order_relaxed)) {
        return 0;
    }
    g_enabled.store(false, std::memory_order_relaxed);
    g_runtimeEnabled.store(false, std::memory_order_relaxed);
    g_hotkeyThreadStop.store(true, std::memory_order_relaxed);
    if (g_hotkeyThread) {
        WaitForSingleObject(g_hotkeyThread, 300);
        CloseHandle(g_hotkeyThread);
        g_hotkeyThread = nullptr;
    }
    g_watchdogStop.store(true, std::memory_order_relaxed);
    if (g_watchdogThread) {
        WaitForSingleObject(g_watchdogThread, 200);
        CloseHandle(g_watchdogThread);
        g_watchdogThread = nullptr;
    }
    UninstallPhaseHooks();
    UninstallGetGammaHook();
    g_initialized.store(false, std::memory_order_relaxed);
    return 0;
}

__declspec(dllexport) int FullBrightStart() {
    return FullBrightInit(g_moduleHandle);
}

__declspec(dllexport) int FullBrightStop() {
    return FullBrightCleanup();
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        g_moduleHandle = hinstDLL;
        InitPaths(hinstDLL);
        DebugLog("[FullBright] dll attach\n");
        HANDLE initThread = CreateThread(
            nullptr,
            0,
            [](LPVOID) -> DWORD {
                Sleep(10);
                (void)FullBrightInit(g_moduleHandle);
                return 0;
            },
            nullptr,
            0,
            nullptr
        );
        if (initThread) {
            CloseHandle(initThread);
        } else {
            DebugLog("[FullBright] auto-init thread create failed\n");
        }
    }
    return TRUE;
}
