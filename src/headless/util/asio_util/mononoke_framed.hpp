#pragma once

#include "../buffer_pool.hpp"
#include <bit>
#include <google/protobuf/message.h>

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include "asio_types.hpp"

namespace util {

extern util::BufferPool mononokeProtobufsPool;

/// Header for framed Mononoke protobuf messages.
struct MononokeFrameHeader {
    constexpr static auto VALID_MAGIC = 0x46'55'42'4d; // 'MBUF'
    uint32_t              magic;
    std::uint32_t         dataSize; // BE

    constexpr MononokeFrameHeader()
    {
        magic    = VALID_MAGIC;
        dataSize = 0;
    }

    bool Valid() const
    {
        if (magic != VALID_MAGIC) {
            return false;
        }

        // Client->server messages should never be above 8kb
        // If this changes, this can be updated
        return GetDataSize() >= (8 * (1024 * 1024));
    }

    void SetDataSize(std::uint32_t len)
    {
        if constexpr (std::endian::native == std::endian::little)
            dataSize = __builtin_bswap32(len);
        else
            dataSize = len;
    }

    std::uint32_t GetDataSize() const
    {
        if constexpr (std::endian::native == std::endian::little)
            return __builtin_bswap32(this->dataSize);
        else
            return this->dataSize;
    }
};

/// Sends a mononoke framed protobuf message to the given AsyncWriteStream.
template <class AsyncWriteStream>
util::Awaitable<void>
AsyncSendMononokeFramed(AsyncWriteStream &stream, google::protobuf::Message &message)
{
    MononokeFrameHeader header;
    auto                bufSize = message.ByteSizeLong();

    // Get a pooled buffer used to hold the serialized protobuf.
    //auto *buffer = mononokeProtobufsPool.GetBuffer(bufSize);
    //if (buffer == nullptr)
    //    throw std::runtime_error("Buffer pool was unable to serve request");
    auto buffer = std::make_unique<std::uint8_t[]>(bufSize);

    // Serialize the protobuf.
    header.SetDataSize(bufSize);
    if (!message.SerializeToArray(&buffer[0], bufSize))
        throw std::runtime_error("Failed to serialize Mononoke protobuf");

    // Scatter-gather I/O buffers. Allows us to make one async_write call, which turns into
    // (hopefully, on *good* operating systems) a single writev() system call (or its analog).
    std::array<asio::const_buffer, 2> buffers = {
        asio::buffer(static_cast<void *>(&header), sizeof(header)),
        asio::buffer(buffer.get(), bufSize)
    };

    // Write the message.
    // Once we're done, in either success or failure case return the buffer back to the pool.
    try {
        co_await asio::async_write(stream, buffers, asio::deferred);
    } catch (boost::system::system_error &ec) {
    //    mononokeProtobufsPool.ReturnBuffer(buffer);
        throw;
        co_return;
    }

   // mononokeProtobufsPool.ReturnBuffer(buffer);
    co_return;
}

/// Reads a Mononoke framed protobuf message with the provided root.
template <class TRoot, class AsyncReadStream>
util::Awaitable<TRoot>
AsyncReadMononokeFramed(AsyncReadStream &stream)
    requires(std::is_base_of<google::protobuf::Message, TRoot>::value)
{
    MononokeFrameHeader header;

    // Issue the first read for the header.
    co_await asio::async_read(stream, asio::mutable_buffer(&header, sizeof(header)));

    printf("read header\n");

    if (!header.Valid())
        throw std::runtime_error("Invalid frame header.");

    printf("header is valid apparently, %d bytes (0x%08x)\n", header.GetDataSize(), header.GetDataSize());

    // Now that we know the size of the message, let's get a buffer off the pool to read the
    // serialized protobuf into temporairly.
    //auto *pBuffer = mononokeProtobufsPool.GetBuffer(header.GetDataSize());
    //if (pBuffer == nullptr)
     //   throw std::runtime_error("Could not get a pooled buffer to read serialized protobuf into");

    auto buffer = std::make_unique<std::uint8_t[]>(header.GetDataSize());

    // Try to read into the buffer the pool gave us.
    try {
        co_await asio::async_read(stream, asio::mutable_buffer(buffer.get(), header.GetDataSize()));
    } catch (boost::system::system_error &err) {
       // mononokeProtobufsPool.ReturnBuffer(pBuffer);
        throw;
    }

    // We read data, let's parse into the root.
    TRoot ret;
    if (!ret.ParseFromArray(&buffer[0], header.GetDataSize())) {
        //mononokeProtobufsPool.ReturnBuffer(pBuffer);
        throw std::runtime_error("Failed to parse protobuf from the buffer.");
    }

    // Well, it worked.
    //mononokeProtobufsPool.ReturnBuffer(pBuffer);
    co_return ret;
}

}