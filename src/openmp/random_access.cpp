#include <iostream>
#include <omp.h>
#include <cstdlib>
#include <chrono>
#include <ctime> 

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "Usage: ./random_access <array_size>" << std::endl;
        return -1;
    }
    int N_array = atoi(argv[1]);
    int N_test = 1000000;
    int* A = new int[N_array];
    int* B = new int[N_array];
    int* R = new int[N_test];
    for (int i = 0; i < N_array; i++) {
        A[i] = rand() % 10; // fill in some arbitrary values
    }
    for (int i = 0; i < N_test; i++) {
        R[i] = rand() % N_array; // fill in some arbitrary values
    }

    auto time_before = std::chrono::system_clock::now();

    #pragma omp parallel
    {
    #pragma omp for
    for (int i = 0; i < N_test; i++) {
        B[i%N_array] = A[R[i]];
    }
    }

    auto time_after_read = std::chrono::system_clock::now();
    std::chrono::duration<double> read_time = time_after_read - time_before;
    
    #pragma omp parallel
    {
    #pragma omp for
    for (int i = 0; i < N_test; i++) {
        B[R[i]] = A[i%N_array];
    }
    }

    auto time_after_write = std::chrono::system_clock::now();
    std::chrono::duration<double> write_time = time_after_write - time_after_read;

    std::cout << read_time.count() << std::endl;
    std::cout << write_time.count() << std::endl;

}