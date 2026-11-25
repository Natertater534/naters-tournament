#include "Arena.hpp"
#include "Fx.hpp"
#include "Theme.hpp"

#include <cmath>

using namespace geode::prelude;

namespace nt::ui {
    namespace {
        constexpr float kTopPad = 64.f;     // room for the top bar + round labels
        constexpr float kBottomPad = 48.f;  // room for the bottom bar
        float s_lastClang = 0.f;
        float s_clock = 0.f;

        CCSprite* swordsSprite(float height) {
            auto* spr = CCSprite::createWithSpriteFrameName("GJ_versusBtn_001.png");
            if (!spr) spr = fx("swords");
            float h = std::max(spr->getContentSize().width, spr->getContentSize().height);
            spr->setScale(h > 0.f ? height / h : 1.f);
            return spr;
        }
    }

    // ---------------------------------------------------------------- Clash

    // Drives two avatars through a wind-up / lunge / impact / recoil loop while their match is live.
    class Clash : public CCNode {
    public:
        static Clash* create(Avatar* a, Avatar* b, CCPoint seatA, CCPoint seatB, float scale) {
            auto* ret = new Clash();
            if (ret->init()) {
                ret->autorelease();
                ret->m_a = WeakRef<Avatar>(a);
                ret->m_b = WeakRef<Avatar>(b);
                ret->m_sa = seatA;
                ret->m_sb = seatB;
                ret->m_scale = scale;
                ret->m_period = fx::rnd(1.45f, 1.85f);
                ret->m_t = fx::rnd(0.f, ret->m_period);
                ret->m_swords = swordsSprite(22.f + 10.f * scale);
                ret->m_baseScale = ret->m_swords->getScale();
                ret->m_swords->setPosition((seatA + seatB) / 2.f + ccp(0.f, 14.f * scale + 6.f));
                ret->addChild(ret->m_swords);
                a->busy = true;
                b->busy = true;
                ret->scheduleUpdate();
                return ret;
            }
            delete ret;
            return nullptr;
        }

        void stop() {
            if (auto a = m_a.lock()) {
                a->setPosition(m_sa);
                a->busy = false;
            }
            if (auto b = m_b.lock()) {
                b->setPosition(m_sb);
                b->busy = false;
            }
            this->unscheduleUpdate();
            this->removeFromParent();
        }

    protected:
        void update(float dt) override {
            auto a = m_a.lock();
            auto b = m_b.lock();
            if (!a || !b) return;
            m_t += dt;
            float ph = std::fmod(m_t, m_period) / m_period;
            float gap = (m_sb.x - m_sa.x) * .5f;
            float off;
            if (ph < .38f) off = -4.f * m_scale * std::sin(ph / .38f * 1.5708f);
            else if (ph < .47f) off = -4.f * m_scale + (gap * .62f + 4.f * m_scale) * std::pow((ph - .38f) / .09f, 2.f);
            else if (ph < .75f) off = gap * .62f * (1.f - std::sin((ph - .47f) / .28f * 1.5708f));
            else off = 0.f;
            a->setPosition(m_sa + ccp(off, 0.f));
            b->setPosition(m_sb - ccp(off, 0.f));

            bool hit = ph >= .47f && ph < .75f;
            if (hit && !m_hit) {
                m_hit = true;
                auto mid = (m_sa + m_sb) / 2.f + ccp(0.f, 13.f * m_scale);
                fx::sparks(this->getParent(), mid, 6, col::goldLight, 22.f * std::max(.6f, m_scale), 40);
                m_swords->stopAllActions();
                m_swords->setScale(m_baseScale * 1.35f);
                m_swords->runAction(CCEaseBackOut::create(CCScaleTo::create(.3f, m_baseScale)));
                m_swords->setRotation(fx::rnd(-12.f, 12.f));
                if (s_clock - s_lastClang > .55f) {
                    s_lastClang = s_clock;
                    fx::play("clang");
                }
            } else if (!hit) {
                m_hit = false;
            }
        }

        WeakRef<Avatar> m_a, m_b;
        CCPoint m_sa, m_sb;
        float m_scale = 1.f, m_period = 1.6f, m_t = 0.f, m_baseScale = 1.f;
        bool m_hit = false;
        CCSprite* m_swords = nullptr;
    };

    // ---------------------------------------------------------------- setup

