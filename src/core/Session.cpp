#include "Session.hpp"
#include "Api.hpp"
#include "Compat.hpp"
#include "Config.hpp"

#include <random>

using namespace geode::prelude;

namespace nt {
    Me me() {
        Me m;
        auto* am = GJAccountManager::sharedState();
        auto* gm = GameManager::sharedState();
        if (am) {
            m.account = am->m_accountID;
            m.name = compat::str(am->m_username);
        }
        if (gm) {
            m.user = gm->m_playerUserID.value();
            m.icon.cube = std::max(1, gm->getPlayerFrame());
            m.icon.p = gm->getPlayerColor();
            m.icon.s = gm->getPlayerColor2();
            m.icon.g = gm->getPlayerGlow() ? gm->getPlayerGlowColor() : -1;
        }
        return m;
    }

    std::string secret() {
        auto value = Mod::get()->getSavedValue<std::string>("secret", "");
        if (value.size() >= 32) return value;
        std::random_device rd;
        std::mt19937_64 rng((static_cast<uint64_t>(rd()) << 32) ^ rd() ^ static_cast<uint64_t>(api::localMs()));
        static constexpr char hex[] = "0123456789abcdef";
        value.clear();
        for (int i = 0; i < 40; ++i) value += hex[rng() & 15];
        Mod::get()->setSavedValue<std::string>("secret", value);
        return value;
    }

    matjson::Value withAuth(matjson::Value extra) {
        auto m = me();
        extra["account_id"] = m.account;
        extra["user_id"] = m.user;
        extra["username"] = m.name;
        extra["icon"] = m.icon.toJson();
        extra["secret"] = secret();
        extra["gd"] = std::string(kGdVersion);
        return extra;
    }

    std::string lastCode() {
        return Mod::get()->getSavedValue<std::string>("last-code", "");
    }

    void setLastCode(std::string const& code) {
        Mod::get()->setSavedValue<std::string>("last-code", code);
    }

    static int64_t s_offset = 0;
    static int64_t s_bestRtt = -1;
    static int64_t s_offsetAt = 0;

    void noteServerTime(int64_t serverMs, int64_t sentLocalMs, int64_t receivedLocalMs) {
        if (serverMs <= 0) return;
        int64_t rtt = std::max<int64_t>(0, receivedLocalMs - sentLocalMs);
        // trust the lowest-latency sample, but refresh it now and then so drift can't pile up
        bool stale = receivedLocalMs - s_offsetAt > 60000;
        if (s_bestRtt < 0 || rtt <= s_bestRtt + 40 || stale) {
            s_offset = serverMs - (sentLocalMs + rtt / 2);
            s_bestRtt = stale ? rtt : (s_bestRtt < 0 ? rtt : std::min(s_bestRtt, rtt));
            s_offsetAt = receivedLocalMs;
        }
    }

    int64_t serverNowMs() {
        return api::localMs() + s_offset;
    }
}
