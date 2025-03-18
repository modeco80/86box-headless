
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

struct MononokeFrameHeader {
    char          magic[4]; // MMSG
    std::uint32_t dataSize;

    MononokeFrameHeader()
    {
        magic[0] = 'M';
        magic[1] = 'M';
        magic[2] = 'S';
        magic[3] = 'G';
    }
};

util::BufferPool mononokeProtobufsPool;

/// Sends a mononoke framed protobuf message to the given AsyncWriteStream.
template <class AsyncWriteStream>
util::Awaitable<void>
AsyncSendMononokeFramed(AsyncWriteStream &stream, google::protobuf::Message &message)
{
    MononokeFrameHeader header;
    auto                bufSize = message.ByteSizeLong();

    auto *buffer = mononokeProtobufsPool.GetBuffer(bufSize);
    if (buffer == nullptr)
        throw std::runtime_error("Buffer pool was unable to serve request");

    if (!message.SerializeToArray(&buffer[0], bufSize))
        throw std::runtime_error("Failed to serialize Mononoke protobuf");

    // Swap data size to BE.
    header.dataSize = __builtin_bswap32(bufSize);

    // Use Scatter-gather I/O for nyoom.
    std::array<asio::const_buffer, 2> buffers = {
        asio::buffer(static_cast<void *>(&header), sizeof(header)),
        asio::buffer(buffer, bufSize)
    };

    // Write it, and once we're done in either success or failure case return the buffer
    // back to the pool.
    try {
        co_await asio::async_write(stream, buffers, asio::deferred);
    } catch (boost::system::system_error &ec) {
        mononokeProtobufsPool.ReturnBuffer(buffer);
        throw;
        co_return;
    }

    mononokeProtobufsPool.ReturnBuffer(buffer);
    co_return;
}

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

                co_await AsyncSendMononokeFramed(socket, serverMessage);

                while (true) {
                    char buffer[4] {};

                    auto n = co_await socket.async_read_some(asio::mutable_buffer(&buffer[0], 4));
                    for (auto i = 0; i < n; ++i)
                        std::putc(buffer[i], stdout);
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

private:
    Server &server;

    // TODO:
    // Surface for blit buffer
    // Mutex for surface (since blit runs on another seperate thread)
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
}
