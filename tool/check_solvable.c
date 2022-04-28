#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

typedef struct st_clarg {
	bool NS_enabled;
	bool HS_enabled;
	bool LC_enabled;
} clarg_t;

typedef struct st_cell {
	int I;   // row index   : (0 <= I <  size)
	int J;   // column index: (0 <= J < size)
	int N;   // digit       : (1 <= N <= size)
} cell_t;

typedef struct  st_grid {
	int **X;   // X[i][j] = 0 if no digit is placed at (i,j), and X[i][j] = n if n (>0) is placed at (i,j).
	bool ***Y; // Y[i][j][k] is true if and only if k is a candidate at (i,j).
	int rank;  // 2 for 4x4 grid, and 3 for 9x9 grid
	int size;  // size = rank * rank
	int step;
  clarg_t arg;
} grid_t;


void print_clues_in_one_line(FILE* out, cell_t *cc, int len, int grid_size);

void reset_assign(int * assign, int len);
bool next_assign(int *assign, int len, int grid_size);
static void assert_assign(int *assign, int len, int grid_size);

void print_grid(FILE* out, grid_t *p);
void init_grid(grid_t *p, int rank, clarg_t arg);
void delete_grid(grid_t *p);
void reset_grid(grid_t *p);
void set_clues(grid_t *p, cell_t *cc, int len);
bool solve(grid_t *p);
bool issolved(grid_t *p);
static void assert_grid(grid_t *p);

static int count_digits(grid_t *p);
static int count_candidates(grid_t *p);

int apply_sudoku_rule(grid_t *p);
static int apply_sudoku_rule_to_row (grid_t *p, int i, int j);
static int apply_sudoku_rule_to_col (grid_t *p, int i, int j);
static int apply_sudoku_rule_to_blk (grid_t *p, int i, int j);
static int apply_sudoku_rule_to_self (grid_t *p, int i, int j);

int apply_naked_singles(grid_t *p);
static int num_candidates(grid_t *p, int i, int j);
static int get_1st_candidate(grid_t *p, int i, int j);

int apply_hidden_singles(grid_t *p);
static int apply_hidden_singles_to_row (grid_t *p, int i, int j);
static int apply_hidden_singles_to_col (grid_t *p, int i, int j);
static int apply_hidden_singles_to_blk (grid_t *p, int i, int j);

int apply_locked_candidates(grid_t *p);
static int remove_candidate_for_ABBR(grid_t *p, int bI, int bJ, int sI, int sJ, int k);
static int remove_candidate_for_ABBC(grid_t *p, int bI, int bJ, int sI, int sJ, int k);
static int remove_candidate_for_ACBB(grid_t *p, int sI, int sJ, int bI, int bJ, int k);
static int remove_candidate_for_ARBB(grid_t *p, int sI, int sJ, int bI, int bJ, int k);
static bool test_condition_for_ABBC  (grid_t *p, int bI, int bJ, int sI, int sJ, int k);
static bool test_condition_for_ABBR  (grid_t *p, int bI, int bJ, int sI, int sJ, int k);
static bool test_condition_for_ACBB  (grid_t *p, int sI, int sJ, int bI, int bJ, int k);
static bool test_condition_for_ARBB  (grid_t *p, int sI, int sJ, int bI, int bJ, int k);

