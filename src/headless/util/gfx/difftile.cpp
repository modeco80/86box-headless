#include "difftile.hpp"

namespace util {

/// Checks if two tile lines have the same contents.
inline bool
TileLineMatches(Pixel *tileLineLeft, Pixel *tileLineRight, std::size_t width)
{
#if 1 // always does a word compare, so this should ideally be faster (less words to compare)
    for (std::size_t i = 0; i < width; ++i)
        if (tileLineLeft[i].raw != tileLineRight[i].raw)
            return false;
    return true;
#else
    return !memcmp(tileLineOne, tileLineRight, width * sizeof(Pixel));
#endif
}

std::size_t
DifferenceTileSurfaces(
    Surface                     *surfaceLeft,
    Surface                     *surfaceRight,
    std::vector<Surface::RectT> &outTiles,
    std::uint32_t                tileSize)
{
    // FIXME: Make sure the surfaces are the same size w/ smth like this.
    //assert(surfaceLeft && surfaceRight);
    //assert((surfaceLeft->GetSize().width == surfaceRight->GetSize().width) && (surfaceLeft->GetSize().height == surfaceRight->GetSize().height))

    auto w = surfaceLeft->GetSize().width;
    auto h = surfaceLeft->GetSize().height;

    auto *surfaceData = surfaceLeft->GetData();
    auto *lastData    = surfaceRight->GetData();

    // so we overwrite
    outTiles.resize(0);

    for (std::uint32_t dy = 0; dy < h; dy += tileSize) {
        for (std::uint32_t dx = 0; dx < w; dx += tileSize) {
            std::uint32_t rw = std::min(tileSize, w - dx);
            std::uint32_t rh = std::min(tileSize, h - dy);

            Surface::RectT r {
                .x      = static_cast<std::uint16_t>(dx),
                .y      = static_cast<std::uint16_t>(dy),
                .width  = static_cast<std::uint16_t>(rw),
                .height = static_cast<std::uint16_t>(rh),
            };

            for (std::uint32_t y = 0; y < r.height; ++y) {
                const auto tileLineIndex = (r.y + y) * w + r.x;
                if (!TileLineMatches(&surfaceData[tileLineIndex], &lastData[tileLineIndex], r.width)) {
                    outTiles.emplace_back(r);
                    break;
                }
            }
        }
    }

    return outTiles.size();
}

}