#include "Arena.hpp"
#include "Fx.hpp"
#include "Theme.hpp"

#include <cmath>

using namespace geode::prelude;

namespace nt::ui {
    namespace {
        constexpr float kTopPad = 64.f;     // room for the top bar + round labels
        constexpr float kBottomPad = 48.f;  // room for the bottom bar
        constexpr float kRest = Avatar::kSwordRest;
        constexpr int kMoveTag = 0x1A70;
        constexpr float kWalkSpeed = 125.f;   // lobby walking, points per second
        constexpr float kMarchSpeed = 185.f;  // a winner walking the bracket path
        constexpr float kEdge = 26.f;         // how close to the screen edge you can walk
        constexpr float kBackRow = 46.f;      // the back row stands this high, on the bleachers
        constexpr float kPi = 3.14159265f;
        constexpr float kKnockBack = 10.f;    // how far the final blow throws the loser (x scale)

        float clamp01(float x) { return std::clamp(x, 0.f, 1.f); }
        float seg(float ph, float a, float b) { return clamp01((ph - a) / (b - a)); }
        float smooth(float x) { return x * x * (3.f - 2.f * x); }
        float outQuad(float x) { return 1.f - (1.f - x) * (1.f - x); }
        float inQuad(float x) { return x * x; }
        float lerpf(float a, float b, float t) { return a + (b - a) * t; }
        CCPoint lerpp(CCPoint a, CCPoint b, float t) { return a + (b - a) * t; }

        // the swoosh a swing leaves: a crescent around the hand, out at the blade's reach
        void slashFx(CCNode* parent, CCPoint hand, int facing, float midAngle, float reach, ccColor3B color,
                     float alpha, int z) {
            if (!parent) return;
            auto* s = fx("slash");
            s->setAnchorPoint({.5f, 18.f / 48.f});  // the crescent's centre of curvature
            s->setPosition(hand);
            s->setRotation(midAngle * facing);
            float k = reach / 21.f;
            s->setScale(k * .9f);
            s->setColor(color);
            s->setOpacity(static_cast<GLubyte>(255.f * clamp01(alpha)));
            s->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            parent->addChild(s, z);
            s->runAction(CCSequence::create(
                CCSpawn::create(CCEaseOut::create(CCScaleTo::create(.2f, k * 1.12f), 2.f),
                                CCSequence::create(CCDelayTime::create(.05f), CCFadeOut::create(.17f), nullptr), nullptr),
                CCRemoveSelf::create(), nullptr));
        }

        // where two blades cross (or the point between their tips when they miss)
        CCPoint crossing(CCPoint p1, CCPoint p2, CCPoint q1, CCPoint q2) {
            auto r = p2 - p1, s = q2 - q1;
            float den = r.x * s.y - r.y * s.x;
            if (std::abs(den) > 1e-4f) {
                auto qp = q1 - p1;
                float t = (qp.x * s.y - qp.y * s.x) / den;
                float u = (qp.x * r.y - qp.y * r.x) / den;
                if (t >= 0.f && t <= 1.f && u >= 0.f && u <= 1.f) return p1 + r * t;
            }
            return (p2 + q2) / 2.f;
        }
    }

    // ---------------------------------------------------------------- Duel

    // Two players trading sword blows while their match is live: wind-up, lunge, the blades
    // meet (or the blow lands), recover. Whoever is further into the level swings more often
    // and lands more of their hits, so the fight on the map follows the real one.
    class Duel : public CCNode {
    public:
        static Duel* create(Avatar* a, Avatar* b, CCPoint seatA, CCPoint seatB, float scale, float reach) {
            auto* ret = new Duel();
            if (!ret->init()) {
                delete ret;
                return nullptr;
            }
            ret->autorelease();
            ret->setup(a, b, seatA, seatB, scale, reach);
            return ret;
        }

        void setLead(float lead) { m_lead = std::clamp(lead, -1.f, 1.f); }

        void stop() {
            if (auto a = m_a.lock()) this->release(a.data(), m_sa);
            if (auto b = m_b.lock()) this->release(b.data(), m_sb);
            this->unscheduleUpdate();
            this->removeFromParent();
        }

    protected:
        enum class Kind { Swing, Standoff };

        void setup(Avatar* a, Avatar* b, CCPoint sa, CCPoint sb, float scale, float reach) {
            m_a = WeakRef<Avatar>(a);
            m_b = WeakRef<Avatar>(b);
            m_sa = sa;
            m_sb = sb;
            m_scale = scale;
            m_reach = reach;
            for (auto* av : {a, b}) {
                av->busy = true;
                av->stopActionByTag(kMoveTag);
                av->setIdle(true);
            }
            a->setPosition(sa);
            b->setPosition(sb);
            if (a->hasSword()) a->setFacing(1);
            else a->setSword(true, 1);
            if (b->hasSword()) b->setFacing(-1);
            else b->setSword(true, -1);
            // a beat of sizing each other up first, out of step with the other duels
            m_kind = Kind::Standoff;
            m_period = fx::rnd(.45f, .95f);
            m_hopper = fx::rndi(0, 1);
            m_attacker = fx::rndi(0, 1);
            this->scheduleUpdate();
        }

        void release(Avatar* av, CCPoint seat) {
            av->setPosition(seat);
            av->swordRest(.15f);
            av->busy = false;
        }

        void next() {
            m_impact = m_swished = m_hopped = false;
            // now and then they just circle each other (never twice in a row)
            if (m_kind == Kind::Swing && fx::rndi(0, 5) == 0) {
                m_kind = Kind::Standoff;
                m_period = fx::rnd(.55f, .85f);
                m_shuffleA = fx::rnd(.4f, 1.f);
                m_shuffleB = fx::rnd(.4f, 1.f);
                m_hopper = fx::rndi(0, 1);
                return;
            }
            m_kind = Kind::Swing;
            float pA = std::clamp(.5f + .3f * m_lead, .2f, .8f);
            int who = fx::rnd(0.f, 1.f) < pA ? 0 : 1;
            m_streak = who == m_attacker ? m_streak + 1 : 1;
            if (m_streak > 3) {
                who = 1 - who;
                m_streak = 1;
            }
            m_attacker = who;
            float lead = who == 0 ? m_lead : -m_lead;
            m_hit = fx::rnd(0.f, 1.f) < std::clamp(.22f + .34f * lead, .08f, .6f);
            m_end = m_hit ? 116.f : 60.f;
            m_period = fx::rnd(1.f, 1.3f);
        }