int main(int argc, char *argv[]) {

	bool bruteforce_mode = false;

  clarg_t clarg;
  clarg.NS_enabled = clarg.HS_enabled = clarg.LC_enabled = false;

  char *str_of_grid = NULL;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			str_of_grid = argv[i];
		} else if (argv[i][1] == 'b') {
			bruteforce_mode = true;
		} else if (argv[i][1] == 'N') {
			clarg.NS_enabled = true;
		} else if (argv[i][1] == 'H') {
			clarg.HS_enabled = true;
		} else if (argv[i][1] == 'L') {
			clarg.LC_enabled = true;
		} else {
			assert(0);
			exit(EXIT_FAILURE);
		}
	}

	if (str_of_grid == NULL) {
		fprintf(stderr, "Usage: check_solvable [option] str_of_grid\n");
		fprintf(stderr, "-b    bruteforce verification over all assignments\n");
		fprintf(stderr, "-N    enable Naked Singles\n");
		fprintf(stderr, "-H    enable Hidden Singles\n");
		fprintf(stderr, "-L    enable Locked Candidates\n");
    fprintf(stderr, "str_of_grid  string of length 4 or 9 representing a grid: the initial letter must not be a hyphen\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "Example 1:\n");
    fprintf(stderr, "Execute the following command to check, in a brute-force way, if there is a set of clues for the cells (0,1), (1,3), (3,1), and (3,3) solvable with naked singles, hidden singles, and locked candidates.\n");
		fprintf(stderr, "Here, rows and columns are numbered from 0!\n");
    fprintf(stderr, "check_solvable -N -H -L -b 0*00000*00000*0*\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "Example 2:\n");
    fprintf(stderr, "Execute the following command to check if the set of clues (0,1)->3, (1,3)->2, (3,1)->4, and (3,3)->1 is solvable with naked singles, hidden singles, and locked candidates.\n");
		fprintf(stderr, "Here, rows and columns are numbered from 0!\n");
    fprintf(stderr, "check_solvable -N -H -L 0300000200000401\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "[Author] Takahisa Toda <todat@acm.org>\n");
    fprintf(stderr, "Graduate School of Information Systems, the University of Electro-Communications\n");
    fprintf(stderr, "1-5-1 Chofugaoka, Chofu, Tokyo 182-8585, Japan\n");
    fprintf(stderr, "http://www.disc.lab.uec.ac.jp/toda/index-en.html\n");
		exit(EXIT_FAILURE);
	}

  int rank;
	int nclues = 0;
  int count = 0;
  while (str_of_grid[count] != '\0') count++;
  if (count == 16) {
    rank = 2;
  } else if (count == 81) {
    rank = 3;
  } else {
    fprintf(stderr, "Error: invalid length\n");
    exit(EXIT_FAILURE);
  }

  const int size = rank * rank; // grid size

  // clue cells
	cell_t *cc = (cell_t*)malloc(sizeof(cell_t) * size * size);
	if (cc == NULL) {
		fprintf(stderr, "ERROR: Memory allocation failed.\n");
		exit(EXIT_FAILURE);
	}

  for (int k = 0; k < count; k++) {
    if (str_of_grid[k] == '0') continue;

    cc[nclues].I = k/size;
    cc[nclues].J = k%size;

    if (bruteforce_mode == false) {
      cc[nclues].N = str_of_grid[k]-'0';
      if (cc[nclues].N <= 0 || size < cc[nclues].N) {
        fprintf(stderr, "Error: invalid character found\n");
        exit(EXIT_FAILURE);
      }
    }
    nclues++;
  }

	fprintf(stdout, "a sudoku of %d x %d grid\n", size, size);
	fprintf(stdout, "a %8s bruteforce verification\n",        bruteforce_mode? "enabled": "disabled");
	fprintf(stdout, "a %8s Naked  Singles\n",    clarg.NS_enabled? "enabled": "disabled");
	fprintf(stdout, "a %8s Hidden Singles\n",    clarg.HS_enabled? "enabled": "disabled");
	fprintf(stdout, "a %8s Locked Candidates\n", clarg.LC_enabled? "enabled": "disabled");
	fprintf(stdout, "a number of clues = %d\n", nclues);
	fprintf(stdout, "a %s\n", str_of_grid);

	grid_t g;
  init_grid(&g, rank, clarg);

  if (bruteforce_mode == false) {

		reset_grid(&g);
    set_clues(&g, cc, nclues);

		bool res = solve(&g);
  
		if (res == true) {
			fprintf(stdout, "s SATISFIABLE\n");
		} else {
	    fprintf(stdout, "s UNSATISFIABLE\n");
		  print_grid(stdout, &g);
    }

    delete_grid(&g);
    free(cc);
		return 0;
  }

  assert(bruteforce_mode == true);

	int *assign = (int*)malloc(sizeof(int) * nclues);
	if (assign == NULL) {
		fprintf(stderr, "ERROR: Memory allocation failed\n");
		exit(EXIT_FAILURE);
	}
	reset_assign(assign, nclues);

	do {
    assert_assign(assign, nclues, g.size);

		reset_grid(&g);
		for (int pos = 0; pos < nclues; pos++) {
      cc[pos].N = assign[pos];
		}
    set_clues(&g, cc, nclues);

		bool res = solve(&g);

		if (res == true) {
			fprintf(stdout, "s SATISFIABLE\n");
      print_clues_in_one_line(stdout, cc, nclues, g.size);

      delete_grid(&g);
      free(assign);
      free(cc);
			return 0;
		}

	} while (true == next_assign(assign, nclues, g.size));

	fprintf(stdout, "s UNSATISFIABLE\n");

  delete_grid(&g);
  free(assign);
  free(cc);
	return 0;
}

void init_grid(grid_t *p, int rank, clarg_t arg)
{
  const int size = rank * rank;

  p->rank = rank;
  p->size = size;
  p->step = 0;
  p->arg  = arg;

	int **X = (int**)malloc(sizeof(int*) * size);
	if (X == NULL) {
		fprintf(stderr, "ERROR: Memory allocation failed\n");
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < size; i++) {
		X[i] = (int*)malloc(sizeof(int) * size);
		if (X[i] == NULL) {
			fprintf(stderr, "ERROR: Memory allocation failed\n");
			exit(EXIT_FAILURE);
		}
	}
  p->X = X;

	bool ***Y = (bool***)malloc(sizeof(bool**) * size);
	if (Y == NULL) {
		fprintf(stderr, "ERROR: Memory allocation failed\n");
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < size; i++) {
		Y[i] = (bool**)malloc(sizeof(bool*) * size);
		if (Y[i] == NULL) {
			fprintf(stderr, "ERROR: Memory allocation failed\n");
			exit(EXIT_FAILURE);
		}
		for (int j = 0; j < size; j++) {
			Y[i][j] = (bool*)malloc(sizeof(bool)* (size + 1));
			if (Y[i][j] == NULL) {
				fprintf(stderr, "ERROR: Memory allocation failed\n");
				exit(EXIT_FAILURE);
			}
		}
	}
  p->Y = Y;

  reset_grid(p);
}

void delete_grid(grid_t *p)
{
  const int size = p->size;

  assert(p->X != NULL);
  assert(p->Y != NULL);
  
  for (int i = 0; i < size; i++) {
    assert(p->X[i] != NULL);
    free(p->X[i]);
    p->X[i] = NULL;

    assert(p->Y[i] != NULL);
    for (int j = 0; j < size; j++) {
      assert(p->Y[i][j] != NULL);
      free(p->Y[i][j]);
      p->Y[i][j] = NULL;
      
    }
    free(p->Y[i]);
    p->Y[i] = NULL;

  }

  free(p->X); free(p->Y);
  p->X = NULL;
  p->Y = NULL;
}

static int count_digits(grid_t *p)
{
  const int size = p->size;
  int count = 0; // number of placed digits

  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      if (p->X[i][j] != 0) count++;
    }
  }

  return count;
}

