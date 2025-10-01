#pragma once

#include <etl/type_traits.h>

namespace emCore::core {

/**
 * @brief Generic strong type wrapper for type safety
 * 
 * Creates distinct types from primitive types to prevent parameter confusion
 * and improve API clarity. Zero-cost abstraction for embedded systems.
 * 
 * @tparam T The underlying type to wrap
 * @tparam Tag A unique tag type to distinguish different strong types
 */
template<typename T, typename Tag>
class strong_type {
public:
    using value_type = T;
    using tag_type = Tag;

private:
    T value_;

public:
    /**
     * @brief Default constructor
     */
    constexpr strong_type() noexcept : value_{} {}

    /**
     * @brief Explicit constructor from underlying type
     */
    explicit constexpr strong_type(const T& value) noexcept : value_(value) {}

    /**
     * @brief Get the underlying value (const)
     */
    constexpr const T& value() const noexcept { return value_; }

    /**
     * @brief Get the underlying value (mutable)
     */
    constexpr T& value() noexcept { return value_; }

    // Comparison operators
    constexpr bool operator==(const strong_type& other) const noexcept {
        return value_ == other.value_;
    }

    constexpr bool operator!=(const strong_type& other) const noexcept {
        return value_ != other.value_;
    }

    constexpr bool operator<(const strong_type& other) const noexcept {
        return value_ < other.value_;
    }

    constexpr bool operator<=(const strong_type& other) const noexcept {
        return value_ <= other.value_;
    }

    constexpr bool operator>(const strong_type& other) const noexcept {
        return value_ > other.value_;
    }

    constexpr bool operator>=(const strong_type& other) const noexcept {
        return value_ >= other.value_;
    }

    // Arithmetic operators (for numeric types)
    template<typename U = T>
    constexpr typename etl::enable_if<etl::is_arithmetic<U>::value, strong_type>::type
    operator+(const strong_type& other) const noexcept {
        return strong_type(value_ + other.value_);
    }

    template<typename U = T>
    constexpr typename etl::enable_if<etl::is_arithmetic<U>::value, strong_type>::type
    operator-(const strong_type& other) const noexcept {
        return strong_type(value_ - other.value_);
    }

    template<typename U = T>
    constexpr typename etl::enable_if<etl::is_arithmetic<U>::value, strong_type>::type
    operator*(const strong_type& other) const noexcept {
        return strong_type(value_ * other.value_);
    }

    template<typename U = T>
    constexpr typename etl::enable_if<etl::is_arithmetic<U>::value, strong_type>::type
    operator/(const strong_type& other) const noexcept {
        return strong_type(value_ / other.value_);
    }

    // Assignment operators
    template<typename U = T>
    constexpr typename etl::enable_if<etl::is_arithmetic<U>::value, strong_type&>::type
    operator+=(const strong_type& other) noexcept {
        value_ += other.value_;
        return *this;
    }

    template<typename U = T>
    constexpr typename etl::enable_if<etl::is_arithmetic<U>::value, strong_type&>::type
    operator-=(const strong_type& other) noexcept {
        value_ -= other.value_;
        return *this;
    }

    template<typename U = T>
    constexpr typename etl::enable_if<etl::is_arithmetic<U>::value, strong_type&>::type
    operator*=(const strong_type& other) noexcept {
        value_ *= other.value_;
        return *this;
    }

    template<typename U = T>
    constexpr typename etl::enable_if<etl::is_arithmetic<U>::value, strong_type&>::type
    operator/=(const strong_type& other) noexcept {
        value_ /= other.value_;
        return *this;
    }

    // Increment/decrement (for numeric types)
    template<typename U = T>
    constexpr typename etl::enable_if<etl::is_arithmetic<U>::value, strong_type&>::type
    operator++() noexcept {
        ++value_;
        return *this;
    }

    template<typename U = T>
    constexpr typename etl::enable_if<etl::is_arithmetic<U>::value, strong_type>::type
    operator++(int) noexcept {
        strong_type temp(*this);
        ++value_;
        return temp;
    }

    template<typename U = T>
    constexpr typename etl::enable_if<etl::is_arithmetic<U>::value, strong_type&>::type
    operator--() noexcept {
        --value_;
        return *this;
    }

    template<typename U = T>
    constexpr typename etl::enable_if<etl::is_arithmetic<U>::value, strong_type>::type
    operator--(int) noexcept {
        strong_type temp(*this);
        --value_;
        return temp;
    }
};

/**
 * @brief Strong type generator class - The C++ way!
 * 
 * This class template generates unique strong types at compile time
 * without using macros. Each instantiation creates a distinct type.
 * 
 * @tparam T The underlying type to wrap
 * @tparam UniqueId A unique identifier (can be any type or value)
 * 
 * Example usage:
 * @code
 * // Define strong types using template specialization
 * using timeout_ms = strong_type_generator<duration_t, 1001>::type;
 * using cpu_core_id = strong_type_generator<u8, 2001>::type;
 * using task_priority = strong_type_generator<u8, 2002>::type;
 * 
 * // Usage
 * void set_timeout(timeout_ms timeout) { }
 * set_timeout(timeout_ms(5000)); // Type-safe!
 * @endcode
 */
/**
 * @brief Strong type generator with automatic unique IDs
 * 
 * Uses __COUNTER__ macro to generate unique IDs automatically.
 * No more manual ID management - each instantiation gets a unique ID!
 * 
 * Example:
 * @code
 * using timeout_ms = strong_type_generator<duration_t>::type;
 * using cpu_core_id = strong_type_generator<u8>::type;
 * using buffer_size = strong_type_generator<size_t>::type;
 * @endcode
 */
template<typename T, int UniqueId = __COUNTER__>
struct strong_type_generator {
    /**
     * @brief Unique tag type generated automatically
     */
    struct unique_tag {
        static constexpr int unique_id = UniqueId;
    };
    
    /**
     * @brief The generated strong type
     */
    using type = strong_type<T, unique_tag>;
    
    /**
     * @brief Factory function to create instances
     */
    static constexpr type create(const T& value) noexcept {
        return type(value);
    }
    
    /**
     * @brief Convenience operator to create instances
     */
    constexpr type operator()(const T& value) const noexcept {
        return type(value);
    }
};

/**
 * @brief Helper function to create strong type instances
 * 
 * @tparam StrongType The strong type to create
 * @param value The underlying value
 * @return StrongType instance
 * 
 * Example:
 * @code
 * auto timeout = make_strong<timeout_ms>(5000);
 * @endcode
 */
template<typename StrongType, typename T>
constexpr StrongType make_strong(const T& value) noexcept {
    return StrongType(value);
}

} // namespace emCore::core