        void update(float dt) override {
            auto a = m_a.lock();
            auto b = m_b.lock();
            if (!a || !b) return;
            m_t += dt;
            if (m_t >= m_period) {
                m_t -= m_period;
                this->next();
            }
            float ph = m_t / m_period;
            float sc = m_scale;
            float fwdA = 0.f, fwdB = 0.f, angA = kRest, angB = kRest;
            bool impactNow = false;

            CCPoint handA {0.f, 0.f}, handB {0.f, 0.f};
            float leanA = 0.f, leanB = 0.f;

            if (m_kind == Kind::Standoff) {
                // a half step back with the blade up, then in again
                float s = std::sin(ph * kPi);
                fwdA = -2.2f * sc * s * m_shuffleA;
                fwdB = -2.2f * sc * s * m_shuffleB;
                angA = lerpf(kRest, 50.f, .8f * s * m_shuffleA);
                angB = lerpf(kRest, 50.f, .8f * s * m_shuffleB);
                leanA = -4.f * s * m_shuffleA;
                leanB = -4.f * s * m_shuffleB;
                if (!m_hopped && ph > .15f) {
                    m_hopped = true;
                    (m_hopper == 0 ? a : b)->hop(6.f * std::max(.6f, sc), .26f);
                }
            } else {
                float gap = std::abs(m_sb.x - m_sa.x);
                // a blow that lands carries the blade into the other body; a blocked one
                // meets the other blade halfway
                float lunge = m_hit ? std::clamp(gap - 43.f * sc, 2.f * sc, gap * .45f) : gap * .18f;
                float back = -3.5f * sc;
                CCPoint up {-14.5f, 16.f};  // hand raised over the head
                CCPoint thrown {2.f, .5f};  // hand thrown forward at the end of the swing
                float atkFwd, atkAng, atkLean;
                CCPoint atkHand;
                if (ph < .26f) {
                    // wind-up: lean away, sword raised up behind the head
                    float u = outQuad(seg(ph, 0.f, .26f));
                    atkFwd = back * u;
                    atkAng = lerpf(kRest, -55.f, u);
                    atkHand = lerpp({0.f, 0.f}, up, u);
                    atkLean = -9.f * u;
                } else if (ph < .34f) {
                    // the strike: lunge in, blade whips over and down
                    float u = inQuad(seg(ph, .26f, .34f));
                    atkFwd = lerpf(back, lunge, u);
                    atkAng = lerpf(-55.f, m_end, u);
                    atkHand = lerpp(up, thrown, u);
                    atkLean = lerpf(-9.f, 10.f, u);
                } else if (ph < .48f) {
                    atkFwd = lunge;
                    // a blocked blade bounces off the other one; a landed one follows through
                    atkAng = m_end - (m_hit ? -6.f : 18.f) * std::sin(seg(ph, .34f, .48f) * kPi);
                    atkHand = thrown;
                    atkLean = 10.f;
                } else {
                    float u = smooth(seg(ph, .48f, .84f));
                    atkFwd = lerpf(lunge, 0.f, u);
                    atkAng = lerpf(m_end, kRest, u);
                    atkHand = lerpp(thrown, {0.f, 0.f}, u);
                    atkLean = lerpf(10.f, 0.f, u);
                }
                // the defender brings the blade up to meet it, or is too late
                float guard = m_hit ? 16.f : 46.f;
                float defAng;
                if (ph < .12f) defAng = kRest;
                else if (ph < .3f) defAng = lerpf(kRest, guard, smooth(seg(ph, .12f, .3f)));
                else if (ph < .52f) defAng = guard;
                else defAng = lerpf(guard, kRest, smooth(seg(ph, .52f, .84f)));
                float defFwd = 0.f, defLean = 0.f;
                float after = (ph - .34f) * m_period;  // seconds since the blow
                if (after > 0.f) {
                    defAng += (m_hit ? -42.f : -16.f) * std::exp(-after * 9.f);
                    float rise = 1.f - std::exp(-after * 45.f);
                    float push = (m_hit ? 6.5f : 2.8f) * sc;
                    defFwd = -push * rise * std::exp(-after * 4.5f);
                    defLean = (m_hit ? -14.f : -5.f) * rise * std::exp(-after * 6.f);
                }
                if (!m_swished && ph >= .26f) {
                    m_swished = true;
                    fx::playLimited("swish", .3f, .3f, fx::rnd(.9f, 1.15f));
                }
                if (!m_impact && ph >= .34f) {
                    m_impact = true;
                    impactNow = true;
                }
                if (m_attacker == 0) {
                    fwdA = atkFwd, angA = atkAng, handA = atkHand, leanA = atkLean;
                    fwdB = defFwd, angB = defAng, leanB = defLean;
                } else {
                    fwdB = atkFwd, angB = atkAng, handB = atkHand, leanB = atkLean;
                    fwdA = defFwd, angA = defAng, leanA = defLean;
                }
            }
            // a faces right, b faces left: "forward" is +x for a and -x for b
            a->setPosition(m_sa + ccp(fwdA, 0.f));
            b->setPosition(m_sb - ccp(fwdB, 0.f));
            a->setSwordAngle(angA);
            b->setSwordAngle(angB);
            a->setHandOffset(handA);
            b->setHandOffset(handB);
            a->setLean(leanA);
            b->setLean(leanB);
            if (impactNow) {
                if (m_attacker == 0) this->impact(a.data(), b.data(), 1);
                else this->impact(b.data(), a.data(), -1);
            }
        }

