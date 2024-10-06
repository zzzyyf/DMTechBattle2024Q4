#pragma once
#include <memory>

namespace dm {

template <class T>
using Ptr = std::shared_ptr<T>;

constexpr static uint32_t request_min_size = 10;
constexpr static uint32_t request_max_size = 100;
static_assert(request_min_size <= request_max_size, "`request_min_size` should not be greater than `request_max_size`!");

}   // end of namespace dm;
