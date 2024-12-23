#include "benchmark.h"

using namespace std;
using namespace std::chrono;

uint64_t nodes = 0;

uint64_t _perft(Position* board, int depth) {
	if (depth == 0) {
		return 1ULL;
	}
	else {
		uint64_t value = 0;
		MoveList ml;
		ml.generate(*board);
		for (Square* s = ml.list; s < ml.end; s++) {
			Bitboard c;
			board->do_move(*s, &c);
			value += _perft(board, depth - 1);
			board->undo_move(*s, &c);
		}
		if (ml.list == ml.end) {
			board->pass();
			ml.generate(*board);
			if (ml.list == ml.end) {
				board->pass();
				return 1ULL;
			}
			value += _perft(board, depth - 1);
			board->pass();
		}
		return value;
	}
}

void perft(Position* board, int depth) {
	system_clock::time_point time_start = system_clock::now();
	milliseconds time = milliseconds(0);

	uint64_t value = 0;
	MoveList ml;
	ml.generate(*board);
	for (Square* s = ml.list; s < ml.end; s++) {
		Bitboard c;
		board->do_move(*s, &c);
		uint64_t sub = _perft(board, depth - 1);
		cout << *s << " " << sub << endl;
		value += sub;
		board->undo_move(*s, &c);
	}
	cout << value << '\n';

	system_clock::time_point time_now = system_clock::now();
	time = duration_cast<milliseconds>(time_now - time_start);
	cout << "took " << time.count() << "ms, " <<
		double(value) / time.count() / 1000 << " MNps" << endl;
}

int _solve(int depth, int alpha, int beta, int& type) {
	nodes++;
	if (Threads.stop) { return EVAL_FAIL; }

	Key root_key = Threads.board->get_key();

	MoveList legal_moves;
	legal_moves.generate(*Threads.board);

	if (legal_moves.list == legal_moves.end) {
		Threads.board->pass();
		legal_moves.generate(*Threads.board);

		if (legal_moves.list == legal_moves.end) {
			Threads.board->pass();
			int score = get_material_eval(*Threads.board);
			type = 0;
			Main_TT.register_entry(root_key, score, GAME_END, 0, 0);
			return score;
		}

		int type_;
		int after_pass = -_solve(depth - 1, -beta, -alpha, type_);
		Threads.board->pass();

		type = -type_;
		Main_TT.register_entry(root_key, after_pass, NULL_MOVE, depth + 1, type);
		return after_pass;
	}

	// Legal moves
	else {
		Square s;
		Square nmove = NULL_MOVE;
		int comp_eval;
		int new_eval = EVAL_INIT;
		int alpha_i = alpha;

		int i = 0;
		type = 0;
		while ((i < legal_moves.length()) && !(Threads.stop)) {
			s = *(legal_moves.list + i);

			Bitboard c;
			Threads.board->do_move(s, &c);

			TTEntry probe;
			Main_TT.probe(Threads.board->get_key(), &probe);
			if (!is_miss(&probe, Threads.board->get_key())) {
				comp_eval = -probe.eval;
				if (comp_eval >= beta &&
					probe.type != -1) {
					new_eval = comp_eval;
					alpha = comp_eval;
					nmove = s;
					type = -1;
					Threads.board->undo_move(s, &c);
					break;
				}
				if (comp_eval < new_eval &&
					probe.type != 1) {
					Threads.board->undo_move(s, &c);
					i++;
					continue;
				}
			}

			int type_;
			comp_eval = -_solve(depth - 1, -beta, -alpha, type_);

			if (comp_eval > new_eval) {
				new_eval = comp_eval;
				nmove = s;
				if (new_eval > alpha) {
					alpha = new_eval;
					if (alpha >= beta) {
						type = -1;
						Threads.board->undo_move(s, &c);
						break;
					}
				}
			}

			Threads.board->undo_move(s, &c);
			i++;
		}

		if (alpha == alpha_i) { type = 1; }

		Main_TT.register_entry(root_key, alpha, nmove, depth + 1, type);

		return alpha;
	}
}

void solve() {
	Threads.stop = false;
	nodes = 0;
	system_clock::time_point time_start = system_clock::now();
	milliseconds time = milliseconds(0);

	int type;
	int eval = _solve(64, EVAL_MIN, EVAL_MAX, type);
	Threads.stop = true;

	system_clock::time_point time_now = system_clock::now();
	time = duration_cast<milliseconds>(time_now - time_start);

	stringstream buf;
	getpv(buf, Threads.board);
	cout << "elapsed time: " << time.count() << "ms"
		<< "\neval: " << eval_print(eval)
		<< "\nnodes: " << nodes
		<< ", " << double(nodes) / time.count() / 1000 << " MNps"
		<< "\nType: " << type
		<< "\npv: " << buf.str() << endl;
}

