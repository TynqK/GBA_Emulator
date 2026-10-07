#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

typedef struct {
    uint32_t r[16];
    uint32_t cpsr;
} CPU;

typedef enum {
    TYPE_DATA_PROCESSING_AND_FSR_TRANSFER,
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
    //if (((instruction >> 25) & 0x7) == 0b001) {
    //   return TYPE_DATA_PROCESSING_AND_FSR_TRANSFER;
    //}.cps

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

    if (((instruction >> 25) & 0x7) == 0b011) {
        if (instruction & (1u << 4)) {
            return TYPE_UNDEFINED;
        } else {
            return TYPE_SINGLE_DATA_TRANSFER;
        }
    }

    return TYPE_UNKNOWN;
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
            cpu->r[14] = cpu->r[15] + 4;
        }

        cpu->r[15] = target;
    }
}


void cpu_step(CPU *cpu, uint8_t *rom)
{
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

    CPU cpu;
    cpu.cpsr = 0;
    cpu.r[15] = 0;

    uint8_t *rom = getFile();

    for (int i = 0; i < 2; i++) {
        cpu_step(&cpu, rom);
    }

    return 0;
}
