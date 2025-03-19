#pragma once
#include <SDL2/SDL.h>
#include <SDL_render.h>

#include <functional>
#include <string_view>
#include "../util/gfx/types.hpp"

namespace sdl {

	struct Window {
		using EventHandlerFn = std::function<void(SDL_Event&)>;
		using SizeT = util::Size<std::uint16_t>;

		Window() = default;
		Window(const std::string_view title, const SizeT& size);

		Window(const Window&) = delete;
		Window(Window&&) = delete; // This effectively acts like a unique_ptr, so maybe we should ALLOW moving

		~Window();

		void Create(const std::string_view title, const SizeT& size);
		void Destroy();

		SDL_Window* Raw() const { return window; }

		SDL_Surface* GetSurface();

		SDL_Rect GetRect();

		void Resize(const SizeT& newSize);

		void Poll();

		void On(SDL_EventType ev, const EventHandlerFn& fn);

	   private:
		SDL_Window* window {};
		std::unordered_map<std::uint32_t, EventHandlerFn> events {};
	};

} // namespace vncc::sdl