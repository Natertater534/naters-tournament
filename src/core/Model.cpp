#include "Model.hpp"

#include <algorithm>
#include <fmt/format.h>

namespace nt {
    bool jhas(matjson::Value const& v, std::string_view key) {
        return v.isObject() && v.contains(key) && !v[key].isNull();
    }

    double jnum(matjson::Value const& v, std::string_view key, double def) {
        if (!jhas(v, key)) return def;
        auto const& x = v[key];
        if (!x.isNumber()) return def;
        return x.asDouble().unwrapOr(def);
    }

    int jint(matjson::Value const& v, std::string_view key, int def) {
        return static_cast<int>(jnum(v, key, def));
    }

    int64_t jlong(matjson::Value const& v, std::string_view key, int64_t def) {
        return static_cast<int64_t>(jnum(v, key, static_cast<double>(def)));
    }

    bool jbool(matjson::Value const& v, std::string_view key, bool def) {
        if (!jhas(v, key)) return def;
        auto const& x = v[key];
        if (!x.isBool()) return def;
        return x.asBool().unwrapOr(def);
    }

    std::string jstr(matjson::Value const& v, std::string_view key, std::string def) {
        if (!jhas(v, key)) return def;
        auto const& x = v[key];
        if (!x.isString()) return def;
        return x.asString().unwrapOr(def);
    }

    static double anum(matjson::Value const& arr, size_t i) {
        if (!arr.isArray() || i >= arr.size()) return 0.0;
        auto const& x = arr[i];
        return x.isNumber() ? x.asDouble().unwrapOr(0.0) : 0.0;
    }

    matjson::Value Icon::toJson() const {
        return matjson::makeObject({{"c", cube}, {"p", p}, {"s", s}, {"g", g}});
    }

    Icon Icon::fromJson(matjson::Value const& v) {
        Icon i;
        i.cube = std::max(1, jint(v, "c", 1));
        i.p = jint(v, "p", 0);
        i.s = jint(v, "s", 3);
        i.g = jint(v, "g", -1);
        return i;
    }

    Player const* State::player(int account) const {
        if (!account) return nullptr;
        for (auto const& p : players) {
            if (p.account == account) return &p;
        }
        return nullptr;
    }

    Match const* State::match(int round, int slot) const {
        for (auto const& m : matches) {
            if (m.round == round && m.slot == slot) return &m;
        }
        return nullptr;
    }

    Match* State::matchMut(int round, int slot) {
        for (auto& m : matches) {
            if (m.round == round && m.slot == slot) return &m;
        }
        return nullptr;
    }

    Match const* State::currentMatch(int account) const {
        if (!account) return nullptr;
        Match const* best = nullptr;
        for (auto const& m : matches) {
            if (!m.has(account)) continue;
            if (m.live()) return &m;
            if (!best || m.round > best->round) best = &m;
        }
        return best;
    }

    int State::nextRound() const {
        if (!t.running()) return t.lobby() ? 1 : 0;
        for (auto const& m : matches) {
            if (m.round == t.round && m.state == "pending" && (m.p1 || m.p2)) return t.round;
        }
        return t.round < t.rounds ? t.round + 1 : 0;
    }

    std::vector<Player const*> State::activePlayers() const {
        std::vector<Player const*> out;
        for (auto const& p : players) {
            if (p.status != "left") out.push_back(&p);
        }
        return out;
    }

