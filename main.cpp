#include <iostream>
#include <sstream>
#include <string>
#include <chrono>

#include "options.h"
#include "board.h"
#include "benchmark.h"
#include "tune.h"
#include "movegen.h"
#include "search.h"
#include "threads.h"
#include "network.h"

#include <bitset>

using namespace std;
using namespace std::chrono;

int main() {

	string input, word;

	system_clock::time_point time_start = system_clock::now();
	milliseconds startup_time = milliseconds(0);
	
	Board::init();
	Position::init();
	Threads.init();

	system_clock::time_point time_now = system_clock::now();
	startup_time = duration_cast<milliseconds>(time_now - time_start);
	cout << nounitbuf << "\nAGIOthello alpha\n" 
		<< "Startup took " << startup_time.count() << "ms\n" << endl;

	while (true) {
		getline(cin, input);

		istringstream ss(input);
		word.clear();
		ss >> skipws >> word;

		if (word == "quit") { break; }

		else if (word == "uci") {
			cout << "id name AGIOthello\n"
				<< "id author Seungrae Kim" << endl;
			// Options
			print_option();
			Threads.acquire_cout();
			cout << "uciok" << endl;
			Threads.release_cout();
		}

		else if (word == "isready") {
			Threads.stop = true;
			Threads.sync();
			Threads.acquire_cout();
			cout << "readyok" << endl;
			Threads.release_cout();
		}

		else if (word == "position") {
			ss >> word;
			string fen;
			if (word == "fen") {
				while (ss >> word && word != "moves") {
					fen += word + " ";
				}
			}
			else if (word == "startpos") {
				fen = startpos_fen;
				ss >> word; // "moves"
			}

			Threads.stop = true;
			Threads.acquire_lock();
			Threads.set_all(fen);
			if (word == "moves") {
				while (ss >> word) {
					Threads.do_move(word);
				}
			}
			Threads.release_lock();
		}

		else if (word == "go") {
			Color c = Threads.get_color();
			float time;
			int max_ply;
			get_time(ss, c, time, max_ply);
			thread t = thread(search_start,
				Threads.threads[0], time, max_ply);
			t.detach();
		}

		else if (word == "stop") {
			Threads.stop = true;
		}

		else if (word == "setoption") {
			set_option(ss);
		}

		// Commands related to weight

		else if (word == "load") {
			Threads.acquire_lock();
			ss >> word;
			load_weights(Threads.n, word);
			Threads.set_weights();
			Threads.release_lock();
		}

		else if (word == "save") {
			ss >> word;
			save_weights(Threads.n, word);
		}

		else if (word == "zero") {
			zero_weights(Threads.n);
			Threads.set_weights();
		}

		else if (word == "rand") {
			ss >> word;
			rand_weights(Threads.n, stoi(word));
			Threads.set_weights();
		}

		else if (word == "setmat") {
			set_weights(Threads.n);
			Threads.set_weights();
		}

		else if (word == "tune") {
			ss >> word;
			int threads = stoi(word);

			ss >> word;
			int64_t games = stoll(word);

			ss >> word;
			int find_depth = stoi(word);

			ss >> word;
			double lr = stod(word);

			thread t = thread(do_learning,
				Threads.n, Threads.n,
				games, threads, find_depth, lr);
			t.detach();
		}

		// Commands for debugging

		else if (word == "moves") {
			Threads.acquire_lock();
			while (ss >> word) {
				Threads.do_move(word);
			}
			Threads.release_lock();
		}

		else if (word == "undo") {
			Threads.acquire_lock();
			while (ss >> word) {
				Threads.undo_move();
			}
			Threads.release_lock();
		}

		else if (word == "showboard") {
			int threadidx;
			threadidx = ss >> threadidx ? threadidx : 0;
			Threads.show(threadidx);
		}

		else if (word == "eval") {
			ss >> word;
			Threads.test_eval();
		}

		else if (word == "perft") {
			int depth;
			ss >> depth;
			Threads.acquire_lock();
			perft(Threads.board, depth);
			Threads.release_lock();
		}

		else if (word == "verify") {
			Threads.board->verify();
		}

		else if (word == "solve") {
			solve();
		}

		else if (word == "nettest") {
			net_verify();
			net_speedtest();
		}

		else if (word == "compute") {
			int16_t acct[32] = { };
			for (int i = 0; i < 32; i++) {
				ss >> acct[i];
			}
			cout << compute(acct, Threads.n, BLACK) << endl;
		}

	}

	return 44;
}
