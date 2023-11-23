
#include "rampserver.hpp"

//#include <google/protobuf/arena.h>
//#include <generated/ramp.pb.h>

#include <cstdio>

// C glue layer used to tack onto the main loop
extern "C" {

    extern char vm_name[1024]; // We use this to make the socket path

    // A pointer to the server. It's heap allocated using new
    ramp::Server* theServer = nullptr;


    void ramp_server_start() {
        theServer = new ramp::Server;

        //std::printf("/tmp/86box-ramp-%s\n", vm_name);
        // Block the main thread like a Boss XD
        //uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    }

    void ramp_server_stop() {
        // done
        theServer->Stop();
        delete theServer;
    }

}

namespace ramp {

    struct Server::Impl {
        Impl(Server& server)
            : server(server) {

        }

        ~Impl() {

        }

        bool Start() {
            return true;
        }

	void Stop() {
		return;
	}
    private:
        Server& server;
    };

    /* static */ Server& Server::the() {
        return *theServer;
    }

    Server::Server()
        : impl(new Impl(*this)) {

    }

    Server::~Server() {
        delete impl;
    }

    bool Server::Start() {
        return impl->Start();
    }

    void Server::Stop() {
	impl->Stop();
    }
}

