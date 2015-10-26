#include <mpi.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>


/* Get number of throws from the user
 */
long long int get_input() {
    long long int num_throws;
    printf("Enter the number of throws: ");
    scanf("%lld", &num_throws);
    return num_throws;
}


int main(int argc, char **argv) {
    int rank, size;
    long long int num_in_circle = 0;
    long long int final_num_in_circle = 0;
    long long int num_throws, i;
    double x, y, pi, start_time, end_time, full_time, max_full_time;


    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Status status;
    if (rank == 0) {
        num_throws = get_input();
        long long int num_throws_per_thread = num_throws / size;
        long long int my_num_throws = num_throws_per_thread + num_throws % size;
        MPI_Bcast (&num_throws_per_thread, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
        start_time = MPI_Wtime();
        srand(time(NULL));
        for (i = 0; i < my_num_throws; i++) {
            x = 2 * ((double) rand() / (double) RAND_MAX) - 1;
            y = 2 * ((double) rand() / (double) RAND_MAX) - 1;

            if (x * x + y * y <= 1) num_in_circle++;
        }
    }
    else {
        long long int num_throws_per_thread;
        MPI_Bcast (&num_throws_per_thread, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
        start_time = MPI_Wtime();
        struct timeval time;
        gettimeofday(&time,NULL);
        srand((time.tv_sec * 1000 * rank) + (time.tv_usec / 1000));
        for (i = 0; i < num_throws_per_thread; i++) {
            x = 2 * ((double) rand() / (double) RAND_MAX) - 1;
            y = 2 * ((double) rand() / (double) RAND_MAX) - 1;

            if (x * x + y * y <= 1) num_in_circle++;
        }
    }
    MPI_Reduce (&num_in_circle, &final_num_in_circle, 1, MPI_LONG_LONG_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    end_time = MPI_Wtime();
    full_time = end_time - start_time;
    MPI_Reduce (&full_time, &max_full_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        printf("Total in circle: %lld out of %lld\n", final_num_in_circle, num_throws);
        pi = (double) final_num_in_circle / (double) num_throws * 4;
        printf("Pi estimate: %f\n", pi);
        printf("run time: %f\n", max_full_time);
    }
    MPI_Finalize();
    return 0;
}
