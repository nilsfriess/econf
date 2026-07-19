#pragma once

#include <cstdlib>
#include <format>
#include <iostream>
#include <source_location>
#include <string_view>

namespace econf {
namespace log {
enum class levels { error = -1, info = 0, verbose = 1 };

static constexpr auto curr_level = levels::verbose;

template <typename... Args>
void error(std::string_view fmt_msg, Args &&...args) {
  std::cout << std::vformat(std::format("[error] {}\n", fmt_msg),
                            std::make_format_args(args...))
            << std::flush;
}

template <typename... Args>
void warn(std::string_view fmt_msg, Args &&...args) {
  std::cout << std::vformat(std::format("[warn] {}\n", fmt_msg),
                            std::make_format_args(args...))
            << std::flush;
}

template <typename... Args>
void info(std::string_view fmt_msg, Args &&...args) {
  std::cout << std::vformat(std::format("[info] {}\n", fmt_msg),
                            std::make_format_args(args...))
            << std::flush;
}

template <typename... Args>
void verbose(std::string_view fmt_msg, Args &&...args) {
  if (curr_level < levels::verbose)
    return;
  std::cout << std::vformat(std::format("[verbose] {}\n", fmt_msg),
                            std::make_format_args(args...))
            << std::flush;
}
} // namespace log

template <typename... Args>
[[noreturn]] void
todo(std::string_view text,
     std::source_location loc = std::source_location::current()) {
  log::error("{}:{}:{}: TODO: {}", loc.file_name(), loc.line(), loc.column(),
             text);
  std::exit(1);
}
} // namespace econf
