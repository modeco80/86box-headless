
#include "buffer_pool.hpp"

#include <cstdlib>
#include <new>

// set to 1 to enable debug messages
#define BUFFERPOOL_DEBUG_LOGS 1

#if BUFFERPOOL_DEBUG_LOGS
	#include <cstdio>
	#define BufferPool_DPRINTF(fmt, ...) printf("util::BufferPool Debug: " fmt "\n", ##__VA_ARGS__)
#else
	#define BufferPool_DPRINTF(fmt, ...)
#endif

namespace util {

	namespace {

		/// The max amount of Buffers in reserve that can be in any pool's freelist 
		/// before we perform garbage colllection to free some memory.
		constexpr static auto kBufferPoolGCSize = 48;

	} // namespace

	BufferPool::~BufferPool() {
		Clear();
	}

	BufferPool::PooledBuffer* BufferPool::GetBuffer(std::size_t wantedLength) {
		BufferPool_DPRINTF("Entering BufferPool::GetBuffer(wantedLength: %lu)", wantedLength);
		PooledBuffer* pIter = freeListHead;

		// Go through head-first the freelist to try and find a buffer that has
		// a suitable capacity.
		while(pIter) {
			// We were able to find a previously returned buffer which has a suitable capacity 
			// and is not in use. Simply return that Buffer. (Happy path)
			if(pIter->length >= wantedLength && !pIter->inUse) {
				BufferPool_DPRINTF("Found unused Buffer in freelist with suitable length for user request (user request %lu, actual length %lu)",
								   wantedLength, pIter->length);
				pIter->inUse = true;
				pIter->userRequestedLength = wantedLength;
				return pIter;
			}
			pIter = pIter->freeListNext;
		}

		BufferPool_DPRINTF("Could not find Buffer for user requested length %lu, allocating a new one", wantedLength);

		// Give up and allocate a new Buffer not on the freelist. (sad path)
		return AllocateBuffer(wantedLength);
	}

	void BufferPool::ReturnBuffer(PooledBuffer* pPooledBuffer) {
		if(pPooledBuffer == nullptr)
			return;

		bool foundInList = false;

		BufferPool_DPRINTF("Entering BufferPool::ReturnBufferUnsafe(pPooledBuffer: %p)", pPooledBuffer);

		if(freeListHead != nullptr) {
			PooledBuffer* pFindIter = freeListHead;
			while(pFindIter) {
				if(pFindIter == pPooledBuffer) {
					BufferPool_DPRINTF("Pooled Buffer %p was found in freelist (%p)", pPooledBuffer, pFindIter);
					foundInList = true;
					break;
				}

				pFindIter = pFindIter->freeListNext;
			}

			if(!foundInList) {
				BufferPool_DPRINTF("Did not find pooled Buffer %p in list", pPooledBuffer);
			}
		}

		// Mark the buffer as free to re-use.
		if(pPooledBuffer->inUse)
			pPooledBuffer->inUse = false;

		// The buffer is already in the pool's freelist,
		// so we do not need to re-add it.
		if(foundInList)
			return;

		BufferPool_DPRINTF("Current freelist size: %lu", FreelistSize());

		// Perform garbage collection
		if(FreelistSize() >= kBufferPoolGCSize) {
			BufferPool_DPRINTF("Garbage collecting freelist, it is too large.");
			GarbageCollect();
		}

		// Insert into the free list.
		pPooledBuffer->freeListPrev = freeListTail;
		if(freeListTail)
			freeListTail->freeListNext = pPooledBuffer;
		else
			freeListHead = pPooledBuffer;

		freeListTail = pPooledBuffer;
	}

	void BufferPool::Clear() {
		// Clear all buffers.
		while(freeListHead) {
			if(auto* ptr = RemoveBuffer(freeListTail); ptr != nullptr) {
				BufferPool_DPRINTF("Clear(): Freeing removed Buffer %p", ptr);
				FreeBuffer(ptr);
			}
		}
	}

	void BufferPool::GarbageCollect() {
		if(!freeListHead)
			return;

		PooledBuffer* pIter = freeListTail;

		while(pIter) {
			auto next = pIter->freeListPrev;

			// Buffer is in use, so move on to the next possible Buffer.
			if(pIter->inUse) {
				pIter = next;
				continue;
			} else {
				BufferPool_DPRINTF("Found unused Buffer with length %lu we can remove from freelist: %p", pIter->length, pIter);
				// Remove Buffer from the list.
				if(auto* ptr = RemoveBuffer(pIter); ptr != nullptr)
					FreeBuffer(ptr);
				pIter = next;
			}
		}
	}

	std::size_t BufferPool::FreelistSize() {
		// It is empty.
		if(freeListHead == nullptr)
			return 0;

		PooledBuffer* pInsertIter = freeListHead;
		std::size_t i = 0;
		while(pInsertIter) {
			i++;
			pInsertIter = pInsertIter->freeListNext;
		}

		return i;
	}

	BufferPool::PooledBuffer* BufferPool::RemoveBuffer(PooledBuffer* buffer) {
		if(!buffer)
			return nullptr;

		if(buffer->freeListPrev != nullptr)
			buffer->freeListPrev->freeListNext = buffer->freeListNext;
		if(buffer->freeListNext != nullptr)
			buffer->freeListNext->freeListPrev = buffer->freeListPrev;

		if(buffer == freeListHead)
			freeListHead = buffer->freeListNext;

		if(buffer == freeListTail)
			freeListTail = buffer->freeListPrev;

		buffer->freeListNext = nullptr;
		buffer->freeListPrev = nullptr;
		return buffer;
	}

	/*static*/ BufferPool::PooledBuffer* BufferPool::AllocateBuffer(std::size_t length) {
		// A Buffer is allocated with the header (the PooledBuffer structure) at the start,
		// then the buffer data.
		auto pAlloced = calloc(((length) * sizeof(std::uint8_t)) + sizeof(PooledBuffer), 1);
		if(pAlloced == nullptr)
			return nullptr;

		// I'm aware the "hip" C++(20|23) way of doing this is e.g: std::start_lifetime_as,
		// but placement new works just fine, and accomplishes the same goal.
		auto pBuffer = new(pAlloced) PooledBuffer;

		// Initialize the pooled Buffer bufferucture explicitly.
		// (TODO: Shouldn't this be done in the conbuffeructor? It'd make this a bit less haphazard)
		pBuffer->length = length;
		pBuffer->userRequestedLength = length;
		pBuffer->freeListPrev = nullptr;
		pBuffer->freeListNext = nullptr;
		pBuffer->inUse = false;
		return pBuffer;
	}

	/*static*/ void BufferPool::FreeBuffer(PooledBuffer* pPooled) {
		pPooled->~PooledBuffer();
		free(pPooled);
	}

} // namespace letsplay
