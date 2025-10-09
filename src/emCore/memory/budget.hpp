#pragma once

// emCore memory budget and compile-time sanity checks
// Header-only, MCU-agnostic, no dynamic allocation, ETL-only.
// Define the following macros in your build to control the global budget and per-subsystem caps:
//   - EMCORE_MEMORY_BUDGET_BYTES          : total bytes available to emCore (required for enforcement)
//   - EMCORE_MAX_TASKS                    : number of tasks (default 16)
//   - EMCORE_MAX_EVENTS                   : event queue length (default 32)
//   - EMCORE_MSG_QUEUE_CAPACITY           : per-mailbox total queue capacity (default 32)
//   - EMCORE_MSG_MAX_TOPICS               : broker topics upper bound (default 8)
//   - EMCORE_MSG_MAX_SUBS_PER_TOPIC       : max subscribers per topic (default 4)
//   - EMCORE_MSG_TOPIC_QUEUES_PER_MAILBOX : per-mailbox topic queues (default 3)
//   - EMCORE_MSG_OVERHEAD_BYTES           : extra constant overhead for broker tables (default 2048)
// Optional fine-tuning knobs (set to 0 to disable):
//   - EMCORE_TASK_MEM_BYTES               : reserved bytes for task subsystem (default 0)
//   - EMCORE_OS_MEM_BYTES                 : reserved bytes for OS glue (default 0)
//   - EMCORE_PROTOCOL_MEM_BYTES           : reserved bytes for protocol (default 0)
//   - EMCORE_DIAGNOSTICS_MEM_BYTES        : reserved bytes for diagnostics (default 0)
//   - EMCORE_EVENT_HANDLER_CAP            : number of event handlers (default EMCORE_MAX_EVENTS)
// Additional sizing knobs used to compute a compile-time minimum for tasks region:
//   - EMCORE_TASK_PER_TCB_BYTES           : approx bytes per task bookkeeping (default 256)
//   - EMCORE_TASK_FIXED_OVERHEAD_BYTES    : fixed bytes for taskmaster bookkeeping (default 512)
//   - EMCORE_TASK_MAILBOX_REF_BYTES       : per-task per-queue ref bytes (default 8)

#include <cstddef>

#include "../event/event.hpp"
#include "../event/event_bus.hpp"
#include "../messaging/message_types.hpp"
#include "../core/config.hpp"

// YAML-generated messaging caps (if any) are already imported by core/config.hpp.

