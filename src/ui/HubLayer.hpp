#pragma once

// The front door: public tournaments to browse, a box for private codes, and hosting.

#include <Geode/Geode.hpp>
#include "../core/Api.hpp"
#include "../core/Model.hpp"
#include "Modals.hpp"
#include "Theme.hpp"

namespace nt::ui {
    using namespace cocos2d;

    class HubLayer : public CCLayer {
    public:
        static CCScene* scene();
        static HubLayer* create();

    protected:
        bool init() override;
        void keyBackClicked() override;
        void onEnterTransitionDidFinish() override;
        void refresh();
        void tick(float dt);
        void rebuildList();
        void open(std::string const& code);
        void joinCode(std::string code);
        void createTournament(CreateSettings const& set);

        std::vector<ListEntry> m_list;
        int m_page = 0;
        bool m_loading = false;
        bool m_loadedOnce = false;
        float m_timer = 0.f;
        api::Holder m_listReq;
        api::Holder m_actionReq;
        CCNode* m_listRoot = nullptr;
        CCMenu* m_listMenu = nullptr;
        CCMenu* m_menu = nullptr;
        CCTextInputNode* m_codeInput = nullptr;
        CCLabelBMFont* m_pageLabel = nullptr;
        bool m_leaving = false;
    };
}
