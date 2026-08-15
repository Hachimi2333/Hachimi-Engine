#pragma once

#include "Core/Layer.h"
#include "Core/Memory.h"

#include <vector>

namespace HachimiEngine
{
    // Owns layers and overlays. Layers are updated first; overlays (such as ImGui) render last.
    class LayerStack
    {
    public:
        LayerStack() = default;
        ~LayerStack();

        void PushLayer(const Ref<Layer>& layer);
        void PushOverlay(const Ref<Layer>& overlay);
        void PopLayer(const Ref<Layer>& layer);
        void PopOverlay(const Ref<Layer>& overlay);
        void PopLayer(Layer* layer);
        void PopOverlay(Layer* overlay);

        const std::vector<Ref<Layer>>& GetLayers() const { return m_Layers; }
        const std::vector<Ref<Layer>>& GetOverlays() const { return m_Overlays; }

        void Update(Timestep timestep) const;
        void RenderImGui() const;
        void DispatchEvent(Event& event) const;

    private:
        std::vector<Ref<Layer>> m_Layers;
        std::vector<Ref<Layer>> m_Overlays;
    };
}
