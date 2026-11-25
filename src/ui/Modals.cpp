#include "Modals.hpp"
#include "Fx.hpp"
#include "../core/Compat.hpp"
#include "../core/Levels.hpp"
#include "../core/Session.hpp"

using namespace geode::prelude;

namespace nt::ui {
    // ---------------------------------------------------------------- Modal

    Modal::~Modal() {
        CCTouchDispatcher::get()->unregisterForcePrio(this);
    }

    void Modal::registerWithTouchDispatcher() {
        CCTouchDispatcher::get()->addTargetedDelegate(this, -500, true);
    }

    void Modal::keyBackClicked() {
        this->onClose(nullptr);
    }

    void Modal::onClose(CCObject*) {
        this->setKeypadEnabled(false);
        this->setTouchEnabled(false);
        this->removeFromParentAndCleanup(true);
    }

    bool Modal::initModal(CCSize size, std::string const& title) {
        m_size = size;
        compat::registerForcePrio(this);
        if (!this->initWithColor({0, 0, 0, 140})) return false;
        auto win = CCDirector::get()->getWinSize();

        m_mainLayer = CCLayer::create();
        this->addChild(m_mainLayer);
        m_mainLayer->ignoreAnchorPointForPosition(false);
        m_mainLayer->setContentSize(size);
        m_mainLayer->setPosition(win / 2.f);

        m_plate = Plate::create(size, col::navy, .97f, col::gold, 1.f, 12.f, 2.f);
        m_mainLayer->addChild(m_plate);
        auto* inner = Plate::create({size.width - 10.f, size.height - 10.f}, col::navy, 0.f, col::goldLight, .22f, 9.f, 1.f);
        inner->setPosition({5.f, 5.f});
        m_mainLayer->addChild(inner);

        m_buttonMenu = CCMenu::create();
        m_buttonMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_buttonMenu, 100);

        if (!title.empty()) {
            m_title = label(title, "goldFont.fnt", .8f, col::white);
            fit(m_title, size.width - 70.f, .8f);
            m_title->setPosition({size.width / 2.f, size.height - 20.f});
            m_mainLayer->addChild(m_title, 2);
        }

        auto* close = frameButton("GJ_closeBtn_001.png", .7f, [this](auto) { this->onClose(nullptr); });
        close->setPosition({6.f, size.height - 6.f});
        m_buttonMenu->addChild(close);

