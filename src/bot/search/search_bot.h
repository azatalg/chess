#pragma once

#include "../bot/bot.h"
#include <EASTL/array.h>
#include <EASTL/vector.h>

namespace chess::ai {

class SearchBot final : public Bot {
public:
    SearchBot(const Board& board, Color color);
    Move choose_move(i32 time_budget_ms = 500);

private:
    enum class Bound : u8 { Exact, Lower, Upper };
    struct TTEntry {
        u64 key = 0;
        Move best{};
        i32 score = 0;
        i16 depth = -1;
        Bound bound = Bound::Exact;
    };

    eastl::vector<TTEntry> table_;
    Timer timer_;
    i32 deadline_ms_ = 0;
    u64 nodes_ = 0;
    bool stopped_ = false;
    eastl::array<eastl::array<Move, 2>, 128> killers_{};
    eastl::array<eastl::array<eastl::array<i32, 64>, 64>, 2> history_{};
    eastl::array<u64, 128> repetition_{};

    i32 negamax(Board& board, i32 depth, i32 alpha, i32 beta, i32 ply);
    i32 quiescence(Board& board, i32 alpha, i32 beta, i32 ply);
    i32 evaluate(const Board& board) const;
    bool time_expired();
    void order_moves(MoveList& moves, Move tt_move, Color side, i32 ply) const;
    i32 move_score(Move move, Move tt_move, Color side, i32 ply) const;
    bool repeated(u64 key, i32 ply) const;
};

} // namespace chess::ai
