#pragma once

// Versus-style look: deep navy plates, gold trim, GD's own fonts and icons.

#include <Geode/Geode.hpp>
#include "../core/Model.hpp"

namespace nt::ui {
    using namespace cocos2d;

    namespace col {
        inline constexpr ccColor3B navy {18, 28, 64};
        inline constexpr ccColor3B navyDeep {8, 12, 32};
        inline constexpr ccColor3B plate {22, 32, 70};
        inline constexpr ccColor3B gold {255, 206, 90};
        inline constexpr ccColor3B goldLight {255, 230, 150};
        inline constexpr ccColor3B blue {90, 180, 255};
        inline constexpr ccColor3B red {255, 92, 84};
        inline constexpr ccColor3B green {110, 255, 140};
        inline constexpr ccColor3B dim {150, 165, 205};
        inline constexpr ccColor3B white {255, 255, 255};
        inline constexpr ccColor3B ink {16, 20, 40};
    }

    ccColor4F c4f(ccColor3B c, float a = 1.f);

    // a rounded rectangle drawn with CCDrawNode (origin bottom-left, size = rect)
    class Plate : public CCNode {
    public:
        static Plate* create(CCSize size, ccColor3B fill, float fillAlpha, ccColor3B border, float borderAlpha,
                             float radius = 8.f, float borderWidth = 1.5f);
        void setFill(ccColor3B fill, float alpha);
        void setBorder(ccColor3B border, float alpha);
        void setSize(CCSize size);

    protected:
        void redraw();
        CCDrawNode* m_draw = nullptr;
        ccColor3B m_fill, m_border;
        float m_fillA = 1.f, m_borderA = 1.f, m_radius = 8.f, m_borderW = 1.5f;
    };

    CCLabelBMFont* label(std::string const& text, char const* font = "bigFont.fnt", float scale = .5f,
                         ccColor3B color = col::white);
    // shrinks the label so it is at most `width` wide
    void fit(CCLabelBMFont* label, float width, float maxScale);

    CCMenuItemSpriteExtra* textButton(std::string const& text, char const* bg, float scale,
                                      geode::Function<void(CCMenuItemSpriteExtra*)> cb, int width = 0);
    CCMenuItemSpriteExtra* frameButton(char const* frame, float scale, geode::Function<void(CCMenuItemSpriteExtra*)> cb);
    CCMenuItemSpriteExtra* nodeButton(CCNode* node, geode::Function<void(CCMenuItemSpriteExtra*)> cb);

    CCSprite* fx(char const* name);           // our own sprites in resources/fx
    CCSprite* emote(int id);                  // Nater's Timer Battles emotes 1..15
    char const* emoteName(int id);
    ccColor3B emoteRarity(int id);
    GJDifficultySprite* difficulty(int face);

    ccColor3B iconColor(int index);
    void sfx(char const* file, float volume = 1.f, float speed = 1.f);

    // GD-like animated backdrop: gradient + drifting background art + optional ground
    class Backdrop : public CCNode {
    public:
        static Backdrop* create(ccColor3B tint, bool ground, float groundY = 60.f);
        float groundY() const { return m_groundY; }
        void setGround(bool visible, bool animate);

    protected:
        bool init(ccColor3B tint, bool ground, float groundY);
        void update(float dt) override;
        std::vector<CCSprite*> m_bg;
        std::vector<CCSprite*> m_ground;
        CCNode* m_groundRoot = nullptr;
        float m_bgW = 0.f, m_groundW = 0.f, m_groundY = 0.f;
        CCNode* m_motes = nullptr;
    };
}
