#include "terminal.h"
#include "bot/flat_mc/flat_mc.h"
#include "bot/search/search_bot.h"
#include "game/game.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>

namespace chess::cli {
namespace {

struct Options {
    bool bot_white = false;
    bool bot_black = false;
    i32 bot_time_ms = 100;
    bool flat_monte_carlo = false;
};

void print_usage(const char* program) {
    std::cout << "Użycie: " << program << " [--bot white|black|both] [--bot-time MS] [--bot-type search|flat]\n"
              << "  bez opcji       człowiek kontra człowiek\n"
              << "  --bot black     człowiek (białe) kontra bot\n"
              << "  --bot white     bot kontra człowiek (czarne)\n"
              << "  --bot both      bot kontra bot\n";
}

bool parse_options(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            return false;
        }
        if (argument == "--bot" && i + 1 < argc) {
            const std::string side = argv[++i];
            options.bot_white = side == "white" || side == "both";
            options.bot_black = side == "black" || side == "both";
            if (side != "white" && side != "black" && side != "both") {
                std::cerr << "Nieznana strona bota: " << side << '\n';
                return false;
            }
        } else if (argument == "--bot-time" && i + 1 < argc) {
            options.bot_time_ms = std::atoi(argv[++i]);
            if (options.bot_time_ms < 1) options.bot_time_ms = 1;
        } else if (argument == "--bot-type" && i + 1 < argc) {
            const std::string type = argv[++i];
            if (type != "search" && type != "flat") {
                std::cerr << "Nieznany typ bota: " << type << '\n';
                return false;
            }
            options.flat_monte_carlo = type == "flat";
        } else {
            std::cerr << "Nieznana opcja: " << argument << '\n';
            print_usage(argv[0]);
            return false;
        }
    }
    return true;
}

char piece_symbol(Piece piece, Color color) {
    static constexpr char symbols[] = {'p', 'n', 'b', 'r', 'q', 'k'};
    const char symbol = symbols[index(piece)];
    return color == Color::White
        ? static_cast<char>(std::toupper(static_cast<unsigned char>(symbol))) : symbol;
}

void print_board(const Board& board) {
    std::cout << "\n    a b c d e f g h\n   -----------------\n";
    for (int rank = 7; rank >= 0; --rank) {
        std::cout << ' ' << rank + 1 << " | ";
        for (int file = 0; file < 8; ++file) {
            Piece piece = Piece::None;
            Color color = Color::White;
            std::cout << (board.piece_at(u8(rank * 8 + file), piece, color)
                              ? piece_symbol(piece, color) : '.') << ' ';
        }
        std::cout << "| " << rank + 1 << '\n';
    }
    std::cout << "   -----------------\n    a b c d e f g h\n\n";
}

std::string normalized(std::string text) {
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) {
        return c == '-' || c == 'x' || std::isspace(c);
    }), text.end());
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

Piece promotion_piece(char symbol) {
    switch (symbol) {
        case 'n': return Piece::Knight;
        case 'b': return Piece::Bishop;
        case 'r': return Piece::Rook;
        case 'q': return Piece::Queen;
        default: return Piece::None;
    }
}

bool parse_move(const std::string& raw, const MoveList& legal, Move& selected) {
    const std::string text = normalized(raw);
    if ((text.size() != 4 && text.size() != 5) || text[0] < 'a' || text[0] > 'h' ||
        text[2] < 'a' || text[2] > 'h' || text[1] < '1' || text[1] > '8' ||
        text[3] < '1' || text[3] > '8') return false;
    const u8 from = u8((text[1] - '1') * 8 + text[0] - 'a');
    const u8 to = u8((text[3] - '1') * 8 + text[2] - 'a');
    const Piece promotion = text.size() == 5 ? promotion_piece(text[4]) : Piece::Queen;
    for (Move move : legal) {
        if (move.from() == from && move.to() == to &&
            (!move.is_promotion() || move.promotion() == promotion)) {
            selected = move;
            return true;
        }
    }
    return false;
}

const char* side_name(Color side) { return side == Color::White ? "białe" : "czarne"; }

std::string move_name(Move move) {
    std::string result;
    result += char('a' + (move.from() & 7));
    result += char('1' + (move.from() >> 3));
    result += char('a' + (move.to() & 7));
    result += char('1' + (move.to() >> 3));
    if (move.is_promotion()) {
        static constexpr char promotion_symbols[] = {'p', 'n', 'b', 'r', 'q', 'k'};
        result += promotion_symbols[index(move.promotion())];
    }
    return result;
}

bool print_result(GameResult result) {
    switch (result) {
        case GameResult::Ongoing: return false;
        case GameResult::WhiteWins: std::cout << "Mat! Wygrywają białe.\n"; break;
        case GameResult::BlackWins: std::cout << "Mat! Wygrywają czarne.\n"; break;
        case GameResult::Stalemate: std::cout << "Pat. Remis.\n"; break;
        case GameResult::FiftyMoveDraw: std::cout << "Remis: reguła 50 ruchów.\n"; break;
        case GameResult::RepetitionDraw: std::cout << "Remis: trzykrotne powtórzenie.\n"; break;
        case GameResult::InsufficientMaterialDraw: std::cout << "Remis: niewystarczający materiał.\n"; break;
    }
    return true;
}

} // namespace

int run(int argc, char** argv) {
    Options options;
    if (!parse_options(argc, argv, options)) return argc > 1 ? 0 : 1;
    Game game;
    std::cout << "Szachy terminalowe\nRuch: e2e4, roszada: e1g1, promocja: e7e8q.\n"
                 "Komendy: help, quit.\n";
    for (;;) {
        print_board(game.board());
        if (print_result(game.result())) return 0;
        if (game.in_check()) std::cout << "Szach!\n";

        const bool bot_turn = game.side_to_move() == Color::White
            ? options.bot_white : options.bot_black;
        if (bot_turn) {
            std::cout << "Bot (" << side_name(game.side_to_move()) << ") myśli...\n";
            Move move;
            if (options.flat_monte_carlo) {
                ai::FlatMonteCarlo bot(game.board(), game.side_to_move());
                move = bot.choose_move(options.bot_time_ms);
            } else {
                ai::SearchBot bot(game.board(), game.side_to_move());
                move = bot.choose_move(options.bot_time_ms);
            }
            std::cout << "Bot gra: " << move_name(move) << '\n';
            game.play(move);
            continue;
        }

        std::cout << "Ruch gracza " << side_name(game.side_to_move()) << ": ";
        std::string input;
        if (!std::getline(std::cin, input)) return 0;
        const std::string command = normalized(input);
        if (command == "quit" || command == "exit" || command == "q") return 0;
        if (command == "help" || command == "h") {
            std::cout << "Format: pole startowe + docelowe, np. e2e4. "
                         "Przy promocji dodaj q/r/b/n.\n";
            continue;
        }
        MoveList legal;
        game.legal_moves(legal);
        Move move;
        if (!parse_move(input, legal, move)) {
            std::cout << "Nieprawidłowy albo niedozwolony ruch.\n";
            continue;
        }
        game.play(move);
    }
}

} // namespace chess::cli
