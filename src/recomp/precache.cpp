// Shader/pipeline precaching: see precache.h and docs/RENDERING.md ("Pipeline precache").
#include "recomp/precache.h"

#include "recomp/enhanced_lighting.h"
#include "recomp/gx_bridge.h"
#include "recomp/hle.h"

#include <aurora/pipeline_cache.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef __ANDROID__
#include "port/install.h"
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace recomp::precache {
namespace {

using Clock = std::chrono::steady_clock;

bool sLog = false;

std::filesystem::path exeDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameW(nullptr, buf, DWORD(std::size(buf)));
    return std::filesystem::path(std::wstring(buf, n)).parent_path();
#elif defined(__ANDROID__)
    // main.cpp extracts the shipped pipelines.bin from the APK assets into the data folder.
    return port::dataDir();
#else
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
}

}  // namespace

void configure() {
    // LM_PIPELINE_THREADS=<n>: pipeline compile threads (default: half the hardware threads, 1..8).
    if (const char* t = std::getenv("LM_PIPELINE_THREADS")) aurora_pipeline_cache_set_threads(uint32_t(std::atoi(t)));
}

void init() {
    // LM_PIPELINE_LOG=1: every pipeline creation (precache/demand, time), every draw that had to
    // wait for its pipeline, and a summary line every 5 s with the worst frame time.
    sLog = std::getenv("LM_PIPELINE_LOG") != nullptr;
    aurora_pipeline_cache_set_log(sLog);
    // LM_PIPELINE_WAIT=0 (testing): skip draws whose pipeline is not ready, as Aurora did before
    // patch 0013 (reproduces the first-run flicker).
    if (const char* w = std::getenv("LM_PIPELINE_WAIT"); w && std::atoi(w) == 0) aurora_pipeline_cache_set_wait(false);
    // The GX accuracy flags select shader variants; set them before the first frame builds the
    // known pipelines, or the first build is for the default flags and wasted.
    recomp::gx::applyAccuracySettings();
    // Likewise the enhanced-lighting parts that select shader variants (Aurora 0217): otherwise the
    // first build is for enhanced lighting off and the second frame queues its variants again.
    recomp::enh::applySettings();
    // LM_PRECACHE=0: no shipped list (cold-cache testing). LM_PIPELINE_LIST=<file>: another list.
    if (const char* p = std::getenv("LM_PRECACHE"); p && std::atoi(p) == 0) return;
    std::string list;
    if (const char* p = std::getenv("LM_PIPELINE_LIST")) list = p;
    else list = (exeDir() / "pipelines.bin").string();
    const uint32_t n = aurora_pipeline_cache_seed(list.c_str());
    if (sLog) log("[precache] %u configs from %s", n, list.c_str());
}

namespace {
// Startup: the known set is (re)built on the first frames (Aurora begin_pipeline_frame) once the
// settings are applied (menu::draw on the first hold frame sets the enhanced-lighting parts, which
// can queue a second batch on the next frame), so the hold lasts at least kMinHoldFrames frames.
constexpr int kMinHoldFrames = 3;
bool sStartupDone = false;
int sHoldFrames = 0;
Clock::time_point sHoldStart{};
bool sHolding = false;
uint32_t sHoldMax = 0;  // most pending pipelines seen in this hold (the bar's total)
bool sSettingsHold = false;  // the current hold is for a settings change (not the startup one)
}  // namespace

bool holdGame() {
    static const bool sNoWait = [] {
        const char* v = std::getenv("LM_PRECACHE_WAIT");
        return v && std::atoi(v) == 0;
    }();
    AuroraPipelineStats s{};
    aurora_pipeline_cache_get_stats(&s);
    bool hold;
    if (!sStartupDone) {
        hold = sHoldFrames < kMinHoldFrames || s.rebuildPending > 0;
        if (sNoWait && sHoldFrames >= 1) hold = false;
        if (!hold) {
            sStartupDone = true;
            // LM_PIPELINE_EXPORT=<file> (list maintenance, tools/pipelines): write the known set
            // (shipped list + this cache, canonical form) once loaded.
            // (with LM_PRECACHE_WAIT=0 the export is written on the first frame, before compiling;
            // LM_PIPELINE_EXPORT_EXIT=1 then ends the program).
            if (const char* path = std::getenv("LM_PIPELINE_EXPORT")) {
                aurora_pipeline_cache_export(path);
                if (std::getenv("LM_PIPELINE_EXPORT_EXIT")) std::_Exit(0);
            }
            log("[precache] game starts after %.2f s of shader preparation (%u pipelines, %u threads)%s",
                std::chrono::duration<double>(Clock::now() - sHoldStart).count(), s.precacheDone, s.workerThreads,
                sNoWait ? " (LM_PRECACHE_WAIT=0: not waited for)" : "");
        }
    } else {
        hold = !sNoWait && s.rebuildPending > 0;
    }
    if (hold && !sHolding) {
        sHoldStart = Clock::now();
        sHoldMax = 0;
        sSettingsHold = sStartupDone;
        if (sStartupDone) log("[precache] settings change: game held for %u pipelines", s.rebuildPending);
    } else if (!hold && sHolding && sSettingsHold) {
        log("[precache] resumed after %.2f s", std::chrono::duration<double>(Clock::now() - sHoldStart).count());
    }
    sHolding = hold;
    sHoldMax = std::max(sHoldMax, s.rebuildPending);
    return hold;
}

