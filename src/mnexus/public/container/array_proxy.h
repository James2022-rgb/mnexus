#pragma once

// c++ headers ------------------------------------------
#include <array>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <initializer_list>
#include <vector>

namespace mnexus::container {

template<class T>
class ArrayProxy {
public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = uint32_t;
  using difference_type = std::ptrdiff_t;
  using pointer = element_type*;
  using const_pointer = element_type const*;
  using reference = element_type&;
  using const_reference = element_type const&;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  constexpr ArrayProxy() : count_(0), ptr_(nullptr) {}
  constexpr ArrayProxy(ArrayProxy const& rhs) : count_(rhs.count_), ptr_(rhs.ptr_) {}
  constexpr ArrayProxy(ArrayProxy&& rhs) noexcept :
      ArrayProxy() {
    swap(*this, rhs);
  }

  constexpr ArrayProxy& operator=(ArrayProxy const& rhs) {
    ptr_   = rhs.ptr_;
    count_ = rhs.count_;

    return *this;
  }
  constexpr ArrayProxy& operator=(ArrayProxy&& rhs) noexcept {
    operator=(rhs);

    rhs.ptr_   = nullptr;
    rhs.count_ = 0;

    return *this;
  }

  constexpr ArrayProxy(std::nullptr_t) : count_(0), ptr_(nullptr) {}

  ArrayProxy(element_type& ptr) : count_(1), ptr_(&ptr) {}

  ArrayProxy(T* ptr, uint32_t count) : count_(count), ptr_(ptr) {}
  template<class U = T, std::enable_if_t<std::is_const_v<U>, int> = 0>
  ArrayProxy(T const* ptr, uint32_t count) : count_(count), ptr_(const_cast<T*>(ptr)) {}

  template<class U = T, std::enable_if_t<std::is_const_v<U>, int> = 0>
  ArrayProxy(std::initializer_list<T>& data) : count_(uint32_t(data.size())), ptr_(data.begin()) {}
  template<class U = T,
           std::enable_if_t<std::is_const_v<U>, int> = 0>
  ArrayProxy(std::initializer_list<std::remove_const_t<T>>& data) : count_(uint32_t(data.size())), ptr_(data.begin()) {}
  template<class U = T, std::enable_if_t<std::is_const_v<U>, int> = 0>
  ArrayProxy(std::initializer_list<T> const&data) : count_(uint32_t(data.size())), ptr_(data.begin()) {}
  template<class U = T,
           std::enable_if_t<std::is_const_v<U>, int> = 0>
  ArrayProxy(std::initializer_list<std::remove_const_t<T>> const data) : count_(uint32_t(data.size())), ptr_(data.begin()) {}

  // std::array
  template<size_t N>
  ArrayProxy(std::array<std::remove_const_t<T>, N>& data) : count_(uint32_t(N)), ptr_(data.data()) {}
  template<size_t N>
  ArrayProxy(std::array<std::remove_const_t<T>, N> const& data) : count_(uint32_t(N)), ptr_(const_cast<T*>(data.data())) {}

  // std::vector
  template<class Allocator = std::allocator<std::remove_const_t<T>>,
           class U = T,
           std::enable_if_t<!std::is_const_v<U>, int> = 0>
  ArrayProxy(std::vector<T, Allocator>& data) : count_(uint32_t(data.size())), ptr_(data.data()) {}
  template<class Allocator = std::allocator<std::remove_const_t<T>>,
           class U = T,
           std::enable_if_t<std::is_const_v<U>, int> = 0>
  ArrayProxy(std::vector<std::remove_const_t<T>, Allocator>& data) : count_(uint32_t(data.size())), ptr_(data.data()) {}
  template<class Allocator = std::allocator<std::remove_const_t<T>>,
           class U = T,
           std::enable_if_t<!std::is_const_v<U>, int> = 0>
  ArrayProxy(std::vector<T, Allocator> const& data) : count_(uint32_t(data.size())), ptr_(data.data()) {}
  template<class Allocator = std::allocator<std::remove_const_t<T>>,
           class U = T,
           std::enable_if_t<std::is_const_v<U>, int> = 0>
  ArrayProxy(std::vector<std::remove_const_t<T>, Allocator> const& data) : count_(uint32_t(data.size())), ptr_(data.data()) {}

  template<class Allocator = std::allocator<std::remove_const_t<T>>>
  operator std::vector<std::remove_const_t<T>, Allocator>() const { return { begin(), end() }; }

  const_iterator begin() const { return ptr_; }
  const_iterator end() const { return ptr_ + count_; }
  iterator begin() { return ptr_; }
  iterator end() { return ptr_ + count_; }
  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }

  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
  const_reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }
  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
  const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

  const_reference front() const { return *ptr_; }
  const_reference back() const { return *(ptr_ + count_ - 1); }
  reference front() { return *ptr_; }
  reference back() { return *(ptr_ + count_ - 1); }

  bool empty() const { return count_ == 0; }

  size_type size() const { return count_; }

  pointer data() const { return ptr_; }

  reference operator[](uint32_t index) const { return *(ptr_ + index); }

  ArrayProxy subset(uint32_t offset, uint32_t count) const {
    return ArrayProxy(ptr_ + offset, count);
  }
  ArrayProxy subset(uint32_t offset) const {
    return subset(offset, count_ - offset);
  }

  ArrayProxy first(uint32_t count) const {
    return subset(0, count);
  }
  ArrayProxy last(uint32_t count) const {
    return subset(count_ - count, count);
  }

  friend void swap(ArrayProxy& lhs, ArrayProxy& rhs) noexcept {
    using std::swap;

    swap(lhs.ptr_, rhs.ptr_);
    swap(lhs.count_, rhs.count_);
  }
private:
  uint32_t count_ = 0;
  element_type* ptr_ = nullptr;
};

} // namespace mnexus::container