        void impact(Avatar* atk, Avatar* def, int dir) {
            auto* parent = this->getParent();
            if (!parent) return;
            float k = std::max(.6f, m_scale);
            constexpr int z = 40;
            if (m_hit) {
                // the blow lands: a slash across them and a flinch
                slashFx(parent, atk->handWorld(), dir, 22.f, m_reach, col::white, .9f, z);
                def->flinch();
                auto body = def->getPosition() + ccp(0.f, 15.f * m_scale);
                fx::flash(parent, body, 13.f * k, col::white, .6f, z);
                fx::sparks(parent, body, 4, iconColor(def->player().icon.p), 16.f * k, z);
                fx::playLimited("slice", .45f, .35f, fx::rnd(.92f, 1.1f));
            } else {
                // blade meets blade
                auto p = crossing(atk->handWorld(), atk->swordTipWorld(), def->handWorld(), def->swordTipWorld());
                fx::sparks(parent, p, 7, col::goldLight, 20.f * k, z);
                fx::flash(parent, p, 8.f * k, col::white, .9f, z);
                fx::playLimited("clang", .42f, .38f, fx::rnd(.9f, 1.12f));
            }
        }

        WeakRef<Avatar> m_a, m_b;
        CCPoint m_sa, m_sb;
        float m_scale = 1.f, m_reach = 25.f;
        Kind m_kind = Kind::Standoff;
        float m_t = 0.f, m_period = 1.f;
        float m_lead = 0.f;  // > 0: a is ahead
        int m_attacker = 0;  // 0: a swings at b
        int m_streak = 0;
        bool m_hit = false;
        float m_end = 60.f;
        bool m_impact = false, m_swished = false, m_hopped = false;
        float m_shuffleA = .7f, m_shuffleB = .7f;
        int m_hopper = 0;
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
        m_litTrail = CCDrawNode::create();
        m_bracketRoot->addChild(m_litTrail, 1);
        m_ghosts = CCNode::create();
        m_bracketRoot->addChild(m_ghosts, 2);
        m_labels = CCNode::create();
        m_bracketRoot->addChild(m_labels, 3);

        this->scheduleUpdate();
        return true;
    }

    void Arena::update(float dt) {
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
        m_jumpCooldown = std::max(0.f, m_jumpCooldown - dt);
        this->updateMarches(dt);
        if (!m_bracket) this->updateWalkers(dt);
    }

    Avatar* Arena::avatarFor(Player const& p, float scale) {
        auto it = m_avatars.find(p.account);
        if (it != m_avatars.end() && it->second) {
            it->second->setPlayer(p);
            return it->second;
        }
        auto* a = Avatar::create(p, scale, true);
        a->setYou(p.account == m_me);
        this->addChild(a, p.account == m_me ? 11 : 10);
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
        float y = m_groundY + (row == 1 ? kBackRow : 0.f);
        return {x, y};
    }

    float Arena::frontScale(int count) const {
        float scale = 1.f;
        this->lobbySpot(0, std::max(1, count), scale);
        return scale;
    }

    float Arena::lobbyX(float x01) const {
        return kEdge + clamp01(x01) * (m_size.width - 2.f * kEdge);
    }

    float Arena::myX01() const {
        auto it = m_avatars.find(m_me);
        if (it == m_avatars.end() || !it->second) return .5f;
        return clamp01((it->second->getPositionX() - kEdge) / std::max(1.f, m_size.width - 2.f * kEdge));
    }

