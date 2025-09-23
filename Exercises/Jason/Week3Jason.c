#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int main(void){
    //1.
// int x;
// int *p;
// p = &x;
// *p = 0;

// 2.
// int x;
// int *p;
// int **pp;
// pp = &p;
// p = &x;
// **pp =0;

//3.
// int x, y;
// int *p = &x;
// p = &x; //Irrelevant call? Is already initialized to be the adress of x in line 23
// *p = 0;
// p = &y;
// *p = 1;

int x;
int* arr[2];
arr[0] = &x;
arr[1] = arr[0];
*(arr[1]) = 1;
*(arr[0]) = *(arr[0]) - 1;

printf("x = %d\n", x);     // print the value of x 
    return 0;
}
