#include <SDL_error.h>
#include <SDL_events.h>
#include <SDL_surface.h>
#include <SDL_video.h>

#include <cstdio>
#include "../util/gfx/surface.hpp"
#include "../util/asio_util/asio_types.hpp"

#include "proto/mononoke_client.pb.h"
#include "proto/mononoke_server.pb.h"
#include "../util/asio_util/mononoke_framed.hpp"

#include <exception>
#include <variant>
#include <deque>

#include "sdl_window.hpp"

template <typename... Ts>
struct OverloadVisitor : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
OverloadVisitor(Ts...) -> OverloadVisitor<Ts...>;

/// Creates a SDL_Surface from a Surface::View, without copying memory.
SDL_Surface *
CreateSDLSurfaceFromUtil(util::Surface::View view)
{
    auto size    = view.GetSize();
    auto surface = SDL_CreateRGBSurfaceFrom(static_cast<void *>(view.Data()), size.width, size.height, 32, view.Pitch(),
                                            // clang-format off
											0x00'ff'00'00, 
											0x00'00'ff'00, 
											0x00'00'00'ff, 
											0xff'00'00'00
                                            // clang-format on
    );

    return surface;
}

/// The VNCC app.
struct VnccApp {
    struct KeyEvent {
        std::uint16_t key;
        bool          pressed;
    };

    struct MouseEvent {
        util::Point<std::uint16_t> at;
        std::uint8_t               buttons;
    };

    struct ResizeEvent {
        util::Surface::SizeT size;
    };

    struct FrameUpdate {
        std::vector<util::Surface::RectT> rects;
    };

    // queue types
    using SendQueueEvent
        = std::variant<KeyEvent, MouseEvent>;
    using RecvQueueEvent = std::variant<ResizeEvent, FrameUpdate>;

    explicit VnccApp(asio::io_context &ioc)
        : ioc(ioc)
    {
    }

    void Run()
    {
        // TODO: Setenvs to force software rendering?
        // Not that we *have to* but it's much less of a thread hog

        if (!InitWindow())
            return;

        // Spawn coroutine
        asio::co_spawn(ioc, CoroMain(), [&](auto ep) { 
            try {
                std::rethrow_exception(ep);
            } catch(std::exception& ex) {
                printf("exception in main coro: %s\n", ex.what());
            }
            stop = true; 
        });

        // Run the Asio and SDL event loops together.
        while (!stop) {
            ioc.poll();
            PollSDL();
        }

        ioc.stop();
    }

private:
    bool InitWindow()
    {
        window.Create("Mononoke Client", { 800, 600 });

        if (window.Raw() == nullptr) {
            return false;
        }

        window.On(SDL_QUIT, [&](auto &ev) {
            static_cast<void>(ev);
            stop = true;
        });

#if 0
		window.On(SDL_MOUSEMOTION, [&](SDL_Event& ev) {
			sendQueue.push_back(
			MouseEvent { .at = { static_cast<u16>(ev.motion.x), static_cast<u16>(ev.motion.y) }, .buttons = currentMouseButtonState });
		});

		window.On(SDL_MOUSEBUTTONDOWN, [&](SDL_Event& ev) {
			int dummy;
			currentMouseButtonState = SDL_GetMouseState(&dummy, &dummy);
			sendQueue.push_back(
			MouseEvent { .at = { static_cast<u16>(ev.button.x), static_cast<u16>(ev.button.y) }, .buttons = currentMouseButtonState });
		});

		window.On(SDL_MOUSEBUTTONUP, [&](SDL_Event& ev) {
			int dummy;
			currentMouseButtonState = SDL_GetMouseState(&dummy, &dummy);
			sendQueue.push_back(
			MouseEvent { .at = { static_cast<u16>(ev.button.x), static_cast<u16>(ev.button.y) }, .buttons = currentMouseButtonState });
		});

		window.On(SDL_KEYDOWN, [&](SDL_Event& ev) { sendQueue.push_back(KeyEvent { static_cast<u16>(ev.key.keysym.sym), true }); });
		window.On(SDL_KEYUP, [&](SDL_Event& ev) { sendQueue.push_back(KeyEvent { static_cast<u16>(ev.key.keysym.sym), false }); });
#endif
        return true;
    }

    void PollSDL()
    {
        // Pop an event off the recv queue
        if (!recvQueue.empty()) {
            auto &ev = recvQueue.front();

            // Operate on the event
            std::visit(OverloadVisitor { [&](ResizeEvent &resize) {
                                            printf("resized to %dx%d\n", resize.size.width, resize.size.height);
                                            ResizeWindow(resize.size);
                                            return;
                                        },
                                         [&](FrameUpdate &frameUpdate) {
                                             if (frameUpdate.rects.size() == 0)
                                                 return;

                                             printf("update %d rects\n", frameUpdate.rects.size());

                                             for (std::size_t i = 0; i < frameUpdate.rects.size(); ++i) {
                                                 auto    &rect = frameUpdate.rects[i];
                                                 SDL_Rect r    = { rect.x, rect.y, rect.width, rect.height };
                                                 SDL_BlitSurface(sdlSurf, &r, window.GetSurface(), &r);
                                             }
                                         } },
                       ev);

            recvQueue.pop_front();
        }

        // Poll SDL events after processing the last event (which will pressent the frame)
        window.Poll();

        SDL_UpdateWindowSurface(window.Raw());
    }

