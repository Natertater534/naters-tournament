#include "VsLayer.hpp"
#include "Fx.hpp"
#include "TournamentLayer.hpp"
#include "../core/Match.hpp"
#include "../core/Session.hpp"

using namespace geode::prelude;

namespace nt::ui {
    CCScene* VsLayer::scene() {
        auto* scene = CCScene::create();
        scene->addChild(VsLayer::create());
        return scene;
    }

    VsLayer* VsLayer::create() {
        auto* ret = new VsLayer();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool VsLayer::init() {
        if (!CCLayer::init()) return false;
        auto win = CCDirector::get()->getWinSize();
        auto* mc = MatchController::get();
        auto const& info = mc->info();

        // split-screen: blue for you, red for them, a bright seam between
        auto* halves = CCDrawNode::create();
        float tilt = 40.f;
        CCPoint left[4] = {{0.f, 0.f}, {win.width / 2.f - tilt, 0.f}, {win.width / 2.f + tilt, win.height}, {0.f, win.height}};
        CCPoint right[4] = {{win.width / 2.f - tilt, 0.f}, {win.width, 0.f}, {win.width, win.height}, {win.width / 2.f + tilt, win.height}};
        halves->drawPolygon(left, 4, c4f({22, 60, 150}), 0.f, c4f(col::white, 0.f));
        halves->drawPolygon(right, 4, c4f({150, 28, 48}), 0.f, c4f(col::white, 0.f));
        this->addChild(halves, -10);
        auto* shadeL = CCLayerGradient::create(ccc4(0, 0, 0, 0), ccc4(0, 0, 20, 170));
        shadeL->setContentSize(win);
        this->addChild(shadeL, -9);
        auto* seam = CCDrawNode::create();
        seam->drawSegment({win.width / 2.f - tilt, 0.f}, {win.width / 2.f + tilt, win.height}, 2.5f, c4f(col::white, .9f));
        this->addChild(seam, -8);
        auto* seamGlow = fx("glow");
        seamGlow->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        seamGlow->setScaleX(.5f);
        seamGlow->setScaleY(win.height / seamGlow->getContentSize().height * 1.4f);
        seamGlow->setRotation(std::atan2(2.f * tilt, win.height) * 57.2958f);
        seamGlow->setPosition(win / 2.f);
        seamGlow->setOpacity(120);
        this->addChild(seamGlow, -8);

        // speed lines drifting across each half
        for (int i = 0; i < 14; ++i) {
            auto* line = fx("spark");
            line->setScaleX(fx::rnd(2.f, 4.f));
            line->setScaleY(.35f);
            line->setOpacity(static_cast<GLubyte>(fx::rnd(25.f, 70.f)));
            bool l = i % 2 == 0;
            float y = fx::rnd(20.f, win.height - 20.f);
            line->setPosition({l ? -60.f : win.width + 60.f, y});
            line->setFlipX(!l);
            float t = fx::rnd(.9f, 1.6f);
            line->runAction(CCRepeatForever::create(CCSequence::create(
                CCDelayTime::create(fx::rnd(0.f, 1.f)),
                CCMoveTo::create(t, {l ? win.width / 2.f : win.width / 2.f, y}),
                CCPlace::create({l ? -60.f : win.width + 60.f, y}), nullptr)));
            this->addChild(line, -7);
        }

        auto* top = label(info.tournamentName, "goldFont.fnt", .6f, col::white);
        fit(top, win.width - 80.f, .6f);
        top->setPosition({win.width / 2.f, win.height - 18.f});
        this->addChild(top, 5);
        auto* round = label(roundName(info.round, info.rounds), "bigFont.fnt", .45f, col::goldLight);
        round->setPosition({win.width / 2.f, win.height - 40.f});
        this->addChild(round, 5);

        m_left = Avatar::create(info.me, 2.f, false);
        m_left->setPosition({-80.f, win.height * .42f});
        this->addChild(m_left, 3);
        m_right = Avatar::create(info.opp, 2.f, false);
        m_right->setPosition({win.width + 80.f, win.height * .42f});
        this->addChild(m_right, 3);

        auto nameplate = [&](Player const& p, float x, ccColor3B accent) {
            auto* n = label(p.name, "goldFont.fnt", .75f, col::white);
            fit(n, win.width * .36f, .75f);
            n->setPosition({x, win.height * .42f - 22.f});
            this->addChild(n, 4);
            auto* st = label(fmt::format("{} wins  -  best {}%", p.wins, static_cast<int>(p.best)), "bigFont.fnt", .3f, accent);
            st->setPosition({x, win.height * .42f - 42.f});
            this->addChild(st, 4);
            n->setOpacity(0);
            st->setOpacity(0);
            n->runAction(CCSequence::create(CCDelayTime::create(.8f), CCFadeIn::create(.3f), nullptr));
            st->runAction(CCSequence::create(CCDelayTime::create(.9f), CCFadeIn::create(.3f), nullptr));
        };
        nameplate(info.me, win.width * .24f, {160, 210, 255});
        nameplate(info.opp, win.width * .76f, {255, 170, 170});

        // the level card
        auto* card = CCNode::create();
        auto* plate = Plate::create({250.f, 44.f}, col::navyDeep, .9f, col::gold, .8f, 9.f, 1.4f);
        plate->setAnchorPoint({.5f, .5f});
        card->addChild(plate);
        if (auto* face = difficulty(info.level.face)) {
            face->setScale(.7f);
            face->setPosition({-100.f, 0.f});
            card->addChild(face);
        }
        auto* lname = label(info.level.name.empty() ? "Level " + std::to_string(info.level.id) : info.level.name,
                            "bigFont.fnt", .45f, col::white);
        fit(lname, 180.f, .45f);
        lname->setAnchorPoint({0.f, .5f});
        lname->setPosition({-78.f, 7.f});
        card->addChild(lname);
        m_status = label("Preparing the level...", "goldFont.fnt", .38f, col::gold);
        m_status->setAnchorPoint({0.f, .5f});
        m_status->setPosition({-78.f, -11.f});
        card->addChild(m_status);
        card->setPosition({win.width / 2.f, -40.f});
        this->addChild(card, 6);
        card->runAction(CCSequence::create(CCDelayTime::create(1.2f),
                                           CCEaseBackOut::create(CCMoveTo::create(.4f, {win.width / 2.f, 40.f})), nullptr));

        m_small = label("", "bigFont.fnt", .35f, col::white);
        m_small->setPosition({win.width / 2.f, win.height * .2f + 28.f});
        this->addChild(m_small, 6);

        m_countRoot = CCNode::create();
        m_countRoot->setPosition({win.width / 2.f, win.height * .7f});
        this->addChild(m_countRoot, 20);

        // quick taunts
        auto* menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        this->addChild(menu, 10);
        int quick[] = {1, 2, 13, 9, 14};
        for (int i = 0; i < 5; ++i) {
            int id = quick[i];
            auto* spr = emote(id);
            spr->setScale(.42f);
            auto* btn = nodeButton(spr, [this, id](auto) { this->sendEmote(id); });
            btn->setPosition({28.f + i * 34.f, win.height * .2f - 28.f});
            menu->addChild(btn);
        }
        auto* back = frameButton("GJ_arrow_01_001.png", .55f, [this](auto) { this->keyBackClicked(); });
        back->setPosition({26.f, win.height - 22.f});
        menu->addChild(back);

        this->setKeypadEnabled(true);
        this->scheduleUpdate();
        this->intro();
        FMODAudioEngine::sharedEngine()->stopAllMusic(true);
        return true;
    }

    void VsLayer::intro() {
        auto win = CCDirector::get()->getWinSize();
        auto land = [this](Avatar* a, int facing) {
            a->squash(1.f);
            fx::dust(this, a->getPosition(), 1.4f, 2);
            fx::play("land");
            // each draws a sword, facing the other
            a->swordDraw(facing, .1f);
        };
        m_left->runAction(CCSequence::create(
            CCDelayTime::create(.25f),
            CCEaseOut::create(CCJumpTo::create(.5f, {win.width * .24f, win.height * .42f}, 60.f, 1), 1.3f),
            CallFuncExt::create([this, land] { land(m_left, 1); }), nullptr));
        m_right->runAction(CCSequence::create(
            CCDelayTime::create(.4f),
            CCEaseOut::create(CCJumpTo::create(.5f, {win.width * .76f, win.height * .42f}, 60.f, 1), 1.3f),
            CallFuncExt::create([this, land] { land(m_right, -1); }), nullptr));
        fx::play("whoosh");

        // VS slam
        this->runAction(CCSequence::create(
            CCDelayTime::create(1.0f),
            CallFuncExt::create([this, win] {
                auto centre = ccp(win.width / 2.f, win.height * .5f);
                auto* swords = fx("swords");
                float h = std::max(swords->getContentSize().width, swords->getContentSize().height);
                float target = h > 0.f ? 92.f / h : 1.f;
                swords->setPosition(centre);
                swords->setScale(target * 3.f);
                swords->setOpacity(0);
                swords->setRotation(-30.f);
                this->addChild(swords, 8);
                swords->runAction(CCSpawn::create(CCEaseIn::create(CCScaleTo::create(.18f, target), 2.f),
                                                  CCFadeIn::create(.12f),
                                                  CCEaseBackOut::create(CCRotateTo::create(.35f, 0.f)), nullptr));
                swords->runAction(CCSequence::create(CCDelayTime::create(.4f), CCRepeatForever::create(CCSequence::create(
                    CCEaseSineInOut::create(CCScaleTo::create(.8f, target * 1.06f)),
                    CCEaseSineInOut::create(CCScaleTo::create(.8f, target)), nullptr)), nullptr));
                auto* vs = label("VS", "goldFont.fnt", 4.f, col::white);
                vs->setPosition(centre + ccp(0.f, -4.f));
                vs->setOpacity(0);
                this->addChild(vs, 9);
                vs->runAction(CCSpawn::create(CCEaseIn::create(CCScaleTo::create(.18f, 1.5f), 2.f), CCFadeIn::create(.1f), nullptr));
            }),
            CCDelayTime::create(.18f),
            CallFuncExt::create([this, win] {
                fx::play("hit");
                fx::play("clang", .7f);
                fx::screenFlash(this, col::white, .7f, .35f);
                fx::shake(this, 7.f, .35f);
                fx::sparks(this, {win.width / 2.f, win.height * .5f}, 16, col::goldLight, 50.f, 12);
                fx::ring(this, {win.width / 2.f, win.height * .5f}, col::gold, 2.2f, .5f, 12);
                // both point their blades across at each other: a challenge
                for (Avatar* a : {m_left, m_right}) {
                    a->runAction(CCSequence::create(
                        CallFuncExt::create([a] { a->swordTo(-40.f, .12f); }),
                        CCDelayTime::create(.12f),
                        CallFuncExt::create([a] { a->swordTo(78.f, .16f); }),
                        CCDelayTime::create(1.1f),
                        CallFuncExt::create([a] { a->swordTo(Avatar::kSwordRest, .4f); }), nullptr));
                }
            }),
            nullptr));
    }

    void VsLayer::showCount(std::string const& text, ccColor3B color, float scale) {
        m_countRoot->removeAllChildren();
        auto* l = label(text, "goldFont.fnt", scale * 2.2f, color);
        l->setOpacity(0);
        m_countRoot->addChild(l);
        l->runAction(CCSpawn::create(CCEaseBackOut::create(CCScaleTo::create(.25f, scale)), CCFadeIn::create(.1f), nullptr));
        l->runAction(CCSequence::create(CCDelayTime::create(.7f), CCFadeOut::create(.25f), nullptr));
    }

    void VsLayer::sendEmote(int id) {
        if (m_emoteCooldown > 0.f) return;
        m_emoteCooldown = 1.3f;
        m_left->emote(id, 1.6f);
        MatchController::get()->sendEmote(id);
    }

    void VsLayer::keyBackClicked() {
        if (m_launched) return;
        m_launched = true;
        CCDirector::get()->replaceScene(
            CCTransitionFade::create(.4f, TournamentLayer::scene(MatchController::get()->info().code)));
        GameManager::sharedState()->fadeInMenuMusic();
    }

    void VsLayer::finish() {
        // the match ended before we even got in (a no-show or the host decided it)
        if (m_done) return;
        m_done = true;
        auto* mc = MatchController::get();
        bool won = mc->iWon();
        this->showCount(won ? "YOU WIN" : "MATCH OVER", won ? col::gold : col::red, .9f);
        m_status->setString(won ? "Your opponent didn't make it" : "Back to the bracket...");
        this->runAction(CCSequence::create(CCDelayTime::create(2.2f), CallFuncExt::create([this] { this->keyBackClicked(); }), nullptr));
    }

    void VsLayer::update(float dt) {
        m_t += dt;
        m_emoteCooldown = std::max(0.f, m_emoteCooldown - dt);
        auto* mc = MatchController::get();
        if (!mc->active() || m_launched) return;
        if (mc->done()) {
            this->finish();
            return;
        }

        for (int id : mc->takeOpponentEmotes()) m_right->emote(id, 1.6f);

        using Prep = MatchController::Prep;
        switch (mc->prep()) {
            case Prep::Ready: m_status->setString("Ready!"); m_status->setColor(col::green); break;
            case Prep::Failed: m_status->setString(mc->prepError().c_str()); m_status->setColor(col::red); break;
            case Prep::Songs: m_status->setString(fmt::format("Getting the music... {}%", mc->prepPercent()).c_str()); break;
            default: m_status->setString("Downloading the level..."); break;
        }
        if (mc->prep() == Prep::Failed) {
            m_retryTimer += dt;
            if (m_retryTimer > 5.f) {
                m_retryTimer = 0.f;
                mc->retryPrep();
            }
        }

        if (m_t < 1.6f) return;  // let the intro land first
        int left = mc->secondsToStart();
        if (left > 3) {
            m_small->setString(fmt::format("Starting in {}", left).c_str());
        } else {
            m_small->setString("");
        }
        if (left <= 3 && left >= 1 && left != m_lastCount) {
            m_lastCount = left;
            this->showCount(std::to_string(left), col::white, 1.f);
            fx::play("tick");
        }
        if (left <= 0) {
            if (m_lastCount != 0) {
                m_lastCount = 0;
                this->showCount("GO!", col::gold, 1.1f);
                fx::play("go");
            }
            if (mc->prep() == Prep::Ready && m_t > 1.8f) {
                m_launched = true;
                if (!mc->launch()) m_launched = false;
            } else if (mc->prep() != Prep::Ready) {
                m_small->setString("Waiting for the download - your clock is running!");
            }
        }
    }
}
