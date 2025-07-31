#pragma once


#include <math/Box.h>
#include <math/Color.h>
#include <math/Vector.h>

#include <scenic/Pose.h>

#include <memory>
#include <unordered_map>
#include <vector>


namespace ad::debug {


struct LineVertex
{
    math::Position<3, float> mPosition;
    math::hdr::Rgba_f mColor = math::hdr::gMagenta<float>;
};

#define SIMPLER_DBGDRAW
#if defined(SIMPLER_DBGDRAW)

struct DebugDrawer
{
    struct Commands
    {
        std::vector<LineVertex> mLineVertices;
    };

    using DrawList = Commands;

    void startFrame()
    {
        mFrameCommands = Commands{};
    }

    void endFrame()
    {
    }

    void addLine(LineVertex aP1, LineVertex aP2)
    {
        commands().mLineVertices.push_back(std::move(aP1));
        commands().mLineVertices.push_back(std::move(aP2));
    }

    void addBox(const math::Box<float> & aBox,
                scenic::Pose aPose,
                math::hdr::Rgba_f aColor);

    Commands & commands()
    {
        return mFrameCommands;
    }

    Commands mFrameCommands;
};

#else

class DebugDrawer
{
    friend class Registry;

    struct PrivateToken
    {};

public:
    enum class Level
    {
        trace,
        debug,
        info,
        warn,
        error,
        off,
    };

    struct Commands
    {
        std::vector<LineVertex> mLineVertices;
    };

    struct DrawList
    {
        DrawList(std::unique_ptr<Commands> aCommands) :
            mCommands{std::move(aCommands)}
        {}

        std::unique_ptr<Commands> mCommands;
    };


    class Registry
    {        
        friend class DebugDrawer;

    public:
        static Registry & GetInstance()
        {
            static Registry gInstance;
            return gInstance;
        }

        void startFrame();
        DrawList endFrame();

        std::shared_ptr<DebugDrawer> addDrawer(const std::string & aName);
        std::shared_ptr<DebugDrawer> get(const std::string & aName) const;

    private:
        std::unordered_map<std::string/*logger name*/, std::shared_ptr<DebugDrawer>> mDrawers;
        // The owning location of the centralized command list, shared by all drawers
        std::unique_ptr<Commands> mFrameCommands;
        Level mGlobalLevel{Level::trace};
    };

    DebugDrawer(Level aLevel, const PrivateToken &) :
        mLevel{aLevel}
    {}

    static Registry & GetRegistry()
    { return Registry::GetInstance(); }

    void addLine(LineVertex aP1, LineVertex aP2)
    {
        commands().mLineVertices.push_back(std::move(aP1));
        commands().mLineVertices.push_back(std::move(aP2));
    }

private:
    Commands & commands()
    {
        return *GetRegistry().mFrameCommands;
    }

    Level mLevel;
};

#endif // SIMPLER_DBGDRAW

} // namespace ad::debug
