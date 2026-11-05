#pragma once
#include <cstddef>
class Print;
class Printable {
public:
  virtual ~Printable() = default;
  virtual size_t printTo(Print &) const = 0;
};
