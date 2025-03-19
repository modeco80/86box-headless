
#include "mononoke_server.hpp"
#include <deque>
#include <google/protobuf/message.h>
#include <set>
#include <stdexcept>
#include <span>

#include "proto/mononoke_client.pb.h"
#include "proto/mononoke_server.pb.h"

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include "util/asio_util/asio_types.hpp"
#include "util/asio_util/async_condvar.hpp"
#include "util/buffer_pool.hpp"

#include "util/asio_util/mononoke_framed.hpp"
#include "util/gfx/difftile.hpp"
#include "util/gfx/surface.hpp"

extern "C" {
#include <86box/video.h>
}

namespace asio       = boost::asio;
using StreamProtocol = asio::local::stream_protocol;

#include <cstdio>

// A pointer to the server. It's heap allocated using new
std::unique_ptr<mononoke::Server> theServer;

// C glue layer used to tack onto the main loop
extern "C" {

extern char vm_name[1024]; // We use this to make the socket path

void
mononoke_server_start()
{
    theServer = std::make_unique<mononoke::Server>();
    theServer->Start();
}

void
mononoke_server_stop()
{
    theServer->Stop();
    theServer.reset();
}
}

namespace mononoke {
    constexpr static auto kMaxWh = 2048;
    constexpr static auto kMaxSize = util::Surface::SizeT { kMaxWh, kMaxWh } ;

struct Server::Impl {

    template <class T>
    inline std::shared_ptr<T> MakeProtobufMsg()
    {
        return std::make_shared<T>();
    }

    /// A Mononoke client session.
    struct ClientSession : public std::enable_shared_from_this<ClientSession> {
        ClientSession(Impl &impl, StreamProtocol::socket &&socket)
            : impl(impl)
            , socket(std::move(socket))
            , server_send_queue_cv(socket.get_executor())
        {
            asio::co_spawn(socket.get_executor(), ReadCoro(), asio::detached);
            asio::co_spawn(socket.get_executor(), WriteCoro(), asio::detached);
        }

        void Close()
        {
            try {
                socket.close();
                server_send_queue_cv.NotifyOne();
                impl.OnSessionClosed(shared_from_this());
            } catch (std::exception &a) {
            }
        }

        void Send(std::shared_ptr<mononoke::ServerMessage> proto)
        {
            server_protos_queue.push_back(proto);
            server_send_queue_cv.NotifyOne();
        }

        util::Awaitable<void> ReadCoro()
        {
            try {
                while (true) {
                    try {
                        auto message = co_await util::AsyncReadMononokeFramed<mononoke::ClientMessage>(socket, true);
                        // TODO: Handle message
                    } catch (std::runtime_error &re) {
                        printf("Error parsing client message: %s\n", re.what());
                        co_return Close();
                    }
                }
            } catch (boost::system::system_error &err) {

                printf("Error in Mononoke server: %s\n", err.what());
                co_return Close();
            }
        }

        util::Awaitable<void> WriteCoro()
        {
            try {
                while (true) {
                    // Wait for there to have been at least something
                    co_await server_send_queue_cv.Wait([&]() {
                        if (!socket.is_open())
                            return true;
                        return !server_protos_queue.empty();
                    });

                    if (!socket.is_open())
                        break;

                    // Write them.
                    while (!server_protos_queue.empty()) {
                        auto &sp = server_protos_queue.front();
                        co_await util::AsyncSendMononokeFramed(socket, *sp);
                        server_protos_queue.pop_front();
                    }
                }
            } catch (boost::system::system_error &err) {
                printf("Error in Mononoke server (write): %s\n", err.what());
                co_return Close();
            }

            printf("just making sure this ends\n");
            co_return;
        }

    private:
        Impl                  &impl;
        StreamProtocol::socket socket;

        std::deque<std::shared_ptr<mononoke::ServerMessage>> server_protos_queue;
        util::AsyncCondVar                                   server_send_queue_cv;
    };

    Impl(Server &server)
        : server(server)
        , acceptor(ioc)
    {
        // Pre-allocate surfaces.
        // Thank 86Box.
        surfaces[0].Resize(kMaxSize);
        surfaces[1].Resize(kMaxSize);
    }

    ~Impl()
    {
    }

    bool
    Start()
    {
        asio::co_spawn(ioc.get_executor(), Listener(), asio::detached);

        ioc.run();
        return true;
    }

    util::Awaitable<void>
    Listener()
    {
        try {
            char path[256] {};
            std::snprintf(&path[0], sizeof(path) - 1, "/tmp/86Box-mononoke-%s", vm_name);

            // do the dance
            acceptor.open();
            acceptor.bind(std::string(path));
            acceptor.listen(asio::socket_base::max_listen_connections);

            printf("Mononoke: Listening on socket \"%s\"\n", path);

            while (true) {
                auto socket = co_await acceptor.async_accept(asio::deferred);

                auto session = std::make_shared<ClientSession>(*this, std::move(socket));

                session->Send(MakeSizeMsg());
                session->Send(MakeInitialBlitMsg());

                sessions.insert(session);
            }
        } catch (boost::system::system_error &err) {
            printf("Error in Mononoke server: %s\n", err.what());
        }

        co_return;
    }

    void
    Stop()
    {
        acceptor.close();
        ioc.stop();
        sessions.clear();
        return;
    }

    void BroadcastToSessions(std::shared_ptr<mononoke::ServerMessage> message)
    {
        for (auto sp : sessions)
            sp->Send(message);
    }