    bool State::apply(matjson::Value const& v) {
        if (jhas(v, "t")) {
            auto const& j = v["t"];
            t.code = jstr(j, "code");
            t.name = jstr(j, "name");
            t.pub = jbool(j, "pub", true);
            t.gd = jstr(j, "gd");
            t.host = jint(j, "host");
            t.hostName = jstr(j, "hostName");
            t.state = jstr(j, "state", "lobby");
            t.max = jint(j, "max", 16);
            t.round = jint(j, "round");
            t.rounds = jint(j, "rounds");
            t.size = jint(j, "size");
            t.time = jint(j, "time", 300);
            t.champ = jint(j, "champ");
            t.rev = jlong(j, "rev", -1);
            t.count = jint(j, "count");
            t.levelRound = jint(j, "levelRound");
            t.pool = jint(j, "pool", 96);
            t.level = {};
            if (jhas(j, "level")) {
                auto const& l = j["level"];
                t.level.id = jlong(l, "id");
                t.level.name = jstr(l, "name");
                t.level.author = jstr(l, "author");
                t.level.face = jint(l, "diff", 1) - 1;
            }
        }
        if (jhas(v, "evLast")) evLast = std::max<int64_t>(evLast, jlong(v, "evLast"));

        bool replaced = false;
        if (jhas(v, "players") && v["players"].isArray()) {
            auto const& arr = v["players"];
            players.clear();
            for (size_t i = 0; i < arr.size(); ++i) {
                auto const& j = arr[i];
                Player p;
                p.account = jint(j, "a");
                p.user = jint(j, "u");
                p.name = jstr(j, "n", "Player");
                p.icon = jhas(j, "i") ? Icon::fromJson(j["i"]) : Icon{};
                p.seed = jint(j, "seed");
                p.status = jstr(j, "st", "in");
                p.wins = jint(j, "w");
                p.best = static_cast<float>(jnum(j, "b"));
                if (p.account) players.push_back(std::move(p));
            }
            replaced = true;
        }
        if (jhas(v, "matches") && v["matches"].isArray()) {
            auto const& arr = v["matches"];
            matches.clear();
            for (size_t i = 0; i < arr.size(); ++i) {
                auto const& j = arr[i];
                Match m;
                m.round = jint(j, "r");
                m.slot = jint(j, "s");
                m.p1 = jint(j, "p1");
                m.p2 = jint(j, "p2");
                m.state = jstr(j, "st", "pending");
                m.level = jlong(j, "lv");
                m.levelName = jstr(j, "ln");
                m.startsAt = jlong(j, "sa");
                m.endsAt = jlong(j, "ea");
                m.winner = jint(j, "w");
                m.why = jstr(j, "why");
                m.b1 = static_cast<float>(jnum(j, "b1"));
                m.b2 = static_cast<float>(jnum(j, "b2"));
                m.a1 = jint(j, "a1");
                m.a2 = jint(j, "a2");
                matches.push_back(std::move(m));
            }
            replaced = true;
        }
        if (jhas(v, "pos") && v["pos"].isArray()) {
            auto const& arr = v["pos"];
            pos.clear();
            for (size_t i = 0; i < arr.size(); ++i) {
                auto const& row = arr[i];
                int acc = static_cast<int>(anum(row, 0));
                if (acc) pos[acc] = {static_cast<float>(anum(row, 1) / 1000.0), static_cast<int>(anum(row, 2))};
            }
        }
        if (jhas(v, "live") && v["live"].isArray()) {
            auto const& arr = v["live"];
            for (size_t i = 0; i < arr.size(); ++i) {
                auto const& row = arr[i];
                auto* m = matchMut(static_cast<int>(anum(row, 0)), static_cast<int>(anum(row, 1)));
                if (!m) continue;
                m->b1 = static_cast<float>(anum(row, 2));
                m->b2 = static_cast<float>(anum(row, 3));
                m->c1 = static_cast<float>(anum(row, 4));
                m->c2 = static_cast<float>(anum(row, 5));
                m->a1 = static_cast<int>(anum(row, 6));
                m->a2 = static_cast<int>(anum(row, 7));
            }
        }
        return replaced;
    }

    std::vector<Event> State::events(matjson::Value const& v) const {
        std::vector<Event> out;
        if (!jhas(v, "ev") || !v["ev"].isArray()) return out;
        auto const& arr = v["ev"];
        for (size_t i = 0; i < arr.size(); ++i) {
            auto const& j = arr[i];
            Event e;
            e.id = jlong(j, "id");
            e.kind = jstr(j, "k");
            e.account = jint(j, "a");
            e.data = jhas(j, "d") ? j["d"] : matjson::Value(matjson::makeObject({}));
            e.t = jlong(j, "t");
            out.push_back(std::move(e));
        }
        return out;
    }

    std::vector<ListEntry> parseList(matjson::Value const& v) {
        std::vector<ListEntry> out;
        if (!jhas(v, "list") || !v["list"].isArray()) return out;
        auto const& arr = v["list"];
        for (size_t i = 0; i < arr.size(); ++i) {
            auto const& j = arr[i];
            ListEntry e;
            e.code = jstr(j, "code");
            e.name = jstr(j, "name");
            e.host = jstr(j, "host");
            e.state = jstr(j, "state");
            e.max = jint(j, "max");
            e.round = jint(j, "round");
            e.rounds = jint(j, "rounds");
            e.n = jint(j, "n");
            e.age = jint(j, "age");
            if (!e.code.empty()) out.push_back(std::move(e));
        }
        return out;
    }

    std::string roundName(int round, int rounds) {
        if (rounds <= 0) return "";
        int fromEnd = rounds - round;
        if (fromEnd == 0) return "FINAL";
        if (fromEnd == 1) return "SEMIS";
        if (fromEnd == 2) return "QUARTERS";
        return fmt::format("ROUND {}", round);
    }

    std::string reasonText(std::string const& why) {
        if (why == "complete") return "FINISHED THE LEVEL";
        if (why == "time") return "TIME'S UP";
        if (why == "forfeit") return "FORFEIT";
        if (why == "host") return "HOST CALL";
        if (why == "bye") return "BYE";
        return "";
    }

    std::string formatClock(int seconds) {
        seconds = std::max(0, seconds);
        return fmt::format("{}:{:02}", seconds / 60, seconds % 60);
    }
}
