#pragma once

#include <stdexcept>
#include <string_view>

namespace acrp {

enum class StatusCode {
  ok = 0,
  invalid_argument,
  invalid_format,
  unsupported_version,
  io_error,
};

class ParseError final : public std::runtime_error {
 public:
  ParseError(StatusCode code, std::string_view message);

  [[nodiscard]] StatusCode code() const noexcept;

 private:
  StatusCode code_;
};

}  // namespace acrp
