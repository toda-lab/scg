#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<assert.h>
#include<unistd.h>

#include "scg_modeler.h"
#include "scg_assert.h"

#include "sudoku_rule.h"
#include "naked_singles.h"
#include "hidden_singles.h"
#include "locked_candidates.h"

#define NDEBUG

typedef struct st_clarg {
        int  rank;
        int  bound;

        bool NS_enabled;
        bool HS_enabled;
        bool LC_enabled;
} clarg_t;

static void usage (void);
static void print_cells (FILE *out, const cell_t *q, int n);

int main (int argc, char *argv[]){
        FILE* in  = NULL;
        FILE* out = stdout;

        clarg_t clarg;

        // default setting
        clarg.NS_enabled = clarg.HS_enabled = clarg.LC_enabled = false;
        clarg.rank  = 2;
        clarg.bound =   (clarg.rank * clarg.rank)
                      * (clarg.rank * clarg.rank)
                      * (clarg.rank * clarg.rank); // too large!

        int          ch;
        extern char  *optarg;
        extern int   optind, opterr;

        while ((ch = getopt(argc, argv, "NHLr:k:o:h")) != -1) {
                switch (ch) {
                        case 'N':
                                clarg.NS_enabled = true;
                                break;

                        case 'H':
                                clarg.HS_enabled = true;
                                break;

                        case 'L':
                                clarg.LC_enabled = true;
                                break;

                        case 'r':
                                clarg.rank = (int)strtol(optarg, NULL, 10);
                                if (clarg.rank < 2) {
                                        fprintf(stderr, "Error: rank must be 2 or larger.\n");
                                        exit(EXIT_FAILURE);
                                }
                                break;

                        case 'k':
                                clarg.bound = (int)strtol(optarg, NULL, 10);
                                assert(clarg.bound >= 0);
                                break;
                        case 'o':
                                out = fopen(optarg, "w");
                                if (out == NULL) {
                                        fprintf(stderr, "Error: cannot open %s\n", optarg);
                                        exit(EXIT_FAILURE);
                                }
                                break;

                        case 'h':
                                usage();
                                exit(EXIT_FAILURE);

                        default:
                                exit(EXIT_FAILURE);
                }
        }

        argc -= optind;
        argv += optind;

        if (argc == 1) {
                in = fopen(argv[0], "r");
                if (in == NULL) {
                        fprintf(stderr, "Error: cannot open %s\n", argv[0]);
                        exit(EXIT_FAILURE);
                }
        } else {
                usage();
                exit(EXIT_FAILURE);
        }

        data_t data;
        init_data(&data, clarg.rank, clarg.bound);

        fprintf(out, "; CSP constraints generated by scg_modeler\n");
        fprintf(out, ";\n");
        fprintf(out, "; [%8s] Naked  Singles\n",     clarg.NS_enabled ? "enabled": "disabled");
        fprintf(out, "; [%8s] Hidden Singles\n",    clarg.HS_enabled ? "enabled": "disabled");
        fprintf(out, "; [%8s] Locked Candidates\n", clarg.LC_enabled ? "enabled": "disabled");
        fprintf(out, ";\n");
        fprintf(out, "; rank  = %d\n",    data.rank);
        fprintf(out, "; size  = %d\n",    data.size);
        fprintf(out, "; max step = %d\n", data.bound);

        read_input(in, &data);

        fprintf(out, "; number of clues = %d\n", data.nclues);
        fprintf(out, "; clue cells:\n");
        print_cells(out, data.cs, data.nclues);

        // add rule and strategies
        add_sudoku_rule(&data); // mandatory
        if (clarg.NS_enabled) add_naked_singles_strategy(&data);
        if (clarg.HS_enabled) add_hidden_singles_strategy(&data);
        if (clarg.LC_enabled) add_locked_candidates_strategy(&data);

        // variable declaration
        fprint_decl_for_x(out, &data);
        fprint_decl_for_y(out, &data);
        fprint_decl_for_z(out, &data);

        // constraints for a general state transition framework
        fprint_cons_for_init (out, &data);
        fprint_cons_for_trans(out, &data);
        fprint_cons_for_final(out, &data);

        // constraints for particular strategies and rules
        fprint_cons_for_strat(out, &data);

        delete_data(&data);

        fclose(in);
        if (out != stdout) fclose(out);

        return 0;
}

static void usage (void)
{
        fprintf(stderr, "Usage: $ scg_modeler option file\n");
        fprintf(stderr, "-o file\tgenerate constraints to the specified file.\n");
        fprintf(stderr, "-N\tenable Naked  Singles\n");
        fprintf(stderr, "-H\tenable Hidden Singles\n");
        fprintf(stderr, "-L\tenable Locked Candidates\n");
        fprintf(stderr, "-r R\tRxR=N holds, where N is the number of rows.\n");
        fprintf(stderr, "-k K\tmaximum step size\n");
        fprintf(stderr, "-h\tthis message\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "[Author] Takahisa Toda <todat@acm.org>\n");
        fprintf(stderr, "Graduate School of Information Systems, the University of Electro-Communications\n");
        fprintf(stderr, "1-5-1 Chofugaoka, Chofu, Tokyo 182-8585, Japan\n");
        fprintf(stderr, "http://www.disc.lab.uec.ac.jp/toda/index-en.html\n");
}

static void print_cells (FILE *out, const cell_t *q, int len)
{

        for (int pos = 0; pos < len; pos++) {
                fprintf(out, "; %d %d\n", q[pos].I + 1, q[pos].J + 1);
        }
}

