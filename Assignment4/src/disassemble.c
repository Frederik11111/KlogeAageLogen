#include "disassemble.h"
#include "extractor.h"
#include <stdio.h>
#include <string.h>

// Macros for extraction
#define OPCODE(inst)   (inst & 0x7F) // Mask anyting but the last 7 bits
#define RD(inst)       ((inst >> 7) & 0x1F) // Shift the first 7, and then mask intill last 5 bits
#define FUNCT3(inst)   ((inst >> 12) & 0x7)
#define RS1(inst)      ((inst >> 15) & 0x1F)
#define RS2(inst)      ((inst >> 20) & 0x1F)
#define FUNCT7(inst)   ((inst >> 25) & 0x7F)

// Array to map register numbers (0-31) to names ("zero", "ra", "sp", etc.)
const char* reg_names[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

void disassemble(uint32_t addr, uint32_t instruction, char* result, size_t buf_size, struct symbols* symbols) {
    uint32_t opcode = OPCODE(instruction);
    uint32_t rd = RD(instruction);
    uint32_t rs1 = RS1(instruction);
    uint32_t rs2 = RS2(instruction);
    uint32_t funct3 = FUNCT3(instruction);
    uint32_t funct7 = FUNCT7(instruction);

    switch (opcode) {
        case 0x33: // R-Type (ADD, SUB, MUL, etc.)
            // TODO: Nested switch on funct3 and funct7 to find exact instruction
            // Example: if funct3 == 0x0 && funct7 == 0x00 -> "add"
            break;

        case 0x13: // I-Type Arithmetic (ADDI, etc.)
            break;
        
        case 0x03: // Loads (LB, LW, etc.)
            break;

        case 0x23: // Stores (SB, SW, etc.)
            break;

        case 0x63: // Branches (BEQ, BNE, etc.)
            break;

        case 0x37: // LUI
            break;
            
        case 0x17: // AUIPC
            break;
            
        case 0x6F: // JAL
            break;
            
        case 0x67: // JALR
            break;
            
        case 0x73: // ECALL
            snprintf(result, buf_size, "ecall");
            break;

        default:
            snprintf(result, buf_size, "unknown");
            break;
    }
}