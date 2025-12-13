#include "disassemble.h"
#include "extractor.h"
#include <stdio.h>
#include <string.h>

// Array to map register numbers
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
    int32_t imm = IMM_I(instruction);
    uint32_t shamt = IMM_I(instruction) & 0x1F;
    int32_t simm = IMM_S(instruction);
    int32_t bimm = IMM_B(instruction);
    uint32_t target = addr + bimm;  


    switch (opcode) {
        case 0x33: // R-Type
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
            break;
            case 0x13:{ // I-Type
                switch (funct3) {
                case 0x0: // ADDI
                    sprintf(result, "addi %s, %s, %d", reg_names[rd], reg_names[rs1], imm);
                    break;
                
                case 0x1: // SLLI
                    sprintf(result, "slli %s, %s, %d", reg_names[rd], reg_names[rs1], shamt);
                    break;
                
                case 0x2: // SLTI
                    sprintf(result, "slti %s, %s, %d", reg_names[rd], reg_names[rs1], imm);
                    break;
                
                case 0x3: // SLTIU 
                    sprintf(result, "sltiu %s, %s, %d", reg_names[rd], reg_names[rs1], imm);
                    break;
                
                case 0x4: // XORI
                    sprintf(result, "xori %s, %s, %d", reg_names[rd], reg_names[rs1], imm);
                    break;
                
                case 0x5:
                    if (funct7 == 0x00) { // SRLI
                        sprintf(result, "srli %s, %s, %d", reg_names[rd], reg_names[rs1], shamt);
                    } else if (funct7 == 0x20) { // SRAI
                        sprintf(result, "srai %s, %s, %d", reg_names[rd], reg_names[rs1], shamt);
                    }
                    break;
                
                case 0x6: // ORI
                    sprintf(result, "ori %s, %s, %d", reg_names[rd], reg_names[rs1], imm);
                    break;
                
                case 0x7: // ANDI
                    sprintf(result, "andi %s, %s, %d", reg_names[rd], reg_names[rs1], imm);
                    break;
            }
            break;  
            }
            case 0x03: // I-Type
                switch (funct3) {
                case 0x0: sprintf(result, "lb %s, %d(%s)", reg_names[rd], imm, reg_names[rs1]); break;
                case 0x1: sprintf(result, "lh %s, %d(%s)", reg_names[rd], imm, reg_names[rs1]); break;
                case 0x2: sprintf(result, "lw %s, %d(%s)", reg_names[rd], imm, reg_names[rs1]); break;
                case 0x4: sprintf(result, "lbu %s, %d(%s)", reg_names[rd], imm, reg_names[rs1]); break;
                case 0x5: sprintf(result, "lhu %s, %d(%s)", reg_names[rd], imm, reg_names[rs1]); break;
            }
            break;
            case 0x23: // S-Type
            {
                switch (funct3) {
                case 0x0: sprintf(result, "sb %s, %d(%s)", reg_names[rs2], simm, reg_names[rs1]); break;
                case 0x1: sprintf(result, "sh %s, %d(%s)", reg_names[rs2], simm, reg_names[rs1]); break;
                case 0x2: sprintf(result, "sw %s, %d(%s)", reg_names[rs2], simm, reg_names[rs1]); break;
            }
            break;
            }
            case 0x63: // B-Type
            {
                switch (funct3) {
                case 0x0: sprintf(result, "beq %s, %s, %x", reg_names[rs1], reg_names[rs2], target); break;
                case 0x1: sprintf(result, "bne %s, %s, %x", reg_names[rs1], reg_names[rs2], target); break;
                case 0x4: sprintf(result, "blt %s, %s, %x", reg_names[rs1], reg_names[rs2], target); break;
                case 0x5: sprintf(result, "bge %s, %s, %x", reg_names[rs1], reg_names[rs2], target); break;
                case 0x6: sprintf(result, "bltu %s, %s, %x", reg_names[rs1], reg_names[rs2], target); break;
                case 0x7: sprintf(result, "bgeu %s, %s, %x", reg_names[rs1], reg_names[rs2], target); break;
            }
            break;
            }
            case 0x37: // U-Type
            {
                uint32_t uimm = IMM_U(instruction) >> 12;
                sprintf(result, "lui %s, 0x%x", reg_names[rd], uimm);
                break;}
            case 0x17: // U-Type
            {
                uint32_t uimm = IMM_U(instruction) >> 12;
                sprintf(result, "auipc %s, 0x%x", reg_names[rd], uimm);
                break;}
            case 0x6F: //J-Type 
            {
                int32_t jimm = IMM_J(instruction);
                uint32_t target = addr + jimm;
                sprintf(result, "jal %s, 0x%x", reg_names[rd], target);
                break; }
            case 0x67: // I-Type    
                sprintf(result, "jalr %s, %d(%s)", reg_names[rd], imm, reg_names[rs1]);
                break;
            case 0x73: // System
                if (funct3 == 0x0 && imm == 0) { // Standard ecall check
                    sprintf(result, "ecall");
                } else {
                    sprintf(result, "system_unk"); // Fallback
                }
                break;
        }
}