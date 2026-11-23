#include "magic_bitboards.h"
#include "attacks.h"
#include <EASTL/array.h>

namespace chess::magic {
namespace {

struct Entry {
    u64 mask = 0;
    u64 magic = 0;
    u32 offset = 0;
    u8 shift = 0;
};

constexpr u32 rook_table_size = 102400;
constexpr u32 bishop_table_size = 5248;

inline constexpr eastl::array<u64, 64> rook_magics = {
    0x8080003885400021ULL,0x2040100040002000ULL,0x0100081100200040ULL,0x1180140800300080ULL,
    0x0200082004100201ULL,0x2280111200040080ULL,0x0480108001001200ULL,0x01000032c2048500ULL,
    0x0002800180400120ULL,0x0004808020004000ULL,0x0620802000100880ULL,0x0904800804100082ULL,
    0x2000808008000400ULL,0x0062001002000804ULL,0x0015000200040100ULL,0x1002000100820044ULL,
    0x0240028000204480ULL,0x0040008020008040ULL,0x0400888010002000ULL,0x0008008080100008ULL,
    0xa000828024002800ULL,0x0404004002010040ULL,0x0004440028411052ULL,0x0081120004984411ULL,
    0xa880004040002010ULL,0x0400500140002000ULL,0x3000200100110040ULL,0x0002000a00204010ULL,
    0x0041040180080080ULL,0x0024008080020004ULL,0x4090080400019002ULL,0x0406140a0000a841ULL,
    0x0000408001002108ULL,0x0100201002400440ULL,0x0220802000801008ULL,0x0408008008801004ULL,
    0x0100040080800800ULL,0x0000020080800400ULL,0x010210268c001508ULL,0x0402210042000894ULL,
    0x5034800040028020ULL,0x2010002000404008ULL,0x2200100020008080ULL,0x0021002010010008ULL,
    0x00e0050008010010ULL,0x4008020004008080ULL,0x5000081001040002ULL,0x012e008054020011ULL,
    0x0000410080002500ULL,0x1c8a011840208600ULL,0x0100801000200880ULL,0x0030100008230100ULL,
    0x0211000800100500ULL,0x0100040002008080ULL,0x82da088110020400ULL,0x0000009114004200ULL,
    0x4000228009021041ULL,0x0040144000802101ULL,0x040100200010400dULL,0x0085016050004409ULL,
    0x2891008208001005ULL,0x4001000400020803ULL,0x000200240800c102ULL,0x0009000080320041ULL
};

inline constexpr eastl::array<u64, 64> bishop_magics = {
    0x00020882008a0200ULL,0x0070344900421200ULL,0x0048024400288844ULL,0x8004040888081040ULL,
    0x0014142010004408ULL,0x420a082208208000ULL,0xc014011108210002ULL,0x3240108090082000ULL,
    0x0210404401040900ULL,0x0080021208420083ULL,0x0010840820810000ULL,0x0001580a10210108ULL,
    0x4400240420004200ULL,0x4068010460040200ULL,0x20000a0104824000ULL,0x0002010090c42020ULL,
    0x0210000560080100ULL,0x0010080801082088ULL,0x063002020c001820ULL,0x1a22001040144008ULL,
    0x5221002290400804ULL,0x400200030108a220ULL,0x8212000100d01480ULL,0x0040241901111000ULL,
    0x0104300125200804ULL,0x0614824090101108ULL,0x0408080001004500ULL,0x0450040050440008ULL,
    0x8010840000802000ULL,0x800042000d011102ULL,0x0024448226521006ULL,0x0020408c090a880cULL,
    0x00082004a9100400ULL,0x8008041001052101ULL,0x0000ac0500900100ULL,0x0000910800040040ULL,
    0x8153050400020020ULL,0x4010020a210e0288ULL,0x4108156402410080ULL,0x001a020205002090ULL,
    0x0018088804000808ULL,0x1002210420008241ULL,0x08000a0082011000ULL,0x0010004010410a01ULL,
    0x1408080104000040ULL,0x1220008122012441ULL,0x0109020800406a05ULL,0x0450440100212040ULL,
    0x2004008210106080ULL,0x4440805108200001ULL,0x4202042128080000ULL,0x208200420a020048ULL,
    0x3000005102020480ULL,0x8000c00901010010ULL,0x0040050802004004ULL,0x1a08101900530080ULL,
    0x004124010401a004ULL,0x0008008888015000ULL,0x0040000110880412ULL,0x00409040802a0800ULL,
    0x0041301190021202ULL,0x4420114148010100ULL,0x0802080284081200ULL,0xc008109102002204ULL
};

u64 relevant_mask(u8 square, int begin, int end) {
    u64 mask = 0;
    for (int direction = begin; direction < end; ++direction) {
        int file = (square & 7) + attacks::directions[direction][0];
        int rank = (square >> 3) + attacks::directions[direction][1];
        while (attacks::valid(file, rank)) {
            const int next_file = file + attacks::directions[direction][0];
            const int next_rank = rank + attacks::directions[direction][1];
            if (!attacks::valid(next_file, next_rank)) break;
            mask |= square_bit(u8(rank * 8 + file));
            file = next_file;
            rank = next_rank;
        }
    }
    return mask;
}

u64 attacks_on_fly(u8 square, u64 occupied, int begin, int end) {
    u64 result = 0;
    for (int direction = begin; direction < end; ++direction) {
        int file = (square & 7) + attacks::directions[direction][0];
        int rank = (square >> 3) + attacks::directions[direction][1];
        while (attacks::valid(file, rank)) {
            const u64 target = square_bit(u8(rank * 8 + file));
            result |= target;
            if (occupied & target) break;
            file += attacks::directions[direction][0];
            rank += attacks::directions[direction][1];
        }
    }
    return result;
}

template <u32 TableSize>
class MagicTable {
public:
    MagicTable(int begin, int end, const eastl::array<u64, 64>& magics) {
        u32 offset = 0;
        for (u8 square = 0; square < 64; ++square) {
            Entry& entry = entries_[square];
            entry.mask = relevant_mask(square, begin, end);
            const int bits = __builtin_popcountll(entry.mask);
            entry.shift = u8(64 - bits);
            entry.offset = offset;
            entry.magic = magics[square];
            const u32 count = 1U << bits;

            u64 subset = 0;
            for (u32 i = 0; i < count; ++i) {
                const u32 magic_index = u32((subset * entry.magic) >> entry.shift);
                table_[offset + magic_index] = attacks_on_fly(square, subset, begin, end);
                subset = (subset - entry.mask) & entry.mask;
            }
            offset += count;
        }
    }

    u64 attacks(u8 square, u64 occupied) const {
        const Entry& entry = entries_[square];
        const u32 index = u32(((occupied & entry.mask) * entry.magic) >> entry.shift);
        return table_[entry.offset + index];
    }

private:
    eastl::array<Entry, 64> entries_{};
    eastl::array<u64, TableSize> table_{};
};

const MagicTable<rook_table_size>& rooks() {
    static const MagicTable<rook_table_size> table(0, 4, rook_magics);
    return table;
}

const MagicTable<bishop_table_size>& bishops() {
    static const MagicTable<bishop_table_size> table(4, 8, bishop_magics);
    return table;
}

} // namespace

u64 rook_attacks(u8 square, u64 occupied) { return rooks().attacks(square, occupied); }
u64 bishop_attacks(u8 square, u64 occupied) { return bishops().attacks(square, occupied); }

} // namespace chess::magic
