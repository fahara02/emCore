#pragma once

#include <cstddef>

#include "../core/types.hpp"
#include "../platform/platform.hpp"
#include "message_types.hpp"
#include "message_broker.hpp" // for Ibroker

#include <etl/map.h>

namespace emCore::messaging {

// Distributed state machine: proposal -> majority ACK -> commit
// Uses small_message for coordination payloads.
template <typename StateT,
          u16 ProposeTopicId, u16 AckTopicId, u16 CommitTopicId,
          size_t MaxPeers,
          size_t MaxOutstanding = 4>
class distributed_state {
public:
    static_assert(sizeof(StateT) <= small_payload_size - 6, "StateT too large for small_message payload");

    distributed_state(broker_uptr<small_message>& broker, task_id_t self_task_id, const StateT& initial) noexcept
        : broker_(*broker), self_task_id_(self_task_id), state_(initial) {}

    // Start a new proposal; returns sequence (>0) or 0 if queue full
    u16 propose(const StateT& new_state) noexcept {
        if (pending_.size() >= pending_.capacity()) { return 0; }
        const u16 seq = static_cast<u16>(local_seq_++);
        pending_info info{}; info.state = new_state; info.acks = 1; (void)pending_.insert(typename pending_map::value_type(seq, info));
        small_message msg{}; msg.header.type = ProposeTopicId; msg.header.sender_id = self_task_id_.value(); msg.header.receiver_id = 0xFFFF; msg.header.sequence_number = seq;
        msg.header.payload_size = encode_proposal_(&msg.payload[0], seq, self_task_id_.value(), new_state); msg.header.timestamp = platform::get_system_time_us();
        (void)broker_.publish(ProposeTopicId, msg, self_task_id_);
        return seq;
    }

    // Process incoming messages for coordination. Guard decides acceptance.
    template <typename GuardFn>
    void process_message(const small_message& msg, const GuardFn& guard) noexcept {
        if (msg.header.type == ProposeTopicId) { on_propose_(msg, guard); }
        else if (msg.header.type == AckTopicId) { on_ack_(msg); }
        else if (msg.header.type == CommitTopicId) { on_commit_(msg); }
    }

