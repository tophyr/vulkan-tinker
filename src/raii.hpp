#pragma once

#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

namespace raii {

template <typename CRTP, typename T, T kNil = T{}> struct UniqueHandle {
  constexpr UniqueHandle() = default;
  explicit UniqueHandle(T t) : t_{t} {}

  UniqueHandle(UniqueHandle const&) = delete;
  UniqueHandle& operator=(UniqueHandle const&) = delete;

  UniqueHandle(UniqueHandle&& other) noexcept : t_{std::exchange(other.t_, kNil)} {}

  UniqueHandle& operator=(UniqueHandle&& other) {
    if (this != &other) {
      maybeDestroy();
      t_ = std::exchange(other.t_, kNil);
    }
    return *this;
  }

  ~UniqueHandle() {
    maybeDestroy();
  }

  operator T() const {
    return t_;
  }

  explicit operator bool() const {
    return *this != kNil;
  }

  void reset() {
    maybeDestroy();
    t_ = kNil;
  }

 private:
  void maybeDestroy() {
    if (t_) {
      static_cast<CRTP*>(this)->destroy(t_);
    }
  }

  T t_{kNil};
};

template <typename Elem, auto Func, typename... Args> std::vector<Elem> VecFetcher(Args&&... args) {
  uint32_t count{};
  Func(args..., &count, nullptr);
  std::vector<Elem> elems{count};
  Func(args..., &count, elems.data());
  return elems;
}

template <typename T, auto Func, typename... Args> T Fetcher(Args&&... args) {
  T t{};
  Func(std::forward<Args>(args)..., &t);
  return t;
}

}  // namespace raii