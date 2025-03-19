#pragma once

#include <cstdint>

namespace util {

template <class T>
struct Point {
    T x;
    T y;
};

template <class T>
struct Size {
    T width;
    T height;

    constexpr std::size_t Linear() const { return static_cast<std::size_t>(width) * static_cast<std::size_t>(height); }
};

template <class T>
struct Rect {
    T x;
    T y;
    T width;
    T height;

    // constexpr Rect(T x, T y, T w, T h) : x(x), y(y), width(w), height(h) {}

    // constexpr Rect() = default;

    // constexpr explicit Rect(Size<T> size) : x(0), y(0), width(size.width), height(size.height) {}

    /**
     * Get the origin coordinate as a point.
     * \return a Point<T> with the origin.
     */
    constexpr auto GetOrigin() const { return Point<T> { .x = x, .y = y }; }

    /**
     * Get the size of this rect.
     * \return a Point<T> which contains the calculated size of the rect
     */
    constexpr auto GetSize() const { return Size<T> { .width = width, .height = height }; }

    constexpr bool InBounds(const Rect &other)
    {
        if (x < other.x || x + other.width > other.x + other.width)
            return false;

        if (y < other.y || x + other.height > other.y + other.height)
            return false;

        return true;
    }

    // more methods.
};


union Pixel {
    std::uint32_t raw;

    /// color accessors
    struct {
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;
        std::uint8_t a;
    };

    constexpr static Pixel FromRgb565(std::uint16_t pixel) {
        return Pixel { .r = static_cast<std::uint8_t>(((pixel & 0xF800) >> 11) << 3),
                       .g = static_cast<std::uint8_t>(((pixel & 0x7E0) >> 5) << 2),
                       .b = static_cast<std::uint8_t>((pixel & 0x1F) << 3),
                       .a = 255 };
    }
};

}