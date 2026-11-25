#include "HubLayer.hpp"
#include "Fx.hpp"
#include "Modals.hpp"
#include "TournamentLayer.hpp"
#include "../core/Compat.hpp"
#include "../core/Config.hpp"
#include "../core/Session.hpp"

#include <cctype>

using namespace geode::prelude;

namespace nt::ui {
    namespace {
        constexpr int kPerPage = 5;
        constexpr char const* kCodeChars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789abcdefghjklmnpqrstuvwxyz";
    }

    CCScene* HubLayer::scene() {
        auto* scene = CCScene::create();
        scene->addChild(HubLayer::create());
        return scene;
    }

    HubLayer* HubLayer::create() {
        auto* ret = new HubLayer();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool HubLayer::init() {
        if (!CCLayer::init()) return false;
        auto win = CCDirector::get()->getWinSize();

        auto* bg = Backdrop::create(col::navy, true, 30.f);
        this->addChild(bg, -10);

        // title
        auto* titleRoot = CCNode::create();
        titleRoot->setPosition({win.width / 2.f, win.height - 30.f});
        this->addChild(titleRoot, 5);
        auto* title = label("NATER'S TOURNAMENT", "goldFont.fnt", .95f, col::white);
        fit(title, win.width - 140.f, .95f);
        titleRoot->addChild(title);
        float tw = title->getScaledContentSize().width;
        for (int side : {-1, 1}) {
            auto* sw = CCSprite::createWithSpriteFrameName("GJ_versusBtn_001.png");
            if (!sw) sw = fx("swords");
            float h = std::max(sw->getContentSize().width, sw->getContentSize().height);
            sw->setScale(h > 0.f ? 30.f / h : 1.f);
            sw->setPosition({side * (tw / 2.f + 24.f), 1.f});
            sw->setFlipX(side > 0);
            titleRoot->addChild(sw);
            sw->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCRotateTo::create(1.2f, side * 8.f)),
                CCEaseSineInOut::create(CCRotateTo::create(1.2f, -side * 4.f)), nullptr)));
        }
        titleRoot->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCMoveBy::create(1.6f, {0.f, 3.f})),
            CCEaseSineInOut::create(CCMoveBy::create(1.6f, {0.f, -3.f})), nullptr)));
        auto* sub = label(fmt::format("Live 1v1 brackets  -  Geometry Dash {}", kGdVersion), "bigFont.fnt", .32f, col::goldLight);
        sub->setPosition({win.width / 2.f, win.height - 52.f});
        this->addChild(sub, 5);

        m_menu = CCMenu::create();
        m_menu->setPosition({0.f, 0.f});
        this->addChild(m_menu, 20);
        auto* back = frameButton("GJ_arrow_01_001.png", .7f, [this](auto) { this->keyBackClicked(); });
        back->setPosition({28.f, win.height - 26.f});
        m_menu->addChild(back);

        // the list
        float listW = std::min(330.f, win.width * .58f);
        float listX = 18.f;
        float listTop = win.height - 70.f;
        auto* listPlate = Plate::create({listW, listTop - 40.f}, col::navyDeep, .78f, col::gold, .55f, 12.f, 1.4f);
        listPlate->setPosition({listX, 40.f});
        this->addChild(listPlate, 1);
        auto* listTitle = label("PUBLIC TOURNAMENTS", "goldFont.fnt", .5f, col::white);
        listTitle->setAnchorPoint({0.f, .5f});
        listTitle->setPosition({listX + 12.f, listTop - 14.f});
        this->addChild(listTitle, 2);
        auto* refresh = frameButton("GJ_updateBtn_001.png", .45f, [this](auto) { this->refresh(); });
        refresh->setPosition({listX + listW - 18.f, listTop - 14.f});
        m_menu->addChild(refresh);

        m_listRoot = CCNode::create();
        m_listRoot->setPosition({listX, 40.f});
        this->addChild(m_listRoot, 3);
        m_listMenu = CCMenu::create();
        m_listMenu->setPosition({0.f, 0.f});
        m_listRoot->addChild(m_listMenu, 5);

        auto* prev = frameButton("GJ_arrow_03_001.png", .45f, [this](auto) {
            if (m_page > 0) {
                --m_page;
                this->rebuildList();
            }
        });
        prev->setPosition({listX + listW / 2.f - 50.f, 52.f});
        m_menu->addChild(prev);
        auto* next = frameButton("GJ_arrow_03_001.png", .45f, [this](auto) {
            if ((m_page + 1) * kPerPage < static_cast<int>(m_list.size())) {
                ++m_page;
                this->rebuildList();
            }
        });
        if (auto* img = static_cast<CCSprite*>(next->getNormalImage())) img->setFlipX(true);
        next->setPosition({listX + listW / 2.f + 50.f, 52.f});
        m_menu->addChild(next);
        m_pageLabel = label("", "bigFont.fnt", .3f, col::dim);
        m_pageLabel->setPosition({listX + listW / 2.f, 52.f});
        this->addChild(m_pageLabel, 4);

        // join with a code + host
        float px = listX + listW + 12.f;
        float pw = win.width - px - 18.f;
        auto* side = Plate::create({pw, listTop - 40.f}, col::navyDeep, .78f, col::gold, .55f, 12.f, 1.4f);
        side->setPosition({px, 40.f});
        this->addChild(side, 1);
        float cx = px + pw / 2.f;
        auto* codeTitle = label("JOIN WITH A CODE", "goldFont.fnt", .45f, col::white);
        fit(codeTitle, pw - 16.f, .45f);
        codeTitle->setPosition({cx, listTop - 14.f});
        this->addChild(codeTitle, 2);
        auto* codeBg = Plate::create({pw - 30.f, 30.f}, col::plate, .95f, col::blue, .6f, 6.f, 1.2f);
        codeBg->setAnchorPoint({.5f, .5f});
        codeBg->setPosition({cx, listTop - 46.f});
        this->addChild(codeBg, 2);
        m_codeInput = CCTextInputNode::create(pw - 44.f, 28.f, "CODE", "bigFont.fnt");
        m_codeInput->setLabelPlaceholderColor({110, 125, 170});
        m_codeInput->setMaxLabelScale(.7f);
        m_codeInput->setMaxLabelLength(5);
        m_codeInput->setAllowedChars(kCodeChars);
        m_codeInput->setPosition({cx, listTop - 46.f});
        this->addChild(m_codeInput, 3);
        auto* joinBtn = textButton("Join", "GJ_button_01.png", .7f, [this](auto) {
            this->joinCode(compat::str(m_codeInput->getString()));
        });
        joinBtn->setPosition({cx, listTop - 80.f});
        m_menu->addChild(joinBtn);

        auto* divider = CCLayerColor::create(ccc4(255, 206, 90, 70), pw - 30.f, 1.f);
        divider->setPosition({px + 15.f, listTop - 104.f});
        this->addChild(divider, 2);

        auto* hostTitle = label("HOST", "goldFont.fnt", .45f, col::white);
        hostTitle->setPosition({cx, listTop - 122.f});
        this->addChild(hostTitle, 2);
        auto* createBtn = textButton("Create", "GJ_button_02.png", .7f, [this](auto) {
            if (!me().loggedIn()) {
                FLAlertLayer::create("Log in first", "Log into your Geometry Dash account to host a tournament.", "OK")->show();
                return;
            }
            if (auto* m = CreateModal::create([this](CreateSettings set) { this->createTournament(set); })) m->show();
        });
        createBtn->setPosition({cx, listTop - 152.f});
        m_menu->addChild(createBtn);
        auto* hint = CCLabelBMFont::create("Viewers join with\nyour code", "chatFont.fnt", pw - 20.f, kCCTextAlignmentCenter);
        hint->setScale(.6f);
        hint->setColor(col::dim);
        hint->setPosition({cx, listTop - 186.f});
        this->addChild(hint, 2);

        auto last = lastCode();
        if (!last.empty()) {
            auto* rejoin = textButton("Last: " + last, "GJ_button_04.png", .5f, [this, last](auto) { this->open(last); });
            rejoin->setPosition({cx, 58.f});
            m_menu->addChild(rejoin);
        }

        if (!me().loggedIn()) {
            auto* warn = label("Log into a Geometry Dash account to join or host", "bigFont.fnt", .3f, col::red);
            warn->setPosition({win.width / 2.f, 18.f});
            this->addChild(warn, 5);
        }

        this->setKeypadEnabled(true);
        this->schedule(schedule_selector(HubLayer::tick), 0.f);
        this->rebuildList();
        this->refresh();
        return true;
    }

    void HubLayer::keyBackClicked() {
        if (m_leaving) return;
        m_leaving = true;
        CCDirector::get()->popSceneWithTransition(.45f, kPopTransitionFade);
    }

    void HubLayer::onEnterTransitionDidFinish() {
        CCLayer::onEnterTransitionDidFinish();
        m_leaving = false;
        this->refresh();
    }

    void HubLayer::tick(float dt) {
        m_timer += dt;
        if (m_timer > 6.f) this->refresh();
    }

    void HubLayer::refresh() {
        m_timer = 0.f;
        if (m_loading) return;
        m_loading = true;
        api::call(m_listReq, "nt_list", matjson::makeObject({{"gd", std::string(kGdVersion)}}), [this](api::Reply reply) {
            m_loading = false;
            if (reply.isErr()) {
                if (!m_loadedOnce) {
                    m_list.clear();
                    this->rebuildList();
                    Notification::create(api::describe(reply.unwrapErr()), NotificationIcon::Error, 2.f)->show();
                }
                return;
            }
            bool first = !m_loadedOnce;
            m_loadedOnce = true;
            auto list = parseList(reply.unwrap());
            bool changed = list.size() != m_list.size();
            for (size_t i = 0; !changed && i < list.size(); ++i) {
                changed = list[i].code != m_list[i].code || list[i].n != m_list[i].n || list[i].state != m_list[i].state;
            }
            m_list = std::move(list);
            if (m_page * kPerPage >= static_cast<int>(m_list.size())) m_page = 0;
            if (changed || first) this->rebuildList();
        });
    }

    void HubLayer::rebuildList() {
        auto win = CCDirector::get()->getWinSize();
        float listW = std::min(330.f, win.width * .58f);
        float top = win.height - 70.f - 40.f - 36.f;
        m_listRoot->removeAllChildren();
        m_listMenu = CCMenu::create();
        m_listMenu->setPosition({0.f, 0.f});
        m_listRoot->addChild(m_listMenu, 5);

        int pages = std::max(1, static_cast<int>((m_list.size() + kPerPage - 1) / kPerPage));
        m_pageLabel->setString(m_list.empty() ? "" : fmt::format("{}/{}", m_page + 1, pages).c_str());

        if (!m_loadedOnce) {
            auto* l = label("Loading...", "goldFont.fnt", .5f, col::white);
            l->setPosition({listW / 2.f, top / 2.f + 20.f});
            m_listRoot->addChild(l);
            return;
        }
        if (m_list.empty()) {
            auto* e = emote(12);
            e->setScale(.7f);
            e->setPosition({listW / 2.f, top / 2.f + 46.f});
            m_listRoot->addChild(e);
            e->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCRotateTo::create(.6f, -8.f)), CCEaseSineInOut::create(CCRotateTo::create(.6f, 8.f)),
                nullptr)));
            auto* l = CCLabelBMFont::create("No public tournaments right now.\nCreate one!", "bigFont.fnt", 300.f,
                                            kCCTextAlignmentCenter);
            l->setScale(.38f);
            l->setPosition({listW / 2.f, top / 2.f});
            m_listRoot->addChild(l);
            return;
        }

        for (int i = 0; i < kPerPage; ++i) {
            int idx = m_page * kPerPage + i;
            if (idx >= static_cast<int>(m_list.size())) break;
            auto const& e = m_list[idx];
            float y = top - i * 40.f;
            auto* card = CCNode::create();
            card->setPosition({10.f, y - 18.f});
            m_listRoot->addChild(card);
            auto* plate = Plate::create({listW - 20.f, 36.f}, col::plate, .92f, e.state == "running" ? col::red : col::blue,
                                        .6f, 8.f, 1.2f);
            card->addChild(plate);

            ccColor3B dotColor = e.state == "lobby" ? col::green : e.state == "running" ? col::red : col::dim;
            auto* dot = fx("dot");
            dot->setColor(dotColor);
            dot->setScale(1.1f);
            dot->setPosition({14.f, 18.f});
            card->addChild(dot);
            if (e.state == "running") {
                dot->runAction(CCRepeatForever::create(CCSequence::create(CCFadeTo::create(.5f, 70), CCFadeTo::create(.5f, 255), nullptr)));
            }

            auto* name = label(e.name, "bigFont.fnt", .4f, col::white);
            fit(name, listW - 140.f, .4f);
            name->setAnchorPoint({0.f, .5f});
            name->setPosition({26.f, 24.f});
            card->addChild(name);
            std::string stateText = e.state == "lobby" ? "OPEN" : e.state == "running" ? roundName(std::max(1, e.round), e.rounds)
                                                                                       : "FINISHED";
            auto* info = label(fmt::format("by {}  -  {}/{}  -  {}", e.host, e.n, e.max, stateText), "goldFont.fnt", .34f,
                               col::goldLight);
            fit(info, listW - 140.f, .34f);
            info->setAnchorPoint({0.f, .5f});
            info->setPosition({26.f, 10.f});
            card->addChild(info);

            auto code = e.code;
            bool open = e.state == "lobby" && e.n < e.max;
            auto* btn = textButton(open ? "Join" : "Watch", open ? "GJ_button_01.png" : "GJ_button_04.png", .5f,
                                   [this, code, open](auto) {
                                       if (open) this->joinCode(code);
                                       else this->open(code);
                                   });
            btn->setPosition({10.f + listW - 20.f - 38.f, y});
            m_listMenu->addChild(btn);

            card->setScale(.9f);
            card->setPositionX(-30.f);
            card->runAction(CCSequence::create(CCDelayTime::create(.04f * i),
                CCSpawn::create(CCEaseBackOut::create(CCMoveTo::create(.3f, {10.f, y - 18.f})),
                                CCEaseBackOut::create(CCScaleTo::create(.3f, 1.f)), nullptr), nullptr));
        }
    }

    void HubLayer::open(std::string const& code) {
        if (m_leaving) return;
        m_leaving = true;
        fx::play("pop");
        CCDirector::get()->pushScene(CCTransitionFade::create(.4f, TournamentLayer::scene(code)));
    }

    void HubLayer::joinCode(std::string code) {
        for (auto& c : code) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (code.size() != 5) {
            Notification::create("Codes are 5 letters long", NotificationIcon::Warning, 1.5f)->show();
            return;
        }
        if (!me().loggedIn()) {
            this->open(code);
            return;
        }
        api::call(m_actionReq, "nt_join", withAuth(matjson::makeObject({{"code", code}})), [this, code](api::Reply reply) {
            if (reply.isErr()) {
                auto err = reply.unwrapErr();
                // still let them watch a tournament that already started
                if (err == "started" || err == "full" || err == "kicked") {
                    Notification::create(api::describe(err), NotificationIcon::Info, 2.f)->show();
                    this->open(code);
                    return;
                }
                FLAlertLayer::create("Can't join", api::describe(err), "OK")->show();
                return;
            }
            this->open(code);
        });
    }

    void HubLayer::createTournament(CreateSettings const& set) {
        auto args = matjson::makeObject({
            {"name", set.name},
            {"public", set.pub},
            {"max", set.max},
            {"time", set.time},
            {"play", set.play},
        });
        api::call(m_actionReq, "nt_create", withAuth(args), [this](api::Reply reply) {
            if (reply.isErr()) {
                FLAlertLayer::create("Can't create", api::describe(reply.unwrapErr()), "OK")->show();
                return;
            }
            auto code = jstr(reply.unwrap(), "code");
            if (code.empty()) return;
            this->open(code);
        });
    }
}
