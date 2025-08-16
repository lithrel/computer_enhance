#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

typedef enum OpCode {
  MOVE_REGMEM_TOFROM_REG = 0b10001000,
  MOVE_IMM_TO_REGMEM     = 0b11000110,
  MOVE_IMM_TO_REG_8      = 0b10110000,
  MOVE_IMM_TO_REG_16     = 0b10111000,
  MOVE_MEM_TO_ACC        = 0b10100000,
  MOVE_ACC_TO_MEM        = 0b10100010,
  MOVE_REGMEM_TO_SEGREG  = 0b10001110,
  MOVE_SEGREG_TO_REGMEM  = 0b10001100,
} OpCode;

typedef enum Mode {
  MOD_NO_DISP = 0b00000000,
  MOD_DISP_8  = 0b01000000,
  MOD_DISP_16 = 0b10000000,
  MOD_REG     = 0b11000000,
} Mode;

unsigned char OP_MASK_DIR = 0b00000010;
unsigned char OP_MASK_WRD = 0b00000001;

unsigned char OP_MASK_MOD = 0b11000000;
unsigned char OP_MASK_REG = 0b00111000;
unsigned char OP_MASK_RM  = 0b00000111;

unsigned char OP_MASK_DST = 0b00000111;
unsigned char OP_MASK_SRC = 0b00111000;

char* registers[] = {
  "al", "cl", "dl", "bl",
  "ah", "ch", "dh", "bh",
  "ax", "cx", "dx", "bx",
  "sp", "bp", "si", "di",
};
char* calculations[] = {
  "bx + si", "bx + di", "bp + si", "bp + di",
  "si", "di", "bp", "bx",
};

typedef struct Operation {
  uint8_t bytes[6]; 
  OpCode opcode;
  Mode mode;
  bool direction;
  bool word; // wide
  char *dst;
  char *src;
} Operation;

char* parseRegBits(unsigned char regBits, unsigned char wordBit)
{
    if (wordBit == 0b00000001) {
      regBits = regBits | 0b00001000;
    }

    return registers[regBits];
}

char* parseRmBits(
  unsigned char rmBits,
  unsigned char wordBit,
  unsigned char modBits,
  FILE *f
) {
    if (modBits == MOD_REG) {
      if (wordBit == 0b00000001) {
        rmBits = rmBits | 0b00001000;
      }
      return registers[rmBits];
    } else if (modBits == MOD_NO_DISP) {
      return calculations[rmBits];
    } else if (modBits == MOD_DISP_8) {
      uint8_t additional;
      fread(&additional, 1, 1, f);

      char add[10];
      sprintf(add, "%d", additional);

      char *str = (char*)malloc(16 * sizeof(char));
      strcpy(str, calculations[rmBits]);
      if (additional == 0) {
        return str;
      }

      strcat(str, " + ");
      strcat(str, add);
      return str;
    } else if (modBits == MOD_DISP_16) {
      uint16_t additional;
      fread(&additional, 2, 1, f);

      char add[10];
      sprintf(add, "%d", additional);

      char *str = (char*)malloc(16 * sizeof(char));
      strcpy(str, calculations[rmBits]);
      if (additional == 0) {
        return str;
      }

      strcat(str, " + ");
      strcat(str, add);
      return str;
    }

    return "";
}

int main(int argc, char **argv)
{
  FILE *f = fopen(argv[1], "r");
  if (f == NULL) {
    printf("Error reading file %s\n", argv[1]);
    return 1;
  }

  size_t readIn;
  size_t readp;
  volatile uint8_t bytes[2];

  bool debug = true;

  while ((readp = fread(&readIn, 2, 1, f)) != 0) {
    Operation op = {};
    op.bytes[0] = readIn >> 8;
    op.bytes[1] = readIn & 0xFF;

    bytes[0] = readIn >> 8; // low
    bytes[1] = readIn & 0xFF; // high

    if (debug) {
      printf("%08b %08b    |    ", bytes[1], bytes[0]);
    }

    //printf("%b %b\n", bytes[1], bytes[0]);
    // high bits
    unsigned char opcodeBits = bytes[1] & 0b11111100;
    unsigned char shortopcodeBits = bytes[1] & 0b11111000;
    unsigned char directionBit = bytes[1] & 0b00000010; // 1=reg on left
    unsigned char wordBit = bytes[1] & 0b00000001; // ~wide 1=16bits, 0=8bits
    // low bits
    unsigned char modBits = bytes[0] & OP_MASK_MOD; // 11=reg to reg
    unsigned char regBits = bytes[0] & OP_MASK_REG;
    unsigned char rmBits = bytes[0] & OP_MASK_RM;

    bool dir = (directionBit & 0b00000010) != 0;

    // parse
    if (opcodeBits == MOVE_REGMEM_TOFROM_REG) {
      op.opcode = MOVE_REGMEM_TOFROM_REG;
      char *dst = parseRmBits(rmBits, wordBit, modBits, f);
      char *src = parseRegBits(regBits >> 3, wordBit);

      if (modBits != MOD_REG) {
        dir ? printf("mov %s, [%s]", src, dst)
          : printf("mov [%s], %s", dst, src);
      } else {
        dir ? printf("mov %s, %s", src, dst)
          : printf("mov %s, %s", dst, src);
      }
    }
    else if (shortopcodeBits == MOVE_IMM_TO_REG_8) {
      op.opcode = MOVE_IMM_TO_REG_8;
      char regMask = 0b00000111;
      char str[10];
      sprintf(str, "%d", bytes[0]);
      op.dst = parseRegBits(bytes[1] & regMask, 0);
      op.src = str;

      printf("mov %s, %s", op.dst, op.src);
    }
    else if (shortopcodeBits == MOVE_IMM_TO_REG_16) {
      op.opcode = MOVE_IMM_TO_REG_8;
      char regMask = 0b00000111;
      uint8_t additional;
      fread(&additional, 1, 1, f);
      op.bytes[2] = additional;
      uint16_t data = 0;
      data = (additional << 8) + bytes[0];
      //printf(":: %016b, %08b %08b\n", data, additional, bytes[0]);
      printf("mov %s, %d", parseRegBits(bytes[1] & regMask, 1), data);
    }
    else {
      op.opcode = opcodeBits;
      printf("unknown %08b, %08b %08b", opcodeBits, bytes[1], bytes[0]);
    }

    printf("\n");
  }

  fclose(f);
  return 0;
}