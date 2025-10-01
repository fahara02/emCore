#pragma once

// ETL compatibility header
// This file helps clangd understand the project structure
// When using Arduino or frameworks with STL, we don't need to provide stubs

// For clangd: Always include initializer_list when available
#if !defined(ETL_NO_STL) && (defined(ARDUINO) || defined(__GLIBCXX__) || defined(_LIBCPP_VERSION))
    // STL is available, use it
    #include <initializer_list>
#elif defined(ETL_NO_STL)
    // Pure embedded environment without STL - provide minimal stub
    #include <cstddef>
    namespace std {
        template<typename T>
        class initializer_list {
        private:
            const T* array_;
            size_t len_;
            
        public:
            using value_type = T;
            using size_type = size_t;
            using iterator = const T*;
            using const_iterator = const T*;
            
            constexpr initializer_list() noexcept : array_(nullptr), len_(0) {}
            constexpr size_t size() const noexcept { return len_; }
            constexpr const T* begin() const noexcept { return array_; }
            constexpr const T* end() const noexcept { return array_ + len_; }
        };
    }
#endif
