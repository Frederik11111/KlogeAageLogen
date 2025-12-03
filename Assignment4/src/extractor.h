// Macros for extraction

#define OPCODE(inst)   (inst & 0x7F) // Mask anyting but the last 7 bits
#define RD(inst)       ((inst >> 7) & 0x1F) // Shift the first 7, and then mask intill last 5 bits
#define FUNCT3(inst)   ((inst >> 12) & 0x7)
#define RS1(inst)      ((inst >> 15) & 0x1F)
#define RS2(inst)      ((inst >> 20) & 0x1F)
#define FUNCT7(inst)   ((inst >> 25) & 0x7F)