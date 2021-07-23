## Parallel Othello Player 
### How to run lightweight Othello version:

1. Clone git repo and cd into the it.  
2. Run the make file by typing 'make'.   
3. Run the script by typing './runplayeronly.sh'.  

### Parallel Implementation 
In the Othello project, I made use of various methods and mechanisms to achieve and aid parallelization. 

#### MiniMax Algorithm
One of the most important elements in this project is the Minimax Algorithm - used for searching. Given it is a search tree algorithm, it can take a relatively 'long' time to execute. In order to minimize waiting time, this algorithm is divided and run by more than one process.  

In my implementation, worker processes are given a subset of moves from the legal move array determined by the current board state. Each process executes a minimax search on its of its moves - the 'best' move is sent back to the master process. The master process plays the best move out of the moves sent by the processes.

#### Alpha Beta Pruning
Without alpha beta pruning, there is the chance that certain branches are explored when they do not need to. In the realm of parallelism, we do not want to be executing code we do not have to - as it would unnecessarily take up time. 

#### Sharing of Alpha Beta Values
Even though alpha beta pruning has already increased efficiency, other processes could still be exploring branches which should be pruned.
In order to overcome this - at least to an extent - my Othello program shares the alpha beta values with other processes at the same depth. 

#### Iterative Deepening
Iterative deeping runs the minimax algorithm to the max depth, but it runs it to each preceeding depth seperately. The reason for the implementation of this at these shallow depths is to allow alpha beta pruning to work more efficiently. 

#### Sorting of Moves
Another implementation mechanism to aid alpha beta pruning is the sorting of legal moves prior to the search. The current moves are sorted from 'best' to 'worst' - these values are determined by a weighting function. 

### Evaluation Function - Finding the 'best' move
Determining the best move was quite challenging. Not so much with regards to implementation, but more so with determining if it actually is the best move. I tried various things in my evaluation function - of which some ended up scoring even worse than the random move generation. I settled on a function which seems to work fine, but I would not always bet on it.

The function combines a weighting evaluation - where different moves have different weightings. For instance, corner pieces would have a higher rating than a middle board piece. On top of this, I combined the weighting evaluation with the number of legal moves a player has left on the end state board. 








