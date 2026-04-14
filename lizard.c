#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#define BOOL			int
#define TRUE			1
#define FALSE			0
#define S64 signed __int64
#define U64 unsigned __int64
#define bool BOOL
#define true TRUE
#define false FALSE
#define GEN_STACK		1120
#define MAX_PLY			32
#define HIST_STACK		400
#define LIGHT			0
#define DARK			1
#define PAWN			0
#define KNIGHT			1
#define BISHOP			2
#define ROOK			3
#define QUEEN			4
#define KING			5
#define EMPTY			6
#define A1				56
#define B1				57
#define C1				58
#define D1				59
#define E1				60
#define F1				61
#define G1				62
#define H1				63
#define A8				0
#define B8				1
#define C8				2
#define D8				3
#define E8				4
#define F8				5
#define G8				6
#define H8				7
#define MATE 32000
#define NAME "Lizard"
#define VERSION "2026-02-09"
#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define DOUBLED_PAWN_PENALTY		10
#define ISOLATED_PAWN_PENALTY		20
#define BACKWARDS_PAWN_PENALTY		8
#define PASSED_PAWN_BONUS			20
#define ROOK_SEMI_OPEN_FILE_BONUS	10
#define ROOK_OPEN_FILE_BONUS		15
#define ROOK_ON_SEVENTH_BONUS		20
#define ROW(value)	  (value >> 3)
#define COL(value)	  (value & 7)
#define RDEPTH(value) ((value>1500)?(3):(2))

typedef struct {
	char from;
	char to;
	char promote;
	char bits;
} SMove;

typedef union {
	SMove sm;
	int u;
} UMove;

//an element of the move stack. it's just a move with a score, so it can be sorted by the search functions
typedef struct {
	UMove um;
	int value;
} VMove;

//an element of the history stack, with the information necessary to take a move back
typedef struct {
	UMove um;
	int capture;
	int castle;
	int ep;
	int move50;
	U64 hash;
} hist_t;

typedef struct {
	int stop;
	int depthLimit;
	U64 timeStart;
	U64 timeLimit;
	U64 nodes;
	U64 nodesLimit;
}SearchInfo;

int color[64];  /* LIGHT, DARK, or EMPTY */
int piece[64];  /* PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, or EMPTY */
int side;  /* the side to move */
int xside;  /* the side not to move */
int castle;  /* a bitfield with the castle permissions. if 1 is set,
				white can still castle kingside. 2 is white queenside.
				4 is black kingside. 8 is black queenside. */
int ep;  /* the en passant square. if white moves e2e4, the en passant
			square is set to e3, because that's where a pawn would move
			in an en passant capture */
int move50;//the number of moves since a capture or pawn move, used to handle the fifty-move-draw rule
U64 hash;//a (more or less) unique number that corresponds to the position
int ply;//the number of half-moves (ply) since the root of the search tree
int hply;//h for history; the number of ply since the beginning of the game

/* gen_dat is some memory for move lists that are created by the move
   generators. The move list for ply n starts at first_move[n] and ends
   at first_move[n + 1]. */
VMove gen_dat[GEN_STACK];
int first_move[MAX_PLY];

/* the history heuristic array (used for move ordering) */
int history[64][64];

/* we need an array of hist_t's so we can take back the
   moves we make */
hist_t hist_dat[HIST_STACK];

/* a "triangular" PV array; for a good explanation of why a triangular
   array is needed, see "How Computers Play Chess" by Levy and Newborn. */
UMove pv_table[MAX_PLY][MAX_PLY];
int pv_length[MAX_PLY];
BOOL follow_pv;

/* random numbers used to compute hash; see set_hash() in board.c */
U64 hash_piece[2][6][64];  /* indexed by piece [color][type][square] */
U64 hash_side;
U64 hash_ep[64];

int mailbox[120] = {
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1,  0,  1,  2,  3,  4,  5,  6,  7, -1,
	 -1,  8,  9, 10, 11, 12, 13, 14, 15, -1,
	 -1, 16, 17, 18, 19, 20, 21, 22, 23, -1,
	 -1, 24, 25, 26, 27, 28, 29, 30, 31, -1,
	 -1, 32, 33, 34, 35, 36, 37, 38, 39, -1,
	 -1, 40, 41, 42, 43, 44, 45, 46, 47, -1,
	 -1, 48, 49, 50, 51, 52, 53, 54, 55, -1,
	 -1, 56, 57, 58, 59, 60, 61, 62, 63, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

int mailbox64[64] = {
	21, 22, 23, 24, 25, 26, 27, 28,
	31, 32, 33, 34, 35, 36, 37, 38,
	41, 42, 43, 44, 45, 46, 47, 48,
	51, 52, 53, 54, 55, 56, 57, 58,
	61, 62, 63, 64, 65, 66, 67, 68,
	71, 72, 73, 74, 75, 76, 77, 78,
	81, 82, 83, 84, 85, 86, 87, 88,
	91, 92, 93, 94, 95, 96, 97, 98
};

BOOL slide[6] = { FALSE, FALSE, TRUE, TRUE, TRUE, FALSE };

int offsets[6] = { 0, 8, 4, 4, 8, 8 };

int offset[6][8] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	{ -21, -19, -12, -8, 8, 12, 19, 21 },
	{ -11, -9, 9, 11, 0, 0, 0, 0 },
	{ -10, -1, 1, 10, 0, 0, 0, 0 },
	{ -11, -10, -9, -1, 1, 9, 10, 11 },
	{ -11, -10, -9, -1, 1, 9, 10, 11 }
};

int castle_mask[64] = {
	 7, 15, 15, 15,  3, 15, 15, 11,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	13, 15, 15, 15, 12, 15, 15, 14
};

char piece_char[6] = { 'A', 'N', 'B', 'R', 'Q', 'K' };

SearchInfo info;

int piece_value[6] = { 100, 320, 330, 500, 900, 0 };

