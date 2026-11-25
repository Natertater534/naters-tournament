// Nater's Tournament: live 1v1 Geometry Dash tournaments.
// Entry point: a crossed-swords button on the main menu opens the tournament hub.

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "core/Match.hpp"
#include "ui/HubLayer.hpp"
#include "ui/Fx.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    // start the match controller early so a match that's already running can be found again
    nt::MatchController::get();
}

class $modify(NTMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto* icon = CCSprite::createWithSpriteFrameName("GJ_versusBtn_001.png");
        if (!icon) icon = nt::ui::fx("swords");
        float h = std::max(icon->getContentSize().width, icon->getContentSize().height);
        icon->setScale(h > 0.f ? 30.f / h : 1.f);
        auto* spr = CircleButtonSprite::create(icon, CircleBaseColor::Green, CircleBaseSize::MediumAlt);
        auto* btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(NTMenuLayer::onTournament));
        btn->setID("tournament-button"_spr);

        if (auto* menu = this->getChildByID("bottom-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        } else {
            auto* own = CCMenu::create();
            auto win = CCDirector::get()->getWinSize();
            own->setPosition({win.width - 40.f, 40.f});
            own->addChild(btn);
            this->addChild(own);
        }
        return true;
    }

    void onTournament(CCObject*) {
        nt::fx::play("pop");
        CCDirector::get()->pushScene(CCTransitionFade::create(.45f, nt::ui::HubLayer::scene()));
    }
};
