#pragma once

// A player's GD icon with a name tag, a shadow and all its little performances
// (dropping in, waving, emoting, hopping, getting knocked out).
// The node's position is the point between the icon's feet.

#include <Geode/Geode.hpp>
#include <functional>
#include "../core/Model.hpp"

namespace nt::ui {
    using namespace cocos2d;

    class Avatar : public CCNode {
    public:
        static Avatar* create(Player const& player, float scale, bool nameTag = true);

        int account() const { return m_player.account; }
        Player const& player() const { return m_player; }
        void setPlayer(Player const& player);
        float iconScale() const { return m_scale; }
        float height() const { return 30.f * m_scale; }
        // the top of the head, in parent coordinates
        CCPoint headWorld();

        void setNameVisible(bool visible);
        void setNameScale(float scale);
        void setDimmed(bool dimmed, bool animate);
        bool dimmed() const { return m_dimmed; }
        void setYou(bool you);
        void setIdle(bool idle);
        // the little random hops and waves an idle avatar does on its own (off for players
        // who walk around themselves, so it only ever moves the way they move it)
        void setAutoHop(bool on) { m_autoHop = on; }
        void setScaleSmooth(float scale, float time);
        // a little stepping bounce while walking around the lobby
        void setWalking(bool walking, int dir = 0);
        bool onGround() const;

        // poof in the sky, fall, land with a squash, then wave hello
        void dropIn(float fromHeight, float delay, std::function<void()> done = nullptr);
        void wave(bool withBubble = true);
        void emote(int id, float hold = 1.9f);
        void hop(float height = 14.f, float time = .38f);
        void squash(float amount = 1.f);
        // jump away in a puff of smoke, then remove itself
        void leaveAndRemove();
        // kicked: launched off the screen spinning
        void yeet(bool right);
        // knocked out in a duel: burst, then lie there greyed out with an X
        void knockOut();
        // happy little double hop
        void celebrate();

        // --- the sword (bracket duels). Angles: 0 = pointing up, positive = forward.
        static constexpr float kSwordRest = 32.f;
        void setSword(bool on, int facing);
        // the sword appears in the hand with a flourish (when the bracket is set)
        void swordDraw(int facing, float delay = 0.f);
        // struck by a blow: rock back, squash, then settle
        void flinch();
        bool hasSword() const { return m_sword != nullptr; }
        int facing() const { return m_facing; }
        void setFacing(int facing);
        void setSwordAngle(float deg);
        void swordTo(float deg, float time);
        // where the sword hand is, relative to where it rests at the front of the cube
        // (icon units; x is forward). Raised behind the head for big swings.
        void setHandOffset(CCPoint offset);
        void handTo(CCPoint offset, float time);
        // tilt the whole body around the feet: positive leans forward, negative back
        void setLean(float deg);
        void leanTo(float deg, float time);
        void swordDrop();
        void swordRaise();
        // back to the resting grip: hand down, blade tilted forward, standing straight
        void swordRest(float time);
        CCPoint handWorld();
        CCPoint swordTipWorld();

        bool busy = false;  // the stage won't move it while it performs

    protected:
        bool init(Player const& player, float scale, bool nameTag);
        void applyIcon();
        void randomHop(float);
        void scheduleRandomHop();

        Player m_player;
        float m_scale = 1.f;
        CCNode* m_body = nullptr;       // squash & stretch pivot (the feet), jumps
        CCNode* m_bob = nullptr;        // idle breathing offset
        SimplePlayer* m_icon = nullptr; // spins around its own centre
        CCSprite* m_shadow = nullptr;
        CCLabelBMFont* m_name = nullptr;
        CCNode* m_bubble = nullptr;
        CCSprite* m_ring = nullptr;
        CCSprite* m_mark = nullptr;
        CCNode* m_hand = nullptr;
        CCSprite* m_sword = nullptr;
        CCPoint m_handOffset {0.f, 0.f};
        float m_lean = 0.f;
        CCPoint handSpot() const;
        int m_facing = 1;
        bool m_dimmed = false;
        bool m_idle = false;
        bool m_you = false;
        bool m_walking = false;
        int m_walkDir = 0;
        bool m_autoHop = true;
        float m_nameScale = .3f;
    };
}
