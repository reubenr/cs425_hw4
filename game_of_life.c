#include<stdio.h>
#include<stdlib.h>
#include<assert.h>

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
    int rows, columns;
    int generations;
    int* board, *swap_board, *tmp_swap;

    if (argc != 2) {
        printf("Usage: ./life.x [generations]\n");
        exit(1);
    }

    board = get_initial_board(&rows, &columns);
    swap_board = malloc(sizeof(int) * rows * columns);
    generations = atoi(argv[1]);
    printf("Rows: %d; Columns: %d\n", rows, columns); // debug

    for (int gen = 0; gen < generations; gen++) {
        generation(swap_board, board, rows, columns);

        // Swap pointers
        tmp_swap = board;
        board = swap_board;
        swap_board = tmp_swap;
 
        // Leave on for debugging, turn off for real run
        write_board(board, rows, columns);
        printf("\n");
    }

    printf("Final board:\n");
    write_board(board, rows, columns); 

    free(board);
    free(swap_board);
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

