#pragma once

#include <cstddef>

#include "types.hpp"

// Optional attribute hook to relocate heavy BSS to external RAM on capable MCUs (e.g., ESP32 PSRAM).
// Keep empty by default for MCU-agnostic header-only design.
#ifndef EMCORE_BSS_ATTR
#define EMCORE_BSS_ATTR
#endif

// Feature toggles (macros for preprocessor guards)
#ifndef EMCORE_ENABLE_ZC
#define EMCORE_ENABLE_ZC 1
#endif
#ifndef EMCORE_ENABLE_EVENT_LOGS
#define EMCORE_ENABLE_EVENT_LOGS 0
#endif

// Enable/disable small-message broker
#ifndef EMCORE_ENABLE_SMALL_BROKER
#define EMCORE_ENABLE_SMALL_BROKER 1
#endif

// Core caps defaults for no-YAML/no-flags builds
#ifndef EMCORE_MAX_TASKS
#define EMCORE_MAX_TASKS 8
#endif
#ifndef EMCORE_MAX_EVENTS
#define EMCORE_MAX_EVENTS 16
#endif

// Global memory budget default for no-YAML/no-flags builds
#ifndef EMCORE_MEMORY_BUDGET_BYTES
#define EMCORE_MEMORY_BUDGET_BYTES 0
#endif

// Global feature toggles (moved from memory/budget.hpp)
#ifndef EMCORE_ENABLE_MESSAGING
#define EMCORE_ENABLE_MESSAGING 0
#endif
#ifndef EMCORE_ENABLE_EVENTS
#define EMCORE_ENABLE_EVENTS 0
#endif
#ifndef EMCORE_ENABLE_TASKS_REGION
#define EMCORE_ENABLE_TASKS_REGION 0
#endif
#ifndef EMCORE_ENABLE_OS_REGION
#define EMCORE_ENABLE_OS_REGION 0
#endif
#ifndef EMCORE_ENABLE_PROTOCOL
#define EMCORE_ENABLE_PROTOCOL 0
#endif
#ifndef EMCORE_ENABLE_DIAGNOSTICS
#define EMCORE_ENABLE_DIAGNOSTICS 0
#endif

// Pools region accounting and thread-safety toggle
#ifndef EMCORE_ENABLE_POOLS_REGION
#define EMCORE_ENABLE_POOLS_REGION 0
#endif
#ifndef EMCORE_POOLS_THREAD_SAFE
#define EMCORE_POOLS_THREAD_SAFE 0
#endif

// Reserve sizes and bookkeeping (moved from memory/budget.hpp)
#ifndef EMCORE_MSG_OVERHEAD_BYTES
#define EMCORE_MSG_OVERHEAD_BYTES 2048
#endif
#ifndef EMCORE_TASK_MEM_BYTES
#define EMCORE_TASK_MEM_BYTES 0
#endif
#ifndef EMCORE_TASK_PER_TCB_BYTES
#define EMCORE_TASK_PER_TCB_BYTES 256
#endif
#ifndef EMCORE_TASK_FIXED_OVERHEAD_BYTES
#define EMCORE_TASK_FIXED_OVERHEAD_BYTES 512
#endif
#ifndef EMCORE_TASK_MAILBOX_REF_BYTES
#define EMCORE_TASK_MAILBOX_REF_BYTES 8
#endif
#ifndef EMCORE_OS_MEM_BYTES
#define EMCORE_OS_MEM_BYTES 0
#endif
#ifndef EMCORE_PROTOCOL_MEM_BYTES
#define EMCORE_PROTOCOL_MEM_BYTES 0
#endif
#ifndef EMCORE_DIAGNOSTICS_MEM_BYTES
#define EMCORE_DIAGNOSTICS_MEM_BYTES 0
#endif

#ifndef EMCORE_PROTOCOL_PACKET_SIZE
#define EMCORE_PROTOCOL_PACKET_SIZE 64
#endif
#ifndef EMCORE_PROTOCOL_MAX_HANDLERS
#define EMCORE_PROTOCOL_MAX_HANDLERS 16
#endif
#ifndef EMCORE_PROTOCOL_RING_SIZE
#define EMCORE_PROTOCOL_RING_SIZE 512
#endif

