#pragma once

#include <memory>
#include <optional>
#include <set>
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

  T const* ptr() const {
    return &t_;
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

template <typename T, auto Dtor, typename Parent, typename AllocationCallbacks = std::nullptr_t, T kNil = T{}>
struct ParentedUniqueHandle : UniqueHandle<ParentedUniqueHandle<T, Dtor, Parent, AllocationCallbacks, kNil>, T> {
  ParentedUniqueHandle(Parent p, T t, AllocationCallbacks ac) : ParentedUniqueHandle{std::tuple{p, t, ac}} {}
  ParentedUniqueHandle(std::tuple<Parent, T, AllocationCallbacks> tup)
      : UniqueHandle<ParentedUniqueHandle<T, Dtor, Parent, AllocationCallbacks, kNil>, T>{std::get<1>(tup)},
        parent_{std::get<0>(tup)},
        callbacks_{std::get<2>(tup)} {}

 protected:
  Parent parent() const {
    return parent_;
  }

 private:
  friend UniqueHandle<ParentedUniqueHandle<T, Dtor, Parent, AllocationCallbacks, kNil>, T>;
  void destroy(T t) {
    Dtor(parent_, t, callbacks_);
  }

  Parent parent_;
  AllocationCallbacks callbacks_;
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

template <template <typename...> typename To> struct ToFunctor {
  template <typename Range> decltype(auto) operator()(Range&& range) {
    using T = std::decay_t<std::remove_cvref_t<std::ranges::range_value_t<Range>>>;
    if constexpr (std::is_same_v<Range, To<T>>) {
      return range;
    } else {
      return To<T>{std::begin(range), std::end(range)};
    }
  }
};
template <template <typename...> typename To, typename Range>
decltype(auto) operator|(Range&& range, ToFunctor<To>&& to) {
  return std::forward<ToFunctor<To>>(to)(std::forward<Range>(range));
}
template <template <typename...> typename To> decltype(auto) to() {
  return ToFunctor<To>{};
}

}  // namespace optalg