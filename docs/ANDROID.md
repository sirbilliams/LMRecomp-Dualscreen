# Android port (AYN Thor)

Fork: `sirbilliams/LMRecomp-Dualscreen`, branch `android`. Goal: run Luigi's Mansion Recompiled
natively on the AYN Thor, then use its second screen as a companion (F1 menu with touch, map,
HUD). Everything Android-specific is behind `#ifdef __ANDROID__` / `if (ANDROID)` so upstream
updates merge cleanly.

## How it fits together

| Piece | Where |
|------|-------|
| Engine (Dawn on Vulkan, SDL3, nod) | Aurora already supports Android; CMake forces SDL3 and nod to build from source (`AURORA_*_PROVIDER=vendor`). |
| App shell (activity, surface lifecycle, launch args, Files-app provider, Gradle staging) | `extern/borealis` (MIT, encounter/borealis); `platforms/android` (Gradle project, based on Dusk's CC0 Android shell). |
| Program | `LuigisMansionRecomp` is built as `libmain.so` (`add_library(SHARED)` + `borealis_configure_android_application`). |
| Game module | Compiled **on the phone** from the player's disc by setup, exactly as on PC, with a bundled clang + ld.lld for arm64 Android (`--target=aarch64-linux-android28`; the module is freestanding, so no NDK sysroot is needed). |
| On-device compiler | Built by `.github/workflows/android-llvm.yml` (LLVM 20, AArch64 only, static libc++) and published as the release `android-llvm-<ver>`. Shipped in the APK as `libclang_exe.so` / `liblld_exe.so`, because the native library folder is the only app folder Android lets a process execute from (`useLegacyPackaging = true` extracts them). `run()` sets argv[0] to `clang` / `ld.lld` so they pick the right mode. At most 4 compile jobs on Android (`LM_BUILD_JOBS` overrides). |

## Android-specific runtime code

- `src/port/android.{h,cpp}`: data folder (`SDL_GetAndroidInternalStoragePath`), native library
  folder (`dladdr`), log redirection, APK asset extraction, disc import, relaunch.
- Data lives in `<files>/config`, `<files>/data`, `<files>/cache` (`install.cpp`); all of it is
  browsable in the Files app under "LM Recompiled Data".
- **Log:** stdout/stderr go to `data/log.txt` (previous run: `log.prev.txt`); crash reports land there too.
- **Disc images:** the picker returns a `content://` URI that nod cannot open, so setup copies the
  image to `data/discs/GLME01.iso` (or `.rvz`, ...) with a progress line, then validates it as usual.
- **Shader precache:** `data/pipelines.bin` ships as an APK asset and is extracted on start.
- **Updater:** `src/port/update_android.cpp` replaces `update.cpp` (updates come as a new APK).
- **Crash handler:** bionic has no `backtrace()` before API 33; `crash_handler.cpp` uses `_Unwind_Backtrace` + `dladdr`.
- **Relaunch** (F1 > switch version / re-run setup): starts the launch intent (with
  `borealis_argv = --setup` when needed) and exits; Android starts it in a new process.

## Companion screen (AYN Thor bottom screen)

Ported from the Dolphin Dualscreen companion (`sirbilliams/Dolphin-Dualscreen`), same look and
behaviour: mansion map following Luigi's room, health, Boos, treasure, keys.

- Java: `platforms/android/.../companion/`. `CompanionScreenController` polls every 100 ms while
  `LMActivity` is resumed, opens a `CompanionPresentation` (non-focusable, so the controller and
  the game keep focus) on the first display that is not the game's, and feeds the view.
- Native: `src/port/android_companion.cpp` (`CompanionNative.gameId/setWatches/getSnapshot`). The
  game runs natively, so watches read guest memory directly; the serial is the guest frame number.
- Addresses are the USA release's (GLME01, from the Luigi's Mansion Archipelago client, MIT); the
  PAL base gets no companion yet.
- Floor images (optional): `data/companion/LuigisMansion/` in the app's data folder (floor_1.png,
  f2.png, f3.png, b1.png, roof.png, lab.png from the Luigi's Mansion PopTracker pack). Without them
  the floors are drawn as labelled room markers. Tap the map to page floors; tap the header to follow Luigi.
- "Pause when window loses focus" defaults to off on Android.

## Building

CI (`.github/workflows/android.yml`) builds the APK on every push. Locally (Linux, Android SDK + NDK 29.0.14206865, JDK 17, Rust with the `aarch64-linux-android` target):

```
export ANDROID_HOME=~/Android/Sdk ANDROID_NDK_VERSION=29.0.14206865
cmake --preset android-arm64
cmake --build --preset android-arm64
# optional: the on-device compiler release unpacked into build/android-llvm (jniLibs/arm64-v8a/*.so)
cd platforms/android && ./gradlew :app:assembleRelease
```

## Roadmap

1. First boot on the Thor, single screen (this branch).
2. Thor performance preset: 60 fps target, Enhanced lighting / AO / volumetric beam off.
3. Companion screen: map + status (done, ported from Dolphin Dualscreen).
4. Next: touch buttons on the companion (F1 menu, GBH, call E. Gadd), option to hide the top-screen
   HUD while the companion shows it (hooks on `fn_8003E3D0`), Pluto-style theming.
5. Later: F1 menu (ImGui) rendered on the bottom screen (second Dawn surface).