// If available, include generated messaging configuration macros produced from YAML.
// This allows YAML to override the default capacities at compile time.
// You can force-ignore this generated header by defining EMCORE_IGNORE_GENERATED_MESSAGING_CONFIG=1
#if !defined(EMCORE_IGNORE_GENERATED_MESSAGING_CONFIG) && defined(__has_include)
#  if __has_include(<emCore/generated/messaging_config.hpp>)
#    include <emCore/generated/messaging_config.hpp>
#  endif
#endif

namespace emCore::config {
        
        // Task system configuration
        constexpr size_t max_tasks = EMCORE_MAX_TASKS;
        constexpr size_t max_task_name_length = 32;
        constexpr duration_t default_task_timeout = 1000; // ms
        
        constexpr size_t max_events = EMCORE_MAX_EVENTS;
        constexpr size_t max_event_handlers = 16;
        constexpr size_t event_queue_size = 64;
        
        // Messaging system configuration - defaults when not specified in YAML
        // These provide minimum safe values. Generators (YAML) can override at build time
        // by defining EMCORE_MSG_QUEUE_CAPACITY, EMCORE_MSG_MAX_TOPICS, and
        // EMCORE_MSG_MAX_SUBS_PER_TOPIC via compile definitions.
        #ifdef EMCORE_MSG_QUEUE_CAPACITY
        constexpr size_t default_mailbox_queue_capacity = EMCORE_MSG_QUEUE_CAPACITY;
        #else
        constexpr size_t default_mailbox_queue_capacity = 4; // tightened to reduce RAM
        #endif
        
        // Feature toggles as constexpr for compile-time branching
        constexpr bool enable_messaging   = (EMCORE_ENABLE_MESSAGING != 0);
        constexpr bool enable_events      = (EMCORE_ENABLE_EVENTS != 0);
        constexpr bool enable_tasks_region= (EMCORE_ENABLE_TASKS_REGION != 0);
        constexpr bool enable_os_region   = (EMCORE_ENABLE_OS_REGION != 0);
        constexpr bool enable_protocol    = (EMCORE_ENABLE_PROTOCOL != 0);
        constexpr bool enable_diagnostics = (EMCORE_ENABLE_DIAGNOSTICS != 0);
        constexpr bool enable_pools_region= (EMCORE_ENABLE_POOLS_REGION != 0);
        constexpr bool pools_thread_safe  = (EMCORE_POOLS_THREAD_SAFE != 0);

        // Reserve sizes exposed as constexpr
        constexpr size_t msg_overhead_bytes         = EMCORE_MSG_OVERHEAD_BYTES;
        constexpr size_t task_mem_bytes             = EMCORE_TASK_MEM_BYTES;
        constexpr size_t task_per_tcb_bytes         = EMCORE_TASK_PER_TCB_BYTES;
        constexpr size_t task_fixed_overhead_bytes  = EMCORE_TASK_FIXED_OVERHEAD_BYTES;
        constexpr size_t task_mailbox_ref_bytes     = EMCORE_TASK_MAILBOX_REF_BYTES;
        constexpr size_t os_mem_bytes               = EMCORE_OS_MEM_BYTES;
        constexpr size_t protocol_mem_bytes         = EMCORE_PROTOCOL_MEM_BYTES;
        constexpr size_t diagnostics_mem_bytes      = EMCORE_DIAGNOSTICS_MEM_BYTES;

        // Protocol minimal sizing knobs as constexpr
        constexpr size_t protocol_packet_size   = EMCORE_PROTOCOL_PACKET_SIZE;
        constexpr size_t protocol_max_handlers  = EMCORE_PROTOCOL_MAX_HANDLERS;
        constexpr size_t protocol_ring_size     = EMCORE_PROTOCOL_RING_SIZE;

        #ifdef EMCORE_MSG_MAX_TOPICS
        constexpr size_t default_max_topics = EMCORE_MSG_MAX_TOPICS;
        #else
        constexpr size_t default_max_topics = 6; // reduced to align with tests
        #endif

