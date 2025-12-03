#include <stdint.h>
#include "simulate.h"
#include "memory.h"
#include "read_elf.h"
#include <stdio.h>
#include <stdlib.h>
#define MAX_BPT_SIZE (16 * 1024)


//Branch history register for gshare
uint32_t gshare_bhr = 0;

//Tabeller for 2-bit saturating counters (0-3)
uint8_t bimodal_bpt[MAX_BPT_SIZE]; 
uint8_t gshare_bpt[MAX_BPT_SIZE];

//Struktur til at holde styr på statistikken for hver predictor:
typedef struct {
    long long total_predictions;
    long long mispredictions;
} PredictorStats;

//Array af statistikker for nemt at logge resultaterne
PredictorStats predictor_results[4];    

//Enum for nem identifikation af de 4 predictor
typedef enum {
    P_NT = 0,       // Always Not Taken
    P_BTFNT = 1,    // Backwards Taken, Forwards Not Taken
    P_BIMODAL = 2,  // Bimodal Predictor
    P_GSHARE = 3,   // Gshare Predictor
} PredictorType;

//Størrelsen af Bimodal/gshare predictor tabellerne, der skal bruges i simuleringen
int active_bimodal_size = 256;
int active_gshare_size = 256;

//Funktion til at initialisere predictor tabellerne
void init_predictors() {
    for (int i = 0; i < 4; i++){
        predictor_results[i].total_predictions = 0;
        predictor_results[i].mispredictions = 0;
    }
    //Initialiser Bimodal og Gshare BPTs til "svagt ikke taget" (værdi 1)
    memset(bimodal_bpt, 1, MAX_BPT_SIZE);
    memset(gshare_bpt, 1, MAX_BPT_SIZE);
    gshare_bhr = 0;
}

// Hjælper funktion til at opdatere en 2-bit tæller
static void update_2bit_counter(uint8_t *counter, int actual_taken) {
    if (actual_taken) {
        if (*counter < 3) {
            (*counter)++;
        }
    } else {
        if (*counter > 0) {
            (*counter)--;
        }
    }
}

//Håndter forudsigelse, tæller fejl og opdaterer tilstanden for alle 4 predictors
void process_branch_prediction(uint32_t pc, int32_t branch_offset, int actual_taken) {

    // For NT og BTFNT
    // [0] NT (Always not taken)
    int nt_predicted = 0;

    // [1] BFTNT (Backwards taken, forwards not taken)
    int btfnt_predicted = (branch_offset < 0);

    // 2. For Bimodal og Gshare (Dynamiske)
    // Bimodal prediction
    uint32_t bimodal_index = (pc >> 2) & (active_bimodal_size - 1);

    // Gshare prediction
    uint32_t gshare_index = ((pc >> 2) ^ (gshare_bhr)) & (active_gshare_size - 1);

    // [2] Bimodal - hvis tæller >= 2, forudsiges taget
    int bimodal_predicted = (bimodal_bpt[bimodal_index] >= 2);

    // [3] Gshare - hvis tæller >=2, forudsiges taget
    int gshare_predicted = (gshare_bpt[gshare_index] >= 2);

    // 3. Tæl fejl og opdater statistik:
    int predictions[4] = {nt_predicted, btfnt_predicted, bimodal_predicted, gshare_predicted};
    for (int i = 0; i < 4; i++) {
        predictor_results[i].total_predictions++;
        if (predictions[i] != actual_taken) {
            predictor_results[i].mispredictions++;
        }
    }

    // 4. Opdater tilstand for Bimodal og Gshare predictors

    //opdater Bimodal BPT
    update_2bit_counter(&bimodal_bpt[bimodal_index], actual_taken);

    //opdater Gshare BPT
    update_2bit_counter(&gshare_bpt[gshare_index], actual_taken);

    //Opdater Gshare BHR
    gshare_bhr = ((gshare_bhr << 1) | (actual_taken ? 1 : 0)) & (active_gshare_size - 1); //Bevæger BHR og tilføjer den faktiske grenstatus
    gshare_bhr &= (active_gshare_size - 1); //Masker til størrelsen af gshare predictor
}
