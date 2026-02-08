#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <windows.h>
#include "main.h"

/* the board representation */
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
int fifty;  /* the number of moves since a capture or pawn move, used
			   to handle the fifty-move-draw rule */
int hash;  /* a (more or less) unique number that corresponds to the
			  position */
int ply;//the number of half-moves (ply) since the root of the search tree
int hply;//h for history; the number of ply since the beginning of the game

/* gen_dat is some memory for move lists that are created by the move
   generators. The move list for ply n starts at first_move[n] and ends
   at first_move[n + 1]. */
gen_t gen_dat[GEN_STACK];
int first_move[MAX_PLY];

/* the history heuristic array (used for move ordering) */
int history[64][64];

/* we need an array of hist_t's so we can take back the
   moves we make */
hist_t hist_dat[HIST_STACK];

/* a "triangular" PV array; for a good explanation of why a triangular
   array is needed, see "How Computers Play Chess" by Levy and Newborn. */
move pv[MAX_PLY][MAX_PLY];
int pv_length[MAX_PLY];
BOOL follow_pv;

/* random numbers used to compute hash; see set_hash() in board.c */
int hash_piece[2][6][64];  /* indexed by piece [color][type][square] */
int hash_side;
int hash_ep[64];

/* Now we have the mailbox array, so called because it looks like a
   mailbox, at least according to Bob Hyatt. This is useful when we
   need to figure out what pieces can go where. Let's say we have a
   rook on square a4 (32) and we want to know if it can move one
   square to the left. We subtract 1, and we get 31 (h5). The rook
   obviously can't move to h5, but we don't know that without doing
   a lot of annoying work. Sooooo, what we do is figure out a4's
   mailbox number, which is 61. Then we subtract 1 from 61 (60) and
   see what mailbox[60] is. In this case, it's -1, so it's out of
   bounds and we can forget it. You can see how mailbox[] is used
   in attack() in board.c. */

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


/* slide, offsets, and offset are basically the vectors that
   pieces can move in. If slide for the piece is FALSE, it can
   only move one square in any one direction. offsets is the
   number of directions it can move in, and offset is an array
   of the actual directions. */

BOOL slide[6] = {
	FALSE, FALSE, TRUE, TRUE, TRUE, FALSE
};

int offsets[6] = {
	0, 8, 4, 4, 8, 8
};

int offset[6][8] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	{ -21, -19, -12, -8, 8, 12, 19, 21 },
	{ -11, -9, 9, 11, 0, 0, 0, 0 },
	{ -10, -1, 1, 10, 0, 0, 0, 0 },
	{ -11, -10, -9, -1, 1, 9, 10, 11 },
	{ -11, -10, -9, -1, 1, 9, 10, 11 }
};


/* This is the castle_mask array. We can use it to determine
   the castling permissions after a move. What we do is
   logical-AND the castle bits with the castle_mask bits for
   both of the move's squares. Let's say castle is 1, meaning
   that white can still castle kingside. Now we play a move
   where the rook on h1 gets captured. We AND castle with
   castle_mask[63], so we have 1&14, and castle becomes 0 and
   white can't castle kingside anymore. */

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


/* the piece letters, for print_board() */
char piece_char[6] = {
	'A', 'N', 'B', 'R', 'Q', 'K'
};

const char* square_name[64] = {
"a8","b8","c8","d8","e8","f8","g8","h8",
"a7","b7","c7","d7","e7","f7","g7","h7",
"a6","b6","c6","d6","e6","f6","g6","h6",
"a5","b5","c5","d5","e5","f5","g5","h5",
"a4","b4","c4","d4","e4","f4","g4","h4",
"a3","b3","c3","d3","e3","f3","g3","h3",
"a2","b2","c2","d2","e2","f2","g2","h2",
"a1","b1","c1","d1","e1","f1","g1","h1"
};

SearchInfo info;

U64 GetTimeMs() {
#ifdef WIN32
	return GetTickCount64();
#else
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec * 1000 + t.tv_usec / 1000;
#endif
}

static void ReadLine(char* str, int mc) {
	char* ptr;
	if (fgets(str, mc, stdin) == NULL)
		exit(0);
	if ((ptr = strchr(str, '\n')) != NULL)
		*ptr = '\0';
}

static char* ParseToken(char* string, char* token)
{
	while (*string == ' ')
		string++;
	while (*string != ' ' && *string != '\0')
		*token++ = *string++;
	*token = '\0';
	return string;
}

