#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/basic_stream.hpp>
#include <boost/beast/core/error.hpp>

#if 0
	#include <boost/asio/experimental/coro.hpp>
	#include <boost/asio/experimental/use_coro.hpp>
#endif

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace util {

	// if we need strands this is here just in case :)
	using BaseExecutorType = asio::any_io_executor;
	using ExecutorType = BaseExecutorType;

	/// Awaitable type (configured for the current executor)
	template <class T>
	using Awaitable = asio::awaitable<T, ExecutorType>;

#if 0 // These currently don't work
	template <typename Yield = void, typename Return = void>
	using Coro = asio::experimental::coro<Yield, Return, ExecutorType>;

	/// Like Coro<> but avoids needing to specify a void Yield (if not required),
	/// useful for a pull-only coroutine
	template<typename Return>
	using CoroReturnOnly = Coro<void, Return>;
#endif

	template <typename Protocol>
	using Acceptor = asio::basic_socket_acceptor<Protocol, ExecutorType>;

	template <typename Protocol>
	using Socket = asio::basic_stream_socket<Protocol, ExecutorType>;

	using SteadyTimer = asio::basic_waitable_timer<std::chrono::steady_clock, asio::wait_traits<std::chrono::steady_clock>, ExecutorType>;

    /// Boost.Beast offers streams with functioning timeout.
	template <typename Protocol>
	using BeastStream = beast::basic_stream<Protocol, ExecutorType>;

	using TcpBeastStream = BeastStream<asio::ip::tcp>;
	using UnixBeastStream = BeastStream<asio::local::stream_protocol>;

} // namespace util