int pawn_pcsq[64] = {
	  0,   0,   0,   0,   0,   0,   0,   0,
	  5,  10,  15,  20,  20,  15,  10,   5,
	  4,   8,  12,  16,  16,  12,   8,   4,
	  3,   6,   9,  12,  12,   9,   6,   3,
	  2,   4,   6,   8,   8,   6,   4,   2,
	  1,   2,   3, -10, -10,   3,   2,   1,
	  0,   0,   0, -40, -40,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0
};

int knight_pcsq[64] = {
	-10, -10, -10, -10, -10, -10, -10, -10,
	-10,   0,   0,   0,   0,   0,   0, -10,
	-10,   0,   5,   5,   5,   5,   0, -10,
	-10,   0,   5,  10,  10,   5,   0, -10,
	-10,   0,   5,  10,  10,   5,   0, -10,
	-10,   0,   5,   5,   5,   5,   0, -10,
	-10,   0,   0,   0,   0,   0,   0, -10,
	-10, -30, -10, -10, -10, -10, -30, -10
};

int bishop_pcsq[64] = {
	-10, -10, -10, -10, -10, -10, -10, -10,
	-10,   0,   0,   0,   0,   0,   0, -10,
	-10,   0,   5,   5,   5,   5,   0, -10,
	-10,   0,   5,  10,  10,   5,   0, -10,
	-10,   0,   5,  10,  10,   5,   0, -10,
	-10,   0,   5,   5,   5,   5,   0, -10,
	-10,   0,   0,   0,   0,   0,   0, -10,
	-10, -10, -20, -10, -10, -20, -10, -10
};

int king_pcsq[64] = {
	-40, -40, -40, -40, -40, -40, -40, -40,
	-40, -40, -40, -40, -40, -40, -40, -40,
	-40, -40, -40, -40, -40, -40, -40, -40,
	-40, -40, -40, -40, -40, -40, -40, -40,
	-40, -40, -40, -40, -40, -40, -40, -40,
	-40, -40, -40, -40, -40, -40, -40, -40,
	-20, -20, -20, -20, -20, -20, -20, -20,
	  0,  20,  40, -20,   0, -20,  40,  20
};

int king_endgame_pcsq[64] = {
	  0,  10,  20,  30,  30,  20,  10,   0,
	 10,  20,  30,  40,  40,  30,  20,  10,
	 20,  30,  40,  50,  50,  40,  30,  20,
	 30,  40,  50,  60,  60,  50,  40,  30,
	 30,  40,  50,  60,  60,  50,  40,  30,
	 20,  30,  40,  50,  50,  40,  30,  20,
	 10,  20,  30,  40,  40,  30,  20,  10,
	  0,  10,  20,  30,  30,  20,  10,   0
};

int flip[64] = {
	 56,  57,  58,  59,  60,  61,  62,  63,
	 48,  49,  50,  51,  52,  53,  54,  55,
	 40,  41,  42,  43,  44,  45,  46,  47,
	 32,  33,  34,  35,  36,  37,  38,  39,
	 24,  25,  26,  27,  28,  29,  30,  31,
	 16,  17,  18,  19,  20,  21,  22,  23,
	  8,   9,  10,  11,  12,  13,  14,  15,
	  0,   1,   2,   3,   4,   5,   6,   7
};

/* pawn_rank[x][y] is the rank of the least advanced pawn of color x on file
   y - 1. There are "buffer files" on the left and right to avoid special-case
   logic later. If there's no pawn on a rank, we pretend the pawn is
   impossibly far advanced (0 for LIGHT and 7 for DARK). This makes it easy to
   test for pawns on a rank and it simplifies some pawn evaluation code. */
int pawn_rank[2][10];

int piece_mat[2];  /* the value of a side's pieces */
int pawn_mat[2];  /* the value of a side's pawns */

U64 GetTimeMs() {
#ifdef WIN32
	return GetTickCount64();
#else
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec * 1000 + t.tv_usec / 1000;
#endif
}

static char* ParseToken(char* string, char* token) {
	while (*string == ' ')
		string++;
	while (*string != ' ' && *string != '\0')
		*token++ = *string++;
	*token = '\0';
	return string;
}

static int EvalLightPawn(int sq) {
	int r;  /* the value to return */
	int f;  /* the pawn's file */
	r = 0;
	f = COL(sq) + 1;
	r += pawn_pcsq[sq];
	/* if there's a pawn behind this one, it's doubled */
	if (pawn_rank[LIGHT][f] > ROW(sq))
		r -= DOUBLED_PAWN_PENALTY;
	/* if there aren't any friendly pawns on either side of
	   this one, it's isolated */
	if ((pawn_rank[LIGHT][f - 1] == 0) &&
		(pawn_rank[LIGHT][f + 1] == 0))
		r -= ISOLATED_PAWN_PENALTY;
	/* if it's not isolated, it might be backwards */
	else if ((pawn_rank[LIGHT][f - 1] < ROW(sq)) &&
		(pawn_rank[LIGHT][f + 1] < ROW(sq)))
		r -= BACKWARDS_PAWN_PENALTY;
	/* add a bonus if the pawn is passed */
	if ((pawn_rank[DARK][f - 1] >= ROW(sq)) &&
		(pawn_rank[DARK][f] >= ROW(sq)) &&
		(pawn_rank[DARK][f + 1] >= ROW(sq)))
		r += (7 - ROW(sq)) * PASSED_PAWN_BONUS;
	return r;
}

static int EvalDarkPawn(int sq) {
	int r;  /* the value to return */
	int f;  /* the pawn's file */
	r = 0;
	f = COL(sq) + 1;
	r += pawn_pcsq[flip[sq]];
	/* if there's a pawn behind this one, it's doubled */
	if (pawn_rank[DARK][f] < ROW(sq))
		r -= DOUBLED_PAWN_PENALTY;
	/* if there aren't any friendly pawns on either side of
	   this one, it's isolated */
	if ((pawn_rank[DARK][f - 1] == 7) &&
		(pawn_rank[DARK][f + 1] == 7))
		r -= ISOLATED_PAWN_PENALTY;
	/* if it's not isolated, it might be backwards */
	else if ((pawn_rank[DARK][f - 1] > ROW(sq)) &&
		(pawn_rank[DARK][f + 1] > ROW(sq)))
		r -= BACKWARDS_PAWN_PENALTY;
	/* add a bonus if the pawn is passed */
	if ((pawn_rank[LIGHT][f - 1] <= ROW(sq)) &&
		(pawn_rank[LIGHT][f] <= ROW(sq)) &&
		(pawn_rank[LIGHT][f + 1] <= ROW(sq)))
		r += ROW(sq) * PASSED_PAWN_BONUS;
	return r;
}

