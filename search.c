#include <stdio.h>
#include <string.h>
#include "main.h"

#define S64 signed __int64
#define U64 unsigned __int64
#define GONULL 1

#define RDEPTH(score)  ((score>1500)?(3):(2))

/* quiesce() is a recursive minimax search function with
   alpha-beta cutoffs. In other words, negamax. It basically
   only searches capture sequences and allows the evaluation
   function to cut the search off (and set alpha). The idea
   is to find a position where there isn't a lot going on
   so the static evaluation function will work. */
int SearchQuiescence(int alpha, int beta)
{
	int i, j, score;
	if (Checkup())
		return 0;

	pv_length[ply] = ply;

	/* are we too deep? */
	if (ply >= MAX_PLY)
		return eval();
	if (hply >= HIST_STACK)
		return eval();

	/* check with the evaluation function */
	score = eval();
	if (score >= beta)
		return beta;
	if (score > alpha)
		alpha = score;

	gen_caps();
	if (follow_pv)  /* are we following the PV? */
		sort_pv();

	/* loop through the moves */
	for (i = first_move[ply]; i < first_move[ply + 1]; ++i) {
		sort(i);
		if (!makemove(gen_dat[i].m.b))
			continue;
		score = -SearchQuiescence(-beta, -alpha);
		takeback();
		if (info.stop)
			return 0;
		if (score > alpha) {
			if (score >= beta)
				return beta;
			alpha = score;

			/* update the PV */
			pv[ply][ply] = gen_dat[i].m;
			for (j = ply + 1; j < pv_length[ply + 1]; ++j)
				pv[ply][j] = pv[ply + 1][j];
			pv_length[ply] = pv_length[ply + 1];
		}
	}
	return alpha;
}

int SearchAlpha(int alpha, int beta, int depth, int null_move)
{
	int i, j; //x;
	BOOL c, f;
	int nullmat;
	int o_side;
	int o_xside;
	int o_ep;
	int o_fifty;
	int o_hash;
	int o_castle;

	/* we're as deep as we want to be; call quiesce() to get
	   a reasonable score and return it. */
	if (!depth)
		return SearchQuiescence(alpha, beta);
	if (Checkup())
		return 0;
	pv_length[ply] = ply;

	/* if this isn't the root of the search tree (where we have
	   to pick a move and can't simply return 0) then check to
	   see if the position is a repeat. if so, we can assume that
	   this line is a draw and return 0. */
	if (ply && reps())
		return 0;

	/* are we too deep? */
	if (ply >= MAX_PLY)
		return eval();
	if (hply >= HIST_STACK)
		return eval();

	/* are we in check? if so, we want to search deeper */
	c = in_check(side);
	if (c)
		++depth;

	/* null move */
	if (GONULL && !c && null_move && ply) {
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
			o_fifty = fifty;
			o_hash = hash;
			o_castle = castle;
			ep = -1;
			fifty = 0;
			side = xside;
			xside = o_side;
			score = -SearchAlpha(-beta, -beta + 1, depth - 1 - RDEPTH(nullmat), 0);
			side = o_side;
			xside = o_xside;
			ep = o_ep;
			fifty = o_fifty;
			hash = o_hash;
			castle = o_castle;
			if (info.stop) {
				return 0;
			}
			if (score >= beta) {
				return beta;
			}
		}
	}

	gen();
	if (follow_pv)  /* are we following the PV? */
		sort_pv();
	f = FALSE;

	/* loop through the moves */
	for (i = first_move[ply]; i < first_move[ply + 1]; ++i) {
		sort(i);
		if (!makemove(gen_dat[i].m.b))
			continue;
		f = TRUE;
		score = -SearchAlpha(-beta, -alpha, depth - 1, 1);
		takeback();
		if (info.stop)
			return 0;
		if (score > alpha) {
			history[(int)gen_dat[i].m.b.from][(int)gen_dat[i].m.b.to] += depth;
			if (score >= beta)
				return beta;
			alpha = score;
			pv[ply][ply] = gen_dat[i].m;
			for (j = ply + 1; j < pv_length[ply + 1]; ++j)
				pv[ply][j] = pv[ply + 1][j];
			pv_length[ply] = pv_length[ply + 1];
			if (!ply) {
				printf("info depth %d score ", depth);
				if (abs(score) < MATE - MAX_PLY)
					printf("cp %d", score);
				else
					printf("mate %d", (score > 0 ? (MATE - score + 1) >> 1 : -(MATE + score) >> 1));
				printf(" time %lld", GetTimeMs() - info.timeStart);
				printf(" nodes %lld pv", info.nodes);
				for (j = 0; j < pv_length[0]; ++j)
					printf(" %s", EmoToUmo(pv[0][j].b));
				printf("\n");
			}
		}
	}
	if (!f)
		if (c)
			return -MAX_SCORE + ply;
		else
			return 0;
	if (fifty >= 100)
		return 0;
	return alpha;
}

void SearchIterate()
{
	int i, j, score;
	ply = 0;

	memset(pv, 0, sizeof(pv));
	memset(history, 0, sizeof(history));
	for (i = 1; i <= info.depthLimit; ++i) {
		follow_pv = TRUE;
		score = SearchAlpha(-10000, 10000, i, 1);
		if (info.stop)
			break;
		if (score > 9000 || score < -9000)
			break;
	}
	printf("bestmove %s\n", EmoToUmo(pv[0][0].b));
	fflush(stdout);
}


/* reps() returns the number of times the current position
   has been repeated. It compares the current value of hash
   to previous values. */
int reps()
{
	int i;
	int r = 0;

	for (i = hply - fifty; i < hply; ++i)
		if (hist_dat[i].hash == hash)
			++r;
	return r;
}


/* sort_pv() is called when the search function is following
   the PV (Principal Variation). It looks through the current
   ply's move list to see if the PV move is there. If so,
   it adds 10,000,000 to the move's score so it's played first
   by the search function. If not, follow_pv remains FALSE and
   search() stops calling sort_pv(). */
void sort_pv()
{
	int i;

	follow_pv = FALSE;
	for (i = first_move[ply]; i < first_move[ply + 1]; ++i)
		if (gen_dat[i].m.u == pv[0][ply].u) {
			follow_pv = TRUE;
			gen_dat[i].score += 10000000;
			return;
		}
}


/* sort() searches the current ply's move list from 'from'
   to the end to find the move with the highest score. Then it
   swaps that move and the 'from' move so the move with the
   highest score gets searched next, and hopefully produces
   a cutoff. */
void sort(int from)
{
	int i;
	int bs;  /* best score */
	int bi;  /* best i */
	gen_t g;

	bs = -1;
	bi = from;
	for (i = from; i < first_move[ply + 1]; ++i)
		if (gen_dat[i].score > bs) {
			bs = gen_dat[i].score;
			bi = i;
		}
	g = gen_dat[from];
	gen_dat[from] = gen_dat[bi];
	gen_dat[bi] = g;
}


//is called once in a while during the search.
int Checkup() {
	if ((++info.nodes & 0xffff) == 0) {
		if (info.timeLimit && GetTimeMs() - info.timeStart > info.timeLimit)
			info.stop = TRUE;
		if (info.nodesLimit && info.nodes > info.nodesLimit)
			info.stop = TRUE;
	}
	return info.stop;
}
