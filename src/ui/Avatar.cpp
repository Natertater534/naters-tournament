#include "Avatar.hpp"
#include "Fx.hpp"
#include "Theme.hpp"

using namespace geode::prelude;

namespace nt::ui {
    namespace {
        constexpr int kBobTag = 0xB0B;
        constexpr int kTiltTag = 0x7117;
        // hops leave the cube turned by a multiple of 90 degrees, like in GD: tilts are made
        // around that resting turn so the face never snaps back upright
        float restSpin(CCNode* icon) {
            return std::round(icon->getRotation() / 90.f) * 90.f;
        }
    }

    Avatar* Avatar::create(Player const& player, float scale, bool nameTag) {
        auto* ret = new Avatar();
        if (ret->init(player, scale, nameTag)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool Avatar::init(Player const& player, float scale, bool nameTag) {
        if (!CCNode::init()) return false;
        m_player = player;
        m_scale = scale;

        m_shadow = fx("shadow");
        m_shadow->setScale(scale * 1.1f);
        m_shadow->setOpacity(150);
        m_shadow->setPosition({0.f, 1.f});
        this->addChild(m_shadow, -2);

        m_body = CCNode::create();
        this->addChild(m_body, 1);
        m_bob = CCNode::create();
        m_body->addChild(m_bob);

        m_icon = SimplePlayer::create(1);
        m_icon->setScale(scale);
        m_icon->setPosition({0.f, 15.f * scale});
        m_bob->addChild(m_icon);
        this->applyIcon();

        if (nameTag) {
            m_nameScale = std::clamp(.2f + .12f * scale, .2f, .34f);
            m_name = label(player.name, "bigFont.fnt", m_nameScale, col::white);
            fit(m_name, 70.f, m_nameScale);
            m_name->setPosition({0.f, -7.f - 2.f * scale});
            this->addChild(m_name, 2);
        }
        return true;
    }

    void Avatar::applyIcon() {
        auto& ic = m_player.icon;
        m_icon->updatePlayerFrame(std::max(1, ic.cube), IconType::Cube);
        if (m_dimmed) {
            m_icon->setColor({95, 95, 110});
            m_icon->setSecondColor({60, 60, 72});
            m_icon->disableGlowOutline();
            m_icon->setOpacity(170);
            return;
        }
        m_icon->setColor(iconColor(ic.p));
        m_icon->setSecondColor(iconColor(ic.s));
        if (ic.g >= 0) m_icon->setGlowOutline(iconColor(ic.g));
        else m_icon->disableGlowOutline();
        m_icon->setOpacity(255);
    }

    void Avatar::setPlayer(Player const& player) {
        bool changed = player.icon.cube != m_player.icon.cube || player.icon.p != m_player.icon.p ||
                       player.icon.s != m_player.icon.s || player.icon.g != m_player.icon.g;
        bool renamed = player.name != m_player.name;
        m_player = player;
        if (changed) this->applyIcon();
        if (renamed && m_name) {
            m_name->setString(player.name.c_str());
            fit(m_name, 70.f, m_nameScale);
        }
    }

    CCPoint Avatar::headWorld() {
        return this->getPosition() + ccp(0.f, (30.f * m_scale + 2.f) * this->getScale());
    }

    void Avatar::setNameVisible(bool visible) {
        if (m_name) m_name->setVisible(visible);
    }

    void Avatar::setNameScale(float scale) {
        m_nameScale = scale;
        if (m_name) fit(m_name, 70.f, scale);
    }

    void Avatar::setDimmed(bool dimmed, bool animate) {
        if (dimmed == m_dimmed) return;
        m_dimmed = dimmed;
        this->applyIcon();
        if (m_name) m_name->setColor(dimmed ? ccColor3B {120, 120, 140} : col::white);
        if (dimmed) {
            this->setIdle(false);
            if (!m_mark) {
                m_mark = fx("cross");
                m_mark->setPosition({9.f * m_scale, 26.f * m_scale});
                this->addChild(m_mark, 5);
            }
            m_mark->setScale(.0f);
            m_mark->runAction(CCEaseBackOut::create(CCScaleTo::create(animate ? .3f : 0.f, .55f + .25f * m_scale)));
            m_icon->stopActionByTag(kTiltTag);
            auto* slump = CCEaseBackOut::create(CCRotateTo::create(animate ? .35f : 0.f, restSpin(m_icon) + 14.f));
            slump->setTag(kTiltTag);
            m_icon->runAction(slump);
            m_shadow->setOpacity(70);
        } else {
            if (m_mark) {
                m_mark->removeFromParent();
                m_mark = nullptr;
            }
            m_icon->setRotation(restSpin(m_icon));
            m_shadow->setOpacity(150);
        }
    }

    void Avatar::setYou(bool you) {
        if (you == m_you) return;
        m_you = you;
        if (you) {
            m_ring = fx("ring");
            m_ring->setColor(col::green);
            m_ring->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            m_ring->setScaleX(.42f * m_scale + .08f);
            m_ring->setScaleY(.14f * m_scale + .03f);
            m_ring->setPosition({0.f, 1.f});
            this->addChild(m_ring, -1);
            m_ring->runAction(CCRepeatForever::create(
                CCSequence::create(CCFadeTo::create(.6f, 90), CCFadeTo::create(.6f, 230), nullptr)));
            if (m_name) m_name->setColor(col::green);
        } else {
            if (m_ring) m_ring->removeFromParent();
            m_ring = nullptr;
            if (m_name) m_name->setColor(m_dimmed ? ccColor3B {120, 120, 140} : col::white);
        }
    }

    void Avatar::setIdle(bool idle) {
        if (idle == m_idle) return;
        m_idle = idle;
        m_bob->stopActionByTag(kBobTag);
        m_bob->setPosition({0.f, 0.f});
        this->unschedule(schedule_selector(Avatar::randomHop));
        if (!idle) return;
        float t = fx::rnd(.9f, 1.3f);
        auto* bob = CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCMoveBy::create(t, {0.f, 1.6f * m_scale})),
            CCEaseSineInOut::create(CCMoveBy::create(t, {0.f, -1.6f * m_scale})), nullptr));
        bob->setTag(kBobTag);
        m_bob->runAction(bob);
        this->scheduleRandomHop();
    }

    void Avatar::scheduleRandomHop() {
        this->scheduleOnce(schedule_selector(Avatar::randomHop), fx::rnd(4.f, 10.f));
    }

    void Avatar::randomHop(float) {
        if (!m_idle) return;
        if (!busy && !m_dimmed && m_autoHop && !m_walking) {
            if (fx::rndi(0, 5) == 0) this->wave(false);
            else this->hop(fx::rnd(8.f, 14.f) * std::max(.6f, m_scale));
        }
        this->scheduleRandomHop();
    }

    void Avatar::setWalking(bool walking, int dir) {
        constexpr int kWalkTag = 0x3A1C;
        auto tilt = [this](float to, float time) {
            m_icon->stopActionByTag(kTiltTag);
            auto* act = CCEaseSineOut::create(CCRotateTo::create(time, to));
            act->setTag(kTiltTag);
            m_icon->runAction(act);
        };
        if (walking == m_walking) {
            if (walking && dir && dir != m_walkDir) {
                m_walkDir = dir;
                tilt(restSpin(m_icon) + dir * 7.f, .1f);
            }
            return;
        }
        m_walking = walking;
        m_walkDir = walking ? dir : 0;
        m_body->stopActionByTag(kWalkTag);
        if (!walking) {
            m_body->runAction(CCEaseElasticOut::create(CCScaleTo::create(.3f, 1.f), .4f));
            tilt(restSpin(m_icon), .15f);
            return;
        }
        if (dir) tilt(restSpin(m_icon) + dir * 7.f, .1f);
        auto* step = CCRepeatForever::create(CCSequence::create(
            CCEaseSineOut::create(CCScaleTo::create(.09f, 1.06f, .9f)),
            CCEaseSineIn::create(CCScaleTo::create(.09f, .96f, 1.06f)), nullptr));
        step->setTag(kWalkTag);
        m_body->runAction(step);
    }

    bool Avatar::onGround() const {
        return std::abs(m_body->getPositionY()) < .5f;
    }

    void Avatar::setScaleSmooth(float scale, float time) {
        this->runAction(CCEaseInOut::create(CCScaleTo::create(time, scale), 2.f));
    }

    void Avatar::squash(float amount) {
        m_body->stopActionByTag(0x5015);
        m_body->setScaleX(1.f + .35f * amount);
        m_body->setScaleY(1.f - .38f * amount);
        auto* back = CCEaseElasticOut::create(CCScaleTo::create(.5f, 1.f), .35f);
        back->setTag(0x5015);
        m_body->runAction(back);
    }

    void Avatar::hop(float height, float time) {
        // stretch up, jump with a quarter turn like a GD cube, squash on landing
        m_body->setScaleX(.88f);
        m_body->setScaleY(1.14f);
        m_body->runAction(CCScaleTo::create(time * .3f, 1.f));
        m_body->runAction(CCSequence::create(
            CCJumpBy::create(time, {0.f, 0.f}, height, 1),
            CallFuncExt::create([this] { this->squash(.45f); }), nullptr));
        m_icon->runAction(CCEaseInOut::create(CCRotateBy::create(time, 90.f), 1.6f));
        m_shadow->runAction(CCSequence::create(
            CCScaleTo::create(time * .5f, m_scale * .7f), CCScaleTo::create(time * .5f, m_scale * 1.1f), nullptr));
    }

    void Avatar::wave(bool withBubble) {
        // a friendly side-to-side tilt, like waving with the whole cube
        m_icon->stopActionByTag(kTiltTag);
        float r = restSpin(m_icon);
        auto* wiggle = CCSequence::create(
            CCEaseSineOut::create(CCRotateTo::create(.12f, r - 16.f)),
            CCEaseSineInOut::create(CCRotateTo::create(.16f, r + 14.f)),
            CCEaseSineInOut::create(CCRotateTo::create(.16f, r - 11.f)),
            CCEaseSineInOut::create(CCRotateTo::create(.14f, r + 7.f)),
            CCEaseSineOut::create(CCRotateTo::create(.12f, r)), nullptr);
        wiggle->setTag(kTiltTag);
        m_icon->runAction(wiggle);
        if (withBubble) this->emote(1, 1.6f);
    }

    void Avatar::emote(int id, float hold) {
        if (m_bubble) {
            m_bubble->removeFromParent();
            m_bubble = nullptr;
        }
        float k = std::clamp(.55f + .4f * m_scale, .6f, 1.f);
        auto* bubble = CCNode::create();
        auto* bg = fx("bubble");
        bg->setAnchorPoint({.5f, 0.f});
        bg->setScale(k);
        bubble->addChild(bg);
        auto* e = ui::emote(id);
        e->setScale(k * .52f);
        e->setPosition({0.f, k * 23.f});
        bubble->addChild(e, 1);
        bubble->setPosition({0.f, 30.f * m_scale + 3.f});
        bubble->setRotation(fx::rnd(-6.f, 6.f));
        this->addChild(bubble, 20);
        m_bubble = bubble;
        fx::popIn(bubble, 1.f, 0.f, .45f);
        e->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCRotateTo::create(.35f, -6.f)),
            CCEaseSineInOut::create(CCRotateTo::create(.35f, 6.f)), nullptr)));
        bubble->runAction(CCSequence::create(
            CCDelayTime::create(hold),
            CCEaseBackIn::create(CCScaleTo::create(.22f, 0.f)),
            CallFuncExt::create([this, bubble] {
                if (m_bubble == bubble) m_bubble = nullptr;
            }),
            CCRemoveSelf::create(), nullptr));
        fx::play("boop");
    }

    void Avatar::dropIn(float fromHeight, float delay, std::function<void()> done) {
        busy = true;
        m_body->setVisible(false);
        m_shadow->setOpacity(0);
        m_shadow->setScale(m_scale * .3f);
        if (m_name) m_name->setOpacity(0);
        m_body->setPosition({0.f, fromHeight});

        this->runAction(CCSequence::create(
            CCDelayTime::create(delay),
            CallFuncExt::create([this, fromHeight] {
                auto* parent = this->getParent();
                fx::poof(parent, this->getPosition() + ccp(0.f, fromHeight + 15.f * m_scale), .55f + .35f * m_scale,
                         col::white, this->getZOrder() + 1);
                fx::play("pop");
                m_body->setVisible(true);
                m_body->setScale(0.f);
                m_body->runAction(CCEaseBackOut::create(CCScaleTo::create(.16f, 1.f)));
                m_icon->setRotation(0.f);
            }),
            CCDelayTime::create(.2f),
            CallFuncExt::create([this] {
                float fall = .42f;
                m_body->runAction(CCEaseIn::create(CCMoveTo::create(fall, {0.f, 0.f}), 2.2f));
                m_icon->runAction(CCEaseIn::create(CCRotateBy::create(fall, 180.f), 1.4f));
                m_shadow->runAction(CCSpawn::create(CCFadeTo::create(fall, 150),
                                                    CCScaleTo::create(fall, m_scale * 1.1f), nullptr));
            }),
            CCDelayTime::create(.42f),
            CallFuncExt::create([this] {
                this->squash(1.f);
                fx::dust(this->getParent(), this->getPosition(), .6f + .4f * m_scale, this->getZOrder() - 1);
                fx::play("land");
                if (m_name) m_name->runAction(CCFadeIn::create(.25f));
            }),
            CCDelayTime::create(.3f),
            CallFuncExt::create([this, done] {
                this->wave(true);
                busy = false;
                if (done) done();
            }),
            nullptr));
    }

    void Avatar::leaveAndRemove() {
        busy = true;
        this->setIdle(false);
        this->hop(18.f * std::max(.7f, m_scale), .3f);
        this->runAction(CCSequence::create(
            CCDelayTime::create(.16f),
            CallFuncExt::create([this] {
                fx::poof(this->getParent(), this->getPosition() + ccp(0.f, 22.f * m_scale), .5f + .3f * m_scale,
                         col::white, this->getZOrder() + 1);
                fx::play("pop");
            }),
            CCEaseBackIn::create(CCScaleTo::create(.18f, 0.f)),
            CCRemoveSelf::create(), nullptr));
    }

    void Avatar::yeet(bool right) {
        busy = true;
        this->setIdle(false);
        if (m_bubble) m_bubble->setVisible(false);
        auto win = CCDirector::get()->getWinSize();
        float dx = (right ? 1.f : -1.f) * (win.width * .8f);
        fx::flash(this->getParent(), this->getPosition() + ccp(0.f, 15.f * m_scale), 30.f, col::red, .9f,
                  this->getZOrder() + 1);
        fx::play("hit");
        m_icon->runAction(CCRotateBy::create(1.f, right ? 900.f : -900.f));
        m_shadow->setVisible(false);
        if (m_name) m_name->setVisible(false);
        this->runAction(CCSequence::create(
            CCJumpBy::create(1.f, {dx, -40.f}, 150.f, 1), CCRemoveSelf::create(), nullptr));
    }

    // ---------------------------------------------------------------- sword

    void Avatar::setSword(bool on, int facing) {
        m_facing = facing >= 0 ? 1 : -1;
        if (!on) {
            if (m_hand) m_hand->removeFromParent();
            m_hand = nullptr;
            m_sword = nullptr;
            m_handOffset = CCPoint(0.f, 0.f);
            this->setLean(0.f);
            return;
        }
        if (!m_hand) {
            m_hand = CCNode::create();
            m_bob->addChild(m_hand, 2);
            m_sword = fx("sword");
            m_sword->setAnchorPoint({.5f, .21f});  // the grip
            m_sword->setScale(.62f * m_scale);
            m_hand->addChild(m_sword);
        }
        m_handOffset = CCPoint(0.f, 0.f);
        m_hand->stopAllActions();
        m_hand->setPosition(this->handSpot());
        m_sword->setFlipX(m_facing < 0);
        this->setSwordAngle(kSwordRest);
    }

    CCPoint Avatar::handSpot() const {
        return {m_facing * (12.5f + m_handOffset.x) * m_scale, (12.f + m_handOffset.y) * m_scale};
    }

    void Avatar::setHandOffset(CCPoint offset) {
        m_handOffset = offset;
        if (!m_hand) return;
        m_hand->stopActionByTag(0x4A2D);
        m_hand->setPosition(this->handSpot());
    }

    void Avatar::handTo(CCPoint offset, float time) {
        m_handOffset = offset;
        if (!m_hand) return;
        m_hand->stopActionByTag(0x4A2D);
        auto* act = CCEaseSineInOut::create(CCMoveTo::create(time, this->handSpot()));
        act->setTag(0x4A2D);
        m_hand->runAction(act);
    }

    void Avatar::setLean(float deg) {
        m_lean = deg;
        m_body->stopActionByTag(0x1EA2);
        m_body->setRotation(deg * m_facing);
    }

    void Avatar::leanTo(float deg, float time) {
        m_lean = deg;
        m_body->stopActionByTag(0x1EA2);
        auto* act = CCEaseSineInOut::create(CCRotateTo::create(time, deg * m_facing));
        act->setTag(0x1EA2);
        m_body->runAction(act);
    }

    void Avatar::swordDraw(int facing, float delay) {
        this->setSword(true, facing);
        float full = .62f * m_scale;
        m_sword->stopAllActions();
        m_sword->setScale(0.f);
        m_sword->setRotation(-70.f * m_facing);
        m_sword->runAction(CCSequence::create(
            CCDelayTime::create(delay),
            CallFuncExt::create([] { fx::playLimited("shing", .09f, .45f, fx::rnd(.94f, 1.1f)); }),
            CCEaseBackOut::create(CCScaleTo::create(.2f, full)), nullptr));
        auto* swing = CCSequence::create(
            CCDelayTime::create(delay),
            CCEaseBackOut::create(CCRotateTo::create(.38f, kSwordRest * m_facing)), nullptr);
        swing->setTag(0x5D);
        m_sword->runAction(swing);
    }

    void Avatar::flinch() {
        m_icon->stopActionByTag(kTiltTag);
        float r = restSpin(m_icon);
        auto* rock = CCSequence::create(
            CCEaseOut::create(CCRotateTo::create(.06f, r - 14.f * m_facing), 2.f),
            CCEaseElasticOut::create(CCRotateTo::create(.45f, r), .35f), nullptr);
        rock->setTag(kTiltTag);
        m_icon->runAction(rock);
        this->squash(.4f);
    }

    void Avatar::setFacing(int facing) {
        int f = facing >= 0 ? 1 : -1;
        if (!m_hand) {
            m_facing = f;
            return;
        }
        if (f == m_facing) return;  // (keeps any swing in progress)
        float angle = m_sword->getRotation() * m_facing;
        m_facing = f;
        m_hand->stopActionByTag(0x4A2D);
        m_hand->setPosition(this->handSpot());
        m_sword->setFlipX(m_facing < 0);
        m_body->setRotation(m_lean * m_facing);
        this->setSwordAngle(angle);
    }

    void Avatar::setSwordAngle(float deg) {
        if (!m_sword) return;
        m_sword->stopActionByTag(0x5D);
        m_sword->setRotation(deg * m_facing);
    }

    void Avatar::swordTo(float deg, float time) {
        if (!m_sword) return;
        m_sword->stopActionByTag(0x5D);
        auto* act = CCEaseSineInOut::create(CCRotateTo::create(time, deg * m_facing));
        act->setTag(0x5D);
        m_sword->runAction(act);
    }

    CCPoint Avatar::handWorld() {
        if (!m_hand || !this->getParent()) return this->getPosition() + this->handSpot() * this->getScale();
        // through the real transforms, so body lean and squash are taken into account
        return this->getParent()->convertToNodeSpace(m_hand->convertToWorldSpace({0.f, 0.f}));
    }

    CCPoint Avatar::swordTipWorld() {
        float rad = ((m_sword ? m_sword->getRotation() : 0.f) + m_body->getRotation()) * 3.14159265f / 180.f;
        float len = 52.f * .79f * .62f * m_scale * this->getScale();
        return this->handWorld() + ccp(std::sin(rad), std::cos(rad)) * len;
    }

    void Avatar::swordDrop() {
        if (!m_sword || !this->getParent()) {
            this->setSword(false, m_facing);
            return;
        }
        auto* parent = this->getParent();
        auto* loose = fx("sword");
        loose->setAnchorPoint({.5f, .21f});
        loose->setScale(m_sword->getScale() * this->getScale());
        loose->setRotation(m_sword->getRotation() + m_body->getRotation());
        loose->setFlipX(m_facing < 0);
        loose->setPosition(this->handWorld());
        parent->addChild(loose, this->getZOrder() + 1);
        float dir = -static_cast<float>(m_facing);
        loose->runAction(CCSequence::create(
            CCSpawn::create(CCJumpBy::create(.65f, {dir * 22.f * m_scale, -12.f * m_scale}, 26.f * m_scale, 1),
                            CCRotateBy::create(.65f, dir * 560.f), nullptr),
            CCDelayTime::create(.4f), CCFadeOut::create(.4f), CCRemoveSelf::create(), nullptr));
        this->setSword(false, m_facing);
    }

    void Avatar::swordRaise() {
        if (!m_sword) return;
        // held up high over the head
        this->handTo({-7.f, 17.f}, .2f);
        this->swordTo(-4.f, .22f);
        this->leanTo(-4.f, .2f);
        this->runAction(CCSequence::create(CCDelayTime::create(.24f), CallFuncExt::create([this] {
            fx::stars(this->getParent(), this->swordTipWorld(), 4, col::goldLight, 14.f, this->getZOrder() + 2);
            this->leanTo(0.f, .4f);
        }), nullptr));
    }

    void Avatar::swordRest(float time) {
        if (!m_sword) return;
        this->handTo({0.f, 0.f}, time);
        this->swordTo(kSwordRest, time);
        this->leanTo(0.f, time);
    }

    void Avatar::knockOut() {
        this->swordDrop();
        auto* parent = this->getParent();
        auto pos = this->getPosition() + ccp(0.f, 15.f * m_scale * this->getScale());
        fx::deathBurst(parent, pos, iconColor(m_player.icon.p), iconColor(m_player.icon.s), .5f + .5f * m_scale,
                       this->getZOrder() + 2);
        this->setDimmed(true, true);
        m_body->setScale(.4f);
        m_body->runAction(CCEaseElasticOut::create(CCScaleTo::create(.6f, 1.f), .4f));
    }

    void Avatar::celebrate() {
        this->runAction(CCSequence::create(
            CallFuncExt::create([this] { this->hop(16.f * std::max(.7f, m_scale), .32f); }),
            CCDelayTime::create(.4f),
            CallFuncExt::create([this] { this->hop(11.f * std::max(.7f, m_scale), .28f); }), nullptr));
    }
}