//evaluates the Light King Pawn on file f
int eval_lkp(int f) {
	int r = 0;
	if (pawn_rank[LIGHT][f] == 6);  /* pawn hasn't moved */
	else if (pawn_rank[LIGHT][f] == 5)
		r -= 10;  /* pawn moved one square */
	else if (pawn_rank[LIGHT][f] != 0)
		r -= 20;  /* pawn moved more than one square */
	else
		r -= 25;  /* no pawn on this file */
	if (pawn_rank[DARK][f] == 7)
		r -= 15;  /* no enemy pawn */
	else if (pawn_rank[DARK][f] == 5)
		r -= 10;  /* enemy pawn on the 3rd rank */
	else if (pawn_rank[DARK][f] == 4)
		r -= 5;   /* enemy pawn on the 4th rank */
	return r;
}

//evaluates the Dark King Pawn on file f
int eval_dkp(int f) {
	int r = 0;
	if (pawn_rank[DARK][f] == 1);
	else if (pawn_rank[DARK][f] == 2)
		r -= 10;
	else if (pawn_rank[DARK][f] != 7)
		r -= 20;
	else
		r -= 25;
	if (pawn_rank[LIGHT][f] == 0)
		r -= 15;
	else if (pawn_rank[LIGHT][f] == 2)
		r -= 10;
	else if (pawn_rank[LIGHT][f] == 3)
		r -= 5;
	return r;
}

int eval_light_king(int sq) {
	int r;  /* the value to return */
	int i;
	r = king_pcsq[sq];
	/* if the king is castled, use a special function to evaluate the
	   pawns on the appropriate side */
	if (COL(sq) < 3) {
		r += eval_lkp(1);
		r += eval_lkp(2);
		r += eval_lkp(3) / 2;  /* problems with pawns on the c & f files
								  are not as severe */
	}
	else if (COL(sq) > 4) {
		r += eval_lkp(8);
		r += eval_lkp(7);
		r += eval_lkp(6) / 2;
	}
	/* otherwise, just assess a penalty if there are open files near
	   the king */
	else {
		for (i = COL(sq); i <= COL(sq) + 2; ++i)
			if ((pawn_rank[LIGHT][i] == 0) &&
				(pawn_rank[DARK][i] == 7))
				r -= 10;
	}
	/* scale the king safety value according to the opponent's material;
	   the premise is that your king safety can only be bad if the
	   opponent has enough pieces to attack you */
	r *= piece_mat[DARK];
	r /= 3100;
	return r;
}

int eval_dark_king(int sq) {
	int r;
	int i;
	r = king_pcsq[flip[sq]];
	if (COL(sq) < 3) {
		r += eval_dkp(1);
		r += eval_dkp(2);
		r += eval_dkp(3) / 2;
	}
	else if (COL(sq) > 4) {
		r += eval_dkp(8);
		r += eval_dkp(7);
		r += eval_dkp(6) / 2;
	}
	else {
		for (i = COL(sq); i <= COL(sq) + 2; ++i)
			if ((pawn_rank[LIGHT][i] == 0) &&
				(pawn_rank[DARK][i] == 7))
				r -= 10;
	}
	r *= piece_mat[LIGHT];
	r /= 3100;
	return r;
}

int EvalPosition() {
	int i;
	int f;  /* file */
	int value[2];  /* each side's score */
	/* this is the first pass: set up pawn_rank, piece_mat, and pawn_mat. */
	for (i = 0; i < 10; ++i) {
		pawn_rank[LIGHT][i] = 0;
		pawn_rank[DARK][i] = 7;
	}
	piece_mat[LIGHT] = 0;
	piece_mat[DARK] = 0;
	pawn_mat[LIGHT] = 0;
	pawn_mat[DARK] = 0;
	for (i = 0; i < 64; ++i) {
		if (color[i] == EMPTY)
			continue;
		if (piece[i] == PAWN) {
			pawn_mat[color[i]] += piece_value[PAWN];
			f = COL(i) + 1;  /* add 1 because of the extra file in the array */
			if (color[i] == LIGHT) {
				if (pawn_rank[LIGHT][f] < ROW(i))
					pawn_rank[LIGHT][f] = ROW(i);
			}
			else {
				if (pawn_rank[DARK][f] > ROW(i))
					pawn_rank[DARK][f] = ROW(i);
			}
		}
		else
			piece_mat[color[i]] += piece_value[piece[i]];
	}

	/* this is the second pass: evaluate each piece */
	value[LIGHT] = piece_mat[LIGHT] + pawn_mat[LIGHT];
	value[DARK] = piece_mat[DARK] + pawn_mat[DARK];
	for (i = 0; i < 64; ++i) {
		if (color[i] == EMPTY)
			continue;
		if (color[i] == LIGHT) {
			switch (piece[i]) {
			case PAWN:
				value[LIGHT] += EvalLightPawn(i);
				break;
			case KNIGHT:
				value[LIGHT] += knight_pcsq[i];
				break;
			case BISHOP:
				value[LIGHT] += bishop_pcsq[i];
				break;
			case ROOK:
				if (pawn_rank[LIGHT][COL(i) + 1] == 0) {
					if (pawn_rank[DARK][COL(i) + 1] == 7)
						value[LIGHT] += ROOK_OPEN_FILE_BONUS;
					else
						value[LIGHT] += ROOK_SEMI_OPEN_FILE_BONUS;
				}
				if (ROW(i) == 1)
					value[LIGHT] += ROOK_ON_SEVENTH_BONUS;
				break;
			case KING:
				if (piece_mat[DARK] <= 1200)
					value[LIGHT] += king_endgame_pcsq[i];
				else
					value[LIGHT] += eval_light_king(i);
				break;
			}
		}
		else {
			switch (piece[i]) {
			case PAWN:
				value[DARK] += EvalDarkPawn(i);
				break;
			case KNIGHT:
				value[DARK] += knight_pcsq[flip[i]];
				break;
			case BISHOP:
				value[DARK] += bishop_pcsq[flip[i]];
				break;
			case ROOK:
				if (pawn_rank[DARK][COL(i) + 1] == 7) {
					if (pawn_rank[LIGHT][COL(i) + 1] == 0)
						value[DARK] += ROOK_OPEN_FILE_BONUS;
					else
						value[DARK] += ROOK_SEMI_OPEN_FILE_BONUS;
				}
				if (ROW(i) == 6)
					value[DARK] += ROOK_ON_SEVENTH_BONUS;
				break;
			case KING:
				if (piece_mat[LIGHT] <= 1200)
					value[DARK] += king_endgame_pcsq[flip[i]];
				else
					value[DARK] += eval_dark_king(i);
				break;
			}
		}
	}
	if (side == LIGHT)
		return value[LIGHT] - value[DARK];
	return ((100 - move50) * (value[DARK] - value[LIGHT])) / 100;
}