static int count_candidates(grid_t *p)
{
  const int size = p->size;
  int count = 0; // total number of candidates

  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      for (int k = 1; k <= size; k++) {
        if (p->Y[i][j][k] == true) count++;
      }
    }
  }
 
 return count;
}

void print_clues_in_one_line(FILE* out, cell_t *cc, int len, int grid_size)
{
  const int nchars = grid_size * grid_size;

  fprintf(out, "a ");

  for (int pos = 0; pos < nchars; pos++) {
    const int i = pos/grid_size;
    const int j = pos%grid_size;
    bool found = false;
    
    for (int k = 0; k < len && found == false; k++) {
      if (cc[k].I == i && cc[k].J == j) {
        assert(1 <= cc[k].N && cc[k].N <= grid_size);
        fprintf(out, "%d", cc[k].N);
        found = true;
      }
    }

    if (found == false) {
      fprintf(out, "0");
    }
  }

  fprintf(out, "\n");
}

static void assert_assign(int *assign, int len, int grid_size)
{
  for (int pos = 0; pos < len; pos++) {
    assert(1 <= assign[pos] && assign[pos] <= grid_size); 
  }
}

static void assert_grid(grid_t *p)
{
	const int size = p->size;

	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
      assert(0 <= p->X[i][j] && p->X[i][j] <= size);
      assert(p->Y[i][j][0] == true);
    }
  }
}

