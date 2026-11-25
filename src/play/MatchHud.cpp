#include "MatchHud.hpp"
#include "../core/Compat.hpp"
#include "../core/Match.hpp"
#include "../ui/Fx.hpp"
#include "../ui/Theme.hpp"

using namespace geode::prelude;
using namespace nt::ui;

namespace nt::play {
    MatchHud* MatchHud::create() {
        auto* ret = new MatchHud();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    static SimplePlayer* iconOf(Player const& p, float scale) {
        auto* icon = SimplePlayer::create(1);
        icon->updatePlayerFrame(std::max(1, p.icon.cube), IconType::Cube);
        icon->setColor(iconColor(p.icon.p));
        icon->setSecondColor(iconColor(p.icon.s));
        if (p.icon.g >= 0) icon->setGlowOutline(iconColor(p.icon.g));
        else icon->disableGlowOutline();
        icon->setScale(scale);
        return icon;
    }

    bool MatchHud::init() {
        if (!CCNode::init()) return false;
        auto win = CCDirector::get()->getWinSize();
        auto* mc = MatchController::get();
        auto const& info = mc->info();

        m_root = CCNode::create();
        m_root->setPosition({win.width / 2.f, win.height - 30.f});
        this->addChild(m_root);

        auto* track = Plate::create({m_trackW + 10.f, 10.f}, col::navyDeep, .72f, col::gold, .7f, 5.f, 1.2f);
        track->setAnchorPoint({.5f, .5f});
        m_root->addChild(track);
        for (int q = 1; q < 4; ++q) {
            auto* tick = CCLayerColor::create(ccc4(255, 255, 255, 60), 1.f, 6.f);
            tick->setPosition({this->xFor(q * 25.f) - .5f, -3.f});
            m_root->addChild(tick);
        }

        m_myFlag = CCLayerColor::create(ccc4(110, 255, 140, 230), 2.f, 14.f);
        m_myFlag->setPosition({this->xFor(0.f) - 1.f, -7.f});
        m_root->addChild(m_myFlag, 2);
        m_oppFlag = CCLayerColor::create(ccc4(255, 92, 84, 240), 2.f, 16.f);
        m_oppFlag->setPosition({this->xFor(0.f) - 1.f, -8.f});
        m_root->addChild(m_oppFlag, 3);
        m_oppFlagLabel = label("0%", "bigFont.fnt", .22f, {255, 150, 140});
        m_oppFlagLabel->setPosition({this->xFor(0.f), -14.f});
        m_root->addChild(m_oppFlagLabel, 3);

        m_oppGlow = ui::fx("glow");
        m_oppGlow->setColor(col::red);
        m_oppGlow->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        m_oppGlow->setScale(.4f);
        m_oppGlow->setOpacity(150);
        m_root->addChild(m_oppGlow, 4);
        m_opp = iconOf(info.opp, .42f);
        m_opp->setPosition({this->xFor(0.f), 11.f});
        m_root->addChild(m_opp, 5);
        m_mine = iconOf(info.me, .3f);
        m_mine->setOpacity(200);
        m_mine->setPosition({this->xFor(0.f), -11.f});
        m_root->addChild(m_mine, 5);

        m_oppName = label(info.opp.name, "goldFont.fnt", .42f, {255, 170, 160});
        fit(m_oppName, 110.f, .42f);
        m_oppName->setAnchorPoint({1.f, .5f});
        m_oppName->setPosition({-m_trackW / 2.f - 14.f, 4.f});
        m_root->addChild(m_oppName);
        m_oppInfo = label("best 0%", "bigFont.fnt", .25f, col::white);
        m_oppInfo->setAnchorPoint({1.f, .5f});
        m_oppInfo->setPosition({-m_trackW / 2.f - 14.f, -9.f});
        m_root->addChild(m_oppInfo);

        m_clock = label("0:00", "bigFont.fnt", .45f, col::white);
        m_clock->setAnchorPoint({0.f, .5f});
        m_clock->setPosition({m_trackW / 2.f + 14.f, 0.f});
        m_root->addChild(m_clock);

        m_note = label("", "bigFont.fnt", .28f, col::red);
        m_note->setPosition({0.f, -26.f});
        m_root->addChild(m_note);

        m_root->setScale(0.f);
        m_root->runAction(CCEaseBackOut::create(CCScaleTo::create(.35f, 1.f)));
        this->scheduleUpdate();
        return true;
    }

    float MatchHud::xFor(float percent) const {
        return -m_trackW / 2.f + m_trackW * std::clamp(percent, 0.f, 100.f) / 100.f;
    }

    void MatchHud::update(float dt) {
        auto* mc = MatchController::get();
        if (!mc->active()) return;
        float k = std::min(1.f, dt * 9.f);
        m_oppShown += (mc->oppCur - m_oppShown) * k;
        m_myShown += (mc->myCur - m_myShown) * k;
        m_opp->setPositionX(this->xFor(m_oppShown));
        m_oppGlow->setPosition(m_opp->getPosition());
        m_mine->setPositionX(this->xFor(m_myShown));
        m_myFlag->setPositionX(this->xFor(mc->myBest) - 1.f);
        m_oppFlag->setPositionX(this->xFor(mc->oppBest) - 1.f);
        m_oppFlagLabel->setPositionX(this->xFor(mc->oppBest));
        m_oppFlagLabel->setString(fmt::format("{}%", static_cast<int>(mc->oppBest)).c_str());
        m_oppInfo->setString(fmt::format("best {}%  -  att {}", static_cast<int>(mc->oppBest), mc->oppAtt).c_str());

        if (mc->oppBest > m_lastOppBest + .5f) {
            if (m_lastOppBest > 0.f || mc->oppBest > 5.f) {
                fx::floatText(m_root, {this->xFor(mc->oppBest), 26.f}, fmt::format("{}%!", static_cast<int>(mc->oppBest)),
                              {255, 150, 140}, .4f);
                fx::stars(m_root, {this->xFor(mc->oppBest), 10.f}, 3, col::red, 14.f, 6);
            }
            m_lastOppBest = mc->oppBest;
        }
        if (mc->oppCur + 8.f < m_lastOppCur) {
            // they died: a tiny GD-style pop where their icon was
            fx::deathBurst(m_root, {this->xFor(m_lastOppCur), 11.f}, iconColor(mc->info().opp.icon.p),
                           iconColor(mc->info().opp.icon.s), .35f, 6);
            m_oppShown = mc->oppCur;
        }
        m_lastOppCur = mc->oppCur;

        int left = mc->secondsLeft();
        m_clock->setString(left > 0 || mc->done() ? formatClock(left).c_str() : "TIME!");
        if (left <= 30 && !mc->done()) {
            m_clock->setColor(col::red);
            if (left != m_lastSecond) {
                m_clock->stopAllActions();
                m_clock->setScale(.58f);
                m_clock->runAction(CCEaseOut::create(CCScaleTo::create(.3f, .45f), 2.f));
                if (left <= 10 && left > 0) fx::play("tick");
            }
        } else {
            m_clock->setColor(col::white);
        }
        m_lastSecond = left;

        for (int id : mc->takeOpponentEmotes()) {
            if (m_bubble) m_bubble->removeFromParent();
            auto* b = CCNode::create();
            auto* bg = ui::fx("bubble");
            bg->setAnchorPoint({.5f, 0.f});
            bg->setScale(.62f);
            b->addChild(bg);
            auto* e = emote(id);
            e->setScale(.32f);
            e->setPosition({0.f, 14.5f});
            b->addChild(e, 1);
            b->setPosition({m_opp->getPositionX(), 20.f});
            m_root->addChild(b, 10);
            m_bubble = b;
            fx::popIn(b, 1.f, 0.f, .4f);
            b->runAction(CCSequence::create(CCDelayTime::create(2.f), CCEaseBackIn::create(CCScaleTo::create(.2f, 0.f)),
                                            CCRemoveSelf::create(), nullptr));
            fx::play("boop");
        }
        if (m_bubble && m_bubble->getParent()) m_bubble->setPositionX(m_opp->getPositionX());

        if (auto* pl = PlayLayer::get()) {
            bool practice = pl->m_isPracticeMode;
            bool start = pl->m_isTestMode;
        #if NT_GD_2209
            start = start || pl->m_startPosObject != nullptr;
        #endif
            m_note->setString(practice ? "PRACTICE - this run doesn't count" : start ? "START POS - this run doesn't count" : "");
        }

        if (mc->done() && !m_resultShown) this->showResult();
        if (m_resultShown) {
            m_resultTimer += dt;
            if (m_resultTimer > 6.5f) {
                m_resultTimer = -999.f;
                if (auto* pl = PlayLayer::get(); pl && mc->owns(pl)) pl->onQuit();
            }
        }
    }

    void MatchHud::showResult() {
        m_resultShown = true;
        auto win = CCDirector::get()->getWinSize();
        auto* mc = MatchController::get();
        bool won = mc->iWon();
        auto const& why = mc->why();

        auto* dim = CCLayerColor::create(ccc4(0, 0, 0, 0), win.width, win.height);
        dim->runAction(CCFadeTo::create(.4f, 130));
        this->addChild(dim, 50);

        auto* root = CCNode::create();
        root->setPosition({win.width / 2.f, win.height / 2.f + 20.f});
        this->addChild(root, 60);
        auto* title = label(won ? "VICTORY!" : "DEFEAT", "goldFont.fnt", 0.f, won ? col::gold : col::red);
        root->addChild(title);
        title->runAction(CCEaseElasticOut::create(CCScaleTo::create(.7f, 1.6f), .5f));

        std::string sub;
        if (why == "complete") sub = won ? "You finished the level first!" : mc->info().opp.name + " finished the level first";
        else if (why == "time") sub = fmt::format("Time's up: {}% vs {}%", static_cast<int>(mc->myBest), static_cast<int>(mc->oppBest));
        else if (why == "forfeit") sub = won ? "Your opponent forfeited" : "You forfeited";
        else if (why == "host") sub = "The host decided this match";
        auto* s = label(sub, "bigFont.fnt", .45f, col::white);
        fit(s, win.width - 80.f, .45f);
        s->setPosition({0.f, -34.f});
        s->setOpacity(0);
        s->runAction(CCSequence::create(CCDelayTime::create(.3f), CCFadeIn::create(.3f), nullptr));
        root->addChild(s);

        auto* menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        root->addChild(menu);
        auto* btn = textButton("Back to the bracket", "GJ_button_01.png", .7f, [](auto) {
            if (auto* pl = PlayLayer::get()) pl->onQuit();
        });
        btn->setPosition({0.f, -74.f});
        menu->addChild(btn);

        fx::screenFlash(this, won ? col::gold : col::red, .45f, .5f, 55);
        if (won) {
            fx::confetti(this, {0.f, win.height, win.width, 20.f}, 90, 58);
            fx::play("win");
        } else {
            fx::play("hit");
        }
    }
}