namespace emCore::memory {

// All configuration is sourced from emCore/core/config.hpp.

// -------- Sizes of key message/event types we actually store --------
using message_t = ::emCore::messaging::medium_message; // broker defaults to medium_message
using event_t   = ::emCore::events::Event;
using handler_registration_t = ::emCore::events::handler_registration;

// -------- constexpr wrappers for macro configuration (improves linting) --------
constexpr std::size_t kMaxTasks                  = ::emCore::config::max_tasks;
constexpr std::size_t kMaxEvents                 = ::emCore::config::max_events;
constexpr std::size_t kMsgQueueCapacity          = ::emCore::config::default_mailbox_queue_capacity;
constexpr std::size_t kMsgMaxTopics              = ::emCore::config::default_max_topics;
constexpr std::size_t kMsgMaxSubsPerTopic        = ::emCore::config::default_max_subscribers_per_topic;
constexpr std::size_t kMsgQueuesPerMailbox       = ::emCore::config::default_max_topic_queues_per_mailbox;
constexpr std::size_t kMsgOverheadBytes          = ::emCore::config::msg_overhead_bytes;
constexpr std::size_t kEventHandlerCap           = ::emCore::config::max_event_handlers;
constexpr std::size_t kTaskMemBytes              = ::emCore::config::task_mem_bytes;
constexpr std::size_t kOsMemBytes                = ::emCore::config::os_mem_bytes;
constexpr std::size_t kProtocolMemBytes          = ::emCore::config::protocol_mem_bytes;
constexpr std::size_t kDiagnosticsMemBytes       = ::emCore::config::diagnostics_mem_bytes;

// -------- Conservative upper-bounds for subsystem memory footprints --------
// Messaging broker: For each task mailbox, we bound memory by total per-mailbox
// queue capacity times message size. This safely upper-bounds high+normal shards.
inline constexpr std::size_t per_mailbox_bytes = kMsgQueueCapacity * sizeof(message_t);
// Add a small fixed overhead per per-mailbox topic queue entry for bookkeeping.
inline constexpr std::size_t per_mailbox_topic_overhead = kMsgQueuesPerMailbox * 32U;
inline constexpr std::size_t messaging_mailboxes_bytes = kMaxTasks * (per_mailbox_bytes + per_mailbox_topic_overhead);
// Add global broker tables overhead (topics/subscribers registries, indices, etc.)
inline constexpr std::size_t messaging_global_overhead_bytes = kMsgOverheadBytes;
inline constexpr std::size_t messaging_total_upper =
    (::emCore::config::enable_messaging ? (messaging_mailboxes_bytes + messaging_global_overhead_bytes) : 0U);

// Events: queue + handlers
inline constexpr std::size_t event_queue_bytes   =
    (::emCore::config::enable_events ? (kMaxEvents * sizeof(event_t)) : 0U);
inline constexpr std::size_t event_handlers_bytes =
    (::emCore::config::enable_events ? (kEventHandlerCap * sizeof(handler_registration_t)) : 0U);
inline constexpr std::size_t events_total_upper  = event_queue_bytes + event_handlers_bytes;

// Tasks/OS/Protocol/Diagnostics reserved blocks
// Compute a compile-time minimum for the tasks region without including taskmaster.hpp (avoid cycles).
// MIN = fixed overhead + per-task bookkeeping + lightweight mailbox refs per queue.
inline constexpr std::size_t kTaskPerTcbBytes         = ::emCore::config::task_per_tcb_bytes;
inline constexpr std::size_t kTaskFixedOverheadBytes  = ::emCore::config::task_fixed_overhead_bytes;
inline constexpr std::size_t kTaskMailboxRefBytes     = ::emCore::config::task_mailbox_ref_bytes;
inline constexpr std::size_t kPerTaskBookkeepingBytes = kTaskPerTcbBytes + (kMsgQueuesPerMailbox * kTaskMailboxRefBytes);
inline constexpr std::size_t kTaskMemBytesMin         = kTaskFixedOverheadBytes + (kMaxTasks * kPerTaskBookkeepingBytes);

inline constexpr std::size_t kTaskMemBytesEffective   = (kTaskMemBytes > 0) ? kTaskMemBytes : kTaskMemBytesMin;
inline constexpr std::size_t tasks_total_upper        = (::emCore::config::enable_tasks_region ? kTaskMemBytesEffective : 0U);
static_assert(!::emCore::config::enable_tasks_region || (kTaskMemBytesEffective >= kTaskMemBytesMin),
              "EMCORE_TASK_MEM_BYTES is below the computed minimum for current caps; either raise EMCORE_TASK_MEM_BYTES,"
              " lower EMCORE_MAX_TASKS/MSG caps, or adjust EMCORE_TASK_PER_TCB_BYTES/EMCORE_TASK_FIXED_OVERHEAD_BYTES.");

inline constexpr std::size_t os_total_upper          = (::emCore::config::enable_os_region ? kOsMemBytes : 0U);

// Compute a conservative minimum for protocol region based on config knobs only.
inline constexpr std::size_t kProtoRingBytes      = ::emCore::config::protocol_ring_size;
inline constexpr std::size_t kProtoPacketsBytes   = ::emCore::config::protocol_packet_size * 4U; // staging
inline constexpr std::size_t kProtoHandlersBytes  = ::emCore::config::protocol_max_handlers * 64U; // dispatch tables
inline constexpr std::size_t kProtoFixedOverhead  = 1024U; // parsers/encoders/pipeline bookkeeping
inline constexpr std::size_t kProtocolMemBytesMin = kProtoRingBytes + kProtoPacketsBytes + kProtoHandlersBytes + kProtoFixedOverhead;

inline constexpr std::size_t kProtocolMemBytesEffective = (kProtocolMemBytes > 0) ? kProtocolMemBytes : kProtocolMemBytesMin;
static_assert(!::emCore::config::enable_protocol || (kProtocolMemBytesEffective >= kProtocolMemBytesMin),
              "EMCORE_PROTOCOL_MEM_BYTES is below computed minimum; increase it or lower EMCORE_PROTOCOL_* knobs.");
inline constexpr std::size_t protocol_total_upper    = (::emCore::config::enable_protocol ? kProtocolMemBytesEffective : 0U);

inline constexpr std::size_t diagnostics_total_upper = (::emCore::config::enable_diagnostics ? kDiagnosticsMemBytes : 0U);

// Memory pools (small/medium/large) total bytes
inline constexpr std::size_t pools_small_bytes  = ::emCore::config::small_block_size  * ::emCore::config::small_pool_count;
inline constexpr std::size_t pools_medium_bytes = ::emCore::config::medium_block_size * ::emCore::config::medium_pool_count;
inline constexpr std::size_t pools_large_bytes  = ::emCore::config::large_block_size  * ::emCore::config::large_pool_count;
inline constexpr std::size_t pools_total_upper  = (::emCore::config::enable_pools_region ? (pools_small_bytes + pools_medium_bytes + pools_large_bytes) : 0U);

// Sum all uppers
inline constexpr std::size_t total_required_upper =
    messaging_total_upper + events_total_upper + tasks_total_upper + os_total_upper + protocol_total_upper + diagnostics_total_upper + pools_total_upper;

// -------- Budget enforcement --------
// Require the integrator to provide a global memory budget and always enforce it.
#ifndef EMCORE_MEMORY_BUDGET_BYTES
#  error "EMCORE_MEMORY_BUDGET_BYTES must be defined (total bytes available to emCore). Add -DEMCORE_MEMORY_BUDGET_BYTES=<bytes> to build_flags."
#endif
constexpr std::size_t kBudgetBytes = static_cast<std::size_t>(EMCORE_MEMORY_BUDGET_BYTES);
// Optional: reserve compile-time headroom for non-emCore RAM (framework/RTOS/etc.)
#ifndef EMCORE_NON_EMCORE_RAM_HEADROOM_BYTES
#  define EMCORE_NON_EMCORE_RAM_HEADROOM_BYTES 0
#endif
constexpr std::size_t kHeadroomBytes = static_cast<std::size_t>(EMCORE_NON_EMCORE_RAM_HEADROOM_BYTES);
constexpr std::size_t kEffectiveEmcoreBudget = (kBudgetBytes > kHeadroomBytes) ? (kBudgetBytes - kHeadroomBytes) : 0;

static_assert(total_required_upper <= kEffectiveEmcoreBudget,
              "emCore config exceeds effective compile-time budget (EMCORE_MEMORY_BUDGET_BYTES - EMCORE_NON_EMCORE_RAM_HEADROOM_BYTES): lower caps or raise budget/headroom");

// -------- Optional compile-time banner (preprocessor-only) --------
// Auto-enable banner when user enforces total RAM budget to reduce flag burden.
#if defined(EMCORE_ENFORCE_TOTAL_RAM_BUDGET) && (EMCORE_ENFORCE_TOTAL_RAM_BUDGET)
#  if !defined(EMCORE_PRINT_BUDGET)
#    define EMCORE_PRINT_BUDGET 1
#  endif
#endif
// Or enable explicitly with: -DEMCORE_PRINT_BUDGET=1
#if defined(EMCORE_PRINT_BUDGET) && (EMCORE_PRINT_BUDGET)
#  ifndef EMCORE_PP_STRINGIZE
#    define EMCORE_PP_STRINGIZE_IMPL(x) #x
#    define EMCORE_PP_STRINGIZE(x) EMCORE_PP_STRINGIZE_IMPL(x)
#  endif
// We can only print macro values here (not constexpr numbers). For exact computed
// bytes, inspect the constexprs: emCore::memory::total_required_upper and
// emCore::memory::required_bytes (from memory/layout.hpp).
#  pragma message ("[emCore] ===== Memory Budget (compile-time banner) =====")
#  pragma message ("[emCore] Budget bytes (EMCORE_MEMORY_BUDGET_BYTES): " EMCORE_PP_STRINGIZE(EMCORE_MEMORY_BUDGET_BYTES))
#  pragma message ("[emCore] Non-emCore headroom (EMCORE_NON_EMCORE_RAM_HEADROOM_BYTES): " EMCORE_PP_STRINGIZE(EMCORE_NON_EMCORE_RAM_HEADROOM_BYTES))
#  pragma message ("[emCore] Max tasks (EMCORE_MAX_TASKS): " EMCORE_PP_STRINGIZE(EMCORE_MAX_TASKS))
#  pragma message ("[emCore] Max events (EMCORE_MAX_EVENTS): " EMCORE_PP_STRINGIZE(EMCORE_MAX_EVENTS))
#  pragma message ("[emCore] Msg queue cap (EMCORE_MSG_QUEUE_CAPACITY): " EMCORE_PP_STRINGIZE(EMCORE_MSG_QUEUE_CAPACITY))
#  pragma message ("[emCore] Msg max topics (EMCORE_MSG_MAX_TOPICS): " EMCORE_PP_STRINGIZE(EMCORE_MSG_MAX_TOPICS))
#  pragma message ("[emCore] Msg max subs/topic (EMCORE_MSG_MAX_SUBS_PER_TOPIC): " EMCORE_PP_STRINGIZE(EMCORE_MSG_MAX_SUBS_PER_TOPIC))
#  pragma message ("[emCore] Msg queues/mailbox (EMCORE_MSG_TOPIC_QUEUES_PER_MAILBOX): " EMCORE_PP_STRINGIZE(EMCORE_MSG_TOPIC_QUEUES_PER_MAILBOX))
#  pragma message ("[emCore] Msg overhead bytes (EMCORE_MSG_OVERHEAD_BYTES): " EMCORE_PP_STRINGIZE(EMCORE_MSG_OVERHEAD_BYTES))
#  pragma message ("[emCore] Reserved (TASK/OS/PROTO/DIAG) bytes: " EMCORE_PP_STRINGIZE(EMCORE_TASK_MEM_BYTES) ", " EMCORE_PP_STRINGIZE(EMCORE_OS_MEM_BYTES) ", " EMCORE_PP_STRINGIZE(EMCORE_PROTOCOL_MEM_BYTES) ", " EMCORE_PP_STRINGIZE(EMCORE_DIAGNOSTICS_MEM_BYTES))
#  pragma message ("[emCore] Note: Exact computed totals are constexpr: emCore::memory::total_required_upper and layout::required_bytes. Effective emCore budget = EMCORE_MEMORY_BUDGET_BYTES - EMCORE_NON_EMCORE_RAM_HEADROOM_BYTES.")
#  pragma message ("[emCore] ===============================================")

