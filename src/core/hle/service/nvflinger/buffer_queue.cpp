// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/alignment.h"
#include "common/scope_exit.h"
#include "core/core_timing.h"
#include "core/hle/service/nvflinger/buffer_queue.h"

namespace Service {
namespace NVFlinger {

BufferQueue::BufferQueue(u32 id, u64 layer_id) : id(id), layer_id(layer_id) {
    native_handle = Kernel::Event::Create(Kernel::ResetType::OneShot, "BufferQueue NativeHandle");
    native_handle->Signal();
}

void BufferQueue::SetPreallocatedBuffer(u32 slot, IGBPBuffer& igbp_buffer) {
    Buffer buffer{};
    buffer.slot = slot;
    buffer.igbp_buffer = igbp_buffer;
    buffer.status = Buffer::Status::Free;

    LOG_WARNING(Service, "Adding graphics buffer %u", slot);

    queue.emplace_back(buffer);

    if (buffer_wait_event) {
        buffer_wait_event->Signal();
    }
}

boost::optional<u32> BufferQueue::DequeueBuffer(u32 width, u32 height) {
    auto itr = std::find_if(queue.begin(), queue.end(), [&](const Buffer& buffer) {
        // Only consider free buffers. Buffers become free once again after they've been Acquired
        // and Released by the compositor, see the NVFlinger::Compose method.
        if (buffer.status != Buffer::Status::Free) {
            return false;
        }

        // Make sure that the parameters match.
        return buffer.igbp_buffer.width == width && buffer.igbp_buffer.height == height;
    });

    if (itr == queue.end()) {
        return boost::none;
    }

    buffer_wait_event = nullptr;

    itr->status = Buffer::Status::Dequeued;
    return itr->slot;
}

const IGBPBuffer& BufferQueue::RequestBuffer(u32 slot) const {
    auto itr = std::find_if(queue.begin(), queue.end(),
                            [&](const Buffer& buffer) { return buffer.slot == slot; });
    ASSERT(itr != queue.end());
    ASSERT(itr->status == Buffer::Status::Dequeued);
    return itr->igbp_buffer;
}

void BufferQueue::QueueBuffer(u32 slot, BufferTransformFlags transform) {
    auto itr = std::find_if(queue.begin(), queue.end(),
                            [&](const Buffer& buffer) { return buffer.slot == slot; });
    ASSERT(itr != queue.end());
    ASSERT(itr->status == Buffer::Status::Dequeued);
    itr->status = Buffer::Status::Queued;
    itr->transform = transform;
}

boost::optional<const BufferQueue::Buffer&> BufferQueue::AcquireBuffer() {
    auto itr = std::find_if(queue.begin(), queue.end(), [](const Buffer& buffer) {
        return buffer.status == Buffer::Status::Queued;
    });
    if (itr == queue.end())
        return boost::none;
    itr->status = Buffer::Status::Acquired;
    return *itr;
}

void BufferQueue::ReleaseBuffer(u32 slot) {
    auto itr = std::find_if(queue.begin(), queue.end(),
                            [&](const Buffer& buffer) { return buffer.slot == slot; });
    ASSERT(itr != queue.end());
    ASSERT(itr->status == Buffer::Status::Acquired);
    itr->status = Buffer::Status::Free;

    if (buffer_wait_event) {
        buffer_wait_event->Signal();
    }
}

u32 BufferQueue::Query(QueryType type) {
    LOG_WARNING(Service, "(STUBBED) called type=%u", static_cast<u32>(type));
    switch (type) {
    case QueryType::NativeWindowFormat:
        // TODO(Subv): Use an enum for this
        static constexpr u32 FormatABGR8 = 1;
        return FormatABGR8;
    }

    UNIMPLEMENTED();
    return 0;
}

void BufferQueue::SetBufferWaitEvent(Kernel::SharedPtr<Kernel::Event>&& wait_event) {
    ASSERT_MSG(!buffer_wait_event, "buffer_wait_event only supports a single waiting thread!");
    buffer_wait_event = std::move(wait_event);
}

} // namespace NVFlinger
} // namespace Service