    bool Arena::canMove() const {
        if (m_bracket || !m_me) return false;
        auto it = m_avatars.find(m_me);
        return it != m_avatars.end() && it->second;
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
            float y = m_groundY + kBackRow;
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

        float front = this->frontScale(count);
        int i = 0;
        for (auto* p : players) {
            float scale;
            auto spot = this->lobbySpot(i++, count, scale);
            auto it = m_avatars.find(p->account);
            if (it == m_avatars.end() || !it->second) continue;
            auto* a = it->second;
            a->setPlayer(*p);
            bool free = m_walkers.contains(p->account);
            a->setZOrder(p->account == m_me ? 11 : (!free && spot.y > m_groundY + 1.f) ? 8 : 10);
            if (a->busy) continue;
            a->setIdle(true);
            if (free) {
                // walkers keep their own spot on the ground, at the crowd's size
                a->setScale(front);
                a->setAutoHop(false);
                continue;
            }
            a->stopActionByTag(kMoveTag);
            if (animate && ccpDistance(a->getPosition(), spot) > 1.f) {
                auto* move = CCEaseInOut::create(CCMoveTo::create(.4f, spot), 2.f);
                move->setTag(kMoveTag);
                a->runAction(move);
            } else {
                a->setPosition(spot);
            }
            a->setScale(scale);
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

    void Arena::freeWalker(int account, Avatar* a) {
        if (m_walkers.contains(account)) return;
        m_walkers[account] = Walker {};
        a->setAutoHop(false);
        a->setZOrder(account == m_me ? 11 : 10);
        if (a->busy || a->getPositionY() <= m_groundY + 1.f) return;
        // down from the bleachers onto the ground first
        float front = this->frontScale(static_cast<int>(m_state.activePlayers().size()));
        a->busy = true;
        a->stopActionByTag(kMoveTag);
        a->setIdle(false);
        a->runAction(CCSequence::create(
            CCSpawn::create(CCJumpTo::create(.36f, {a->getPositionX(), m_groundY}, 14.f, 1),
                            CCScaleTo::create(.36f, front), nullptr),
            CallFuncExt::create([this, a] {
                a->squash(.7f);
                fx::dust(this, a->getPosition(), .5f, a->getZOrder() - 1);
                fx::play("land", .6f);
                a->busy = false;
                a->setIdle(true);
            }),
            nullptr));
    }

    void Arena::applyPositions(State const& state, std::set<int> const& fresh) {
        if (m_bracket) return;
        for (auto const& [acc, pj] : state.pos) {
            float x01 = pj.first;
            int jumps = pj.second;
            auto it = m_avatars.find(acc);
            if (it == m_avatars.end() || !it->second) continue;
            auto* a = it->second;
            float x = this->lobbyX(x01);
            bool snap = fresh.contains(acc);
            if (acc == m_me) {
                // we drive our own avatar; the server only remembers where we were
                m_myJumps = std::max(m_myJumps, jumps);
                if (snap && !m_walkers.contains(acc)) {
                    this->freeWalker(acc, a);
                    if (!a->busy) a->setPosition({x, m_groundY});
                }
                continue;
            }
            bool first = !m_walkers.contains(acc);
            if (first) this->freeWalker(acc, a);
            auto& w = m_walkers[acc];
            if (snap && !a->busy) {
                a->setPosition({x, m_groundY});
                w.target = -1.f;
            } else {
                w.target = x;
            }
            if (!first && w.jumps >= 0 && jumps > w.jumps && !a->busy && a->onGround()) {
                a->hop(15.f, .36f);
            }
            w.jumps = jumps;
        }
    }

    void Arena::steer(int dir) {
        m_dir = std::clamp(dir, -1, 1);
        if (m_dir && this->canMove()) {
            this->freeWalker(m_me, m_avatars[m_me]);
            m_myTarget = -1.f;
        }
    }

    void Arena::jumpMe() {
        if (!this->canMove() || m_jumpCooldown > 0.f) return;
        auto* a = m_avatars[m_me];
        this->freeWalker(m_me, a);
        if (a->busy || !a->onGround()) return;
        a->hop(15.f, .36f);
        m_jumpCooldown = .38f;
        ++m_myJumps;
        m_myMoved = true;
    }

    void Arena::walkMeTo(float x) {
        if (!this->canMove()) return;
        this->freeWalker(m_me, m_avatars[m_me]);
        m_myTarget = std::clamp(x, kEdge, m_size.width - kEdge);
    }

    void Arena::updateWalkers(float dt) {
        for (auto& [acc, w] : m_walkers) {
            auto it = m_avatars.find(acc);
            if (it == m_avatars.end() || !it->second) continue;
            auto* a = it->second;
            if (a->busy) continue;
            float x = a->getPositionX();
            float nx = x;
            if (acc == m_me) {
                if (m_dir) {
                    nx = x + m_dir * kWalkSpeed * dt;
                    m_myTarget = -1.f;
                } else if (m_myTarget >= 0.f) {
                    float d = m_myTarget - x;
                    float step = kWalkSpeed * dt;
                    if (std::abs(d) <= step) {
                        nx = m_myTarget;
                        m_myTarget = -1.f;
                    } else {
                        nx = x + (d > 0.f ? step : -step);
                    }
                }
            } else if (w.target >= 0.f) {
                // catch up faster the further behind they are, so nobody lags a poll behind
                float d = w.target - x;
                float step = std::max(kWalkSpeed, std::abs(d) * 2.2f) * dt;
                if (std::abs(d) <= step) {
                    nx = w.target;
                    w.target = -1.f;
                } else {
                    nx = x + (d > 0.f ? step : -step);
                }
            }
            nx = std::clamp(nx, kEdge, m_size.width - kEdge);
            bool moving = std::abs(nx - x) > .01f;
            if (moving) {
                a->setPositionX(nx);
                if (acc == m_me) m_myMoved = true;
            }
            a->setWalking(moving, moving ? (nx > x ? 1 : -1) : 0);
        }
    }

    // ---------------------------------------------------------------- bracket geometry

    float Arena::bracketScale() const {
        // the default per bracket size, then shrunk until every pair fits its column and row
        float base = m_rounds <= 1 ? 1.15f : m_rounds == 2 ? .95f : m_rounds == 3 ? .78f : m_rounds == 4 ? .6f : .48f;
        int cols = std::max(1, 2 * m_rounds - 1);
        float colW = (m_size.width - 32.f) / cols;
        int perSide = std::max(1, (m_size2 >> 1) / 2);
        float step = (m_size.height - kTopPad - kBottomPad) / perSide;
        float byW = (colW * .92f - 32.f) / 60.f;
        float byH = (step * .9f - 10.f) / 30.f;
        return std::clamp(std::min({base, byW, byH}), .3f, 1.2f);
    }

    float Arena::halfGap() const {
        return 10.f + 15.f * this->bracketScale();
    }

    float Arena::reach() const {
        // hand to blade tip: the sword art's blade (41 pt of 52) at the avatar's sword scale
        return 52.f * .79f * .62f * this->bracketScale();
    }

    int Arena::sideOf(int round, int slot) const {
        if (round >= m_rounds) return 0;
        int perSide = (m_size2 >> round) / 2;
        return slot < perSide ? -1 : 1;
    }

    CCPoint Arena::matchCentre(int round, int slot) const {
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
        auto a = this->matchCentre(round - 1, slot * 2);
        auto b = this->matchCentre(round - 1, slot * 2 + 1);
        return {x, (a.y + b.y) / 2.f};
    }

    float Arena::feetY(int round, int slot) const {
        return this->matchCentre(round, slot).y - 15.f * this->bracketScale();
    }

    CCPoint Arena::seatPos(int round, int slot, int which) const {
        auto c = this->matchCentre(round, slot);
        float d = this->halfGap();
        return {c.x + (which == 1 ? -d : d), c.y - 15.f * this->bracketScale()};
    }

    std::vector<CCPoint> Arena::linePath(int round, int slot) const {
        if (round >= m_rounds) return {};
        float sc = this->bracketScale();
        float d = this->halfGap();
        float body = 15.f * sc + 3.f;
        int side = this->sideOf(round, slot);
        auto c = this->matchCentre(round, slot);
        auto n = this->matchCentre(round + 1, slot / 2);
        float y0 = c.y - 15.f * sc, y1 = n.y - 15.f * sc;
        float out = side < 0 ? c.x + d + body : c.x - d - body;  // this pair's outer edge
        float in = side < 0 ? n.x - d - body : n.x + d + body;   // the next pair's near edge
        float midX = (out + in) / 2.f;
        return {{out, y0}, {midX, y0}, {midX, y1}, {in, y1}};
    }

    void Arena::buildBracket(State const& state) {
        m_size2 = std::max(2, state.t.size);
        m_rounds = std::max(1, state.t.rounds);
        for (auto& [k, duel] : m_duels) duel->stop();
        m_duels.clear();
        for (auto& [k, glow] : m_glows) glow->removeFromParent();
        m_glows.clear();
        m_pct.clear();
        m_matchKeys.clear();
        m_labels->removeAllChildren();
        if (m_trophy) m_trophy->removeFromParent();
        if (m_rays) m_rays->removeFromParent();
        m_trophy = nullptr;
        m_rays = nullptr;

        float sc = this->bracketScale();
        for (auto const& m : state.matches) {
            m_matchKeys.insert({m.round, m.slot});
            float ls = std::clamp(.16f + .14f * sc, .18f, .32f);
            auto* p1 = label("", "bigFont.fnt", ls, col::white);
            auto* p2 = label("", "bigFont.fnt", ls, col::white);
            p1->setPosition(this->seatPos(m.round, m.slot, 1) + ccp(0.f, 30.f * sc + 6.f));
            p2->setPosition(this->seatPos(m.round, m.slot, 2) + ccp(0.f, 30.f * sc + 6.f));
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
                auto c = this->matchCentre(r, slot);
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
        auto fc = this->matchCentre(m_rounds, 0);
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
        float d = this->halfGap();
        float body = 15.f * sc + 3.f;
        for (auto const& m : state.matches) {
            // the ground the pair stands on (a faint line, part of the path)
            auto c = this->matchCentre(m.round, m.slot);
            float fy = c.y - 15.f * sc;
            m_lines->drawSegment({c.x - d - body, fy}, {c.x + d + body, fy}, 1.f, c4f(col::blue, .2f));

            auto path = this->linePath(m.round, m.slot);
            if (path.size() < 2) continue;
            auto key = std::make_pair(m.round, m.slot);
            bool lit = m.done() && m.winner && !m_marching.contains(key);
            if (lit) {
                for (size_t i = 1; i < path.size(); ++i) {
                    m_lines->drawSegment(path[i - 1], path[i], 3.6f, c4f(col::gold, .13f));
                }
            }
            auto color = lit ? c4f(col::gold, .9f) : c4f(col::blue, .3f);
            for (size_t i = 1; i < path.size(); ++i) {
                m_lines->drawSegment(path[i - 1], path[i], lit ? 1.5f : 1.f, color);
            }
        }
    }

    void Arena::refreshMatches(State const& state) {
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
            // a faint ghost where they already won
            for (auto const& m : state.matches) {
                if (!m.has(p.account) || m.round >= deepest->round) continue;
                auto* g = SimplePlayer::create(1);
                g->updatePlayerFrame(std::max(1, p.icon.cube), IconType::Cube);
                g->setColor(iconColor(p.icon.p));
                g->setSecondColor(iconColor(p.icon.s));
                g->setOpacity(70);
                g->setScale(sc * .8f);
                g->setPosition(this->seatPos(m.round, m.slot, m.p1 == p.account ? 1 : 2) + ccp(0.f, 13.f * sc));
                m_ghosts->addChild(g);
            }
            placed.insert(p.account);
            auto* a = this->avatarFor(p, sc);
            if (a->busy) continue;
            if (std::abs(a->iconScale() - sc) > .01f) {
                // rebuild at the bracket's scale
                auto pos = a->getPosition();
                m_avatars.erase(p.account);
                a->removeFromParent();
                a = this->avatarFor(p, sc);
                a->setPosition(pos);
            }
            bool out = (deepest->done() && deepest->winner && deepest->winner != p.account) || p.status == "left";
            bool champ = state.t.finished() && state.t.champ == p.account;
            a->stopActionByTag(kMoveTag);
            if (champ && m_trophy) seat = m_trophy->getPosition() + ccp(0.f, 18.f);
            // the beaten lie where the final blow knocked them
            if (out && deepest->done() && deepest->why != "bye") seat.x += (which == 1 ? 1.f : -1.f) * kKnockBack * sc;
            a->setZOrder(out ? 9 : 10);
            if (animate && ccpDistance(a->getPosition(), seat) > 1.f) {
                auto* move = CCEaseInOut::create(CCMoveTo::create(.45f, seat), 2.f);
                move->setTag(kMoveTag);
                a->runAction(move);
            } else {
                a->setPosition(seat);
            }
            a->setScale(1.f);
            a->setNameVisible(m_rounds <= 3);
            a->setDimmed(out, false);
            a->setIdle(!out);
            a->setAutoHop(!out);
            int facing = which == 1 ? 1 : -1;
            if (out) {
                a->setSword(false, facing);
            } else if (!a->hasSword()) {
                a->setSword(true, facing);
            } else {
                a->setFacing(facing);
            }
            if (champ) {
                a->setFacing(1);
                a->setSwordAngle(-6.f);
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
        m_state = state;
        bool bracket = !state.t.lobby() && state.t.size > 0 && !state.matches.empty();
        if (bracket != m_bracket || (bracket && (state.t.size != m_size2 || m_matchKeys.size() != state.matches.size()))) {
            m_bracket = bracket;
            if (bracket) {
                this->buildBracket(state);
                m_walkers.clear();
                m_dir = 0;
                m_myTarget = -1.f;
            }
        }
        m_lobbyDeco->setVisible(!m_bracket);
        m_bracketRoot->setVisible(m_bracket);
        if (m_waiting) m_waiting->setVisible(false);
        if (m_bracket) {
            this->drawLines(state);
            this->refreshMatches(state);
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
                    m_walkers.erase(it->first);
                    it = m_avatars.erase(it);
                } else {
                    ++it;
                }
            }
            std::set<int> created;
            int i = 0;
            int count = static_cast<int>(state.activePlayers().size());
            for (auto* p : state.activePlayers()) {
                float scale;
                auto spot = this->lobbySpot(i++, count, scale);
                if (!m_avatars.contains(p->account)) {
                    auto* a = this->avatarFor(*p, 1.f);
                    a->setPosition(spot);
                    a->setScale(scale);
                    created.insert(p->account);
                }
            }
            this->layoutLobby(state, true);
            // fresh avatars appear right where their players are standing
            this->applyPositions(state, created);
            return;
        }
        this->tickLive(state);
    }

    void Arena::tickLive(State const& state) {
        m_state = state;
        if (!m_bracket) {
            this->applyPositions(state, {});
            return;
        }
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
            auto dit = m_duels.find(key);
            if (m.live()) {
                if (dit == m_duels.end()) {
                    auto i1 = m_avatars.find(m.p1);
                    auto i2 = m_avatars.find(m.p2);
                    if (i1 != m_avatars.end() && i2 != m_avatars.end() && i1->second && i2->second &&
                        !i1->second->busy && !i2->second->busy) {
                        this->startDuel(m.round, m.slot, i1->second, i2->second);
                        dit = m_duels.find(key);
                    }
                }
                if (dit != m_duels.end()) {
                    // the one further into the level presses the fight
                    dit->second->setLead((m.b1 - m.b2) / 30.f);
                }
            } else if (dit != m_duels.end()) {
                this->stopDuel(m.round, m.slot);
            }
        }
    }

    void Arena::startDuel(int round, int slot, Avatar* a, Avatar* b) {
        auto key = std::make_pair(round, slot);
        auto sa = this->seatPos(round, slot, 1);
        auto sb = this->seatPos(round, slot, 2);
        auto* duel = Duel::create(a, b, sa, sb, this->bracketScale(), this->reach());
        if (!duel) return;
        this->addChild(duel, 30);
        m_duels[key] = duel;

        // a soft pool of light under a live fight
        if (!m_glows.contains(key)) {
            auto* glow = fx("glow");
            glow->setColor(col::gold);
            glow->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            float w = (sb.x - sa.x) + 40.f * this->bracketScale();
            float base = glow->getContentSize().width;
            glow->setScaleX(w / base * 1.2f);
            glow->setScaleY(w / base * .3f);
            glow->setPosition({(sa.x + sb.x) / 2.f, sa.y + 1.f});
            glow->setOpacity(90);
            glow->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCFadeTo::create(.8f, 60)),
                CCEaseSineInOut::create(CCFadeTo::create(.8f, 125)), nullptr)));
            m_bracketRoot->addChild(glow, 1);
            m_glows[key] = glow;
        }
    }

    void Arena::stopDuel(int round, int slot) {
        auto key = std::make_pair(round, slot);
        if (auto it = m_duels.find(key); it != m_duels.end()) {
            it->second->stop();
            m_duels.erase(it);
        }
        if (auto it = m_glows.find(key); it != m_glows.end()) {
            it->second->stopAllActions();
            it->second->runAction(CCSequence::create(CCFadeOut::create(.4f), CCRemoveSelf::create(), nullptr));
            m_glows.erase(it);
        }
    }

    // ---------------------------------------------------------------- events

    void Arena::onJoin(State const& state, int account) {
        m_state = state;
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
            a = this->avatarFor(*p, 1.f);
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
        m_state = state;
        auto it = m_avatars.find(account);
        if (it == m_avatars.end() || !it->second) return;
        if (!m_bracket) {
            it->second->leaveAndRemove();
            m_avatars.erase(it);
            m_walkers.erase(account);
            this->layoutLobby(state, true);
        } else {
            it->second->setDimmed(true, true);
            it->second->setSword(false, it->second->facing());
        }
    }

    void Arena::onKick(State const& state, int account) {
        m_state = state;
        auto it = m_avatars.find(account);
        if (it == m_avatars.end() || !it->second) return;
        auto* a = it->second;
        m_avatars.erase(it);
        m_walkers.erase(account);
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
        m_state = state;
        m_bracket = true;
        m_walkers.clear();
        m_dir = 0;
        m_myTarget = -1.f;
        this->buildBracket(state);
        m_lobbyDeco->setVisible(false);
        if (m_waiting) m_waiting->setVisible(false);
        m_bracketRoot->setVisible(true);
        this->drawLines(state);
        this->refreshMatches(state);

        // everyone jumps from the crowd to their spot on the map and draws a sword
        float sc = this->bracketScale();
        int i = 0;
        for (auto const& p : state.players) {
            if (p.status == "left") continue;
            Match const* deepest = nullptr;
            for (auto const& m : state.matches) {
                if (m.has(p.account) && (!deepest || m.round > deepest->round)) deepest = &m;
            }
            if (!deepest) continue;
            int which = deepest->p1 == p.account ? 1 : 2;
            auto seat = this->seatPos(deepest->round, deepest->slot, which);
            CCPoint from {m_size.width / 2.f, m_groundY};
            float fromScale = 1.f;
            if (auto it = m_avatars.find(p.account); it != m_avatars.end() && it->second) {
                from = it->second->getPosition();
                fromScale = it->second->getScale();
                it->second->removeFromParent();
                m_avatars.erase(it);
            }
            auto* a = this->avatarFor(p, sc);
            a->setPosition(from);
            a->setScale(fromScale / std::max(.3f, sc));
            a->busy = true;
            a->setIdle(false);
            a->setNameVisible(m_rounds <= 3);
            int facing = which == 1 ? 1 : -1;
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
                CallFuncExt::create([this, a, facing] {
                    a->squash(.8f);
                    fx::dust(this, a->getPosition(), .6f, a->getZOrder() - 1);
                    a->swordDraw(facing, .08f);
                    a->busy = false;
                    a->setIdle(true);
                }),
                nullptr));
            ++i;
        }
        this->runAction(CCSequence::create(
            CCDelayTime::create(.25f), CallFuncExt::create([] { fx::play("whoosh"); }),
            CCDelayTime::create(.4f + i * .07f + .3f),
            CallFuncExt::create([this] {
                fx::play("drum");
                fx::shake(this, 3.f, .2f);
            }),
            nullptr));
        fx::banner(this->getParent() ? this->getParent() : this, "THE BRACKET IS SET!",
                   fmt::format("{} players, {} rounds", static_cast<int>(state.activePlayers().size()), m_rounds),
                   col::gold, 1.6f);
        for (auto const& m : state.matches) {
            if (m.done()) m_doneShown.insert({m.round, m.slot});
        }
    }

    void Arena::onRound(State const& state, int round) {
        m_state = state;
        this->refreshMatches(state);
        float sc = this->bracketScale();
        for (auto const& m : state.matches) {
            if (m.round != round || !m.live()) continue;
            auto c = this->matchCentre(m.round, m.slot);
            fx::ring(this, {c.x, c.y - 4.f * sc}, col::gold, 1.f + .4f * sc, .5f, 35);
        }
        this->tickLive(state);
    }

    // ---------------------------------------------------------------- the march

    void Arena::march(Avatar* a, int round, int slot, int which, std::function<void()> done) {
        auto line = this->linePath(round, slot);
        if (line.empty()) {
            if (done) done();
            return;
        }
        float sc = this->bracketScale();
        int nextWhich = slot % 2 == 0 ? 1 : 2;
        auto to = this->seatPos(round + 1, slot / 2, nextWhich);
        MarchInfo mi;
        mi.avatar = WeakRef<Avatar>(a);
        mi.key = {round, slot};
        auto from = a->getPosition();
        mi.pts.push_back(from);
        mi.hops.push_back(0.f);
        // onto the path (unless the final cut already carried them past its start),
        // hopping over the one they just beat if they're in the way
        float out = line[1].x > line[0].x ? 1.f : -1.f;
        bool onPath = (from.x - line[0].x) * out >= 0.f && std::abs(from.y - line[0].y) < 2.f;
        if (!onPath) {
            mi.pts.push_back(line[0]);
            mi.hops.push_back(this->inTheWay(a, from.x, line[0].x, line[0].y) ? 22.f * sc + 8.f : 0.f);
        }
        mi.litStart = line[0];
        mi.litFrom = mi.pts.size();
        for (size_t i = 1; i < line.size(); ++i) {
            mi.pts.push_back(line[i]);
            mi.hops.push_back(0.f);
        }
        mi.litTo = mi.pts.size() - 1;
        // onto the next stage, over whoever already waits there if theirs is the far spot
        mi.pts.push_back(to);
        mi.hops.push_back(this->inTheWay(a, line.back().x, to.x, to.y) ? 20.f * sc + 6.f : 0.f);
        mi.done = std::move(done);
        m_marching.insert(mi.key);
        this->drawLines(m_state);
        float length = 0.f;
        for (size_t i = 1; i < mi.pts.size(); ++i) length += ccpDistance(mi.pts[i - 1], mi.pts[i]);
        this->trail(a, iconColor(a->player().icon.p), length / kMarchSpeed + .2f);
        fx::play("whoosh", .7f);
        m_marches.push_back(std::move(mi));
    }

    bool Arena::inTheWay(Avatar* self, float x0, float x1, float y) const {
        float lo = std::min(x0, x1), hi = std::max(x0, x1);
        float pad = 10.f * this->bracketScale();
        for (auto const& [acc, other] : m_avatars) {
            if (!other || other == self || !other->isVisible()) continue;
            auto p = other->getPosition();
            if (std::abs(p.y - y) < 6.f && p.x > lo - pad && p.x < hi + pad) return true;
        }
        return false;
    }

    void Arena::updateMarches(float dt) {
        m_litTrail->clear();
        std::vector<std::function<void()>> finished;
        for (auto it = m_marches.begin(); it != m_marches.end();) {
            auto& mi = *it;
            auto a = mi.avatar.lock();
            if (a) {
                float step = kMarchSpeed * dt;
                while (step > 0.f && mi.seg < mi.pts.size()) {
                    float len = ccpDistance(mi.pts[mi.seg - 1], mi.pts[mi.seg]);
                    float left = len - mi.segPos;
                    if (step >= left) {
                        step -= left;
                        ++mi.seg;
                        mi.segPos = 0.f;
                    } else {
                        mi.segPos += step;
                        step = 0.f;
                    }
                }
                CCPoint base = mi.pts.back();
                CCPoint pos = base;
                if (mi.seg < mi.pts.size()) {
                    auto from = mi.pts[mi.seg - 1], to = mi.pts[mi.seg];
                    float u = mi.segPos / std::max(.001f, ccpDistance(from, to));
                    base = from + (to - from) * u;
                    pos = base;
                    if (mi.hops[mi.seg] > 0.f) pos.y += mi.hops[mi.seg] * 4.f * u * (1.f - u);
                    int dx = to.x > from.x + .5f ? 1 : to.x < from.x - .5f ? -1 : 0;
                    a->setWalking(true, dx);
                    if (dx) a->setFacing(dx);
                }
                a->setPosition(pos);

                // light the path behind them
                if (mi.seg >= mi.litFrom) {
                    std::vector<CCPoint> lit {mi.litStart};
                    size_t last = std::min(mi.seg, mi.litTo + 1);
                    for (size_t i = mi.litFrom; i < last; ++i) lit.push_back(mi.pts[i]);
                    if (mi.seg <= mi.litTo) lit.push_back(base);
                    for (size_t i = 1; i < lit.size(); ++i) {
                        m_litTrail->drawSegment(lit[i - 1], lit[i], 3.6f, c4f(col::gold, .13f));
                        m_litTrail->drawSegment(lit[i - 1], lit[i], 1.5f, c4f(col::gold, .9f));
                    }
                }
            }
            if (!a || mi.seg >= mi.pts.size()) {
                if (a) a->setWalking(false);
                m_marching.erase(mi.key);
                if (mi.done) finished.push_back(std::move(mi.done));
                it = m_marches.erase(it);
            } else {
                ++it;
            }
        }
        if (!finished.empty()) {
            this->drawLines(m_state);
            for (auto& done : finished) done();
        }
    }

    // ---------------------------------------------------------------- results

    void Arena::onResult(State const& state, Event const& e) {
        m_state = state;
        if (!m_bracket) return;
        int r = jint(e.data, "r"), s = jint(e.data, "s");
        int w = jint(e.data, "w"), l = jint(e.data, "l");
        auto why = jstr(e.data, "why");
        auto key = std::make_pair(r, s);
        this->stopDuel(r, s);
        if (m_doneShown.contains(key) || why == "bye") {
            m_doneShown.insert(key);
            this->drawLines(state);
            this->refreshMatches(state);
            this->layoutBracket(state, true);
            return;
        }
        m_doneShown.insert(key);

        auto wi = m_avatars.find(w);
        if (wi == m_avatars.end() || !wi->second) {
            this->drawLines(state);
            this->layoutBracket(state, true);
            return;
        }
        Ref<Avatar> winner = wi->second;
        Ref<Avatar> loser = nullptr;
        if (auto li = m_avatars.find(l); li != m_avatars.end() && li->second) loser = li->second;

        auto* match = state.match(r, s);
        int winnerWhich = match && match->p1 == w ? 1 : 2;
        bool isFinal = r >= m_rounds;
        float sc = this->bracketScale();
        float reach = this->reach();
        auto seatW = this->seatPos(r, s, winnerWhich);
        auto seatL = this->seatPos(r, s, 3 - winnerWhich);
        int dir = seatW.x < seatL.x ? 1 : -1;  // the way the winner strikes

        // both stand still and square up for the whole performance
        for (Avatar* av : {winner.data(), loser.data()}) {
            if (!av) continue;
            av->busy = true;
            av->setIdle(false);
            av->stopActionByTag(kMoveTag);
            av->setWalking(false);
        }
        winner->setPosition(seatW);
        if (winner->hasSword()) winner->setFacing(dir);
        else winner->setSword(true, dir);
        if (loser) {
            loser->setPosition(seatL);
            if (loser->hasSword()) loser->setFacing(-dir);
            else loser->setSword(true, -dir);
        }
        float delay = std::min(m_resultDelay, 3.f);
        m_resultDelay += .7f;
        auto text = why == "complete" ? std::string("100%!") : reasonText(why);
        auto c = this->matchCentre(r, s);
        // the winner cuts straight through and stops on the far side
        auto past = seatL + ccp(dir * 22.f * sc, 0.f);

        this->runAction(CCSequence::create(
            CCDelayTime::create(delay),
            // the loser raises a guard; the winner draws all the way back
            CallFuncExt::create([winner, loser, dir, sc] {
                winner->handTo({-15.f, 17.f}, .26f);
                winner->swordTo(-75.f, .26f);
                winner->leanTo(-12.f, .26f);
                winner->runAction(CCEaseOut::create(CCMoveBy::create(.26f, {-dir * 6.f * sc, 0.f}), 2.f));
                if (loser) {
                    loser->swordTo(46.f, .2f);
                    loser->leanTo(-4.f, .2f);
                }
            }),
            CCDelayTime::create(.38f),
            // one dash, straight through them
            CallFuncExt::create([this, winner, loser, past, dir, sc, reach] {
                this->trail(winner.data(), col::white, .3f);
                winner->runAction(CCEaseOut::create(CCMoveTo::create(.09f, past), 3.f));
                winner->handTo({2.f, 0.f}, .07f);
                winner->swordTo(138.f, .08f);
                winner->leanTo(14.f, .07f);
                fx::play("whoosh", .9f, 1.25f);
                fx::play("slice", .9f);
                if (loser) {
                    auto body = loser->getPosition() + ccp(0.f, 15.f * sc);
                    slashFx(this, body - ccp(dir * reach * .7f, 4.f * sc), dir, 38.f, reach * 1.35f, col::white, 1.f, 41);
                    slashFx(this, body - ccp(dir * reach * .7f, 4.f * sc), dir, 38.f, reach * 1.6f, col::gold, .5f, 40);
                    // the cut itself: a bright line across them where the blade went through
                    auto* cut = fx("spark");
                    cut->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
                    float len = 64.f * std::max(.6f, sc);
                    float sx = len / cut->getContentSize().width;
                    cut->setScaleX(sx);
                    cut->setScaleY(.6f);
                    cut->setRotation(dir * 24.f);
                    cut->setFlipX(dir < 0);
                    cut->setPosition(body);
                    this->addChild(cut, 42);
                    cut->runAction(CCSequence::create(
                        CCDelayTime::create(.3f),
                        CCSpawn::create(CCFadeOut::create(.18f), CCScaleTo::create(.18f, sx * 1.4f, 0.f), nullptr),
                        CCRemoveSelf::create(), nullptr));
                }
            }),
            // the beat after the cut: nobody moves
            CCDelayTime::create(.36f),
            CallFuncExt::create([this, winner, loser, dir, sc, c, text] {
                float k = std::max(.6f, sc);
                if (loser) {
                    auto body = loser->getPosition() + ccp(0.f, 15.f * sc);
                    loser->knockOut();
                    loser->setZOrder(9);
                    // they crumple back the way the blade came from
                    loser->runAction(CCJumpBy::create(.36f, {-dir * kKnockBack * sc, 0.f}, 9.f * sc + 4.f, 1));
                    fx::sparks(this, body, 14, col::goldLight, 34.f * k, 42);
                    fx::flash(this, body, 38.f * k, col::white, .9f, 41);
                }
                fx::play("hit");
                FMODAudioEngine::sharedEngine()->playEffect("explode_11.ogg");
                fx::shake(this, 5.f, .3f);
                if (!text.empty()) fx::floatText(this, c + ccp(0.f, 30.f * sc + 22.f), text, col::gold, .45f);
                winner->leanTo(0.f, .3f);
            }),
            CCDelayTime::create(.32f),
            // turn round to the one they beat and hold the sword up
            CallFuncExt::create([this, winner, dir, sc] {
                winner->setFacing(-dir);
                winner->setHandOffset({0.f, 0.f});
                winner->swordRaise();
                winner->celebrate();
                fx::stars(this, winner->getPosition() + ccp(0.f, 26.f * sc), 6, col::gold, 26.f, 40);
                fx::play("win");
            }),
            CCDelayTime::create(1.1f),
            CallFuncExt::create([this, r, s, winner, loser, winnerWhich, isFinal] {
                if (loser) loser->busy = false;
                if (isFinal) {
                    winner->busy = false;
                    winner->setIdle(true);
                    this->drawLines(m_state);
                    return;
                }
                this->march(winner.data(), r, s, winnerWhich, [this, winner, s] {
                    int nextWhich = s % 2 == 0 ? 1 : 2;
                    winner->setFacing(nextWhich == 1 ? 1 : -1);
                    winner->swordRest(.2f);
                    winner->squash(.8f);
                    fx::dust(this, winner->getPosition(), .6f, winner->getZOrder() - 1);
                    fx::play("land", .8f);
                    winner->busy = false;
                    winner->setIdle(true);
                    this->refreshMatches(m_state);
                    this->layoutBracket(m_state, true);
                    this->tickLive(m_state);
                });
            }),
            nullptr));
    }

    void Arena::onChampion(State const& state, int account) {
        m_state = state;
        auto win = CCDirector::get()->getWinSize();
        auto* p = state.player(account);
        std::string name = p ? p->name : std::string("???");
        float delay = std::min(m_resultDelay, 3.f) + 1.7f;
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
                            a->setFacing(1);
                            a->swordRaise();
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
        float sc = this->bracketScale();
        float d = this->halfGap();
        for (auto const& key : m_matchKeys) {
            auto c = this->matchCentre(key.first, key.second);
            float fy = c.y - 15.f * sc;
            CCRect box {c.x - d - 15.f * sc - 6.f, fy - 8.f, 2.f * (d + 15.f * sc + 6.f), 30.f * sc + 20.f};
            if (box.containsPoint(p)) {
                round = key.first;
                slot = key.second;
                return true;
            }
        }
        return false;
    }
}
