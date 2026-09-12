#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/RenderPass.h"

#include <utility>
#include <vector>

namespace HachimiEngine
{
    // Ordered list of render passes that makes up a frame.
    //
    // The order is the container order, and it is set up in one place by SceneRenderer, so
    // the sequence a frame runs in is readable at a glance instead of being implied by the
    // call order inside a draw function.
    class RenderPipeline
    {
    public:
        // Constructs a pass in place and returns it, so callers can keep a reference for
        // configuration (toggling shadows, changing a radius) without a lookup.
        template<typename T, typename... Args>
        T& AddPass(Args&&... args)
        {
            Scope<T> pass = CreateScope<T>(std::forward<Args>(args)...);
            T& reference = *pass;
            m_Passes.push_back(std::move(pass));
            return reference;
        }

        // Runs every pass in order. A null pass is skipped rather than fatal.
        void Execute(RenderPassContext& context) const;

        void Clear() { m_Passes.clear(); }

        size_t GetPassCount() const { return m_Passes.size(); }
        const std::vector<Scope<RenderPass>>& GetPasses() const { return m_Passes; }

    private:
        std::vector<Scope<RenderPass>> m_Passes;
    };
}