void drawHold() {
    sHoldFrames++;
    AuroraPipelineStats s{};
    aurora_pipeline_cache_get_stats(&s);
    // With a warm cache the whole set loads in about a second: show nothing for the first moment.
    const double t = std::chrono::duration<double>(Clock::now() - sHoldStart).count();
    if (t < 0.3) return;
    const uint32_t total = std::max(sHoldMax, 1u);
    const uint32_t done = total - std::min(s.rebuildPending, total);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float scale = std::max(1.0f, vp->WorkSize.y / 720.0f);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420 * scale, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::Begin("##preparing", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowFontScale(scale);
    if (sHoldMax < 64 && t < 2.0) {
        ImGui::TextUnformatted("Loading...");
    } else {
        ImGui::Text("Preparing shaders... %u / %u", done, total);
        ImGui::ProgressBar(float(done) / float(total), ImVec2(-1, 0), "");
        ImGui::TextDisabled(sStartupDone ? "New graphics settings: the game continues when this is done."
                                             : "Once per graphics setup; later starts take about a second.");
    }
    ImGui::End();
}

Progress progress() {
    AuroraPipelineStats s{};
    aurora_pipeline_cache_get_stats(&s);
    return Progress{s.precacheDone, s.precacheTotal};
}

void frame() {
    static Clock::time_point start = Clock::now(), last = start, window = start;
    static float worstMs = 0.0f;
    static uint32_t frames = 0;
    const auto now = Clock::now();
    worstMs = std::max(worstMs, std::chrono::duration<float, std::milli>(now - last).count());
    last = now;
    frames++;

    AuroraPipelineStats s{};
    aurora_pipeline_cache_get_stats(&s);
    // The startup precompile time (fresh cache: the whole shipped list), logged once.
    static bool sReported = false;
    if (sLog && !sReported && s.precacheTotal > 0 && s.precacheDone >= s.precacheTotal) {
        sReported = true;
        log("[precache] all %u known pipelines ready after %.2f s (%u threads)", s.precacheTotal,
            std::chrono::duration<double>(now - start).count(), s.workerThreads);
    }
    if (sLog && now - window >= std::chrono::seconds(5)) {
        log("[precache] %.0fs: precache %u/%u, pending %u, demand-created %u, stalled draws %u (%.1f ms total, "
            "max %.1f ms), dropped draws %u, worst frame %.1f ms over %u frames, %u threads",
            std::chrono::duration<double>(now - start).count(), s.precacheDone, s.precacheTotal, s.pending,
            s.demandCreated, s.stalledDraws, s.stallNs / 1e6, s.maxStallNs / 1e6, s.droppedDraws, worstMs, frames,
            s.workerThreads);
        window = now;
        worstMs = 0.0f;
        frames = 0;
    }

    // Small corner note while the background compile is still running (first launch without
    // setup, after a driver update or a settings change that needs other shaders). Loading the
    // whole list from the shader cache takes about 1 s on every launch, so the note only appears
    // once that is clearly exceeded, i.e. when shaders are really being compiled.
    // (Only without the hold, LM_PRECACHE_WAIT=0; single new configs met in play compile unnoticed.)
    if (!sHolding && s.rebuildPending > 0 && now - start > std::chrono::seconds(4)) {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 8, vp->WorkPos.y + vp->WorkSize.y - 8),
                                ImGuiCond_Always, ImVec2(1, 1));
        ImGui::SetNextWindowBgAlpha(0.4f);
        ImGui::Begin("##precache", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoNav);
        ImGui::Text("Compiling shaders: %u left", s.rebuildPending);
        ImGui::End();
    }
}

}  // namespace recomp::precache