  /* Duplicate as warnings so toolchains/PIO print them even in non-verbose mode */
#  if defined(__GNUC__) || defined(__clang__)
#    warning "[emCore] Memory Budget banner: EMCORE_MEMORY_BUDGET_BYTES=" EMCORE_PP_STRINGIZE(EMCORE_MEMORY_BUDGET_BYTES) \
             ", HEADROOM=" EMCORE_PP_STRINGIZE(EMCORE_NON_EMCORE_RAM_HEADROOM_BYTES)
#    warning "[emCore] Caps: TASKS=" EMCORE_PP_STRINGIZE(EMCORE_MAX_TASKS) \
             ", EVENTS=" EMCORE_PP_STRINGIZE(EMCORE_MAX_EVENTS) \
             ", QCAP=" EMCORE_PP_STRINGIZE(EMCORE_MSG_QUEUE_CAPACITY)
#  endif
#endif

// Expose a small report for logging/test assertions
struct budget_report {
    std::size_t messaging_bytes;
    std::size_t events_bytes;
    std::size_t tasks_bytes;
    std::size_t os_bytes;
    std::size_t protocol_bytes;
    std::size_t diagnostics_bytes;
    std::size_t pools_bytes;
    std::size_t total_upper;
};

constexpr budget_report report() noexcept {
    return budget_report{ messaging_total_upper, events_total_upper, tasks_total_upper, os_total_upper,
                          protocol_total_upper, diagnostics_total_upper, pools_total_upper, total_required_upper };
}

} // namespace emCore::memory
