#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <float.h>

#include "record.h"
#include "coord_query.h"

// K-d tree node structure
struct kdtree {
  const struct record *rs;  // Pointer to the record
  int axis;                 // 0 for lon, 1 for lat
  struct kdtree *left;      // Left subtree
  struct kdtree *right;     // Right subtree
};


// Helper functions for qsort longitude
int cmp_lon(const void *a, const void *b) {               // Compare function for qsort by longitude
  const struct record *ra = *(const struct record **)a;   // Cast to pointer to pointer to struct record
  const struct record *rb = *(const struct record **)b;   // Cast to pointer to pointer to struct record
  if (ra->lon < rb->lon) return -1;                       // Compare longitudes
  if (ra->lon > rb->lon) return 1;                        // Compare longitudes
  return 0;
}
// Helper functions for qsort latitude                    // Compare function for qsort by latitude
int cmp_lat(const void *a, const void *b) {                 
  const struct record *ra = *(const struct record **)a;
  const struct record *rb = *(const struct record **)b;
  if (ra->lat < rb->lat) return -1;                       // Compare latitudes
  if (ra->lat > rb->lat) return 1;                        // Compare latitudes
  return 0;
}

// Recursive function to build the k-d tree
struct kdtree* build_kdtree(struct record **points, int n, int depth) { // points is an array of pointers to records
  if (n <= 0) return NULL;                                              // Base case: no points to process

  int axis = depth % 2;                                                  // Alternate between longitude (0) and latitude (1)
    if (axis == 0) {                                                     // if axis is 0, sort by longitude
        qsort(points, n, sizeof(struct record*), cmp_lon);               // Sort by longitude
    } else {                                                              // if axis is 1, sort by latitude
        qsort(points, n, sizeof(struct record*), cmp_lat);               // Sort by latitude
    }

    int median = n / 2;                                                  
    struct kdtree *node = malloc(sizeof(struct kdtree));                 // Allocate memory for the node
    node->rs = points[median];                                           // Set the record pointer to the median point
    node->axis = axis;                                                   // Set the axis

    node->left = build_kdtree(points, median, depth + 1);                // Recursively build the left subtree
    node->right = build_kdtree(points + median + 1, n - median - 1, depth + 1); // Recursively build the right subtree

  return node;
}

// Tree wrapper
struct kdtree* mk_kdtree(const struct record* rs, int n) {              // rs is the array of records
  struct record **coord = malloc(n * sizeof(struct record*));           // Allocate memory for an array of pointers to records
  if (!coord) {
      fprintf(stderr, "Failed to allocate memory for coord array (errno: %s)\n", strerror(errno)); 
      exit(1);                                                                
  }
  for (int i = 0; i < n; i++) {
      coord[i] = (struct record*)&rs[i];                                // Fill the array with pointers to the records
  }

    struct kdtree *tree = build_kdtree(coord, n, 0);                    // Build the k-d tree
    free(coord);
    return tree;

}

// Free the k-d tree
void free_kdtree(struct kdtree* tree) {                                 // Recursively free the k-d tree
    if (!tree) return;
    free_kdtree(tree->left);
    free_kdtree(tree->right);
    free(tree);
}

// Euclidian distance 
static inline double euclidian_distance(double lon1, double lat1, double lon2, double lat2) { 
    double dx = lon1 - lon2;                                        // Calculate the difference in longitude
    double dy = lat1 - lat2;                                        // Calculate the difference in latitude            
    return dx * dx + dy * dy;                                       // Return squared distance
}


// Recursive nearest neighbor search
void lookup_kdtree_recursive(struct kdtree *node, double lon, double lat, const struct record **best, double *best_dist) {  
    if (!node) return; // Base case: empty node

    double dist = euclidian_distance(node->rs->lon, node->rs->lat, lon, lat); // Calculate distance to current node
    if (dist < *best_dist) {                                                  // If this node is closer than the best found so far
        *best_dist = dist;                                                    // Update best distance
        *best = node->rs;                                                     // Update best record
    }

    int axis = node->axis;                                                    // Current axis (0 for lon, 1 for lat)
    struct kdtree *next_branch = NULL;                                        // Determine which branch to search next
    struct kdtree *opposite_branch = NULL;                                    // Determine the opposite branch

    if ((axis == 0 && lon < node->rs->lon) || (axis == 1 && lat < node->rs->lat)) {
        next_branch = node->left;                                             // Search left subtree
        opposite_branch = node->right;                                        // Search right subtree
    } else {
        next_branch = node->right;                                            // Search right subtree
        opposite_branch = node->left;                                         // Search left subtree
    }
 
    lookup_kdtree_recursive(next_branch, lon, lat, best, best_dist);          // Search the next branch

    double axis_dist = (axis == 0) ? (lon - node->rs->lon) : (lat - node->rs->lat); // Distance to the splitting plane, how far are we from the plane
    if (axis_dist * axis_dist < *best_dist) {                                       // If the distance to the plane is less than the best distance found so far
        lookup_kdtree_recursive(opposite_branch, lon, lat, best, best_dist);        // Search the opposite branch
    }
}

const struct record* lookup_kdtree(struct kdtree *tree, double lon, double lat) {  
    const struct record *best = NULL;                                               // Pointer to the best record found
    double best_dist = DBL_MAX;                                                     // Initialize best distance to a very large number
    lookup_kdtree_recursive(tree, lon, lat, &best, &best_dist);                     // Start the recursive search
    return best;                                                                    // Return the best record found

}

int main(int argc, char** argv) {
  return coord_query_loop(argc, argv,
                          (mk_index_fn)mk_kdtree,
                          (free_index_fn)free_kdtree,
                          (lookup_fn)lookup_kdtree);
}