void reset_grid(grid_t *p)
{
	const int size = p->size;

	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			p->X[i][j] = 0;
			for (int k = 0; k <= size; k++) {
				p->Y[i][j][k] = true;
			}
		}
	}

	p->step = 0;
}

void set_clues(grid_t *p, cell_t *cc, int len)
{
		for (int pos = 0; pos < len; pos++) {
			int i = cc[pos].I;
			int j = cc[pos].J;
			p->X[i][j] = cc[pos].N;
		}
}

void print_grid(FILE* out, grid_t *p)
{
	const int size = p->size;

  fprintf(out, "a grid in step %d: ", p->step);
  for (int pos = 0; pos < size * size; pos++) {
    int i = pos/size;
    int j = pos%size;
    fprintf(out, "%d", p->X[i][j]);
  }
  fprintf(out, "\n");

	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
      fprintf(out, "a candidates at (%d,%d): ", i,j);
			for(int k = 1; k <= size; k++) {
        if (p->Y[i][j][k] == true) fprintf(out, "%d, ", k);
			}
      fprintf(out, "\n");
		}
	}
}

void reset_assign(int *assign, int len)
{
	for (int pos = 0; pos < len; pos++) {
		assign[pos] = 1;
	}
}

bool next_assign(int *assign, int len, int grid_size)
{
	for (int pos = 0; pos < len; pos++) {
		if (assign[pos] == grid_size) {
			assign[pos] = 1;
			continue;
		} else {
			assert(assign[pos] < grid_size);
			assign[pos]++;
			return true;
		}
	}
	return false;
}

bool solve(grid_t *p)
{
	int num_placed, num_removed;
	do {
    const int count1_before = count_digits(p);
    const int count2_before = count_candidates(p);

    assert_grid(p);
    num_placed = num_removed = 0;

		int res = apply_sudoku_rule(p);
    num_removed += res;

		if (p->arg.NS_enabled == true) {
			int res = apply_naked_singles(p);
      num_placed += res;
		}

		if (p->arg.HS_enabled == true) {
			int res = apply_hidden_singles(p);
      num_placed += res;
		}

		if (p->arg.LC_enabled == true) {
			int res = apply_locked_candidates(p);
      num_removed += res;
		}

    (p->step)++;

    const int count1_after = count_digits(p);
    const int count2_after = count_candidates(p);
    assert(count1_after == count1_before + num_placed);
    assert(count2_after == count2_before - num_removed);

	} while (num_removed + num_placed > 0);

	return issolved(p);
}

bool issolved(grid_t *p)
{
	const int size = p->size;

	for(int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			if (p->X[i][j]              == 0) return false;
			if (num_candidates(p, i, j) == 0) return false;
		}
	}

  assert(count_digits(p) == size * size);
  assert(count_candidates(p) == size * size);

	return true;
}