        #ifdef EMCORE_MSG_MAX_SUBS_PER_TOPIC
        constexpr size_t default_max_subscribers_per_topic = EMCORE_MSG_MAX_SUBS_PER_TOPIC;
        #else
        constexpr size_t default_max_subscribers_per_topic = 3; // tightened to reduce RAM
        #endif

        // Mailbox per-topic sub-queues configuration
        // Number of per-topic queue slots per mailbox
        #ifdef EMCORE_MSG_TOPIC_QUEUES_PER_MAILBOX
        constexpr size_t default_max_topic_queues_per_mailbox = EMCORE_MSG_TOPIC_QUEUES_PER_MAILBOX;
        #else
        constexpr size_t default_max_topic_queues_per_mailbox = 1; // tightened to reduce RAM
        #endif

        // High-priority reservation ratio for each per-topic queue (numerator/denominator)
        #ifdef EMCORE_MSG_TOPIC_HIGH_RATIO_NUM
        constexpr size_t default_topic_high_ratio_num = EMCORE_MSG_TOPIC_HIGH_RATIO_NUM;
        #else
        constexpr size_t default_topic_high_ratio_num = 1; // 1/4 by default
        #endif

        #ifdef EMCORE_MSG_TOPIC_HIGH_RATIO_DEN
        constexpr size_t default_topic_high_ratio_den = EMCORE_MSG_TOPIC_HIGH_RATIO_DEN;
        #else
        constexpr size_t default_topic_high_ratio_den = 4;
        #endif

        // QoS / Delivery settings (overridable via build defines)
        #ifdef EMCORE_MSG_QOS_PENDING_LIMIT
        constexpr size_t default_qos_pending_limit = EMCORE_MSG_QOS_PENDING_LIMIT;
        #else
        constexpr size_t default_qos_pending_limit = 4; // tightened to reduce RAM
        #endif

        #ifdef EMCORE_MSG_QOS_ACK_TIMEOUT_US
        constexpr u32 default_ack_timeout_us = EMCORE_MSG_QOS_ACK_TIMEOUT_US;
        #else
        constexpr u32 default_ack_timeout_us = 500000; // 500 ms
        #endif

        #ifdef EMCORE_MSG_REPUBLISH_BUFFER
        constexpr size_t default_republish_buffer = EMCORE_MSG_REPUBLISH_BUFFER;
        #else
        constexpr size_t default_republish_buffer = 4; // tightened to reduce RAM
        #endif

        // Zero-copy pool sizing (constexpr used by templates)
        #ifdef EMCORE_ZC_BLOCK_SIZE
        constexpr size_t zc_block_size = EMCORE_ZC_BLOCK_SIZE;
        #else
        constexpr size_t zc_block_size = 16; // conservative default
        #endif

        #ifdef EMCORE_ZC_BLOCK_COUNT
        constexpr size_t zc_block_count = EMCORE_ZC_BLOCK_COUNT;
        #else
        constexpr size_t zc_block_count = 4;  // conservative default
        #endif

        // Event log capacities (constexpr used by templates)
        #ifdef EMCORE_EVENT_LOG_MED_CAP
        constexpr size_t event_log_med_cap = EMCORE_EVENT_LOG_MED_CAP;
        #else
        constexpr size_t event_log_med_cap = 4;  // tightened to reduce RAM
        #endif

        #ifdef EMCORE_EVENT_LOG_SML_CAP
        constexpr size_t event_log_sml_cap = EMCORE_EVENT_LOG_SML_CAP;
        #else
        constexpr size_t event_log_sml_cap = 4;  // tightened to reduce RAM
        #endif

        #ifdef EMCORE_EVENT_LOG_ZC_CAP
        constexpr size_t event_log_zc_cap = EMCORE_EVENT_LOG_ZC_CAP;
        #else
        constexpr size_t event_log_zc_cap = 2;  // tightened to reduce RAM
        #endif
        
        // Memory pool configuration
        constexpr size_t small_block_size = 32;
        constexpr size_t medium_block_size = 128;
        constexpr size_t large_block_size = 512;
        