void net_speedtest() {
	Net* n = new Net;
	zero_weights(n);
	rand_weights_all(n, 2);

	int16_t P1[SIZE_F1 * 2] = { };

	cout << "Update test: 64M calls\n";

	// Update
	system_clock::time_point time_start = system_clock::now();
	milliseconds time = milliseconds(0);

	Bitboard pieces[3] = {};
	Piece sq[64] = { };
	pieces[EMPTY] = FullBoard;

	for (int j = 0; j < 1000000; j++) {
		for (Square s = A1; s < SQ_END; ++s) {
			Piece p = Piece(rng.get() & 3);
			if (p == MISC) { p = EMPTY; }

			update_L0(P1, s, sq[s], p, n);

			pieces[sq[s]] ^= SquareBoard[s];
			pieces[p]     ^= SquareBoard[s];
			sq[s] = p;
		}
	}

	system_clock::time_point time_now = system_clock::now();
	time = duration_cast<milliseconds>(time_now - time_start);

	cout << "elapsed time: " << time.count() << "ms"
		<< ", " << double(64000) / time.count() << " MOps" << endl;

	cout << "Evaluate test: 10M calls\n";
	int v[2];

	// Evaluate
	time_start = system_clock::now();

	for (int i = 0; i < 10000000; i++) {
		for (int j = 0; j < SIZE_F1; j++) {
			uint64_t r = rng.get();
			P1[j++] = int16_t((r >> 0) & 127);
			P1[j++] = int16_t((r >> 8) & 127);
			P1[j++] = int16_t((r >> 16) & 127);
			P1[j]   = int16_t((r >> 24) & 127);
		}
		compute(v, P1, n, BLACK);
	}

	time_now = system_clock::now();
	time = duration_cast<milliseconds>(time_now - time_start);

	cout << "elapsed time: " << time.count() << "ms"
		<< ", " << double(10000) / time.count() << " MOps" 
		<< '\n' << v[0] << ' ' << v[1] << endl;
}

int find_best(Position& board, int depth,
	int alpha, int beta) {
	if (depth < 1) { return eval(board); }

	MoveList legal_moves;
	legal_moves.generate(board);

	if (legal_moves.list == legal_moves.end) {
		board.pass();
		legal_moves.generate(board);
		if (legal_moves.list == legal_moves.end) {
			board.pass();
			return get_material_eval(board);
		}
		int after_pass = -find_best(board, depth - 1, -beta, -alpha);
		board.pass();
		return after_pass;
	}

	Square s;
	Square nmove = NULL_MOVE;
	int comp_eval;
	int new_eval = EVAL_INIT;

	for (int i = 0; i < legal_moves.length(); i++) {
		s = legal_moves.list[i];

		Bitboard c;
		board.do_move(s, &c);
		comp_eval = -find_best(board, depth - 1, -beta, -alpha);
		board.undo_move(s, &c);

		if (comp_eval > new_eval) {
			new_eval = comp_eval;
			nmove = s;
			if (new_eval > alpha) { alpha = new_eval; }
			if (alpha > beta) { break; }
		}
	}
	return new_eval;
}

int find_g(Position& board, int depth,
	int alpha, int beta) {
	if (depth < 1) { return get_material_eval(board); }

	MoveList legal_moves;
	legal_moves.generate(board);

	if (legal_moves.list == legal_moves.end) {
		board.pass();
		legal_moves.generate(board);
		if (legal_moves.list == legal_moves.end) {
			board.pass();
			return get_material_eval(board);
		}
		int after_pass = -find_g(board, depth - 1, -beta, -alpha);
		board.pass();
		return after_pass;
	}

	Square s;
	Square nmove = NULL_MOVE;
	int comp_eval;
	int new_eval = EVAL_INIT;

	for (int i = 0; i < legal_moves.length(); i++) {
		s = legal_moves.list[i];

		Bitboard c;
		board.do_move(s, &c);
		comp_eval = -find_g(board, depth - 1, -beta, -alpha);
		board.undo_move(s, &c);

		if (comp_eval > new_eval) {
			new_eval = comp_eval;
			nmove = s;
			if (new_eval > alpha) { alpha = new_eval; }
			if (alpha > beta) { break; }
		}
	}
	return new_eval;
}

