#pragma once

// Versus-style popups: navy plate, gold trim, built on FLAlertLayer so touch priority
// behaves the same on GD 2.2081 and 2.2091.

#include <Geode/Geode.hpp>
#include <functional>
#include "../core/Model.hpp"
#include "Theme.hpp"

namespace nt::ui {
    using namespace cocos2d;

    class Modal : public FLAlertLayer {
    public:
        virtual void onClose(CCObject*);

    protected:
        ~Modal() override;
        bool initModal(CCSize size, std::string const& title);
        void registerWithTouchDispatcher() override;
        void keyBackClicked() override;
        CCMenuItemSpriteExtra* addButton(std::string const& text, char const* bg, CCPoint pos, float scale,
                                         geode::Function<void(CCMenuItemSpriteExtra*)> cb, int width = 0);
        CCTextInputNode* addInput(CCPoint pos, float width, std::string const& placeholder, std::string const& initial,
                                  std::string const& allowed, int maxLen);

        CCSize m_size;
        Plate* m_plate = nullptr;
        CCLabelBMFont* m_title = nullptr;
    };

    class InputModal : public Modal {
    public:
        static InputModal* create(std::string const& title, std::string const& desc, std::string const& placeholder,
                                  std::string const& initial, std::string const& allowed, int maxLen,
                                  std::string const& okText, std::function<void(std::string)> cb);

    protected:
        bool init(std::string const& title, std::string const& desc, std::string const& placeholder,
                  std::string const& initial, std::string const& allowed, int maxLen, std::string const& okText,
                  std::function<void(std::string)> cb);
        CCTextInputNode* m_input = nullptr;
        std::function<void(std::string)> m_cb;
    };

    class ConfirmModal : public Modal {
    public:
        static ConfirmModal* create(std::string const& title, std::string const& text, std::string const& yes,
                                    std::function<void()> cb, bool danger = false);

    protected:
        bool init(std::string const& title, std::string const& text, std::string const& yes, std::function<void()> cb,
                  bool danger);
        std::function<void()> m_cb;
    };

    class EmoteModal : public Modal {
    public:
        static EmoteModal* create(std::function<void(int)> cb);

    protected:
        bool init(std::function<void(int)> cb);
        std::function<void(int)> m_cb;
    };

    struct CreateSettings {
        std::string name;
        bool pub = true;
        int max = 16;
        int time = 180;
        bool play = true;
    };

    class CreateModal : public Modal {
    public:
        static CreateModal* create(std::function<void(CreateSettings)> cb);

    protected:
        bool init(std::function<void(CreateSettings)> cb);
        void refresh();
        CCMenuItemSpriteExtra* chip(std::string const& text, CCPoint pos, float width, std::function<void()> cb);
        CreateSettings m_set;
        CCTextInputNode* m_name = nullptr;
        std::vector<std::pair<CCMenuItemSpriteExtra*, std::function<bool()>>> m_chips;
        std::function<void(CreateSettings)> m_cb;
    };

    class PlayerModal : public Modal {
    public:
        static PlayerModal* create(State const& state, int account, bool canKick, std::function<void()> onKick);

    protected:
        bool init(State const& state, int account, bool canKick, std::function<void()> onKick);
        std::function<void()> m_onKick;
    };

    class MatchModal : public Modal {
    public:
        // host tool: decide a match by hand
        static MatchModal* create(State const& state, Match const& match, std::function<void(int)> onWinner);

    protected:
        bool init(State const& state, Match const& match, std::function<void(int)> onWinner);
        std::function<void(int)> m_cb;
    };

    class LevelModal : public Modal, public LevelManagerDelegate {
    public:
        // host tool: look a level up by ID and pick it for the next round
        static LevelModal* create(std::function<void(LevelRef)> cb);
        void onClose(CCObject*) override;

    protected:
        ~LevelModal() override;
        bool init(std::function<void(LevelRef)> cb);
        void search();
        void showLevel(GJGameLevel* level);
        void loadLevelsFinished(cocos2d::CCArray* levels, char const* key) override;
        void loadLevelsFailed(char const* key) override;
        void loadLevelsFinished(cocos2d::CCArray* levels, char const* key, int type) override;
        void loadLevelsFailed(char const* key, int type) override;

        CCTextInputNode* m_input = nullptr;
        CCNode* m_preview = nullptr;
        CCMenuItemSpriteExtra* m_use = nullptr;
        LevelRef m_found;
        bool m_searching = false;
        std::function<void(LevelRef)> m_cb;
    };

    int faceFor(GJGameLevel* level);
}
