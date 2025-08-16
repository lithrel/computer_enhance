#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

typedef enum OpCode {
  MOVE_IMM_TO_REG_8      = 0b10110000, // 4
  MOVE_IMM_TO_REG_16     = 0b10111000, // 5
  MOVE_REGMEM_TOFROM_REG = 0b10001000, // 6
  MOVE_IMM_TO_REGMEM     = 0b11000110, // 7
  MOVE_MEM_TO_ACC        = 0b10100000, // 7
  MOVE_ACC_TO_MEM        = 0b10100010, // 7
  MOVE_REGMEM_TO_SEGREG  = 0b10001110, // 8
  MOVE_SEGREG_TO_REGMEM  = 0b10001100, // 8
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

//OpCode getOpcode(unsigned char bits)
//{
//};

char* parseRegBits(unsigned char regBits, unsigned char wordBit)
{
    if (wordBit == 0b00000001) {
      regBits = regBits | 0b00001000;
    }

    return registers[regBits];
}

char* parseRmBits(
  unsigned char rmBits,
  bool word,
  unsigned char modBits,
  FILE *f
) {
    if (modBits == MOD_REG) {
      if (word) {
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

  bool debug = false;
  while ((readp = fread(&readIn, 2, 1, f)) != 0) {
    Operation op = {};
    op.bytes[0] = readIn >> 8;
    op.bytes[1] = readIn & 0xFF;

    if (debug) {
      printf("%08b %08b  |  ", op.bytes[1], op.bytes[0]);
    }

    // high bits
    unsigned char opcodeBits = op.bytes[1] & 0b11111100;
    unsigned char shortopcodeBits = op.bytes[1] & 0b11111000;
    // low bits
    unsigned char regBits = op.bytes[0] & OP_MASK_REG;
    unsigned char rmBits = op.bytes[0] & OP_MASK_RM;

    op.mode      = op.bytes[0] & OP_MASK_MOD; // Mode
    op.direction = (op.bytes[1] & OP_MASK_DIR) != 0; // 1=reg on left
    op.word      = (op.bytes[1] & OP_MASK_WRD) != 0; // ~wide 1=16bits, 0=8bits

    // parse
    if (opcodeBits == MOVE_REGMEM_TOFROM_REG) {
      op.opcode = MOVE_REGMEM_TOFROM_REG;
      char *dst = parseRmBits(rmBits, op.word, op.mode, f);
      char *src = parseRegBits(regBits >> 3, op.word);

      if (op.mode != MOD_REG) {
        op.direction ? printf("mov %s, [%s]", src, dst)
          : printf("mov [%s], %s", dst, src);
      } else {
        op.direction ? printf("mov %s, %s", src, dst)
          : printf("mov %s, %s", dst, src);
      }
    }
    else if (shortopcodeBits == MOVE_IMM_TO_REG_8) {
      op.opcode = MOVE_IMM_TO_REG_8;
      char regMask = 0b00000111;
      char str[10];
      sprintf(str, "%d", op.bytes[0]);
      op.dst = parseRegBits(op.bytes[1] & regMask, 0);
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
      data = (additional << 8) + op.bytes[0];
      //printf(":: %016b, %08b %08b\n", data, additional, bytes[0]);
      printf("mov %s, %d", parseRegBits(op.bytes[1] & regMask, 1), data);
    }
    else {
      op.opcode = opcodeBits;
      printf("??? %08b, %08b %08b", op.opcode, op.bytes[1], op.bytes[0]);
    }

    printf("\n");
  }

  fclose(f);
  return 0;
}