void SetFen(const char* s) {
	char ffen[256];
	char fcolor[2];
	char fcastle[5];
	char fep[4];
	sscanf(s, "%s %s %s %s %d", ffen, fcolor, fcastle, fep, &move50);
	int sq = 0;
	for (int i = 0; i < 64; ++i) {
		color[i] = EMPTY;
		piece[i] = EMPTY;
	}
	for (int n = 0; n < strlen(ffen); n++) {
		switch (ffen[n]) {
		case '1': sq += 1; break;
		case '2': sq += 2; break;
		case '3': sq += 3; break;
		case '4': sq += 4; break;
		case '5': sq += 5; break;
		case '6': sq += 6; break;
		case '7': sq += 7; break;
		case '8': sq += 8; break;
		case 'p': color[sq] = DARK; piece[sq] = PAWN;   ++sq; break;
		case 'n': color[sq] = DARK; piece[sq] = KNIGHT; ++sq; break;
		case 'b': color[sq] = DARK; piece[sq] = BISHOP; ++sq; break;
		case 'r': color[sq] = DARK; piece[sq] = ROOK;   ++sq; break;
		case 'q': color[sq] = DARK; piece[sq] = QUEEN;  ++sq; break;
		case 'k': color[sq] = DARK; piece[sq] = KING;   ++sq; break;
		case 'P': color[sq] = LIGHT; piece[sq] = PAWN;   ++sq; break;
		case 'N': color[sq] = LIGHT; piece[sq] = KNIGHT; ++sq; break;
		case 'B': color[sq] = LIGHT; piece[sq] = BISHOP; ++sq; break;
		case 'R': color[sq] = LIGHT; piece[sq] = ROOK;   ++sq; break;
		case 'Q': color[sq] = LIGHT; piece[sq] = QUEEN;  ++sq; break;
		case 'K': color[sq] = LIGHT; piece[sq] = KING;   ++sq; break;
		case '/': break;
		}
	}
	side = -1;
	xside = -1;
	for (int n = 0; n < strlen(fcolor); n++) {
		switch (fcolor[n]) {
		case 'w': side = LIGHT; xside = DARK; break;
		case 'b': side = DARK; xside = LIGHT; break;
		}
	}

	castle = 0;

	for (int n = 0; n < strlen(fcastle); n++) {
		switch (fcastle[n]) {
		case 'K': castle |= 1; break;
		case 'Q': castle |= 2; break;
		case 'k': castle |= 4; break;
		case 'q': castle |= 8; break;
		case '-': break;
		}
	}
	ep = -1;
	if (fep[0] != '-')
		ep = (fep[0] - 'a') + 8 * (7 - fep[1] - '1');
}

//XORs some shifted random numbers together to make sure we have good coverage of all 64 bits
static U64 Rand64() {
	U64 r = rand();
	for (int i = 0; i < 8; i++)
		r ^= ((U64)rand() << (i * 8));
	return r;
}

//init_hash() initializes the random numbers used by set_hash()
void InitHash() {
	int i, j, k;
	srand(0);
	for (i = 0; i < 2; ++i)
		for (j = 0; j < 6; ++j)
			for (k = 0; k < 64; ++k)
				hash_piece[i][j][k] = Rand64();
	hash_side = Rand64();
	for (i = 0; i < 64; ++i)
		hash_ep[i] = Rand64();
}

static void GetHash() {
	int i;
	hash = 0;
	for (i = 0; i < 64; ++i)
		if (color[i] != EMPTY)
			hash ^= hash_piece[color[i]][piece[i]][i];
	if (side == DARK)
		hash ^= hash_side;
	if (ep != -1)
		hash ^= hash_ep[ep];
}

void InitBoard(const char* s) {
	SetFen(s);
	ply = 0;
	hply = 0;
	GetHash();
	first_move[0] = 0;
}

//attack() returns TRUE if square sq is being attacked by side s and FALSE otherwise
BOOL attack(int sq, int s)
{
	int i, j, n;

	for (i = 0; i < 64; ++i)
		if (color[i] == s) {
			if (piece[i] == PAWN) {
				if (s == LIGHT) {
					if (COL(i) != 0 && i - 9 == sq)
						return TRUE;
					if (COL(i) != 7 && i - 7 == sq)
						return TRUE;
				}
				else {
					if (COL(i) != 0 && i + 7 == sq)
						return TRUE;
					if (COL(i) != 7 && i + 9 == sq)
						return TRUE;
				}
			}
			else
				for (j = 0; j < offsets[piece[i]]; ++j)
					for (n = i;;) {
						n = mailbox[mailbox64[n] + offset[piece[i]][j]];
						if (n == -1)
							break;
						if (n == sq)
							return TRUE;
						if (color[n] != EMPTY)
							break;
						if (!slide[piece[i]])
							break;
					}
		}
	return FALSE;
}

