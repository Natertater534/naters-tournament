#include "Fx.hpp"
#include "../core/Compat.hpp"

#include <cmath>
#include <random>
#include <unordered_map>

using namespace geode::prelude;

namespace nt::fx {
    namespace {
        constexpr float kPi = 3.14159265358979f;
        std::mt19937& rng() {
            static std::mt19937 r(static_cast<unsigned>(std::random_device {}()));
            return r;
        }
    }

    float rnd(float lo, float hi) {
        return std::uniform_real_distribution<float>(lo, hi)(rng());
    }

    int rndi(int lo, int hi) {
        return std::uniform_int_distribution<int>(lo, hi)(rng());
    }

    void play(char const* name) {
        auto* engine = FMODAudioEngine::sharedEngine();
        if (!engine) return;
        auto rel = Mod::get()->expandSpriteName(std::string(name) + ".ogg");
        auto full = compat::str(CCFileUtils::sharedFileUtils()->fullPathForFilename(rel.c_str(), false));
        engine->playEffect(full);
    }

    // ---------------------------------------------------------------- Particles

    Particles* Particles::create() {
        auto* ret = new Particles();
        if (ret->init()) {
            ret->autorelease();
            ret->scheduleUpdate();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    void Particles::add(CCSprite* spr, CCPoint pos, CCPoint vel, float life, float gravity, float drag, float spin,
                        float scale0, float scale1, bool fade) {
        spr->setPosition(pos);
        spr->setScale(scale0);
        this->addChild(spr);
        m_ps.push_back({spr, vel, life, 0.f, gravity, drag, spin, scale0, scale1, spr->getOpacity(), fade});
    }

    void Particles::update(float dt) {
        dt = std::min(dt, 1.f / 20.f);
        for (auto& p : m_ps) {
            if (!p.spr) continue;
            p.age += dt;
            float k = std::clamp(p.age / p.life, 0.f, 1.f);
            if (k >= 1.f) {
                p.spr->removeFromParent();
                p.spr = nullptr;
                continue;
            }
            p.vel.y -= p.gravity * dt;
            float damp = std::max(0.f, 1.f - p.drag * dt);
            p.vel = p.vel * damp;
            p.spr->setPosition(p.spr->getPosition() + p.vel * dt);
            p.spr->setRotation(p.spr->getRotation() + p.spin * dt);
            p.spr->setScale(p.s0 + (p.s1 - p.s0) * k);
            if (p.fade) {
                float a = k < .6f ? 1.f : 1.f - (k - .6f) / .4f;
                p.spr->setOpacity(static_cast<GLubyte>(p.a0 * a));
            }
        }
        std::erase_if(m_ps, [](P const& p) { return p.spr == nullptr; });
        if (m_ps.empty()) this->removeFromParent();
    }

    static Particles* layer(CCNode* parent, int z) {
        auto* ps = Particles::create();
        parent->addChild(ps, z);
        return ps;
    }

    // ---------------------------------------------------------------- effects

    void poof(CCNode* parent, CCPoint pos, float scale, ccColor3B color, int z) {
        if (!parent) return;
        auto* ps = layer(parent, z);
        int n = 9;
        for (int i = 0; i < n; ++i) {
            float a = kPi * 2.f * i / n + rnd(-.25f, .25f);
            float sp = rnd(70.f, 120.f) * scale;
            auto* s = ui::fx("puff");
            s->setColor(color);
            s->setOpacity(235);
            s->setRotation(rnd(0.f, 360.f));
            ps->add(s, pos + ccp(std::cos(a), std::sin(a)) * 4.f * scale, ccp(std::cos(a), std::sin(a)) * sp,
                    rnd(.38f, .55f), 0.f, 6.5f, rnd(-90.f, 90.f), rnd(.55f, .8f) * scale, rnd(1.05f, 1.35f) * scale);
        }
        // the dense core
        auto* core = ui::fx("puff");
        core->setColor(color);
        ps->add(core, pos, ccp(0.f, 8.f), .32f, 0.f, 0.f, 40.f, .9f * scale, 1.6f * scale);
        stars(parent, pos, 4, ui::col::goldLight, 34.f * scale, z + 1);
    }

    void dust(CCNode* parent, CCPoint pos, float scale, int z) {
        if (!parent) return;
        auto* ps = layer(parent, z);
        for (int side = -1; side <= 1; side += 2) {
            for (int i = 0; i < 4; ++i) {
                auto* s = ui::fx("puff");
                s->setColor({200, 210, 240});
                s->setOpacity(170);
                ps->add(s, pos + ccp(side * rnd(2.f, 8.f), 2.f), ccp(side * rnd(40.f, 85.f), rnd(6.f, 26.f)) * scale,
                        rnd(.3f, .45f), 0.f, 5.f, side * rnd(40.f, 160.f), rnd(.25f, .35f) * scale, rnd(.5f, .7f) * scale);
            }
        }
    }

    void sparks(CCNode* parent, CCPoint pos, int count, ccColor3B color, float dist, int z) {
        if (!parent) return;
        auto* ps = layer(parent, z);
        for (int i = 0; i < count; ++i) {
            float a = rnd(0.f, kPi * 2.f);
            float sp = rnd(.6f, 1.f) * dist * 5.f;
            auto* s = ui::fx("spark");
            s->setColor(color);
            s->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            s->setRotation(-a * 180.f / kPi);
            ps->add(s, pos, ccp(std::cos(a), std::sin(a)) * sp, rnd(.22f, .38f), 0.f, 7.f, 0.f, rnd(.5f, .9f), .1f);
        }
    }

    void stars(CCNode* parent, CCPoint pos, int count, ccColor3B color, float spread, int z) {
        if (!parent) return;
        auto* ps = layer(parent, z);
        for (int i = 0; i < count; ++i) {
            float a = rnd(0.f, kPi * 2.f);
            auto* s = ui::fx("star");
            s->setColor(color);
            s->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            ps->add(s, pos, ccp(std::cos(a), std::sin(a)) * rnd(.6f, 1.f) * spread * 2.6f, rnd(.45f, .7f), 0.f, 3.f,
                    rnd(-200.f, 200.f), rnd(.5f, .8f), 0.f);
        }
    }

    void flash(CCNode* parent, CCPoint pos, float radius, ccColor3B color, float alpha, int z) {
        if (!parent) return;
        auto* g = ui::fx("glow");
        g->setColor(color);
        g->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        g->setPosition(pos);
        float base = radius * 2.f / g->getContentSize().width;
        g->setScale(base * .4f);
        g->setOpacity(static_cast<GLubyte>(255 * alpha));
        parent->addChild(g, z);
        g->runAction(CCSequence::create(
            CCSpawn::create(CCEaseOut::create(CCScaleTo::create(.18f, base), 2.f), CCFadeOut::create(.32f), nullptr),
            CCRemoveSelf::create(), nullptr));
    }

    void ring(CCNode* parent, CCPoint pos, ccColor3B color, float toScale, float time, int z) {
        if (!parent) return;
        auto* r = ui::fx("ring");
        r->setColor(color);
        r->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        r->setPosition(pos);
        r->setScale(.15f);
        parent->addChild(r, z);
        r->runAction(CCSequence::create(
            CCSpawn::create(CCEaseOut::create(CCScaleTo::create(time, toScale), 2.4f), CCFadeOut::create(time), nullptr),
            CCRemoveSelf::create(), nullptr));
    }

    void confetti(CCNode* parent, CCRect area, int count, int z) {
        if (!parent) return;
        static ccColor3B const colors[] = {{255, 90, 90}, {255, 206, 90}, {110, 255, 140}, {90, 180, 255},
                                           {200, 120, 255}, {255, 255, 255}};
        auto* ps = layer(parent, z);
        for (int i = 0; i < count; ++i) {
            auto* s = ui::fx("square");
            s->setColor(colors[rndi(0, 5)]);
            s->setScaleX(rnd(.7f, 1.4f));
            CCPoint p {rnd(area.getMinX(), area.getMaxX()), rnd(area.getMinY(), area.getMaxY())};
            CCPoint v {rnd(-90.f, 90.f), rnd(60.f, 260.f)};
            ps->add(s, p, v, rnd(2.2f, 3.4f), 260.f, 1.2f, rnd(-540.f, 540.f), rnd(.8f, 1.2f), rnd(.6f, 1.f));
        }
    }

    void deathBurst(CCNode* parent, CCPoint pos, ccColor3B c1, ccColor3B c2, float scale, int z) {
        if (!parent) return;
        ring(parent, pos, {255, 255, 255}, 1.1f * scale, .4f, z);
        flash(parent, pos, 34.f * scale, {255, 255, 255}, .9f, z);
        auto* ps = layer(parent, z + 1);
        for (int i = 0; i < 14; ++i) {
            float a = rnd(0.f, kPi * 2.f);
            auto* s = ui::fx("square");
            s->setColor(i % 2 ? c1 : c2);
            ps->add(s, pos, ccp(std::cos(a), std::sin(a)) * rnd(80.f, 200.f) * scale, rnd(.45f, .7f), 120.f, 2.f,
                    rnd(-400.f, 400.f), rnd(1.1f, 1.8f) * scale, .2f);
        }
    }

    void shake(CCNode* node, float strength, float duration) {
        if (!node) return;
        static std::unordered_map<CCNode*, CCPoint> s_origins;
        constexpr int kTag = 0x5A4B;
        CCPoint origin = node->getPosition();
        if (auto it = s_origins.find(node); it != s_origins.end() && node->getActionByTag(kTag)) {
            origin = it->second;
            node->stopActionByTag(kTag);
        }
        s_origins[node] = origin;
        auto* steps = CCArray::create();
        int n = std::max(2, static_cast<int>(duration / .03f));
        for (int i = 0; i < n; ++i) {
            float k = strength * (1.f - static_cast<float>(i) / n);
            steps->addObject(CCMoveTo::create(.03f, origin + ccp(rnd(-k, k), rnd(-k, k))));
        }
        steps->addObject(CCMoveTo::create(.03f, origin));
        steps->addObject(CallFuncExt::create([node] { s_origins.erase(node); }));
        auto* seq = CCSequence::create(steps);
        seq->setTag(kTag);
        node->runAction(seq);
    }

    CCLabelBMFont* floatText(CCNode* parent, CCPoint pos, std::string const& text, ccColor3B color, float scale,
                             char const* font, int z) {
        if (!parent) return nullptr;
        auto* l = ui::label(text, font, 0.f, color);
        l->setPosition(pos);
        parent->addChild(l, z);
        l->runAction(CCSequence::create(
            CCEaseBackOut::create(CCScaleTo::create(.25f, scale)),
            CCSpawn::create(CCEaseOut::create(CCMoveBy::create(1.1f, {0.f, 22.f}), 2.f),
                            CCSequence::create(CCDelayTime::create(.6f), CCFadeOut::create(.5f), nullptr), nullptr),
            CCRemoveSelf::create(), nullptr));
        return l;
    }

    void screenFlash(CCNode* parent, ccColor3B color, float alpha, float time, int z) {
        if (!parent) return;
        auto win = CCDirector::get()->getWinSize();
        auto* c = CCLayerColor::create(ccc4(color.r, color.g, color.b, static_cast<GLubyte>(255 * alpha)), win.width,
                                       win.height);
        parent->addChild(c, z);
        c->runAction(CCSequence::create(CCFadeOut::create(time), CCRemoveSelf::create(), nullptr));
    }

    void banner(CCNode* parent, std::string const& title, std::string const& subtitle, ccColor3B accent, float hold,
                int z) {
        if (!parent) return;
        auto win = CCDirector::get()->getWinSize();
        auto* root = CCNode::create();
        root->setPosition({0.f, win.height / 2.f + 10.f});
        parent->addChild(root, z);

        float h = subtitle.empty() ? 46.f : 60.f;
        auto* band = ui::Plate::create({win.width + 40.f, h}, ui::col::navyDeep, .88f, accent, .9f, 0.f, 2.f);
        band->setAnchorPoint({.5f, .5f});
        band->setPosition({win.width / 2.f, 0.f});
        band->setScaleY(0.f);
        root->addChild(band);
        band->runAction(CCSequence::create(CCEaseBackOut::create(CCScaleTo::create(.22f, 1.f, 1.f)),
                                           CCDelayTime::create(hold + .25f),
                                           CCEaseIn::create(CCScaleTo::create(.18f, 1.f, 0.f), 2.f), nullptr));

        auto* t = ui::label(title, "goldFont.fnt", .95f, ui::col::white);
        ui::fit(t, win.width - 60.f, .95f);
        float ts = t->getScale();
        t->setPosition({-win.width / 2.f, subtitle.empty() ? 1.f : 9.f});
        root->addChild(t, 2);
        t->runAction(CCSequence::create(
            CCEaseBackOut::create(CCMoveTo::create(.32f, {win.width / 2.f, t->getPositionY()})),
            CCDelayTime::create(hold),
            CCEaseIn::create(CCMoveTo::create(.25f, {win.width * 1.5f, t->getPositionY()}), 2.f), nullptr));
        t->setScale(ts);

        if (!subtitle.empty()) {
            auto* s = ui::label(subtitle, "bigFont.fnt", .42f, accent);
            ui::fit(s, win.width - 80.f, .42f);
            s->setPosition({win.width * 1.5f, -15.f});
            root->addChild(s, 2);
            s->runAction(CCSequence::create(
                CCDelayTime::create(.06f),
                CCEaseBackOut::create(CCMoveTo::create(.32f, {win.width / 2.f, -15.f})),
                CCDelayTime::create(hold - .06f),
                CCEaseIn::create(CCMoveTo::create(.25f, {-win.width / 2.f, -15.f}), 2.f), nullptr));
        }
        root->runAction(CCSequence::create(CCDelayTime::create(hold + .8f), CCRemoveSelf::create(), nullptr));
    }

    void popIn(CCNode* node, float scale, float delay, float time) {
        if (!node) return;
        node->setScale(0.f);
        node->runAction(CCSequence::create(CCDelayTime::create(delay),
                                           CCEaseElasticOut::create(CCScaleTo::create(time, scale), .55f), nullptr));
    }
}
