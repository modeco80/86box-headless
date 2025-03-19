
#include "mononoke_server.hpp"
#include <google/protobuf/message.h>
#include <set>
#include <stdexcept>

#include "proto/mononoke_client.pb.h"
#include "proto/mononoke_server.pb.h"

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include "util/asio_util/asio_types.hpp"
#include "util/buffer_pool.hpp"

#include "util/asio_util/mononoke_framed.hpp"

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

    /// A Mononoke client session.
    struct ClientSession : public std::enable_shared_from_this<ClientSession> {
        ClientSession(Impl &impl, StreamProtocol::socket &&socket)
            : impl(impl)
            , socket(std::move(socket))
        {
            asio::co_spawn(socket.get_executor(), ReadCoro(), asio::detached);
        }

        void Close()
        {
            try {
                socket.close();
                impl.OnSessionClosed(shared_from_this());
            } catch (std::exception &a) {
            }
        }

        util::Awaitable<void> ReadCoro()
        {
            try {

                // TEMP: Send a resize message to make sure AsyncSendMononokeFramed works
                mononoke::ServerMessage serverMessage;
                auto                   *resize = serverMessage.mutable_resize();
                resize->set_w(640);
                resize->set_h(480);

                co_await util::AsyncSendMononokeFramed(socket, serverMessage);

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

        // TODO: on connect we should send some info
        // like initial size and full framebuffer at the time

    private:
        Impl                  &impl;
        StreamProtocol::socket socket;
        // TODO: Send buffer
    };

    Impl(Server &server)
        : server(server)
        , acceptor(ioc)
    {
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

    void OnSessionClosed(std::shared_ptr<ClientSession> session)
    {
        printf("Session closed\n");
        sessions.erase(session);
    }

    void BlitResize(u16 w, u16 h)
    {
        // Implement me!
    }

    void Blit(u16 x, u16 y, u16 w, u16 h)
    {
        // Implement me!
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

void Server::Blit(u16 x, u16 y, u16 w, u16 h) {
    impl->Blit(x, y, w, h);
}
}