int apply_sudoku_rule(grid_t *p)
{
	const int size = p->size;
  int count = 0; // number of eliminated candidates

	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {

      if (p->X[i][j] == 0) continue;

      int res;
      res = apply_sudoku_rule_to_self(p, i, j);
      count += res;

			res = apply_sudoku_rule_to_row (p, i, j);
      count += res;

			res = apply_sudoku_rule_to_col (p, i, j);
      count += res;

			res = apply_sudoku_rule_to_blk (p, i, j);
      count += res;

		}
	}

	return count;
}

static int apply_sudoku_rule_to_self (grid_t *p, int i, int j)
{
	const int size = p->size;

	if (p->X[i][j] == 0) return 0;

  int count = 0; // number of eliminated candidates

	for (int k = 1; k <= size; k++) {
    if (k == p->X[i][j]) continue;

		if (p->Y[i][j][k] == true) {
      count++;
		}
		p->Y[i][j][k] = false;
	}

	return count;
}


static int apply_sudoku_rule_to_row (grid_t *p, int i, int j)
{
	const int size = p->size;
	const int n = p->X[i][j];

	if (n == 0) return 0;

  int count = 0; // number of eliminated candidates

	//row;
	for (int k = 0; k < size; k++) {
    if (k == j) continue;

		if (p->Y[i][k][n] == true) {
        count++;
		}
		p->Y[i][k][n] = false;
	}

	return count;

}

static int apply_sudoku_rule_to_col (grid_t *p, int i, int j)
{
	const int size = p->size;
	const int n = p->X[i][j];

	if (n == 0) return 0;

  int count = 0; // number of eliminated candidates

	//column;
	for (int k = 0; k < size; k++) {
    if (k == i) continue;

		if (p->Y[k][j][n] == true) {
      count++;
		}
		p->Y[k][j][n] = false;
	}

	return count;
}

static int apply_sudoku_rule_to_blk (grid_t *p, int i, int j)
{
	const int rank = p->rank;
	const int size = p->size;
	const int n = p->X[i][j];

	if (n == 0) return 0;

  int count = 0; // number of eliminated candidates

	//block;
	int i0 = (i/rank)*rank;
	int j0 = (j/rank)*rank;
	for (int di = 0; di < rank; di++) {
		for (int dj = 0; dj < rank; dj++) {
			int i1 = i0+di;
			int j1 = j0+dj;

      if (i == i1 && j == j1) continue;

			if (p->Y[i1][j1][n] == true) {
        count++;
			}
			p->Y[i1][j1][n] = false;

		}
	}

	return count;
}

int apply_naked_singles(grid_t *p)
{
	const int size = p->size;
	int count = 0; // number of placed digits

	for (int i = 0; i < size; i ++) {
		for (int j = 0; j < size; j++) {

			if (p->X[i][j] != 0) continue; // already placed.

			int n = num_candidates(p, i, j);
			if (n == 1) {
				p->X[i][j] = get_1st_candidate(p, i, j);
				assert(p->X[i][j] > 0);
				count++;
			}

		}
	}

  return count;
}

static int num_candidates(grid_t *p, int i, int j)
{
	const int size = p->size;

	int count = 0;

	for (int k = 1; k <= size; k++) {
		if (p->Y[i][j][k] == true) count++;
	}

	return count;
}

static int get_1st_candidate(grid_t *p, int i, int j)
{
	const int size = p->size;

	for (int k = 1; k <= size; k++) {
		if (p->Y[i][j][k] == true) return k;
	}

	return 0; // no candidate exists.
}

int apply_hidden_singles(grid_t *p)
{
	const int size = p->size;
  int count = 0; // number of placed digits

	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {

      if (p->X[i][j] != 0) continue;

      assert(p->X[i][j] == 0);
			if (apply_hidden_singles_to_row (p, i, j) == 1) {
				assert(p->X[i][j] != 0);
         count++;
				continue;
			}

      assert(p->X[i][j] == 0);
			if (apply_hidden_singles_to_col (p, i, j) == 1) {
				assert(p->X[i][j] != 0);
        count++;
				continue;
			}

      assert(p->X[i][j] == 0);
			if (apply_hidden_singles_to_blk (p, i, j) == 1) {
				assert(p->X[i][j] != 0);
        count++;
				continue;
			}

		}
	}

	return count;
}

