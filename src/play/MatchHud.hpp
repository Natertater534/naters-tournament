#pragma once

// In-level race HUD: a track under GD's progress bar with both icons riding it,
// best-run flags, the match clock, the opponent's emotes, and the result.

#include <Geode/Geode.hpp>

namespace nt::play {
    using namespace cocos2d;

    class MatchHud : public CCNode {
    public:
        static MatchHud* create();

    protected:
        bool init() override;
        void update(float dt) override;
        float xFor(float percent) const;
        void showResult();

        CCNode* m_root = nullptr;
        SimplePlayer* m_opp = nullptr;
        SimplePlayer* m_mine = nullptr;
        CCSprite* m_oppGlow = nullptr;
        CCLayerColor* m_oppFlag = nullptr;
        CCLayerColor* m_myFlag = nullptr;
        CCLabelBMFont* m_oppFlagLabel = nullptr;
        CCLabelBMFont* m_oppName = nullptr;
        CCLabelBMFont* m_oppInfo = nullptr;
        CCLabelBMFont* m_clock = nullptr;
        CCLabelBMFont* m_note = nullptr;
        CCNode* m_bubble = nullptr;
        float m_trackW = 220.f;
        float m_oppShown = 0.f;
        float m_myShown = 0.f;
        float m_lastOppBest = 0.f;
        float m_lastOppCur = 0.f;
        int m_lastSecond = -1;
        bool m_resultShown = false;
        float m_resultTimer = 0.f;
    };
}
