#pragma once

#include <Geode/Geode.hpp>
#include <matjson.hpp>
#include <string>
#include <vector>
#include <map>

namespace nt {
    // ---- tiny JSON readers (never throw, default on anything odd)
    double jnum(matjson::Value const& v, std::string_view key, double def = 0.0);
    int jint(matjson::Value const& v, std::string_view key, int def = 0);
    int64_t jlong(matjson::Value const& v, std::string_view key, int64_t def = 0);
    bool jbool(matjson::Value const& v, std::string_view key, bool def = false);
    std::string jstr(matjson::Value const& v, std::string_view key, std::string def = "");
    bool jhas(matjson::Value const& v, std::string_view key);

    struct Icon {
        int cube = 1;
        int p = 0;   // primary colour index
        int s = 3;   // secondary colour index
        int g = -1;  // glow colour index, -1 = no glow

        matjson::Value toJson() const;
        static Icon fromJson(matjson::Value const& v);
    };

    struct Player {
        int account = 0;
        int user = 0;
        std::string name;
        Icon icon;
        int seed = 0;
        std::string status = "in";  // in | out | left
        int wins = 0;
        float best = 0.f;

        bool active() const { return status == "in"; }
    };

    struct Match {
        int round = 0;
        int slot = 0;
        int p1 = 0;
        int p2 = 0;
        std::string state = "pending";  // pending | live | done
        int64_t level = 0;
        std::string levelName;
        int64_t startsAt = 0;
        int64_t endsAt = 0;
        int winner = 0;
        std::string why;
        float b1 = 0.f, b2 = 0.f;  // best %
        float c1 = 0.f, c2 = 0.f;  // current run %
        int a1 = 0, a2 = 0;        // attempts

        bool live() const { return state == "live"; }
        bool done() const { return state == "done"; }
        bool has(int acc) const { return acc && (p1 == acc || p2 == acc); }
        int other(int acc) const { return p1 == acc ? p2 : p1; }
    };

    struct LevelRef {
        int64_t id = 0;
        std::string name;
        std::string author;
        int face = 0;  // GJDifficultySprite index: -1 auto, 0 n/a, 1-5, 6 hard demon, 7-10 demons
        bool valid() const { return id > 0; }
    };

    struct Tournament {
        std::string code;
        std::string name;
        bool pub = true;
        std::string gd;
        int host = 0;
        std::string hostName;
        std::string state = "lobby";  // lobby | running | finished
        int max = 16;
        int round = 0;
        int rounds = 0;
        int size = 0;
        int time = 300;
        int champ = 0;
        int64_t rev = -1;
        LevelRef level;
        int levelRound = 0;  // the round the current level was rolled for
        int pool = 96;       // which difficulties levels are rolled from (see Levels.hpp)
        int count = 0;

        bool lobby() const { return state == "lobby"; }
        bool running() const { return state == "running"; }
        bool finished() const { return state == "finished"; }
    };

    struct Event {
        int64_t id = 0;
        std::string kind;  // join leave kick emote start level round result champion close created
        int account = 0;
        matjson::Value data;
        int64_t t = 0;
    };

    struct ListEntry {
        std::string code;
        std::string name;
        std::string host;
        std::string state;
        int max = 0;
        int round = 0;
        int rounds = 0;
        int n = 0;
        int age = 0;
    };

    struct State {
        Tournament t;
        std::vector<Player> players;
        std::vector<Match> matches;
        int64_t evLast = 0;
        // lobby: where each player stands (0..1 across the stage) and how many times they've jumped
        std::map<int, std::pair<float, int>> pos;
        // the round the host should start next (or 0 when there's none left)
        int nextRound() const;

        Player const* player(int account) const;
        Match const* match(int round, int slot) const;
        Match* matchMut(int round, int slot);
        // the match an account is in right now: live first, then the newest pending/done one
        Match const* currentMatch(int account) const;
        std::vector<Player const*> activePlayers() const;
        bool isHost(int account) const { return account && t.host == account; }

        // Applies one server response. Returns true when players/matches were replaced.
        bool apply(matjson::Value const& v);
        std::vector<Event> events(matjson::Value const& v) const;
    };

    std::vector<ListEntry> parseList(matjson::Value const& v);

    std::string roundName(int round, int rounds);
    std::string reasonText(std::string const& why);
    std::string formatClock(int seconds);
}
