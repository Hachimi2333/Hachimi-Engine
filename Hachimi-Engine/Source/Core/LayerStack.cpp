#include "Core/LayerStack.h"

#include <algorithm>

namespace HachimiEngine
{
    LayerStack::~LayerStack()
    {
        for (auto it = m_Overlays.rbegin(); it != m_Overlays.rend(); ++it)
        {
            (*it)->OnDetach();
        }
        for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it)
        {
            (*it)->OnDetach();
        }
    }

    void LayerStack::PushLayer(const Ref<Layer>& layer)
    {
        m_Layers.push_back(layer);
        layer->OnAttach();
    }

    void LayerStack::PushOverlay(const Ref<Layer>& overlay)
    {
        m_Overlays.push_back(overlay);
        overlay->OnAttach();
    }

    void LayerStack::PopLayer(const Ref<Layer>& layer)
    {
        const auto it = std::find(m_Layers.begin(), m_Layers.end(), layer);
        if (it != m_Layers.end())
        {
            (*it)->OnDetach();
            m_Layers.erase(it);
        }
    }

    void LayerStack::PopOverlay(const Ref<Layer>& overlay)
    {
        const auto it = std::find(m_Overlays.begin(), m_Overlays.end(), overlay);
        if (it != m_Overlays.end())
        {
            (*it)->OnDetach();
            m_Overlays.erase(it);
        }
    }

    void LayerStack::PopLayer(Layer* layer)
    {
        const auto it = std::find_if(m_Layers.begin(), m_Layers.end(), [layer](const Ref<Layer>& candidate)
        {
            return candidate.get() == layer;
        });

        if (it != m_Layers.end())
        {
            (*it)->OnDetach();
            m_Layers.erase(it);
        }
    }

    void LayerStack::PopOverlay(Layer* overlay)
    {
        const auto it = std::find_if(m_Overlays.begin(), m_Overlays.end(), [overlay](const Ref<Layer>& candidate)
        {
            return candidate.get() == overlay;
        });

        if (it != m_Overlays.end())
        {
            (*it)->OnDetach();
            m_Overlays.erase(it);
        }
    }

    bool LayerStack::ContainsLayer(Layer* layer) const
    {
        return std::any_of(m_Layers.begin(), m_Layers.end(), [layer](const Ref<Layer>& candidate)
        {
            return candidate.get() == layer;
        });
    }

    bool LayerStack::ContainsOverlay(Layer* overlay) const
    {
        return std::any_of(m_Overlays.begin(), m_Overlays.end(), [overlay](const Ref<Layer>& candidate)
        {
            return candidate.get() == overlay;
        });
    }

    void LayerStack::Update(Timestep timestep) const
    {
        // Iterate over a snapshot so layers can safely push/pop mid-frame.
        const std::vector<Ref<Layer>> layers = m_Layers;
        for (const auto& layer : layers)
        {
            layer->OnUpdate(timestep);
        }

        const std::vector<Ref<Layer>> overlays = m_Overlays;
        for (const auto& overlay : overlays)
        {
            overlay->OnUpdate(timestep);
        }
    }

    void LayerStack::RenderImGui() const
    {
        // Iterate over a snapshot so layers can safely push/pop mid-frame.
        const std::vector<Ref<Layer>> layers = m_Layers;
        for (const auto& layer : layers)
        {
            layer->OnImGuiRender();
        }

        const std::vector<Ref<Layer>> overlays = m_Overlays;
        for (const auto& overlay : overlays)
        {
            overlay->OnImGuiRender();
        }
    }

    void LayerStack::DispatchEvent(Event& event) const
    {
        // Events travel from the topmost overlay/layer down until handled.
        // Use snapshots so layers can safely push/pop while dispatching.
        const std::vector<Ref<Layer>> overlays = m_Overlays;
        for (auto it = overlays.rbegin(); it != overlays.rend(); ++it)
        {
            if (event.Handled)
            {
                break;
            }
            (*it)->OnEvent(event);
        }

        const std::vector<Ref<Layer>> layers = m_Layers;
        for (auto it = layers.rbegin(); it != layers.rend(); ++it)
        {
            if (event.Handled)
            {
                break;
            }
            (*it)->OnEvent(event);
        }
    }
}
