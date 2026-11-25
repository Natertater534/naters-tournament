#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

using namespace geode::prelude;

class $modify(NTMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto* spr = CCSprite::createWithSpriteFrameName("GJ_versusBtn_001.png");
        auto* btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(NTMenuLayer::onTournament));
        btn->setID("tournament-button"_spr);
        if (auto* menu = this->getChildByID("bottom-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        }
        return true;
    }

    void onTournament(CCObject*) {
        FLAlertLayer::create("Nater's Tournament", "Build check.", "OK")->show();
    }
};
