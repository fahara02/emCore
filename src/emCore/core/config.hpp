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
#define EMCORE_ENABLE_EVENT_LOGS 1
#endif

// If available, include generated messaging configuration macros produced from YAML.
// This allows YAML to override the default capacities at compile time.
#if defined(__has_include)
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

        #ifdef EMCORE_MSG_MAX_TOPICS
        constexpr size_t default_max_topics = EMCORE_MSG_MAX_TOPICS;
        #else
        constexpr size_t default_max_topics = 12; // tightened to reduce RAM
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
        
}  // namespace emCore::config
