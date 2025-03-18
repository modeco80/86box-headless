#pragma once
#include "asio_types.hpp"

namespace util {

	/// An asynchronous version of std::condition_variable.
	/// Useful for synchronizing or pausing coroutines.
	struct AsyncCondVar {
		AsyncCondVar(asio::any_io_executor exec) {
			timer = std::make_unique<SteadyTimer>(exec);
			timer->expires_at(std::chrono::steady_clock::time_point::max());
		}

		void NotifyOne() { timer->cancel_one(); }

		void NotifyAll() { timer->cancel(); }

		template <class Predicate>
		Awaitable<void> Wait(Predicate pred) {
			while(!pred()) {
				try {
					co_await timer->async_wait(asio::deferred);
				} catch(...) {
					// swallow errors
				}
			}
		}

	   private:
		std::unique_ptr<SteadyTimer> timer;
	};

} // namespace util