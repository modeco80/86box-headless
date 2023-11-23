
#include <functional>
#include <cstdint>
#include <string>
#include <memory>

using u8 = std::uint8_t;
using s8 = std::int8_t;

using u16 = std::uint16_t;
using s16 = std::int16_t;

using u32 = std::uint32_t;
using s32 = std::int32_t;

using u64 = std::uint64_t;
using s64 = std::int64_t;

namespace ramp {

	struct Server {

		Server();
		~Server();

        Server(const Server&) = delete;
        Server(Server&&) = delete;

		bool Start();
		void Stop(); // unlinks socket as well? or toctou
		

        // callback handlers (replace this with a more performant one?)
        std::function<void(u16, u16, u8)> OnMouse;
        std::function<void(u16)> OnKey;


	
		// Valid when a r8mp server is created; invalid otherwise
		static Server& the();
	
		// internal emulator surface - only call these if you know
		
		
		void BlitResize(u16 w, u16 h);
		
		// dunno how to handle this. i hope i can just use buffer32 as is
		void Blit(u16 x, u16 y, u16 w, u16 h, const u8* data);
		
		void Audio(const u8* sampleData, u16 sampleCount);
	
	private:
        struct Impl;
        std::unique_ptr<Impl> impl;
	};

}