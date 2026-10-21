// Android build of the updater interface (docs/ANDROID.md): the APK is installed and updated
// through Android, so there is nothing to download or swap in place. The menu sees an idle
// updater that never notifies. update.cpp (WinHTTP / curl + tar) is not built on Android.
#include "port/update.h"

#include <algorithm>
#include <cstdlib>

namespace port::update {

const char* buildVersion() { return LM_VERSION; }

const char* currentVersion() {
    static const std::string v = [] {
        const char* o = std::getenv("LM_UPDATE_VERSION");
        std::string s = o && *o ? o : LM_VERSION;
        if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) s.erase(s.begin());
        return s;
    }();
    return v.c_str();
}

int compareVersions(const std::string& a, const std::string& b) {
    auto parts = [](std::string s) {
        if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) s.erase(s.begin());
        if (const auto dash = s.find_first_of("-+ "); dash != std::string::npos) s.resize(dash);
        std::vector<long> v;
        size_t pos = 0;
        while (pos <= s.size()) {
            size_t e = s.find('.', pos);
            if (e == std::string::npos) e = s.size();
            v.push_back(std::atol(s.substr(pos, e - pos).c_str()));
            pos = e + 1;
        }
        while (!v.empty() && v.back() == 0) v.pop_back();
        return v;
    };
    const auto x = parts(a), y = parts(b);
    for (size_t i = 0; i < std::max(x.size(), y.size()); i++) {
        const long p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
        if (p != q) return p < q ? -1 : 1;
    }
    return 0;
}

State state() {
    State s;
    s.message = "Updates come with a new APK.";
    s.installWritable = false;
    s.packaged = false;
    return s;
}

void startupCheck() {}
void checkNow() {}
void download() {}
void cancel() {}
void installAndRestart() {}
bool shouldNotify() { return false; }
void skipThisVersion() {}
void later() {}
bool dismissed() { return true; }
void cleanupAfterUpdate() {}

}  // namespace port::update
