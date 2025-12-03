#include "disassemble.h"
#include "extractor.h"
#include <stdio.h>
#include <string.h>

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
        case 0x33: // R-Type
            // Distinguish based on funct3 first
            switch (funct3) {
                case 0x0: 
                    if (funct7 == 0x00) { // ADD
                        sprintf(result, "add %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    } else if (funct7 == 0x20) { // SUB
                        sprintf(result, "sub %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    } else if (funct7 == 0x01) { // MUL
                        sprintf(result, "mul %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    }
                    break;
                case 0x1:
                    if (funct7 == 0x00) { // SLL
                        sprintf(result, "sll %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    } else if (funct7 == 0x01) { // MULH
                        sprintf(result, "mulh %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    }
                    break;
                case 0x2:
                    if (funct7 == 0x00) { // SLT
                        sprintf(result, "slt %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    } else if (funct7 == 0x01) { // MULHSU
                        sprintf(result, "mulhsu %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    }
                    break;
                case 0x3:
                    if (funct7 == 0x00) { // SLTU
                        sprintf(result, "sltu %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    } else if (funct7 == 0x01) { // MULHU
                        sprintf(result, "mulhu %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    }
                    break;
                case 0x4:
                    if (funct7 == 0x00) { // XOR
                        sprintf(result, "xor %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    } else if (funct7 == 0x01) { // DIV
                        sprintf(result, "div %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    }
                    break;
                case 0x5: 
                    if (funct7 == 0x00) { // SRL
                        sprintf(result, "srl %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    } else if (funct7 == 0x20) { // SRA
                        sprintf(result, "sra %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    } else if (funct7 == 0x01) { // DIVU
                        sprintf(result, "divu %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    }
                    break;
                case 0x6:
                    if (funct7 == 0x00) { // OR
                        sprintf(result, "or %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    } else if (funct7 == 0x01) { // REM
                        sprintf(result, "rem %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    }
                    break;
                case 0x7:
                    if (funct7 == 0x00) { // AND
                        sprintf(result, "and %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    } else if (funct7 == 0x01) { // REMU
                        sprintf(result, "remu %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                    }
                    break;
                }
        case 0x13: // I-Type
            switch (funct3){
                case 0x0:{ // ADDI
                    sprintf(result, "and %s, %s, %s", reg_names[rd], reg_names[rs1], reg_names[rs2]);
                }
            }
}