    void ResizeWindow(const sdl::Window::SizeT &size)
    {
        window.Resize(size);
        CreateSDLSurface();
    }

    void DestroySDLSurface()
    {
        if (sdlSurf) {
            SDL_FreeSurface(sdlSurf);
        }
    }

    void CreateSDLSurface()
    {
        if (sdlSurf) {
            DestroySDLSurface();
        }

        sdlSurf = CreateSDLSurfaceFromUtil(surf.AsView());
        if (!sdlSurf) {
            return;
        }
    }

    void DestroyWindow()
    {
        window.Destroy();
        DestroySDLSurface();
    }

    util::Awaitable<void> CoroMain()
    {
        asio::steady_timer timer { ioc };

        asio::local::stream_protocol::socket socket(ioc);

        co_await socket.async_connect("/tmp/86Box-mononoke-build");

        printf("Connected to server\n");

        while (!stop) {
            if (!socket.is_open()) {
                printf("Connection closed..?\n");
                stop = true;
                break;
            }

            auto msg = co_await util::AsyncReadMononokeFramed<mononoke::ServerMessage>(socket);

            switch (msg.Union_case()) {
                case mononoke::ServerMessage::kRects:
                    {
                        std::vector<util::Surface::RectT> rects;
                        auto rectsMsg = msg.rects();

                        for(std::size_t i = 0; i < rectsMsg.rects_size(); ++i) {
                            auto rectMsg = rectsMsg.rects(i);
                            auto rect = util::Surface::RectT {
                                .x = static_cast<std::uint16_t>(rectMsg.x()),
                                .y = static_cast<std::uint16_t>(rectMsg.y()),
                                .width = static_cast<std::uint16_t>(rectMsg.w()),
                                .height = static_cast<std::uint16_t>(rectMsg.h()),
                            };

                            auto rectPixels = reinterpret_cast<const util::Pixel*>(rectMsg.rectdata().data());
                            auto surfPixels = surf.GetData();



                            // Manually blit pixels
                            for(auto y = 0; y < rect.height; ++y) {
                                memcpy(&surfPixels[(rect.y + y) * surf.GetSize().width + rect.x], &rectPixels[y * rect.width], rect.width * sizeof(util::Pixel));

                                // GRRRR
                                for(auto x = 0; x < rect.width; ++x) {
                                    surfPixels[(rect.y + y) * surf.GetSize().width + (rect.x + x)].a = 0xff;
                                }
                            }

                            printf("painted rect %dx%d @ %dx%d\n", rect.width, rect.height, rect.x, rect.y);

                            rects.push_back(rect);
                        }


                        recvQueue.push_back(FrameUpdate {
                            .rects = rects
                        });
                    }
                    break;

                case mononoke::ServerMessage::kResize:
                    {
                        auto resize = msg.resize();
                        util::Surface::SizeT utilSize = { static_cast<std::uint16_t>(resize.w()), static_cast<std::uint16_t>(resize.h()) };
                        surf.Resize(utilSize);
                        recvQueue.push_back(ResizeEvent {
                            .size = utilSize
                        });
                    }
                    break;

                default:
                    printf("Unhandled union case\n");
                    break;
            }

            #if 0
            // Send a single event
            if (!sendQueue.empty()) {
                auto &ev = sendQueue.front();

                    co_await std::visit(OverloadVisitor { [&](KeyEvent &key) -> Awaitable<void> {
                                                             co_await client.KeyUpdate(key.key, key.pressed);
                                                             co_return;
                                                         },

                                                          [&](MouseEvent &mouse) -> Awaitable<void> {
                                                              co_await client.MouseUpdate(mouse.at.x, mouse.at.y, mouse.buttons);
                                                              co_return;
                                                          } }

                                        ,
                                        ev);
                // Done processing this event
                sendQueue.pop_front();
            }

#endif


            timer.expires_after(std::chrono::milliseconds(16));
            co_await timer.async_wait(asio::deferred);
        }

        recvQueue.clear();
        sendQueue.clear();
        DestroyWindow();
        co_return;
    }

    asio::io_context &ioc;

    // SDL window state
    sdl::Window  window;
    SDL_Surface *sdlSurf { nullptr };

    util::Surface surf;

    std::uint8_t currentMouseButtonState = 0;

    bool stop { false };

    std::deque<SendQueueEvent> sendQueue {};
    std::deque<RecvQueueEvent> recvQueue {};
};

int
main(int argc, char **argv)
{
    //if (argc != 2 || std::string_view(argv[1]) == "--help") {
    //    std::fprintf(stderr, "Usage: %s [Mononoke server, UDS]\n", argv[0]);
    //    return 1;
    //}

    asio::io_context ioc(1);
    VnccApp          app(ioc);

    SDL_Init(SDL_INIT_VIDEO);

    app.Run();

    SDL_Quit();
    return 0;
}