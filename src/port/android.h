#pragma once
// Android platform helpers (AYN Thor port; docs/ANDROID.md). Everything here is compiled only
// for __ANDROID__; the other platforms never include it.
#ifdef __ANDROID__
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace port::android {

// The app's internal files folder (/data/data/<package>/files). Settings, game modules, memory
// cards and imported disc images live under it. Visible in the Files app through the
// Borealis documents provider.
std::filesystem::path filesDir();

// Folder the APK's native libraries were extracted to (useLegacyPackaging): libmain.so and the
// bundled compiler (libclang_exe.so, liblld_exe.so). The only app folder Android lets us execute
// programs from.
std::filesystem::path nativeLibDir();

// Sends stdout and stderr to <dataDir>/log.txt (and keeps the previous run as log.prev.txt), so
// the log can be read on the device from the Files app. Call first thing in main.
void redirectLogs(const std::filesystem::path& dir);

// Copies an APK asset (e.g. "pipelines.bin") to dest when it is missing or a different size.
bool extractAsset(const char* asset, const std::filesystem::path& dest);

// A file the user picked comes back as a content:// URI, which nod cannot open. Copies it into
// <dataDir>/discs/<name> and returns that path. progress gets bytes copied / total (total 0
// if unknown); cancel stops it. Returns an empty path and sets error on failure.
std::filesystem::path importDisc(const std::string& uri, const std::string& name,
                                 const std::function<void(std::uint64_t, std::uint64_t)>& progress,
                                 const std::atomic_bool* cancel, std::string& error);

// Restarts the app (new process), optionally into the setup screen, then exits this process.
[[noreturn]] void relaunch(bool setup);

}  // namespace port::android
#endif
