#include "simulate.h"
#include "extractor.h"
#include "disassemble.h"
#include "memory.h"
#include "read_elf.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // Required for memset

// ==========================================
//    BRANCH PREDICTION LOGIC (From Main)
// ==========================================

#define MAX_BPT_SIZE (16 * 1024)

// Branch history register for gshare
uint32_t gshare_bhr = 0;

// Tables for 2-bit saturating counters (0-3)
uint8_t bimodal_bpt[MAX_BPT_SIZE]; 
uint8_t gshare_bpt[MAX_BPT_SIZE];

// Structure to track stats for each predictor:
typedef struct {
    long long total_predictions;
    long long mispredictions;
} PredictorStats;

// Array of stats to log results easily
PredictorStats predictor_results[4];    

// Enum for easy identification of the 4 predictors
typedef enum {
    P_NT = 0,       // Always Not Taken
    P_BTFNT = 1,    // Backwards Taken, Forwards Not Taken
    P_BIMODAL = 2,  // Bimodal Predictor
    P_GSHARE = 3,   // Gshare Predictor
} PredictorType;

// Size of Bimodal/gshare predictor tables to use in simulation
int active_bimodal_size = 256;
int active_gshare_size = 256;

// Function to initialize predictor tables
void init_predictors() {
    for (int i = 0; i < 4; i++){
        predictor_results[i].total_predictions = 0;
        predictor_results[i].mispredictions =