    void OnSessionClosed(std::shared_ptr<ClientSession> session)
    {
        printf("Session closed\n");
        sessions.erase(session);
    }

    // NOTE: This function assumes you are holding the surface lock when you call it
    std::shared_ptr<mononoke::ServerMessage> MakeSizeMsg()
    {
        auto sm   = std::make_shared<mononoke::ServerMessage>();
        auto size = sm->mutable_resize();

        {
            size->set_w(this->size.width);
            size->set_h(this->size.height);
        }

        return sm;
    }

    std::shared_ptr<mononoke::ServerMessage> MakeBlitMsg(std::span<const util::Surface::RectT> rects)
    {
        auto             sm       = std::make_shared<mononoke::ServerMessage>();
        auto             rectsMsg = sm->mutable_rects();
        std::scoped_lock lk(surfaceLock);
        {
            for (auto &rect : rects) {
                auto *rectMsg = rectsMsg->add_rects();

                // Set everything up.
                rectMsg->set_x(rect.x);
                rectMsg->set_y(rect.y);
                rectMsg->set_w(rect.width);
                rectMsg->set_h(rect.height);

                util::Surface::SizeT           sizeTemp   = { rect.width, rect.height };
                std::unique_ptr<util::Pixel[]> pixelsTemp = std::make_unique<util::Pixel[]>(sizeTemp.Linear());

                // Paint the buffer.
                {
                    auto &buffer = surfaces[backBufferIndex];
                    auto *bufPtr = buffer.GetData();

                    // Copy line by line the data.
                    for (auto y = 0; y < rect.height; y++) {
                        memcpy(&pixelsTemp[y * sizeTemp.width], &bufPtr[(rect.y + y) * 2048 + rect.x], rect.width * sizeof(util::Pixel));
                    }
                }

                // ARGH why does protobuf gencode do this..
                //auto *pRectString = rectMsg->mutable_rectdata();
                //pRectString->resize(sizeTemp.Linear() * sizeof(util::Pixel));
                //memcpy(&pRectString->data()[0], &pixelsTemp[0], sizeTemp.Linear() * sizeof(util::Pixel));
                rectMsg->set_rectdata(reinterpret_cast<const char *>(&pixelsTemp[0]), sizeTemp.Linear() * sizeof(util::Pixel));
            }
        }

        return sm;
    }

    std::shared_ptr<mononoke::ServerMessage> MakeInitialBlitMsg()
    {
        const util::Surface::RectT rectArr[1] = {
            { .x      = 0,
             .y      = 0,
             .width  = size.width,
             .height = size.height }
        };

        return MakeBlitMsg(rectArr);
    }

    void BlitResize(u16 w, u16 h)
    {

        if (size.width != w && size.height != h) {
            {
                std::scoped_lock lk(surfaceLock);
                printf("Mononoke: display resized to %dx%d\n", w, h);
                // No I'm not happy.
                size = { w, h };
            }

            auto sizeMsg = [&]() {
                std::scoped_lock lk(surfaceLock);
                return MakeSizeMsg();
            }();

            auto blitMsg = MakeInitialBlitMsg();
            BroadcastToSessions(sizeMsg);
            BroadcastToSessions(blitMsg);
        }
    }

    void Blit(u16 x, u16 y, u16 w, u16 h)
    {

        {
            std::scoped_lock lk(surfaceLock);
            for (u32 row = 0; row < h; ++row)
                video_copy(reinterpret_cast<std::uint8_t *>(&surfaces[backBufferIndex].GetData()[row * surfaces[backBufferIndex].GetSize().width]), &(buffer32->line[y + row][x]), w * sizeof(uint32_t));

            util::DifferenceTileSurfaces(&surfaces[backBufferIndex], &surfaces[!backBufferIndex], rects);
            backBufferIndex = !backBufferIndex;
        }

        if (rects.size() == 0)
            return;

        auto sm = MakeBlitMsg(rects);

        // Post message broadcast back to the main thread.
        // Make the message on the blit thread, why not.
        asio::post(ioc, [this, sm_clone = sm]() {
            BroadcastToSessions(sm_clone);
        });
    }

private:
    Server &server;

    // TODO:
    // double-buffered Surface for blit buffer (we'll need two buffers to difference,
    //  because 86Box doesn't do it for us. Shame)
    // Mutex for surfaces (since blit runs on another seperate thread)
    // write pingpong index (can be a u8)
    //
    // PCM data buffer

    util::Surface                     surfaces[2];
    std::uint8_t                      backBufferIndex = 0; // use !backBufferIndex to get the last fully-drawn frame.
    util::Surface::SizeT              size {};
    std::vector<util::Surface::RectT> rects;
    std::mutex                        surfaceLock; // Must be held to mutate the above fields

    asio::io_context         ioc { 1 };
    StreamProtocol::acceptor acceptor;

    std::set<std::shared_ptr<ClientSession>> sessions;
};

Server &
Server::the()
{
    return *theServer;
}

Server::Server()
    : impl(std::make_unique<Impl>(*this))
{
}

Server::~Server() = default;

bool
Server::Start()
{
    return impl->Start();
}

void
Server::Stop()
{
    impl->Stop();
}

void
Server::BlitResize(u16 w, u16 h)
{
    impl->BlitResize(w, h);
}

void
Server::Blit(u16 x, u16 y, u16 w, u16 h)
{
    impl->Blit(x, y, w, h);
}
}
