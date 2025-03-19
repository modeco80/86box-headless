#include "surface.hpp"

#include <cstring>

namespace util {

Surface::Surface()
{
}

void
Surface::Resize(SizeT newSize)
{
    // Keep the old pixel data around for a bit
    // (would moving *this work better.. probably)
    auto oldBuffer = std::move(pixelData);
    auto oldSize   = size;

    pixelData = std::make_unique<Pixel[]>(newSize.width * newSize.height);
    if (!pixelData) {
        pixelData = std::move(oldBuffer);
        return;
    }

    size = newSize;

    // NOTE: might be nice to have this as a utility factory
    // in the Size template itself...

    if (oldBuffer) {
        // Copy the old framebuffer data then release it.
        auto copyableSize = SizeT { std::min(oldSize.width, size.width), std::min(oldSize.height, size.height) };
        Paint({ 0, 0, copyableSize.width, copyableSize.height }, { 0, 0 }, oldBuffer.get());
    }

    oldBuffer.reset();
}

void
Surface::Clear()
{
    pixelData.reset();
    size = { 0, 0 };
}

void
Surface::Paint(RectT srcAt, PointT dstAt, Pixel *pixelData)
{
    auto src_buffer = pixelData;
    // COLLABVM_ASSERT(src_buffer != nullptr, "the fuck are you DOING");

    for (std::size_t i = ((srcAt.x) + (srcAt.y * size.width)); i < (srcAt.y + srcAt.height) * size.width; i += size.width) {
        memcpy(this->pixelData.get() + i, src_buffer, srcAt.width * sizeof(Pixel));
        src_buffer += srcAt.width;
    }
}

Surface::View
Surface::Subrect(const RectT &at)
{
    // TODO: bound-check the rect to make sure it's inside ourselves
    return Surface::View { .surf = this, .offs = at };
}

}