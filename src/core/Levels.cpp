#include "Levels.hpp"

#include <algorithm>
#include <charconv>
#include <random>

using namespace geode::prelude;

namespace nt {
    std::vector<Difficulty> const& difficulties() {
        static std::vector<Difficulty> const list = {
            {0, 1, "Easy", 1, 0},
            {1, 2, "Normal", 2, 0},
            {2, 3, "Hard", 3, 0},
            {3, 4, "Harder", 4, 0},
            {4, 5, "Insane", 5, 0},
            {5, 7, "Easy Demon", -2, 1},
            {6, 8, "Medium Demon", -2, 2},
            {7, 6, "Hard Demon", -2, 3},
            {8, 9, "Insane Demon", -2, 4},
            {9, 10, "Extreme Demon", -2, 5},
        };
        return list;
    }

    std::vector<Difficulty> poolOptions(int pool) {
        std::vector<Difficulty> out;
        for (auto const& d : difficulties()) {
            if (pool & (1 << d.bit)) out.push_back(d);
        }
        return out;
    }

    int faceFor(GJGameLevel* level) {
        if (!level) return 0;
        if (level->m_autoLevel) return -1;
        if (level->m_demon.value() != 0) {
            switch (level->m_demonDifficulty) {
                case 3: return 7;
                case 4: return 8;
                case 5: return 9;
                case 6: return 10;
                default: return 6;
            }
        }
        return std::clamp(level->getAverageDifficulty(), 0, 5);
    }

    LevelRef refFor(GJGameLevel* level) {
        LevelRef ref;
        if (!level) return ref;
        ref.id = level->m_levelID.value();
        ref.name = compat::str(level->m_levelName);
        ref.author = compat::str(level->m_creatorName);
        ref.face = faceFor(level);
        return ref;
    }

    // ---------------------------------------------------------------- LevelRoller
    // The search follows the approach of the Versus mod's random maps (MIT, Tipp7):
    // featured + rated, one difficulty at a time, first page for the total, then a random page.

    namespace {
        std::mt19937& rng() {
            static std::mt19937 r(static_cast<unsigned>(std::random_device {}()));
            return r;
        }
    }

