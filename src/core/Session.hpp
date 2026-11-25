#pragma once

#include "Model.hpp"
#include <Geode/Geode.hpp>

namespace nt {
    struct Me {
        int account = 0;
        int user = 0;
        std::string name;
        Icon icon;
        bool loggedIn() const { return account > 0 && !name.empty(); }
    };

    // Who the local player is, plus the per-install secret that proves it to the server.
    Me me();
    std::string secret();

    // {account_id, user_id, username, icon, secret, gd} merged into `extra`.
    matjson::Value withAuth(matjson::Value extra = matjson::makeObject({}));

    // The tournament this install last joined or hosted (so a restart can find its way back).
    std::string lastCode();
    void setLastCode(std::string const& code);

    // Server clock: offset learned from responses (server ms - local ms).
    void noteServerTime(int64_t serverMs, int64_t sentLocalMs, int64_t receivedLocalMs);
    int64_t serverNowMs();
}
