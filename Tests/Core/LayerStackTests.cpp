// Session teardown must run while application services still exist; layers need no window here.
#include <doctest/doctest.h>

#include "Core/LayerStack.h"

#include <vector>

using namespace HachimiEngine;

TEST_SUITE("Core")
{
    TEST_CASE("clearing layers detaches once in reverse order before releasing ownership")
    {
        std::vector<int> log;
        class Probe final : public Layer
        {
        public:
            Probe(std::vector<int>& log, int id) : m_Log(log), m_Id(id) {}
            void OnDetach() override { m_Log.push_back(m_Id); }
        private:
            std::vector<int>& m_Log;
            int m_Id;
        };
        {
            LayerStack stack;
            stack.PushLayer(CreateRef<Probe>(log, 1));
            stack.PushLayer(CreateRef<Probe>(log, 2));
            stack.PushOverlay(CreateRef<Probe>(log, 3));
            stack.Clear();
            stack.Clear();
            CHECK(stack.GetLayers().empty());
            CHECK(stack.GetOverlays().empty());
        }
        CHECK(log == std::vector<int> { 3, 2, 1 });
    }
}
