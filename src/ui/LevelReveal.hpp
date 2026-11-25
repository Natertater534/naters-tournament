#pragma once

// The random-level roulette: difficulty faces flick past, slow down, land on the rolled
// level with a slam, then the level card shows its name before tucking into the corner.

#include <Geode/Geode.hpp>
#include <functional>
#include "../core/Model.hpp"

namespace nt::ui {
    using namespace cocos2d;

    class LevelReveal : public CCNode {
    public:
        static LevelReveal* create(int pool, std::string const& title);
        // keep spinning until land() or fail()
        void spin();
        void land(LevelRef const& level, std::function<void()> done = nullptr);
        void fail(std::string const& message);
        bool landing() const { return m_landing; }

    protected:
        bool init(int pool, std::string const& title);
        void update(float dt) override;
        void showFace(int face, bool final);

        std::vector<int> m_faces;
        CCNode* m_box = nullptr;
        CCNode* m_faceHolder = nullptr;
        CCLabelBMFont* m_title = nullptr;
        CCLabelBMFont* m_name = nullptr;
        CCLabelBMFont* m_by = nullptr;
        float m_interval = .07f;
        float m_timer = 0.f;
        size_t m_index = 0;
        bool m_landing = false;
        int m_swapsLeft = 0;
        LevelRef m_target;
        std::function<void()> m_done;
    };
}
