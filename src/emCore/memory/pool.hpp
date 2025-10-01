#pragma once

#include "../core/types.hpp"
#include "../core/config.hpp"
#include <etl/array.h>

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
        alignas(sizeof(void*)) u8 pool_[BlockSize * BlockCount];
        etl::array<memory_block_header, BlockCount> headers_;
        memory_block_header* free_list_;
        size_t allocated_count_;
        
    public:
        memory_pool() noexcept : free_list_(nullptr), allocated_count_(0) {
            initialize();
        }
        
        /**
         * @brief Initialize the memory pool
         */
        void initialize() noexcept {
            // Initialize free list
            free_list_ = &headers_[0];
            
            for (size_t i = 0; i < BlockCount; ++i) {
                headers_[i].size = BlockSize;
                headers_[i].is_free = true;
                headers_[i].next = (i < BlockCount - 1) ? &headers_[i + 1] : nullptr;
            }
            
            allocated_count_ = 0;
        }
        
        /**
         * @brief Allocate a memory block
         * @param size Requested size (must be <= BlockSize)
         * @return Pointer to allocated memory or nullptr if failed
         */
        void* allocate(size_t size) noexcept {
            if (size > BlockSize || free_list_ == nullptr) {
                return nullptr;
            }
            
            memory_block_header* block = free_list_;
            free_list_ = block->next;
            
            block->is_free = false;
            block->next = nullptr;
            
            ++allocated_count_;
            
            // Calculate memory address from header index
            size_t index = block - &headers_[0];
            return &pool_[index * BlockSize];
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
            if (mem_ptr < pool_ || mem_ptr >= pool_ + sizeof(pool_)) {
                return false; // Not from this pool
            }
            
            size_t index = (mem_ptr - pool_) / BlockSize;
            if (index >= BlockCount) {
                return false;
            }
            
            memory_block_header* block = &headers_[index];
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