        this->setKeypadEnabled(true);
        this->setTouchEnabled(true);
        return true;
    }

    CCMenuItemSpriteExtra* Modal::addButton(std::string const& text, char const* bg, CCPoint pos, float scale,
                                            geode::Function<void(CCMenuItemSpriteExtra*)> cb, int width) {
        auto* btn = textButton(text, bg, scale, std::move(cb), width);
        btn->setPosition(pos);
        m_buttonMenu->addChild(btn);
        return btn;
    }

    CCTextInputNode* Modal::addInput(CCPoint pos, float width, std::string const& placeholder,
                                     std::string const& initial, std::string const& allowed, int maxLen) {
        auto* bg = Plate::create({width, 30.f}, col::navyDeep, .95f, col::blue, .6f, 6.f, 1.2f);
        bg->setAnchorPoint({.5f, .5f});
        bg->setPosition(pos);
        m_mainLayer->addChild(bg);
        auto* input = CCTextInputNode::create(width - 16.f, 28.f, placeholder.c_str(), "bigFont.fnt");
        input->setLabelPlaceholderColor({110, 125, 170});
        input->setMaxLabelScale(.6f);
        input->setMaxLabelLength(maxLen);
        if (!allowed.empty()) input->setAllowedChars(allowed);
        if (!initial.empty()) input->setString(initial);
        input->setPosition(pos);
        m_mainLayer->addChild(input, 5);
        return input;
    }

    // ---------------------------------------------------------------- InputModal

    InputModal* InputModal::create(std::string const& title, std::string const& desc, std::string const& placeholder,
                                   std::string const& initial, std::string const& allowed, int maxLen,
                                   std::string const& okText, std::function<void(std::string)> cb) {
        auto* ret = new InputModal();
        if (ret->init(title, desc, placeholder, initial, allowed, maxLen, okText, std::move(cb))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool InputModal::init(std::string const& title, std::string const& desc, std::string const& placeholder,
                          std::string const& initial, std::string const& allowed, int maxLen,
                          std::string const& okText, std::function<void(std::string)> cb) {
        if (!this->initModal({300.f, 170.f}, title)) return false;
        m_cb = std::move(cb);
        if (!desc.empty()) {
            auto* d = label(desc, "chatFont.fnt", .7f, col::dim);
            fit(d, 260.f, .7f);
            d->setPosition({150.f, 118.f});
            m_mainLayer->addChild(d);
        }
        m_input = this->addInput({150.f, 84.f}, 240.f, placeholder, initial, allowed, maxLen);
        this->addButton(okText, "GJ_button_01.png", {150.f, 34.f}, .8f, [this](auto) {
            auto text = compat::str(m_input->getString());
            auto cb = m_cb;
            this->onClose(nullptr);
            if (cb) cb(text);
        });
        return true;
    }

    // ---------------------------------------------------------------- ConfirmModal

    ConfirmModal* ConfirmModal::create(std::string const& title, std::string const& text, std::string const& yes,
                                       std::function<void()> cb, bool danger) {
        auto* ret = new ConfirmModal();
        if (ret->init(title, text, yes, std::move(cb), danger)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool ConfirmModal::init(std::string const& title, std::string const& text, std::string const& yes,
                            std::function<void()> cb, bool danger) {
        if (!this->initModal({300.f, 150.f}, title)) return false;
        m_cb = std::move(cb);
        auto* t = CCLabelBMFont::create(text.c_str(), "chatFont.fnt", 250.f, kCCTextAlignmentCenter);
        t->setScale(.75f);
        t->setColor(col::white);
        t->setPosition({150.f, 86.f});
        m_mainLayer->addChild(t);
        this->addButton("Cancel", "GJ_button_04.png", {95.f, 32.f}, .75f, [this](auto) { this->onClose(nullptr); });
        this->addButton(yes, danger ? "GJ_button_06.png" : "GJ_button_01.png", {205.f, 32.f}, .75f, [this](auto) {
            auto cb = m_cb;
            this->onClose(nullptr);
            if (cb) cb();
        });
        return true;
    }

    // ---------------------------------------------------------------- EmoteModal

    EmoteModal* EmoteModal::create(std::function<void(int)> cb) {
        auto* ret = new EmoteModal();
        if (ret->init(std::move(cb))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool EmoteModal::init(std::function<void(int)> cb) {
        if (!this->initModal({330.f, 230.f}, "Emotes")) return false;
        m_cb = std::move(cb);
        for (int i = 0; i < 15; ++i) {
            int id = i + 1;
            int col = i % 5, row = i / 5;
            auto* holder = CCNode::create();
            holder->setContentSize({50.f, 50.f});
            auto* glow = fx("glow");
            glow->setColor(emoteRarity(id));
            glow->setOpacity(110);
            glow->setScale(.7f);
            glow->setPosition({25.f, 25.f});
            holder->addChild(glow);
            auto* e = emote(id);
            e->setScale(.68f);
            e->setPosition({25.f, 25.f});
            holder->addChild(e);
            auto* btn = nodeButton(holder, [this, id](auto) {
                auto cb = m_cb;
                this->onClose(nullptr);
                if (cb) cb(id);
            });
            btn->setPosition({45.f + col * 60.f, 166.f - row * 62.f});
            m_buttonMenu->addChild(btn);
            auto* name = label(emoteName(id), "bigFont.fnt", .22f, col::dim);
            name->setPosition({45.f + col * 60.f, 140.f - row * 62.f});
            m_mainLayer->addChild(name, 3);
        }
        return true;
    }

    // ---------------------------------------------------------------- CreateModal

    CreateModal* CreateModal::create(std::function<void(CreateSettings)> cb) {
        auto* ret = new CreateModal();
        if (ret->init(std::move(cb))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    CCMenuItemSpriteExtra* CreateModal::chip(std::string const& text, CCPoint pos, float width, std::function<void()> cb) {
        auto* node = CCNode::create();
        node->setContentSize({width, 24.f});
        auto* plate = Plate::create({width, 24.f}, col::navyDeep, .95f, col::blue, .5f, 6.f, 1.2f);
        plate->setTag(1);
        node->addChild(plate);
        auto* l = label(text, "bigFont.fnt", .38f, col::white);
        fit(l, width - 8.f, .38f);
        l->setPosition({width / 2.f, 12.f});
        l->setTag(2);
        node->addChild(l);
        auto* btn = nodeButton(node, [this, cb](auto) {
            cb();
            this->refresh();
            fx::play("tick");
        });
        btn->setPosition(pos);
        m_buttonMenu->addChild(btn);
        return btn;
    }

    void CreateModal::refresh() {
        for (auto& [btn, selected] : m_chips) {
            auto* node = btn->getNormalImage();
            if (!node) continue;
            bool on = selected();
            if (auto* plate = static_cast<Plate*>(node->getChildByTag(1))) {
                plate->setFill(on ? col::plate : col::navyDeep, .95f);
                plate->setBorder(on ? col::gold : col::blue, on ? 1.f : .45f);
            }
            if (auto* l = static_cast<CCLabelBMFont*>(node->getChildByTag(2))) l->setColor(on ? col::gold : col::dim);
            if (auto* face = static_cast<CCSprite*>(node->getChildByTag(3))) {
                face->setColor(on ? ccColor3B {255, 255, 255} : ccColor3B {80, 86, 110});
                face->setOpacity(on ? 255 : 150);
            }
        }
        if (m_poolNote) {
            auto picked = poolOptions(m_set.pool);
            std::string text;
            if (picked.size() == difficulties().size()) text = "Any featured level";
            else if (picked.size() == 1) text = std::string("Featured ") + picked[0].name + " levels";
            else {
                text = "Featured: ";
                for (size_t i = 0; i < picked.size(); ++i) {
                    if (i) text += i + 1 == picked.size() ? " or " : ", ";
                    text += picked[i].name;
                }
            }
            m_poolNote->setString(text.c_str());
            fit(m_poolNote, 330.f, .55f);
        }
    }

    CCMenuItemSpriteExtra* CreateModal::faceChip(Difficulty const& d, CCPoint pos) {
        constexpr float w = 30.f, h = 34.f;
        auto* node = CCNode::create();
        node->setContentSize({w, h});
        auto* plate = Plate::create({w, h}, col::navyDeep, .95f, col::blue, .5f, 6.f, 1.2f);
        plate->setTag(1);
        node->addChild(plate);
        if (auto* face = difficulty(d.face)) {
            float size = std::max(face->getContentSize().width, face->getContentSize().height);
            face->setScale(size > 0.f ? 25.f / size : .5f);
            face->setPosition({w / 2.f, h / 2.f + (d.demon ? 1.f : 0.f)});
            face->setTag(3);
            node->addChild(face);
        }
        int bit = d.bit;
        auto* btn = nodeButton(node, [this, bit](CCMenuItemSpriteExtra* self) {
            int next = m_set.pool ^ (1 << bit);
            if (!(next & 1023)) {
                // at least one difficulty has to stay in
                fx::play("tick", .8f, .7f);
                fx::shake(self, 3.f, .2f);
                return;
            }
            m_set.pool = next;
            this->refresh();
            fx::play("tick");
        });
        btn->setPosition(pos);
        m_buttonMenu->addChild(btn);
        m_chips.push_back({btn, [this, bit] { return (m_set.pool & (1 << bit)) != 0; }});
        return btn;
    }

    bool CreateModal::init(std::function<void(CreateSettings)> cb) {
        if (!this->initModal({380.f, 316.f}, "Create Tournament")) return false;
        m_cb = std::move(cb);
        auto self = me();
        m_set.name = self.name.empty() ? std::string("My Tournament") : self.name + "'s Tournament";
        m_set.pub = Mod::get()->getSavedValue<bool>("create-public", true);
        m_set.max = Mod::get()->getSavedValue<int>("create-max", 16);
        m_set.time = Mod::get()->getSavedValue<int>("create-time", 180);
        m_set.play = Mod::get()->getSavedValue<bool>("create-play", true);
        m_set.pool = std::clamp(static_cast<int>(Mod::get()->getSavedValue<int64_t>("create-pool", 96)), 1, 1023);

        auto row = [&](float y, char const* text) {
            auto* l = label(text, "goldFont.fnt", .5f, col::white);
            l->setAnchorPoint({0.f, .5f});
            l->setPosition({20.f, y});
            m_mainLayer->addChild(l);
        };

        row(264.f, "Name");
        m_name = this->addInput({236.f, 264.f}, 250.f, "Tournament name", m_set.name,
                                " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'!?-&.#:", 32);

        row(228.f, "Who");
        m_chips.push_back({this->chip("Public", {170.f, 228.f}, 90.f, [this] { m_set.pub = true; }),
                           [this] { return m_set.pub; }});
        m_chips.push_back({this->chip("Private (code)", {290.f, 228.f}, 120.f, [this] { m_set.pub = false; }),
                           [this] { return !m_set.pub; }});

        row(194.f, "Players");
        int maxes[] = {4, 8, 16, 32};
        for (int i = 0; i < 4; ++i) {
            int v = maxes[i];
            m_chips.push_back({this->chip(std::to_string(v), {150.f + i * 56.f, 194.f}, 48.f, [this, v] { m_set.max = v; }),
                               [this, v] { return m_set.max == v; }});
        }

        row(160.f, "Time");
        int times[] = {60, 120, 180, 300, 600};
        for (int i = 0; i < 5; ++i) {
            int v = times[i];
            m_chips.push_back({this->chip(fmt::format("{}m", v / 60), {142.f + i * 50.f, 160.f}, 42.f,
                                          [this, v] { m_set.time = v; }),
                               [this, v] { return m_set.time == v; }});
        }

        // which difficulties the random levels are rolled from (tap to switch each one)
        row(126.f, "Levels");
        auto const& all = difficulties();
        for (size_t i = 0; i < all.size(); ++i) {
            float x = 36.f + static_cast<float>(i) * 34.f + (i >= 5 ? 6.f : 0.f);
            this->faceChip(all[i], {x, 96.f});
        }
        m_poolNote = label("", "chatFont.fnt", .55f, col::dim);
        m_poolNote->setPosition({190.f, 70.f});
        m_mainLayer->addChild(m_poolNote);

        m_chips.push_back({this->chip("I'm playing too", {100.f, 36.f}, 150.f, [this] { m_set.play = !m_set.play; }),
                           [this] { return m_set.play; }});

        this->addButton("Create", "GJ_button_01.png", {292.f, 36.f}, .85f, [this](auto) {
            auto name = compat::str(m_name->getString());
            if (!name.empty()) m_set.name = name;
            Mod::get()->setSavedValue<bool>("create-public", m_set.pub);
            Mod::get()->setSavedValue<int>("create-max", m_set.max);
            Mod::get()->setSavedValue<int>("create-time", m_set.time);
            Mod::get()->setSavedValue<bool>("create-play", m_set.play);
            Mod::get()->setSavedValue<int64_t>("create-pool", m_set.pool);
            auto cb = m_cb;
            auto set = m_set;
            this->onClose(nullptr);
            if (cb) cb(set);
        });
        this->refresh();
        return true;
    }

    // ---------------------------------------------------------------- PlayerModal

    PlayerModal* PlayerModal::create(State const& state, int account, bool canKick, std::function<void()> onKick) {
        auto* ret = new PlayerModal();
        if (ret->init(state, account, canKick, std::move(onKick))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool PlayerModal::init(State const& state, int account, bool canKick, std::function<void()> onKick) {
        auto* p = state.player(account);
        if (!p) return false;
        if (!this->initModal({260.f, 190.f}, p->name)) return false;
        m_onKick = std::move(onKick);

        auto* icon = SimplePlayer::create(1);
        icon->updatePlayerFrame(std::max(1, p->icon.cube), IconType::Cube);
        icon->setColor(iconColor(p->icon.p));
        icon->setSecondColor(iconColor(p->icon.s));
        if (p->icon.g >= 0) icon->setGlowOutline(iconColor(p->icon.g));
        else icon->disableGlowOutline();
        icon->setScale(1.5f);
        icon->setPosition({56.f, 112.f});
        m_mainLayer->addChild(icon);
        icon->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCMoveBy::create(1.1f, {0.f, 3.f})),
            CCEaseSineInOut::create(CCMoveBy::create(1.1f, {0.f, -3.f})), nullptr)));

        std::string status = p->status == "in" ? (state.t.lobby() ? "READY" : "STILL IN")
                            : p->status == "out" ? "KNOCKED OUT"
                            : "LEFT";
        if (state.t.finished() && state.t.champ == p->account) status = "CHAMPION";
        ccColor3B sc = status == "CHAMPION" ? col::gold : p->status == "in" ? col::green : col::red;
        auto* st = label(status, "goldFont.fnt", .55f, sc);
        st->setAnchorPoint({0.f, .5f});
        st->setPosition({104.f, 138.f});
        m_mainLayer->addChild(st);

        auto line = [&](float y, std::string const& k, std::string const& v) {
            auto* kl = label(k, "bigFont.fnt", .33f, col::dim);
            kl->setAnchorPoint({0.f, .5f});
            kl->setPosition({104.f, y});
            m_mainLayer->addChild(kl);
            auto* vl = label(v, "bigFont.fnt", .38f, col::white);
            vl->setAnchorPoint({1.f, .5f});
            vl->setPosition({240.f, y});
            m_mainLayer->addChild(vl);
        };
        line(112.f, "Wins", std::to_string(p->wins));
        line(92.f, "Best run", fmt::format("{}%", static_cast<int>(p->best)));
        line(72.f, "Seed", p->seed ? fmt::format("#{}", p->seed) : std::string("-"));
        if (auto* m = state.currentMatch(account); m && m->live()) {
            float b = m->p1 == account ? m->b1 : m->b2;
            line(52.f, "Now", fmt::format("{}% live", static_cast<int>(b)));
        }

        if (canKick && account != state.t.host) {
            this->addButton("Kick", "GJ_button_06.png", {200.f, 26.f}, .6f, [this](auto) {
                auto cb = m_onKick;
                this->onClose(nullptr);
                if (cb) cb();
            });
        }
        return true;
    }

    // ---------------------------------------------------------------- MatchModal

    MatchModal* MatchModal::create(State const& state, Match const& match, std::function<void(int)> onWinner) {
        auto* ret = new MatchModal();
        if (ret->init(state, match, std::move(onWinner))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool MatchModal::init(State const& state, Match const& match, std::function<void(int)> onWinner) {
        if (!this->initModal({300.f, 180.f}, "Decide this match")) return false;
        m_cb = std::move(onWinner);
        auto* hint = label("Host tool: pick who moves on.", "chatFont.fnt", .65f, col::dim);
        hint->setPosition({150.f, 136.f});
        m_mainLayer->addChild(hint);
        int ids[2] = {match.p1, match.p2};
        for (int i = 0; i < 2; ++i) {
            auto* p = state.player(ids[i]);
            if (!p) continue;
            float x = i == 0 ? 80.f : 220.f;
            auto* icon = SimplePlayer::create(1);
            icon->updatePlayerFrame(std::max(1, p->icon.cube), IconType::Cube);
            icon->setColor(iconColor(p->icon.p));
            icon->setSecondColor(iconColor(p->icon.s));
            icon->setPosition({x, 100.f});
            m_mainLayer->addChild(icon);
            auto* n = label(p->name, "bigFont.fnt", .35f, col::white);
            fit(n, 110.f, .35f);
            n->setPosition({x, 74.f});
            m_mainLayer->addChild(n);
            int id = p->account;
            this->addButton("Wins", "GJ_button_01.png", {x, 36.f}, .6f, [this, id](auto) {
                auto cb = m_cb;
                this->onClose(nullptr);
                if (cb) cb(id);
            });
        }
        auto* vs = label("VS", "goldFont.fnt", .8f, col::gold);
        vs->setPosition({150.f, 100.f});
        m_mainLayer->addChild(vs);
        return true;
    }
}
