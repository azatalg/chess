#include "bot/search/search_bot.h"
#include <cstdlib>
#include <iostream>

int main() {
    chess::Board board = chess::Board::from_fen(
        "7k/5Q2/6K1/8/8/8/8/8 w - - 0 1");
    chess::ai::SearchBot bot(board, chess::Color::White);
    const chess::Move move = bot.choose_move(100);
    chess::StateInfo state;
    board.make_move(move, state);
    chess::MoveList replies;
    board.generate_legal(replies);
    if (!replies.empty() || !board.in_check(chess::Color::Black)) {
        std::cerr << "SearchBot missed mate in one\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
