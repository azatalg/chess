#include <iostream>
#include "./game/board.h"
using namespace std;

int main() {
	Board board;

	vector<Moves> moves = board.get_all_legal_moves(Color::WHITE);
	for(auto i: moves) {
		cout<<i.getStart()<<' '<<i.getEnd()<<'\n';
	}
}
