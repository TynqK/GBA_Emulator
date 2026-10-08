#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <assert.h>


typedef struct {
    // Main
    uint32_t r[16];
    uint32_t cpsr;

    // FIQ bank
    uint32_t r8_fiq;
    uint32_t r9_fiq;
    uint32_t r10_fiq;
    uint32_t r11_fiq;
    uint32_t r12_fiq;
    uint32_t r13_fiq;
    uint32_t r14_fiq;

    // IRQ bank
    uint32_t r13_irq;
    uint32_t r14_irq;

    // SVC bank
    uint32_t r13_svc;
    uint32_t r14_svc;

    // Abort bank
    uint32_t r13_abt;
    uint32_t r14_abt;

    // Undefined bank
    uint32_t r13_und;
    uint32_t r14_und;

    // SPSR
    uint32_t spsr_fiq;
    uint32_t spsr_irq;
    uint32_t spsr_svc;
    uint32_t spsr_abt;
    uint32_t spsr_und;
} CPU;

typedef enum {
    MODE_USER       = 0b10000,
    MODE_FIQ        = 0b10001,
    MODE_IRQ        = 0b10010,
    MODE_SVC        = 0b10011,
    MODE_ABORT      = 0b10111,
    MODE_UNDEFINED  = 0b11011,
    MODE_SYSTEM     = 0b11111,
} CPUMode;

typedef enum {
    TYPE_DATA_PROCESSING_AND_PSR_TRANSFER,
    TYPE_MULTIPLY,
    TYPE_MULTIPLY_LONG,
    TYPE_SINGLE_DATA_SWAP,
    TYPE_BRANCH_AND_EXCHANGE,
    TYPE_HALFWORD_DATA_TRANSFER_REGISTER_OFFSET,
    TYPE_HALFWORD_DATA_TRANSFER_IMMEDIATE_OFFSET,
    TYPE_SINGLE_DATA_TRANSFER,
    TYPE_UNDEFINED,
    TYPE_BLOCK_DATA_TRANSFER,
    TYPE_BRANCH,
    TYPE_COPROCESSOR_DATA_TRANSFER,
    TYPE_COPROCESSOR_DATA_OPERATION,
    TYPE_COPROCESSOR_REGISTER_TRANSFER,
    TYPE_SOFTWARE_INTERRUPT,
    TYPE_UNKNOWN
} InstructionType;


uint8_t *getFile() {
    // .bin/.gba File Reading
    FILE *file = fopen("gba_bios.bin", "rb");

    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    uint8_t *rom = malloc(size);

    fread(rom, 1, size, file);
    fclose(file);

    return rom;
}


uint32_t read32(uint8_t *rom, uint32_t address) {
    return (uint32_t)rom[address]
    | ((uint32_t)rom[address + 1] << 8)
    | ((uint32_t)rom[address + 2] << 16)
    | ((uint32_t)rom[address + 3] << 24);
}

uint8_t cond_check(uint32_t instruction, CPU *cpu) {
    uint32_t condBits = instruction >> 28;

    uint32_t N = cpu->cpsr >> 31;
    uint32_t Z = (cpu->cpsr << 1) >> 31;
    uint32_t C = (cpu->cpsr << 2) >> 31;
    uint32_t V = (cpu->cpsr << 3) >> 31;

    switch (condBits) {
        case 0: return Z == 1;              // EQ
        case 1: return Z == 0;              // NE
        case 2: return C == 1;              // CS
        case 3: return C == 0;              // CC
        case 4: return N == 1;              // MI
        case 5: return N == 0;              // PL
        case 6: return V == 1;              // VS
        case 7: return V == 0;              // VC
        case 8: return C == 1 && Z == 0;    // HI
        case 9: return C == 0 || Z == 1;    // LS
        case 10: return N == V;             // GE
        case 11: return N != V;             // LT
        case 12: return Z == 0 && N == V;   // GT
        case 13: return Z == 1 || N != V;   // LE
        case 14: return 1;                  // AL
        default:  return 0;
    }
}


