#pragma once

#include "../config.h"

namespace chess::magic {

// Initialized once and then reduced to mask/multiply/shift/table lookups.
u64 rook_attacks(u8 square, u64 occupied);
u64 bishop_attacks(u8 square, u64 occupied);

} // namespace chess::magic
