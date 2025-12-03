#include "simulate.h"
#include "extractor.h"
#include "disassemble.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Macros handle sign extension for immediate values
// If the sign bit is set, extend the sign to the full 32 bits
#define SIGN_EXTEND_12(x) ((((int32_t)(x)) << 20) >> 20)
#define SIGN_EXTEND_20(x) ((((int32_t)(x)) << 12) >> 12)

struct Stat simulate(struct memory *mem, int start_addr, FILE *log_file, struct symbols* symbols) {
    struct Stat stats = {0};
    
    // RISC-V has 32 registers (x0-x31). x0 is hardwired to 0.
    int32_t regs[32] = {0}; 
    uint32_t pc = start_addr;
    int32_t next_pc;
    int running = 1;

    // registers x0 starts as 0
    regs[0] = 0;

    while (running) {
        // Fetch from memory
        uint32_t inst = memory_rd_w(mem, pc);

        if (log_file) {
            char buf[128];
            // use disassemble
            disassemble(pc, inst, buf, sizeof(buf), symbols);
            fprintf(log_file, "%8x : %08x       %s\n", pc, inst, buf);
        }

        // Decode
        // use macros from extractor.h
        uint32_t opcode = GET_OPCODE(inst);
        uint32_t rd     = GET_RD(inst);
        uint32_t rs1    = GET_RS1(inst);
        uint32_t rs2    = GET_RS2(inst);
        uint32_t funct3 = GET_FUNCT3(inst);
        uint32_t funct7 = GET_FUNCT7(inst);

        // unpack
        int32_t imm_i = SIGN_EXTEND_12(inst >> 20);
        int32_t imm_s = SIGN_EXTEND_12(((inst >> 25) << 5) | ((inst >> 7) & 0x1F));
        
        // B-type
        int32_t imm_b = SIGN_EXTEND_12(
            ((inst & 0x80000000) >> 19) | 
            ((inst & 0x80) << 4) | 
            ((inst >> 20) & 0x7E0) | 
            ((inst >> 7) & 0x1E)
        );
        
        // U-type
        int32_t imm_u = inst & 0xFFFFF000;
        
        // J-type
        int32_t imm_j = SIGN_EXTEND_20(
            ((inst & 0x80000000) >> 11) | 
            (inst & 0xFF000) | 
            ((inst >> 9) & 0x800) | 
            ((inst >> 20) & 0x7FE0)
        );

        // Next instruction by default
        next_pc = pc + 4;

        // Execute
        switch (opcode) {
            case 0x37: // LUI (Load Upper Immediate)
                if (rd != 0) regs[rd] = imm_u;
                break;
            
            case 0x17: // AUIPC (Add Upper Immediate to PC)
                if (rd != 0) regs[rd] = pc + imm_u;
                break;
            
            case 0x6F: // JAL (Jump and Link)
                if (rd != 0) regs[rd] = pc + 4; // save return address
                next_pc = pc + imm_j;           // Jump
                break;
            
            case 0x67: // JALR (Jump and Link Register)
                {
                    int32_t target = (regs[rs1] + imm_i) & ~1;
                    if (rd != 0) regs[rd] = pc + 4;
                    next_pc = target;
                }
                break;
            
            case 0x63: // Branch
                {
                    int take = 0;
                    switch (funct3) {
                        case 0x0: take = (regs[rs1] == regs[rs2]); break; // BEQ
                        case 0x1: take = (regs[rs1] != regs[rs2]); break; // BNE
                        case 0x4: take = (regs[rs1] < regs[rs2]); break;  // BLT
                        case 0x5: take = (regs[rs1] >= regs[rs2]); break; // BGE
                        case 0x6: take = ((uint32_t)regs[rs1] < (uint32_t)regs[rs2]); break; // BLTU
                        case 0x7: take = ((uint32_t)regs[rs1] >= (uint32_t)regs[rs2]); break; // BGEU
                    }
                    if (take) next_pc = pc + imm_b;
                }
                break;
            
            case 0x03: // LOAD (I-Type)
                {
                    int32_t addr = regs[rs1] + imm_i;
                    int32_t val = 0;
                    switch (funct3) {
                        case 0x0: val = (int8_t)memory_rd_b(mem, addr); break;  // LB
                        case 0x1: val = (int16_t)memory_rd_h(mem, addr); break; // LH
                        case 0x2: val = memory_rd_w(mem, addr); break;          // LW
                        case 0x4: val = (uint8_t)memory_rd_b(mem, addr); break; // LBU
                        case 0x5: val = (uint16_t)memory_rd_h(mem, addr); break;// LHU
                    }
                    if (rd != 0) regs[rd] = val;
                }
                break;
            
            case 0x23: // STORE (S-Type)
                {
                    int32_t addr = regs[rs1] + imm_s;
                    switch (funct3) {
                        case 0x0: memory_wr_b(mem, addr, regs[rs2]); break; // SB
                        case 0x1: memory_wr_h(mem, addr, regs[rs2]); break; // SH
                        case 0x2: memory_wr_w(mem, addr, regs[rs2]); break; // SW
                    }
                }
                break;
            
            case 0x13: // ALUi (I-Type)
                {
                    int32_t val = 0;
                    switch (funct3) {
                        case 0x0: val = regs[rs1] + imm_i; break; // ADDI
                        case 0x2: val = (regs[rs1] < imm_i) ? 1 : 0; break; // SLTI
                        case 0x3: val = ((uint32_t)regs[rs1] < (uint32_t)imm_i) ? 1 : 0; break; // SLTIU
                        case 0x4: val = regs[rs1] ^ imm_i; break; // XORI
                        case 0x6: val = regs[rs1] | imm_i; break; // ORI
                        case 0x7: val = regs[rs1] & imm_i; break; // ANDI
                        case 0x1: val = regs[rs1] << (imm_i & 0x1F); break; // SLLI
                        case 0x5: // SRLI og SRAI deler funct3
                            if ((inst >> 30) & 1) 
                                val = regs[rs1] >> (imm_i & 0x1F); // SRAI (arithmetic shift)
                            else 
                                val = (uint32_t)regs[rs1] >> (imm_i & 0x1F); // SRLI (logical shift)
                            break;
                    }
                    if (rd != 0) regs[rd] = val;
                }
                break;
            
            case 0x33: // ALU (R-Type) - Regn med to registre
                {
                    int32_t val = 0;
                    if (funct7 == 0x00) {
                        // Standard operations
                        switch (funct3) {
                            case 0x0: val = regs[rs1] + regs[rs2]; break; // ADD
                            case 0x1: val = regs[rs1] << (regs[rs2] & 0x1F); break; // SLL
                            case 0x2: val = (regs[rs1] < regs[rs2]) ? 1 : 0; break; // SLT
                            case 0x3: val = ((uint32_t)regs[rs1] < (uint32_t)regs[rs2]) ? 1 : 0; break; // SLTU
                            case 0x4: val = regs[rs1] ^ regs[rs2]; break; // XOR
                            case 0x5: val = (uint32_t)regs[rs1] >> (regs[rs2] & 0x1F); break; // SRL
                            case 0x6: val = regs[rs1] | regs[rs2]; break; // OR
                            case 0x7: val = regs[rs1] & regs[rs2]; break; // AND
                        }
                    } else if (funct7 == 0x20) {
                        // Variants (SUB / SRA)
                        switch (funct3) {
                            case 0x0: val = regs[rs1] - regs[rs2]; break; // SUB
                            case 0x5: val = regs[rs1] >> (regs[rs2] & 0x1F); break; // SRA
                        }
                    } else if (funct7 == 0x01) {
                        // M Extension (Multiplikation og Division)
                        switch (funct3) {
                            case 0x0: val = regs[rs1] * regs[rs2]; break; // MUL
                            
                            case 0x4: // DIV
                                if (regs[rs2] == 0) val = -1; 
                                else val = regs[rs1] / regs[rs2]; 
                                break;
                            case 0x5: // DIVU
                                if (regs[rs2] == 0) val = -1;
                                else val = (uint32_t)regs[rs1] / (uint32_t)regs[rs2];
                                break;
                            case 0x6: // REM
                                if (regs[rs2] == 0) val = regs[rs1];
                                else val = regs[rs1] % regs[rs2];
                                break;
                            case 0x7: // REMU
                                if (regs[rs2] == 0) val = regs[rs1];
                                else val = (uint32_t)regs[rs1] % (uint32_t)regs[rs2];
                                break;
                        }
                    }
                    if (rd != 0) regs[rd] = val;
                }
                break;
            
            case 0x73: // Ecall
                {
                    int syscall_id = regs[17]; 
                    
                    switch (syscall_id) {
                        case 1: // getchar
                            regs[10] = getchar();
                            break;
                        case 2: // putchar
                            putchar((char)regs[10]);
                            break;
                        case 3: // terminate
                        case 93: // exit (standard linux)
                            running = 0;
                            break;
                        default:
                            break;
                    }
                }
                break;
                
            default:
                // Illegal instruction - stop simulation
                fprintf(stderr, "Unknown instruction at 0x%08x: 0x%08x\n", pc, inst);
                running = 0;
                break;
        }
        
        regs[0] = 0;
        
        // Update pc
        pc = next_pc;
        stats.insns++;
    }
    return stats;
}