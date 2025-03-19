
#include "mononoke_server.hpp"
#include <deque>
#include <google/protobuf/message.h>
#include <set>
#include <stdexcept>

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

struct Server::Impl {

    template<class T>
    inline std::shared_ptr<T> MakeProtobufMsg() {
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
                        auto message = co_await util::AsyncReadMononokeFramed<mononoke::ClientMessage>(socket);
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
        // Thank 86Box.
        surfaces[0].Resize({ 2048, 2048 });
        surfaces[1].Resize({ 2048, 2048 });
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

                session->Send(MakeInitialSizeMsg());
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

    void BroadcastToSessions(std::shared_ptr<mononoke::ServerMessage> message) {
        for(auto sp: sessions)
            sp->Send(message);
    }

    void OnSessionClosed(std::shared_ptr<ClientSession> session)
    {
        printf("Session closed\n");
        sessions.erase(session);
    }

    std::shared_ptr<mononoke::ServerMessage> MakeInitialSizeMsg() {
        auto sm = std::make_shared<mononoke::ServerMessage>();
        auto size = sm->mutable_resize();

        {
            std::scoped_lock lk(surfaceLock);
            size->set_w(this->size.width);
            size->set_h(this->size.height);
        }

        return sm;
    }


    std::shared_ptr<mononoke::ServerMessage> MakeInitialBlitMsg()
    {
        std::unique_ptr<util::Pixel[]> pixelsTemp;
        util::Surface::SizeT           sizeTemp;

        // Paint the buffer.
        {
            std::scoped_lock lk(surfaceLock);
            sizeTemp   = size;
            pixelsTemp = std::make_unique<util::Pixel[]>(size.Linear());

            auto &buffer = surfaces[backBufferIndex];
            auto *bufPtr = buffer.GetData();

            // Copy line by line the data.
            for (auto y = 0; y < size.height; y++) {
                memcpy(&pixelsTemp[y * (std::size_t)size.width], &bufPtr[y * 2048], (std::size_t)size.width * sizeof(util::Pixel));
            }
        }

        // Okay, we've got the buffer. Now let's do something worthwhile with it.
        auto sm = std::make_shared<mononoke::ServerMessage>();

        // Messy.
        auto  rects = sm->mutable_rects();
        auto *rect  = rects->add_rects();
        rect->set_x(0);
        rect->set_y(0);
        rect->set_w(sizeTemp.width);
        rect->set_h(sizeTemp.height);
        rect->set_rectdata(reinterpret_cast<const char *>(&pixelsTemp[0]), sizeTemp.Linear() * sizeof(util::Pixel));

        return sm;
    }

    void BlitResize(u16 w, u16 h)
    {

        if (size.width != w && size.height != h) {
            {
                std::scoped_lock lk(surfaceLock);
                printf("Mononoke: display (would be) resized to %dx%d\n", w, h);
                // No I'm not happy.
                size = { w, h };
            }

            auto blitMsg = MakeInitialBlitMsg();
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

#if 0
        printf("Differing rects: %lu\n", rects.size());
        for (auto &rect : rects) {
            printf("  %dx%d @ %dx%d\n", rect.width, rect.height, rect.x, rect.y);
        }
#endif
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
    util::Surface::SizeT              size{};
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