InstructionType instruction_type(uint32_t instruction) {
    if (((instruction >> 25) & 0x7) == 0b101) {
        return TYPE_BRANCH;
    }

    if (((instruction >> 24) & 0xF) == 0b1111) {
        return TYPE_SOFTWARE_INTERRUPT;
    }

    if (((instruction >> 24) & 0xF) == 0b1110) {
        if (instruction & (1u << 4)) {
            return TYPE_COPROCESSOR_REGISTER_TRANSFER;
        } else {
            return TYPE_COPROCESSOR_DATA_OPERATION;
        }
    }

    if (((instruction >> 25) & 0x7) == 0b110) {
        return TYPE_COPROCESSOR_DATA_TRANSFER;
    }

    if (((instruction >> 25) & 0x7) == 0b100) {
        return TYPE_BLOCK_DATA_TRANSFER;
    }

    if (((instruction >> 26) & 0x3) == 0b01) {
        return TYPE_SINGLE_DATA_TRANSFER;
    }

    // (bits 27-25) 000 instructions and TYPE_DATA_PROCESSING_AND_FSR_TRANSFER (000/001)
    if (((instruction >> 26) & 0x3) == 0b00) {
        if ((instruction >> 25) & 1) {
            return TYPE_DATA_PROCESSING_AND_PSR_TRANSFER;
        }

        if ((instruction & 0x0FFFFFF0) == 0x012FFF10) {
            return TYPE_BRANCH_AND_EXCHANGE;
        }

        if (((instruction >> 22) & 1) == 0 &&
            ((instruction >> 7) & 0x1F) == 0b00001 &&
            ((instruction >> 4) & 1) == 1 &&
            ((instruction >> 5) & 0x3) != 0) {

            return TYPE_HALFWORD_DATA_TRANSFER_REGISTER_OFFSET;
        }

        if (((instruction >> 22) & 1) == 1 &&
            ((instruction >> 7) & 1) == 1 &&
            ((instruction >> 4) & 1) == 1 &&
            ((instruction >> 5) & 0x3) != 0) {

            return TYPE_HALFWORD_DATA_TRANSFER_IMMEDIATE_OFFSET;
        }
        // (bits 7-4) 1001 instructions
        if (((instruction >> 4) & 0xF) == 0b1001) {

            if (((instruction >> 23) & 0x3) == 0b10 &&
                ((instruction >> 20) & 0x3) == 0b00 &&
                ((instruction >> 4)  & 0xFF) == 0b00001001) {

                return TYPE_SINGLE_DATA_SWAP;
            }

            if (((instruction >> 22) & 0x7) == 0b000) {
                return TYPE_MULTIPLY;
            }

            if (((instruction >> 23) & 0x3) == 0b01) {
                return TYPE_MULTIPLY_LONG;
            }

        }

        return TYPE_DATA_PROCESSING_AND_PSR_TRANSFER;
    }

    return TYPE_UNKNOWN;
}


uint32_t cpu_read_reg(CPU *cpu, uint32_t reg) {
    CPUMode mode = cpu->cpsr & 0x1F;

    if (reg <= 7 || reg == 15) {
        return cpu->r[reg];
    }

    if (reg >= 8 && reg <= 12) {
        if (mode == MODE_FIQ) {
            switch (reg) {
                case 8:  return cpu->r8_fiq;
                case 9:  return cpu->r9_fiq;
                case 10: return cpu->r10_fiq;
                case 11: return cpu->r11_fiq;
                case 12: return cpu->r12_fiq;
            }
        }

        return cpu->r[reg];
    }

    if (reg == 13 || reg == 14) {
        switch (mode) {
            case MODE_FIQ:
                return reg == 13 ? cpu->r13_fiq : cpu->r14_fiq;

            case MODE_IRQ:
                return reg == 13 ? cpu->r13_irq : cpu->r14_irq;

            case MODE_SVC:
                return reg == 13 ? cpu->r13_svc : cpu->r14_svc;

            case MODE_ABORT:
                return reg == 13 ? cpu->r13_abt : cpu->r14_abt;

            case MODE_UNDEFINED:
                return reg == 13 ? cpu->r13_und : cpu->r14_und;

            default:
                return cpu->r[reg];
        }
    }

    return cpu->r[reg];
}


