#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: out2str rank sugar.out\n");
    fprintf(stderr, "rank   2 for 4x4 grid, 3 for 9x9 grid, etc\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "From the output of Sugar, this program makes the string of the initial grid.\n");
    fprintf(stderr, "scg_modeler -N -H -L -r 2 -k 10 r2/r2c4-997 > in.csp\n");
    fprintf(stderr, "sugar in.csp > tmp\n");
    fprintf(stderr, "cat tmp | grep \"x_[0-9]_[0-9]_0\\s\\+[1-9]\" | sed -e \"s/^a\\s\\+x_\\([0-9]\\)_\\([0-9]\\)_0\\s\\+\\([1-9]\\)/\\1 \\2 \\3/g\" > sugar.out\n");
    fprintf(stderr, "cat sugar.out\n");
    fprintf(stderr, "2 0 3\n");
    fprintf(stderr, "3 0 2\n");
    fprintf(stderr, "0 2 1\n");
    fprintf(stderr, "1 2 3\n");
    fprintf(stderr, "out2str 2 sugar.out\n");
    fprintf(stderr, "0010003030002000\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "[Author] Takahisa Toda <todat@acm.org>\n");
    fprintf(stderr, "Graduate School of Information Systems, the University of Electro-Communications\n");
    fprintf(stderr, "1-5-1 Chofugaoka, Chofu, Tokyo 182-8585, Japan\n");
    fprintf(stderr, "http://www.disc.lab.uec.ac.jp/toda/index-en.html\n");
    exit(EXIT_FAILURE);
  }

  const int rank = (int)strtol(argv[1], NULL, 10);
  assert(rank >= 2);
  const int size = rank * rank; 

  FILE *in = fopen(argv[2], "r");
  if (in == NULL) {
    fprintf(stderr, "Error: cannot open %s\n", argv[2]);
    exit(EXIT_FAILURE);
  }

  int i, j, n;
  int nclues; // number of clue cells with duplicates
  i = j = nclues = 0;
  char buf[BUFSIZ];
  assert(BUFSIZ > size*size);
  for (int k = 0; k < size* size; k++) {
    buf[k] = -1;
  }

  while(fscanf(in, "%d %d %d", &i,&j,&n) != EOF) {

    if (0 <= i && i < size
     && 0 <= j && j < size
     && 0 <= n && n <= size
     && nclues < (size * size)) {

      buf[i*size+j] = n;
      nclues++;

    } else if (false == (nclues < (size * size))) {

      fprintf(stderr, "ERROR: Too many clue cells are given.\n");
      exit(EXIT_FAILURE);

    } else {

      fprintf(stderr, "ERROR: Invalid number is found at line %d.\n", nclues + 1);
      exit(EXIT_FAILURE);

    }

  }

  for (int k = 0; k < size* size; k++) {
      fprintf(stdout, "%d", buf[k] == -1? 0:buf[k]);
  }
  fprintf(stdout, "\n");

  fclose(in);

  return 0;
}
