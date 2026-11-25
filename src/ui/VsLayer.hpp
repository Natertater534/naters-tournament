#pragma once

// The face-off before a match: both icons slam in, VS, the level card, a synced countdown,
// then straight into the level.

#include <Geode/Geode.hpp>
#include "Avatar.hpp"
#include "Theme.hpp"

namespace nt::ui {
    using namespace cocos2d;

    class VsLayer : public CCLayer {
    public:
        static CCScene* scene();
        static VsLayer* create();

    protected:
        bool init() override;
        void update(float dt) override;
        void keyBackClicked() override;
        void intro();
        void showCount(std::string const& text, ccColor3B color, float scale);
        void sendEmote(int id);
        void finish();

        Avatar* m_left = nullptr;
        Avatar* m_right = nullptr;
        CCLabelBMFont* m_status = nullptr;
        CCLabelBMFont* m_small = nullptr;
        CCNode* m_countRoot = nullptr;
        int m_lastCount = 999;
        bool m_launched = false;
        bool m_done = false;
        float m_t = 0.f;
        float m_emoteCooldown = 0.f;
        float m_retryTimer = 0.f;
    };
}
