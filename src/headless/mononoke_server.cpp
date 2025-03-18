
#include "mononoke_server.hpp"
#include <set>

#include "util/asio_util/asio_types.hpp"

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
    // done
    theServer->Stop();
}
}

namespace mononoke {


/// A connected Mononoke user.
struct ServerSession : public std::enable_shared_from_this<ServerSession> {
    ServerSession(StreamProtocol::socket &&socket)
        : socket(std::move(socket))
    {
        // begin async read/write loop
    }

    void Close() 
    {

    }

    void DoRead()
    {
    }

    void OnRead(boost::system::error_code ec, std::size_t bytes_read)
    {
        if (ec) {
            // TODO: close connection (and notify server so it can delete us, otherwise
            // we'll leak)
            return Close();
        }

        // (TODO) Handle mononoke server message 


        // Do another read (we have the clear capability to)
        DoRead();
    }

private:
    StreamProtocol::socket socket;
    // TODO: Send buffer
};

struct Server::Impl {
    Impl(Server &server)
        : server(server), acceptor(ioc)
    {
    }

    ~Impl()
    {
    }

    bool Start()
    {
        // Start acceptor, if it fails, return false (so VM will exit)
        // Run I/O context

        ioc.run();
        return true;
    }

    void Stop()
    {
        // Close and delete all sessions
        // Stop acceptor
        // Stop I/O context (Server::Start will return)
        // ...

        return;
    }

private:
    Server &server;

    asio::io_context ioc{1};
    StreamProtocol::acceptor acceptor;

    std::set<std::shared_ptr<ServerSession>> sessions;
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