void cpu_write_reg(CPU *cpu, uint32_t reg, uint32_t value) {
    CPUMode mode = cpu->cpsr & 0x1F;

    if (reg <= 7 || reg == 15) {
        cpu->r[reg] = value;
        return;
    }

    if (reg >= 8 && reg <= 12) {
        if (mode == MODE_FIQ) {
            switch (reg) {
                case 8:  cpu->r8_fiq = value;  return;
                case 9:  cpu->r9_fiq = value;  return;
                case 10: cpu->r10_fiq = value; return;
                case 11: cpu->r11_fiq = value; return;
                case 12: cpu->r12_fiq = value; return;
            }
        }

        cpu->r[reg] = value;
        return;
    }

    if (reg == 13 || reg == 14) {
        switch (mode) {
            case MODE_FIQ:
                if (reg == 13) {
                    cpu->r13_fiq = value;
                } else {
                    cpu->r14_fiq = value;
                }
                return;

            case MODE_IRQ:
                if (reg == 13) {
                    cpu->r13_irq = value;
                } else {
                    cpu->r14_irq = value;
                }
                return;

            case MODE_SVC:
                if (reg == 13) {
                    cpu->r13_svc = value;
                } else {
                    cpu->r14_svc = value;
                }
                return;

            case MODE_ABORT:
                if (reg == 13) {
                    cpu->r13_abt = value;
                } else {
                    cpu->r14_abt = value;
                }
                return;

            case MODE_UNDEFINED:
                if (reg == 13) {
                    cpu->r13_und = value;
                } else {
                    cpu->r14_und = value;
                }
                return;

            default:
                cpu->r[reg] = value;
                return;
        }
    }
}


uint32_t *cpu_spsr(CPU *cpu)
{
    CPUMode mode = cpu->cpsr & 0x1F;

    switch (mode) {
        case MODE_FIQ:
            return &cpu->spsr_fiq;

        case MODE_IRQ:
            return &cpu->spsr_irq;

        case MODE_SVC:
            return &cpu->spsr_svc;

        case MODE_ABORT:
            return &cpu->spsr_abt;

        case MODE_UNDEFINED:
            return &cpu->spsr_und;

        default:
            return NULL;
    }
}


uint32_t cpu_mode(CPU *cpu) {
    return cpu->cpsr & 0x1F;
}


