// Android platform helpers (docs/ANDROID.md).
#ifdef __ANDROID__
#include "port/android.h"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_system.h>

#include <dlfcn.h>
#include <jni.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace port::android {

std::filesystem::path filesDir() {
    if (const char* p = SDL_GetAndroidInternalStoragePath(); p && *p) return p;
    return ".";
}

std::filesystem::path nativeLibDir() {
    Dl_info dl{};
    if (dladdr(reinterpret_cast<const void*>(&nativeLibDir), &dl) && dl.dli_fname)
        return std::filesystem::path(dl.dli_fname).parent_path();
    return {};
}

void redirectLogs(const std::filesystem::path& dir) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const auto log = dir / "log.txt";
    std::filesystem::rename(log, dir / "log.prev.txt", ec);
    if (std::freopen(log.c_str(), "w", stdout)) setvbuf(stdout, nullptr, _IOLBF, 0);
    // stderr shares the file (crash reports land after the last log lines).
    dup2(fileno(stdout), fileno(stderr));
    setvbuf(stderr, nullptr, _IONBF, 0);
}

bool extractAsset(const char* asset, const std::filesystem::path& dest) {
    SDL_IOStream* in = SDL_IOFromFile(asset, "rb");  // relative paths are APK assets on Android
    if (!in) return false;
    const Sint64 size = SDL_GetIOSize(in);
    std::error_code ec;
    if (size >= 0 && std::filesystem::exists(dest, ec) && std::filesystem::file_size(dest, ec) == std::uintmax_t(size)) {
        SDL_CloseIO(in);
        return true;
    }
    std::filesystem::create_directories(dest.parent_path(), ec);
    const auto tmp = dest.string() + ".tmp";
    FILE* out = std::fopen(tmp.c_str(), "wb");
    bool ok = out != nullptr;
    std::vector<char> buf(1 << 20);
    while (ok) {
        const size_t n = SDL_ReadIO(in, buf.data(), buf.size());
        if (n == 0) break;
        ok = std::fwrite(buf.data(), 1, n, out) == n;
    }
    SDL_CloseIO(in);
    if (out) ok = (std::fclose(out) == 0) && ok;
    if (ok) std::filesystem::rename(tmp, dest, ec);
    else std::filesystem::remove(tmp, ec);
    return ok && !ec;
}

std::filesystem::path importDisc(const std::string& uri, const std::string& name,
                                 const std::function<void(std::uint64_t, std::uint64_t)>& progress,
                                 const std::atomic_bool* cancel, std::string& error) {
    SDL_IOStream* in = SDL_IOFromFile(uri.c_str(), "rb");  // SDL opens content:// URIs on Android
    if (!in) {
        error = std::string("Could not open the chosen file: ") + SDL_GetError();
        return {};
    }
    const Sint64 size = SDL_GetIOSize(in);
    const std::uint64_t total = size > 0 ? std::uint64_t(size) : 0;
    const auto dir = filesDir() / "discs";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const auto dest = dir / name;
    const auto tmp = dir / (name + ".part");
    FILE* out = std::fopen(tmp.c_str(), "wb");
    if (!out) {
        SDL_CloseIO(in);
        error = "Could not write " + tmp.string();
        return {};
    }
    std::vector<char> buf(4 << 20);
    std::uint64_t got = 0;
    bool ok = true;
    for (;;) {
        if (cancel && cancel->load()) {
            ok = false;
            error = "Cancelled";
            break;
        }
        const size_t n = SDL_ReadIO(in, buf.data(), buf.size());
        if (n == 0) {
            if (SDL_GetIOStatus(in) == SDL_IO_STATUS_ERROR) {
                ok = false;
                error = std::string("Reading the disc image failed: ") + SDL_GetError();
            }
            break;
        }
        if (std::fwrite(buf.data(), 1, n, out) != n) {
            ok = false;
            error = "Not enough free space to copy the disc image";
            break;
        }
        got += n;
        if (progress) progress(got, total);
    }
    SDL_CloseIO(in);
    ok = (std::fclose(out) == 0) && ok;
    if (!ok) {
        std::filesystem::remove(tmp, ec);
        return {};
    }
    std::filesystem::remove(dest, ec);
    std::filesystem::rename(tmp, dest, ec);
    if (ec) {
        error = "Could not store the disc image: " + ec.message();
        return {};
    }
    return dest;
}

void relaunch(bool setup) {
    std::fflush(nullptr);
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (env && activity) {
        jclass actCls = env->GetObjectClass(activity);
        jobject pm = env->CallObjectMethod(
            activity, env->GetMethodID(actCls, "getPackageManager", "()Landroid/content/pm/PackageManager;"));
        jobject pkg = env->CallObjectMethod(activity, env->GetMethodID(actCls, "getPackageName", "()Ljava/lang/String;"));
        jclass pmCls = env->GetObjectClass(pm);
        jobject intent = env->CallObjectMethod(
            pm, env->GetMethodID(pmCls, "getLaunchIntentForPackage", "(Ljava/lang/String;)Landroid/content/Intent;"), pkg);
        if (intent) {
            jclass intentCls = env->GetObjectClass(intent);
            constexpr jint kNewTask = 0x10000000, kClearTask = 0x00008000;
            env->CallObjectMethod(intent, env->GetMethodID(intentCls, "addFlags", "(I)Landroid/content/Intent;"),
                                  kNewTask | kClearTask);
            if (setup) {
                jclass strCls = env->FindClass("java/lang/String");
                jobjectArray argv = env->NewObjectArray(1, strCls, env->NewStringUTF("--setup"));
                env->CallObjectMethod(
                    intent,
                    env->GetMethodID(intentCls, "putExtra", "(Ljava/lang/String;[Ljava/lang/String;)Landroid/content/Intent;"),
                    env->NewStringUTF("borealis_argv"), argv);
            }
            env->CallVoidMethod(activity, env->GetMethodID(actCls, "startActivity", "(Landroid/content/Intent;)V"), intent);
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }
    // The activity manager starts the pending activity in a new process once this one is gone.
    std::_Exit(0);
}

}  // namespace port::android
#endif
