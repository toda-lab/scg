# SCG_MODELER, May 28th 2020
CSP encoder for the strategy-solvable Sudoku clues problem

# Files
```
data/
min30     30 minimum Sudokus randomly chosen from the collection of Gordon F. Royle
r2c3      all possible arrangements of 3 clue positions for 4x4 sudoku
r2c4      all possible arrangements of 4 clue positions for 4x4 sudoku
rand100   100 arrangements for 9x9 sudoku, generated by varing the position and the number of clues (from 20 to 79) at random.

src/
scg_modeler.c   CSP encoder for the strategy-solvable Sudoku clues problem

tool/
check_solvable.c  simple program to check solvability
str2in.c          encoder of grid format
out2str.c         decoder of grid format
```

# Compilation
The executable file of the CSP encoder, scg_modeler, will be generated by the following commands.
```
cd src
./compile.sh
```

The executable files of support tools, check_solvable, str2in, and out2str will be generated by the following commands. 
```
cd tool
gcc -std=c99 -o check_solvable check_solvable.c
gcc -std=c99 -o str2in         str2in.c
gcc -std=c99 -o out2str        out2str.c
```
# Usage of scg_modeler
```
Usage: scg_modeler option file
-o file	generate constraints to the specified file.
-N	enable Naked  Singles
-H	enable Hidden Singles
-L	enable Locked Candidates
-r R	RxR=N holds, where N is the number of rows.
-k K	maximum step size
-h	this message
```

# str2in
```
Usage: str2in string_of_grid
string_of_grid  string of length 4 or 9: 0 means a non-clue cell, and any other character means a clue cell.
```
- This program converts the string representation of a grid into a list of cells.
- The list-of-cells format is required for the input of scg_modeler.
- Example:
```
str2in 0*00000*00000*0* > scg.in
cat scg.in
1 2
2 4
4 2
4 4
scg_modeler -N -H -L -r 2 -k 10 scg.in > in.csp
sugar in.csp
```

# out2str
```
Usage: out2str rank sugar.out
rank   2 for 4x4 grid, 3 for 9x9 grid, etc
```

- This program reads the output file of Sugar CSP solver and prints out the string representation of an initial grid.
- Example:
```
scg_modeler -N -H -L -r 2 -k 10 r2/r2c4-997 > in.csp
sugar in.csp > tmp
cat tmp | grep "x_[0-9]_[0-9]_0\s\+[1-9]" | sed -e "s/^a\s\+x_\([0-9]\)_\([0-9]\)_0\s\+\([1-9]\)/\1 \2 \3/g" > sugar.out
cat sugar.out
2 0 3
3 0 2
0 2 1
1 2 3
out2str 2 sugar.out
0010003030002000
```

# check_solvable
```
Usage: check_solvable [option] str_of_grid
-b    bruteforce verification over all assignments
-N    enable Naked Singles
-H    enable Hidden Singles
-L    enable Locked Candidates
str_of_grid  string of length 4 or 9 representing a grid: the initial letter must not be a hyphen
```

- Example 1:
- Execute the following command to check, in a brute-force way, if there is a set of clues for the cells (0,1), (1,3), (3,1), and (3,3) solvable with naked singles, hidden singles, and locked candidates.
- Here, rows and columns are numbered from 0!
```
check_solvable -N -H -L -b 0*00000*00000*0*
```

- Example 2:
- Execute the following command to check if the set of clues (0,1)->3, (1,3)->2, (3,1)->4, and (3,3)->1 is solvable with naked singles, hidden singles, and locked candidates.
- Here, rows and columns are numbered from 0!
```
check_solvable -N -H -L 0300000200000401
```

# References
- Nishikawa, N. and Toda, T.: [Exact Method for Generating Strategy-Solvable Sudoku Clues](https://doi.org/10.3390/a13070171)
- Tamura, N. Sugar: [a SAT-based Constraint Solver](https://cspsat.gitlab.io/sugar/)
- Een, N.; Sorensson, N. [The MiniSat Page](http://minisat.se/)
- Davis, T. [The mathematics of Sudoku](http://www.geometer.org/mathcircles/sudoku.pdf)
- Royle, G.F. [A collection of 49,151 distinct Sudoku configurations with 17 entries](https://staffhome.ecm.uwa.edu.au/~00013890/sudokumin.php)

# Author
Takahisa Toda <todat@acm.org>
Graduate School of Information Systems, the University of Electro-Communications
1-5-1 Chofugaoka, Chofu, Tokyo 182-8585, Japan
[http://www.disc.lab.uec.ac.jp/toda/index-en.htm](http://www.disc.lab.uec.ac.jp/toda/index-en.html)