void Execute(CPU *cpu, uint32_t instruction, InstructionType instType) {
    if (instType == TYPE_BRANCH) {
        int32_t offset = instruction & 0x00FFFFFF;

        if (offset & 0x00800000) {
            offset |= 0xFF000000;
        }

        offset <<= 2;

        uint32_t target = cpu->r[15] + 8 + offset;

        if (((instruction >> 24) & 1) == 1) {
            // BL
            cpu_write_reg(cpu, 14, cpu->r[15] + 4);
        }

        cpu->r[15] = target;

        return;
    }

    if (instType == TYPE_DATA_PROCESSING_AND_PSR_TRANSFER) {
        /* MRS: PSR -> register */
        if ((instruction & 0x0FBF0FFF) == 0x010F0000) {
            uint32_t P  = (instruction >> 22) & 1;
            uint32_t Rd = (instruction >> 12) & 0xF;

            if (P == 0) {
                cpu_write_reg(cpu, Rd, cpu->cpsr);
            } else {
                uint32_t *spsr = cpu_spsr(cpu);

                if (spsr != NULL) {
                    cpu_write_reg(cpu, Rd, *spsr);
                }
            }

            return;
        }

        /* MSR: register -> PSR */
        if ((instruction & 0x0FB0FFF0) == 0x0120F000) {
            uint32_t P  = (instruction >> 22) & 1;
            uint32_t Rm = instruction & 0xF;
            uint32_t value = cpu_read_reg(cpu, Rm);
            uint32_t field_mask = (instruction >> 16) & 0xF;

            if (field_mask == 0b1000) {
                // PSR_TRANSFER MSR reg -> PSR flags only
                if (P == 0) {
                    cpu->cpsr = (cpu->cpsr & 0x00FFFFFF) |
                    (value & 0xFF000000);
                } else {
                    uint32_t *spsr = cpu_spsr(cpu);

                    if (spsr != NULL) {
                        *spsr = (*spsr & 0x00FFFFFF) |
                        (value & 0xFF000000);
                    }
                }
            } else {
                // PSR_TRANSFER MSR reg -> PSR
                if (P == 0) {
                    uint32_t mask = 0;

                    if (field_mask & 0b1000)
                        mask |= 0xFF000000;

                    if (field_mask & 0b0100)
                        mask |= 0x00FF0000;

                    if (field_mask & 0b0010)
                        mask |= 0x0000FF00;

                    if (field_mask & 0b0001) {
                        if (cpu_mode(cpu) != MODE_USER)
                            mask |= 0x000000FF;
                    }

                    cpu->cpsr = (cpu->cpsr & ~mask) | (value & mask);
                } else {
                    uint32_t *spsr = cpu_spsr(cpu);

                    if (spsr != NULL) {
                        uint32_t mask = 0;

                        if (field_mask & 0b1000)
                            mask |= 0xFF000000;

                        if (field_mask & 0b0100)
                            mask |= 0x00FF0000;

                        if (field_mask & 0b0010)
                            mask |= 0x0000FF00;

                        if (field_mask & 0b0001)
                            mask |= 0x000000FF;

                        *spsr = (*spsr & ~mask) | (value & mask);
                    }
                }
            }

            return;
        }

        /* MSR: immediate -> PSR */
        if ((instruction & 0x0FB0F000) == 0x0320F000) {
            uint32_t field_mask = (instruction >> 16) & 0xF;
            uint32_t P = (instruction >> 22) & 1;

            uint32_t rotate_imm = (instruction >> 8) & 0xF;
            uint32_t imm8 = instruction & 0xFF;

            uint32_t value;

            if (rotate_imm == 0) {
                value = imm8;
            } else {
                uint32_t rotate = rotate_imm * 2;

                value = (imm8 >> rotate) |
                (imm8 << (32 - rotate));
            }

            if (field_mask == 0b1000) {
                if (P == 0) {
                    cpu->cpsr = (cpu->cpsr & 0x00FFFFFF) |
                    (value & 0xFF000000);
                } else {
                    uint32_t *spsr = cpu_spsr(cpu);

                    if (spsr != NULL) {
                        *spsr = (*spsr & 0x00FFFFFF) |
                        (value & 0xFF000000);
                    }
                }

            } else {
                uint32_t mask = 0;

                if (field_mask & 0b1000)
                    mask |= 0xFF000000;

                if (field_mask & 0b0100)
                    mask |= 0x00FF0000;

                if (field_mask & 0b0010)
                    mask |= 0x0000FF00;

                if (field_mask & 0b0001) {
                    if (cpu_mode(cpu) != MODE_USER)
                        mask |= 0x000000FF;
                }

                if (P == 0) {
                    cpu->cpsr = (cpu->cpsr & ~mask) | (value & mask);
                } else {
                    uint32_t *spsr = cpu_spsr(cpu);

                    if (spsr != NULL) {
                        *spsr = (*spsr & ~mask) | (value & mask);
                    }
                }
            }

            return;
        }

    }
}


void cpu_step(CPU *cpu, uint8_t *rom) {
    uint32_t instruction = read32(rom, cpu->r[15]);
    printf("%X\n", instruction);

    if (!cond_check(instruction, cpu)) {
        cpu->r[15] += 4;
        return;
    }

    InstructionType type = instruction_type(instruction);
    uint32_t old_pc = cpu->r[15];

    Execute(cpu, instruction, type);

    if (cpu->r[15] == old_pc) {
        cpu->r[15] += 4;
    }
}


int main() {

    CPU cpu = {0};

    cpu.cpsr = MODE_SVC | (1u << 7) | (1u << 6);

    uint8_t *rom = getFile();

    for (int i = 0; i < 2; i++) {
        cpu_step(&cpu, rom);
    }

    return 0;
}
