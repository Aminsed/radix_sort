// This is a header guard to prevent the file from being included multiple times
// It checks if RADIX_SORT_H is not defined, and if not, it defines it
#ifndef RADIX_SORT_H
#define RADIX_SORT_H

// These are the necessary header files for the code to work properly
// stdlib.h provides functions for memory allocation and deallocation
// string.h provides functions for string manipulation
// pthread.h provides functions for creating and managing threads
// immintrin.h provides intrinsic functions for SIMD operations
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <immintrin.h>

// These are some constants used throughout the code
// MAX_THREADS is the maximum number of threads that can be used for sorting
// RADIX is the number of possible values for each digit (0-255 for 8-bit digits)
// INSERTION_SORT_THRESHOLD is the threshold for switching to insertion sort for small subarrays
#define MAX_THREADS 8
#define RADIX 256
#define INSERTION_SORT_THRESHOLD 32

// This is a structure to hold the sorting parameters for each thread
// It contains pointers to the array, the range of elements to sort,
// the current digit being processed, and various arrays for counting and output
typedef struct {
    int* ptr_to_arr;  // Pointer to the array to be sorted
    int start_idx;  // Starting index of the subarray
    int end_idx;  // Ending index of the subarray
    int digit_exp;  // Current digit being processed (0 for least significant digit)
    int* cnt_arr_local;  // Local count array for each thread
    int* cnt_arr_global;  // Global count array shared by all threads
    int* sorted_arr;  // Output array for storing sorted elements
} SortParams;

// This is the insertion sort function
// It sorts a subarray of arr[] from index start_idx to end_idx
void sort_insertion(int arr[], int start_idx, int end_idx) {
    int i, j, key;
    // Loop through the subarray starting from index start_idx+1
    for (i = start_idx + 1; i <= end_idx; i++) {
        key = arr[i];  // Store the current element as the key
        j = i - 1;
        // Shift elements greater than the key to the right
        while (j >= start_idx && arr[j] > key) {
            arr[j + 1] = arr[j];
            j = j - 1;
        }
        arr[j + 1] = key;  // Place the key in its correct position
    }
}

// This is the counting sort function
// It sorts a subarray of arr[] from index start_idx to end_idx based on the current digit
void sort_counting(int arr[], int start_idx, int end_idx, int digit_exp, int* cnt_arr_local, int* cnt_arr_global, int* sorted_arr) {
    int i, digit;
    // Count the occurrences of each digit in the current subarray
    for (i = start_idx; i <= end_idx; i++) {
        // Extract the current digit from the element using bitwise operations
        // (arr[i] >> (8 * digit_exp)) shifts the element right by 8*digit_exp bits
        // This is equivalent to dividing the element by 256^digit_exp
        // For example, if digit_exp is 0, it shifts by 0 bits (no shift)
        // If digit_exp is 1, it shifts by 8 bits (dividing by 256)
        // If digit_exp is 2, it shifts by 16 bits (dividing by 256^2), and so on
        // & 0xFF masks out all bits except the least significant 8 bits (current digit)
        // 0xFF is the hexadecimal representation of the binary number 11111111
        // By performing a bitwise AND with 0xFF, we keep only the least significant 8 bits
        // This effectively extracts the current digit from the element
        digit = (arr[i] >> (8 * digit_exp)) & 0xFF;
        cnt_arr_local[digit] = cnt_arr_local[digit] + 1;
    }

    // Accumulate the local count into the global count
    for (i = 0; i < RADIX; i++)
        cnt_arr_global[i] = cnt_arr_global[i] + cnt_arr_local[i];

    // Compute the starting positions of each digit in the output array
    for (i = 1; i < RADIX; i++)
        cnt_arr_global[i] = cnt_arr_global[i] + cnt_arr_global[i - 1];

    // Distribute the elements into the output array based on their digits
    for (i = end_idx; i >= start_idx; i--) {
        digit = (arr[i] >> (8 * digit_exp)) & 0xFF;
        sorted_arr[cnt_arr_global[digit] - 1] = arr[i];
        cnt_arr_global[digit] = cnt_arr_global[digit] - 1;
    }

    // Copy the sorted elements back to the original array
    for (i = start_idx; i <= end_idx; i++)
        arr[i] = sorted_arr[i - start_idx];
}