    [[nodiscard]] StateT current() const noexcept { return state_; }

private:
    struct pending_info { StateT state; u16 acks; };
    using pending_map = etl::map<u16, pending_info, MaxOutstanding>;

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    static u16 encode_proposal_(u8* dst, u16 seq, u16 from, const StateT& state_obj) noexcept {
        u8* ptr = dst; *ptr++ = static_cast<u8>(seq & 0xFF); *ptr++ = static_cast<u8>((seq >> 8) & 0xFF);
        *ptr++ = static_cast<u8>(from & 0xFF); *ptr++ = static_cast<u8>((from >> 8) & 0xFF);
        const u8* sptr = reinterpret_cast<const u8*>(&state_obj);
        for (size_t i = 0; i < sizeof(StateT); ++i) { *ptr++ = *sptr++; }
        return static_cast<u16>(ptr - dst);
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    static bool decode_proposal_(const small_message& msg, u16& seq, u16& from, StateT& out_state) noexcept {
        if (msg.header.payload_size < (sizeof(StateT) + 4)) { return false; }
        const u8* payload_ptr = &msg.payload[0];
        seq = static_cast<u16>(payload_ptr[0] | (static_cast<u16>(payload_ptr[1]) << 8));
        from = static_cast<u16>(payload_ptr[2] | (static_cast<u16>(payload_ptr[3]) << 8));
        u8* dst_ptr = reinterpret_cast<u8*>(&out_state);
        for (size_t i = 0; i < sizeof(StateT); ++i) { dst_ptr[i] = payload_ptr[4 + i]; }
        return true;
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    static u16 encode_ack_(u8* dst, u16 seq, u16 from, bool accept) noexcept {
        u8* ptr = dst; *ptr++ = static_cast<u8>(seq & 0xFF); *ptr++ = static_cast<u8>((seq >> 8) & 0xFF);
        *ptr++ = static_cast<u8>(from & 0xFF); *ptr++ = static_cast<u8>((from >> 8) & 0xFF); *ptr++ = static_cast<u8>(accept ? 1 : 0);
        return static_cast<u16>(ptr - dst);
    }

    static bool decode_ack_(const small_message& msg, u16& seq, u16& from, bool& accept) noexcept {
        if (msg.header.payload_size < 5) { return false; }
        const u8* payload_ptr = reinterpret_cast<const u8*>(&msg.payload[0]);
        seq = static_cast<u16>(payload_ptr[0] | (static_cast<u16>(payload_ptr[1]) << 8));
        from = static_cast<u16>(payload_ptr[2] | (static_cast<u16>(payload_ptr[3]) << 8)); accept = (payload_ptr[4] != 0); return true;
    }

    static u16 encode_commit_(u8* dst, u16 seq, const StateT& state_obj) noexcept {
        u8* ptr = dst; *ptr++ = static_cast<u8>(seq & 0xFF); *ptr++ = static_cast<u8>((seq >> 8) & 0xFF);
        const u8* sptr = reinterpret_cast<const u8*>(&state_obj); for (size_t i = 0; i < sizeof(StateT); ++i) { *ptr++ = *sptr++; }
        return static_cast<u16>(ptr - dst);
    }

    static bool decode_commit_(const small_message& msg, u16& seq, StateT& out_state) noexcept {
        if (msg.header.payload_size < (sizeof(StateT) + 2)) { return false; }
        const u8* payload_ptr = reinterpret_cast<const u8*>(&msg.payload[0]);
        seq = static_cast<u16>(payload_ptr[0] | (static_cast<u16>(payload_ptr[1]) << 8));
        u8* dst_ptr = reinterpret_cast<u8*>(&out_state); for (size_t i = 0; i < sizeof(StateT); ++i) { dst_ptr[i] = payload_ptr[2 + i]; }
        return true;
    }

    template <typename GuardFn>
    void on_propose_(const small_message& msg, const GuardFn& guard) noexcept {
        u16 seq = 0; u16 from = 0; StateT proposed{};
        if (!decode_proposal_(msg, seq, from, proposed)) { return; }
        if (from == self_task_id_.value()) { return; }
        const bool accept = guard(state_, proposed);
        if (accept) {
            small_message ack{}; ack.header.type = AckTopicId; ack.header.sender_id = self_task_id_.value(); ack.header.receiver_id = from; ack.header.sequence_number = seq;
            ack.header.payload_size = encode_ack_(&ack.payload[0], seq, self_task_id_.value(), true); ack.header.timestamp = platform::get_system_time_us();
            (void)broker_.publish(AckTopicId, ack, self_task_id_);
        }
    }

    void on_ack_(const small_message& msg) noexcept {
        u16 seq = 0; u16 from = 0; bool accept = false; (void)from;
        if (!decode_ack_(msg, seq, from, accept)) { return; }
        if (!accept) { return; }
        auto iter = pending_.find(seq);
        if (iter == pending_.end()) { return; }
        pending_info& info = iter->second; ++info.acks; const u16 majority = static_cast<u16>((MaxPeers / 2) + 1);
        if (info.acks >= majority) {
            state_ = info.state;
            small_message commit{}; commit.header.type = CommitTopicId; commit.header.sender_id = self_task_id_.value(); commit.header.receiver_id = 0xFFFF; commit.header.sequence_number = seq;
            commit.header.payload_size = encode_commit_(&commit.payload[0], seq, state_); commit.header.timestamp = platform::get_system_time_us();
            (void)broker_.publish(CommitTopicId, commit, self_task_id_);
            pending_.erase(iter);
        }
    }

    void on_commit_(const small_message& msg) noexcept {
        u16 seq = 0; (void)seq; StateT committed{}; if (!decode_commit_(msg, seq, committed)) { return; } state_ = committed;
    }

    Ibroker<small_message>& broker_;
    task_id_t self_task_id_;
    StateT state_;
    pending_map pending_{};
    u32 local_seq_{1};
};

} // namespace emCore::messaging