    Arena* Arena::create(CCSize size, float groundY) {
        auto* ret = new Arena();
        if (ret->init(size, groundY)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool Arena::init(CCSize size, float groundY) {
        if (!CCNode::init()) return false;
        m_size = size;
        m_groundY = groundY;
        this->setContentSize(size);

        m_lobbyDeco = CCNode::create();
        this->addChild(m_lobbyDeco, -1);
        m_bracketRoot = CCNode::create();
        this->addChild(m_bracketRoot, -1);
        m_lines = CCDrawNode::create();
        m_bracketRoot->addChild(m_lines, 0);
        m_ghosts = CCNode::create();
        m_bracketRoot->addChild(m_ghosts, 2);
        m_labels = CCNode::create();
        m_bracketRoot->addChild(m_labels, 3);

        this->scheduleUpdate();
        return true;
    }

    void Arena::update(float dt) {
        s_clock += dt;
        for (auto& t : m_trails) {
            t.life -= dt;
            auto target = t.target.lock();
            if (target && t.life > 0.f) {
                t.streak->setPosition(target->getPosition() + ccp(0.f, target->height() * .5f * target->getScale()));
            }
        }
        std::erase_if(m_trails, [](TrailInfo& t) {
            if (t.life > -0.6f && t.target.valid()) return false;
            t.streak->removeFromParent();
            return true;
        });
        m_resultDelay = std::max(0.f, m_resultDelay - dt);
    }

    Avatar* Arena::avatarFor(State const& state, Player const& p, float scale) {
        auto it = m_avatars.find(p.account);
        if (it != m_avatars.end() && it->second) {
            it->second->setPlayer(p);
            return it->second;
        }
        auto* a = Avatar::create(p, scale, true);
        a->setYou(p.account == m_me);
        this->addChild(a, 10);
        m_avatars[p.account] = a;
        return a;
    }

    void Arena::trail(Avatar* a, ccColor3B color, float life) {
        auto path = Mod::get()->expandSpriteName("dot.png");
        auto* streak = CCMotionStreak::create(.35f, 2.f, 7.f * std::max(.6f, a->iconScale()), color, path.c_str());
        if (!streak) return;
        streak->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        streak->setPosition(a->getPosition() + ccp(0.f, a->height() * .5f));
        this->addChild(streak, 9);
        m_trails.push_back({WeakRef<Avatar>(a), streak, life});
    }

    // ---------------------------------------------------------------- lobby

    CCPoint Arena::lobbySpot(int index, int count, float& scale) const {
        int rows = count > 12 ? 2 : 1;
        int front = rows == 1 ? count : (count + 1) / 2;
        int row = (rows == 2 && index >= front) ? 1 : 0;
        int col = row == 0 ? index : index - front;
        int inRow = row == 0 ? front : count - front;
        float usable = m_size.width - 80.f;
        float spacing = std::min(62.f, usable / std::max(1, inRow));
        scale = std::clamp(spacing / 50.f, .55f, 1.f) * (row == 1 ? .86f : 1.f);
        float x = m_size.width / 2.f + (col - (inRow - 1) / 2.f) * spacing;
        // a gentle crowd shape: the middle stands a touch further back
        float y = m_groundY + (row == 1 ? 46.f : 0.f);
        return {x, y};
    }

    void Arena::layoutLobby(State const& state, bool animate) {
        auto players = state.activePlayers();
        int count = static_cast<int>(players.size());

        // the back row stands on a strip of GD blocks
        m_lobbyDeco->removeAllChildren();
        if (count > 12) {
            float dummy;
            auto left = this->lobbySpot((count + 1) / 2, count, dummy);
            auto right = this->lobbySpot(count - 1, count, dummy);
            float x0 = left.x - 24.f, x1 = right.x + 24.f;
            float y = m_groundY + 46.f;
            float bw = 15.f;
            for (float x = x0; x < x1; x += bw) {
                auto* block = CCSprite::createWithSpriteFrameName("square_01_001.png");
                if (!block) {
                    auto* plate = Plate::create({x1 - x0, 7.f}, col::plate, .9f, col::blue, .7f, 2.f, 1.f);
                    plate->setPosition({x0, y - 7.f});
                    m_lobbyDeco->addChild(plate);
                    break;
                }
                block->setScale(bw / block->getContentSize().width);
                block->setAnchorPoint({0.f, 1.f});
                block->setPosition({x, y});
                block->setColor({70, 120, 230});
                m_lobbyDeco->addChild(block);
            }
        }

        int i = 0;
        for (auto* p : players) {
            float scale;
            auto spot = this->lobbySpot(i, count, scale);
            auto it = m_avatars.find(p->account);
            if (it != m_avatars.end() && it->second) {
                auto* a = it->second;
                a->setPlayer(*p);
                a->setZOrder(i >= ((count + 1) / 2) && count > 12 ? 8 : 10);
                if (!a->busy) {
                    a->stopActionByTag(0x1A70);
                    if (animate && ccpDistance(a->getPosition(), spot) > 1.f) {
                        auto* move = CCEaseInOut::create(CCMoveTo::create(.4f, spot), 2.f);
                        move->setTag(0x1A70);
                        a->runAction(move);
                    } else {
                        a->setPosition(spot);
                    }
                    a->setScale(scale);
                    a->setIdle(true);
                }
            }
            ++i;
        }

        if (!m_waiting) {
            m_waiting = CCNode::create();
            auto* t = label("WAITING FOR PLAYERS", "goldFont.fnt", .7f, col::white);
            t->setOpacity(200);
            m_waiting->addChild(t);
            auto* dots = label("...", "goldFont.fnt", .7f, col::white);
            dots->setPosition({t->getScaledContentSize().width / 2.f + 12.f, 0.f});
            dots->runAction(CCRepeatForever::create(CCSequence::create(
                CCFadeTo::create(.5f, 40), CCFadeTo::create(.5f, 255), nullptr)));
            m_waiting->addChild(dots);
            m_waiting->setPosition({m_size.width / 2.f, m_groundY + 118.f});
            m_waiting->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCMoveBy::create(1.4f, {0.f, 4.f})),
                CCEaseSineInOut::create(CCMoveBy::create(1.4f, {0.f, -4.f})), nullptr)));
            this->addChild(m_waiting, 5);
        }
        m_waiting->setVisible(!m_bracket && count < 2);
    }

    // ---------------------------------------------------------------- bracket geometry

    float Arena::bracketScale() const {
        switch (m_rounds) {
            case 1: return 1.15f;
            case 2: return .95f;
            case 3: return .78f;
            case 4: return .6f;
            default: return .48f;
        }
    }

    int Arena::sideOf(int round, int slot) const {
        if (round >= m_rounds) return 0;
        int perSide = (m_size2 >> round) / 2;
        return slot < perSide ? -1 : 1;
    }

    CCPoint Arena::cardCentre(int round, int slot) const {
        int cols = 2 * m_rounds - 1;
        float margin = 16.f;
        float colW = (m_size.width - 2.f * margin) / cols;
        float top = m_size.height - kTopPad, bottom = kBottomPad;
        int side = this->sideOf(round, slot);
        int col = side < 0 ? round - 1 : side > 0 ? cols - round : m_rounds - 1;
        float x = margin + (col + .5f) * colW;
        if (round >= m_rounds) return {x, (top + bottom) / 2.f};
        if (round == 1) {
            int perSide = (m_size2 >> 1) / 2;
            int k = side < 0 ? slot : slot - perSide;
            float step = (top - bottom) / std::max(1, perSide);
            return {x, top - (k + .5f) * step};
        }
        auto a = this->cardCentre(round - 1, slot * 2);
        auto b = this->cardCentre(round - 1, slot * 2 + 1);
        return {x, (a.y + b.y) / 2.f};
    }

    CCPoint Arena::seatPos(int round, int slot, int which) const {
        auto c = this->cardCentre(round, slot);
        float d = 13.f + 15.f * this->bracketScale();
        float y = c.y - 15.f * this->bracketScale();  // avatar position = feet
        return {c.x + (which == 1 ? -d : d), y};
    }

    std::vector<CCPoint> Arena::travelPath(int round, int slot, int which) const {
        std::vector<CCPoint> path;
        auto from = this->seatPos(round, slot, which);
        if (round >= m_rounds) return path;
        int nextSlot = slot / 2;
        int nextWhich = slot % 2 == 0 ? 1 : 2;
        auto to = this->seatPos(round + 1, nextSlot, nextWhich);
        auto c = this->cardCentre(round, slot);
        auto n = this->cardCentre(round + 1, nextSlot);
        float midX = (c.x + n.x) / 2.f;
        float lift = 6.f;
        path.push_back({midX, from.y + lift});
        path.push_back({midX, to.y + lift});
        path.push_back(to);
        return path;
    }

    void Arena::buildBracket(State const& state) {
        m_size2 = std::max(2, state.t.size);
        m_rounds = std::max(1, state.t.rounds);
        for (auto& [k, card] : m_cards) card->removeFromParent();
        m_cards.clear();
        m_pct.clear();
        m_labels->removeAllChildren();
        if (m_trophy) m_trophy->removeFromParent();
        if (m_rays) m_rays->removeFromParent();
        m_trophy = nullptr;
        m_rays = nullptr;

        float sc = this->bracketScale();
        float d = 13.f + 15.f * sc;
        for (auto const& m : state.matches) {
            auto c = this->cardCentre(m.round, m.slot);
            CCSize cs {2.f * d + 30.f * sc + 12.f, 30.f * sc + 16.f};
            auto* card = Plate::create(cs, col::plate, .78f, col::blue, .35f, 6.f, 1.2f);
            card->setAnchorPoint({.5f, .5f});
            card->setPosition(c);
            m_bracketRoot->addChild(card, 1);
            m_cards[{m.round, m.slot}] = card;

            auto* p1 = label("", "bigFont.fnt", .22f + .1f * sc, col::white);
            auto* p2 = label("", "bigFont.fnt", .22f + .1f * sc, col::white);
            p1->setPosition(this->seatPos(m.round, m.slot, 1) + ccp(0.f, 30.f * sc + 7.f));
            p2->setPosition(this->seatPos(m.round, m.slot, 2) + ccp(0.f, 30.f * sc + 7.f));
            m_labels->addChild(p1);
            m_labels->addChild(p2);
            m_pct[{m.round, m.slot}] = {p1, p2};
        }

        // round names across the top
        float top = m_size.height - kTopPad + 16.f;
        for (int r = 1; r <= m_rounds; ++r) {
            auto name = roundName(r, m_rounds);
            for (int side : {-1, 1}) {
                if (r == m_rounds && side == 1) continue;
                int slot = r == m_rounds ? 0 : (side < 0 ? 0 : (m_size2 >> r) / 2);
                auto c = this->cardCentre(r, slot);
                auto* l = label(name, "goldFont.fnt", r == m_rounds ? .5f : .38f,
                                r == m_rounds ? col::gold : col::goldLight);
                fit(l, 90.f, r == m_rounds ? .5f : .38f);
                l->setPosition({c.x, top});
                l->setTag(r);
                l->setOpacity(r == state.t.round || state.t.round == 0 ? 255 : 150);
                m_labels->addChild(l);
            }
        }

        // the prize above the final
        auto fc = this->cardCentre(m_rounds, 0);
        m_rays = fx("rays");
        m_rays->setColor(col::gold);
        m_rays->setOpacity(70);
        m_rays->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        m_rays->setScale(.8f);
        m_rays->runAction(CCRepeatForever::create(CCRotateBy::create(16.f, 360.f)));
        m_trophy = fx("trophy");
        m_trophy->setScale(m_rounds <= 2 ? 1.f : .8f);
        float ty = std::min(fc.y + 30.f * sc + 40.f, m_size.height - kTopPad - 6.f);
        if (m_rounds == 1) ty = fc.y + 70.f;
        m_trophy->setPosition({fc.x, ty});
        m_rays->setPosition(m_trophy->getPosition());
        m_trophy->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCMoveBy::create(1.3f, {0.f, 3.f})),
            CCEaseSineInOut::create(CCMoveBy::create(1.3f, {0.f, -3.f})), nullptr)));
        m_bracketRoot->addChild(m_rays, 0);
        m_bracketRoot->addChild(m_trophy, 4);
    }

    void Arena::drawLines(State const& state) {
        m_lines->clear();
        float sc = this->bracketScale();
        float d = 13.f + 15.f * sc;
        for (auto const& m : state.matches) {
            if (m.round >= m_rounds) continue;
            auto c = this->cardCentre(m.round, m.slot);
            auto n = this->cardCentre(m.round + 1, m.slot / 2);
            int side = this->sideOf(m.round, m.slot);
            float edge = c.x + (side < 0 ? d + 15.f * sc + 6.f : -(d + 15.f * sc + 6.f));
            float midX = (c.x + n.x) / 2.f;
            float nextEdge = n.x + (side < 0 ? -(d + 15.f * sc + 6.f) : d + 15.f * sc + 6.f);
            bool lit = m.done() && !m.why.empty();
            auto color = lit ? c4f(col::gold, .85f) : c4f(col::blue, .28f);
            float w = lit ? 1.4f : 1.f;
            m_lines->drawSegment({edge, c.y}, {midX, c.y}, w, color);
            m_lines->drawSegment({midX, c.y}, {midX, n.y}, w, color);
            m_lines->drawSegment({midX, n.y}, {nextEdge, n.y}, w, color);
        }
    }

    void Arena::refreshCards(State const& state) {
        for (auto const& m : state.matches) {
            auto it = m_cards.find({m.round, m.slot});
            if (it == m_cards.end()) continue;
            auto* card = static_cast<Plate*>(it->second);
            card->stopActionByTag(77);
            if (m.live()) {
                card->setBorder(col::gold, .95f);
                card->setFill(col::plate, .92f);
            } else if (m.done()) {
                card->setBorder(col::gold, .45f);
                card->setFill(col::navyDeep, .8f);
            } else {
                card->setBorder(col::blue, .35f);
                card->setFill(col::plate, .7f);
            }
        }
        // highlight the current round's labels
        if (!m_labels->getChildrenCount()) return;
        for (auto* node : CCArrayExt<CCNode*>(m_labels->getChildren())) {
            if (auto* l = typeinfo_cast<CCLabelBMFont*>(node); l && l->getTag() > 0) {
                l->setOpacity(l->getTag() == state.t.round || state.t.round == 0 ? 255 : 130);
            }
        }
    }

    void Arena::layoutBracket(State const& state, bool animate) {
        float sc = this->bracketScale();
        m_ghosts->removeAllChildren();
        std::set<int> placed;
        for (auto const& p : state.players) {
            if (p.status == "left" && !state.currentMatch(p.account)) continue;
            Match const* deepest = nullptr;
            for (auto const& m : state.matches) {
                if (!m.has(p.account)) continue;
                if (!deepest || m.round > deepest->round) deepest = &m;
            }
            if (!deepest) continue;
            int which = deepest->p1 == p.account ? 1 : 2;
            auto seat = this->seatPos(deepest->round, deepest->slot, which);
            // ghosts where they've already won
            for (auto const& m : state.matches) {
                if (!m.has(p.account) || m.round >= deepest->round) continue;
                auto* g = SimplePlayer::create(1);
                g->updatePlayerFrame(std::max(1, p.icon.cube), IconType::Cube);
                g->setColor(iconColor(p.icon.p));
                g->setSecondColor(iconColor(p.icon.s));
                g->setOpacity(80);
                g->setScale(sc * .8f);
                g->setPosition(this->seatPos(m.round, m.slot, m.p1 == p.account ? 1 : 2) + ccp(0.f, 13.f * sc));
                m_ghosts->addChild(g);
            }
            auto* a = this->avatarFor(state, p, sc);
            placed.insert(p.account);
            bool out = (deepest->done() && deepest->winner && deepest->winner != p.account) || p.status == "left";
            bool champ = state.t.finished() && state.t.champ == p.account;
            if (!a->busy) {
                a->stopActionByTag(0x1A70);
                if (champ && m_trophy) seat = m_trophy->getPosition() + ccp(0.f, 18.f);
                if (animate && ccpDistance(a->getPosition(), seat) > 1.f) {
                    auto* move = CCEaseInOut::create(CCMoveTo::create(.45f, seat), 2.f);
                    move->setTag(0x1A70);
                    a->runAction(move);
                } else {
                    a->setPosition(seat);
                }
                a->setScale(1.f);
                if (std::abs(a->iconScale() - sc) > .01f) {
                    // rebuild at the bracket's scale
                    m_avatars.erase(p.account);
                    a->removeFromParent();
                    a = this->avatarFor(state, p, sc);
                    a->setPosition(seat);
                }
                a->setNameVisible(m_rounds <= 4);
                a->setDimmed(out, false);
                a->setIdle(!out && !deepest->live());
            }
        }
        // anyone no longer in the bracket view
        for (auto it = m_avatars.begin(); it != m_avatars.end();) {
            if (!placed.contains(it->first) && it->second && !it->second->busy) {
                it->second->removeFromParent();
                it = m_avatars.erase(it);
            } else {
                ++it;
            }
        }
    }

    // ---------------------------------------------------------------- state

    void Arena::sync(State const& state) {
        bool bracket = !state.t.lobby() && state.t.size > 0 && !state.matches.empty();
        if (bracket != m_bracket || (bracket && (state.t.size != m_size2 || m_cards.size() != state.matches.size()))) {
            m_bracket = bracket;
            if (bracket) this->buildBracket(state);
        }
        m_lobbyDeco->setVisible(!m_bracket);
        m_bracketRoot->setVisible(m_bracket);
        if (m_waiting) m_waiting->setVisible(false);
        if (m_bracket) {
            this->drawLines(state);
            this->refreshCards(state);
            this->layoutBracket(state, true);
            for (auto const& m : state.matches) {
                if (m.done()) m_doneShown.insert({m.round, m.slot});
            }
        } else {
            // lobby: drop avatars of players who are gone
            std::set<int> present;
            for (auto* p : state.activePlayers()) present.insert(p->account);
            for (auto it = m_avatars.begin(); it != m_avatars.end();) {
                if (!present.contains(it->first) && it->second && !it->second->busy) {
                    it->second->removeFromParent();
                    it = m_avatars.erase(it);
                } else {
                    ++it;
                }
            }
            int i = 0;
            int count = static_cast<int>(state.activePlayers().size());
            for (auto* p : state.activePlayers()) {
                float scale;
                auto spot = this->lobbySpot(i++, count, scale);
                if (!m_avatars.contains(p->account)) {
                    auto* a = this->avatarFor(state, *p, 1.f);
                    a->setPosition(spot);
                    a->setScale(scale);
                }
            }
            this->layoutLobby(state, true);
        }
        this->tickLive(state);
    }

    void Arena::tickLive(State const& state) {
        if (!m_bracket) return;
        float sc = this->bracketScale();
        for (auto const& m : state.matches) {
            auto key = std::make_pair(m.round, m.slot);
            auto pit = m_pct.find(key);
            if (pit != m_pct.end()) {
                auto [l1, l2] = pit->second;
                if (m.live()) {
                    l1->setString(fmt::format("{}%", static_cast<int>(m.b1)).c_str());
                    l2->setString(fmt::format("{}%", static_cast<int>(m.b2)).c_str());
                    l1->setColor(m.b1 > m.b2 ? col::gold : col::white);
                    l2->setColor(m.b2 > m.b1 ? col::gold : col::white);
                    l1->setVisible(true);
                    l2->setVisible(true);
                } else if (m.done() && m.why != "bye") {
                    l1->setString(fmt::format("{}%", static_cast<int>(m.b1)).c_str());
                    l2->setString(fmt::format("{}%", static_cast<int>(m.b2)).c_str());
                    l1->setColor(m.winner == m.p1 ? col::green : col::dim);
                    l2->setColor(m.winner == m.p2 ? col::green : col::dim);
                    l1->setVisible(m.p1 != 0);
                    l2->setVisible(m.p2 != 0);
                } else {
                    l1->setVisible(false);
                    l2->setVisible(false);
                }
            }
            bool clashing = m_clashes.contains(key);
            if (m.live() && !clashing) {
                auto i1 = m_avatars.find(m.p1);
                auto i2 = m_avatars.find(m.p2);
                if (i1 != m_avatars.end() && i2 != m_avatars.end() && !i1->second->busy && !i2->second->busy) {
                    this->startClash(m.round, m.slot, i1->second, i2->second);
                }
            } else if (!m.live() && clashing) {
                this->stopClash(m.round, m.slot);
            }
        }
        (void)sc;
    }

    void Arena::startClash(int round, int slot, Avatar* a, Avatar* b) {
        a->setIdle(false);
        b->setIdle(false);
        a->stopActionByTag(0x1A70);
        b->stopActionByTag(0x1A70);
        auto* clash = Clash::create(a, b, this->seatPos(round, slot, 1), this->seatPos(round, slot, 2),
                                    this->bracketScale());
        this->addChild(clash, 30);
        m_clashes[{round, slot}] = clash;
    }

    void Arena::stopClash(int round, int slot) {
        auto it = m_clashes.find({round, slot});
        if (it == m_clashes.end()) return;
        it->second->stop();
        m_clashes.erase(it);
    }

    // ---------------------------------------------------------------- events

    void Arena::onJoin(State const& state, int account) {
        if (m_bracket) return;
        auto* p = state.player(account);
        if (!p) return;
        auto players = state.activePlayers();
        int count = static_cast<int>(players.size());
        int index = 0;
        for (int i = 0; i < count; ++i) {
            if (players[i]->account == account) index = i;
        }
        float scale;
        auto spot = this->lobbySpot(index, count, scale);
        Avatar* a = nullptr;
        if (auto it = m_avatars.find(account); it != m_avatars.end() && it->second) {
            a = it->second;
            if (a->busy) return;
        } else {
            a = this->avatarFor(state, *p, 1.f);
        }
        a->setPosition(spot);
        a->setScale(scale);
        a->setIdle(false);
        float delay = std::min(m_resultDelay, 1.2f);
        m_resultDelay += .3f;
        a->dropIn(110.f + fx::rnd(0.f, 30.f), delay, [this, account] {
            if (auto it = m_avatars.find(account); it != m_avatars.end() && it->second) it->second->setIdle(true);
        });
        this->layoutLobby(state, true);
    }

    void Arena::onLeave(State const& state, int account) {
        auto it = m_avatars.find(account);
        if (it == m_avatars.end() || !it->second) return;
        if (!m_bracket) {
            it->second->leaveAndRemove();
            m_avatars.erase(it);
            this->layoutLobby(state, true);
        } else {
            it->second->setDimmed(true, true);
        }
    }

    void Arena::onKick(State const& state, int account) {
        auto it = m_avatars.find(account);
        if (it == m_avatars.end() || !it->second) return;
        auto* a = it->second;
        m_avatars.erase(it);
        a->yeet(a->getPositionX() > m_size.width / 2.f);
        fx::shake(this, 4.f, .25f);
        if (!m_bracket) this->layoutLobby(state, true);
    }

    void Arena::onEmote(int account, int emote) {
        auto it = m_avatars.find(account);
        if (it == m_avatars.end() || !it->second) return;
        it->second->emote(emote);
    }

    CCPoint Arena::avatarPos(int account) const {
        auto it = m_avatars.find(account);
        if (it == m_avatars.end() || !it->second) return {m_size.width / 2.f, m_size.height / 2.f};
        return it->second->getPosition();
    }

    void Arena::onStart(State const& state) {
        m_bracket = true;
        this->buildBracket(state);
        m_lobbyDeco->runAction(CCFadeOut::create(.3f));
        m_lobbyDeco->setVisible(false);
        if (m_waiting) m_waiting->setVisible(false);
        m_bracketRoot->setVisible(true);
        this->drawLines(state);
        this->refreshCards(state);

        // fly everyone from the crowd to their seat
        float sc = this->bracketScale();
        int i = 0;
        for (auto const& p : state.players) {
            if (p.status == "left") continue;
            Match const* deepest = nullptr;
            for (auto const& m : state.matches) {
                if (m.has(p.account) && (!deepest || m.round > deepest->round)) deepest = &m;
            }
            if (!deepest) continue;
            auto seat = this->seatPos(deepest->round, deepest->slot, deepest->p1 == p.account ? 1 : 2);
            auto from = m_avatars.contains(p.account) ? m_avatars[p.account]->getPosition()
                                                       : ccp(m_size.width / 2.f, m_groundY);
            if (auto it = m_avatars.find(p.account); it != m_avatars.end()) {
                it->second->removeFromParent();
                m_avatars.erase(it);
            }
            auto* a = this->avatarFor(state, p, sc);
            a->setPosition(from);
            a->setScale(1.f / std::max(.4f, sc) * .85f);
            a->busy = true;
            a->setNameVisible(m_rounds <= 4);
            float delay = .25f + i * .07f;
            float t = .7f;
            a->runAction(CCSequence::create(
                CCDelayTime::create(delay),
                CallFuncExt::create([this, a] {
                    this->trail(a, iconColor(a->player().icon.p), .9f);
                    a->hop(10.f, .3f);
                }),
                CCSpawn::create(CCEaseInOut::create(CCJumpTo::create(t, seat, 60.f + fx::rnd(0.f, 40.f), 1), 1.6f),
                                CCEaseInOut::create(CCScaleTo::create(t, 1.f), 2.f), nullptr),
                CallFuncExt::create([this, a] {
                    a->squash(.8f);
                    fx::dust(this, a->getPosition(), .6f, a->getZOrder() - 1);
                    a->busy = false;
                    a->setIdle(true);
                }),
                nullptr));
            ++i;
        }
        this->runAction(CCSequence::create(
            CCDelayTime::create(.25f), CallFuncExt::create([] { fx::play("whoosh"); }),
            CCDelayTime::create(.4f + i * .07f + .3f),
            CallFuncExt::create([this] { fx::play("drum"); fx::shake(this, 3.f, .2f); }), nullptr));
        fx::banner(this->getParent() ? this->getParent() : this, "THE BRACKET IS SET!",
                   fmt::format("{} players, {} rounds", static_cast<int>(state.activePlayers().size()), m_rounds),
                   col::gold, 1.6f);
        for (auto const& m : state.matches) {
            if (m.done()) m_doneShown.insert({m.round, m.slot});
        }
    }

    void Arena::onRound(State const& state, int round) {
        this->refreshCards(state);
        for (auto const& m : state.matches) {
            if (m.round != round || !m.live()) continue;
            auto it = m_cards.find({m.round, m.slot});
            if (it == m_cards.end()) continue;
            fx::ring(this, it->second->getPosition(), col::gold, 1.f, .5f, 35);
        }
        this->tickLive(state);
    }

    void Arena::march(Avatar* a, std::vector<CCPoint> const& path, float scale, std::function<void()> done) {
        auto* steps = CCArray::create();
        CCPoint prev = a->getPosition();
        for (auto const& p : path) {
            float t = std::clamp(ccpDistance(prev, p) / 260.f, .12f, .45f);
            steps->addObject(CCEaseSineInOut::create(CCMoveTo::create(t, p)));
            prev = p;
        }
        steps->addObject(CallFuncExt::create([this, a, done] {
            a->squash(.9f);
            fx::dust(this, a->getPosition(), .6f, a->getZOrder() - 1);
            fx::play("land");
            if (done) done();
        }));
        this->trail(a, iconColor(a->player().icon.p), 1.4f);
        a->runAction(CCSequence::create(steps));
        (void)scale;
    }

    void Arena::onResult(State const& state, Event const& e) {
        if (!m_bracket) return;
        int r = jint(e.data, "r"), s = jint(e.data, "s");
        int w = jint(e.data, "w"), l = jint(e.data, "l");
        auto why = jstr(e.data, "why");
        auto key = std::make_pair(r, s);
        if (m_doneShown.contains(key) || why == "bye") {
            m_doneShown.insert(key);
            this->drawLines(state);
            this->refreshCards(state);
            this->layoutBracket(state, true);
            return;
        }
        m_doneShown.insert(key);

        auto wi = m_avatars.find(w);
        auto li = m_avatars.find(l);
        if (wi == m_avatars.end() || li == m_avatars.end()) {
            this->layoutBracket(state, true);
            return;
        }
        // keep both alive and still for the whole performance
        this->stopClash(r, s);
        Ref<Avatar> winner = wi->second;
        Ref<Avatar> loser = li->second;
        winner->busy = true;
        loser->busy = true;
        winner->setIdle(false);
        loser->setIdle(false);
        winner->stopActionByTag(0x1A70);
        loser->stopActionByTag(0x1A70);
        float delay = std::min(m_resultDelay, 3.f);
        m_resultDelay += .55f;

        auto* match = state.match(r, s);
        int winnerWhich = match && match->p1 == w ? 1 : 2;
        auto path = this->travelPath(r, s, winnerWhich);
        bool isFinal = r >= m_rounds;

        this->runAction(CCSequence::create(
            CCDelayTime::create(delay),
            CallFuncExt::create([winner, loser] {
                // wind up...
                auto back = ccp(winner->getPositionX() < loser->getPositionX() ? -6.f : 6.f, 0.f);
                winner->runAction(CCEaseOut::create(CCMoveBy::create(.16f, back), 2.f));
            }),
            CCDelayTime::create(.18f),
            CallFuncExt::create([winner, loser] {
                // ...and strike
                auto target = loser->getPosition() + ccp(winner->getPositionX() < loser->getPositionX() ? -8.f : 8.f, 0.f);
                winner->runAction(CCEaseIn::create(CCMoveTo::create(.1f, target), 2.f));
                fx::play("whoosh");
            }),
            CCDelayTime::create(.1f),
            CallFuncExt::create([this, r, s, winner, loser, why] {
                loser->knockOut();
                fx::play("hit");
                FMODAudioEngine::sharedEngine()->playEffect("explode_11.ogg");
                fx::shake(this, 5.f, .3f);
                auto mid = (winner->getPosition() + loser->getPosition()) / 2.f + ccp(0.f, 12.f);
                fx::sparks(this, mid, 12, col::goldLight, 30.f, 40);
                fx::flash(this, mid, 36.f, col::white, .9f, 41);
                auto knock = loser->getPosition() + ccp(winner->getPositionX() < loser->getPositionX() ? 10.f : -10.f, 0.f);
                loser->runAction(CCEaseOut::create(CCMoveTo::create(.2f, knock), 2.f));
                auto c = this->cardCentre(r, s);
                auto text = why == "complete" ? std::string("100%!") : reasonText(why);
                if (!text.empty()) fx::floatText(this, c + ccp(0.f, 34.f * this->bracketScale() + 10.f), text, col::gold, .45f);
            }),
            CCDelayTime::create(.28f),
            CallFuncExt::create([this, r, s, winner, winnerWhich] {
                winner->runAction(CCEaseOut::create(CCMoveTo::create(.16f, this->seatPos(r, s, winnerWhich)), 2.f));
                winner->celebrate();
                fx::stars(this, winner->getPosition() + ccp(0.f, 20.f), 6, col::gold, 26.f, 40);
                fx::play("win");
            }),
            CCDelayTime::create(.9f),
            CallFuncExt::create([this, state, winner, loser, path, isFinal] {
                loser->busy = false;
                if (isFinal || path.empty()) {
                    winner->busy = false;
                    this->drawLines(state);
                    this->refreshCards(state);
                    return;
                }
                this->drawLines(state);
                this->march(winner.data(), path, this->bracketScale(), [this, winner, state] {
                    winner->busy = false;
                    this->refreshCards(state);
                    this->layoutBracket(state, true);
                    this->tickLive(state);
                });
            }),
            nullptr));
    }

    void Arena::onChampion(State const& state, int account) {
        auto win = CCDirector::get()->getWinSize();
        auto* p = state.player(account);
        std::string name = p ? p->name : std::string("???");
        float delay = std::min(m_resultDelay, 3.f) + 1.6f;
        this->runAction(CCSequence::create(
            CCDelayTime::create(delay),
            CallFuncExt::create([this, account, name, win] {
                auto it = m_avatars.find(account);
                Avatar* a = it != m_avatars.end() ? it->second : nullptr;
                if (a && m_trophy) {
                    a->busy = true;
                    a->setIdle(false);
                    auto top = m_trophy->getPosition() + ccp(0.f, 18.f);
                    this->trail(a, col::gold, 1.2f);
                    a->runAction(CCSequence::create(
                        CCEaseInOut::create(CCJumpTo::create(.8f, top, 50.f, 1), 1.6f),
                        CallFuncExt::create([this, a] {
                            a->squash(1.f);
                            auto* crown = fx("crown");
                            crown->setScale(.7f + .5f * a->iconScale());
                            crown->setPosition({0.f, a->height() + 60.f});
                            a->addChild(crown, 30);
                            crown->runAction(CCEaseBounceOut::create(
                                CCMoveTo::create(.5f, {0.f, a->height() + 6.f * a->iconScale()})));
                            a->busy = false;
                            a->setIdle(true);
                            a->celebrate();
                        }),
                        nullptr));
                }
                if (m_rays) {
                    m_rays->runAction(CCSpawn::create(CCFadeTo::create(.6f, 200), CCScaleTo::create(.6f, 1.6f), nullptr));
                }
                fx::confetti(this, {0.f, win.height, win.width, 30.f}, 140, 80);
                fx::screenFlash(this, col::gold, .35f, .6f);
                fx::play("win");
                FMODAudioEngine::sharedEngine()->playEffect("achievement_01.ogg");
                fx::banner(this->getParent() ? this->getParent() : this, "CHAMPION!", name, col::gold, 3.f);
            }),
            nullptr));
    }

    // ---------------------------------------------------------------- hit tests

    int Arena::avatarAt(CCPoint p) const {
        int best = 0;
        float bestD = 1e9f;
        for (auto const& [acc, a] : m_avatars) {
            if (!a || !a->isVisible()) continue;
            auto centre = a->getPosition() + ccp(0.f, a->height() * .5f * a->getScale());
            float r = std::max(14.f, a->height() * .75f * a->getScale());
            float d = ccpDistance(centre, p);
            if (d < r && d < bestD) {
                bestD = d;
                best = acc;
            }
        }
        return best;
    }

    bool Arena::matchAt(CCPoint p, int& round, int& slot) const {
        if (!m_bracket) return false;
        for (auto const& [key, card] : m_cards) {
            auto box = card->boundingBox();
            if (box.containsPoint(p)) {
                round = key.first;
                slot = key.second;
                return true;
            }
        }
        return false;
    }
}
