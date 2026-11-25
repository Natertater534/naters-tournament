#pragma once

// The live stage of the tournament screen: the lobby crowd standing on the ground,
// then the clan-war bracket map with clashing icons, knockouts and winners marching on.

#include <Geode/Geode.hpp>
#include <functional>
#include <map>
#include <set>
#include "../core/Model.hpp"
#include "Avatar.hpp"

namespace nt::ui {
    using namespace cocos2d;

    class Clash;

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

        // returns the account under `p` (stage coordinates), or 0
        int avatarAt(CCPoint p) const;
        // returns true and the round/slot of the match card under `p`
        bool matchAt(CCPoint p, int& round, int& slot) const;
        CCPoint avatarPos(int account) const;
        bool bracketMode() const { return m_bracket; }

    protected:
        bool init(CCSize size, float groundY);
        void update(float dt) override;

        // lobby
        void layoutLobby(State const& state, bool animate);
        CCPoint lobbySpot(int index, int count, float& scale) const;

        // bracket
        struct Seat {
            CCPoint pos;
            int round = 0, slot = 0, side = 0;  // side: -1 left, 1 right, 0 final
        };
        void buildBracket(State const& state);
        void layoutBracket(State const& state, bool animate);
        CCPoint cardCentre(int round, int slot) const;
        int sideOf(int round, int slot) const;
        CCPoint seatPos(int round, int slot, int which) const;  // which: 1 = p1, 2 = p2
        std::vector<CCPoint> travelPath(int round, int slot, int which) const;
        void drawLines(State const& state);
        void refreshCards(State const& state);
        float bracketScale() const;
        void startClash(int round, int slot, Avatar* a, Avatar* b);
        void stopClash(int round, int slot);
        Avatar* avatarFor(State const& state, Player const& p, float scale);
        void trail(Avatar* a, ccColor3B color, float life);
        void march(Avatar* a, std::vector<CCPoint> const& path, float scale, std::function<void()> done);

        CCSize m_size;
        float m_groundY = 60.f;
        int m_me = 0;
        bool m_bracket = false;
        int m_size2 = 0;  // bracket size (power of two)
        int m_rounds = 0;
        std::map<int, Avatar*> m_avatars;     // account -> avatar
        std::map<std::pair<int, int>, Clash*> m_clashes;
        std::map<std::pair<int, int>, CCNode*> m_cards;
        std::map<std::pair<int, int>, std::pair<CCLabelBMFont*, CCLabelBMFont*>> m_pct;
        std::set<std::pair<int, int>> m_doneShown;  // results already animated
        CCNode* m_lobbyDeco = nullptr;
        CCNode* m_bracketRoot = nullptr;
        CCDrawNode* m_lines = nullptr;
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
        float m_resultDelay = 0.f;
    };
}
