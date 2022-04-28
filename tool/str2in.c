#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: str2in string_of_grid\n");
    fprintf(stderr, "string_of_grid  string of length 4 or 9: 0 means a non-clue cell, and any other character means a clue cell.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "From a string of grid, this program prints out positions of cells,\n");
    fprintf(stderr, "which can be used as input for scg_modeler.\n");
    fprintf(stderr, "str2in 0*00000*00000*0* > scg.in\n");
    fprintf(stderr, "cat scg.in\n");
    fprintf(stderr, "1 2\n");
    fprintf(stderr, "2 4\n");
    fprintf(stderr, "4 2\n");
    fprintf(stderr, "4 4\n");
    fprintf(stderr, "scg_modeler -N -H -L -r 2 -k 10 scg.in > in.csp\n");
    fprintf(stderr, "sugar in.csp\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "[Author] Takahisa Toda <todat@acm.org>\n");
    fprintf(stderr, "Graduate School of Information Systems, the University of Electro-Communications\n");
    fprintf(stderr, "1-5-1 Chofugaoka, Chofu, Tokyo 182-8585, Japan\n");
    fprintf(stderr, "http://www.disc.lab.uec.ac.jp/toda/index-en.html\n");
    exit(EXIT_FAILURE);
  }

  const char *grid = argv[1];

  int count = 0;
  while (grid[count] != '\0') count++;

  int size;
  if (count == 16) size = 4;
  else if (count == 81) size = 9;
  else {
    fprintf(stderr, "Error: given string has an invalid length\n"); 
    exit(EXIT_FAILURE);
  }

  for (int k = 0; k < count; k++) {
    int i,j,n;

    i = k/size;
    j = k%size;

    if (grid[k] == '0') continue;
  
    fprintf(stdout, "%d %d\n", i+1, j+1);
  }

  return 0;
}