void SetFen(const char* s)
{
	int i;
	int z;
	int a = 0;
	int sq = 0;
	int n = (int)strlen(s);

	for (i = 0; i < 64; ++i) {
		color[i] = EMPTY;
		piece[i] = EMPTY;
	}

	for (i = 0, z = 0; i < n && z == 0; ++i) {
		switch (s[i]) {
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
		default: z = 1; break;
		}
		a = i;
	}

	side = -1;
	xside = -1;

	++a;

	for (i = a, z = 0; i < n && z == 0; ++i) {
		switch (s[i]) {
		case 'w': side = LIGHT; xside = DARK; break;
		case 'b': side = DARK; xside = LIGHT; break;
		default: z = 1; break;
		}
		a = i;
	}

	castle = 0;

	for (i = a + 1, z = 0; i < n && z == 0; ++i) {
		switch (s[i]) {
		case 'K': castle |= 1; break;
		case 'Q': castle |= 2; break;
		case 'k': castle |= 4; break;
		case 'q': castle |= 8; break;
		case '-': break;
		default: z = 1; break;
		}
		a = i;
	}

	ep = -1;

	for (i = a + 1, z = 0; i < n && z == 0; ++i) {
		switch (s[i]) {
		case '-': break;
		case 'a': ep = 0; break;
		case 'b': ep = 1; break;
		case 'c': ep = 2; break;
		case 'd': ep = 3; break;
		case 'e': ep = 4; break;
		case 'f': ep = 5; break;
		case 'g': ep = 6; break;
		case 'h': ep = 7; break;
		case '1': ep += 56; break;
		case '2': ep += 48; break;
		case '3': ep += 40; break;
		case '4': ep += 32; break;
		case '5': ep += 24; break;
		case '6': ep += 16; break;
		case '7': ep += 8; break;
		case '8': ep += 0; break;
		default: z = 1; break;
		}
	}
}

//parse the move s (in coordinate notation) and return the move's index in gen_dat, or -1 if the move is illegal or -2 if unknow command.
int ParseMove(char* s) {
	int from, to, i;

	/* make sure the string looks like a move */
	if (s[0] < 'a' || s[0] > 'h' ||
		s[1] < '0' || s[1] > '9' ||
		s[2] < 'a' || s[2] > 'h' ||
		s[3] < '0' || s[3] > '9')
		return -2;	// unknow command, before -1

	from = s[0] - 'a';
	from += 8 * (8 - (s[1] - '0'));
	to = s[2] - 'a';
	to += 8 * (8 - (s[3] - '0'));

	for (i = 0; i < first_move[1]; ++i)
		if (gen_dat[i].m.b.from == from && gen_dat[i].m.b.to == to) {

			/* if the move is a promotion, handle the promotion piece; assume
			 * that the promotion moves occur consecutively in gen_dat. */
			if (gen_dat[i].m.b.bits & 32)
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
				default:        /* assume it's a queen */
					return i + 3;
				}
			return i;
		}
	/* didn't find the move, illegal move */
	return -1;
}

//Engine MOve TO Uci MOve
char* EmoToUmo(SMove m)
{
	static char str[6];
	char c;
	if (m.bits & 32) {
		switch (m.promote) {
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
			COL(m.from) + 'a',
			8 - ROW(m.from),
			COL(m.to) + 'a',
			8 - ROW(m.to),
			c);
	}
	else
		sprintf(str, "%c%d%c%d",
			COL(m.from) + 'a',
			8 - ROW(m.from),
			COL(m.to) + 'a',
			8 - ROW(m.to));
	return str;
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
	int m;
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
			m = ParseMove(token);
			if (m < 0 || !makemove(gen_dat[m].m.b))
				printf("Illegal move (%s).\n", token);
			ply = 0;
			gen();
		}
}

static void ResetLimits() {
	info.stop = FALSE;
	info.nodes = 0;
	info.depthLimit = 64;
	info.nodesLimit = 0;
	info.timeLimit = 0;
	info.timeStart = GetTimeMs();
}

static void ParseGo(char* ptr) {
	ResetLimits();
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
	char *ptr,token[80];
	ptr = ParseToken(command, token);
	if (!strncmp(token, "ucinewgame",10)) {}
	else if (strncmp(token, "uci",3) == 0) {
		printf("id name %s\nuciok\n", NAME);
		fflush(stdout);
	}
	else if (!strncmp(token, "isready",7)) {
		printf("readyok\n");
		fflush(stdout);
	}
	else if (!strncmp(token, "position",8))
		ParsePosition(ptr);
	else if (!strncmp(token, "go",2))
		ParseGo(ptr);
	else if (!strncmp(token, "quit",4))
		exit(0);
	else if (!strncmp(token, "print",5))
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
