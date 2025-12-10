#include "simulate.h"
#include "extractor.h"
#include "disassemble.h"
#include "memory.h"
#include "read_elf.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

//    BRANCH PREDICTION DATA STRUCTURES

#define NUM_SIZES 4
const int BP_SIZES[NUM_SIZES] = {256, 1024, 4096, 16384};

typedef struct {
    long long predictions;
    long long mispredictions;
} BPStats;

uint8_t bimodal_table[4][16384]; 
uint8_t gshare_table[4][16384];
uint32_t global_history = 0;

BPStats stats_nt;
BPStats stats_btfnt;
BPStats stats_bimodal[NUM_SIZES];
BPStats stats_gshare[NUM_SIZES];

uint8_t update_2bit_counter(uint8_t counter, int taken) {
    if (taken) {
        if (counter < 3) return counter + 1;
    } else {
        if (counter > 0) return counter - 1;
    }
    return counter;
}

int predict_2bit(uint8_t counter) {
    return (counter >= 2);
}

void init_predictors() {
    memset(&stats_nt, 0, sizeof(BPStats));
    memset(&stats_btfnt, 0, sizeof(BPStats));
    memset(stats_bimodal, 0, sizeof(BPStats) * NUM_SIZES);
    memset(stats_gshare, 0, sizeof(BPStats) * NUM_SIZES);
    global_history = 0;
    memset(bimodal_table, 1, sizeof(bimodal_table));
    memset(gshare_table, 1, sizeof(gshare_table));
}

void update_branch_stats(unsigned int pc, int taken, int offset) {
    stats_nt.predictions++;
    if (taken) stats_nt.mispredictions++;

    stats_btfnt.predictions++;
    int prediction_btfnt = (offset < 0);
    if (prediction_btfnt != taken) stats_btfnt.mispredictions++;

    for (int i = 0; i < NUM_SIZES; i++) {
        int size = BP_SIZES[i];
        int mask = size - 1;

        // Bimodal
        int bim_idx = (pc >> 2) & mask;
        int bim_pred = predict_2bit(bimodal_table[i][bim_idx]);
        stats_bimodal[i].predictions++;
        if (bim_pred != taken) stats_bimodal[i].mispredictions++;
        bimodal_table[i][bim_idx] = update_2bit_counter(bimodal_table[i][bim_idx], taken);

        // gShare
        int gs_idx = ((pc >> 2) ^ global_history) & mask;
        int gs_pred = predict_2bit(gshare_table[i][gs_idx]);
        stats_gshare[i].predictions++;
        if (gs_pred != taken) stats_gshare[i].mispredictions++;
        gshare_table[i][gs_idx] = update_2bit_counter(gshare_table[i][gs_idx], taken);
    }
    global_history = (global_history << 1) | (taken & 1);
}

//    FILE I/O SUPPORT

FILE* open_files[16] = {NULL}; // Simple file table

// Helper function to read null-terminated string from memory
void mem_read_str(struct memory* mem, int addr, char* buf, int max) {
    int i = 0;
    while (i < max - 1) {
        char c = (char)memory_rd_b(mem, addr + i);
        buf[i] = c;
        if (c == 0) break;
        i++;
    }
    buf[i] = 0;
}

//    SIMULATOR LOGIC

int regs[32]; 

int read_reg(int r) {
    if (r == 0) return 0;
    return regs[r];
}

// write to register file with logging
void write_reg(int r, int val, FILE* log) {
    if (r == 0) return;
    regs[r] = val;
    if (log) fprintf(log, " R[%d] <- %x", r, val);
}

struct Stat simulate(struct memory* mem, int start_addr, FILE* log_file, struct symbols* symbols) {
    struct Stat stats = {0};
    unsigned int pc = start_addr;
    int running = 1;
    
    for (int i = 0; i < 32; i++) regs[i] = 0;
    init_predictors();
    
    // Setup stdio i fil-tabel
    open_files[0] = stdin;
    open_files[1] = stdout;
    open_files[2] = stderr;

    char dis_buffer[128];

