#pragma once

// ETL compatibility header
// This file helps clangd understand the project structure
// When using Arduino or frameworks with STL, we don't need to provide stubs

// Prefer the real header if available; only define a stub when it's truly absent.
#if defined(__has_include)
#  if __has_include(<initializer_list>)
#    include <initializer_list>
#  elif defined(ETL_NO_STL)
#    include <cstddef>
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
#  endif
#else
#  if defined(ETL_NO_STL)
#    include <cstddef>
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
#  else
#    include <initializer_list>
#  endif
#endif
