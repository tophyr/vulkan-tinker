#pragma once

#include <memory>
#include <optional>
#include <set>
#include <unordered_set>
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

namespace optalg {

template <typename Collection, typename Op>
std::optional<std::remove_cvref_t<decltype(*std::declval<Collection>().begin())>> find_if(
    Collection const& collection, Op&& op) {
  auto it = std::find_if(collection.cbegin(), collection.cend(), std::forward<Op>(op));
  if (it != collection.cend()) {
    return *it;
  }
  return std::nullopt;
}

template <typename Collection>
auto unique_vector(Collection const& collection) {
  using T = std::remove_cvref_t<decltype(*std::declval<Collection>().begin())>;
  auto const& uniqued = [&] {
    if constexpr (std::is_same_v<Collection, std::set<T>> || std::is_same_v<Collection, std::unordered_set<T>>) {
      return collection;
    } else {
      return std::set<T>{collection.cbegin(), collection.cend()};
    }
  }();
  return std::vector<T>{uniqued.cbegin(), uniqued.cend()};
}

}  // namespace optalg