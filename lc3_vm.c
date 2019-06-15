#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

//////TEMP NOTE
//THESE INCLUDES ALL ADDED NOW BUT NOT NEC USED YET....LOOK THEM UP, ETC

// ON PART 4 NEXT


// 65536 (2^16) memory locations, each can store a 16 bit value (128kb total) 
uint16_t memory[UINT16_MAX];


//////////////////////////////////////////
// Registers (16 bits each)
enum
{
	// general purpose registers
	R_R0 = 0,
	R_R1,
	R_R2,
	R_R3,
	R_R4,
	R_R5,
	R_R6,
	R_R7,
	// program counter register
	R_PC,
	// condition flag register
	R_COND,
	// number of registers total:
	R_COUNT
};

// Store the registers in an array (16 bit value per register)
uint16_t reg[R_COUNT];


////////////////////////////////////////
// Opcodes (CPU instruction set)
// each instruction will be 16 bits long with left 4 bits storing opcode, right 12 storing parameters
enum
{
	OP_BR = 0, // branch
	OP_ADD,    // add
	OP_LD,     // load
	OP_ST,     // store
	OP_JSR,    // jump register
	OP_AND,    // bitwise AND
	OP_LDR,    // load register
	OP_STR,    // store register
	OP_RTI,    // UNUSED
	OP_NOT,    // bitwise NOT
	OP_LDI,    // load indirect
	OP_STI,    // store indirect
	OP_JMP,    // jump
	OP_RES,    // reserved UNUSED
	OP_LEA,    // load effective address
	OP_TRAP    // execute trap
};


//////////////////////////////////////
// Condition flags
// (provide info about most recent calculation, LC3 has 3 to indicate sign of last calc)
enum
{
	FL_POS = 1 << 0, // positive 
	FL_ZRO = 1 << 1, // zero     
	FL_NEG = 1 << 2, // negative


	
