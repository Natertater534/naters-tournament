#pragma once

// The live stage of the tournament screen: the lobby crowd you can walk around in,
// then the clan-war bracket map where matched players duel with swords, the winner
// lands the final blow and marches along the lit path to the next round.

#include <Geode/Geode.hpp>
#include <functional>
#include <map>
#include <set>
#include "../core/Model.hpp"
#include "Avatar.hpp"

namespace nt::ui {
    using namespace cocos2d;

    class Duel;

    class Arena : public CCNode {
    public:
        static Arena* create(CCSize size, float groundY);

        void setMe(int account) { m_me = account; }
        // Brings every avatar and bracket piece in line with `state` (no fanfare).
        void sync(State const& state);
        void tickLive(State const& state);

        void onJoin(State const& state, int account);
        void onLeave(State const& state, int account);
        void onKick(State const& state, int account);
        void onEmote(int account, int emote);
        void onStart(State const& state);
        void onRound(State const& state, int round);
        void onResult(State const& state, Event const& e);
        void onChampion(State const& state, int account);

        // --- lobby movement (only while the tournament is in its lobby)
        void steer(int dir);         // -1 left, 0 stop, 1 right
        void jumpMe();
        void walkMeTo(float x);      // stage x
        bool canMove() const;
        bool myMoved() const { return m_myMoved; }
        void clearMyMoved() { m_myMoved = false; }
        float myX01() const;
        int myJumps() const { return m_myJumps; }

        // returns the account under `p` (stage coordinates), or 0
        int avatarAt(CCPoint p) const;
        // returns true and the round/slot of the match under `p`
        bool matchAt(CCPoint p, int& round, int& slot) const;
        CCPoint avatarPos(int account) const;
        bool bracketMode() const { return m_bracket; }
        float groundY() const { return m_groundY; }

    protected:
        bool init(CCSize size, float groundY);
        void update(float dt) override;

        // lobby
        void layoutLobby(State const& state, bool animate);
        CCPoint lobbySpot(int index, int count, float& scale) const;
        float lobbyX(float x01) const;
        float frontScale(int count) const;
        // fresh: avatars created just now, placed straight at their spot instead of walking there
        void applyPositions(State const& state, std::set<int> const& fresh);
        void freeWalker(int account, Avatar* a);
        void updateWalkers(float dt);

        // bracket
        void buildBracket(State const& state);
        void layoutBracket(State const& state, bool animate);
        CCPoint matchCentre(int round, int slot) const;
        float feetY(int round, int slot) const;
        int sideOf(int round, int slot) const;
        CCPoint seatPos(int round, int slot, int which) const;  // which: 1 = p1 (left, faces right), 2 = p2
        // the drawn path from a match to the next one (start .. end), at the players' feet
        std::vector<CCPoint> linePath(int round, int slot) const;
        void drawLines(State const& state);
        void refreshMatches(State const& state);
        float bracketScale() const;
        float halfGap() const;
        float reach() const;
        float padWidth() const;
        void startDuel(int round, int slot, Avatar* a, Avatar* b);
        void stopDuel(int round, int slot);
        Avatar* avatarFor(Player const& p, float scale);
        void trail(Avatar* a, ccColor3B color, float life);
        void march(Avatar* a, int round, int slot, int which, std::function<void()> done);
        bool inTheWay(Avatar* self, float x0, float x1, float y) const;
        void updateMarches(float dt);

        CCSize m_size;
        float m_groundY = 60.f;
        int m_me = 0;
        bool m_bracket = false;
        int m_size2 = 0;  // bracket size (power of two)
        int m_rounds = 0;
        State m_state;    // the latest state, for animations that finish later
        std::map<int, Avatar*> m_avatars;  // account -> avatar
        std::map<std::pair<int, int>, Duel*> m_duels;
        std::map<std::pair<int, int>, CCSprite*> m_glows;  // soft light under a live duel
        std::map<std::pair<int, int>, std::pair<CCLabelBMFont*, CCLabelBMFont*>> m_pct;
        std::set<std::pair<int, int>> m_doneShown;  // results already animated
        std::set<std::pair<int, int>> m_marching;   // lines a winner is walking right now
        std::set<std::pair<int, int>> m_matchKeys;
        CCNode* m_lobbyDeco = nullptr;
        CCNode* m_bracketRoot = nullptr;
        CCDrawNode* m_lines = nullptr;
        CCDrawNode* m_litTrail = nullptr;
        CCNode* m_ghosts = nullptr;
        CCNode* m_labels = nullptr;
        CCSprite* m_trophy = nullptr;
        CCSprite* m_rays = nullptr;
        CCNode* m_waiting = nullptr;
        struct TrailInfo {
            geode::WeakRef<Avatar> target;
            CCMotionStreak* streak;
            float life;
        };
        std::vector<TrailInfo> m_trails;
        struct MarchInfo {
            geode::WeakRef<Avatar> avatar;
            std::vector<CCPoint> pts;   // pts[0] is where it starts
            std::vector<float> hops;    // hop height on the way to pts[i]
            CCPoint litStart;               // where the bracket line starts
            size_t litFrom = 0, litTo = 0;  // the rest of the line, as indexes into pts
            size_t seg = 1;
            float segPos = 0.f;
            std::pair<int, int> key;
            std::function<void()> done;
        };
        std::vector<MarchInfo> m_marches;
        float m_resultDelay = 0.f;

        // lobby movement
        struct Walker {
            float target = -1.f;  // stage x to walk to, or -1
            int jumps = -1;       // the jump count last seen from the server
        };
        std::map<int, Walker> m_walkers;  // players who have moved at least once
        int m_dir = 0;
        float m_myTarget = -1.f;
        bool m_myMoved = false;
        int m_myJumps = 0;
        float m_jumpCooldown = 0.f;
    };
}