int play_r(Position& board1, int sd, bool rand) {
	MoveList legal_moves;
	legal_moves.generate(board1);
	if (legal_moves.list == legal_moves.end) {
		board1.pass();
		legal_moves.generate(board1);
		if (legal_moves.list == legal_moves.end) {
			board1.pass();
			return get_material_eval(board1);
		}
		int r = -play_r(board1, sd, !rand);
		board1.pass();
		return r;
	}
	else {
		Square s;
		Square nmove = NULL_MOVE;
		int comp_eval;
		int new_eval = EVAL_INIT;

		if (rand) {
			nmove = legal_moves.list[rng.get() % legal_moves.length()];
		}
		else {
			for (int i = 0; i < legal_moves.length(); i++) {
				s = legal_moves.list[i];
				Bitboard c;
				board1.do_move(s, &c);
				comp_eval = -find_best(board1, sd, EVAL_MIN, -new_eval);
				if (comp_eval > new_eval) {
					new_eval = comp_eval;
					nmove = s;
				}
				board1.undo_move(s, &c);
			}
		}
		Bitboard c;
		board1.do_move(nmove, &c);
		int r = -play_r(board1, sd, !rand);
		board1.undo_move(nmove, &c);
		return r;
	}
}

int play_g(Position& board1, int sd, bool rand) {
	MoveList legal_moves;
	legal_moves.generate(board1);
	if (legal_moves.list == legal_moves.end) {
		board1.pass();
		legal_moves.generate(board1);
		if (legal_moves.list == legal_moves.end) {
			board1.pass();
			return get_material_eval(board1);
		}
		int r = -play_g(board1, sd, !rand);
		board1.pass();
		return r;
	}
	else {
		Square s;
		Square nmove = NULL_MOVE;
		int comp_eval;
		int new_eval = EVAL_INIT;
		for (int i = 0; i < legal_moves.length(); i++) {
			s = legal_moves.list[i];
			Bitboard c;
			board1.do_move(s, &c);
			if (rand) {
				comp_eval = -find_g(board1, sd, EVAL_MIN, -new_eval);
			}
			else {
				comp_eval = -find_best(board1, sd, EVAL_MIN, -new_eval);
			}
			if (comp_eval > new_eval) {
				new_eval = comp_eval;
				nmove = s;
			}
			board1.undo_move(s, &c);
		}
		Bitboard c;
		board1.do_move(nmove, &c);
		int r = -play_g(board1, sd, !rand);
		board1.undo_move(nmove, &c);
		return r;
	}
}

int play_n(Position& board1, Position& board2, int sd) {

	MoveList legal_moves;
	legal_moves.generate(board1);
	if (legal_moves.list == legal_moves.end) {
		board1.pass();
		legal_moves.generate(board1);
		if (legal_moves.list == legal_moves.end) {
			board1.pass();
			return get_material_eval(board1);
		}
		board2.pass();
		int r = -play_n(board2, board1, sd);
		board1.pass();
		board2.pass();
		return r;
	}
	else {
		if (board1.get_count_empty() < 9) {
			return find_best(board1, 64, EVAL_MIN, EVAL_MAX);
		}

		Square s;
		Square nmove = NULL_MOVE;
		int comp_eval;
		int new_eval = EVAL_INIT;
		for (int i = 0; i < legal_moves.length(); i++) {
			s = legal_moves.list[i];
			Bitboard c;
			board1.do_move(s, &c);
			comp_eval = -find_best(board1, sd, EVAL_MIN, -new_eval);
			if (comp_eval > new_eval) {
				new_eval = comp_eval;
				nmove = s;
			}
			board1.undo_move(s, &c);
		}
		Bitboard c1, c2;
		board1.do_move(nmove, &c1);
		board2.do_move(nmove, &c2);
		int r = -play_n(board2, board1, sd);
		board1.undo_move(nmove, &c1);
		board2.undo_move(nmove, &c2);
		return r;
	}
}

