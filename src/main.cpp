// Nater's Tournament: live 1v1 Geometry Dash tournaments.
// Entry point: the mod's own crossed-swords badge on the main menu opens the tournament hub.
// It gets a menu of its own in the empty space left of the big main buttons (most mods add
// theirs to the bottom row, so that's where they would pile up on top of each other).

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "core/Match.hpp"
#include "ui/Fx.hpp"
#include "ui/HubLayer.hpp"
#include "ui/Theme.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    // start the match controller early so a match that's already running can be found again
    nt::MatchController::get();
}

namespace {
    constexpr float kBadgeW = 64.f, kBadgeH = 70.f;

    CCRect worldBox(CCNode* node) {
        auto* parent = node->getParent();
        auto box = node->boundingBox();
        if (!parent) return box;
        auto bl = parent->convertToWorldSpace({box.getMinX(), box.getMinY()});
        auto tr = parent->convertToWorldSpace({box.getMaxX(), box.getMaxY()});
        return {bl.x, bl.y, tr.x - bl.x, tr.y - bl.y};
    }

    // does anything another menu shows sit inside `rect`?
    bool crowded(CCNode* layer, CCNode* self, CCRect rect) {
        for (auto* child : CCArrayExt<CCNode*>(layer->getChildren())) {
            auto* menu = typeinfo_cast<CCMenu*>(child);
            if (!menu || menu == self || !menu->isVisible()) continue;
            for (auto* item : CCArrayExt<CCNode*>(menu->getChildren())) {
                if (item->isVisible() && worldBox(item).intersectsRect(rect)) return true;
            }
        }
        return false;
    }

    // halfway between the screen's left edge and the main buttons, at their height;
    // nudged up or down if some other mod already put something there
    CCPoint spotFor(CCNode* layer, CCNode* self) {
        auto win = CCDirector::get()->getWinSize();
        CCPoint spot {win.width * .12f, win.height * .5f};
        if (auto* main = layer->getChildByID("main-menu"); main && main->getChildrenCount()) {
            float minX = win.width, minY = win.height, maxY = 0.f;
            for (auto* child : CCArrayExt<CCNode*>(main->getChildren())) {
                if (!child->isVisible()) continue;
                auto box = worldBox(child);
                minX = std::min(minX, box.getMinX());
                minY = std::min(minY, box.getMinY());
                maxY = std::max(maxY, box.getMaxY());
            }
            if (minX >= kBadgeW + 8.f && maxY > minY) spot = CCPoint(minX / 2.f, (minY + maxY) / 2.f);
        }
        for (float dy : {0.f, 56.f, -56.f, 112.f}) {
            CCPoint p {spot.x, spot.y + dy};
            CCRect rect {p.x - kBadgeW / 2.f, p.y - kBadgeH / 2.f, kBadgeW, kBadgeH};
            if (!crowded(layer, self, rect)) return p;
        }
        return spot;
    }

    CCNode* badgeNode() {
        auto* root = CCNode::create();
        root->setContentSize({kBadgeW, kBadgeH});

        auto* glow = nt::ui::fx("glow");
        glow->setColor(nt::ui::col::gold);
        glow->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        glow->setScale(1.1f);
        glow->setOpacity(70);
        glow->setPosition({kBadgeW / 2.f, 42.f});
        glow->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCFadeTo::create(1.4f, 40)),
            CCEaseSineInOut::create(CCFadeTo::create(1.4f, 95)), nullptr)));
        root->addChild(glow);

        auto* coin = nt::ui::fx("badge");
        float w = coin->getContentSize().width;
        coin->setScale(w > 0.f ? 48.f / w : 1.f);
        coin->setPosition({kBadgeW / 2.f, 42.f});
        coin->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCMoveBy::create(1.5f, {0.f, 2.f})),
            CCEaseSineInOut::create(CCMoveBy::create(1.5f, {0.f, -2.f})), nullptr)));
        root->addChild(coin, 1);

        // every few seconds a glint runs over the rim
        auto* glint = nt::ui::fx("star");
        glint->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        glint->setColor(nt::ui::col::goldLight);
        glint->setScale(0.f);
        glint->setPosition({kBadgeW / 2.f + 15.f, 57.f});
        glint->runAction(CCRepeatForever::create(CCSequence::create(
            CCDelayTime::create(3.6f),
            CCSpawn::create(CCEaseOut::create(CCScaleTo::create(.18f, .55f), 2.f), CCRotateBy::create(.5f, 90.f), nullptr),
            CCEaseIn::create(CCScaleTo::create(.3f, 0.f), 2.f), nullptr)));
        root->addChild(glint, 2);

        auto* text = nt::ui::label("TOURNAMENT", "goldFont.fnt", .38f, nt::ui::col::white);
        nt::ui::fit(text, kBadgeW + 6.f, .38f);
        text->setPosition({kBadgeW / 2.f, 9.f});
        root->addChild(text, 1);
        return root;
    }
}

class $modify(NTMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto* btn = CCMenuItemSpriteExtra::create(badgeNode(), this, menu_selector(NTMenuLayer::onTournament));
        btn->setID("tournament-button"_spr);
        auto* menu = CCMenu::create();
        menu->setID("tournament-menu"_spr);
        menu->setContentSize({kBadgeW, kBadgeH});
        menu->ignoreAnchorPointForPosition(false);
        menu->setAnchorPoint({.5f, .5f});
        btn->setPosition({kBadgeW / 2.f, kBadgeH / 2.f});
        menu->addChild(btn);
        this->addChild(menu, 5);
        menu->setPosition(spotFor(this, menu));

        // other mods may add their buttons after us: look again once the menu is on screen
        menu->runAction(CCSequence::create(CCDelayTime::create(0.f), CallFuncExt::create([this, menu] {
            menu->setPosition(spotFor(this, menu));
        }), nullptr));

        // a little entrance the first time the menu shows up
        menu->setScale(0.f);
        menu->runAction(CCSequence::create(CCDelayTime::create(.15f),
                                           CCEaseBackOut::create(CCScaleTo::create(.35f, 1.f)), nullptr));
        return true;
    }

    void onTournament(CCObject*) {
        nt::fx::play("pop");
        CCDirector::get()->pushScene(CCTransitionFade::create(.45f, nt::ui::HubLayer::scene()));
    }
};