    LevelRoller* LevelRoller::create() {
        auto* ret = new LevelRoller();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    LevelRoller::~LevelRoller() {
        auto* glm = GameLevelManager::sharedState();
        if (glm && glm->m_levelManagerDelegate == this) glm->m_levelManagerDelegate = nullptr;
    }

    void LevelRoller::roll(int pool, Done done) {
        m_done = std::move(done);
        m_options = poolOptions(pool);
        std::shuffle(m_options.begin(), m_options.end(), rng());
        m_option = 0;
        m_total = -1;
        m_requests = 0;
        m_timer = 0.f;
        m_wait = 0.f;
        m_awaitTotal = false;
        m_firstPage.clear();
        if (m_options.empty()) {
            this->finish({}, "No difficulties are selected for this tournament.");
            return;
        }
        this->scheduleUpdate();
        this->request(0);
    }

    void LevelRoller::request(int page) {
        if (!m_done || m_option >= m_options.size()) return;
        auto const& d = m_options[m_option];
        m_search = GJSearchObject::create(SearchType::MostLiked);
        m_search->m_page = page;
        m_search->m_starFilter = true;
        m_search->m_featuredFilter = true;
        m_search->m_difficulty = std::to_string(d.gdDiff);
        m_search->m_demonFilter = static_cast<GJDifficulty>(d.demon);
        m_search->m_length = std::string("1,2,3,4");
        m_key = m_search->getKey();
        m_pending = true;
        m_wait = 0.f;
        ++m_requests;
        auto* glm = GameLevelManager::sharedState();
        glm->m_levelManagerDelegate = this;
        glm->getOnlineLevels(m_search);
    }

    void LevelRoller::nextOption() {
        m_pending = false;
        m_awaitTotal = false;
        m_firstPage.clear();
        ++m_option;
        m_total = -1;
        m_requests = 0;
        if (m_option >= m_options.size()) {
            this->finish({}, "Couldn't find a featured level for these difficulties. Try again!");
            return;
        }
        this->request(0);
    }

    void LevelRoller::finish(LevelRef level, std::string error) {
        m_pending = false;
        m_awaitTotal = false;
        this->unscheduleUpdate();
        auto* glm = GameLevelManager::sharedState();
        if (glm && glm->m_levelManagerDelegate == this) glm->m_levelManagerDelegate = nullptr;
        auto done = std::move(m_done);
        m_done = nullptr;
        if (done) done(std::move(level), std::move(error));
    }

    void LevelRoller::update(float dt) {
        if (!m_done) return;
        m_timer += dt;
        m_wait += dt;
        if (m_timer > 25.f) {
            this->finish({}, "GD's level servers didn't answer. Try again!");
            return;
        }
        if (m_awaitTotal && m_wait > .4f) this->pickFirstPage();
        else if (m_pending && m_wait > 8.f) this->nextOption();
    }

    void LevelRoller::setupPageInfo(compat::GdStr info, char const* key) {
        if (!key || m_key != key || (!m_pending && !m_awaitTotal)) return;
        std::string text = compat::str(info);
        int total = 0;
        auto end = text.find(':');
        std::from_chars(text.data(), text.data() + (end == std::string::npos ? text.size() : end), total);
        if (total > 0) m_total = total;
        // the first page's levels may have arrived before its page info
        if (m_awaitTotal) this->pickFirstPage();
    }

    std::vector<LevelRef> LevelRoller::usable(cocos2d::CCArray* levels) {
        std::vector<LevelRef> out;
        if (!levels) return out;
        for (auto* obj : CCArrayExt<CCObject*>(levels)) {
            auto* level = typeinfo_cast<GJGameLevel*>(obj);
            if (!level || level->m_levelID.value() <= 0) continue;
            if (level->isPlatformer() || level->m_twoPlayerMode) continue;
            out.push_back(refFor(level));
        }
        return out;
    }

    void LevelRoller::pickFrom(std::vector<LevelRef> const& picks) {
        if (picks.empty()) {
            this->nextOption();
            return;
        }
        this->finish(picks[std::uniform_int_distribution<size_t>(0, picks.size() - 1)(rng())], "");
    }

    void LevelRoller::pickFirstPage() {
        m_awaitTotal = false;
        // the first page tells us how many there are: jump to a random page
        if (m_total > 10) {
            int pages = std::min(m_total - 1, 999) / 10;
            int page = std::uniform_int_distribution<int>(0, pages)(rng());
            if (page > 0) {
                this->request(page);
                return;
            }
        }
        this->pickFrom(m_firstPage);
    }

    void LevelRoller::loadLevelsFinished(cocos2d::CCArray* levels, char const* key) {
        if (!m_pending || !key || m_key != key) return;
        m_pending = false;
        if (m_requests == 1) {
            m_firstPage = this->usable(levels);
            if (m_total < 0) {
                // wait a moment for the page info (it can come right after the levels)
                m_awaitTotal = true;
                m_wait = 0.f;
                return;
            }
            this->pickFirstPage();
            return;
        }
        auto picks = this->usable(levels);
        // a random page that came back empty falls back to the first one
        this->pickFrom(picks.empty() ? m_firstPage : picks);
    }

    void LevelRoller::loadLevelsFinished(cocos2d::CCArray* levels, char const* key, int) {
        this->loadLevelsFinished(levels, key);
    }

    void LevelRoller::loadLevelsFailed(char const* key) {
        if (!m_pending || !key || m_key != key) return;
        m_pending = false;
        // a random page failing still leaves the first page to pick from
        if (m_requests > 1 && !m_firstPage.empty()) this->pickFrom(m_firstPage);
        else this->nextOption();
    }

    void LevelRoller::loadLevelsFailed(char const* key, int) {
        this->loadLevelsFailed(key);
    }
}
