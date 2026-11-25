#pragma once

// Everything about "my" current match, alive across scene changes:
// level download, launching the level, progress reports, the opponent's progress,
// the result, and getting back to the tournament screen afterwards.

#include <Geode/Geode.hpp>
#include "Api.hpp"
#include "Model.hpp"

namespace nt {
    struct MatchInfo {
        std::string code;
        std::string tournamentName;
        int round = 0;
        int slot = 0;
        int rounds = 0;
        Player me;
        Player opp;
        bool meIsP1 = true;
        LevelRef level;
        int64_t startsAt = 0;
        int64_t endsAt = 0;
    };

    class MatchController : public cocos2d::CCNode, public LevelDownloadDelegate, public MusicDownloadDelegate {
    public:
        static MatchController* get();

        enum class Prep { Idle, Downloading, Songs, Ready, Failed };

        bool active() const { return m_active; }
        MatchInfo const& info() const { return m_info; }
        bool isMatch(std::string const& code, int round, int slot) const;
        // Takes over a match from the tournament state (keeps progress if it's the same match).
        void begin(State const& state, Match const& match, int myAccount);
        // Keeps times/result in sync from tournament polls.
        void sync(Match const& match);
        void clear();

        bool live() const { return m_state == "live"; }
        bool done() const { return m_state == "done"; }
        int winner() const { return m_winner; }
        std::string const& why() const { return m_why; }
        bool iWon() const { return done() && m_winner == m_info.me.account; }
        int secondsLeft() const;
        int secondsToStart() const;

        Prep prep() const { return m_prep; }
        std::string const& prepError() const { return m_prepError; }
        int prepPercent() const;
        void prefetch(int64_t levelId);
        void retryPrep();

        // Into the level now (if it's ready). Returns false if not ready yet.
        bool launch();
        bool launchedOnce() const { return m_launchedOnce; }
        bool owns(PlayLayer* layer) const;
        bool inLevel() const;
        void leftLevel();
        bool wantsReturn() const;
        void returnToTournament();

        // from the PlayLayer hooks
        void onFrame(float percent, int attempts, bool counts);
        void onComplete();

        void sendEmote(int id);
        std::vector<int> takeOpponentEmotes();

        // Background watch: notices my match starting while I'm on another screen.
        void watch(std::string const& code) { m_watchCode = code; }

        float myBest = 0.f, myCur = 0.f;
        int myAtt = 0;
        bool myDone = false;
        float oppBest = 0.f, oppCur = 0.f;
        int oppAtt = 0;
        bool oppSeen = false;

    protected:
        bool init() override;
        void tick(float dt);
        void sendProgress();
        void onReply(matjson::Value const& v, int64_t sent);
        void checkResources();
        void setPrep(Prep prep, std::string error = "");
        void backgroundWatch(float dt);

        void levelDownloadFinished(GJGameLevel* level) override;
        void levelDownloadFailed(int response) override;
        void downloadSongFinished(int id) override;
        void downloadSongFailed(int id, GJSongError error) override;
        void downloadSFXFinished(int id) override;
        void downloadSFXFailed(int id, GJSongError error) override;

        bool m_active = false;
        MatchInfo m_info;
        std::string m_state = "pending";
        int m_winner = 0;
        std::string m_why;

        Prep m_prep = Prep::Idle;
        std::string m_prepError;
        int64_t m_prepLevel = 0;
        geode::Ref<GJGameLevel> m_level;
        std::vector<int> m_songs;
        std::vector<int> m_effects;
        bool m_musicWatching = false;
        float m_prepTimer = 0.f;

        geode::WeakRef<PlayLayer> m_play;
        bool m_launched = false;
        bool m_launchedOnce = false;

        api::Holder m_progressReq;
        bool m_inFlight = false;
        float m_sendTimer = 0.f;
        int64_t m_lastEmoteEv = 0;
        std::vector<int> m_oppEmotes;

        std::string m_watchCode;
        float m_watchTimer = 0.f;
        bool m_watchInFlight = false;
        api::Holder m_watchReq;
    };
}
