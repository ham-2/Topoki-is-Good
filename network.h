#ifndef NETWORK_INCLUDED
#define NETWORK_INCLUDED

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#ifdef _WIN64
#include <direct.h>
inline std::string getcwd_wrap(char* dst, int bytes) { 
	_getcwd(dst, bytes);
	return std::string(dst) + "\\";
}
#else
#include <unistd.h>
inline std::string getcwd_wrap(char* dst, int bytes) { 
	getcwd(dst, bytes);
	return std::string(dst) + "/";
}
#endif

#include "board.h"
#include "misc.h"

constexpr int SIZE_F0 = 128 + 64 * 4 * 8;
constexpr int SIZE_F1 = 32;
constexpr int SIZE_F2 = 32;
constexpr int SIZE_F3 = 32;

constexpr int SHIFT_L0 = 5;
constexpr int SHIFT_L1 = 5;

constexpr int EVAL_BITS = 16;

struct Net {

	alignas(32) 
	int8_t L0_a[SIZE_F0 * SIZE_F1];
	int16_t L0_b[SIZE_F1];

	int8_t L1_a[SIZE_F1 * SIZE_F2];
	int16_t L1_b[SIZE_F2];

	int8_t L2_a[SIZE_F2 * SIZE_F3];
	int16_t L2_b[SIZE_F3];

	int16_t L3_a[SIZE_F3];
	int32_t L3_b;

};

inline void zero_weights(Net* net) { memset(net, 0, sizeof(Net)); }
void rand_weights(Net* net, int bits);
void set_weights(Net* net);

void load_weights(Net* net, std::string filename);
void save_weights(Net* net, std::string filename);

inline int _get_pair(Piece p1, Piece p2, Color c);
void compute_L0(int16_t* dst, Piece* squares, Bitboard* pieces, Net* n);
void update_L0(int16_t* dst, Square s, Piece* squares, Piece to, Net* n);
void ReLUClip_L0(int16_t* dst, int16_t* src, Color side_to_move);
void ReLUClip_L1(int16_t* dst, int16_t* src);
void ReLUClip_L2(int16_t* dst, int16_t* src);
void compute_layer(int16_t* dst, int16_t* src, int8_t* a, int16_t* b);
void compute_L3(int64_t* dst, int16_t* src, Net* n);

int compute(int16_t* src, Net* n, Color side_to_move);

void verify_SIMD(Net* n);

#endif