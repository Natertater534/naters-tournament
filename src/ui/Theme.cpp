#include "Theme.hpp"
#include "../core/Compat.hpp"

#include <cmath>
#include <random>

namespace { constexpr float kPi = 3.14159265358979f; }

using namespace geode::prelude;

namespace nt::ui {
    ccColor4F c4f(ccColor3B c, float a) {
        return ccColor4F {c.r / 255.f, c.g / 255.f, c.b / 255.f, a};
    }

    // ---------------------------------------------------------------- Plate

    Plate* Plate::create(CCSize size, ccColor3B fill, float fillAlpha, ccColor3B border, float borderAlpha,
                         float radius, float borderWidth) {
        auto* ret = new Plate();
        if (ret->init()) {
            ret->m_fill = fill;
            ret->m_fillA = fillAlpha;
            ret->m_border = border;
            ret->m_borderA = borderAlpha;
            ret->m_radius = radius;
            ret->m_borderW = borderWidth;
            ret->m_draw = CCDrawNode::create();
            ret->addChild(ret->m_draw);
            ret->setContentSize(size);
            ret->redraw();
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    void Plate::setFill(ccColor3B fill, float alpha) {
        m_fill = fill;
        m_fillA = alpha;
        this->redraw();
    }

    void Plate::setBorder(ccColor3B border, float alpha) {
        m_border = border;
        m_borderA = alpha;
        this->redraw();
    }

    void Plate::setSize(CCSize size) {
        this->setContentSize(size);
        this->redraw();
    }

    void Plate::redraw() {
        if (!m_draw) return;
        m_draw->clear();
        auto size = this->getContentSize();
        float r = std::min({m_radius, size.width / 2.f, size.height / 2.f});
        std::vector<CCPoint> pts;
        constexpr int seg = 6;
        CCPoint centres[4] = {
            {size.width - r, size.height - r}, {r, size.height - r}, {r, r}, {size.width - r, r}};
        for (int c = 0; c < 4; ++c) {
            float a0 = kPi / 2.f * c;
            for (int i = 0; i <= seg; ++i) {
                float a = a0 + kPi / 2.f * i / seg;
                pts.push_back(ccp(centres[c].x + std::cos(a) * r, centres[c].y + std::sin(a) * r));
            }
        }
        m_draw->drawPolygon(pts.data(), static_cast<unsigned int>(pts.size()), c4f(m_fill, m_fillA),
                            m_borderA > 0.f ? m_borderW : 0.f, c4f(m_border, m_borderA));
    }

    // ---------------------------------------------------------------- labels & buttons

    CCLabelBMFont* label(std::string const& text, char const* font, float scale, ccColor3B color) {
        auto* l = CCLabelBMFont::create(text.c_str(), font);
        l->setScale(scale);
        l->setColor(color);
        return l;
    }

    void fit(CCLabelBMFont* l, float width, float maxScale) {
        if (!l) return;
        l->limitLabelWidth(width, maxScale, .1f);
    }

    CCMenuItemSpriteExtra* textButton(std::string const& text, char const* bg, float scale,
                                      geode::Function<void(CCMenuItemSpriteExtra*)> cb, int width) {
        auto* spr = ButtonSprite::create(text.c_str(), width, 0, 1.f, width > 0, "goldFont.fnt", bg, 30.f);
        spr->setScale(scale);
        return CCMenuItemExt::createSpriteExtra(spr, std::move(cb));
    }

    CCMenuItemSpriteExtra* frameButton(char const* frame, float scale, geode::Function<void(CCMenuItemSpriteExtra*)> cb) {
        auto* spr = CCSprite::createWithSpriteFrameName(frame);
        if (!spr) spr = CCSprite::create();
        spr->setScale(scale);
        return CCMenuItemExt::createSpriteExtra(spr, std::move(cb));
    }

    CCMenuItemSpriteExtra* nodeButton(CCNode* node, geode::Function<void(CCMenuItemSpriteExtra*)> cb) {
        return CCMenuItemExt::createSpriteExtra(node, std::move(cb));
    }

    CCSprite* fx(char const* name) {
        auto path = Mod::get()->expandSpriteName(std::string(name) + ".png");
        auto* spr = CCSprite::create(path.c_str());
        if (!spr) {
            log::warn("Missing sprite {}", path);
            spr = CCSprite::create();
        }
        return spr;
    }

    CCSprite* emote(int id) {
        id = std::clamp(id, 1, 15);
        auto path = Mod::get()->expandSpriteName(fmt::format("emote-{:02}.png", id));
        auto* spr = CCSprite::create(path.c_str());
        if (!spr) spr = CCSprite::create();
        return spr;
    }

    char const* emoteName(int id) {
        static char const* names[] = {"Hi", "GL HF", "Nice one", "GG", "Thanks", "Rematch", "So close", "Lucky",
                                      "Wow", "Haha", "Bye bye", "Crying", "Good luck", "EZ", "Cheater"};
        return names[std::clamp(id, 1, 15) - 1];
    }

    ccColor3B emoteRarity(int id) {
        if (id <= 6) return {90, 230, 120};
        if (id <= 9) return {90, 170, 255};
        if (id <= 12) return {190, 110, 255};
        return {255, 200, 60};
    }

    GJDifficultySprite* difficulty(int face) {
        auto* spr = GJDifficultySprite::create(std::clamp(face, -1, 10), GJDifficultyName::Short);
        return spr;
    }

    ccColor3B iconColor(int index) {
        auto* gm = GameManager::sharedState();
        if (!gm || index < 0) return col::white;
        return gm->colorForIdx(index);
    }

    void sfx(char const* file, float, float) {
        if (auto* engine = FMODAudioEngine::sharedEngine()) engine->playEffect(file);
    }

    // ---------------------------------------------------------------- Backdrop

    Backdrop* Backdrop::create(ccColor3B tint, bool ground, float groundY) {
        auto* ret = new Backdrop();
        if (ret->init(tint, ground, groundY)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool Backdrop::init(ccColor3B tint, bool ground, float groundY) {
        if (!CCNode::init()) return false;
        auto win = CCDirector::get()->getWinSize();
        this->setContentSize(win);
        m_groundY = ground ? groundY : 0.f;

        auto* grad = CCLayerGradient::create(ccc4(tint.r, tint.g, tint.b, 255),
                                             ccc4(col::navyDeep.r, col::navyDeep.g, col::navyDeep.b, 255));
        grad->setContentSize(win);
        this->addChild(grad, -10);

        // GD's own level background, tinted and slowly drifting
        for (int i = 0; i < 3; ++i) {
            auto* bg = CCSprite::create("game_bg_01_001.png");
            if (!bg) break;
            float scale = win.height / bg->getContentSize().height;
            bg->setScale(scale);
            bg->setAnchorPoint({0.f, 0.f});
            bg->setColor({static_cast<GLubyte>(tint.r * .9f + 20), static_cast<GLubyte>(tint.g * .9f + 30),
                          static_cast<GLubyte>(std::min(255, tint.b + 70))});
            bg->setOpacity(70);
            m_bgW = bg->getScaledContentSize().width - 1.f;
            bg->setPosition({i * m_bgW, 0.f});
            this->addChild(bg, -9);
            m_bg.push_back(bg);
        }

        m_groundRoot = CCNode::create();
        this->addChild(m_groundRoot, -5);
        if (ground) {
            for (int i = 0; i < 8; ++i) {
                auto* g = CCSprite::create("groundSquare_01_001.png");
                if (!g) break;
                float scale = .7f;
                g->setScale(scale);
                g->setAnchorPoint({0.f, 1.f});
                g->setColor({30, 48, 110});
                m_groundW = g->getScaledContentSize().width - .5f;
                g->setPosition({i * m_groundW, groundY});
                m_groundRoot->addChild(g, 0);
                m_ground.push_back(g);
            }
            auto* shade = CCLayerGradient::create(ccc4(0, 0, 0, 0), ccc4(4, 6, 18, 200));
            shade->setContentSize({win.width, groundY});
            m_groundRoot->addChild(shade, 1);
            auto* line = CCLayerColor::create(ccc4(255, 255, 255, 200), win.width, 1.2f);
            line->setPosition({0.f, groundY - .6f});
            m_groundRoot->addChild(line, 2);
            auto* glow = fx("glow");
            glow->setScaleX(win.width / glow->getContentSize().width * 1.1f);
            glow->setScaleY(.25f);
            glow->setPosition({win.width / 2.f, groundY});
            glow->setOpacity(70);
            glow->setColor(col::blue);
            m_groundRoot->addChild(glow, 2);
        }

        // floating motes catch the light
        m_motes = CCNode::create();
        this->addChild(m_motes, -2);
        std::mt19937 rng(1234);
        std::uniform_real_distribution<float> rx(0.f, win.width), ry(m_groundY, win.height), rs(.25f, .7f),
            rt(6.f, 14.f);
        for (int i = 0; i < 26; ++i) {
            auto* d = fx("dot");
            d->setScale(rs(rng));
            d->setOpacity(static_cast<GLubyte>(40 + (i * 37) % 80));
            d->setColor(i % 3 ? col::blue : col::goldLight);
            float x = rx(rng), y = ry(rng);
            d->setPosition({x, y});
            float t = rt(rng);
            d->runAction(CCRepeatForever::create(CCSequence::create(
                CCMoveBy::create(t, {18.f, 26.f}), CCMoveBy::create(t, {-18.f, -26.f}), nullptr)));
            d->runAction(CCRepeatForever::create(CCSequence::create(
                CCFadeTo::create(t * .5f, 30), CCFadeTo::create(t * .5f, 110), nullptr)));
            m_motes->addChild(d);
        }

        auto* vig = fx("vignette");
        vig->setScaleX(win.width / vig->getContentSize().width);
        vig->setScaleY(win.height / vig->getContentSize().height);
        vig->setPosition(win / 2.f);
        vig->setOpacity(200);
        this->addChild(vig, 50);

        this->scheduleUpdate();
        return true;
    }

    void Backdrop::setGround(bool visible, bool animate) {
        if (!m_groundRoot) return;
        CCPoint target = visible ? CCPoint {0.f, 0.f} : CCPoint {0.f, -m_groundY - 12.f};
        m_groundRoot->stopAllActions();
        if (animate) m_groundRoot->runAction(CCEaseInOut::create(CCMoveTo::create(.6f, target), 2.f));
        else m_groundRoot->setPosition(target);
    }

    void Backdrop::update(float dt) {
        if (!m_bg.empty() && m_bgW > 0.f) {
            for (auto* bg : m_bg) {
                bg->setPositionX(bg->getPositionX() - dt * 6.f);
                if (bg->getPositionX() + m_bgW < 0.f) bg->setPositionX(bg->getPositionX() + m_bgW * m_bg.size());
            }
        }
        if (!m_ground.empty() && m_groundW > 0.f) {
            for (auto* g : m_ground) {
                g->setPositionX(g->getPositionX() - dt * 14.f);
                if (g->getPositionX() + m_groundW < 0.f) g->setPositionX(g->getPositionX() + m_groundW * m_ground.size());
            }
        }
    }
}
