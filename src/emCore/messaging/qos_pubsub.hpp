#pragma once

#include <cstddef>

#include "../core/types.hpp"
#include "../core/config.hpp"
#include "../os/time.hpp"
#include "message_types.hpp"
#include "message_broker.hpp" // for Ibroker
#include "../error/result.hpp"
#include <etl/map.h>

namespace emCore::messaging {

// QoS Publisher: ACK-based delivery with retransmission and ordering support
template <typename MsgType,
          size_t PendingLimit = config::default_qos_pending_limit>
class qos_publisher {
public:
    qos_publisher(broker_uptr<MsgType>& broker, task_id_t from_task_id, u16 ack_topic_id) noexcept
        : broker_(*broker), from_task_id_(from_task_id), ack_topic_id_(ack_topic_id) {}

    // New: allow constructing with a non-owning broker reference (no unique_ptr required)
    qos_publisher(Ibroker<MsgType>& broker, task_id_t from_task_id, u16 ack_topic_id) noexcept
        : broker_(broker), from_task_id_(from_task_id), ack_topic_id_(ack_topic_id) {}

    result<void, error_code> publish(u16 topic_id, MsgType& msg) noexcept {
        msg.header.flags = static_cast<u8>(static_cast<message_flags>(msg.header.flags) | message_flags::requires_ack);
        if (msg.header.timestamp == 0) { msg.header.timestamp = os::time_us(); }
        if (msg.header.sequence_number == 0) { msg.header.sequence_number = next_seq_(); }
        msg.header.type = topic_id;

        if (pending_.full()) {
            return result<void, error_code>(error_code::out_of_memory);
        }
        pending_entry new_entry{};
        new_entry.msg = msg; // copy; zero-copy envelopes bump refcount
        new_entry.last_send = msg.header.timestamp;
        new_entry.attempts = 1;
        auto insert_result = pending_.insert(typename pending_map::value_type(msg.header.sequence_number, new_entry));
        if (!insert_result.second) {
            return result<void, error_code>(error_code::out_of_memory);
        }
        return broker_.publish(topic_id, msg, from_task_id_);
    }

    void pump_retransmit() noexcept {
        const timestamp_t now = os::time_us();
        for (auto iter = pending_.begin(); iter != pending_.end(); ++iter) {
            pending_entry& entry_ref = iter->second;
            if ((now - entry_ref.last_send) >= static_cast<timestamp_t>(config::default_ack_timeout_us)) {
                entry_ref.last_send = now;
                ++entry_ref.attempts;
                (void)broker_.publish(entry_ref.msg.header.type, entry_ref.msg, from_task_id_);
            }
        }
    }

    void on_ack(const message_ack& ack) noexcept {
        auto iter = pending_.find(ack.sequence_number);
        if (iter != pending_.end()) { pending_.erase(iter); }
        (void)ack;
    }

    [[nodiscard]] size_t pending_count() const noexcept { return pending_.size(); }

    bool try_handle_ack_message(const small_message& msg) noexcept {
        if (msg.header.type != ack_topic_id_) { return false; }
        if (msg.header.payload_size != sizeof(message_ack)) { return false; }
        message_ack ack{};
        const u8* src_ptr = &msg.payload[0];
        u8* dst_ptr = reinterpret_cast<u8*>(&ack);
        for (size_t i = 0; i < sizeof(message_ack); ++i) { *dst_ptr++ = *src_ptr++; }
        on_ack(ack);
        return true;
    }

private:
    struct pending_entry { MsgType msg; timestamp_t last_send; u16 attempts; };
    using pending_map = etl::map<u16, pending_entry, PendingLimit>;
    u16 next_seq_() noexcept { return static_cast<u16>(local_seq_++); }

    Ibroker<MsgType>& broker_;
    task_id_t from_task_id_;
    u16 ack_topic_id_;
    pending_map pending_{};
    u32 local_seq_{1};
};

// QoS Subscriber: sends ACKs and enforces per-(sender,topic) monotonic ordering
template <typename MsgType,
          size_t TrackLimit = 32>
class qos_subscriber {
public:
    qos_subscriber(broker_uptr<MsgType>& broker, task_id_t self_task_id, u16 ack_topic_id) noexcept
        : broker_(*broker), self_task_id_(self_task_id), ack_topic_id_(ack_topic_id) {}

    // New: allow constructing with a non-owning broker reference (no unique_ptr required)
    qos_subscriber(Ibroker<MsgType>& broker, task_id_t self_task_id, u16 ack_topic_id) noexcept
        : broker_(broker), self_task_id_(self_task_id), ack_topic_id_(ack_topic_id) {}

    result<MsgType, error_code> receive(timeout_ms_t timeout) noexcept {
        auto res = broker_.receive(self_task_id_, timeout);
        if (!res.is_ok()) { return res; }
        MsgType msg = res.value();

        const u32 key = (static_cast<u32>(msg.header.sender_id) << 16) | static_cast<u32>(msg.header.type);
        auto iter = last_seq_.find(key);
        const u16 seq = msg.header.sequence_number;
        if (iter != last_seq_.end()) {
            if (seq == iter->second) { send_ack_(seq, msg.header.sender_id, true); return result<MsgType, error_code>(error_code::not_found); }
            if (static_cast<i32>(seq) - static_cast<i32>(iter->second) <= 0) { send_ack_(seq, msg.header.sender_id, true); return result<MsgType, error_code>(error_code::not_found); }
            iter->second = seq;
        } else {
            if (last_seq_.size() < last_seq_.capacity()) { last_seq_.insert(typename seq_map::value_type(key, seq)); }
        }

        if (has_flag(static_cast<message_flags>(msg.header.flags), message_flags::requires_ack)) { send_ack_(seq, msg.header.sender_id, true); }
        return result<MsgType, error_code>(msg);
    }

private:
    using seq_map = etl::map<u32, u16, TrackLimit>;

    void send_ack_(u16 seq, u16 to_sender, bool success) noexcept {
        message_ack ack{seq, to_sender, success, 0};
        small_message ack_msg{};
        ack_msg.header.type = ack_topic_id_;
        ack_msg.header.sender_id = self_task_id_.value();
        ack_msg.header.receiver_id = to_sender;
        ack_msg.header.payload_size = sizeof(ack);
        ack_msg.header.timestamp = os::time_us();
        if (sizeof(ack) <= sizeof(ack_msg.payload)) {
            const u8* src = reinterpret_cast<const u8*>(&ack);
            for (size_t i = 0; i < sizeof(ack); ++i) { ack_msg.payload[i] = src[i]; }
            (void)broker_.publish(ack_msg.header.type, ack_msg, self_task_id_);
        }
    }

    Ibroker<MsgType>& broker_;
    task_id_t self_task_id_;
    u16 ack_topic_id_;
    seq_map last_seq_{};
};

} // namespace emCore::messaging
