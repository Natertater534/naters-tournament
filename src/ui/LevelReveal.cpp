#include "LevelReveal.hpp"
#include "Fx.hpp"
#include "Theme.hpp"
#include "../core/Levels.hpp"

using namespace geode::prelude;

namespace nt::ui {
    LevelReveal* LevelReveal::create(int pool, std::string const& title) {
        auto* ret = new LevelReveal();
        if (ret->init(pool, title)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool LevelReveal::init(int pool, std::string const& title) {
        if (!CCNode::init()) return false;
        for (auto const& d : poolOptions(pool)) m_faces.push_back(d.face);
        if (m_faces.empty()) m_faces = {7, 8, 6};
        auto win = CCDirector::get()->getWinSize();
        this->setPosition(win / 2.f + ccp(0.f, 6.f));

        m_box = CCNode::create();
        this->addChild(m_box);
        auto* rays = fx("rays");
        rays->setColor(col::gold);
        rays->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        rays->setOpacity(70);
        rays->setScale(1.3f);
        rays->setPosition({0.f, 8.f});
        rays->runAction(CCRepeatForever::create(CCRotateBy::create(9.f, 360.f)));
        m_box->addChild(rays, -1);
        auto* plate = Plate::create({236.f, 152.f}, col::navyDeep, .95f, col::gold, 1.f, 14.f, 2.f);
        plate->setAnchorPoint({.5f, .5f});
        m_box->addChild(plate);
        m_title = label(title, "goldFont.fnt", .55f, col::white);
        fit(m_title, 210.f, .55f);
        m_title->setPosition({0.f, 58.f});
        m_box->addChild(m_title, 2);
        m_faceHolder = CCNode::create();
        m_faceHolder->setPosition({0.f, 10.f});
        m_box->addChild(m_faceHolder, 2);
        m_name = label("", "bigFont.fnt", .5f, col::white);
        m_name->setPosition({0.f, -40.f});
        m_box->addChild(m_name, 2);
        m_by = label("", "goldFont.fnt", .4f, col::gold);
        m_by->setPosition({0.f, -58.f});
        m_box->addChild(m_by, 2);

        fx::popIn(m_box, 1.f, 0.f, .5f);
        this->showFace(m_faces[0], false);
        this->scheduleUpdate();
        return true;
    }

    void LevelReveal::spin() {
        m_landing = false;
        m_interval = .07f;
    }

    void LevelReveal::land(LevelRef const& level, std::function<void()> done) {
        if (m_landing && m_swapsLeft == 0 && m_target.id == level.id) return;
        m_target = level;
        m_done = std::move(done);
        m_landing = true;
        m_swapsLeft = 9;
        m_interval = .07f;
    }

    void LevelReveal::fail(std::string const& message) {
        m_landing = true;
        m_swapsLeft = -1;
        m_name->setString(message.c_str());
        m_name->setColor(col::red);
        fit(m_name, 210.f, .4f);
        fx::play("hit");
        this->runAction(CCSequence::create(CCDelayTime::create(2.2f), CCEaseBackIn::create(CCScaleTo::create(.25f, 0.f)),
                                           CCRemoveSelf::create(), nullptr));
    }

    void LevelReveal::showFace(int face, bool final) {
        m_faceHolder->removeAllChildren();
        auto* spr = difficulty(face);
        if (!spr) return;
        float scale = final ? 1.7f : 1.35f;
        spr->setScale(scale * 1.3f);
        spr->runAction(CCEaseOut::create(CCScaleTo::create(.12f, scale), 2.f));
        m_faceHolder->addChild(spr);
        if (!final) {
            fx::play("tick");
            return;
        }
        // the slam
        fx::play("drum");
        fx::flash(m_box, {0.f, 10.f}, 70.f, col::gold, .9f, 1);
        fx::ring(m_box, {0.f, 10.f}, col::gold, 2.f, .5f, 1);
        fx::stars(m_box, {0.f, 10.f}, 8, col::goldLight, 40.f, 3);
        fx::shake(this, 5.f, .3f);
        auto name = m_target.name.empty() ? "Level " + std::to_string(m_target.id) : m_target.name;
        m_name->setString(name.c_str());
        m_name->setColor(col::white);
        fit(m_name, 214.f, .55f);
        fx::popIn(m_name, m_name->getScale(), .05f, .45f);
        auto by = (m_target.author.empty() ? std::string() : "by " + m_target.author + "   ") + "#" + std::to_string(m_target.id);
        m_by->setString(by.c_str());
        fit(m_by, 214.f, .4f);
        fx::popIn(m_by, m_by->getScale(), .15f, .45f);
        auto win = CCDirector::get()->getWinSize();
        auto done = m_done;
        this->runAction(CCSequence::create(
            CCDelayTime::create(2.6f),
            CCSpawn::create(CCEaseIn::create(CCMoveTo::create(.35f, {win.width - 94.f, win.height - 70.f}), 2.f),
                            CCEaseIn::create(CCScaleTo::create(.35f, .2f), 2.f), nullptr),
            CallFuncExt::create([done] {
                if (done) done();
            }),
            CCRemoveSelf::create(), nullptr));
    }

    void LevelReveal::update(float dt) {
        if (m_swapsLeft < 0) return;
        m_timer += dt;
        if (m_timer < m_interval) return;
        m_timer = 0.f;
        if (!m_landing) {
            m_index = (m_index + 1) % m_faces.size();
            this->showFace(m_faces[m_index], false);
            return;
        }
        if (m_swapsLeft > 1) {
            --m_swapsLeft;
            m_interval *= 1.27f;
            m_index = (m_index + 1) % m_faces.size();
            this->showFace(m_faces[m_index], false);
        } else if (m_swapsLeft == 1) {
            m_swapsLeft = -1;
            this->showFace(m_target.face, true);
        }
    }
}
