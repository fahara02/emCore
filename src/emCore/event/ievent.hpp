#pragma once

// Universal event tag interface for emCore
// - Header-only, ETL-only, no RTTI, no dynamic allocation
// - Used to mark event payload types that participate in the event system

namespace emCore::events {

struct IEvent {
    // Tag type: no virtuals to avoid RTTI and vtables on MCUs
    // Extend or embed in user-defined event payloads if needed
protected:
    constexpr IEvent() = default;
};

} // namespace emCore::evt
