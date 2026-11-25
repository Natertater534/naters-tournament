#include "TournamentLayer.hpp"
#include "Fx.hpp"
#include "Modals.hpp"
#include "VsLayer.hpp"
#include "../core/Compat.hpp"
#include "../core/Config.hpp"
#include "../core/Match.hpp"
#include "../core/Session.hpp"

using namespace geode::prelude;

namespace nt::ui {
    namespace {
        constexpr float kPollLobbyFast = .7f;  // the lobby polls a little faster so walking looks live
        constexpr float kMoveSendGap = .3f;    // at most this often we tell the server where we are

        void pulse(CCMenuItemSpriteExtra* btn) {
            auto* img = btn ? btn->getNormalImage() : nullptr;
            if (!img) return;
            float s = img->getScale();
            img->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(.7f, s * 1.07f)),
                CCEaseSineInOut::create(CCScaleTo::create(.7f, s)), nullptr)));
        }
    }

    CCScene* TournamentLayer::scene(std::string const& code, bool justJoined) {
        auto* scene = CCScene::create();
        scene->addChild(TournamentLayer::create(code, justJoined));
        return scene;
    }

    TournamentLayer* TournamentLayer::create(std::string const& code, bool justJoined) {
        auto* ret = new TournamentLayer();
        if (ret->init(code, justJoined)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool TournamentLayer::init(std::string const& code, bool justJoined) {
        if (!CCLayer::init()) return false;
        m_code = code;
        m_justJoined = justJoined;
        m_me = me().account;
        auto win = CCDirector::get()->getWinSize();

        m_backdrop = Backdrop::create(col::navy, true, 58.f);
        this->addChild(m_backdrop, -10);

        m_arena = Arena::create(win, 58.f);
        m_arena->setMe(m_me);
        this->addChild(m_arena, 0);

        m_roller = LevelRoller::create();
        this->addChild(m_roller);

        // top bar
        auto* bar = Plate::create({win.width - 16.f, 40.f}, col::navyDeep, .82f, col::gold, .55f, 10.f, 1.4f);
        bar->setPosition({8.f, win.height - 46.f});
        this->addChild(bar, 10);

        m_title = label("Loading...", "goldFont.fnt", .7f, col::white);
        m_title->setPosition({win.width / 2.f, win.height - 20.f});
        this->addChild(m_title, 11);
        m_sub = label("", "bigFont.fnt", .3f, col::dim);
        m_sub->setPosition({win.width / 2.f, win.height - 37.f});
        this->addChild(m_sub, 11);

        m_menu = CCMenu::create();
        m_menu->setPosition({0.f, 0.f});
        this->addChild(m_menu, 20);
        auto* back = frameButton("GJ_arrow_01_001.png", .6f, [this](auto) { this->onBack(); });
        back->setPosition({30.f, win.height - 26.f});
        m_menu->addChild(back);

        // the join code, big enough to read off a stream
        m_codeBadge = CCNode::create();
        auto* badge = Plate::create({104.f, 30.f}, col::plate, .95f, col::gold, .9f, 8.f, 1.4f);
        badge->setAnchorPoint({.5f, .5f});
        m_codeBadge->addChild(badge);
        auto* codeTitle = label("CODE", "bigFont.fnt", .22f, col::dim);
        codeTitle->setPosition({-32.f, 0.f});
        m_codeBadge->addChild(codeTitle);
        m_codeLabel = label(code, "goldFont.fnt", .62f, col::gold);
        m_codeLabel->setPosition({10.f, 1.f});
        m_codeBadge->addChild(m_codeLabel);
        auto* copyBtn = nodeButton(m_codeBadge, [this](auto) {
            utils::clipboard::write(m_code);
            Notification::create("Copied the code " + m_code, NotificationIcon::Success, 1.5f)->show();
        });
        copyBtn->setPosition({win.width - 66.f, win.height - 26.f});
        m_menu->addChild(copyBtn);

        m_bottom = CCMenu::create();
        m_bottom->setPosition({0.f, 0.f});
        this->addChild(m_bottom, 20);

        m_feed = CCNode::create();
        m_feed->setPosition({14.f, win.height - 64.f});
        this->addChild(m_feed, 12);

        m_loading = CCNode::create();
        auto* spinner = fx("ring");
        spinner->setScale(.5f);
        spinner->setColor(col::gold);
        spinner->runAction(CCRepeatForever::create(CCSequence::create(
            CCScaleTo::create(.5f, .6f), CCScaleTo::create(.5f, .45f), nullptr)));
        m_loading->addChild(spinner);
        m_loading->setPosition(win / 2.f);
        this->addChild(m_loading, 30);

        this->setKeypadEnabled(true);
        this->setKeyboardEnabled(true);
        this->setTouchEnabled(true);
        this->schedule(schedule_selector(TournamentLayer::poll), 0.f);
        m_pollTimer = 99.f;
        setLastCode(code);
        return true;
    }

    void TournamentLayer::onEnterTransitionDidFinish() {
        CCLayer::onEnterTransitionDidFinish();
        m_pollTimer = 99.f;
    }

    void TournamentLayer::onExit() {
        this->releaseKeys();
        CCLayer::onExit();
    }

    void TournamentLayer::keyBackClicked() {
        this->onBack();
    }

    void TournamentLayer::onBack() {
        if (m_busy) return;
        m_busy = true;
        this->releaseKeys();
        this->unschedule(schedule_selector(TournamentLayer::poll));
        if (m_state.t.running() && m_state.player(m_me)) MatchController::get()->watch(m_code);
        CCDirector::get()->popSceneWithTransition(.4f, kPopTransitionFade);
    }

    // ---------------------------------------------------------------- walking around the lobby

    void TournamentLayer::keyDown(enumKeyCodes key, double timestamp) {
        switch (key) {
            case KEY_A:
            case KEY_Left: m_left = true; break;
            case KEY_D:
            case KEY_Right: m_right = true; break;
            case KEY_W:
            case KEY_Up:
            case KEY_Space:
                if (!m_jumpHeld) m_arena->jumpMe();
                m_jumpHeld = true;
                break;
            default: CCLayer::keyDown(key, timestamp); return;
        }
        m_arena->steer((m_right ? 1 : 0) - (m_left ? 1 : 0));
    }

    void TournamentLayer::keyUp(enumKeyCodes key, double timestamp) {
        switch (key) {
            case KEY_A:
            case KEY_Left: m_left = false; break;
            case KEY_D:
            case KEY_Right: m_right = false; break;
            case KEY_W:
            case KEY_Up:
            case KEY_Space: m_jumpHeld = false; break;
            default: CCLayer::keyUp(key, timestamp); return;
        }
        m_arena->steer((m_right ? 1 : 0) - (m_left ? 1 : 0));
    }

    void TournamentLayer::releaseKeys() {
        m_left = m_right = m_jumpHeld = false;
        if (m_arena) m_arena->steer(0);
    }

    void TournamentLayer::tickMovement(float dt) {
        if (!m_loaded || !m_state.t.lobby()) return;
        // a popup takes the keyboard: its key-ups never reach us, so let go of everything
        if (m_left || m_right || m_jumpHeld) {
            if (auto* scene = CCDirector::get()->getRunningScene()) {
                for (auto* child : CCArrayExt<CCNode*>(scene->getChildren())) {
                    if (typeinfo_cast<FLAlertLayer*>(child)) {
                        this->releaseKeys();
                        break;
                    }
                }
            }
        }
        // holding jump keeps hopping, like holding in a level
        if (m_jumpHeld) m_arena->jumpMe();

        if (m_arena->myMoved() && m_moveHint && !m_movedOnce) {
            m_movedOnce = true;
            m_moveHint->runAction(CCSequence::create(CCFadeOut::create(.4f), CCRemoveSelf::create(), nullptr));
            m_moveHint = nullptr;
        }

        m_moveTimer += dt;
        if (!m_arena->myMoved() || m_moveSending || m_moveTimer < kMoveSendGap) return;
        m_moveTimer = 0.f;
        m_moveSending = true;
        m_arena->clearMyMoved();
        auto args = matjson::makeObject({{"code", m_code}, {"x", m_arena->myX01()}, {"j", m_arena->myJumps()}});
        api::call(m_moveReq, "nt_move", withAuth(args), [this](api::Reply) { m_moveSending = false; });
    }

    void TournamentLayer::showMoveHint() {
        if (m_moveHint || m_movedOnce || !m_arena->canMove()) return;
        auto win = CCDirector::get()->getWinSize();
        m_moveHint = CCNode::create();
        auto* text = label("A / D  walk      W  jump      or tap the ground", "bigFont.fnt", .27f, col::white);
        text->setOpacity(190);
        m_moveHint->addChild(text);
        m_moveHint->setPosition({win.width / 2.f, 50.f});
        this->addChild(m_moveHint, 15);
        text->setScale(0.f);
        text->runAction(CCSequence::create(CCDelayTime::create(1.4f), CCEaseBackOut::create(CCScaleTo::create(.3f, .27f)),
                                           CCDelayTime::create(9.f), CCFadeOut::create(.6f), nullptr));
    }

    // ---------------------------------------------------------------- touches

    void TournamentLayer::registerWithTouchDispatcher() {
        CCTouchDispatcher::get()->addTargetedDelegate(this, 0, false);
    }

    bool TournamentLayer::ccTouchBegan(CCTouch* touch, CCEvent*) {
        m_touchStart = touch->getLocation();
        return m_loaded;
    }

    void TournamentLayer::ccTouchEnded(CCTouch* touch, CCEvent*) {
        auto p = touch->getLocation();
        if (ccpDistance(p, m_touchStart) > 12.f) return;
        auto local = m_arena->convertToNodeSpace(p);
        bool isHost = m_state.isHost(m_me);
        if (int acc = m_arena->avatarAt(local)) {
            this->releaseKeys();
            if (auto* modal = PlayerModal::create(m_state, acc, isHost, [this, acc] {
                    this->host("kick", matjson::makeObject({{"target", acc}}));
                })) {
                modal->show();
            }
            return;
        }
        // in the lobby: walk to where you tapped
        auto win = CCDirector::get()->getWinSize();
        if (m_state.t.lobby() && m_arena->canMove() && p.y < win.height - 50.f) {
            m_arena->walkMeTo(local.x);
            return;
        }
        int r, s;
        if (isHost && m_arena->matchAt(local, r, s)) {
            auto* m = m_state.match(r, s);
            if (!m || m->done() || !m->p1 || !m->p2) return;
            this->releaseKeys();
            if (auto* modal = MatchModal::create(m_state, *m, [this, r, s](int winner) {
                    this->host("win", matjson::makeObject({{"round", r}, {"slot", s}, {"target", winner}}));
                })) {
                modal->show();
            }
        }
    }

    // ---------------------------------------------------------------- polling

    void TournamentLayer::poll(float dt) {
        this->tickMovement(dt);
        m_pollTimer += dt;
        float interval = m_state.t.lobby() ? kPollLobbyFast : kPollLobby;
        if (m_failures > 0) interval = std::min(8.f, 1.5f * m_failures);
        if (m_polling || m_busy || m_pollTimer < interval) return;
        m_pollTimer = 0.f;
        m_polling = true;
        auto args = matjson::makeObject({{"code", m_code}});
        args["since"] = m_loaded ? m_state.evLast : -1;
        if (m_loaded) args["rev"] = m_state.t.rev;
        int64_t sent = api::localMs();
        api::call(m_pollReq, "nt_state", withAuth(args), [this, sent](api::Reply reply) {
            m_polling = false;
            if (reply.isErr()) {
                auto err = reply.unwrapErr();
                if (err == "notfound") {
                    m_busy = true;
                    this->unschedule(schedule_selector(TournamentLayer::poll));
                    FLAlertLayer::create("Tournament over", "This tournament has been closed.", "OK")->show();
                    this->runAction(CCSequence::create(CCDelayTime::create(1.2f), CallFuncExt::create([] {
                        CCDirector::get()->popSceneWithTransition(.4f, kPopTransitionFade);
                    }), nullptr));
                    return;
                }
                if (++m_failures == 3) {
                    Notification::create(api::describe(err), NotificationIcon::Error, 2.5f)->show();
                }
                return;
            }
            m_failures = 0;
            this->onState(reply.unwrap(), sent, false);
        });
    }

    void TournamentLayer::onState(matjson::Value const& v, int64_t sent, bool) {
        noteServerTime(jlong(v, "now"), sent, api::localMs());
        bool first = !m_loaded;
        bool replaced = m_state.apply(v);
        auto events = m_state.events(v);
        m_me = me().account;
        m_arena->setMe(m_me);

        if (first) {
            m_loaded = true;
            if (m_loading) {
                m_loading->removeFromParent();
                m_loading = nullptr;
            }
            m_backdrop->setGround(m_state.t.lobby(), false);
            m_arena->sync(m_state);
            // a little hello for whoever is already here (and a proper entrance for us)
            int i = 0;
            if (m_state.t.lobby()) {
                if (m_justJoined && m_state.player(m_me)) m_arena->onJoin(m_state, m_me);
                for (auto* p : m_state.activePlayers()) {
                    int acc = p->account;
                    if (acc == m_me && m_justJoined) continue;
                    this->runAction(CCSequence::create(CCDelayTime::create(.3f + .12f * i++), CallFuncExt::create([this, acc] {
                        m_arena->onEmote(acc, 1);
                    }), nullptr));
                    if (i > 8) break;
                }
                this->showMoveHint();
            }
            m_revealedId = m_state.t.level.id;
        } else {
            this->handleEvents(events);
            if (replaced) m_arena->sync(m_state);
            else m_arena->tickLive(m_state);
            m_backdrop->setGround(m_state.t.lobby(), true);
            if (m_state.t.lobby()) this->showMoveHint();
        }
        this->refreshChrome();
        this->setLevelCard();
        this->maybeEnterMatch();
    }

    std::string TournamentLayer::nameOf(int account) const {
        if (auto* p = m_state.player(account)) return p->name;
        return "Someone";
    }

    void TournamentLayer::handleEvents(std::vector<Event> const& events) {
        for (auto const& e : events) {
            if (e.kind == "join") {
                this->feed(jstr(e.data, "n", this->nameOf(e.account)) + " joined", col::green);
                m_arena->onJoin(m_state, e.account);
            } else if (e.kind == "leave") {
                this->feed(jstr(e.data, "n", this->nameOf(e.account)) + " left", col::dim);
                m_arena->onLeave(m_state, e.account);
            } else if (e.kind == "kick") {
                this->feed(this->nameOf(e.account) + " was kicked", col::red);
                m_arena->onKick(m_state, e.account);
                if (e.account == m_me) {
                    Notification::create("The host removed you from this tournament.", NotificationIcon::Warning, 3.f)->show();
                }
            } else if (e.kind == "emote") {
                // our own emotes already played the moment we sent them
                if (e.account == m_me && api::localMs() - m_lastLocalEmote < 5000) continue;
                m_arena->onEmote(e.account, jint(e.data, "e", 1));
            } else if (e.kind == "start") {
                this->feed("The bracket is set!", col::gold);
                this->releaseKeys();
                if (m_moveHint) {
                    m_moveHint->removeFromParent();
                    m_moveHint = nullptr;
                }
                m_backdrop->setGround(false, true);
                m_arena->onStart(m_state);
            } else if (e.kind == "level") {
                LevelRef lv;
                lv.id = jlong(e.data, "id");
                lv.name = jstr(e.data, "name");
                lv.author = jstr(e.data, "author");
                lv.face = jint(e.data, "diff") - 1;
                this->feed("Level: " + (lv.name.empty() ? std::to_string(lv.id) : lv.name), col::gold);
                this->showReveal(lv, jint(e.data, "r", m_state.nextRound()));
            } else if (e.kind == "round") {
                int r = jint(e.data, "r");
                auto name = roundName(r, m_state.t.rounds);
                this->feed(name + " started!", col::gold);
                fx::banner(this, name + "  -  FIGHT!", jstr(e.data, "name", ""), col::gold, 1.4f);
                fx::play("go");
                m_arena->onRound(m_state, r);
            } else if (e.kind == "result") {
                int w = jint(e.data, "w"), l = jint(e.data, "l");
                auto why = jstr(e.data, "why");
                if (why != "bye" && l) {
                    auto how = why == "complete" ? std::string(" (finished)") : why == "time" ? std::string(" (time)")
                             : why == "forfeit" ? std::string(" (forfeit)") : why == "host" ? std::string(" (host)") : "";
                    this->feed(this->nameOf(w) + " beat " + this->nameOf(l) + how, col::goldLight);
                }
                m_arena->onResult(m_state, e);
            } else if (e.kind == "champion") {
                this->feed(this->nameOf(e.account) + " is the champion!", col::gold);
                m_arena->onChampion(m_state, e.account);
            } else if (e.kind == "close") {
                this->feed("The host closed the tournament", col::red);
            }
        }
    }

    void TournamentLayer::feed(std::string const& text, ccColor3B color) {
        auto* children = m_feed->getChildren();
        int count = m_feed->getChildrenCount();
        if (children) {
            for (auto* node : CCArrayExt<CCNode*>(children)) {
                node->runAction(CCEaseOut::create(CCMoveBy::create(.2f, {0.f, -14.f}), 2.f));
            }
            if (count >= 5) {
                if (auto* oldest = static_cast<CCNode*>(children->objectAtIndex(0))) oldest->removeFromParent();
            }
        }
        auto* l = label(text, "bigFont.fnt", .3f, color);
        fit(l, 200.f, .3f);
        l->setAnchorPoint({0.f, .5f});
        l->setPosition({-220.f, 0.f});
        m_feed->addChild(l);
        l->runAction(CCSequence::create(
            CCEaseBackOut::create(CCMoveTo::create(.3f, {0.f, 0.f})),
            CCDelayTime::create(7.f),
            CCFadeOut::create(.6f),
            CCRemoveSelf::create(), nullptr));
    }

    // ---------------------------------------------------------------- the level roll

    std::string TournamentLayer::revealTitle(int round) const {
        int rounds = std::max(1, m_state.t.rounds);
        if (round <= 0) round = 1;
        if (round >= rounds) return "LEVEL FOR THE FINAL";
        auto name = roundName(round, rounds);
        if (rounds - round <= 2) return "LEVEL FOR THE " + name;
        return "LEVEL FOR " + name;
    }

    void TournamentLayer::rollLevel() {
        if (m_rolling || m_revealing || !m_state.isHost(m_me)) return;
        int next = m_state.nextRound();
        if (next <= 0) return;
        m_rolling = true;
        m_revealing = true;
        m_rolledId = 0;
        if (auto old = m_reveal.lock()) old->removeFromParent();
        auto* reveal = LevelReveal::create(m_state.t.pool, this->revealTitle(next));
        this->addChild(reveal, 40);
        reveal->spin();
        m_reveal = WeakRef<LevelReveal>(reveal);
        fx::play("whoosh");
        this->refreshChrome(true);

        m_roller->roll(m_state.t.pool, [this](LevelRef lv, std::string err) {
            if (!lv.valid()) {
                m_rolling = false;
                m_revealing = false;
                if (auto r = m_reveal.lock()) r->fail(err.empty() ? std::string("No level found. Try again!") : err);
                m_reveal = WeakRef<LevelReveal>();
                this->refreshChrome(true);
                return;
            }
            m_rolledId = lv.id;
            m_revealedId = lv.id;
            auto args = matjson::makeObject({{"level_id", lv.id},
                                             {"level_name", lv.name},
                                             {"level_author", lv.author},
                                             {"level_diff", lv.face + 1}});
            this->host("level", args, [this, lv](bool ok) {
                m_rolling = false;
                auto r = m_reveal.lock();
                if (!ok) {
                    m_revealing = false;
                    if (r) r->fail("Couldn't save the level. Try again!");
                    m_reveal = WeakRef<LevelReveal>();
                    this->refreshChrome(true);
                    return;
                }
                if (r) r->land(lv, [this] { this->revealDone(); });
                else this->revealDone();
                this->refreshChrome(true);
            });
        });
    }

    void TournamentLayer::showReveal(LevelRef const& level, int round) {
        if (!level.valid() || level.id == m_revealedId) return;
        // our own roll lands by itself
        if (m_rolling || (m_rolledId && level.id == m_rolledId)) return;
        m_revealedId = level.id;
        m_revealing = true;
        if (auto old = m_reveal.lock()) old->removeFromParent();
        auto* reveal = LevelReveal::create(m_state.t.pool, this->revealTitle(round));
        this->addChild(reveal, 40);
        reveal->spin();
        m_reveal = WeakRef<LevelReveal>(reveal);
        fx::play("whoosh");
        // a short drumroll for everyone watching, then the slam
        WeakRef<LevelReveal> weak(reveal);
        auto lv = level;
        this->runAction(CCSequence::create(CCDelayTime::create(.9f), CallFuncExt::create([this, weak, lv] {
            if (auto r = weak.lock()) r->land(lv, [this] { this->revealDone(); });
            else this->revealDone();
        }), nullptr));
        this->refreshChrome(true);
    }

    void TournamentLayer::revealDone() {
        m_revealing = false;
        m_reveal = WeakRef<LevelReveal>();
        m_levelShown = -1;
        this->setLevelCard();
        this->refreshChrome(true);
    }

    // ---------------------------------------------------------------- chrome

    void TournamentLayer::setLevelCard() {
        if (m_revealing) return;  // the reveal flies into this corner and becomes the card
        auto const& lv = m_state.t.level;
        if (lv.id == m_levelShown) return;
        m_levelShown = lv.id;
        if (m_levelCard) {
            m_levelCard->removeFromParent();
            m_levelCard = nullptr;
        }
        if (!lv.valid() || m_state.t.finished()) return;
        auto win = CCDirector::get()->getWinSize();
        m_levelCard = CCNode::create();
        auto* plate = Plate::create({168.f, 34.f}, col::navyDeep, .85f, col::gold, .6f, 8.f, 1.2f);
        m_levelCard->addChild(plate);
        if (auto* face = difficulty(lv.face)) {
            face->setScale(.55f);
            face->setPosition({18.f, 17.f});
            m_levelCard->addChild(face);
        }
        auto* name = label(lv.name.empty() ? "Level " + std::to_string(lv.id) : lv.name, "bigFont.fnt", .34f, col::white);
        fit(name, 128.f, .34f);
        name->setAnchorPoint({0.f, .5f});
        name->setPosition({34.f, 22.f});
        m_levelCard->addChild(name);
        auto* by = label(lv.author.empty() ? "#" + std::to_string(lv.id) : "by " + lv.author, "goldFont.fnt", .32f, col::gold);
        fit(by, 128.f, .32f);
        by->setAnchorPoint({0.f, .5f});
        by->setPosition({34.f, 9.f});
        m_levelCard->addChild(by);
        CCPoint at {win.width - 178.f, win.height - 86.f};
        this->addChild(m_levelCard, 11);
        m_levelCard->setPosition(at);
        fx::popIn(m_levelCard, 1.f, 0.f, .35f);
    }

    void TournamentLayer::refreshChrome(bool force) {
        auto win = CCDirector::get()->getWinSize();
        auto const& t = m_state.t;
        m_title->setString(t.name.c_str());
        fit(m_title, win.width - 260.f, .7f);
        std::string stateText = t.lobby() ? "OPEN" : t.running() ? "LIVE" : "FINISHED";
        std::string sub = fmt::format("{}  -  host {}  -  {}/{} players  -  {}", t.pub ? "PUBLIC" : "PRIVATE",
                                      t.hostName, t.count, t.max, stateText);
        if (t.running()) sub += "  -  " + roundName(std::max(1, t.round), t.rounds);
        m_sub->setString(sub.c_str());
        fit(m_sub, win.width - 260.f, .3f);

        // rebuild the bottom bar only when what it should show changes
        bool joined = false;
        if (auto* p = m_state.player(m_me)) joined = p->status == "in" || p->status == "out";
        bool host = m_state.isHost(m_me);
        bool anyLive = false;
        for (auto const& m : m_state.matches) {
            if (m.round == t.round && m.live()) anyLive = true;
        }
        int next = m_state.nextRound();
        bool rolled = next > 0 && t.levelRound == next && t.level.valid();
        auto* myMatch = m_state.currentMatch(m_me);
        bool canReturn = myMatch && myMatch->live() && MatchController::get()->isMatch(m_code, myMatch->round, myMatch->slot);
        auto key = fmt::format("{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}", t.state, joined, host, t.round, anyLive, next,
                               rolled, canReturn, t.count, m_rolling, m_revealing, t.level.id);
        if (key == m_chromeKey && !force) return;
        m_chromeKey = key;
        m_bottom->removeAllChildren();

        float y = 24.f;
        if (joined) {
            auto* emoteSpr = emote(1);
            emoteSpr->setScale(.5f);
            auto* emoteBtn = nodeButton(emoteSpr, [this](auto) { this->onEmote(); });
            emoteBtn->setPosition({34.f, y + 4.f});
            m_bottom->addChild(emoteBtn);
            if (!t.finished()) {
                auto* leave = textButton("Leave", "GJ_button_04.png", .5f, [this](auto) { this->onLeave(); });
                leave->setPosition({92.f, y});
                m_bottom->addChild(leave);
            }
        }

        if (t.lobby() && !joined) {
            auto* join = textButton("Join", "GJ_button_01.png", .85f, [this](auto) { this->onJoin(); });
            join->setPosition({win.width / 2.f - (host ? 120.f : 0.f), y + 2.f});
            m_bottom->addChild(join);
            pulse(join);
        }

        if (canReturn) {
            auto* back = textButton("Back to match", "GJ_button_01.png", .65f, [](auto) {
                if (!MatchController::get()->launch()) {
                    CCDirector::get()->replaceScene(CCTransitionFade::create(.4f, VsLayer::scene()));
                }
            });
            back->setPosition({win.width - 80.f, y});
            m_bottom->addChild(back);
        }

        if (!host) return;
        if (t.lobby()) {
            auto* start = textButton(t.count >= 2 ? "Start tournament" : "Need 2 players",
                                     t.count >= 2 ? "GJ_button_01.png" : "GJ_button_04.png", .65f, [this](auto) {
                if (m_state.t.count < 2) return;
                this->releaseKeys();
                if (auto* m = ConfirmModal::create("Start?", "Lock the bracket with the players who are here now?",
                                                   "Start", [this] { this->host("start"); })) {
                    m->show();
                }
            });
            start->setPosition({win.width / 2.f + (joined ? 75.f : 140.f), y});
            m_bottom->addChild(start);
            // practice bots: try the whole thing (or show it off) without a second player
            if (t.count < t.max) {
                auto* bots = textButton("Add bots", "GJ_button_02.png", .5f, [this](auto) {
                    this->host("bots", matjson::makeObject({{"count", 3}}));
                });
                bots->setPosition({win.width / 2.f + (joined ? -60.f : 10.f), y});
                m_bottom->addChild(bots);
            }
        } else if (t.running() && next > 0) {
            bool busy = m_rolling || m_revealing;
            std::string rollText = m_rolling ? "Rolling..." : rolled ? "Reroll" : "Roll level";
            auto* roll = textButton(rollText, rolled || busy ? "GJ_button_04.png" : "GJ_button_02.png", .55f, [this](auto) {
                this->rollLevel();
            });
            roll->setPosition({win.width / 2.f - 70.f, y});
            m_bottom->addChild(roll);
            if (!rolled && !busy && !anyLive) pulse(roll);

            bool ready = rolled && !anyLive && !busy;
            std::string text = anyLive ? "Round in progress" : !rolled ? "Roll a level first"
                             : fmt::format("Start {}", roundName(next, t.rounds));
            auto* round = textButton(text, ready ? "GJ_button_01.png" : "GJ_button_04.png", .55f, [this, ready](auto) {
                if (!ready) return;
                this->host("round");
            });
            round->setPosition({win.width / 2.f + 80.f, y});
            m_bottom->addChild(round);
            if (ready) pulse(round);
        } else if (t.finished()) {
            auto* close = textButton("Close tournament", "GJ_button_06.png", .55f, [this](auto) {
                if (auto* m = ConfirmModal::create("Close?", "Remove this tournament from the list?", "Close",
                                                   [this] { this->host("close"); }, true)) {
                    m->show();
                }
            });
            close->setPosition({win.width / 2.f, y});
            m_bottom->addChild(close);
        }
    }

    // ---------------------------------------------------------------- my match

    void TournamentLayer::maybeEnterMatch() {
        if (m_busy || !m_state.t.running()) return;
        auto* m = m_state.currentMatch(m_me);
        if (!m || !m->live()) return;
        auto* mc = MatchController::get();
        mc->begin(m_state, *m, m_me);
        if (mc->launchedOnce()) return;
        if (serverNowMs() > m->startsAt + 45000) return;
        m_busy = true;
        this->releaseKeys();
        this->unschedule(schedule_selector(TournamentLayer::poll));
        CCDirector::get()->replaceScene(CCTransitionFade::create(.4f, VsLayer::scene()));
    }

    // ---------------------------------------------------------------- actions

    void TournamentLayer::host(std::string const& action, matjson::Value extra, std::function<void(bool)> done) {
        extra["code"] = m_code;
        extra["action"] = action;
        auto& holder = action == "level" ? m_levelReq : m_actionReq;
        api::call(holder, "nt_host", withAuth(extra), [this, done](api::Reply reply) {
            if (reply.isErr()) {
                Notification::create(api::describe(reply.unwrapErr()), NotificationIcon::Error, 2.5f)->show();
                if (done) done(false);
                return;
            }
            m_pollTimer = 99.f;
            if (done) done(true);
        });
    }

    void TournamentLayer::onJoin() {
        if (!me().loggedIn()) {
            FLAlertLayer::create("Log in first", "Log into your Geometry Dash account to join tournaments.", "OK")->show();
            return;
        }
        api::call(m_actionReq, "nt_join", withAuth(matjson::makeObject({{"code", m_code}})), [this](api::Reply reply) {
            if (reply.isErr()) {
                FLAlertLayer::create("Can't join", api::describe(reply.unwrapErr()), "OK")->show();
                return;
            }
            fx::play("pop");
            m_pollTimer = 99.f;
        });
    }

    void TournamentLayer::onLeave() {
        this->releaseKeys();
        std::string text = m_state.t.lobby() ? "Leave the lobby?" : "Leaving now forfeits your match. Leave?";
        if (auto* m = ConfirmModal::create("Leave", text, "Leave", [this] {
                api::call(m_actionReq, "nt_leave", withAuth(matjson::makeObject({{"code", m_code}})), [this](api::Reply reply) {
                    if (reply.isErr()) {
                        Notification::create(api::describe(reply.unwrapErr()), NotificationIcon::Error, 2.f)->show();
                        return;
                    }
                    MatchController::get()->watch("");
                    m_pollTimer = 99.f;
                });
            }, true)) {
            m->show();
        }
    }

    void TournamentLayer::onEmote() {
        this->releaseKeys();
        if (auto* m = EmoteModal::create([this](int id) {
                m_lastLocalEmote = api::localMs();
                m_arena->onEmote(m_me, id);
                api::fire("nt_emote", withAuth(matjson::makeObject({{"code", m_code}, {"emote", id}})),
                          [](api::Reply reply) {
                              if (reply.isErr() && reply.unwrapErr() == "slow") {
                                  Notification::create("Slow down a little!", NotificationIcon::Warning, 1.f)->show();
                              }
                          });
            })) {
            m->show();
        }
    }
}
