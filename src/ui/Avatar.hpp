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
        CCPoint headWorld() const;

        void setNameVisible(bool visible);
        void setNameScale(float scale);
        void setDimmed(bool dimmed, bool animate);
        bool dimmed() const { return m_dimmed; }
        void setYou(bool you);
        void setIdle(bool idle);
        void setScaleSmooth(float scale, float time);

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
        bool m_dimmed = false;
        bool m_idle = false;
        bool m_you = false;
        float m_nameScale = .3f;
    };
}