BOOL in_check(int s) {
	int i;
	for (i = 0; i < 64; ++i)
		if (piece[i] == KING && color[i] == s)
			return attack(i, s ^ 1);
	return TRUE;
}

//gen_promote() is just like gen_push(), only it puts 4 moves on the move stack, one for each possible promotion piece
void gen_promote(int from, int to, int bits) {
	int i;
	VMove* g;

	for (i = KNIGHT; i <= QUEEN; ++i) {
		g = &gen_dat[first_move[ply + 1]++];
		g->um.sm.from = (char)from;
		g->um.sm.to = (char)to;
		g->um.sm.promote = (char)i;
		g->um.sm.bits = (char)(bits | 32);
		g->value = 1000000 + (i * 10);
	}
}

void gen_push(int from, int to, int bits) {
	VMove* g;
	if (bits & 16) {
		if (side == LIGHT) {
			if (to <= H8) {
				gen_promote(from, to, bits);
				return;
			}
		}
		else {
			if (to >= A1) {
				gen_promote(from, to, bits);
				return;
			}
		}
	}
	g = &gen_dat[first_move[ply + 1]++];
	g->um.sm.from = (char)from;
	g->um.sm.to = (char)to;
	g->um.sm.promote = 0;
	g->um.sm.bits = (char)bits;
	if (color[to] != EMPTY)
		g->value = 1000000 + (piece[to] * 10) - piece[from];
	else
		g->value = history[from][to];
}


//generates pseudo-legal moves for the current position
void gen() {
	int i, j, n;

	/* so far, we have no moves for the current ply */
	first_move[ply + 1] = first_move[ply];

	for (i = 0; i < 64; ++i)
		if (color[i] == side) {
			if (piece[i] == PAWN) {
				if (side == LIGHT) {
					if (COL(i) != 0 && color[i - 9] == DARK)
						gen_push(i, i - 9, 17);
					if (COL(i) != 7 && color[i - 7] == DARK)
						gen_push(i, i - 7, 17);
					if (color[i - 8] == EMPTY) {
						gen_push(i, i - 8, 16);
						if (i >= 48 && color[i - 16] == EMPTY)
							gen_push(i, i - 16, 24);
					}
				}
				else {
					if (COL(i) != 0 && color[i + 7] == LIGHT)
						gen_push(i, i + 7, 17);
					if (COL(i) != 7 && color[i + 9] == LIGHT)
						gen_push(i, i + 9, 17);
					if (color[i + 8] == EMPTY) {
						gen_push(i, i + 8, 16);
						if (i <= 15 && color[i + 16] == EMPTY)
							gen_push(i, i + 16, 24);
					}
				}
			}
			else
				for (j = 0; j < offsets[piece[i]]; ++j)
					for (n = i;;) {
						n = mailbox[mailbox64[n] + offset[piece[i]][j]];
						if (n == -1)
							break;
						if (color[n] != EMPTY) {
							if (color[n] == xside)
								gen_push(i, n, 1);
							break;
						}
						gen_push(i, n, 0);
						if (!slide[piece[i]])
							break;
					}
		}

	/* generate castle moves */
	if (side == LIGHT) {
		if (castle & 1)
			gen_push(E1, G1, 2);
		if (castle & 2)
			gen_push(E1, C1, 2);
	}
	else {
		if (castle & 4)
			gen_push(E8, G8, 2);
		if (castle & 8)
			gen_push(E8, C8, 2);
	}

	/* generate en passant moves */
	if (ep != -1) {
		if (side == LIGHT) {
			if (COL(ep) != 0 && color[ep + 7] == LIGHT && piece[ep + 7] == PAWN)
				gen_push(ep + 7, ep, 21);
			if (COL(ep) != 7 && color[ep + 9] == LIGHT && piece[ep + 9] == PAWN)
				gen_push(ep + 9, ep, 21);
		}
		else {
			if (COL(ep) != 0 && color[ep - 9] == DARK && piece[ep - 9] == PAWN)
				gen_push(ep - 9, ep, 21);
			if (COL(ep) != 7 && color[ep - 7] == DARK && piece[ep - 7] == PAWN)
				gen_push(ep - 7, ep, 21);
		}
	}
}


/* gen_caps() is basically a copy of gen() that's modified to
   only generate capture and promote moves. It's used by the
   quiescence search. */
void gen_caps()
{
	int i, j, n;

	first_move[ply + 1] = first_move[ply];
	for (i = 0; i < 64; ++i)
		if (color[i] == side) {
			if (piece[i] == PAWN) {
				if (side == LIGHT) {
					if (COL(i) != 0 && color[i - 9] == DARK)
						gen_push(i, i - 9, 17);
					if (COL(i) != 7 && color[i - 7] == DARK)
						gen_push(i, i - 7, 17);
					if (i <= 15 && color[i - 8] == EMPTY)
						gen_push(i, i - 8, 16);
				}
				if (side == DARK) {
					if (COL(i) != 0 && color[i + 7] == LIGHT)
						gen_push(i, i + 7, 17);
					if (COL(i) != 7 && color[i + 9] == LIGHT)
						gen_push(i, i + 9, 17);
					if (i >= 48 && color[i + 8] == EMPTY)
						gen_push(i, i + 8, 16);
				}
			}
			else
				for (j = 0; j < offsets[piece[i]]; ++j)
					for (n = i;;) {
						n = mailbox[mailbox64[n] + offset[piece[i]][j]];
						if (n == -1)
							break;
						if (color[n] != EMPTY) {
							if (color[n] == xside)
								gen_push(i, n, 1);
							break;
						}
						if (!slide[piece[i]])
							break;
					}
		}
	if (ep != -1) {
		if (side == LIGHT) {
			if (COL(ep) != 0 && color[ep + 7] == LIGHT && piece[ep + 7] == PAWN)
				gen_push(ep + 7, ep, 21);
			if (COL(ep) != 7 && color[ep + 9] == LIGHT && piece[ep + 9] == PAWN)
				gen_push(ep + 9, ep, 21);
		}
		else {
			if (COL(ep) != 0 && color[ep - 9] == DARK && piece[ep - 9] == PAWN)
				gen_push(ep - 9, ep, 21);
			if (COL(ep) != 7 && color[ep - 7] == DARK && piece[ep - 7] == PAWN)
				gen_push(ep - 7, ep, 21);
		}
	}
}

