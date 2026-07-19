#pragma once

#include <format>
#include <stdexcept>
#include <string>
#include <utility>

namespace econf {
class Error : public std::runtime_error {
public:
  explicit Error(std::string msg) : std::runtime_error(std::move(msg)) {}
};

template <typename... Args>
[[noreturn]] void fail(std::format_string<Args...> fmt, Args &&...args) {
  throw Error(std::format(fmt, std::forward<Args>(args)...));
}
} // namespace econf
