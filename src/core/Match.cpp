#include "Match.hpp"
#include "Compat.hpp"
#include "Config.hpp"
#include "Session.hpp"
#include "../ui/TournamentLayer.hpp"
#include "../ui/VsLayer.hpp"

#include <sstream>

using namespace geode::prelude;

namespace nt {
    namespace {
        std::vector<int> parseIds(std::string const& csv) {
            std::vector<int> out;
            std::stringstream ss(csv);
            std::string part;
            while (std::getline(ss, part, ',')) {
                try {
                    int id = std::stoi(part);
                    if (id > 0) out.push_back(id);
                } catch (...) {
                }
            }
            return out;
        }

        template <class T>
        T* runningLayer() {
            auto* scene = CCDirector::get()->getRunningScene();
            if (!scene) return nullptr;
            return scene->getChildByType<T>(0);
        }
    }

    MatchController* MatchController::get() {
        static MatchController* s_instance = nullptr;
        if (!s_instance) {
            s_instance = new MatchController();
            s_instance->init();
        }
        return s_instance;
    }

    bool MatchController::init() {
        if (!CCNode::init()) return false;
        CCDirector::get()->getScheduler()->scheduleSelector(schedule_selector(MatchController::tick), this, 0.f, false);
        return true;
    }

    bool MatchController::isMatch(std::string const& code, int round, int slot) const {
        return m_active && m_info.code == code && m_info.round == round && m_info.slot == slot;
    }

    void MatchController::begin(State const& state, Match const& match, int myAccount) {
        bool same = this->isMatch(state.t.code, match.round, match.slot);
        m_active = true;
        m_info.code = state.t.code;
        m_info.tournamentName = state.t.name;
        m_info.round = match.round;
        m_info.slot = match.slot;
        m_info.rounds = state.t.rounds;
        m_info.meIsP1 = match.p1 == myAccount;
        if (auto* p = state.player(myAccount)) m_info.me = *p;
        int oppId = match.other(myAccount);
        if (auto* p = state.player(oppId)) {
            m_info.opp = *p;
        } else {
            m_info.opp = Player {};
            m_info.opp.account = oppId;
            m_info.opp.name = "???";
        }
        m_info.level = state.t.level;
        if (match.level && match.level != m_info.level.id) {
            m_info.level = LevelRef {};
            m_info.level.id = match.level;
            m_info.level.name = match.levelName;
        }
        m_info.startsAt = match.startsAt;
        m_info.endsAt = match.endsAt;
        if (!same) {
            m_state = match.state;
            m_winner = match.winner;
            m_why = match.why;
            myBest = myCur = 0.f;
            myAtt = 0;
            myDone = false;
            oppBest = oppCur = 0.f;
            oppAtt = 0;
            oppSeen = false;
            m_launched = false;
            m_launchedOnce = false;
            m_oppEmotes.clear();
            m_lastEmoteEv = state.evLast;
        }
        this->sync(match);
        this->prefetch(m_info.level.id);
    }

    void MatchController::sync(Match const& match) {
        if (!m_active || match.round != m_info.round || match.slot != m_info.slot) return;
        m_info.startsAt = match.startsAt;
        m_info.endsAt = match.endsAt;
        if (m_state != "done") {
            m_state = match.state;
            m_winner = match.winner;
            m_why = match.why;
        }
        float ob = m_info.meIsP1 ? match.b2 : match.b1;
        float oc = m_info.meIsP1 ? match.c2 : match.c1;
        int oa = m_info.meIsP1 ? match.a2 : match.a1;
        oppBest = std::max(oppBest, ob);
        if (oa > 0 || oc > 0.f) {
            oppCur = oc;
            oppAtt = std::max(oppAtt, oa);
        }
    }

    void MatchController::clear() {
        m_active = false;
        m_info = MatchInfo {};
        m_state = "pending";
        m_winner = 0;
        m_why.clear();
        m_launched = false;
    }

    int MatchController::secondsLeft() const {
        if (!m_info.endsAt) return 0;
        return static_cast<int>(std::max<int64_t>(0, (m_info.endsAt - serverNowMs() + 999) / 1000));
    }

    int MatchController::secondsToStart() const {
        if (!m_info.startsAt) return 0;
        return static_cast<int>((m_info.startsAt - serverNowMs() + 999) / 1000);
    }

    // ---------------------------------------------------------------- level & songs

