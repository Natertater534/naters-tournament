#include "Api.hpp"
#include "Config.hpp"

#include <chrono>
#include <list>

using namespace geode::prelude;

namespace nt::api {
    static web::WebRequest makeRequest(matjson::Value args) {
        web::WebRequest req;
        req.header("apikey", kApiKey);
        req.header("Content-Type", "application/json");
        req.header("Accept", "application/json");
        req.bodyJSON(matjson::makeObject({{"a", std::move(args)}}));
        return req;
    }

    static Reply parse(web::WebResponse& res) {
        if (!res.ok()) {
            if (res.code() <= 0) return Err(std::string("network"));
            log::warn("Tournament server answered {}: {}", res.code(), res.string().unwrapOr(""));
            return Err(fmt::format("http{}", res.code()));
        }
        auto json = res.json();
        if (json.isErr()) return Err(std::string("badjson"));
        auto value = std::move(json).unwrap();
        if (value.isObject() && value.contains("error")) {
            return Err(value["error"].asString().unwrapOr("error"));
        }
        return Ok(std::move(value));
    }

    void call(Holder& holder, std::string const& fn, matjson::Value args, Callback cb) {
        holder.spawn(
            makeRequest(std::move(args)).post(std::string(kApiUrl) + fn),
            [cb = std::move(cb)](web::WebResponse res) mutable {
                if (cb) cb(parse(res));
            }
        );
    }

    void fire(std::string const& fn, matjson::Value args, Callback cb) {
        // keep holders alive until their request ends; prune finished ones as we go
        static std::list<Holder> s_holders;
        s_holders.remove_if([](Holder const& h) { return !h.isPending(); });
        auto& holder = s_holders.emplace_back();
        call(holder, fn, std::move(args), std::move(cb));
    }

    std::string describe(std::string const& code) {
        if (code == "network") return "Can't reach the tournament server. Check your connection.";
        if (code == "notfound") return "That tournament doesn't exist anymore.";
        if (code == "full") return "That tournament is full.";
        if (code == "started") return "That tournament already started.";
        if (code == "taken") return "Your account is already in this tournament from another device.";
        if (code == "kicked") return "The host removed you from this tournament.";
        if (code == "version") return "That tournament is for a different Geometry Dash version.";
        if (code == "account") return "Log into your Geometry Dash account first.";
        if (code == "limit") return "You're already hosting 3 tournaments. Close one first.";
        if (code == "host") return "Only the host can do that.";
        if (code == "few") return "You need at least 2 players to start.";
        if (code == "level") return "Pick a level for this round first.";
        if (code == "busy") return "Wait for the current round to finish.";
        if (code == "state") return "Not possible right now.";
        if (code == "slow") return "Slow down a little!";
        if (code == "self") return "You can't kick yourself.";
        return "Something went wrong (" + code + ").";
    }

    int64_t localMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }
}
