# Radix Sort

This is an implementation of the Radix Sort algorithm in C. It is inspired by the book "Algorithms in C" by Robert Sedgewick.

## Usage

To use the Radix Sort algorithm in your C project, follow these steps:


#include "radix_sort.h"
#include <stdio.h>

int main() {
    int arr[] = {170, 45, 75, 90, 802, 24, 2, 66};
    int n = sizeof(arr) / sizeof(arr[0]);

    sort_radix(arr, n);

    // Print the sorted array
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    return 0;
}
