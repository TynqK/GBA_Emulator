#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


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


InstructionType instruction_type(uint32_t instruction) {
    if (((instruction >> 25) & 0x7) == 0b001) {
        return TYPE_DATA_PROCESSING_AND_FSR_TRANSFER;
    }

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

    return TYPE_UNKNOWN;
}

int main() {

    uint8_t *rom = getFile();
    uint32_t instruction = read32(rom, 0);
    printf("%X\n", instruction);

    // Other
    CPU cpu;

    return 0;
}