void test_thread(Net* n1, Net* n2, 
	int type, int games, int depth_start, int depth_search,
	int* res) 
{
	Position board1(n1);
	Position board2(n2);

	int win  = 0;
	int draw = 0;
	int loss = 0;

	system_clock::time_point time_start = system_clock::now();
	milliseconds time = milliseconds(0);

	for (int i = 0; i < games; i++) {
		board1.set(startpos_fen);

		for (int j = 0; j < depth_start; j++) {
			MoveList legal_moves;
			legal_moves.generate(board1);
			if (legal_moves.list == legal_moves.end) {
				board1.pass();
			}
			else {
				Square m = legal_moves.list[rng.get() % legal_moves.length()];
				Bitboard c;
				board1.do_move(m, &c);
			}
		}

		MoveList legal_moves;
		legal_moves.generate(board1);
		if (legal_moves.list == legal_moves.end) {
			board1.pass();
			legal_moves.generate(board1);
			if (legal_moves.list == legal_moves.end) {
				i--;
				continue;
			}
		}

		board1.set_accumulator();

		int score1, score2;

		switch (type) {
		case 0: {
			score1 = play_r(board1, depth_search - 1, false);
			score2 = play_r(board1, depth_search - 1, true);
			break;
		}
		case 1: {
			score1 = play_g(board1, depth_search - 1, false);
			score2 = play_g(board1, depth_search - 1, true);
			break;
		}
		case 2: {
			board2 = board1;
			board2.set_accumulator();

			score1 = play_n(board1, board2, depth_search - 1);
			score2 = play_n(board2, board1, depth_search - 1);
			break;
		}
		}

		score1 = score1 > 0 ? 1 : score1 < 0 ? -1 : 0;
		score2 = score2 > 0 ? -1 : score2 < 0 ? 1 : 0;
		if (score1 + score2 == 0) { res[1]++; }
		else if (score1 + score2 > 0) { res[0]++; }
		else { res[2]++; }
		board1.set(startpos_fen);
	}

	res[0] += win;
	res[1] += draw;
	res[2] += loss;
}


double test_net(int threads, int games, int depth_start, int depth_search, int type,
	 Net* n) 
{
	cout << "Testing with " << threads << " Threads, ";
	switch (type) {
	case 0: {
		cout << "vs Random \n";
		break;
	}
	case 1: {
		cout << "vs Greedy \n";
		break;
	}
	case 2: {
		cout << "vs Net \n";
		break;
	}
	}

	int win  = 0;
	int draw = 0;
	int loss = 0;
	int res[32][3] = {};
	thread* t[32];

	system_clock::time_point time_start = system_clock::now();
	milliseconds time = milliseconds(0);

	for (int i = 0; i < threads; i++) {
		int games_ = (i == 0) ? games - (games / threads) * (threads - 1) : (games / threads);
		t[i] = new thread(test_thread, Threads.n, n, type, games_, depth_start, depth_search, res[i]);
	}

	for (int i = 0; i < threads; i++) {
		t[i]->join();
		win += res[i][0];
		draw += res[i][1];
		loss += res[i][2];
	}

	system_clock::time_point time_now = system_clock::now();
	time = duration_cast<milliseconds>(time_now - time_start);

	double elo_max = log10(double(2 * games) - 1) * 400;

	double elo_diff = (win == games) ? elo_max :
		(loss == games) ? -elo_max : log10(double(games) / (0.01 + loss + double(draw) / 2) - 1) * 400;

	cout << "+" << win << " =" << draw << " -" << loss
		<< "\nelo " << int(elo_diff)
		<< "\ntime " << time.count() << "ms" << endl;

	return elo_diff;
}

void test_batch(string dir, int threads, int games, int depth_start, int depth_search) {
	Net n;
	string name[32];
	double elo_[32][32] = {};
	double elo[2][32] = {};

	int end = 0;
	dir = filesystem::current_path().string() + "/" + dir;
	for (const auto& entry : filesystem::directory_iterator(dir)) {
		if (entry.path().extension() != ".bin") { continue; }
		name[end] = entry.path().string();
		if (end++ >= 32) { break; }
	}
	
	for (int i = 0; i < end; i++) {
		cout << "Weight: " << name[i] << '\n';
		load_weights(Threads.n, name[i]);
		for (int opp = i + 1; opp < end; opp++) {
			load_weights(&n, name[opp]);
			double ed = test_net(threads, games, depth_start, depth_search, 2, &n);
			elo_[i][opp] = ed;
			elo_[opp][i] = -ed;
		}
		cout << endl;
	}

	cout << "Elo: \n";

	int idx_curr = 0;
	for (int it = 0; it < 16; it++) {
		for (int i = 0; i < end; i++) {
			elo[idx_curr][i] = 0;
			for (int j = 0; j < end; j++) {
				elo[idx_curr][i] += elo[idx_curr ^ 1][j] + elo_[i][j];
			}
			elo[idx_curr][i] /= end;
		}
		idx_curr ^= 1;
	}

	for (int i = 0; i < end; i++) {
		cout << filesystem::path(name[i]).filename() << ' ' << elo[idx_curr ^ 1][i] << '\n';
	}
	cout << endl;
}