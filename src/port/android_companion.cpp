// AYN Thor bottom screen: the Java companion (platforms/android, package
// io.github.sirbilliams.lmrecompiled.companion) reads game state through these functions
// (docs/ANDROID.md, "Companion screen"). The game runs natively here, so values come straight from
// guest memory; no per-frame capture is needed. Reads race with the game thread like a debugger's
// would: aligned 8/16/32-bit loads, at worst one frame stale.
#ifdef __ANDROID__
#include "port/install.h"
#include "recomp/controls.h"
#include "recomp/ctx.h"

#include <SDL3/SDL_events.h>
#include <jni.h>

#include <mutex>
#include <vector>

namespace recomp {
u32 copyDispCount();  // hle_gx.cpp: guest frame number
}

namespace {

using GuestU32 = recomp::u32;

struct Watch {
    GuestU32 address;
    int offset, size;
    bool deref;
};

std::mutex sM;
std::vector<Watch> sWatches;

bool validGuest(GuestU32 a, int size) {
    // Cached or uncached main RAM inside the host window.
    const GuestU32 seg = a >> 28;
    if (seg != 0x8 && seg != 0xC) return false;
    const GuestU32 off = a & 0x0FFFFFFF;
    return off + GuestU32(size) <= recomp::kHostMemSize;
}

bool readValue(const Watch& w, GuestU32& out) {
    if (!recomp::g_mem) return false;
    GuestU32 a = w.address;
    if (w.deref) {
        if (!validGuest(a, 4)) return false;
        const GuestU32 p = recomp::rd32(a);
        if (p == 0) return false;
        a = p + GuestU32(w.offset);
    } else {
        a += GuestU32(w.offset);
    }
    if (!validGuest(a, w.size)) return false;
    switch (w.size) {
    case 1: out = recomp::rd8(a); return true;
    case 2: out = recomp::rd16(a); return true;
    case 4: out = recomp::rd32(a); return true;
    default: return false;
    }
}

}  // namespace

extern "C" {

// "GLME01" / "GLMP01" once the game is running, else "".
JNIEXPORT jstring JNICALL Java_io_github_sirbilliams_lmrecompiled_companion_CompanionNative_gameId(JNIEnv* env, jclass) {
    const char* id = recomp::g_mem ? port::gameId(port::install().running) : "";
    return env->NewStringUTF(id);
}

// spec: groups of four ints (address, offset, size 1/2/4, deref 0/1); null clears.
JNIEXPORT void JNICALL Java_io_github_sirbilliams_lmrecompiled_companion_CompanionNative_setWatches(JNIEnv* env, jclass,
                                                                                                    jintArray spec) {
    std::vector<Watch> watches;
    if (spec) {
        const jsize n = env->GetArrayLength(spec);
        std::vector<jint> v(static_cast<size_t>(n));
        env->GetIntArrayRegion(spec, 0, n, v.data());
        for (jsize i = 0; i + 3 < n; i += 4)
            watches.push_back({GuestU32(v[size_t(i)]), int(v[size_t(i + 1)]), int(v[size_t(i + 2)]), v[size_t(i + 3)] != 0});
    }
    std::lock_guard lk(sM);
    sWatches = std::move(watches);
}

// [serial, value0, valid0, value1, valid1, ...]; serial is the guest frame number + 1 (0: no game).
JNIEXPORT jintArray JNICALL Java_io_github_sirbilliams_lmrecompiled_companion_CompanionNative_getSnapshot(JNIEnv* env,
                                                                                                         jclass) {
    std::vector<jint> out;
    {
        std::lock_guard lk(sM);
        out.reserve(1 + sWatches.size() * 2);
        out.push_back(recomp::g_mem ? jint(recomp::copyDispCount() + 1) : 0);
        for (const auto& w : sWatches) {
            GuestU32 v = 0;
            const bool ok = readValue(w, v);
            out.push_back(jint(v));
            out.push_back(ok ? 1 : 0);
        }
    }
    jintArray arr = env->NewIntArray(jsize(out.size()));
    env->SetIntArrayRegion(arr, 0, jsize(out.size()), out.data());
    return arr;
}

// Touch buttons: GameCube buttons (PAD_BUTTON_* mask) held for `frames` controller reads.
JNIEXPORT void JNICALL Java_io_github_sirbilliams_lmrecompiled_companion_CompanionNative_pressButtons(JNIEnv*, jclass,
                                                                                                      jint mask,
                                                                                                      jint frames) {
    recomp::controls::pressButtons(static_cast<unsigned short>(mask), frames);
}

// Opens or closes the F1 options menu on the main screen (the same F1 key event the menu handles).
JNIEXPORT void JNICALL Java_io_github_sirbilliams_lmrecompiled_companion_CompanionNative_toggleMenu(JNIEnv*, jclass) {
    SDL_Event ev{};
    ev.type = SDL_EVENT_KEY_DOWN;
    ev.key.key = SDLK_F1;
    ev.key.scancode = SDL_SCANCODE_F1;
    ev.key.down = true;
    SDL_PushEvent(&ev);
}

}  // extern "C"
#endif
