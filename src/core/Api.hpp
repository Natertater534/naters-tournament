#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <matjson.hpp>
#include <functional>

namespace nt::api {
    using Reply = geode::Result<matjson::Value, std::string>;
    using Callback = std::function<void(Reply)>;
    using Holder = geode::async::TaskHolder<geode::utils::web::WebResponse>;

    // The callback runs on the main thread, and never after `holder` is destroyed.
    void call(Holder& holder, std::string const& fn, matjson::Value args, Callback cb);
    // Fire-and-forget: the request outlives whoever sent it.
    void fire(std::string const& fn, matjson::Value args, Callback cb = nullptr);

    // Human-readable text for an error code from the server or the network.
    std::string describe(std::string const& code);

    int64_t localMs();
}
