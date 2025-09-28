#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// void print_arr(char* s, int* arr, int n) {
//     printf("%s: ", s);
//     for (int i=0; i<n-1; i++) {
//         printf("%d, ", arr[i]);
//     }
//     printf("%d\n", arr[n-1]);
// }

// struct array_elm{
//     int a;
//     int b;
// };


// int compare(const void *a, const void *b) {
//     int x = *(const int *)a;
//     int y = *(const int *)b;
//     return (x > y) - (x < y);  // returns +1, 0, or -1
// }

// int compareInvers(const void *a, const void *b) {
//     int x = *(const int *)a;
//     int y = *(const int *)b;
//     return (x < y) - (x >y);
// }

// int main(int argc, char* argv[]) {
//     if (argc != 2) {
//         fprintf(stderr, "Usage: %s N\n", argv[0]);
//         exit(1);
//     }
//     int n = atoi(argv[1]);

//     int* arr = malloc(n*sizeof(int));

//     // If you want to generate the same numbers every time comment out the 
//     // below line
//     srand(time(NULL));

//     for (int i=0; i<n; i++) {
//         arr[i] = rand() % 100 + 1;
//     }

//     qsort(arr, n, sizeof(int), compare);

//     print_arr("Initial array\0", arr, n);

//     qsort(arr, n, sizeof(int), compareInvers);

//     print_arr("Initial array\0", arr, n);

//     // Most of your work should be done below here

//     free(arr);
// }


struct array_elem {
    int a;
    int b;
} typedef array_elem;

int compare_by_a(const void *x, const void *y) {
    const array_elem *e1 = x;
    const array_elem *e2 = y;
    return (e1->a > e2->a) - (e1->a < e2->a);
}

int compare_by_b(const void *x, const void *y) {
    const array_elem *e1 = x;
    const array_elem *e2 = y;
    return (e1->b > e2->b) - (e1->b < e2->b);
}



void print_struct_arr(char* s, array_elem* arr, int n) {
    printf("%s:\n", s);
    for (int i = 0; i < n; i++) {
        printf("  elem[%d]: a=%d, b=%d\n", i, arr[i].a, arr[i].b);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s N\n", argv[0]);
        exit(1);
    }
    int n = atoi(argv[1]);
    array_elem* arr = malloc(n * sizeof(array_elem));
    srand(time(NULL));

    for (int i = 0; i < n; i++) {
        arr[i].a = rand() % 100 + 1;
        arr[i].b = rand() % 100 + 1;
    }

    print_struct_arr("Random struct array", arr, n);

    qsort(arr, n, sizeof(struct array_elem), compare_by_a);
    print_struct_arr("Sorted by a", arr, n);

    qsort(arr, n, sizeof(struct array_elem), compare_by_b);
    print_struct_arr("Sorted by b", arr, n);

    free(arr);
    return 0;
}


