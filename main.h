#define BOOL			int
#define TRUE			1
#define FALSE			0
#define S64 signed __int64
#define U64 unsigned __int64

#define bool BOOL
#define true TRUE
#define false FALSE

#define MATE 32000

#define GEN_STACK		1120
#define MAX_PLY			32
#define HIST_STACK		512

#define LIGHT			0
#define DARK			1

#define PAWN			0
#define KNIGHT			1
#define BISHOP			2
#define ROOK			3
#define QUEEN			4
#define KING			5

#define EMPTY			6

/* useful squares */
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

#define ROW(score)			(score >> 3)
#define COL(score)			(score & 7)

#define MAX_SCORE 10000
#define MAX_TIME 60000
#define NAME "Lizard"
#define VERSION "2026-02-08"
#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

typedef struct {
	char from;
	char to;
	char promote;
	char bits;
} SMove;

typedef union {
	SMove b;
	int u;
} move;

/* an element of the move stack. it's just a move with a
   score, so it can be sorted by the search functions. */
typedef struct {
	move m;
	int score;
} gen_t;

/* an element of the history stack, with the information
   necessary to take a move back. */
typedef struct {
	move m;
	int capture;
	int castle;
	int ep;
	int fifty;
	int hash;
} hist_t;

typedef struct {
	int stop;
	int depthLimit;
	S64 timeStart;
	S64 timeLimit;
	U64 nodes;
	U64 nodesLimit;
}SearchInfo;

extern SearchInfo info;

extern int color[64];
extern int piece[64];
extern int side;
extern int xside;
extern int castle;
extern int ep;
extern int fifty;
extern int hash;
extern int ply;
extern int hply;
extern gen_t gen_dat[GEN_STACK];
extern int first_move[MAX_PLY];
extern int history[64][64];
extern hist_t hist_dat[HIST_STACK];
extern int max_time;
extern int max_depth;
extern int start_time;
extern int stop_time;
extern int nodes;
extern move pv[MAX_PLY][MAX_PLY];
extern int pv_length[MAX_PLY];
extern BOOL follow_pv;
extern int hash_piece[2][6][64];
extern int hash_side;
extern int hash_ep[64];
extern int mailbox[120];
extern int mailbox64[64];
extern BOOL slide[6];
extern int offsets[6];
extern int offset[6][8];
extern int castle_mask[64];
extern char piece_char[6];
extern int init_color[64];
extern int init_piece[64];
extern const char* defFen;
extern const char* square_name[];
extern const char* num;
extern int piece_value[6];

static int score;


/* prototypes */

/* board.c */
void InitBoard(const char *s);
void InitHash();
int hash_rand();
void set_hash();
BOOL in_check(int s);
BOOL attack(int sq, int s);
void gen();
void gen_caps();
void gen_push(int from, int to, int bits);
void gen_promote(int from, int to, int bits);
BOOL makemove(SMove m);
void takeback();

/* search.c */
void SearchIterate();
int SearchAlpha(int alpha, int beta, int depth, int null_move);
int SearchQuiescence(int alpha, int beta);
int reps();
void sort_pv();
void sort(int from);
int Checkup();

/* eval.c */
int eval();
int eval_light_pawn(int sq);
int eval_dark_pawn(int sq);
int eval_light_king(int sq);
int eval_lkp(int f);
int eval_dark_king(int sq);
int eval_dkp(int f);

/* main.c */
U64 GetTimeMs();
int main();
int ParseMove(char *s);
char *EmoToUmo(SMove m);
void PrintBoard();
void SetFen(const char* s);
