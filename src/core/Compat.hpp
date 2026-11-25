#pragma once

// The few places where Geode v5 (GD 2.2081) and Geode v6 (GD 2.2091) differ.

#include <Geode/Geode.hpp>

#if GEODE_COMP_GD_VERSION >= 22090
    #define NT_GD_2209 1
#else
    #define NT_GD_2209 0
#endif

namespace nt::compat {
    // the string type GD's own virtual functions take (needed to override them)
#if NT_GD_2209
    using GdStr = std::string;
#else
    using GdStr = gd::string;
#endif

    inline cocos2d::CCScene* playScene(GJGameLevel* level) {
    #if NT_GD_2209
        return PlayLayer::scene(level, false);
    #else
        return PlayLayer::scene(level, false, false);
    #endif
    }

    inline void registerForcePrio(cocos2d::CCObject* obj) {
    #if NT_GD_2209
        cocos2d::CCTouchDispatcher::get()->registerForcePrio(obj, 2, 0);
    #else
        cocos2d::CCTouchDispatcher::get()->registerForcePrio(obj, 2);
    #endif
    }

    // gd::string on 2.2081, std::string on 2.2091
    template <class S>
    inline std::string str(S const& s) {
        return std::string(s);
    }
}