    int MatchController::prepPercent() const {
        switch (m_prep) {
            case Prep::Ready: return 100;
            case Prep::Songs: {
                auto* music = MusicDownloadManager::sharedState();
                int total = 0, have = 0;
                for (int id : m_songs) {
                    ++total;
                    if (music->isSongDownloaded(id)) ++have;
                }
                for (int id : m_effects) {
                    ++total;
                    if (music->isSFXDownloaded(id)) ++have;
                }
                return total ? 50 + 50 * have / total : 95;
            }
            case Prep::Downloading: return 25;
            default: return 0;
        }
    }

    void MatchController::setPrep(Prep prep, std::string error) {
        m_prep = prep;
        m_prepError = std::move(error);
        m_prepTimer = 0.f;
        if (prep != Prep::Songs && m_musicWatching) {
            MusicDownloadManager::sharedState()->removeMusicDownloadDelegate(this);
            m_musicWatching = false;
        }
        auto* glm = GameLevelManager::sharedState();
        if (prep != Prep::Downloading && glm->m_levelDownloadDelegate == this) glm->m_levelDownloadDelegate = nullptr;
    }

    void MatchController::prefetch(int64_t levelId) {
        if (levelId <= 0) return;
        if (m_prepLevel == levelId && (m_prep == Prep::Downloading || m_prep == Prep::Songs || m_prep == Prep::Ready)) {
            return;
        }
        m_prepLevel = levelId;
        m_level = nullptr;
        auto* glm = GameLevelManager::sharedState();
        auto* saved = glm->getSavedLevel(static_cast<int>(levelId));
        if (saved && !compat::str(saved->m_levelString).empty()) {
            this->levelDownloadFinished(saved);
            return;
        }
        this->setPrep(Prep::Downloading);
        glm->m_levelDownloadDelegate = this;
        glm->downloadLevel(static_cast<int>(levelId), false, 0);
    }

    void MatchController::retryPrep() {
        int64_t id = m_prepLevel;
        m_prepLevel = 0;
        m_prep = Prep::Idle;
        this->prefetch(id);
    }

    void MatchController::levelDownloadFinished(GJGameLevel* level) {
        if (!level || level->m_levelID.value() != m_prepLevel) return;
        if (compat::str(level->m_levelString).empty()) {
            this->setPrep(Prep::Failed, "This level has no data.");
            return;
        }
        m_level = level;
        m_songs.clear();
        m_effects.clear();
        if (level->m_songID > 0) m_songs.push_back(level->m_songID);
        for (int id : parseIds(compat::str(level->m_songIDs))) {
            if (std::find(m_songs.begin(), m_songs.end(), id) == m_songs.end()) m_songs.push_back(id);
        }
        m_effects = parseIds(compat::str(level->m_sfxIDs));
        this->setPrep(Prep::Songs);
        auto* music = MusicDownloadManager::sharedState();
        music->addMusicDownloadDelegate(this);
        m_musicWatching = true;
        this->checkResources();
    }

    void MatchController::levelDownloadFailed(int) {
        if (m_prep != Prep::Downloading) return;
        this->setPrep(Prep::Failed, "Couldn't download the level.");
    }

    void MatchController::checkResources() {
        if (m_prep != Prep::Songs) return;
        auto* music = MusicDownloadManager::sharedState();
        bool needsUrl = false;
        for (int id : m_songs) needsUrl |= id > 10000000 && !music->isSongDownloaded(id);
        for (int id : m_effects) needsUrl |= !music->isSFXDownloaded(id);
        if (needsUrl && compat::str(music->m_customContentURL).empty()) {
            music->getCustomContentURL();
            return;
        }
        bool ready = true;
        int running = 0;
        for (int id : m_songs) {
            if (music->isSongDownloaded(id)) continue;
            ready = false;
            if (music->isRunningActionForSongID(id)) {
                ++running;
                continue;
            }
            if (running < 3) {
                music->downloadSong(id);
                ++running;
            }
        }
        for (int id : m_effects) {
            if (music->isSFXDownloaded(id)) continue;
            ready = false;
            if (music->isDLActive(music->getSFXDownloadKey(id))) {
                ++running;
                continue;
            }
            if (running < 3) {
                music->downloadSFX(id);
                ++running;
            }
        }
        if (ready) this->setPrep(Prep::Ready);
    }

