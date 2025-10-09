#pragma once

#include "../core/types.hpp"
#include "../core/config.hpp"
#include <etl/array.h>
#include "../os/sync.hpp"

namespace emCore {
    
    /**
     * @brief Memory block header for tracking allocation
     */
    struct memory_block_header {
        size_t size;
        bool is_free;
        memory_block_header* next;
        
        memory_block_header() noexcept : size(0), is_free(true), next(nullptr) {}
    };
    
    /**
     * @brief Fixed-size memory pool without dynamic allocation
     * @tparam BlockSize Size of each memory block
     * @tparam BlockCount Number of blocks in the pool
     */
    template<size_t BlockSize, size_t BlockCount>
    class memory_pool {
    private:
        // Ensure robust alignment for MCU/peripheral (DMA-safe) access and allow
        // relocating pool storage via EMCORE_BSS_ATTR (e.g., PSRAM on ESP32).
        EMCORE_BSS_ATTR alignas(std::max_align_t) u8 pool_[BlockSize * BlockCount];
        etl::array<memory_block_header, BlockCount> headers_;
        // Indirection pointers to allow optional external backing without changing API
        u8* pool_ptr_;
        memory_block_header* headers_ptr_;
        memory_block_header* free_list_;
        size_t allocated_count_;
        // Optional thread-safety
        mutable os::critical_section cs_;
        
    public:
        static_assert(BlockSize > 0, "memory_pool BlockSize must be > 0");
        static_assert(BlockCount > 0, "memory_pool BlockCount must be > 0");
        memory_pool() noexcept : pool_ptr_(pool_), headers_ptr_(headers_.data()), free_list_(nullptr), allocated_count_(0) {
            initialize();
        }
        /**
         * @brief Construct with external backing buffers (optional)
         * @param external_buffer Buffer of at least BlockSize*BlockCount bytes
         * @param external_buffer_bytes Size of external_buffer in bytes
         * @param external_headers Array of BlockCount headers
         * @param header_count Number of headers in external_headers
         */
        memory_pool(u8* external_buffer,
                    size_t external_buffer_bytes,
                    memory_block_header* external_headers,
                    size_t header_count) noexcept
            : pool_ptr_(pool_), headers_ptr_(headers_.data()), free_list_(nullptr), allocated_count_(0) {
            // Validate and adopt external buffers if sizes match, otherwise fall back to internal storage.
            if (external_buffer != nullptr && external_buffer_bytes >= storage_bytes()) {
                pool_ptr_ = external_buffer;
            }
            if (external_headers != nullptr && header_count >= BlockCount) {
                headers_ptr_ = external_headers;
            }
            initialize();
        }
        
        /**
         * @brief Initialize the memory pool
         */
        void initialize() noexcept {
            // Initialize free list
            free_list_ = &headers_ptr_[0];
            
            for (size_t i = 0; i < BlockCount; ++i) {
                headers_ptr_[i].size = BlockSize;
                headers_ptr_[i].is_free = true;
                headers_ptr_[i].next = (i < BlockCount - 1) ? &headers_ptr_[i + 1] : nullptr;
            }
            
            allocated_count_ = 0;
        }
        
        /**
         * @brief Allocate a memory block
         * @param size Requested size (must be <= BlockSize)
         * @return Pointer to allocated memory or nullptr if failed
         */
        void* allocate(size_t size) noexcept {
            // Optional thread-safety scope
            struct cs_scope { os::critical_section& c; explicit cs_scope(os::critical_section& c_) : c(c_) { if (config::pools_thread_safe) c.enter(); } ~cs_scope(){ if (config::pools_thread_safe) c.exit(); } };
            cs_scope guard(cs_);
            if (size > BlockSize || free_list_ == nullptr) {
                return nullptr;
            }
            
            memory_block_header* block = free_list_;
            free_list_ = block->next;
            
            block->is_free = false;
            block->next = nullptr;
            
            ++allocated_count_;
            
            // Calculate memory address from header index
            size_t index = static_cast<size_t>(block - &headers_ptr_[0]);
            return &pool_ptr_[index * BlockSize];
        }
        
