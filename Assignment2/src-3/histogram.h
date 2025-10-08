// This header file contains not just function prototypes, but also
// the definitions.  This means it does not need to be compiled
// separately.
//
// You should not need to modify this file.

#ifndef HISTOGRAM_H
#define HISTOGRAM_H

// Move the cursor down 'n' lines.  Negative 'n' supported.
static void move_lines(int n) {
  if (n < 0) {
    printf("\033[%dA", -n);
  } else {
    printf("\033[%dB", n);
  }
}

// Clear from cursor to end of line.
static void clear_line() {
  printf("\033[K");
}

// Print a visual representation of a histogram to the screen.  After
// printing, the cursor is moved back to the beginning of the output.
// This means that next time print_histogram() is called, the previous
// output will be overwritten.
static void print_histogram(int histogram[8]) {
  int64_t bits_seen = 0;

  for (int i = 0; i < 8; i++) {
    bits_seen += histogram[i];
  }

  for (int i = 0; i < 8; i++) {
    clear_line();
    printf("Bit %d: ", i);

    double proportion = histogram[i] / ((double)bits_seen);
    for (int i = 0; i < 60*proportion; i++) {
      printf("*");
    }
    printf("\n");
  }

  clear_line();
  printf("%ld bits processed.\n", (long)bits_seen);
  move_lines(-9);
}

// Merge the former histogram into the latter, setting the former to
// zero in the process.
static void merge_histogram(int from[8], int to[8]) {
  for (int i = 0; i < 8; i++) {
    to[i] += from[i];
    from[i] = 0;
  }
}

// Update the histogram with the bits of a byte.
static void update_histogram(int histogram[8], unsigned char byte) {
  // For all bits in a byte...
  for (int i = 0; i < 8; i++) {
    // count if bit 'i' is set.
    if (byte & (1<<i)) {
      histogram[i]++;
    }
  }
}

#endif
