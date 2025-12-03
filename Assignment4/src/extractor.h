// Helper macros to extract fields from a 32-bit instruction
// See RISC-V Green Card for these bit positions

#define GET_OPCODE(inst)  ((inst) & 0x0000007F)
#define GET_RD(inst)      (((inst) >> 7)  & 0x0000001F)
#define GET_FUNCT3(inst)  (((inst) >> 12) & 0x00000007)
#define GET_RS1(inst)     (((inst) >> 15) & 0x0000001F)
#define GET_RS2(inst)     (((inst) >> 20) & 0x0000001F)
#define GET_FUNCT7(inst)  (((inst) >> 25) & 0x0000007F)