    // Exeqution loop
    while (running) {
        unsigned int instruction = memory_rd_w(mem, pc);
        unsigned int next_pc = pc + 4;

        unsigned int opcode = OPCODE(instruction);
        unsigned int rd = RD(instruction);
        unsigned int rs1 = RS1(instruction);
        unsigned int rs2 = RS2(instruction);
        unsigned int funct3 = FUNCT3(instruction);
        unsigned int funct7 = FUNCT7(instruction);
        
        int32_t imm_i = IMM_I(instruction);
        int32_t imm_s = IMM_S(instruction);
        int32_t imm_b = IMM_B(instruction);
        int32_t imm_u = IMM_U(instruction);
        int32_t imm_j = IMM_J(instruction);

        if (log_file) {
            disassemble(pc, instruction, dis_buffer, 128, symbols);
            fprintf(log_file, "%6ld => %8x : %08x %-30s", stats.insns + 1, pc, instruction, dis_buffer);
        }

        int taken_branch = 0;
        int is_branch = 0;

        // Instruction execution
        switch (opcode) {
            case 0x33: // R-Type
                {
                    int val1 = read_reg(rs1);
                    int val2 = read_reg(rs2);
                    int res = 0;
                    if (funct7 == 0x01) { // RV32M
                        switch (funct3) {
                            case 0: res = val1 * val2; break;
                            case 1: res = (int)((long long)val1 * (long long)val2 >> 32); break;
                            case 2: res = (int)((long long)val1 * (unsigned int)val2 >> 32); break;
                            case 3: res = (int)((unsigned long long)(unsigned int)val1 * (unsigned int)val2 >> 32); break;
                            case 4: res = (val2 == 0) ? -1 : (val1 == -2147483648 && val2 == -1) ? -2147483648 : val1 / val2; break;
                            case 5: res = (val2 == 0) ? -1 : (unsigned int)val1 / (unsigned int)val2; break;
                            case 6: res = (val2 == 0) ? val1 : (val1 == -2147483648 && val2 == -1) ? 0 : val1 % val2; break;
                            case 7: res = (val2 == 0) ? val1 : (unsigned int)val1 % (unsigned int)val2; break;
                        }
                    } else { // RV32I
                        switch (funct3) {
                            case 0: res = (funct7 == 0x20) ? (val1 - val2) : (val1 + val2); break;
                            case 1: res = val1 << (val2 & 0x1F); break;
                            case 2: res = (val1 < val2) ? 1 : 0; break;
                            case 3: res = ((unsigned int)val1 < (unsigned int)val2) ? 1 : 0; break;
                            case 4: res = val1 ^ val2; break;
                            case 5: res = (funct7 == 0x20) ? (val1 >> (val2 & 0x1F)) : ((unsigned int)val1 >> (val2 & 0x1F)); break;
                            case 6: res = val1 | val2; break;
                            case 7: res = val1 & val2; break;
                        }
                    }
                    write_reg(rd, res, log_file);
                }
                break;
            case 0x13: // I-Type
                {
                    int val1 = read_reg(rs1);
                    int res = 0;
                    switch (funct3) {
                        case 0: res = val1 + imm_i; break;
                        case 1: res = val1 << (imm_i & 0x1F); break;
                        case 2: res = (val1 < imm_i) ? 1 : 0; break;
                        case 3: res = ((unsigned int)val1 < (unsigned int)imm_i) ? 1 : 0; break;
                        case 4: res = val1 ^ imm_i; break;
                        case 5: 
                            if ((imm_i >> 5) & 0x20) res = val1 >> (imm_i & 0x1F);
                            else res = (unsigned int)val1 >> (imm_i & 0x1F);
                            break;
                        case 6: res = val1 | imm_i; break;
                        case 7: res = val1 & imm_i; break;
                    }
                    write_reg(rd, res, log_file);
                }
                break;
            case 0x03: // Loads
                {
                    int addr = read_reg(rs1) + imm_i;
                    int val = 0;
                    switch (funct3) {
                        case 0: val = (int)(int8_t)memory_rd_b(mem, addr); break;
                        case 1: val = (int)(int16_t)memory_rd_h(mem, addr); break;
                        case 2: val = memory_rd_w(mem, addr); break;
                        case 4: val = memory_rd_b(mem, addr); break;
                        case 5: val = memory_rd_h(mem, addr); break;
                    }
                    write_reg(rd, val, log_file);
                }
                break;
            case 0x23: // Stores
                {
                    int addr = read_reg(rs1) + imm_s;
                    int val = read_reg(rs2);
                    switch (funct3) {
                        case 0: memory_wr_b(mem, addr, val); break;
                        case 1: memory_wr_h(mem, addr, val); break;
                        case 2: memory_wr_w(mem, addr, val); break;
                    }
                    if (log_file) fprintf(log_file, " Mem[%x] <- %x", addr, val);
                }
                break;
            case 0x63: // Branches
                {
                    is_branch = 1;
                    int val1 = read_reg(rs1);
                    int val2 = read_reg(rs2);
                    int cond = 0;
                    switch (funct3) {
                        case 0: cond = (val1 == val2); break;
                        case 1: cond = (val1 != val2); break;
                        case 4: cond = (val1 < val2); break;
                        case 5: cond = (val1 >= val2); break;
                        case 6: cond = ((unsigned int)val1 < (unsigned int)val2); break;
                        case 7: cond = ((unsigned int)val1 >= (unsigned int)val2); break;
                    }
                    update_branch_stats(pc, cond, imm_b);
                    if (cond) {
                        next_pc = pc + imm_b;
                        taken_branch = 1;
                    }
                }
                break;
            case 0x6F: // JAL
                write_reg(rd, pc + 4, log_file);
                next_pc = pc + imm_j;
                break;
            case 0x67: // JALR
                {
                    int target = (read_reg(rs1) + imm_i) & ~1;
                    write_reg(rd, pc + 4, log_file);
                    next_pc = target;
                }
                break;
            case 0x37: // LUI
                write_reg(rd, imm_u, log_file);
                break;
            case 0x17: // AUIPC
                write_reg(rd, pc + imm_u, log_file);
                break;
            case 0x73: // ECALL
                {
                    int a7 = read_reg(17);
                    int a0 = read_reg(10);
                    int a1 = read_reg(11);
                    int a2 = read_reg(12);
                    
                    switch (a7) {
                        case 1: // getchar
                            {
                                int c = getchar();
                                write_reg(10, c, log_file);
                            }
                            break;
                        case 2: // putchar
                            putchar((char)a0);
                            break;
                        case 3: // exit
                        case 93: // exit
                            running = 0;
                            break;
                        case 4: // read_int_buffer(file, buffer, max_size)
                            {
                                if (a0 >= 0 && a0 < 16 && open_files[a0]) {
                                    int count = 0;
                                    int val;
                                    // Læs heltal fra filen indtil max eller EOF
                                    for (int k=0; k<a2; k++) {
                                        if (fscanf(open_files[a0], "%d", &val) != 1) break;
                                        memory_wr_w(mem, a1 + k*4, val);
                                        count++;
                                    }
                                    write_reg(10, count, log_file); // Returværdi: antal læst
                                } else {
                                    write_reg(10, -1, log_file); // Fejl
                                }
                            }
                            break;
                        case 5: // write_int_buffer(file, buffer, size)
                            {
                                if (a0 >= 0 && a0 < 16 && open_files[a0]) {
                                    int count = 0;
                                    for (int k=0; k<a2; k++) {
                                        int val = memory_rd_w(mem, a1 + k*4);
                                        fprintf(open_files[a0], "%d ", val); // Radix forventer mellemrum
                                        count++;
                                    }
                                    //
                                    write_reg(10, count, log_file);
                                } else {
                                    write_reg(10, -1, log_file);
                                }
                            }
                            break;
                        case 6: // open_file / close_file
                            
                            if (a0 > 0x1000) { 
                                // OPEN
                                char path[256];
                                char mode[16];
                                mem_read_str(mem, a0, path, 256);
                                mem_read_str(mem, a1, mode, 16);
                                
                                int slot = -1;
                                for(int k=3; k<16; k++) { // Find ledig plads (skip 0-2)
                                    if (open_files[k] == NULL) { slot = k; break; }
                                }
                                if (slot != -1) {
                                    open_files[slot] = fopen(path, mode);
                                    if (open_files[slot]) write_reg(10, slot, log_file);
                                    else write_reg(10, -1, log_file);
                                } else {
                                    write_reg(10, -1, log_file);
                                }
                            } else {
                                // CLOSE
                                if (a0 >= 3 && a0 < 16 && open_files[a0]) {
                                    fclose(open_files[a0]);
                                    open_files[a0] = NULL;
                                    write_reg(10, 0, log_file);
                                } else {
                                    write_reg(10, -1, log_file);
                                }
                            }
                            break;
                        default:
                            printf("\nUnknown syscall: %d\n", a7);
                            running = 0;
                            break;
                    }
                }
                break;
            
            default:
                break;
        }

        if (log_file) {
            if (is_branch && taken_branch) fprintf(log_file, " {T}");
            fprintf(log_file, "\n");
        }

        pc = next_pc;
        stats.insns++;
    }

    // Close any remaining open files
    for(int k=3; k<16; k++) {
        if(open_files[k]) fclose(open_files[k]);
    }

    printf("\n--- Branch Predictor Results ---\n");
    printf("Total Branches: %lld\n", stats_nt.predictions);
    
    printf("\n[NT] Always Not Taken:\n");
    printf("  Mispredictions: %lld\n", stats_nt.mispredictions);
    
    printf("\n[BTFNT] Backwards Taken, Forward Not Taken:\n");
    printf("  Mispredictions: %lld\n", stats_btfnt.mispredictions);

    printf("\n[Bimodal] Predictor:\n");
    for (int i = 0; i < NUM_SIZES; i++) {
        printf("  Size %5d: Mispredictions: %lld\n", BP_SIZES[i], stats_bimodal[i].mispredictions);
    }

    printf("\n[gShare] Predictor:\n");
    for (int i = 0; i < NUM_SIZES; i++) {
        printf("  Size %5d: Mispredictions: %lld\n", BP_SIZES[i], stats_gshare[i].mispredictions);
    }
    printf("--------------------------------\n");

    return stats;
}