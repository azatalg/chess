#include "bot/search/search_bot.h"
#include "game/board.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using chess::Board;
using chess::Move;
using chess::MoveList;
using chess::StateInfo;
using chess::u64;

volatile u64 sink = 0;

u64 perft(Board& board, int depth) {
    if (depth == 0) return 1;
    MoveList moves;
    board.generate_legal(moves);
    u64 nodes = 0;
    for (Move move : moves) {
        StateInfo state;
        board.make_move(move, state);
        nodes += perft(board, depth - 1);
        board.unmake_move(move, state);
    }
    return nodes;
}

struct Result {
    std::string name;
    std::string unit;
    u64 operations;
    double seconds;
};

template <class Work>
Result measure(const std::string& name, const std::string& unit, Work work) {
    const auto started = Clock::now();
    const u64 operations = work();
    const double seconds = std::chrono::duration<double>(Clock::now() - started).count();
    return {name, unit, operations, seconds};
}

void print_result(const Result& result) {
    const double rate = result.seconds > 0.0 ? result.operations / result.seconds : 0.0;
    std::cout << std::left << std::setw(28) << result.name << std::right
              << std::setw(11) << std::fixed << std::setprecision(2)
              << result.seconds * 1000.0 << " ms  "
              << std::setw(13) << std::setprecision(0) << rate << ' '
              << result.unit << "/s\n";
}

bool selected(const std::string& name, const std::string& filter) {
    return filter.empty() || name.find(filter) != std::string::npos;
}

void usage(const char* program) {
    std::cout << "Usage: " << program << " [--quick|--full] [--filter TEXT] [--list]\n"
              << "  --quick       short, repeatable run (default)\n"
              << "  --full        deeper perft and longer search samples\n"
              << "  --filter TEXT run benchmarks whose name contains TEXT\n"
              << "  --list        list benchmark names\n";
}
} // namespace

int main(int argc, char** argv) {
    bool full = false;
    bool list = false;
    std::string filter;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--full") full = true;
        else if (argument == "--quick") full = false;
        else if (argument == "--list") list = true;
        else if (argument == "--filter" && i + 1 < argc) filter = argv[++i];
        else if (argument == "--help" || argument == "-h") { usage(argv[0]); return 0; }
        else { std::cerr << "Unknown or incomplete argument: " << argument << '\n'; usage(argv[0]); return 2; }
    }

    const std::vector<std::string> names = {
        "movegen/start", "movegen/kiwipete", "make-unmake/start",
        "fen/parse", "perft/start", "perft/kiwipete", "search/middlegame"
    };
    if (list) {
        for (const auto& name : names) std::cout << name << '\n';
        return 0;
    }

    std::cout << "Chess engine benchmark (" << (full ? "full" : "quick") << ")\n";
    std::cout << "Times are wall-clock; use a Release build for meaningful comparisons.\n\n";

    std::vector<Result> results;
    auto add = [&](const std::string& name, const std::string& unit, auto work) {
        if (selected(name, filter)) results.push_back(measure(name, unit, work));
    };
    const char* kiwipete_fen =
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    const char* middle_fen =
        "r1bq1rk1/pp2bppp/2n1pn2/2pp4/3P4/2PBPN2/PPQ2PPP/R1B2RK1 w - - 2 10";
    const u64 repetitions = full ? 2'000'000 : 250'000;

    add("movegen/start", "positions", [=] {
        Board board;
        u64 moves = 0;
        for (u64 i = 0; i < repetitions; ++i) { MoveList list; board.generate_legal(list); moves += list.size(); }
        sink += moves;
        return repetitions;
    });
    add("movegen/kiwipete", "positions", [=] {
        Board board = Board::from_fen(kiwipete_fen);
        u64 moves = 0;
        for (u64 i = 0; i < repetitions; ++i) { MoveList list; board.generate_legal(list); moves += list.size(); }
        sink += moves;
        return repetitions;
    });
    add("make-unmake/start", "moves", [=] {
        Board board;
        MoveList moves;
        board.generate_legal(moves);
        u64 count = 0;
        const u64 rounds = repetitions / moves.size();
        for (u64 i = 0; i < rounds; ++i) for (Move move : moves) {
            StateInfo state; board.make_move(move, state); board.unmake_move(move, state); ++count;
        }
        sink += board.hash();
        return count;
    });
    add("fen/parse", "positions", [=] {
        const u64 count = full ? 1'000'000 : 100'000;
        u64 hashes = 0;
        for (u64 i = 0; i < count; ++i) hashes ^= Board::from_fen(middle_fen).hash() + i;
        sink += hashes;
        return count;
    });
    add("perft/start", "nodes", [=] {
        Board board;
        const u64 nodes = perft(board, full ? 6 : 5);
        sink += nodes;
        return nodes;
    });
    add("perft/kiwipete", "nodes", [=] {
        Board board = Board::from_fen(kiwipete_fen);
        const u64 nodes = perft(board, full ? 5 : 4);
        sink += nodes;
        return nodes;
    });
    add("search/middlegame", "searches", [=] {
        const int samples = full ? 5 : 3;
        const int budget_ms = full ? 500 : 100;
        for (int i = 0; i < samples; ++i) {
            Board board = Board::from_fen(middle_fen);
            chess::ai::SearchBot bot(board, board.side_to_move());
            sink += bot.choose_move(budget_ms).to();
        }
        return static_cast<u64>(samples);
    });

    if (results.empty()) {
        std::cerr << "No benchmark matches filter '" << filter << "'. Use --list.\n";
        return 1;
    }
    for (const Result& result : results) print_result(result);
    std::cout << "\nTip: compare runs on the same machine and build configuration.\n";
    return EXIT_SUCCESS;
}
