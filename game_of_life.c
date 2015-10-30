#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<mpi.h>

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

int main(int argc, char** argv) {
    int rows, columns, rank, size, up_nbr, down_nbr;
    int *board, *local_swap_board, *sendcounts, *offsets, *tmp_swap, *rows_columns;
    int generations;
    int *local_board;
    if (argc != 2) {
        printf("Usage: ./life.x [generations]\n");
        exit(1);
    }

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Status status;
    board = NULL;
    rows_columns = malloc(sizeof(int) * 2);
    if (rank == 0) {
        board = get_initial_board(&rows, &columns);
        rows_columns[0] = rows;
        rows_columns[1] = columns;
        MPI_Bcast(rows_columns, 2, MPI_INT, 0, MPI_COMM_WORLD);// broadcast size of full board
    }
    else {
        MPI_Bcast(rows_columns, 2, MPI_INT, 0, MPI_COMM_WORLD);// receive broadcast of size of board
        rows = rows_columns[0];
        columns = rows_columns[1];
    }
    generations = atoi(argv[1]);
    //printf("Rows: %d; Columns: %d\n", rows, columns); // debug

    sendcounts = malloc(sizeof(int) * size); //array inexed by rank with num elements sent to each process

    for (int i = 0; i < size; i++) {
        sendcounts[i] = columns * ((rows / size) + 1);
    }
    int too_big = (size * ((rows / size) + 1)) - (size * (rows / size) + rows % size);
    //too_big is the number of extra rows I allocated to processes max of 1 extra per process


    int decrementer = 1; // used to count backwards in send counts and remove extra rows
    while (too_big > 0) {
        sendcounts[size - decrementer] -= columns; // remove a row by removing columns number of integers
        too_big--;
        decrementer++;
    }

    offsets = malloc(sizeof(int) * size);// array to inform rank 0 where to break up input array
    offsets[0] = 0;
    for (int i = 1; i < size; i++) {
        offsets[i] = offsets[i - 1] + sendcounts[i - 1];
    }

    local_board = malloc(sizeof(int) * ( 3 * ((rows / size) * columns) + columns));
    local_swap_board = malloc(sizeof(int) * ( 3 * ((rows / size) * columns) + columns));

    for (int gen = 0; gen < generations; gen++) {
        MPI_Scatterv(board, sendcounts, offsets, MPI_INT, local_board + columns, sendcounts[rank], MPI_INT, 0, MPI_COMM_WORLD);
        if (rank == 0){
            for (int i = 0; i < columns; i++) local_board[i] = 0;
            //set top ghost row of data to all zeros so as not to influence neighbor count
        }
        if (rank == size - 1){
            int start = sendcounts[rank] + columns;
            for (int i = start; i < start + columns; i++) local_board[i] = 0;
            //set bottom ghost row of data to all zeros so as not to influence neighbor count
        }

        up_nbr = rank + 1;
        if (up_nbr >= size) up_nbr = MPI_PROC_NULL;
        down_nbr = rank - 1;
        if (down_nbr < 0) down_nbr = MPI_PROC_NULL;

        if ((rank % 2) == 0) {
            /* exchange up */
            MPI_Sendrecv( local_board + sendcounts[rank], columns, MPI_INT, up_nbr, 0,
                          local_board + sendcounts[rank] + columns, columns, MPI_INT, up_nbr, 0,
                          MPI_COMM_WORLD, &status );

        }
        else {
            /* exchange down */
            MPI_Sendrecv( local_board + columns, columns, MPI_INT, down_nbr, 0,
                          local_board, columns, MPI_INT, down_nbr, 0,
                          MPI_COMM_WORLD, &status );
        }

        /* Do the second set of exchanges */
        if ((rank % 2) == 1) {
            /* exchange up */
            MPI_Sendrecv( local_board + sendcounts[rank], columns, MPI_INT, up_nbr, 1,
                          local_board + sendcounts[rank] + columns, columns, MPI_INT, up_nbr, 1,
                          MPI_COMM_WORLD, &status );
        }
        else {
            /* exchange down */
            MPI_Sendrecv( local_board + columns, columns, MPI_INT, down_nbr, 1,
                          local_board, columns, MPI_INT, down_nbr, 1,
                          MPI_COMM_WORLD, &status );
        }


        generation(local_swap_board, local_board, sendcounts[rank]/columns + 2, columns);

        MPI_Gatherv(local_swap_board + columns, sendcounts[rank], MPI_INT, board, sendcounts, offsets, MPI_INT, 0, MPI_COMM_WORLD);
    }

    if (rank == 0) {
    printf("Final board:\n");
    write_board(board, rows, columns);
    }

    free(local_board);
    free(board);
    free(local_swap_board);
    return 0;
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
            new_board[index1D(i, j, columns)] =
                next_value(old_board[index1D(i, j, columns)],
                    count_neighbors(old_board, i, j, rows, columns));
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
   assert(neighbors >= 0 && neighbors <= 8);

   if (neighbors < 2) return 0;  // dies from isolation if alive
   if (neighbors == 2) return cur_val; // keep current state
   if (neighbors == 3) return 1; // survives if alive; spawns if dead
   return 0;  // 4+ neighbors => dead
}

