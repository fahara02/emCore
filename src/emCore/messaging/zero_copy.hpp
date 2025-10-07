#pragma once
#include "../core/types.hpp"
#include "message_types.hpp"
#include "../os/sync.hpp"
#include <cstddef>
// ---- Consolidated Zero-Copy Messaging Support ----
namespace emCore::messaging {

    // Forward declaration of pool template
    template <size_t BlockSize, size_t BlockCount>
    class zero_copy_pool;
    
    // Handle for zero_copy_pool blocks with refcount.
    template <typename PoolT>
    class zc_handle {
    public:
        using pool_type = PoolT;
    
        zc_handle() noexcept : pool_(nullptr), index_(0xFFFF), size_(0) {}
    
        zc_handle(const zc_handle& other) noexcept : pool_(other.pool_), index_(other.index_), size_(other.size_) {
            add_ref_();
        }
    
        zc_handle& operator=(const zc_handle& other) noexcept {
            if (this != &other) {
                release_();
                pool_ = other.pool_;
                index_ = other.index_;
                size_ = other.size_;
                add_ref_();
            }
            return *this;
        }
    
        zc_handle(zc_handle&& other) noexcept : pool_(other.pool_), index_(other.index_), size_(other.size_) {
            other.pool_ = nullptr; other.index_ = 0xFFFF; other.size_ = 0;
        }
    
        zc_handle& operator=(zc_handle&& other) noexcept {
            if (this != &other) {
                release_();
                pool_ = other.pool_; index_ = other.index_; size_ = other.size_;
                other.pool_ = nullptr; other.index_ = 0xFFFF; other.size_ = 0;
            }
            return *this;
        }
    
        ~zc_handle() { release_(); }
    
        [[nodiscard]] bool valid() const noexcept { return pool_ != nullptr && index_ != 0xFFFF; }
        [[nodiscard]] u8* data() noexcept { return valid() ? pool_->data(index_) : nullptr; }
        [[nodiscard]] const u8* data() const noexcept { return valid() ? pool_->data(index_) : nullptr; }
        [[nodiscard]] u16 size() const noexcept { return size_; }
    
    private:
        friend pool_type; // Only pool can construct raw handle safely
        zc_handle(pool_type* pool, u16 index, u16 size) noexcept : pool_(pool), index_(index), size_(size) { add_ref_(); }
        void add_ref_() noexcept { if (pool_ && index_ != 0xFFFF) { pool_->add_ref(index_); } }
        void release_() noexcept { if (pool_ && index_ != 0xFFFF) { pool_->release(index_); } }
    
        pool_type* pool_;
        u16 index_;
        u16 size_;
    };
    
    // Zero-copy pool with fixed-size blocks and reference counting.
    template <size_t BlockSize, size_t BlockCount>
    class zero_copy_pool {
    public:
        struct node {
            alignas(4) u8 payload[BlockSize];
            u16 size;
            u16 refs;
            u16 next;
            bool in_use;
        };
    
        using handle_t = zc_handle<zero_copy_pool<BlockSize, BlockCount>>;
    
        zero_copy_pool() noexcept { initialize(); }
    
        void initialize() noexcept {
            free_head_ = 0;
            for (size_t i = 0; i < BlockCount; ++i) {
                nodes_[i].size = 0;
                nodes_[i].refs = 0;
                nodes_[i].next = (i == BlockCount - 1) ? 0xFFFF : static_cast<u16>(i + 1);
                nodes_[i].in_use = false;
            }
        }
    
        handle_t allocate(u16 size) noexcept {
            if (size > BlockSize) { return handle_t(); }
            cs_.enter();
            if (free_head_ == 0xFFFF) { cs_.exit(); return handle_t(); }
            u16 idx = free_head_;
            free_head_ = nodes_[idx].next;
            nodes_[idx].size = size;
            // Do not pre-increment refs here because handle constructor will add_ref_()
            nodes_[idx].refs = 0;
            nodes_[idx].in_use = true;
            nodes_[idx].next = 0xFFFF;
            cs_.exit();
            return handle_t(this, idx, size);
        }
    
        void add_ref(u16 index) noexcept {
            cs_.enter();
            if (index < BlockCount && nodes_[index].in_use && nodes_[index].refs != 0xFFFF) { ++nodes_[index].refs; }
            cs_.exit();
        }
    
        void release(u16 index) noexcept {
            cs_.enter();
            if (index < BlockCount && nodes_[index].in_use && nodes_[index].refs > 0) {
                if (--nodes_[index].refs == 0) {
                    nodes_[index].in_use = false;
                    nodes_[index].next = free_head_;
                    free_head_ = index;
                }
            }
            cs_.exit();
        }
    
        u8* data(u16 index) noexcept { return (index < BlockCount) ? nodes_[index].payload : nullptr; }
        const u8* data(u16 index) const noexcept { return (index < BlockCount) ? nodes_[index].payload : nullptr; }
        u16 block_size(u16 index) const noexcept { return (index < BlockCount) ? nodes_[index].size : 0; }
    
        [[nodiscard]] size_t capacity() const noexcept { return BlockCount; }
    
    private:
        os::critical_section cs_;
        etl::array<node, BlockCount> nodes_{};
        u16 free_head_{0xFFFF};
    };
    
    // Zero-copy message envelope with header and handle to pool memory.
    template <typename PoolT>
    struct zc_message_envelope {
        message_header header{};
        typename PoolT::handle_t handle{};
    
        [[nodiscard]] u8* payload_data() noexcept { return handle.valid() ? handle.data() : nullptr; }
        [[nodiscard]] const u8* payload_data() const noexcept { return handle.valid() ? handle.data() : nullptr; }
        [[nodiscard]] u16 payload_size() const noexcept { return handle.valid() ? handle.size() : 0; }
    
        [[nodiscard]] bool has_flag(message_flags flag) const noexcept {
            return (static_cast<message_flags>(header.flags) & flag) == flag;
        }
    };
    
    } // namespace emCore::messaging
    
