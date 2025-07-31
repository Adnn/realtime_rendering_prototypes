#include "DebugDrawer.h"

#include "../log/Logging.h"


namespace ad::debug {


#if defined(SIMPLER_DBGDRAW)

namespace {

    //  Line list representing the connectivity of a box
    // Given as corner indices for math::Box ordering
    const std::vector<unsigned int> gBoxLineConnectivity{
        0, 1,   1, 3,   3, 2,   2, 0, // z min face
        4, 5,   5, 7,   7, 6,   6, 4, // z max face
        0, 4,   5, 1,   3, 7,   2, 6, // join
    };

} // unnamed namespace


// TODO: port the better box implementation from Snacman debug drawer
void DebugDrawer::addBox(const math::Box<float> & aBox,
                         scenic::Pose aPose,
                         math::hdr::Rgba_f aColor)
{
    ad::math::AffineMatrix<4, float> transform{aPose};
    for (std::size_t cornedIdx = 0;
         cornedIdx < gBoxLineConnectivity.size();
         cornedIdx += 2)
    {
        LineVertex p1{
            .mPosition = (math::homogeneous::makePosition(aBox.cornerAt(gBoxLineConnectivity[cornedIdx]))
                         * transform).xyz(),
            .mColor = aColor,
        };
        LineVertex p2{
            .mPosition = (math::homogeneous::makePosition(aBox.cornerAt(gBoxLineConnectivity[cornedIdx + 1]))
                         * transform).xyz(),
            .mColor = aColor,
        };
        addLine(p1, p2);
    }
}


#else
void DebugDrawer::Registry::startFrame()
{
    mFrameCommands = std::make_unique<Commands>();
}
    

DebugDrawer::DrawList DebugDrawer::Registry::endFrame()
{ 
    return{std::move(mFrameCommands)};
}

std::shared_ptr<DebugDrawer> DebugDrawer::Registry::addDrawer(const std::string & aName)
{
    auto [drawer, didInsert] = mDrawers.emplace(aName, std::make_shared<DebugDrawer>(mGlobalLevel, DebugDrawer::PrivateToken{}));
    if(!didInsert)
    {
        ADLOG(warn)("DebugDrawer name '{}' already registered.", aName);
    }
    return drawer->second;
}


std::shared_ptr<DebugDrawer> DebugDrawer::Registry::get(const std::string & aName) const
{
    auto found = mDrawers.find(aName);
    if(found == mDrawers.end())
    {
        ADLOG(error)("No DebugDrawer named '{}'.", aName);
        throw std::invalid_argument{"Requested DebugDrawer is not present."};
    }
    else
    {
        return found->second;
    }
}
#endif // SIMPLER_DBGDRAW

} // namespace ad::debug
