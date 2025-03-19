#include "sdl_window.hpp"

#include <SDL_pixels.h>
#include <SDL_render.h>
#include <SDL_video.h>

namespace sdl {

	Window::Window(const std::string_view title, const SizeT& size) {
		Create(title, size);
	}

	Window::~Window() {
		Destroy();
	}

	void Window::Create(const std::string_view title, const SizeT& size) {
		// TODO Error handle
		if(!window) {
			window = SDL_CreateWindow(title.data(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, size.width, size.height, SDL_WINDOW_SHOWN);
		}
	}

	void Window::Destroy() {
		if(window) {
			SDL_DestroyWindow(window);
		}
	}

	SDL_Surface* Window::GetSurface() {
		return SDL_GetWindowSurface(window);
	}


	void Window::Poll() {
		SDL_Event ev;

		while(SDL_PollEvent(&ev) > 0) {
			// Call the given event handler if found
			if(events.find(ev.type) != events.end()) {
				events[ev.type](ev);
			}
		}
	}

	void Window::On(SDL_EventType ev, const EventHandlerFn& fn) {
		events[ev] = fn;
	}

	SDL_Rect Window::GetRect() {
		SDL_Rect rect {};
		SDL_GetWindowSize(window, &rect.w, &rect.h);
		return rect;
	}

	void Window::Resize(const SizeT& size) {
		SDL_SetWindowSize(window, size.width, size.height);
	}

} // namespace vncc::sdl