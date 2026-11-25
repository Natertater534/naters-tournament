// Hooks that tie a tournament match to GD's own level: progress, completion,
// the race HUD, emotes in the pause menu, and quitting back to the bracket.

#include <Geode/Geode.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "../core/Compat.hpp"
#include "../core/Match.hpp"
#include "../ui/Theme.hpp"
#include "MatchHud.hpp"

using namespace geode::prelude;

namespace {
    bool runCounts(PlayLayer* pl) {
        if (pl->m_isPracticeMode || pl->m_isTestMode) return false;
    #if NT_GD_2209
        if (pl->m_startPosObject) return false;
    #endif
        return true;
    }

    bool s_quittingMatch = false;
}

class $modify(NTPlayLayer, PlayLayer) {
    struct Fields {
        bool checked = false;
        bool mine = false;
        nt::play::MatchHud* hud = nullptr;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto* mc = nt::MatchController::get();
        if (!m_fields->checked) {
            m_fields->checked = true;
            m_fields->mine = mc->owns(this);
            if (m_fields->mine) {
                m_fields->hud = nt::play::MatchHud::create();
                this->addChild(m_fields->hud, 1000);
            }
        }
        if (!m_fields->mine) return;
        mc->onFrame(this->getCurrentPercent(), m_attempts, runCounts(this));
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        auto* mc = nt::MatchController::get();
        if (mc->owns(this) && runCounts(this)) mc->onComplete();
    }

    void onQuit() {
        auto* mc = nt::MatchController::get();
        s_quittingMatch = mc->owns(this);
        PlayLayer::onQuit();
        s_quittingMatch = false;
    }
};

class $modify(NTGameManager, GameManager) {
    void returnToLastScene(GJGameLevel* level) {
        auto* mc = nt::MatchController::get();
        if (s_quittingMatch && mc->wantsReturn()) {
            mc->returnToTournament();
            return;
        }
        GameManager::returnToLastScene(level);
    }
};

class $modify(NTPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto* pl = PlayLayer::get();
        auto* mc = nt::MatchController::get();
        if (!pl || !mc->owns(pl)) return;
        auto win = CCDirector::get()->getWinSize();
        auto* menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        this->addChild(menu, 20);
        int quick[] = {1, 2, 4, 7, 9, 10, 13, 14};
        int n = 8;
        float spacing = 32.f;
        for (int i = 0; i < n; ++i) {
            int id = quick[i];
            auto* spr = nt::ui::emote(id);
            spr->setScale(.4f);
            auto* btn = nt::ui::nodeButton(spr, [id](auto* sender) {
                nt::MatchController::get()->sendEmote(id);
                sender->setEnabled(false);
                sender->runAction(CCSequence::create(CCDelayTime::create(1.3f), CallFuncExt::create([sender] {
                    sender->setEnabled(true);
                }), nullptr));
                Notification::create(std::string("Sent ") + nt::ui::emoteName(id), NotificationIcon::None, .8f)->show();
            });
            btn->setPosition({win.width / 2.f + (i - (n - 1) / 2.f) * spacing, 22.f});
            menu->addChild(btn);
        }
        auto* hint = nt::ui::label("Send an emote to your opponent", "bigFont.fnt", .25f, nt::ui::col::dim);
        hint->setPosition({win.width / 2.f, 42.f});
        this->addChild(hint, 20);
    }
};
