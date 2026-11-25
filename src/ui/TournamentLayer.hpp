#pragma once

// The tournament screen: lobby / bracket stage, top bar with the join code,
// host controls, the live feed. This is the screen Nater shows on stream.

#include <Geode/Geode.hpp>
#include "../core/Api.hpp"
#include "../core/Model.hpp"
#include "Arena.hpp"
#include "Theme.hpp"

namespace nt::ui {
    using namespace cocos2d;

    class TournamentLayer : public CCLayer {
    public:
        static CCScene* scene(std::string const& code);
        static TournamentLayer* create(std::string const& code);

    protected:
        bool init(std::string const& code);
        void onEnterTransitionDidFinish() override;
        void keyBackClicked() override;
        void registerWithTouchDispatcher() override;
        bool ccTouchBegan(CCTouch* touch, CCEvent* event) override;
        void ccTouchEnded(CCTouch* touch, CCEvent* event) override;

        void poll(float);
        void onState(matjson::Value const& v, int64_t sent, bool fromAction);
        void handleEvents(std::vector<Event> const& events);
        void refreshChrome();
        void maybeEnterMatch();
        void feed(std::string const& text, ccColor3B color);
        void setLevelCard();
        std::string nameOf(int account) const;

        void host(std::string const& action, matjson::Value extra = matjson::makeObject({}));
        void onJoin();
        void onLeave();
        void onEmote();
        void onBack();

        std::string m_code;
        State m_state;
        bool m_loaded = false;
        bool m_busy = false;
        int m_me = 0;
        float m_pollTimer = 0.f;
        api::Holder m_pollReq;
        api::Holder m_actionReq;
        bool m_polling = false;
        int m_failures = 0;
        int64_t m_lastLocalEmote = 0;
        CCPoint m_touchStart;

        Backdrop* m_backdrop = nullptr;
        Arena* m_arena = nullptr;
        CCLabelBMFont* m_title = nullptr;
        CCLabelBMFont* m_sub = nullptr;
        CCLabelBMFont* m_codeLabel = nullptr;
        CCNode* m_codeBadge = nullptr;
        CCNode* m_levelCard = nullptr;
        CCNode* m_feed = nullptr;
        CCMenu* m_menu = nullptr;
        CCMenu* m_bottom = nullptr;
        CCNode* m_loading = nullptr;
        std::string m_chromeKey;
        int64_t m_levelShown = -1;
    };
}