//takeback() is very similar to makemove(), only backwards
void takeback() {
	SMove um;
	side ^= 1;
	xside ^= 1;
	--ply;
	--hply;
	um = hist_dat[hply].um.sm;
	castle = hist_dat[hply].castle;
	ep = hist_dat[hply].ep;
	move50 = hist_dat[hply].move50;
	hash = hist_dat[hply].hash;
	color[(int)um.from] = side;
	if (um.bits & 32)
		piece[(int)um.from] = PAWN;
	else
		piece[(int)um.from] = piece[(int)um.to];
	if (hist_dat[hply].capture == EMPTY) {
		color[(int)um.to] = EMPTY;
		piece[(int)um.to] = EMPTY;
	}
	else {
		color[(int)um.to] = xside;
		piece[(int)um.to] = hist_dat[hply].capture;
	}
	if (um.bits & 2) {
		int from, to;

		switch (um.to) {
		case 62:
			from = F1;
			to = H1;
			break;
		case 58:
			from = D1;
			to = A1;
			break;
		case 6:
			from = F8;
			to = H8;
			break;
		case 2:
			from = D8;
			to = A8;
			break;
		default:  /* shouldn't get here */
			from = -1;
			to = -1;
			break;
		}
		color[to] = side;
		piece[to] = ROOK;
		color[from] = EMPTY;
		piece[from] = EMPTY;
	}
	if (um.bits & 4) {
		if (side == LIGHT) {
			color[um.to + 8] = xside;
			piece[um.to + 8] = PAWN;
		}
		else {
			color[um.to - 8] = xside;
			piece[um.to - 8] = PAWN;
		}
	}
}

//makemove() makes a move. If the move is illegal, it undoes whatever it did and returns FALSE. Otherwise, it returns TRUE
BOOL makemove(SMove um) {

	/* test to see if a castle move is legal and move the rook
	   (the king is moved with the usual move code later) */
	if (um.bits & 2) {
		int from, to;

		if (in_check(side))
			return FALSE;
		switch (um.to) {
		case 62:
			if (color[F1] != EMPTY || color[G1] != EMPTY ||
				attack(F1, xside) || attack(G1, xside))
				return FALSE;
			from = H1;
			to = F1;
			break;
		case 58:
			if (color[B1] != EMPTY || color[C1] != EMPTY || color[D1] != EMPTY ||
				attack(C1, xside) || attack(D1, xside))
				return FALSE;
			from = A1;
			to = D1;
			break;
		case 6:
			if (color[F8] != EMPTY || color[G8] != EMPTY ||
				attack(F8, xside) || attack(G8, xside))
				return FALSE;
			from = H8;
			to = F8;
			break;
		case 2:
			if (color[B8] != EMPTY || color[C8] != EMPTY || color[D8] != EMPTY ||
				attack(C8, xside) || attack(D8, xside))
				return FALSE;
			from = A8;
			to = D8;
			break;
		default:  /* shouldn't get here */
			from = -1;
			to = -1;
			break;
		}
		color[to] = color[from];
		piece[to] = piece[from];
		color[from] = EMPTY;
		piece[from] = EMPTY;
	}

	/* back up information so we can take the move back later. */
	hist_dat[hply].um.sm = um;
	hist_dat[hply].capture = piece[(int)um.to];
	hist_dat[hply].castle = castle;
	hist_dat[hply].ep = ep;
	hist_dat[hply].move50 = move50;
	hist_dat[hply].hash = hash;
	++ply;
	++hply;

	/* update the castle, en passant, and
	   fifty-move-draw variables */
	castle &= castle_mask[(int)um.from] & castle_mask[(int)um.to];
	if (um.bits & 8) {
		if (side == LIGHT)
			ep = um.to + 8;
		else
			ep = um.to - 8;
	}
	else
		ep = -1;
	if (um.bits & 17)
		move50 = 0;
	else
		++move50;

	/* move the piece */
	color[(int)um.to] = side;
	if (um.bits & 32)
		piece[(int)um.to] = um.promote;
	else
		piece[(int)um.to] = piece[(int)um.from];
	color[(int)um.from] = EMPTY;
	piece[(int)um.from] = EMPTY;

	/* erase the pawn if this is an en passant move */
	if (um.bits & 4) {
		if (side == LIGHT) {
			color[um.to + 8] = EMPTY;
			piece[um.to + 8] = EMPTY;
		}
		else {
			color[um.to - 8] = EMPTY;
			piece[um.to - 8] = EMPTY;
		}
	}

	/* switch sides and test for legality (if we can capture
	   the other guy's king, it's an illegal position and
	   we need to take the move back) */
	side ^= 1;
	xside ^= 1;
	if (in_check(xside)) {
		takeback();
		return FALSE;
	}
	GetHash();
	return TRUE;
}

//Engine MOve TO Uci MOve
char* EmoToUmo(SMove um)
{
	static char str[6];
	char c;
	if (um.bits & 32) {
		switch (um.promote) {
		case KNIGHT:
			c = 'n';
			break;
		case BISHOP:
			c = 'b';
			break;
		case ROOK:
			c = 'r';
			break;
		default:
			c = 'q';
			break;
		}
		sprintf(str, "%c%d%c%d%c",
			COL(um.from) + 'a',
			8 - ROW(um.from),
			COL(um.to) + 'a',
			8 - ROW(um.to),
			c);
	}
	else
		sprintf(str, "%c%d%c%d",
			COL(um.from) + 'a',
			8 - ROW(um.from),
			COL(um.to) + 'a',
			8 - ROW(um.to));
	return str;
}

