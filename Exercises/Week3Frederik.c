#include <stdio.h>

int main(void) {
/*
    int x;
    int *p;
    p = &x;
    *p = 0;
    printf("x = %d\n", x);

    int y;
    int *q;
    int **qq;
    q = &y;
    qq = &q;
    **qq = 0;
    printf("y = %d\n", y);


int x, y;
int *p = &x;
p = &x;
*p = 0;
p = &y;
*p = 1;
printf("x = %d, y = %d\n", x, y);


    return 0;
    */

    int x, y;
    int* arr[2];
    arr[0] = &x;
    arr[1] = arr[0];
    *(arr[1]) = 1;
    *(arr[0]) = *(arr[0]) - 1;
    printf("x = %d, y = %d\n", x, y);

    return 0;

    
}





