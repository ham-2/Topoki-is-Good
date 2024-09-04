#include <vector>

#ifdef _WIN64
#include <windows.h>
#include <memoryapi.h>
#else
#include <stdlib.h>
#endif

#include "table.h"

using namespace std;

	int num_threads = 1;
	
	void* table_malloc(size_t size) {
#ifdef _WIN64
		return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
		return aligned_alloc(4096, size);
#endif
	}

	void table_free(void* table) {
#ifdef _WIN64
		VirtualFree(table, 0, MEM_RELEASE);
#else
		free(table);
#endif
	}

size_t TT::TT_MB_SIZE = 1;
uint64_t TT::TT_LENGTH = TT::TT_MB_SIZE * 1024 * 1024 / sizeof(TTEntry);

uint64_t SIZE_NUM = (uint64_t)(TT::TT_LENGTH - 1);

TT Main_TT;
int TT::tt_sn;

TT::TT() {
	table = static_cast<TTEntry*>(table_malloc(TT_LENGTH * sizeof(TTEntry)));
}

TT::~TT() {
	table_free(table);
}

int TT::probe(Key key, TTEntry* probe) {
	TTEntry* entry_ptr = table + (key & SIZE_NUM);
	entry_ptr->m.lock();
	if (entry_ptr->key == key) { // Real hit
		memcpy(probe, entry_ptr, sizeof(TTEntry));
		entry_ptr->m.unlock();
		return 0;
	}
	else { // False hit or New node
		entry_ptr->m.unlock();
		return -1;
	}
}

void TT::register_entry(Key key, int depth, int eval, Square move) {
	TTEntry* entry_ptr = table + (key & SIZE_NUM);
	entry_ptr->m.lock();
	if (depth >= entry_ptr->depth ||
		tt_sn > entry_ptr->table_sn) {
		entry_ptr->key = key;
		entry_ptr->depth = depth;
		entry_ptr->eval = eval;
		entry_ptr->nmove = move;
		entry_ptr->table_sn = tt_sn;
	}
	entry_ptr->m.unlock();
	return;
}

TTEntry* TT::backup_entry(Key key) {
	TTEntry* entry_ptr = table + (key & SIZE_NUM);
	TTEntry* temp = new TTEntry;
	entry_ptr->m.lock();
	memcpy(temp, entry_ptr, sizeof(TTEntry));
	entry_ptr->m.unlock();
	temp->m.unlock();
	return temp;
}

void TT::write_entry(TTEntry* ptr) {
	TTEntry* entry_ptr = table + (ptr->key & SIZE_NUM);
	entry_ptr->m.lock();
	ptr->m.lock();
	memcpy(entry_ptr, ptr, sizeof(TTEntry));
	entry_ptr->m.unlock();
}

void TT::clear_entry(Key key) {
	TTEntry* entry_ptr = table + (key & SIZE_NUM);
	entry_ptr->m.lock();
	entry_ptr->key = 0;
	entry_ptr->depth = 0;
	entry_ptr->eval = 0;
	entry_ptr->nmove = Square(0);
	entry_ptr->table_sn = 0;
	entry_ptr->m.unlock();
	return;
}

void TT::clear() {
	tt_sn = 1;
	std::memset(table, 0, TT_LENGTH * sizeof(TTEntry));
}

void TT::change_size(size_t new_size) {
	table_free(table);
	TT_MB_SIZE = new_size;
	TT_LENGTH = TT_MB_SIZE * 1024 * 1024 / sizeof(TTEntry);
	SIZE_NUM = (uint64_t)(TT::TT_LENGTH - 1);
	table = static_cast<TTEntry*>(table_malloc(TT_LENGTH * sizeof(TTEntry)));
}

void getpv(ostream& os, Position* board, int& depth) {
	string pv;
	TTEntry probe = {};
	if (Main_TT.probe(board->get_key(), &probe) == 0 &&
		probe.nmove != GAME_END) {
		Undo u;
		board->do_move_wrap(probe.nmove, &u);
		os << probe.nmove << " ";
		depth++;
		getpv(os, board, depth);
		board->undo_move_wrap(probe.nmove);
	}
}