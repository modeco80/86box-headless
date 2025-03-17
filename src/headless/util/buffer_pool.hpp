#pragma once

#include <cstddef>
#include <cstdint>

namespace util {

	/// A buffer pool
	struct BufferPool {

		struct PooledBuffer {
			PooledBuffer() = default;
			PooledBuffer(const PooledBuffer&) = delete;
			PooledBuffer(PooledBuffer&&) = delete;

			inline std::uint8_t* GetPointer() {
				// memory is allocated after this header, so we can just use this.
				return reinterpret_cast<std::uint8_t*>(this + 1);
			}

			/// Returns the length the user wanted
			constexpr std::size_t GetLength() const { return userRequestedLength; }

			/// Always returns the true size of the buffer
			constexpr std::size_t GetCapacity() const { return length; }
		protected:
			friend BufferPool;

			PooledBuffer* freeListPrev;
			PooledBuffer* freeListNext;

			std::size_t length;
			std::size_t userRequestedLength;
			bool inUse;
		};

		BufferPool() = default;
		BufferPool(const BufferPool&) = delete;
		BufferPool(BufferPool&&) = delete;

		~BufferPool();

		PooledBuffer* GetBuffer(std::size_t wantedLength);

		void ReturnBuffer(PooledBuffer* pPooledBuffer);

		/// Clears the pool's freelist, freeing all memory used immediately.
		/// This function should only be used if none of the buffers are in use;
		/// this function does not check for you.
		///
		/// Note that in normal usage you do not need to call this function;
		/// the destructor will automatically call this and clear whatever is in the freelist
		/// when the program exits.
		void Clear();

	public:

		/// Collects "garbage" unused buffers. Only buffers which
		/// are currently in use (and thus should not be removed/freed)
		/// will be kept in the pool's freelist; the rest will be freed.
		///
		/// You do not need to call this on your own, but it is provided
		/// as a public API in the case it may end up being useful.
		void GarbageCollect();

	private:
		/// Returns the size of the pool's freelist.
		std::size_t FreelistSize();

		/// Removes a buffer from the freelist. Returns the unlinked
		/// buffer so it can be freed with [BufferPool::FreeBuffer()].
		PooledBuffer* RemoveBuffer(PooledBuffer* str);

		/// Allocates a unlinked PooledBuffer structure, with the given buffer length.
		static PooledBuffer* AllocateBuffer(std::size_t length);

		/// Frees a unlinked PooledBuffer structure.
		static void FreeBuffer(PooledBuffer* pPooled);

		PooledBuffer* freeListHead{nullptr};
		PooledBuffer* freeListTail{nullptr};
	};

} // namespace util