static int apply_hidden_singles_to_row (grid_t *p, int i, int j)
{
	const int size = p->size;

	if (p->X[i][j] != 0) return 0;

	for (int v = 1; v <= size; v++) {

		bool found = false;

		for (int k = 0; k < size && found == false; k++) {
			if (k != j && p->Y[i][k][v] == true) {
					found = true;
			}
		}

		if (found == false)  {
			p->X[i][j] = v;
			return 1;
		}

	}

	return 0;

}

static int apply_hidden_singles_to_col (grid_t *p, int i, int j)
{
	const int size = p->size;

	if (p->X[i][j] != 0) return 0;

	for (int v = 1; v <= size; v++) {

		bool found = false;

		for (int k = 0; k < size && found == false; k++) {
			if (k != i && p->Y[k][j][v] == true) {
				found = true;
			}
		}

		if (found == false)  {
			p->X[i][j] = v;
			return 1;
		}

	}

	return 0;
}

static int apply_hidden_singles_to_blk (grid_t *p, int i, int j) 
{
	const int rank = p->rank;
	const int size = p->size;

	if (p->X[i][j] != 0) return 0;

	for (int v = 1; v <= size; v++) {

		bool found = false;

		int i0 = (i/rank)*rank;
		int j0 = (j/rank)*rank;

		for (int di = 0; di < rank && found == false; di++) {
			for (int dj = 0; dj < rank && found == false; dj++) {

				int i1 = i0+di;
				int j1 = j0+dj;

				if ((i != i1 || j != j1)
					  && (p->Y[i1][j1][v] == true)) {
						found = true;
			  }

			}
		}

		if (found == false)  {
			p->X[i][j] = v;
			return 1;
		}

	}

	return 0;
}

int apply_locked_candidates(grid_t *p)
{
	const int size = p->size;
	const int rank = p->rank;

  int count = 0; // number of eliminated candidates

	// for all pairs of rows  and blocks
	for (int sI = 0; sI < size; sI++) {
		const int sJ = 0;

    for (int bI = 0; bI < rank; bI++) {
		  for (int bJ = 0; bJ < rank; bJ++) {
			  for (int k = 1; k <= size; k++) {

				  if (true == test_condition_for_ARBB(p, sI, sJ, bI, bJ, k)) {
            int res = remove_candidate_for_ABBR(p, bI, bJ, sI, sJ, k);
            count += res;
				  }

          if (true == test_condition_for_ABBR(p, bI, bJ, sI, sJ, k)) {
					  int res = remove_candidate_for_ARBB(p, sI, sJ, bI, bJ, k);
            count += res;
				  }

			  }
		  }
    }
	}

	// for all pairs of columns  and blocks
	for (int sJ = 0; sJ < size; sJ++) {
		const int sI = 0;

		for (int bI = 0; bI < rank; bI++) {
      for (int bJ = 0; bJ < rank; bJ++) {
  			for (int k = 1; k <= size; k++) {

	  			if (true == test_condition_for_ACBB(p, sI, sJ, bI, bJ, k)) {
            int res = remove_candidate_for_ABBC(p, bI, bJ, sI, sJ, k);
            count += res;
				  }
          
          if (true == test_condition_for_ABBC(p, bI, bJ, sI, sJ, k)) {
					  int res = remove_candidate_for_ACBB(p, sI, sJ, bI, bJ, k);
            count += res;
				  }

			  }
		  }
	  }
  }

	return count;
}

