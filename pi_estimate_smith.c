#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>

void runSequential(unsigned long long num_throws);
void runMPIParallel(unsigned long long num_throws, int mpi_rank, int mpi_size);
/* Get number of throws from the user
 */
unsigned long long int get_input() {
    long long int num_throws;
    printf("Enter the number of throws: ");
    scanf("%llu", &num_throws);
    return num_throws;
}


int main(int argc, char** argv) {
	// Initialize MPI
	MPI_Init(&argc, &argv);

	// Get MPI Rank and Size
	int mpi_rank;
	int mpi_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

	// Run sequential for testing purposes
	long long int num_throws = 0;
	if (mpi_rank == 0) {
		num_throws = get_input();
		runSequential(num_throws);
	}

	// Run MPI Parallel version
	runMPIParallel(num_throws, mpi_rank, mpi_size);

	return 0;
}

void runSequential(unsigned long long num_throws) {
	unsigned long long num_in_circle = 0;
	unsigned long long i;
	double x, y, pi;

	for (i=0; i < num_throws; i++) {
		x = 2*((double)rand() / (double)RAND_MAX) - 1;
		y = 2*((double)rand() / (double)RAND_MAX) - 1;

		if (x*x + y*y <= 1) num_in_circle++;
	}

	printf("Sequential Total in circle: %llu out of %llu\n", num_in_circle, num_throws);
	pi = (double)num_in_circle / (double) num_throws * 4;
	printf("Sequential Pi estimate: %f\n", pi);

}

void runMPIParallel(unsigned long long num_throws, int mpi_rank, int mpi_size) {
	if (mpi_rank == 0) printf("Start\n");
	// Broadcast chunk of num_throws to processes
	unsigned long long* num_throws_chunks = malloc(sizeof(unsigned long long));
	(*num_throws_chunks) = num_throws / mpi_size;
	unsigned long long remainder = num_throws % mpi_size;
	if (mpi_rank == 0) printf("Calc'd chunks: %llu\n", *num_throws_chunks);

	MPI_Bcast(num_throws_chunks, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
	if (mpi_rank == 0) printf("Broadcasted\n");

	// Run in parallel
	// Seed RNG
	srand(time(NULL)*mpi_rank);
	unsigned long long* num_in_circle = malloc(sizeof(unsigned long long));;
	*num_in_circle = 0;
	unsigned long long i;
	double x, y, pi;
	if (mpi_rank == 0) printf("Seeded. Running\n");
	if (mpi_rank == 0) { unsigned long long tempchunks = *num_throws_chunks;
		printf("Chunk Size: %llu\n", tempchunks);
	}
	for (i=0; i < (*num_throws_chunks); i++) {
		if (mpi_rank == 0) printf("i=%llu\n", i);
		x = 2*((double)rand() / (double)RAND_MAX) - 1;
		y = 2*((double)rand() / (double)RAND_MAX) - 1;

		if (x*x + y*y <= 1) (*num_in_circle)++;
	}
	if (mpi_rank == 0) printf("Running Remainder\n");

	// Run remainder on rank0 for uneven splits
	if (mpi_rank == 0) {
		for (i=0; i < remainder; i++) {
			x = 2*((double)rand() / (double)RAND_MAX) - 1;
			y = 2*((double)rand() / (double)RAND_MAX) - 1;

			if (x*x + y*y <= 1) (*num_in_circle)++;
		}
	}

	if (mpi_rank == 0) printf("Making res\n");
	// Reduce num_in_circle calculation
	unsigned long long* num_in_circle_res = NULL;
	if (mpi_rank == 0) {
		num_in_circle_res = malloc(sizeof(unsigned long long));
	}
	if (mpi_rank == 0) printf("Reducing\n");

	MPI_Reduce(num_in_circle, num_in_circle_res, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
	if (mpi_rank == 0) printf("Reduced\n");

	// Print Results
	if (mpi_rank == 0) {
		unsigned long long circleres = (*num_in_circle_res);
		printf("Are you alive?\n");
		printf("Parallel Total in circle: %llu out of %llu\n", circleres, num_throws);
		pi = (double)(circleres) / (double) num_throws * 4;
		printf("Parallel Pi estimate: %f\n", pi);

		// Cleanup for rank0
		free(num_in_circle_res);
	}

	// Cleanup
	free(num_in_circle);
	free(num_throws_chunks);
}
