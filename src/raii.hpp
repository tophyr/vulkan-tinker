#pragma once

#include <memory>
#include <type_traits>

namespace raii {

template <typename T, auto Ctor, auto Dtor>
struct MoveOnlyHolder final { // must be final to use destroy/reconstruct pattern in operator=
  template <typename... Args>
  Holder(Args&&... args)
    : t_{Ctor(std::forward<Args>(args)...)} { }

  Holder(Holder const&) = delete;
  Holder& operator=(Holder const&) = delete;

  Holder(Holder&& other) noexcept
    : t_{ std::exchange(other.t_, 0) } {
  }

  Holder& operator=(Holder&& other) {
    if (this != &other) {
      std::destroy_at(this);
      std::construct_at(this, std::move(other));
    }
    return *this;
  }

  ~Holder() noexcept {
    if (t_ != T{}) {
      Dtor(t_);
    }
  }

  operator T() const {
    return t_;
  }

private:
  T t_{};
};

}