// wheter Y[i][j][k] is false for all (i,j) in A - B such that  A: the si-th row, B: the block of (bi,bj)
// Note: return false if A and B have no common cell.
static bool  test_condition_for_ARBB(grid_t *p, int sI, int sJ, int bI, int bJ, int k)
{
	const int rank = p->rank;
	const int size = p->size;

	assert(sJ == 0);
	assert(1 <= k && k <= size);

	const int first_I = bI*rank;
	const int first_J = bJ*rank;

	int run_I = sI;
	int run_J = sJ;
  assert(run_J == 0);

  if (run_I < first_I || first_I + rank - 1 < run_I ) {
    return false; 
  }
	assert(sI/rank == bI);

	for (; run_J < size; run_J++) {
		if (first_J <= run_J && run_J <= first_J + rank - 1) {
			continue; // skip for all cells in B
		}
		if (p->Y[run_I][run_J][k] == true) {
			return false;
		}
	}

	return true;
}

// wheter Y[i][j][k] is false for all (i,j) in A - B such that  A: the sj-th column, B: the block of (bi,bj)
// Note: return false if A and B have no common cell.
static bool  test_condition_for_ACBB(grid_t *p, int sI, int sJ, int bI, int bJ, int k)
{
	const int rank = p->rank;
	const int size = p->size;

	assert(sI == 0);
	assert(1 <= k && k <= size);

	const int first_I = bI*rank;
	const int first_J = bJ*rank;

	int run_I = sI;
	int run_J = sJ;
  assert(run_I == 0);

  if (run_J < first_J || first_J + rank - 1 < run_J) {
    return false;
  }
	assert(sJ/rank == bJ);

	for (; run_I < size; run_I++) {
		if (first_I <= run_I && run_I <= first_I + rank - 1) {
			continue; // skip for all cells in B
		}
		if (p->Y[run_I][run_J][k] == true) {
			return false;
		}
	}

	return true;
}

// wheter Y[i][j][k] is false for all (i,j) in A - B such that  A: the block of (bi,bj), B: the si-th row
// Note: return false if A and B have no common cell.
static bool  test_condition_for_ABBR(grid_t *p, int bI, int bJ, int sI, int sJ, int k)
{
	const int rank = p->rank;
	const int size = p->size;

	assert(sJ == 0);
	assert(1 <= k && k <= size);

	const int first_I = bI*rank;
	const int first_J = bJ*rank;

  if (sI < first_I || first_I + rank - 1 < sI) {
    return false; 
  }
	assert(bI == sI/rank);

	for(int dI = 0; dI < rank; dI++) {
		for(int dJ = 0; dJ < rank; dJ++) {
			const int run_I = first_I + dI;
			const int run_J = first_J + dJ;

			if (run_I == sI) continue; // skip for all cells in B

			if (p->Y[run_I][run_J][k] == true) {
				return false;
			}
		}
	}

	return true;
}

// wheter Y[i][j][k] is false for all (i,j) in A - B such that  A: the block of (bi,bj), B: the sj-th column
// Note: return false if A and B have no common cell.
static bool  test_condition_for_ABBC(grid_t *p, int bI, int bJ, int sI, int sJ, int k)
{
	const int rank = p->rank;
	const int size = p->size;

	assert(sI == 0);
	assert(1 <= k && k <= size);

	const int first_I = bI*rank;
	const int first_J = bJ*rank;

  if (sJ < first_J || first_J + rank - 1 < sJ){
    return false;
  }
	assert(bJ == sJ/rank);

	for(int dI = 0; dI < rank; dI++) {
		for(int dJ = 0; dJ < rank; dJ++) {
			const int run_I = first_I + dI;
			const int run_J = first_J + dJ;

			if (run_J == sJ) continue; // skip for all cells in B

			if (p->Y[run_I][run_J][k] == true) {
				return false;
			}
		}
	}

	return true;
}

