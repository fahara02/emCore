#include "runtime.hpp"

namespace emCore::runtime {

alignas(8) unsigned char g_arena[emCore::memory::required_bytes];

} // namespace emCore::runtime