//parse the move s (in coordinate notation) and return the move's index in gen_dat, or -1 if the move is illegal or -2 if unknow command.
int ParseMove(char* s) {
	int from, to, i;
	if (s[0] < 'a' || s[0] > 'h' ||
		s[1] < '0' || s[1] > '9' ||
		s[2] < 'a' || s[2] > 'h' ||
		s[3] < '0' || s[3] > '9')
		return -2;
	from = s[0] - 'a';
	from += 8 * (8 - (s[1] - '0'));
	to = s[2] - 'a';
	to += 8 * (8 - (s[3] - '0'));
	for (i = 0; i < first_move[1]; ++i)
		if (gen_dat[i].um.sm.from == from && gen_dat[i].um.sm.to == to) {
			if (gen_dat[i].um.sm.bits & 32)
				switch (s[4]) {
				case 'N':
				case 'n':
					return i;
				case 'B':
				case 'b':
					return i + 1;
				case 'R':
				case 'r':
					return i + 2;
				default:
					return i + 3;
				}
			return i;
		}
	return -1;
}

//is called once in a while during the search.
int CheckUp() {
	if (!(++info.nodes & 0xffff)) {
		if (info.timeLimit && GetTimeMs() - info.timeStart > info.timeLimit)
			info.stop = TRUE;
		if (info.nodesLimit && info.nodes > info.nodesLimit)
			info.stop = TRUE;
	}
	return info.stop;
}

void SortPv() {
	int i;
	follow_pv = FALSE;
	for (i = first_move[ply]; i < first_move[ply + 1]; ++i)
		if (gen_dat[i].um.u == pv_table[0][ply].u) {
			follow_pv = TRUE;
			gen_dat[i].value += 10000000;
			return;
		}
}

//searches the current ply's move list from 'from' to the end to find the move with the highest score
void sort(int from) {
	int i;
	int bs;  /* best score */
	int bi;  /* best i */
	VMove g;
	bs = -1;
	bi = from;
	for (i = from; i < first_move[ply + 1]; ++i)
		if (gen_dat[i].value > bs) {
			bs = gen_dat[i].value;
			bi = i;
		}
	g = gen_dat[from];
	gen_dat[from] = gen_dat[bi];
	gen_dat[bi] = g;
}

//returns the number of times the current position has been repeated
static int Reps() {
	int r = 0;
	for (int i = hply - move50; i < hply; ++i)
		if (hist_dat[i].hash == hash)
			++r;
	return r;
}

int SearchQuiescence(int alpha, int beta) {
	int i, j, value;
	if (CheckUp())
		return 0;

	pv_length[ply] = ply;

	/* are we too deep? */
	if (ply >= MAX_PLY - 1)
		return EvalPosition();
	if (hply >= HIST_STACK - 1)
		return EvalPosition();

	/* check with the evaluation function */
	value = EvalPosition();
	if (value >= beta)
		return beta;
	if (value > alpha)
		alpha = value;

	gen_caps();
	if (follow_pv)  /* are we following the PV? */
		SortPv();

	/* loop through the moves */
	for (i = first_move[ply]; i < first_move[ply + 1]; ++i) {
		sort(i);
		if (!makemove(gen_dat[i].um.sm))
			continue;
		value = -SearchQuiescence(-beta, -alpha);
		takeback();
		if (info.stop)
			return 0;
		if (alpha < value) {
			alpha = value;
			if (alpha >= beta)
				return beta;

			/* update the PV */
			pv_table[ply][ply] = gen_dat[i].um;
			for (j = ply + 1; j < pv_length[ply + 1]; ++j)
				pv_table[ply][j] = pv_table[ply + 1][j];
			pv_length[ply] = pv_length[ply + 1];
		}
	}
	return alpha;
}

int SearchAlpha(int alpha, int beta, int depth, int null_move) {
	int i, j; //x;
	int nullmat;
	int o_side;
	int o_xside;
	int o_ep;
	int o_fifty;
	U64 o_hash;
	int o_castle;

	BOOL inCheck = in_check(side);
	if (inCheck)
		++depth;
	if (depth < 1)
		return SearchQuiescence(alpha, beta);
	if (CheckUp())
		return 0;
	pv_length[ply] = ply;

	/* if this isn't the root of the search tree (where we have
	   to pick a move and can't simply return 0) then check to
	   see if the position is a repeat. if so, we can assume that
	   this line is a draw and return 0. */
	if (ply)
		if (move50 >= 100 || Reps())
			return 0;

	/* are we too deep? */
	if (ply >= MAX_PLY - 1)
		return EvalPosition();
	if (hply >= HIST_STACK - 1)
		return EvalPosition();

	/* null move */
	if (null_move && !inCheck && ply) {
		nullmat = 0;
		for (i = 0; i < 64; ++i) {
			if (piece[i] != EMPTY && piece[i] != PAWN && color[i] == side) {
				nullmat += piece_value[piece[i]];
			}
		}
		if (depth > RDEPTH(nullmat)) {
			o_side = side;
			o_xside = xside;
			o_ep = ep;
			o_fifty = move50;
			o_hash = hash;
			o_castle = castle;
			ep = -1;
			move50 = 0;
			side = xside;
			xside = o_side;
			int value = -SearchAlpha(-beta, -beta + 1, depth - 1 - RDEPTH(nullmat), 0);
			side = o_side;
			xside = o_xside;
			ep = o_ep;
			move50 = o_fifty;
			hash = o_hash;
			castle = o_castle;
			if (info.stop) {
				return 0;
			}
			if (value >= beta)
				return beta;
		}
	}

	gen();
	if (follow_pv)  /* are we following the PV? */
		SortPv();
	int legalMoves = 0;

	/* loop through the moves */
	for (i = first_move[ply]; i < first_move[ply + 1]; ++i) {
		sort(i);
		if (!makemove(gen_dat[i].um.sm))
			continue;
		int value = -SearchAlpha(-beta, -alpha, depth - 1, 1);
		takeback();
		if (info.stop)
			return 0;
		legalMoves++;
		if (alpha < value) {
			history[(int)gen_dat[i].um.sm.from][(int)gen_dat[i].um.sm.to] += depth;

			alpha = value;
			if (alpha >= beta)
				return beta;

			pv_table[ply][ply] = gen_dat[i].um;
			for (j = ply + 1; j < pv_length[ply + 1]; ++j)
				pv_table[ply][j] = pv_table[ply + 1][j];
			pv_length[ply] = pv_length[ply + 1];

			if (!ply) {
				printf("info depth %d score ", depth);
				if (abs(value) < MATE - MAX_PLY)
					printf("cp %d", value);
				else
					printf("mate %d", (value > 0 ? (MATE - value + 1) >> 1 : -(MATE + value) >> 1));
				printf(" time %lld", GetTimeMs() - info.timeStart);
				printf(" nodes %lld pv", info.nodes);
				for (j = 0; j < pv_length[0]; ++j)
					printf(" %s", EmoToUmo(pv_table[0][j].sm));
				printf("\n");
			}
		}
	}
	if (!legalMoves)
		return inCheck ? ply - MATE : 0;
	return alpha;
}

