#!/bin/bash

mpirun -n 2 player/main 4 black.txt 
#mpirun -n 4 --oversubscribe ./hello

#run program multiple times too see if a deadlock occurs...
#max=10
#for (( i=0; i <= $max; ++i ))
#do
#    mpirun -n 4 player/main 4 black.txt 
#done