#pragma once

#include "query/tuple.hpp"
#include <cstddef>
#include <string>

namespace TupleCodec {

std::string serialize(const Tuple& tuple);
Tuple deserialize(const char* data, size_t length);

} // namespace TupleCodec
