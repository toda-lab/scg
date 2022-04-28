#!/bin/bash

gcc -std=c99 -o scg_modeler scg_main.c scg_modeler.c scg_assert.c naked_singles.c sudoku_rule.c hidden_singles.c locked_candidates.c


