#pragma once

// Level pools and the random level roll. Like Versus' random map: a GD online search of
// featured levels at a chosen difficulty, on a random page, then a random pick.

#include <Geode/Geode.hpp>
#include <functional>
#include "Compat.hpp"
#include "Model.hpp"

namespace nt {
    struct Difficulty {
        int bit;          // bit in the tournament's pool mask
        int face;         // GJDifficultySprite index
        char const* name;
        int gdDiff;       // GD search "diff": 1..5, or -2 for demons
        int demon;        // GD search "demonFilter": 1 easy .. 5 extreme, 0 for non-demons
    };

    std::vector<Difficulty> const& difficulties();
    std::vector<Difficulty> poolOptions(int pool);
    int faceFor(GJGameLevel* level);
    LevelRef refFor(GJGameLevel* level);

    class LevelRoller : public cocos2d::CCNode, public LevelManagerDelegate {
    public:
        using Done = std::function<void(LevelRef, std::string)>;  // a level, or an error
        static LevelRoller* create();
        void roll(int pool, Done done);
        bool busy() const { return static_cast<bool>(m_done); }

    protected:
        ~LevelRoller() override;
        void update(float dt) override;
        void request(int page);
        void finish(LevelRef level, std::string error);
        void nextOption();
        std::vector<LevelRef> usable(cocos2d::CCArray* levels);
        void pickFrom(std::vector<LevelRef> const& picks);
        void pickFirstPage();

        void loadLevelsFinished(cocos2d::CCArray* levels, char const* key) override;
        void loadLevelsFailed(char const* key) override;
        void loadLevelsFinished(cocos2d::CCArray* levels, char const* key, int type) override;
        void loadLevelsFailed(char const* key, int type) override;
        void setupPageInfo(compat::GdStr info, char const* key) override;

        Done m_done;
        std::vector<Difficulty> m_options;
        size_t m_option = 0;
        int m_total = -1;
        int m_requests = 0;
        bool m_pending = false;
        bool m_awaitTotal = false;
        std::vector<LevelRef> m_firstPage;
        std::string m_key;
        float m_timer = 0.f;
        float m_wait = 0.f;
        geode::Ref<GJSearchObject> m_search;
    };
}
