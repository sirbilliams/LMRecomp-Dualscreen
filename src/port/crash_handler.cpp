// Prints a symbolized stack trace on abort() / std::terminate / unhandled SEH exceptions
// (Windows) or fatal signals (Linux: SIGSEGV, SIGBUS, SIGILL, SIGFPE), so crashes in game or
// library code can be located from a log.
#include "port/crash_handler.h"

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <chrono>
#include <exception>
#include <initializer_list>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#else
#include <dlfcn.h>
#ifdef __ANDROID__
#include <unwind.h>
#else
#include <execinfo.h>
#endif
#include <ucontext.h>
#include <unistd.h>
#endif

#ifdef __ANDROID__
// Bionic only has backtrace() from API 33; the app targets 28. Same output shape as glibc's
// backtrace_symbols_fd ("binary(symbol+off) [addr]"), written without allocating.
namespace {
struct UnwindState {
    void** frames;
    int max, count;
};
_Unwind_Reason_Code unwindOne(_Unwind_Context* ctx, void* arg) {
    auto* st = static_cast<UnwindState*>(arg);
    const uintptr_t pc = _Unwind_GetIP(ctx);
    if (pc && st->count < st->max) st->frames[st->count++] = reinterpret_cast<void*>(pc);
    return st->count < st->max ? _URC_NO_REASON : _URC_END_OF_STACK;
}
int backtrace(void** frames, int max) {
    UnwindState st{frames, max, 0};
    _Unwind_Backtrace(&unwindOne, &st);
    return st.count;
}
void backtrace_symbols_fd(void* const* frames, int count, int fd) {
    for (int i = 0; i < count; i++) {
        Dl_info dl{};
        char line[512];
        int n;
        if (dladdr(frames[i], &dl) && dl.dli_fname) {
            const char* base = static_cast<const char*>(dl.dli_saddr ? dl.dli_saddr : dl.dli_fbase);
            n = std::snprintf(line, sizeof(line), "%s(%s+0x%lx) [%p] (+0x%lx)\n", dl.dli_fname,
                              dl.dli_sname ? dl.dli_sname : "",
                              static_cast<unsigned long>(static_cast<const char*>(frames[i]) - base), frames[i],
                              static_cast<unsigned long>(static_cast<const char*>(frames[i]) -
                                                         static_cast<const char*>(dl.dli_fbase)));
        } else {
            n = std::snprintf(line, sizeof(line), "? [%p]\n", frames[i]);
        }
        if (n > 0) (void)!write(fd, line, size_t(n) < sizeof(line) ? size_t(n) : sizeof(line) - 1);
    }
}
}  // namespace
#endif

namespace port {
namespace {

void printStack(const char* reason) {
    std::fprintf(stderr, "\n=== crash: %s ===\n", reason);
#ifdef _WIN32
    void* frames[64];
    const USHORT count = CaptureStackBackTrace(0, 64, frames, nullptr);
    // Module + offset first, before loading symbols: symbol loading for a large game module
    // can take long enough that a killed or hung process never prints the symbolized trace
    // (llvm-symbolizer --obj=<module> 0x<offset> resolves these offline).
    std::fprintf(stderr, "  thread %lu, raw frames:\n", static_cast<unsigned long>(GetCurrentThreadId()));
    for (USHORT i = 0; i < count; i++) {
        HMODULE mod = nullptr;
        char name[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               static_cast<LPCSTR>(frames[i]), &mod))
            GetModuleFileNameA(mod, name, sizeof(name));
        const char* base = std::strrchr(name, '\\');
        std::fprintf(stderr, "  #%02u %s+0x%llx\n", i, base ? base + 1 : name,
                     static_cast<unsigned long long>(static_cast<const char*>(frames[i]) - reinterpret_cast<const char*>(mod)));
    }
    std::fflush(stderr);
    HANDLE process = GetCurrentProcess();
    static bool sSymInit = SymInitialize(process, nullptr, TRUE);
    (void)sSymInit;
    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256];
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
    for (USHORT i = 0; i < count; i++) {
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        DWORD64 displacement = 0;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisp = 0;
        const DWORD64 addr = reinterpret_cast<DWORD64>(frames[i]);
        const bool haveSym = SymFromAddr(process, addr, &displacement, symbol);
        const bool haveLine = SymGetLineFromAddr64(process, addr, &lineDisp, &line);
        std::fprintf(stderr, "  #%02u %s+0x%llx", i, haveSym ? symbol->Name : "?",
                     static_cast<unsigned long long>(displacement));
        if (haveLine) {
            std::fprintf(stderr, " (%s:%lu)", line.FileName, line.LineNumber);
        }
        std::fputc('\n', stderr);
    }
#else
    // backtrace_symbols_fd writes "binary(symbol+off) [addr]" lines without allocating; the
    // executable's own functions are named only for exported symbols, so the offsets are
    // what to feed to addr2line (docs/LINUX.md).
    std::fflush(stderr);
    void* frames[64];
    const int count = backtrace(frames, 64);
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
#endif
    std::fflush(stderr);
}