    void MatchController::downloadSongFinished(int) { this->checkResources(); }
    void MatchController::downloadSFXFinished(int) { this->checkResources(); }
    void MatchController::downloadSongFailed(int id, GJSongError) {
        if (std::find(m_songs.begin(), m_songs.end(), id) == m_songs.end()) return;
        // play without the song rather than miss the match
        log::warn("Song {} failed to download; the match plays without it", id);
        std::erase(m_songs, id);
        this->checkResources();
    }
    void MatchController::downloadSFXFailed(int id, GJSongError) {
        std::erase(m_effects, id);
        this->checkResources();
    }

    // ---------------------------------------------------------------- the level

    bool MatchController::launch() {
        if (!m_active || m_prep != Prep::Ready || !m_level) return false;
        auto* scene = compat::playScene(m_level);
        if (!scene) return false;
        m_play = WeakRef<PlayLayer>(scene->getChildByType<PlayLayer>(0));
        m_launched = true;
        m_launchedOnce = true;
        CCDirector::get()->replaceScene(CCTransitionFade::create(.4f, scene));
        return true;
    }

    bool MatchController::owns(PlayLayer* layer) const {
        if (!m_active || !layer) return false;
        auto mine = m_play.lock();
        return mine && mine.data() == layer;
    }

    bool MatchController::inLevel() const {
        auto mine = m_play.lock();
        return mine && mine.data() == PlayLayer::get();
    }

    void MatchController::leftLevel() {
        m_play = WeakRef<PlayLayer>();
    }

    bool MatchController::wantsReturn() const {
        return m_active && m_launched;
    }

    void MatchController::returnToTournament() {
        m_play = WeakRef<PlayLayer>();
        m_launched = false;
        auto code = m_info.code;
        CCDirector::get()->replaceScene(CCTransitionFade::create(.45f, ui::TournamentLayer::scene(code)));
        GameManager::sharedState()->fadeInMenuMusic();
    }

    void MatchController::onFrame(float percent, int attempts, bool counts) {
        if (!m_active) return;
        myAtt = std::max(myAtt, attempts);
        if (!counts || !live() || myDone) return;
        myCur = std::clamp(percent, 0.f, 100.f);
        myBest = std::max(myBest, myCur);
    }

    void MatchController::onComplete() {
        if (!m_active || !live() || myDone) return;
        myDone = true;
        myCur = myBest = 100.f;
        m_sendTimer = 999.f;  // report right away
    }

    void MatchController::sendEmote(int id) {
        if (!m_active) return;
        auto args = matjson::makeObject({{"code", m_info.code}, {"emote", id}});
        api::fire("nt_emote", withAuth(args));
    }

    std::vector<int> MatchController::takeOpponentEmotes() {
        auto out = std::move(m_oppEmotes);
        m_oppEmotes.clear();
        return out;
    }

    // ---------------------------------------------------------------- reporting

    void MatchController::tick(float dt) {
        dt = std::min(dt, .25f);
        if (m_prep == Prep::Songs) {
            m_prepTimer += dt;
            if (m_prepTimer > 1.f) {
                m_prepTimer = 0.f;
                this->checkResources();
            }
        } else if (m_prep == Prep::Downloading) {
            m_prepTimer += dt;
            // another layer may have taken the download delegate: check the saved level now and then
            if (m_prepTimer > 2.f) {
                m_prepTimer = 0.f;
                auto* glm = GameLevelManager::sharedState();
                auto* saved = glm->getSavedLevel(static_cast<int>(m_prepLevel));
                if (saved && !compat::str(saved->m_levelString).empty()) this->levelDownloadFinished(saved);
                else if (glm->m_levelDownloadDelegate != this) {
                    glm->m_levelDownloadDelegate = this;
                    glm->downloadLevel(static_cast<int>(m_prepLevel), false, 0);
                }
            }
        }

        if (m_active && !m_info.code.empty() && !done()) {
            int64_t now = serverNowMs();
            bool window = m_info.startsAt && now > m_info.startsAt - 6000 && now < m_info.endsAt + 8000;
            m_sendTimer += dt;
            float interval = this->inLevel() ? kReportInterval : 2.f;
            if (window && !m_inFlight && m_sendTimer >= interval) {
                m_sendTimer = 0.f;
                this->sendProgress();
            }
        }

        this->backgroundWatch(dt);
    }

