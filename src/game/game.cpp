#include "game.h"

namespace chess {

Game::Game() {
    position_history_.push_back(board_.hash());
}

void Game::play(Move move) {
    board_.make_move(move);
    position_history_.push_back(board_.hash());
}

bool Game::threefold_repetition() const {
    int occurrences = 0;
    for (u64 hash : position_history_)
        if (hash == board_.hash() && ++occurrences == 3) return true;
    return false;
}

GameResult Game::result() {
    if (board_.halfmove_clock() >= 100) return GameResult::FiftyMoveDraw;
    if (threefold_repetition()) return GameResult::RepetitionDraw;
    if (board_.insufficient_material()) return GameResult::InsufficientMaterialDraw;

    MoveList moves;
    board_.generate_legal(moves);
    if (!moves.empty()) return GameResult::Ongoing;
    if (!in_check()) return GameResult::Stalemate;
    return side_to_move() == Color::White ? GameResult::BlackWins : GameResult::WhiteWins;
}

} // namespace chess
