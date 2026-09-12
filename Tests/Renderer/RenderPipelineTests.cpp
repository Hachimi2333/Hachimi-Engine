// RenderPipeline: pass order, which is the whole contract of the class.
//
// The passes themselves need an OpenGL context, but the pipeline only sequences them, so
// the ordering is verified with recording passes. RendererContext is left uninitialized:
// no GL context is created here, and every accessor on it is unreachable from a pass that
// does not draw.

#include <doctest/doctest.h>

#include "Core/Memory.h"
#include "Renderer/RenderPipeline.h"
#include "Renderer/RendererContext.h"
#include "Renderer/RenderView.h"

#include <ostream>
#include <string>
#include <string_view>
#include <utility>

using namespace HachimiEngine;

namespace
{
    // Records the order in which the pipeline reached it.
    class RecordingPass final : public RenderPass
    {
    public:
        RecordingPass(std::string& log, std::string name)
            : m_Log(log)
            , m_Name(std::move(name))
        {
        }

        std::string_view GetName() const override { return m_Name; }

        void Execute(RenderPassContext& context) override
        {
            m_Log += m_Name;
            ++m_ExecutionCount;
            // Reading the shared frame state proves the context reaches every pass.
            m_LastShadowEnabled = context.DirectionalShadowEnabled;
        }

        int GetExecutionCount() const { return m_ExecutionCount; }
        bool GetLastShadowEnabled() const { return m_LastShadowEnabled; }

    private:
        std::string& m_Log;
        std::string m_Name;
        int m_ExecutionCount = 0;
        bool m_LastShadowEnabled = false;
    };
}

TEST_SUITE("Renderer")
{
    TEST_CASE("passes run in the order they were added")
    {
        RendererContext renderers;
        const RenderView view;
        RenderPassContext context { renderers, view };

        std::string log;
        RenderPipeline pipeline;
        pipeline.AddPass<RecordingPass>(log, "shadow");
        pipeline.AddPass<RecordingPass>(log, "sky");
        pipeline.AddPass<RecordingPass>(log, "opaque");

        pipeline.Execute(context);

        CHECK(log == "shadowskyopaque");
        CHECK(pipeline.GetPassCount() == 3);
    }

    TEST_CASE("a pass keeps its identity and runs exactly once per frame")
    {
        RendererContext renderers;
        const RenderView view;
        RenderPassContext context { renderers, view };

        std::string log;
        RenderPipeline pipeline;
        RecordingPass& pass = pipeline.AddPass<RecordingPass>(log, "only");

        pipeline.Execute(context);
        CHECK(pass.GetName() == "only");
        CHECK(pass.GetExecutionCount() == 1);
    }

    TEST_CASE("an empty pipeline is a no-op")
    {
        RendererContext renderers;
        const RenderView view;
        RenderPassContext context { renderers, view };

        const RenderPipeline pipeline;
        pipeline.Execute(context);

        CHECK(pipeline.GetPassCount() == 0);
    }

    TEST_CASE("the frame state written by a pass reaches the passes after it")
    {
        RendererContext renderers;
        const RenderView view;
        RenderPassContext context { renderers, view };

        std::string log;
        RenderPipeline pipeline;

        // Stands in for the shadow pass, which is the pass that publishes the handoff.
        class PublisherPass final : public RenderPass
        {
        public:
            std::string_view GetName() const override { return "publisher"; }
            void Execute(RenderPassContext& passContext) override { passContext.DirectionalShadowEnabled = true; }
        };

        pipeline.AddPass<PublisherPass>();
        RecordingPass& consumer = pipeline.AddPass<RecordingPass>(log, "consumer");

        pipeline.Execute(context);

        CHECK(consumer.GetLastShadowEnabled());
    }

    TEST_CASE("clearing the pipeline removes every pass")
    {
        RendererContext renderers;
        const RenderView view;
        RenderPassContext context { renderers, view };

        std::string log;
        RenderPipeline pipeline;
        pipeline.AddPass<RecordingPass>(log, "a");
        pipeline.Clear();
        pipeline.AddPass<RecordingPass>(log, "b");

        pipeline.Execute(context);

        CHECK(pipeline.GetPassCount() == 1);
        CHECK(log == "b");
    }
}