// make Y[i][j][k] false for all (i,j) in A - B such that  A: the si-th row, B: the block of (bi,bj).
// Note: return 0 if A and B have no common cell.
static int remove_candidate_for_ARBB(grid_t *p, int sI, int sJ, int bI, int bJ, int k)
{
	const int rank = p->rank;
	const int size = p->size;
	assert(sJ == 0);
	assert(1 <= k && k <= size);

	int count = 0; // number of eliminated candidates

	const int first_I = bI*rank;
	const int first_J = bJ*rank;

	int run_I = sI;
	int run_J = sJ;

  if (run_I < first_I || first_I + rank - 1 < run_I) {
    return 0;
  }

	for (; run_J < size; run_J++) {
		if (first_J <= run_J && run_J <= first_J + rank - 1) {
			continue; // skip for all cells in B
		}
		if (p->Y[run_I][run_J][k] == true) {
			count++;
			p->Y[run_I][run_J][k] = false;
		}
	}

	return count;
}

// make Y[i][j][k] false for all (i,j) in A - B such that  A: the si-th column, B: the block of (bi,bj).
// Note: return 0 if A and B have no common cell.
static int  remove_candidate_for_ACBB(grid_t *p, int sI, int sJ, int bI, int bJ, int k)
{
	const int rank = p->rank;
	const int size = p->size;
	assert(sI == 0);
	assert(1 <= k && k <= size);

	const int first_I = bI*rank;
	const int first_J = bJ*rank;

	int count = 0; // number of eliminated candidates

	int run_I = sI;
	int run_J = sJ;

  if (run_J < first_J || first_J + rank - 1 < run_J) {
    return 0;
  }

	for (; run_I < size; run_I++) {
		if (first_I <= run_I && run_I <= first_I + rank - 1) {
			continue; // skip for all cells in B
		}
		if (p->Y[run_I][run_J][k] == true) {
			count++;
			p->Y[run_I][run_J][k] = false;
		}
	}

	return count;
}

// make Y[i][j][k] false for all (i,j) in A - B such that  A: the block of (bi,bj), B: the si-th row..
// Note: return 0 if A and B have no common cell.
static int  remove_candidate_for_ABBR(grid_t *p, int bI, int bJ, int sI, int sJ, int k)
{
	const int rank = p->rank;
	const int size = p->size;
	assert(sJ == 0);
	assert(1 <= k && k <= size);

	int count = 0; // number of eliminated candidates

	const int first_I = bI*rank;
	const int first_J = bJ*rank;

  if (sI < first_I || first_I + rank - 1 < sI) {
    return 0;
  }

	for(int dI = 0; dI < rank; dI++) {
		for(int dJ = 0; dJ < rank; dJ++) {
			const int run_I = first_I + dI;
			const int run_J = first_J + dJ;

			if (run_I == sI) continue; // skip for all cells in B

			if (p->Y[run_I][run_J][k] == true) {
				p->Y[run_I][run_J][k] = false;
				count++;
			}
		}
	}

	return count;
}

// make Y[i][j][k] false for all (i,j) in A - B such that  A: the block of (bi,bj), B: the sj-th column.
// Note: return 0 if A and B have no common cell.
static int  remove_candidate_for_ABBC(grid_t *p, int bI, int bJ, int sI, int sJ, int k)
{
	const int rank = p->rank;
	const int size = p->size;
	assert(sI == 0);
	assert(1 <= k && k <= size);

	int count = 0; // number of eliminated candidates

	const int first_I = bI*rank;
	const int first_J = bJ*rank;

  if (sJ < first_J || first_J + rank - 1 < sJ) {
    return 0;
  }

	for(int dI = 0; dI < rank; dI++) {
		for(int dJ = 0; dJ < rank; dJ++) {
			const int run_I = first_I + dI;
			const int run_J = first_J + dJ;

			if (run_J == sJ) continue; // skip for all cells in B

			if (p->Y[run_I][run_J][k] == true) {
				p->Y[run_I][run_J][k] = false;
				count++;
			}
		}
	}

	return count;
}
