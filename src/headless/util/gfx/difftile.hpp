#pragma once

#include "surface.hpp"
#include <vector>

namespace util {
    
    std::size_t DifferenceTileSurfaces(
        /// Surfaces to difference
        Surface* surfaceLeft,
        Surface* surfaceRight,
        std::vector<Surface::RectT>& outTiles,
        std::uint32_t tileSize = 16
    );

}