#include <stdio.h>



// 1. Given an integer value int x, return the value multiplied by 8.
int multiplyBy8(int x) {
    return x << 3; // Using bitwise left shift to multiply by 8
}

// 2. Given an integer value int x, return true if the value is equal to 6.
int isEqualTo6(int x1) {
    return !(x1 ^ 6); // Using bitwise XOR to check equality
}

// 3. Given an integer value int x, return true if the value is less than or equal to 0.
int isLessThanOrEqualTo0(int x2) {
    return (x2 >> 31) | !x2;  // Check if x2 is negative or zero
}


// 4. Given integer values int x and int y, return true if the value of x and y are different.
int areDifferent(int x3, int y) { 
    return (x3 ^ y) != 0; // Using bitwise XOR to check if values are different
} 

int main(void) {
    int x = 6, x1 = 10, x2 = 3, x3 = 0;

    printf("multiplyBy8(%d) = %d\n", x, multiplyBy8(x));
    printf("isEqualTo6(%d) = %d\n", x, isEqualTo6(x));
    printf("isLessThanOrEqualTo0(%d) = %d\n", x2, isLessThanOrEqualTo0(x2));
    printf("isLessThanOrEqualTo0(%d) = %d\n", x3, isLessThanOrEqualTo0(x3));
    printf("areDifferent(%d, %d) = %d\n", x, x1, areDifferent(x, x1));
    printf("areDifferent(%d, %d) = %d\n", x, x, areDifferent(x, x));

    return 0;
}
    






