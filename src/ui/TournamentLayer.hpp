#pragma once

// The tournament screen: lobby / bracket stage, top bar with the join code,
// host controls, the live feed. This is the screen Nater shows on stream.

#include <Geode/Geode.hpp>
#include "../core/Api.hpp"
#include "../core/Levels.hpp"
#include "../core/Model.hpp"
#include "Arena.hpp"
#include "LevelReveal.hpp"
#include "Theme.hpp"

namespace nt::ui {
    using namespace cocos2d;

    class TournamentLayer : public CCLayer {
    public:
        // justJoined: play our own drop-in when the screen first loads
        static CCScene* scene(std::string const& code, bool justJoined = false);
        static TournamentLayer* create(std::string const& code, bool justJoined = false);

    protected:
        bool init(std::string const& code, bool justJoined);
        void onEnterTransitionDidFinish() override;
        void onExit() override;
        void keyBackClicked() override;
        void keyDown(enumKeyCodes key, double timestamp) override;
        void keyUp(enumKeyCodes key, double timestamp) override;
        void registerWithTouchDispatcher() override;
        bool ccTouchBegan(CCTouch* touch, CCEvent* event) override;
        void ccTouchEnded(CCTouch* touch, CCEvent* event) override;

        void poll(float);
        void tickMovement(float dt);
        void releaseKeys();
        void onState(matjson::Value const& v, int64_t sent, bool fromAction);
        void handleEvents(std::vector<Event> const& events);
        void refreshChrome(bool force = false);
        void maybeEnterMatch();
        void feed(std::string const& text, ccColor3B color);
        void setLevelCard();
        void showMoveHint();
        std::string nameOf(int account) const;

        // the random level: the host rolls it, everyone watches it land
        void rollLevel();
        void showReveal(LevelRef const& level, int round);
        void revealDone();
        std::string revealTitle(int round) const;

        void host(std::string const& action, matjson::Value extra = matjson::makeObject({}),
                  std::function<void(bool)> done = nullptr);
        void onJoin();
        void onLeave();
        void onEmote();
        void onBack();

        std::string m_code;
        State m_state;
        bool m_loaded = false;
        bool m_justJoined = false;
        bool m_busy = false;
        int m_me = 0;
        float m_pollTimer = 0.f;
        api::Holder m_pollReq;
        api::Holder m_actionReq;
        api::Holder m_levelReq;
        api::Holder m_moveReq;
        bool m_polling = false;
        int m_failures = 0;
        int64_t m_lastLocalEmote = 0;
        CCPoint m_touchStart;

        // lobby movement
        bool m_left = false, m_right = false, m_jumpHeld = false;
        float m_moveTimer = 0.f;
        bool m_moveSending = false;
        CCNode* m_moveHint = nullptr;
        bool m_movedOnce = false;

        // level roll
        LevelRoller* m_roller = nullptr;
        geode::WeakRef<LevelReveal> m_reveal;
        bool m_rolling = false;
        bool m_revealing = false;
        int64_t m_rolledId = 0;    // what our own roll landed on (its event is ours)
        int64_t m_revealedId = 0;  // the last level shown in a reveal

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
