// data types
typedef unsigned long long u64;
typedef unsigned int u32;
typedef int i32;
typedef unsigned short u8;

// enums
enum class Piece { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
enum class Color { WHITE, BLACK };

// methods for enums
inline Color opposite(Color c){
    return c == Color::WHITE ? Color::BLACK : Color::WHITE;
}
// makro
#define U64(x) static_cast<u64>(x)
#define U8(x) static_cast<u8>(x)

// const 
constexpr i32 INF = 1e9 + 7;
