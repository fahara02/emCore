#pragma once

#include <cstdint>

#include "../core/etl_compat.hpp"
#include <etl/optional.h>
#include <etl/utility.h>

namespace emCore {

// Common error codes
enum class error_code : int8_t {
    success = 0,
    invalid_parameter = -1,
    out_of_memory = -2,
    timeout = -3,
    not_found = -4,
    already_exists = -5,
    not_initialized = -6,
    hardware_error = -7
};

// Result type for error handling without exceptions
template<typename T, typename E = error_code>
class result {
private:
    etl::optional<T> value_;
    etl::optional<E> error_;

public:
    explicit result(const T& value) noexcept : value_(value) {}
    
    explicit result(T&& value) noexcept : value_(etl::forward<T>(value)) {}
    
    explicit result(const E& error) noexcept : error_(error) {}

    [[nodiscard]] bool is_ok() const noexcept { return value_.has_value(); }
    
    [[nodiscard]] bool is_error() const noexcept { return error_.has_value(); }

    const T& value() const noexcept { return value_.value(); }
    
    T& value() noexcept { return value_.value(); }

    const E& error() const noexcept { return error_.value(); }
    
    E& error() noexcept { return error_.value(); }

    const T& value_or(const T& default_value) const noexcept {
        return is_ok() ? value() : default_value;
    }
};

// Specialization for void result type
template<typename E>
class result<void, E> {
private:
    etl::optional<E> error_; //error

public:
    result() noexcept : error_() {}
    
    explicit result(const E& error) noexcept : error_(error) {}

    [[nodiscard]] bool is_ok() const noexcept { return !error_.has_value(); }
    
    [[nodiscard]] bool is_error() const noexcept { return error_.has_value(); }

    const E& error() const noexcept { return error_.value(); }
    
    E& error() noexcept { return error_.value(); }
};

// Helper function for creating successful void results
inline result<void, error_code> ok() noexcept {
    return {};
}

// Helper function for creating successful results with a value
template<typename T>
inline result<T, error_code> ok(const T& value) noexcept {
    return result<T, error_code>(value);
}

// Helper function for creating successful results with a moved value
template<typename T>
inline result<T, error_code> ok(T&& value) noexcept {
    return result<T, error_code>(etl::forward<T>(value));
}

}  // namespace emCore