void onAbort(int) {
    printStack("abort()");
    std::_Exit(3);
}

#ifdef _WIN32
// First chance, before anything unwinds: stack overflows and faults in the game module DLL
// may never reach the unhandled-exception filter with enough stack to symbolize, so report
// the code, the faulting address and the module it is in with minimal stack use.
LONG WINAPI onVectored(EXCEPTION_POINTERS* info) {
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION)
        return EXCEPTION_CONTINUE_SEARCH;
    static volatile LONG sOnce = 0;
    if (InterlockedExchange(&sOnce, 1)) return EXCEPTION_CONTINUE_SEARCH;
    char buf[256];
    HMODULE mod = nullptr;
    char name[128] = "?";
    const void* pc = info->ExceptionRecord->ExceptionAddress;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCSTR>(pc), &mod))
        GetModuleFileNameA(mod, name, sizeof(name));
    const int n = wsprintfA(buf, "\n=== exception 0x%08lX at %p (%s+0x%lX), rsp %p ===\n", code, pc, name,
                            static_cast<unsigned long>(static_cast<const char*>(pc) - reinterpret_cast<const char*>(mod)),
                            reinterpret_cast<void*>(info->ContextRecord->Rsp));
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), buf, DWORD(n), &written, nullptr);
    return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI onException(EXCEPTION_POINTERS* info) {
    char reason[64];
    std::snprintf(reason, sizeof(reason), "exception 0x%08lX", info->ExceptionRecord->ExceptionCode);
    printStack(reason);
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
// Fatal signals (Linux): report the signal, the faulting address, the instruction and the
// binary it is in (e.g. the game module .so), then the stack. Runs on an alternate stack on
// the main thread so a stack overflow there can still be reported. Afterwards the default
// action runs (core dump / exit status of the signal).
volatile sig_atomic_t sInSignal = 0;

void onSignal(int sig, siginfo_t* info, void* uctx) {
    if (sInSignal) {
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }
    sInSignal = 1;
    const void* pc = nullptr;
    const void* sp = nullptr;
#if defined(__x86_64__)
    if (uctx) {
        const auto* uc = static_cast<const ucontext_t*>(uctx);
        pc = reinterpret_cast<const void*>(uc->uc_mcontext.gregs[REG_RIP]);
        sp = reinterpret_cast<const void*>(uc->uc_mcontext.gregs[REG_RSP]);
    }
#elif defined(__aarch64__)
    if (uctx) {
        const auto* uc = static_cast<const ucontext_t*>(uctx);
        pc = reinterpret_cast<const void*>(uc->uc_mcontext.pc);
        sp = reinterpret_cast<const void*>(uc->uc_mcontext.sp);
    }
#endif
    Dl_info dl{};
    const bool haveDl = pc && dladdr(pc, &dl) && dl.dli_fname;
    char buf[512];
    const int n = std::snprintf(buf, sizeof(buf), "\n=== signal %d (%s) at pc %p (%s+0x%lx), fault address %p, sp %p ===\n",
                                sig, strsignal(sig), pc, haveDl ? dl.dli_fname : "?",
                                haveDl ? static_cast<unsigned long>(static_cast<const char*>(pc) -
                                                                    static_cast<const char*>(dl.dli_fbase))
                                       : 0ul,
                                info ? info->si_addr : nullptr, sp);
    if (n > 0) (void)!write(STDERR_FILENO, buf, size_t(n) < sizeof(buf) ? size_t(n) : sizeof(buf) - 1);
    printStack(strsignal(sig));
    signal(sig, SIG_DFL);
    raise(sig);
}

void installSignalHandlers() {
    // backtrace() loads libgcc_s on first use (allocates): do that now, not in a handler.
    void* warm[1];
    backtrace(warm, 1);
    static char sAltStack[64 * 1024];
    stack_t ss{};
    ss.ss_sp = sAltStack;
    ss.ss_size = sizeof(sAltStack);
    sigaltstack(&ss, nullptr);
    struct sigaction sa{};
    sa.sa_sigaction = &onSignal;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    for (int sig : {SIGSEGV, SIGBUS, SIGILL, SIGFPE}) sigaction(sig, &sa, nullptr);
}
#endif

#ifdef _WIN32
// ---- Hang watchdog (LM_HANG_DUMP=<seconds>) ----
std::atomic<unsigned long long> sHeartbeat{0};

void printThreadStack(HANDLE process, DWORD tid) {
    HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
    if (!th) return;
    if (SuspendThread(th) == DWORD(-1)) {
        CloseHandle(th);
        return;
    }
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL;
    if (GetThreadContext(th, &ctx)) {
        std::fprintf(stderr, "--- thread %lu ---\n", static_cast<unsigned long>(tid));
        STACKFRAME64 sf{};
        sf.AddrPC.Offset = ctx.Rip;
        sf.AddrPC.Mode = AddrModeFlat;
        sf.AddrFrame.Offset = ctx.Rbp;
        sf.AddrFrame.Mode = AddrModeFlat;
        sf.AddrStack.Offset = ctx.Rsp;
        sf.AddrStack.Mode = AddrModeFlat;
        alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256];
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        for (int i = 0; i < 40; i++) {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, th, &sf, &ctx, nullptr, SymFunctionTableAccess64,
                             SymGetModuleBase64, nullptr) ||
                !sf.AddrPC.Offset)
                break;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = 255;
            DWORD64 disp = 0;
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(line);
            DWORD lineDisp = 0;
            const bool haveSym = SymFromAddr(process, sf.AddrPC.Offset, &disp, symbol);
            const bool haveLine = SymGetLineFromAddr64(process, sf.AddrPC.Offset, &lineDisp, &line);
            std::fprintf(stderr, "  #%02d %s+0x%llx", i, haveSym ? symbol->Name : "?", static_cast<unsigned long long>(disp));
            if (haveLine) std::fprintf(stderr, " (%s:%lu)", line.FileName, line.LineNumber);
            std::fputc('\n', stderr);
        }
    }
    ResumeThread(th);
    CloseHandle(th);
}