void SearchIterate() {
	int i, value;
	ply = 0;
	memset(pv_length, 0, sizeof(pv_length));
	memset(pv_table, 0, sizeof(pv_table));
	memset(history, 0, sizeof(history));
	for (i = 1; i <= info.depthLimit; ++i) {
		follow_pv = TRUE;
		value = SearchAlpha(-MATE, MATE, i, 1);
		if (info.stop)
			break;
	}
	printf("bestmove %s\n", EmoToUmo(pv_table[0][0].sm));
	fflush(stdout);
}

//prints the board
void PrintBoard() {
	const char* s = "   +---+---+---+---+---+---+---+---+\n";
	const char* t = "     A   B   C   D   E   F   G   H\n";
	printf(t);
	for (int r = 0; r < 8; r++) {
		printf(s);
		printf(" %d |", 8 - r);
		for (int f = 0; f < 8; f++) {
			int i = r * 8 + f;
			switch (color[i]) {
			case EMPTY:
				printf("   |");
				break;
			case LIGHT:
				printf(" %c |", piece_char[piece[i]]);
				break;
			case DARK:
				printf(" %c |", piece_char[piece[i]] + ('a' - 'A'));
				break;
			}
		}
		printf(" %d \n", 8 - r);
	}
	printf(s);
	printf(t);
}


static void ParsePosition(char* ptr) {
	int um;
	char token[80], fen[80];
	ptr = ParseToken(ptr, token);
	if (strcmp(token, "fen") == 0) {
		fen[0] = '\0';
		for (;;) {
			ptr = ParseToken(ptr, token);
			if (*token == '\0' || strcmp(token, "moves") == 0)
				break;
			strcat(fen, token);
			strcat(fen, " ");
		}
		InitBoard(fen);
	}
	else {
		ptr = ParseToken(ptr, token);
		InitBoard(START_FEN);
	}
	ply = 0;
	gen();
	if (strcmp(token, "moves") == 0)
		for (;;) {
			ptr = ParseToken(ptr, token);
			if (*token == '\0')
				break;
			um = ParseMove(token);
			if (um < 0 || !makemove(gen_dat[um].um.sm))
				printf("Illegal move (%s).\n", token);
			ply = 0;
			gen();
		}
}

static void ParseGo(char* ptr) {
	info.stop = FALSE;
	info.nodes = 0;
	info.depthLimit = 64;
	info.nodesLimit = 0;
	info.timeLimit = 0;
	info.timeStart = GetTimeMs();
	char token[80];
	int wtime = 0;
	int btime = 0;
	int winc = 0;
	int binc = 0;
	int movestogo = 32;
	for (;;) {
		ptr = ParseToken(ptr, token);
		if (*token == '\0')
			break;
		else if (strcmp(token, "wtime") == 0) {
			ptr = ParseToken(ptr, token);
			wtime = atoi(token);
		}
		else if (strcmp(token, "btime") == 0) {
			ptr = ParseToken(ptr, token);
			btime = atoi(token);
		}
		else if (strcmp(token, "winc") == 0) {
			ptr = ParseToken(ptr, token);
			winc = atoi(token);
		}
		else if (strcmp(token, "binc") == 0) {
			ptr = ParseToken(ptr, token);
			binc = atoi(token);
		}
		else if (strcmp(token, "movestogo") == 0) {
			ptr = ParseToken(ptr, token);
			movestogo = atoi(token);
		}
		else if (strcmp(token, "movetime") == 0) {
			ptr = ParseToken(ptr, token);
			info.timeLimit = atoi(token);
		}
		else if (strcmp(token, "depth") == 0) {
			ptr = ParseToken(ptr, token);
			info.depthLimit = atoi(token);
		}
		else if (strcmp(token, "nodes") == 0) {
			ptr = ParseToken(ptr, token);
			info.nodesLimit = atoi(token);
		}
	}
	int time = side ? btime : wtime;
	int inc = side ? binc : winc;
	if (time)
		info.timeLimit = min(time / movestogo + inc, time / 2);
	SearchIterate();
}

static void UciCommand(char* command) {
	char token[80], * ptr;
	ptr = ParseToken(command, token);
	if (strncmp(token, "ucinewgame", 10) == 0) {}
	else if (strncmp(token, "uci", 3) == 0) {
		printf("id name %s\n", NAME);
		printf("uciok\n");
		fflush(stdout);
	}
	else if (strncmp(token, "isready", 7) == 0) {
		printf("readyok\n");
		fflush(stdout);
	}
	else if (strncmp(token, "position", 8) == 0)
		ParsePosition(ptr);
	else if (strncmp(token, "go", 2) == 0)
		ParseGo(ptr);
	else if (strncmp(token, "quit", 4) == 0)
		exit(0);
	else if (strncmp(token, "print", 5) == 0)
		PrintBoard();
}

static void UciLoop() {
	char line[4000];
	while (fgets(line, sizeof(line), stdin))
		UciCommand(line);
}

int main() {
	printf("%s %s\n", NAME, VERSION);
	InitHash();
	InitBoard(START_FEN);
	UciLoop();
	return 0;
}
