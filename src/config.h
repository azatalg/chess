typedef unsigned long long u64;
typedef short i8;
typedef unsigned int u32;
typedef unsigned short u8;
#define U64(x) static_cast<u64>(x)
#define U8(x) static_cast<u8>(x)
enum class Piece { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
enum class Color { WHITE, BLACK };