        constexpr size_t small_pool_count = 16;
        constexpr size_t medium_pool_count = 8;
        constexpr size_t large_pool_count = 4;
        
        #ifdef EMCORE_PLATFORM_ESP32
            constexpr u32 system_clock_hz = 240000000; // 240MHz
            constexpr size_t stack_size_default = 4096;
        #elif defined(EMCORE_PLATFORM_ARDUINO)
            constexpr u32 system_clock_hz = 16000000; // 16MHz
            constexpr size_t stack_size_default = 1024;
        #else
            constexpr u32 system_clock_hz = 1000000; // 1MHz default
            constexpr size_t stack_size_default = 2048;
        #endif
        
        // Debug configuration
        #ifdef EMCORE_DEBUG
            constexpr bool debug_enabled = true;
            constexpr bool assert_enabled = true;
        #else
            constexpr bool debug_enabled = false;
            constexpr bool assert_enabled = false;
        #endif
        
        // -------- Compile-time sanity checks for YAML/flags interrelations --------
        static_assert(max_tasks >= 1, "EMCORE_MAX_TASKS must be >= 1");
        static_assert(max_events >= 1, "EMCORE_MAX_EVENTS must be >= 1");

        // Messaging
        static_assert(!enable_messaging || (default_mailbox_queue_capacity >= 1),
                      "EMCORE_MSG_QUEUE_CAPACITY must be >= 1 when messaging is enabled");
        static_assert(!enable_messaging || (default_max_topics >= 1),
                      "EMCORE_MSG_MAX_TOPICS must be >= 1 when messaging is enabled");
        static_assert(!enable_messaging || (default_max_subscribers_per_topic >= 1),
                      "EMCORE_MSG_MAX_SUBS_PER_TOPIC must be >= 1 when messaging is enabled");
        static_assert(!enable_messaging || (default_max_subscribers_per_topic <= max_tasks),
                      "EMCORE_MSG_MAX_SUBS_PER_TOPIC must be <= EMCORE_MAX_TASKS");
        static_assert(!enable_messaging || (default_max_topic_queues_per_mailbox >= 1),
                      "EMCORE_MSG_TOPIC_QUEUES_PER_MAILBOX must be >= 1 when messaging is enabled");
        static_assert(default_topic_high_ratio_den != 0, "EMCORE_MSG_TOPIC_HIGH_RATIO_DEN must not be 0");
        static_assert(default_topic_high_ratio_num <= default_topic_high_ratio_den,
                      "EMCORE_MSG_TOPIC_HIGH_RATIO_NUM must be <= DEN");
        static_assert(!enable_messaging || (default_max_topic_queues_per_mailbox <= default_mailbox_queue_capacity),
                      "Per-mailbox topic queues should not exceed total mailbox queue capacity");

        // Protocol
        static_assert(!enable_protocol || (protocol_max_handlers >= 1),
                      "EMCORE_PROTOCOL_MAX_HANDLERS must be >= 1 when protocol is enabled");
        static_assert(!enable_protocol || (protocol_packet_size >= 1),
                      "EMCORE_PROTOCOL_PACKET_SIZE must be >= 1 when protocol is enabled");
        static_assert(!enable_protocol || (protocol_ring_size >= protocol_packet_size),
                      "EMCORE_PROTOCOL_RING_SIZE must be >= EMCORE_PROTOCOL_PACKET_SIZE");

        // Pools
        static_assert(!enable_pools_region || (small_block_size > 0 && medium_block_size > 0 && large_block_size > 0),
                      "Pool block sizes must be > 0 when pools region is enabled");
        // Counts are size_t; implicitly >= 0; keep a sanity upper bound to catch wild YAML/macros
        static_assert(!enable_pools_region || (small_pool_count <= 4096 && medium_pool_count <= 4096 && large_pool_count <= 4096),
                      "Pool block counts unreasonably large; check EMCORE_*_pool_count defines");

        // Events
        static_assert(!enable_events || (max_events >= 1),
                      "EMCORE_MAX_EVENTS must be >= 1 when events are enabled");
}  // namespace emCore::config
