#include "game/board.h"
#include <cstdlib>
#include <iostream>

namespace {
using namespace chess;

u64 perft(Board& board, int depth) {
    if (depth == 0) return 1;
    MoveList moves;
    board.generate_legal(moves);
    u64 nodes = 0;
    for (Move move : moves) {
        StateInfo undo;
        board.make_move(move, undo);
        nodes += perft(board, depth - 1);
        board.unmake_move(move, undo);
    }
    return nodes;
}

bool expect(u64 actual, u64 expected, const char* name) {
    if (actual == expected) return true;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    return false;
}

bool generation_partition(Board& board, const char* name) {
    MoveList all, captures, quiets;
    board.generate(all, Generation::All);
    board.generate(captures, Generation::Captures);
    board.generate(quiets, Generation::Quiets);
    return expect(u64(captures.size() + quiets.size()), all.size(), name);
}
}

int main() {
    chess::Board board;
    const chess::u64 initial_hash = board.hash();
    bool ok = expect(perft(board, 1), 20, "perft(1)") &&
              expect(perft(board, 2), 400, "perft(2)") &&
              expect(perft(board, 3), 8902, "perft(3)") &&
              expect(perft(board, 4), 197281, "start perft(4)") &&
              expect(perft(board, 5), 4865609, "start perft(5)") &&
              expect(perft(board, 6), 119060324, "start perft(6)") &&
              generation_partition(board, "start generation partition") &&
              expect(board.hash(), initial_hash, "make/unmake hash");

    chess::Board kiwipete = chess::Board::from_fen(
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    ok = ok && expect(perft(kiwipete, 1), 48, "kiwipete perft(1)") &&
         expect(perft(kiwipete, 2), 2039, "kiwipete perft(2)") &&
         expect(perft(kiwipete, 3), 97862, "kiwipete perft(3)") &&
         generation_partition(kiwipete, "kiwipete generation partition");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
