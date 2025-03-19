#pragma once

#include "types.hpp"
#include <memory>

namespace util {

    /// A simple framebuffer surface in RGBA8888 format.
	struct Surface {
		using SizeType = std::uint16_t;
		using PointT = Point<SizeType>;
		using SizeT = Size<SizeType>;
		using RectT = Rect<SizeType>;

		/// A cheap copyable view of a surface. Valid as long as the surface object exists.
		struct View {
			[[nodiscard]] constexpr SizeT GetSize() const { return { .width = offs.width, .height = offs.height }; }

			Pixel* Data() { return &surf->pixelData.get()[offs.y * surf->GetSize().width + offs.x]; }

			std::size_t Pitch() {
				// the pitch is the original framebuffer's width
				// basically, how many bytes we need to go to
				// get to the next line of the original framebuffer
				return (std::size_t)surf->GetSize().width * sizeof(Pixel);
			}

			Surface* surf; // Introspection only
			RectT offs;	   // do not touch or I will be saddened
		};

		Surface();

		inline explicit Surface(SizeT size) : Surface() { Resize(size); }

		Surface(const Surface&) = delete;
		Surface& operator=(const Surface&) = delete;

		// Allow move-construction of framebuffers
		Surface(Surface&&) noexcept = default;
		Surface& operator=(Surface&&) noexcept = default;

		inline Pixel& operator[](PointT pt) { return pixelData[size.width * pt.y + pt.x]; }

		/// Resize this framebuffer.
		void Resize(SizeT newSize);

		void Clear();

		[[nodiscard]] constexpr const SizeT& GetSize() const { return size; }

		[[nodiscard]] Pixel* GetData() const { return pixelData.get(); }

		/// Paint a given bit of data to the surface.
		void Paint(RectT srcAt, PointT dstAt, Pixel* pixelData);

		/// Returns a whole-surface View
		inline View AsView() {
			return Subrect({
			.x = 0,
			.y = 0,
			.width = size.width,
			.height = size.height,
			});
		}

		View Subrect(const RectT& at);

	   private:
		// yknow
		constexpr auto Stride() const { return size.width * sizeof(Pixel); }

		SizeT size {};
		std::unique_ptr<Pixel[]> pixelData;
	};

}