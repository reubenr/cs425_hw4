//
//  main.c
//  Game_of_Life_proto
//

#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<mpi.h>

#define NEIGHBOR_OFFSET 2

// Read initial board from stdin or file (via redirect) and return pointer to it
// File format will be:
// #rows #columns
// Followed by each row in the format: 001110100
// Board will be rectangular, but not necessarily square
// Returns the board (as a 1D array laid out in row-major order),
//   and populates the rows and columns variables
int* get_initial_board(int* rows, int* columns);
void write_board(int* board, int rows, int columns); // write board to stdout

// simulate 1 generation
void generation(int* new_board, int* old_board, int rows, int columns);

// return # neighbors of cell [row][column]
int count_neighbors(int* board, int r, int c, int rows, int columns);

// given # of neighbors and current value, return next value
int next_value(int cur_val, int neighbors);

// helper function to convert 2D to 1D index
int index1D(int r, int c, int columns) { return r * columns + c; }

void runGameOfLife(int mpi_rank, int mpi_size, int* initialboard, int maxgen, int rows, int cols);

int main(int argc, char** argv) {
    // MPI Init:
    MPI_Init(&argc, &argv);
    int mpi_rank;
    int mpi_size;
    
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    
    int rows;
    int cols;
    int maxgen;
    int* initialboard = NULL;
    if (mpi_rank == 0) {
        if (argc < 2) {
            printf("Usage: ./life.x generations\n");
            exit(1);
        }
        maxgen = atoi(argv[1]);
        initialboard = get_initial_board(&rows, &cols);
        write_board(initialboard, rows, cols);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    runGameOfLife(mpi_rank, mpi_size, initialboard, maxgen, rows, cols);
}

void runGameOfLife(int mpi_rank, int mpi_size, int* initialboard, int maxgen, int rows, int cols) {
    // Initialization Code
    // Broadcast rows and cols to every rank
    MPI_Bcast(&rows, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&cols, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&maxgen, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (mpi_rank == 1) {
        //printf("rows = %d, cols = %d, maxgen = %d\n", rows, cols, maxgen);
    }
    
    // Get # of rows per process
    int numRowsPerThread = rows/mpi_size;
    
    // Create local 2D array for given number of rows
    int remainderRows = rows % mpi_size;
    // Rank mpi_size-1 picks up the remaining slack... could be problematic...
    if (mpi_rank == mpi_size-1) {
        numRowsPerThread += remainderRows;
    }
    int* localNewBoard = malloc(sizeof(int) * (numRowsPerThread+NEIGHBOR_OFFSET) * cols);
    int* localOldBoard = malloc(sizeof(int) * (numRowsPerThread+NEIGHBOR_OFFSET) * cols);
    
    // Zero out game board
    for (int i=0; i<numRowsPerThread+NEIGHBOR_OFFSET; i++) {
        for (int j=0; j<cols; j++) {
            localNewBoard[index1D(i, j, cols)] = 0;
            localOldBoard[index1D(i, j, cols)] = 0;
        }
    }
    
    // Initialize the neighbor row numbers and pointers
    // Pointer to the up neighbor's row start
    int rowUpRank = mpi_rank-1;
    int* rowUp = localNewBoard;
    
    // Starting offset for local rows
    int* localRows = localNewBoard + cols;
    int* localOldRows = localOldBoard + cols;
    
    // Last row for local rows
    int* localLastRow = localNewBoard + (numRowsPerThread * cols);
    
    // Pointer to the down neighbors row start
    int rowDownRank = mpi_rank+1;
    int* rowDown = localNewBoard + ((numRowsPerThread+1) * cols);
    
    // Send each process its chunk of the array
    MPI_Scatter(initialboard, numRowsPerThread * cols, MPI_INT, localRows, numRowsPerThread * cols, MPI_INT, 0, MPI_COMM_WORLD);

    // Send rank-1 it's final chunks if any
    if ((remainderRows != 0) && (mpi_rank == 0)) {
        int* finalRows = initialboard + (mpi_size * numRowsPerThread * cols) + cols;
        //printf("R%d->R%d Sending remainder chunk\n", mpi_rank, mpi_size-1);
        MPI_Send(finalRows, remainderRows*cols, MPI_INT, mpi_size-1, 2, MPI_COMM_WORLD);
    }
    else if ((remainderRows != 0) && (mpi_rank == mpi_size-1)) {
        int* finalRows = localNewBoard + (numRowsPerThread+1 * cols);
        //printf("R%d<-R%d Receiving remainder chunk\n", mpi_rank, 0);
        MPI_Recv(finalRows, remainderRows*cols, MPI_INT, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    
    // Init Generation
    int currgen = 1;
    
    // MPI_Barrier
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Run each iteration in lockstep based on current generation #
    while (currgen <= maxgen) {
        MPI_Barrier(MPI_COMM_WORLD);
        
        // Store localNewBoard into previous board
        // Todo just swap pointers...
        for (unsigned int i=0; i<numRowsPerThread+NEIGHBOR_OFFSET; i++) {
            for (unsigned int j=0; j<cols; j++) {
                localOldBoard[index1D(i, j, cols)] = localNewBoard[index1D(i, j, cols)];
            }
        }
        
        // MPI_Barrier
        MPI_Barrier(MPI_COMM_WORLD);
        
        // Neighbor Synchronization Point
        // Send to +-1 ranks this previous board
        // Recv from +- ranks for their previous board
        if (mpi_rank == 0) {
            // Send last row to rank+1
            MPI_Send(localLastRow, cols, MPI_INT, mpi_rank+1, 0, MPI_COMM_WORLD);
            
            // Receive first row from rank+1
            MPI_Recv(rowDown, cols, MPI_INT, mpi_rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        else if (mpi_rank == mpi_size-1) {
            // Send first row to rank-1
            MPI_Send(localLastRow, cols, MPI_INT, mpi_rank-1, 0, MPI_COMM_WORLD);
            
            // Receive last row from rank-1
            MPI_Recv(rowUp, cols, MPI_INT, mpi_rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        else {
            // Send first row to rank-1
            MPI_Send(localLastRow, cols, MPI_INT, mpi_rank-1, 0, MPI_COMM_WORLD);
            
            // Receive last row from rank-1
            MPI_Recv(rowUp, cols, MPI_INT, mpi_rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            // Send last row to rank+1
            MPI_Send(localLastRow, cols, MPI_INT, mpi_rank+1, 0, MPI_COMM_WORLD);
            
            // Receive first row from rank+1
            MPI_Recv(rowDown, cols, MPI_INT, mpi_rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        
        // Probably not needed...
        MPI_Barrier(MPI_COMM_WORLD);
        
        // Generate new rows in localNewBoard
        if (mpi_rank == 0) {
            generation(localRows, localOldRows, numRowsPerThread+NEIGHBOR_OFFSET-1, cols);
        }
        else if (mpi_rank == mpi_size-1) {
            generation(localNewBoard, localOldBoard, numRowsPerThread+NEIGHBOR_OFFSET-1, cols);
        }
        else {
            generation(localNewBoard, localOldBoard, numRowsPerThread+NEIGHBOR_OFFSET, cols);
        }
        
        // Increment current generation and rerun
        currgen++;
    }

    // Gather all rows from processes and consolidate
    MPI_Gather(localRows, numRowsPerThread*cols, MPI_INT, initialboard, numRowsPerThread*cols, MPI_INT, 0, MPI_COMM_WORLD);
    
    
    // Gather remaining from mpi_size-1
    if ((remainderRows != 0) && (mpi_rank == 0)) {
        int* finalRows = initialboard + (mpi_size * numRowsPerThread * cols) + cols;
        MPI_Recv(finalRows, remainderRows*cols, MPI_INT, mpi_size-1, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    else if ((remainderRows != 0) && (mpi_rank == mpi_size-1)) {
        int* finalRows = localNewBoard + (numRowsPerThread+1 * cols);
        MPI_Send(finalRows, remainderRows*cols, MPI_INT, 0, 2, MPI_COMM_WORLD);
    }
    
    // Print out final board
    if (mpi_rank == 0) {
        printf("\nFinal Board:\n");
        write_board(initialboard, rows, cols);
    }
    
}

int* get_initial_board(int* rows, int* columns) {
    int* board;
    int i;
    scanf("%d %d", rows, columns);
    board = malloc(sizeof(int) * (*rows) * (*columns));
    for (i = 0; i < (*rows) * (*columns); i++) {
        scanf("%d", &(board[i]));
    }
    return board;
}

void write_board(int* board, int rows, int columns) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            printf("%d\t", board[index1D(i, j, columns)]);
        }
        printf("\n");
    }
}

void generation(int* new_board, int* old_board, int rows, int columns) {
    for (int i=0; i < rows; i++) {
        for (int j=0; j < columns; j++) {
            new_board[index1D(i, j, columns)] = next_value(old_board[index1D(i, j, columns)], count_neighbors(old_board, i, j, rows, columns));
        }
    }
}

int count_neighbors(int* board, int r, int c, int rows, int columns) {
    int neighbors = 0;

    // Top left
    if (r != 0 && c != 0)
       neighbors += board[index1D(r - 1, c - 1, columns)];
    // Top middle
    if (r != 0)
       neighbors += board[index1D(r - 1, c, columns)];
    // Top right
    if (r != 0 && c != columns - 1)
       neighbors += board[index1D(r - 1, c + 1, columns)];
    // Middle left
    if (c != 0)
       neighbors += board[index1D(r, c - 1, columns)];
    // Middle right
    if (c != columns - 1)
       neighbors += board[index1D(r, c + 1, columns)];
    // Bottom left
    if (r != rows - 1 && c != 0)
       neighbors += board[index1D(r + 1, c - 1, columns)];
    // Bottom middle 
    if (r != rows - 1)
       neighbors += board[index1D(r + 1, c, columns)];
    // Bottom right 
    if (r != rows - 1)
       neighbors += board[index1D(r + 1, c + 1, columns)];

    return neighbors;
}

int next_value(int cur_val, int neighbors) {
   // Given a cell's current value (1 or 0) and its number of live
   // neighbors (0-8), return its next value
   assert(cur_val == 0 || cur_val == 1);
   if (!(neighbors >= 0 && neighbors <= 8)) {
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    printf("Rank = %d, Neighbors = %d\n", rank, neighbors);
   }
   
   
   assert(neighbors >= 0 && neighbors <= 8);

   if (neighbors < 2) return 0;  // dies from isolation if alive
   if (neighbors == 2) return cur_val; // keep current state
   if (neighbors == 3) return 1; // survives if alive; spawns if dead
   return 0;  // 4+ neighbors => dead
}

