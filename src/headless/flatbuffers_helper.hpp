#pragma once
#include <flatbuffers/flatbuffers.h>

namespace ramp {

/// A helper class for dealing with FlatBuffers.
struct FlatbufferHelper {
    // default constructor. we default to a 256k buffer size to avoid any allocations
    // occuring in flatbuffers code (but we do reuse the buffer anyways so its probably not that big of a deal)
    FlatbufferHelper()
        : fbBuilder(262144)
    {
    }

    /// A helper function to make building FlatBuffers root messages a bit less of a PITA.
    ///
    /// [preBuild] can return a anon struct (or tuple) with the fields you need serialized before the message is ready to be built
    /// [createCallback] is a callback which recieves the builder so fields can be added and the prebuild structure
    /// [doneCallback] is a callback which recieves the finished flatbuffer pointer and size.
    template <class Root, class PreBuild, class Create, class Done>
    inline void BuildMessage(PreBuild &&preBuild, Create &&createCallback, Done &&doneCallback)
    {
        auto preData = preBuild(fbBuilder);

        {
            typename Root::Builder messageBuilder { fbBuilder };
            createCallback(preData, messageBuilder);
            auto message = messageBuilder.Finish();
            fbBuilder.Finish(message);
        }

        // call the user's done callback then clear the buffer builder so it can be reused for another message without reallocating.
        // (we typically will copy the resulting buffer to a ramp::MessageBuffer object that is ref-counted, so this is perfectly fine).
        doneCallback(fbBuilder.GetBufferPointer(), fbBuilder.GetSize());
        fbBuilder.Clear();
    }

private:
    /// Shared FlatBuffers builder.
    flatbuffers::FlatBufferBuilder fbBuilder {};
};

} // namespace ramp