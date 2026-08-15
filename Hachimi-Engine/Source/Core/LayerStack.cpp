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
        for (const auto& layer : m_Layers)
        {
            layer->OnUpdate(timestep);
        }
        for (const auto& overlay : m_Overlays)
        {
            overlay->OnUpdate(timestep);
        }
    }

    void LayerStack::RenderImGui() const
    {
        for (const auto& layer : m_Layers)
        {
            layer->OnImGuiRender();
        }
        for (const auto& overlay : m_Overlays)
        {
            overlay->OnImGuiRender();
        }
    }

    void LayerStack::DispatchEvent(Event& event) const
    {
        // Events travel from the topmost overlay/layer down until handled.
        for (auto it = m_Overlays.rbegin(); it != m_Overlays.rend(); ++it)
        {
            if (event.Handled)
            {
                break;
            }
            (*it)->OnEvent(event);
        }

        for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it)
        {
            if (event.Handled)
            {
                break;
            }
            (*it)->OnEvent(event);
        }
    }
}