    void MatchController::sendProgress() {
        auto args = matjson::makeObject({
            {"code", m_info.code},
            {"round", m_info.round},
            {"slot", m_info.slot},
            {"cur", static_cast<double>(myCur)},
            {"best", static_cast<double>(myBest)},
            {"att", myAtt},
            {"done", myDone},
            {"since", m_lastEmoteEv},
        });
        m_inFlight = true;
        int64_t sent = api::localMs();
        api::call(m_progressReq, "nt_progress", withAuth(args), [this, sent](api::Reply reply) {
            m_inFlight = false;
            if (reply.isOk()) this->onReply(reply.unwrap(), sent);
            else if (reply.unwrapErr() == "auth" || reply.unwrapErr() == "notfound") this->clear();
        });
    }

    void MatchController::onReply(matjson::Value const& v, int64_t sent) {
        noteServerTime(jlong(v, "now"), sent, api::localMs());
        if (jlong(v, "ea")) m_info.endsAt = jlong(v, "ea");
        if (jlong(v, "sa")) m_info.startsAt = jlong(v, "sa");
        if (jhas(v, "opp")) {
            auto const& o = v["opp"];
            oppBest = std::max(oppBest, static_cast<float>(jnum(o, "b")));
            oppCur = static_cast<float>(jnum(o, "c"));
            oppAtt = std::max(oppAtt, jint(o, "a"));
            oppSeen = oppSeen || oppAtt > 0 || oppBest > 0.f;
        }
        if (jhas(v, "ev") && v["ev"].isArray()) {
            auto const& arr = v["ev"];
            for (size_t i = 0; i < arr.size(); ++i) {
                auto const& e = arr[i];
                int64_t id = jlong(e, "id");
                if (id <= m_lastEmoteEv) continue;
                m_lastEmoteEv = id;
                if (jstr(e, "k") == "emote" && jint(e, "a") == m_info.opp.account && jhas(e, "d")) {
                    m_oppEmotes.push_back(jint(e["d"], "e", 1));
                }
            }
        }
        if (jhas(v, "evLast")) m_lastEmoteEv = std::max(m_lastEmoteEv, jlong(v, "evLast"));
        auto st = jstr(v, "st", m_state);
        if (st == "done" && m_state != "done") {
            m_winner = jint(v, "w");
            m_why = jstr(v, "why");
        }
        m_state = st;
    }

    // ---------------------------------------------------------------- background watch

    void MatchController::backgroundWatch(float dt) {
        if (m_watchCode.empty()) return;
        m_watchTimer += dt;
        if (m_watchTimer < kPollBackground || m_watchInFlight) return;
        m_watchTimer = 0.f;

        auto* scene = CCDirector::get()->getRunningScene();
        if (!scene || typeinfo_cast<CCTransitionScene*>(scene)) return;
        // these screens handle the match themselves
        if (runningLayer<ui::TournamentLayer>() || runningLayer<ui::VsLayer>()) return;
        if (this->inLevel()) return;

        auto self = me();
        if (!self.loggedIn()) return;
        m_watchInFlight = true;
        int64_t sent = api::localMs();
        auto code = m_watchCode;
        api::call(m_watchReq, "nt_state", withAuth(matjson::makeObject({{"code", code}, {"since", -1}})),
            [this, code, sent](api::Reply reply) {
                m_watchInFlight = false;
                if (reply.isErr()) {
                    if (reply.unwrapErr() == "notfound") m_watchCode.clear();
                    return;
                }
                auto v = reply.unwrap();
                noteServerTime(jlong(v, "now"), sent, api::localMs());
                State st;
                st.apply(v);
                if (!st.t.running()) {
                    if (st.t.finished()) m_watchCode.clear();
                    return;
                }
                int myId = me().account;
                auto* p = st.player(myId);
                if (!p || p->status != "in") {
                    m_watchCode.clear();
                    return;
                }
                auto* m = st.currentMatch(myId);
                if (!m || !m->live()) return;
                if (this->isMatch(code, m->round, m->slot) && m_launched) return;
                // never yank someone out of a level or the editor; just tell them
                if (PlayLayer::get() || LevelEditorLayer::get()) {
                    Notification::create("Your tournament match is starting! Open Nater's Tournament.",
                                         NotificationIcon::Warning, 4.f)->show();
                    return;
                }
                this->begin(st, *m, myId);
                CCDirector::get()->pushScene(CCTransitionFade::create(.4f, ui::VsLayer::scene()));
            });
    }
}