// This is the thread function for Radix Sort
// It is called by each thread to sort its assigned subarray
void* sort_radix_thread(void* arg) {
    SortParams* params = (SortParams*)arg;  // Cast the void pointer to SortParams
    int size = params->end_idx - params->start_idx + 1;  // Calculate the size of the subarray

    // If the subarray is small enough, use insertion sort
    if (size <= INSERTION_SORT_THRESHOLD) {
        sort_insertion(params->ptr_to_arr, params->start_idx, params->end_idx);
    }
    // Otherwise, use counting sort
    else {
        sort_counting(params->ptr_to_arr, params->start_idx, params->end_idx, params->digit_exp,
                      params->cnt_arr_local, params->cnt_arr_global, params->sorted_arr);
    }

    return NULL;
}

// This is the main Radix Sort function
// It sorts the array arr[] of size n using multiple threads
void sort_radix(int arr[], int n) {
    int i, max_elem = arr[0];
    // Find the maximum element in the array
    for (i = 1; i < n; i++) {
        if (arr[i] > max_elem)
            max_elem = arr[i];
    }

    // Allocate memory for global count array and output array
    // calloc is used to allocate memory and initialize all elements to zero
    // It takes two arguments: the number of elements and the size of each element
    // Here, we allocate RADIX elements of size sizeof(int) for the global count array
    // and n elements of size sizeof(int) for the output array
    int* cnt_arr_global = (int*)calloc(RADIX, sizeof(int));
    int* sorted_arr = (int*)malloc(n * sizeof(int));

    int digit_exp;
    // Iterate through each digit from least significant to most significant
    for (digit_exp = 0; max_elem >> (8 * digit_exp) > 0; digit_exp++) {
        pthread_t threads[MAX_THREADS];  // Array to store thread identifiers
        SortParams params[MAX_THREADS];  // Array to store sorting parameters for each thread
        int* cnt_arrs_local[MAX_THREADS];  // Array to store local count arrays for each thread

        // Allocate memory for local count arrays
        // Each thread gets its own local count array
        // We use calloc to allocate RADIX elements of size sizeof(int) for each local count array
        for (i = 0; i < MAX_THREADS; i++)
            cnt_arrs_local[i] = (int*)calloc(RADIX, sizeof(int));

        int chunk_size = (n + MAX_THREADS - 1) / MAX_THREADS;  // Calculate the chunk size for each thread
        // Create threads and assign sorting tasks
        for (i = 0; i < MAX_THREADS; i++) {
            params[i].ptr_to_arr = arr;
            params[i].start_idx = i * chunk_size;
            params[i].end_idx = (i == MAX_THREADS - 1) ? n - 1 : (i + 1) * chunk_size - 1;
            params[i].digit_exp = digit_exp;
            params[i].cnt_arr_local = cnt_arrs_local[i];
            params[i].cnt_arr_global = cnt_arr_global;
            params[i].sorted_arr = sorted_arr;
            // pthread_create is used to create a new thread
            // It takes four arguments:
            // - a pointer to the thread identifier (pthread_t*)
            // - thread attributes (NULL for default attributes)
            // - the thread function to be executed (sort_radix_thread)
            // - a pointer to the argument to be passed to the thread function (void*)
            // Here, we pass the address of the SortParams struct as the argument
            pthread_create(&threads[i], NULL, sort_radix_thread, (void*)&params[i]);
        }

        // Wait for all threads to complete
        // pthread_join is used to wait for a thread to terminate
        // It takes two arguments:
        // - the thread identifier (pthread_t)
        // - a pointer to the return value of the thread function (NULL if not needed)
        // Here, we wait for each thread to finish execution before proceeding
        for (i = 0; i < MAX_THREADS; i++)
            pthread_join(threads[i], NULL);

        // Reset the global count array
        // memset is used to fill a block of memory with a specific value
        // It takes three arguments:
        // - a pointer to the memory block (cnt_arr_global)
        // - the value to be set (0)
        // - the number of bytes to be set (RADIX * sizeof(int))
        // Here, we reset all elements of the global count array to zero
        memset(cnt_arr_global, 0, RADIX * sizeof(int));

        // Free the memory allocated for local count arrays
        // free is used to deallocate the memory previously allocated by malloc or calloc
        // It takes one argument:
        // - a pointer to the memory block to be deallocated (cnt_arrs_local[i])
        // Here, we free the memory allocated for each local count array
        for (i = 0; i < MAX_THREADS; i++)
            free(cnt_arrs_local[i]);
    }

    // Free the memory allocated for global count array and output array
    // We use free to deallocate the memory allocated by calloc and malloc
    free(cnt_arr_global);
    free(sorted_arr);
}

// This is the end of the header guard
#endif // RADIX_SORT_H