        /**
         * @brief Deallocate a memory block
         * @param ptr Pointer to memory to deallocate
         * @return True if successful, false otherwise
         */
        bool deallocate(void* ptr) noexcept {
            if (ptr == nullptr) {
                return false;
            }
            
            // Calculate block index from memory address
            u8* mem_ptr = static_cast<u8*>(ptr);
            if (mem_ptr < pool_ptr_ || mem_ptr >= (pool_ptr_ + storage_bytes())) {
                return false; // Not from this pool
            }
            
            // Optional thread-safety scope
            struct cs_scope { os::critical_section& c; explicit cs_scope(os::critical_section& c_) : c(c_) { if (config::pools_thread_safe) c.enter(); } ~cs_scope(){ if (config::pools_thread_safe) c.exit(); } };
            cs_scope guard(cs_);

            size_t index = static_cast<size_t>((mem_ptr - pool_ptr_) / BlockSize);
            if (index >= BlockCount) {
                return false;
            }
            
            memory_block_header* block = &headers_ptr_[index];
            if (block->is_free) {
                return false; // Double free
            }
            
            // Add back to free list
            block->is_free = true;
            block->next = free_list_;
            free_list_ = block;
            
            --allocated_count_;
            return true;
        }
        
        /**
         * @brief Get number of allocated blocks
         * @return Number of allocated blocks
         */
        size_t get_allocated_count() const noexcept {
            return allocated_count_;
        }
        
        /**
         * @brief Get number of free blocks
         * @return Number of free blocks
         */
        size_t get_free_count() const noexcept {
            return BlockCount - allocated_count_;
        }
        
        /**
         * @brief Check if pool is full
         * @return True if no free blocks available
         */
        bool is_full() const noexcept {
            return allocated_count_ == BlockCount;
        }
        
        /**
         * @brief Get block size
         * @return Size of each block in bytes
         */
        constexpr size_t get_block_size() const noexcept {
            return BlockSize;
        }
        
        /**
         * @brief Get total number of blocks
         * @return Total number of blocks in pool
         */
        constexpr size_t get_block_count() const noexcept {
            return BlockCount;
        }

        /**
         * @brief Get total storage bytes occupied by this pool's data area
         */
        static constexpr size_t storage_bytes() noexcept {
            return BlockSize * BlockCount;
        }
    };
    
    /**
     * @brief Multi-pool memory manager
     */
    class memory_manager {
    private:
        memory_pool<config::small_block_size, config::small_pool_count> small_pool_;
        memory_pool<config::medium_block_size, config::medium_pool_count> medium_pool_;
        memory_pool<config::large_block_size, config::large_pool_count> large_pool_;
        
    public:
        memory_manager() noexcept = default;
        
        /**
         * @brief Allocate memory from appropriate pool
         * @param size Requested size
         * @return Pointer to allocated memory or nullptr
         */
        void* allocate(size_t size) noexcept {
            if (size <= config::small_block_size) {
                return small_pool_.allocate(size);
            } else if (size <= config::medium_block_size) {
                return medium_pool_.allocate(size);
            } else if (size <= config::large_block_size) {
                return large_pool_.allocate(size);
            }
            return nullptr; // Size too large
        }
        
        /**
         * @brief Deallocate memory
         * @param ptr Pointer to memory to deallocate
         * @return True if successful
         */
        bool deallocate(void* ptr) noexcept {
            if (ptr == nullptr) {
                return false;
            }
            
            // Try each pool
            return small_pool_.deallocate(ptr) ||
                   medium_pool_.deallocate(ptr) ||
                   large_pool_.deallocate(ptr);
        }
        
        /**
         * @brief Get memory usage statistics
         */
        struct memory_stats {
            size_t small_allocated;
            size_t small_free;
            size_t medium_allocated;
            size_t medium_free;
            size_t large_allocated;
            size_t large_free;
        };
        
        memory_stats get_stats() const noexcept {
            return {
                small_pool_.get_allocated_count(),
                small_pool_.get_free_count(),
                medium_pool_.get_allocated_count(),
                medium_pool_.get_free_count(),
                large_pool_.get_allocated_count(),
                large_pool_.get_free_count()
            };
        }
    };
    
} // namespace emCore
