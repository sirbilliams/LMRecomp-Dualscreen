#include "port/install.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif
#ifdef __ANDROID__
#include "port/android.h"
#endif

namespace port {
namespace {

InstallState sInstall;
std::vector<std::string> sArgs;

}  // namespace

const char* gameId(GameVersion v) { return v == GameVersion::PAL ? "GLMP01" : "GLME01"; }
const char* versionName(GameVersion v) { return v == GameVersion::PAL ? "PAL" : "USA"; }

Avail usCodeFeature() {
    const auto& i = sInstall;
    if (i.running == GameVersion::USA) return Avail::Yes;
    return i.usReady ? Avail::OtherBase : Avail::NeedsUsDisc;
}

Avail palBaseFeature() {
    const auto& i = sInstall;
    if (i.running == GameVersion::PAL) return Avail::Yes;
    return i.palReady ? Avail::OtherBase : Avail::NeedsPalDisc;
}

// The other release's Hidden Mansion needs that release's data: the overlay setup builds from
// both discs (docs/SETUP.md; PAL rules on the USA game, USA rules on the European game).
Avail hiddenMansionPal() {
    const auto& i = sInstall;
    if (i.running == GameVersion::PAL || i.hmOverlayReady) return Avail::Yes;
    return Avail::NeedsPalDisc;
}

Avail hiddenMansionUs() {
    const auto& i = sInstall;
    if (i.running == GameVersion::USA || i.hmOverlayReady) return Avail::Yes;
    return Avail::NeedsUsDisc;
}

const char* availNote(Avail a) {
    switch (a) {
    case Avail::Yes: return nullptr;
    case Avail::OtherBase:
        return sInstall.running == GameVersion::USA ? "applies when playing the European version"
                                                    : "applies when playing the USA version";
    case Avail::NeedsUsDisc: return "needs the USA disc: F1 > Game > Add or change discs";
    case Avail::NeedsPalDisc: return "needs the European disc: F1 > Game > Add or change discs";
    case Avail::NotYet: return "not available on the European version yet; play the USA version";
    }
    return nullptr;
}

InstallState& install() { return sInstall; }

namespace {

namespace fs = std::filesystem;

// The product was called "Luigi's Mansion PC" up to 1.0.1; its folders are moved to the new
// name on the first start (docs/SETUP.md "What lives where").
constexpr const char* kFolderName = "Luigi's Mansion Recompiled";
constexpr const char* kOldFolderName = "Luigi's Mansion PC";

std::string u8(const fs::path& p) {
    const auto s = p.u8string();
    return std::string(s.begin(), s.end());
}

// True when the folder holds nothing but what the Linux launcher writes before the game
// starts (LuigisMansionRecompiled.sh logs into the data folder first).
bool onlyLauncherLogs(const fs::path& dir) {
    std::error_code ec;
    for (auto it = fs::directory_iterator(dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
        const std::string n = u8(it->path().filename());
        if (n != "last_run.log" && n != "previous_run.log" && n != ".last_exit_status") return false;
    }
    return !ec;
}

// Moves <base>/<old name> to <base>/<new name> when only the old one exists: settings,
// install.ini, memory cards, game modules, overlays and caches carry over (paths inside them
// are not tied to the folder: module.key hashes the sources, install.ini holds disc paths).
// A rename that fails (a file held open by another program, another volume) falls back to a
// copy; the old folder is kept then.
void migrateOldFolder(const fs::path& base) {
    std::error_code ec;
    const fs::path oldDir = base / fs::path(std::u8string(reinterpret_cast<const char8_t*>(kOldFolderName)));
    const fs::path newDir = base / fs::path(std::u8string(reinterpret_cast<const char8_t*>(kFolderName)));
    if (!fs::is_directory(oldDir, ec)) return;
    fs::path heldLogs;  // the launcher's logs of this run, put back after the move
    if (fs::exists(newDir, ec)) {
        // A 1.0.x launcher that is still used writes its log into the old data folder again:
        // nothing else in it, so remove it (the forwarder replaces that launcher, update.cpp).
        if (onlyLauncherLogs(oldDir) && !onlyLauncherLogs(newDir)) {
            fs::remove_all(oldDir, ec);
            if (!ec) std::printf("[install] removed %s (held only launcher logs)\n", u8(oldDir).c_str());
            std::fflush(stdout);
            return;
        }
        if (!fs::is_directory(newDir, ec) || !onlyLauncherLogs(newDir)) return;
        heldLogs = base / fs::path(std::u8string(reinterpret_cast<const char8_t*>(kFolderName)) + u8".logs-tmp");
        fs::remove_all(heldLogs, ec);
        fs::rename(newDir, heldLogs, ec);
        if (ec) {
            std::printf("[install] %s holds only launcher logs, but could not be moved aside (%s); "
                        "the old folder %s is not used\n",
                        u8(newDir).c_str(), ec.message().c_str(), u8(oldDir).c_str());
            return;
        }
    }
    fs::rename(oldDir, newDir, ec);
    if (!ec) {
        std::printf("[install] moved %s to %s\n", u8(oldDir).c_str(), u8(newDir).c_str());
    } else {
        const std::string why = ec.message();
        if (!heldLogs.empty()) {
            std::error_code e2;
            fs::rename(heldLogs, newDir, e2);
            if (!e2) heldLogs.clear();
        }
        fs::create_directories(newDir, ec);
        fs::copy(oldDir, newDir, fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
        if (!ec)
            std::printf("[install] could not rename %s (%s); copied it to %s and kept the old folder\n",
                        u8(oldDir).c_str(), why.c_str(), u8(newDir).c_str());
        else
            std::printf("[install] could not rename %s (%s) and copying it to %s failed (%s); the old folder is "
                        "kept, copy what is missing by hand\n",
                        u8(oldDir).c_str(), why.c_str(), u8(newDir).c_str(), ec.message().c_str());
    }
    if (!heldLogs.empty()) {
        for (auto it = fs::directory_iterator(heldLogs, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
            std::error_code e2;
            fs::rename(it->path(), newDir / it->path().filename(), e2);  // replaces the old run's log
        }
        fs::remove_all(heldLogs, ec);
    }
    std::fflush(stdout);
}

#ifdef _WIN32
std::filesystem::path appDir() {
    const wchar_t* base = _wgetenv(L"APPDATA");
    if (!base) return std::filesystem::path(L".");
    static const bool sMigrated = (migrateOldFolder(base), true);
    (void)sMigrated;
    std::filesystem::path dir = std::filesystem::path(base) / L"Luigi's Mansion Recompiled";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}
#else
// XDG base directory: $<var> if set to an absolute path, else $HOME/<fallback>.
std::filesystem::path xdgDir(const char* var, const char* fallback) {
    std::filesystem::path base;
    if (const char* v = std::getenv(var); v && v[0] == '/') base = v;
    else if (const char* home = std::getenv("HOME"); home && home[0]) base = std::filesystem::path(home) / fallback;
    else base = ".";
    // Once per base folder (config, data and cache are separate on Linux).
    static std::mutex sM;
    static std::vector<std::filesystem::path> sDone;
    {
        std::lock_guard lk(sM);
        if (std::find(sDone.begin(), sDone.end(), base) == sDone.end()) {
            sDone.push_back(base);
            migrateOldFolder(base);
        }
    }
    std::filesystem::path dir = base / kFolderName;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}
#endif

}  // namespace

std::filesystem::path configDir() {
#ifdef _WIN32
    return appDir();
#elif defined(__ANDROID__)
    static const std::filesystem::path d = [] {
        auto p = android::filesDir() / "config";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }();
    return d;
#else
    return xdgDir("XDG_CONFIG_HOME", ".config");
#endif
}

std::filesystem::path dataDir() {
#ifdef _WIN32
    return appDir();
#elif defined(__ANDROID__)
    static const std::filesystem::path d = [] {
        auto p = android::filesDir() / "data";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }();
    return d;
#else
    return xdgDir("XDG_DATA_HOME", ".local/share");
#endif
}

std::filesystem::path cacheDir() {
#ifdef _WIN32
    return appDir();
#elif defined(__ANDROID__)
    static const std::filesystem::path d = [] {
        auto p = android::filesDir() / "cache";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }();
    return d;
#else
    return xdgDir("XDG_CACHE_HOME", ".cache");
#endif
}

std::filesystem::path hiddenMansionOverlayDir() { return dataDir() / "hidden_mansion"; }

void loadInstall() {
    std::ifstream in(configDir() / "install.ini");
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto eq = line.find('=');
        if (eq == std::string::npos || line[0] == ';' || line[0] == '[') continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "us_disc") sInstall.usDisc = value;
        else if (key == "pal_disc") sInstall.palDisc = value;
        else if (key == "base") sInstall.base = value == "PAL" ? GameVersion::PAL : GameVersion::USA;
    }
}

void saveInstall() {
    std::ofstream out(configDir() / "install.ini");
    out << "[install]\n";
    out << "us_disc=" << sInstall.usDisc << '\n';
    out << "pal_disc=" << sInstall.palDisc << '\n';
    out << "base=" << versionName(sInstall.base) << '\n';
}

void setLaunchArgs(int argc, char** argv) { sArgs.assign(argv, argv + argc); }

void relaunch(bool setup, const std::filesystem::path& exePath) {
#ifdef _WIN32
    wchar_t exe[MAX_PATH * 4];
    GetModuleFileNameW(nullptr, exe, DWORD(std::size(exe)));
    if (!exePath.empty()) wcsncpy_s(exe, exePath.wstring().c_str(), _TRUNCATE);
    // Re-use the original command line (minus the program name).
    std::wstring cmd = L"\"" + std::wstring(exe) + L"\"";
    int argc = 0;
    if (wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc)) {
        for (int i = 1; i < argc; i++) {
            const std::wstring a = argv[i];
            if (a == L"--setup" || a == L"--setup-auto") continue;
            cmd += L" \"" + a + L"\"";
        }
        LocalFree(argv);
    }
    if (setup) cmd += L" --setup";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(exe, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else if (!exePath.empty()) {
        return;  // the updater reports it
    }
#elif defined(__ANDROID__)
    (void)exePath;
    android::relaunch(setup);
#else
    std::vector<char*> argv;
    for (auto& a : sArgs) argv.push_back(a.data());
    static char setupArg[] = "--setup";
    if (setup) argv.push_back(setupArg);
    argv.push_back(nullptr);
    if (!exePath.empty()) {
        std::fflush(nullptr);
        execv(exePath.c_str(), argv.data());
        return;  // exec failed: the caller reports it
    }
    if (fork() == 0) execv("/proc/self/exe", argv.data());
#endif
    std::_Exit(0);
}

}  // namespace port
