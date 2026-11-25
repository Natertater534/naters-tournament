#pragma once

// Fire-and-forget effects. Every node removes itself when its animation ends.

#include <Geode/Geode.hpp>
#include "Theme.hpp"

namespace nt::fx {
    using namespace cocos2d;

    float rnd(float lo, float hi);
    int rndi(int lo, int hi);

    // Plays one of the mod's own sounds (resources/sfx/<name>.ogg).
    void play(char const* name);

    // Simple physics particles: velocity, gravity, drag, spin, fade.
    class Particles : public CCNode {
    public:
        static Particles* create();
        void add(CCSprite* spr, CCPoint pos, CCPoint vel, float life, float gravity = 0.f, float drag = 0.f,
                 float spin = 0.f, float scale0 = 1.f, float scale1 = 0.f, bool fade = true);

    protected:
        struct P {
            CCSprite* spr;
            CCPoint vel;
            float life, age, gravity, drag, spin, s0, s1;
            GLubyte a0;
            bool fade;
        };
        void update(float dt) override;
        std::vector<P> m_ps;
    };

    void poof(CCNode* parent, CCPoint pos, float scale = 1.f, ccColor3B color = {255, 255, 255}, int z = 20);
    void dust(CCNode* parent, CCPoint pos, float scale = 1.f, int z = 4);
    void sparks(CCNode* parent, CCPoint pos, int count, ccColor3B color, float dist = 40.f, int z = 30);
    void stars(CCNode* parent, CCPoint pos, int count, ccColor3B color, float spread = 30.f, int z = 30);
    void flash(CCNode* parent, CCPoint pos, float radius, ccColor3B color, float alpha = 1.f, int z = 30);
    void ring(CCNode* parent, CCPoint pos, ccColor3B color, float toScale = 1.4f, float time = .45f, int z = 25);
    void confetti(CCNode* parent, CCRect area, int count, int z = 80);
    // GD-style death: a white ring plus the icon's colours flying apart as squares
    void deathBurst(CCNode* parent, CCPoint pos, ccColor3B c1, ccColor3B c2, float scale = 1.f, int z = 40);
    void shake(CCNode* node, float strength, float duration);
    CCLabelBMFont* floatText(CCNode* parent, CCPoint pos, std::string const& text, ccColor3B color, float scale,
                             char const* font = "goldFont.fnt", int z = 60);
    void screenFlash(CCNode* parent, ccColor3B color, float alpha, float time, int z = 90);
    // A wide band across the middle of the screen with a title and a smaller line under it.
    void banner(CCNode* parent, std::string const& title, std::string const& subtitle, ccColor3B accent,
                float hold = 1.6f, int z = 95);
    // Scale-pop a node in from nothing (elastic overshoot).
    void popIn(CCNode* node, float scale, float delay = 0.f, float time = .45f);
}
