#pragma once

#include <memory>
#include <type_traits>

namespace raii {

template <typename T, auto Ctor, auto Dtor>
struct MoveOnlyHolder final { // must be final to use destroy/reconstruct pattern in operator=
  template <typename... Args>
  MoveOnlyHolder(Args&&... args)
    : t_{Ctor(std::forward<Args>(args)...)} { }

  MoveOnlyHolder(MoveOnlyHolder const&) = delete;
  MoveOnlyHolder& operator=(MoveOnlyHolder const&) = delete;

  MoveOnlyHolder(MoveOnlyHolder&& other) noexcept
    : t_{ std::exchange(other.t_, 0) } {
  }

  MoveOnlyHolder& operator=(MoveOnlyHolder&& other) {
    if (this != &other) {
      std::destroy_at(this);
      std::construct_at(this, std::move(other));
    }
    return *this;
  }

  ~MoveOnlyHolder() noexcept {
    if (t_ != kDefault) {
      Dtor(t_);
    }
  }

  operator T() const {
    return t_;
  }

private:
  static constexpr T kDefault{};
  T t_{};
};

}