// Every thread's stack (except the watchdog's own), symbolized: where a hung process waits.
void dumpAllThreads(const char* reason) {
    std::fprintf(stderr, "\n=== %s: all thread stacks ===\n", reason);
    HANDLE process = GetCurrentProcess();
    static bool sSymInit = SymInitialize(process, nullptr, TRUE);
    (void)sSymInit;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te))
        if (te.th32OwnerProcessID == pid && te.th32ThreadID != self) printThreadStack(process, te.th32ThreadID);
    CloseHandle(snap);
    std::fprintf(stderr, "=== end of thread stacks ===\n");
    std::fflush(stderr);
}
#endif

}  // namespace

void heartbeat() {
#ifdef _WIN32
    sHeartbeat.fetch_add(1, std::memory_order_relaxed);
#endif
}

void startHangWatchdog() {
#ifdef _WIN32
    const char* env = std::getenv("LM_HANG_DUMP");
    const int seconds = env ? std::atoi(env) : 0;
    if (seconds <= 0) return;
    std::thread([seconds] {
        unsigned long long last = sHeartbeat.load();
        int still = 0;
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            const unsigned long long now = sHeartbeat.load();
            if (now != last) {
                last = now;
                still = 0;
                continue;
            }
            // Only once the main loop has run at all (startup can block longer).
            if (now && ++still >= seconds && still < seconds + 3) {  // three samples, a second apart
                char reason[96];
                std::snprintf(reason, sizeof(reason), "main loop stalled for %d s (LM_HANG_DUMP)", seconds);
                dumpAllThreads(reason);
            }
        }
    }).detach();
#endif
}

void installCrashHandler() {
    std::signal(SIGABRT, onAbort);
    std::set_terminate([] {
        printStack("std::terminate");
        std::_Exit(3);
    });
#ifdef _WIN32
    SetUnhandledExceptionFilter(onException);
    AddVectoredExceptionHandler(1, onVectored);
    // Route CRT abort() through SIGABRT instead of an immediate fast-fail.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#else
    installSignalHandlers();
#endif
}

}  // namespace port
