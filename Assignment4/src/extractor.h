// Macros for extraction

#define OPCODE(inst)   (inst & 0x7F) // Mask anyting but the last 7 bits
#define RD(inst)       ((inst >> 7) & 0x1F) // Shift the first 7, and then mask intill last 5 bits
#define FUNCT3(inst)   ((inst >> 12) & 0x7)
#define RS1(inst)      ((inst >> 15) & 0x1F)
#define RS2(inst)      ((inst >> 20) & 0x1F)
#define FUNCT7(inst)   ((inst >> 25) & 0x7F)
#define IMM_I(inst)    ((int32_t)inst >> 20) //Immediat int
#define IMM_S(inst)   ( (int32_t)(inst & 0xFE000000) >> 20 ) | ( (inst >> 7) & 0x1F )
#define IMM_B(inst)   ( \
    ((int32_t)(inst & 0x80000000) >> 19) | \
    ((inst & 0x80) << 4) | \
    ((inst >> 20) & 0x7E0) | \
    ((inst >> 7) & 0x1E) )
#define IMM_U(inst)   (inst & 0xFFFFF000)
#define IMM_J(inst)   ( \
    ((int32_t)(inst & 0x80000000) >> 11) | \
    (inst & 0x000FF000) | \
    ((inst & 0x00100000) >> 9) | \
    ((inst & 0x7FE00000) >> 20) )