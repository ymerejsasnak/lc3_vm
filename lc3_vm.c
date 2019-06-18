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
	FL_POS = 1 << 0, // positive   b001
	FL_ZRO = 1 << 1, // zero       b010
	FL_NEG = 1 << 2 // negative    b100
}



// sign extend function (adds 1s to missing bits to make a <16 number a 16 bit number
// IF is neg number (ie 2's compliment .. sign bit is 1)
// ex. 10101 becomes 1111 1111 1111 0101
uint16_t sign_extend(uint16_t x, int bit_count)
{
	if ((x >> (bit_count - 1)) & 1) {     // if sign bit is 1
		x |= (0xFFFF << bit_count);   // places ones in all positions left of initial 'bit count'
	}
	return x;
}


// update condition flags (any time a value is written to a register!)
void update_flags(uint16_t r)
{
	if (reg[r] == 0)
	{
		reg[R_COND] = FL_ZRO;
	}
    else if (reg[r] >> 15) // ie leftmost bit is 1, thus val is neg
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}












//////////////////////////////////////////
////////////////////////////////////////
///////////////////////////////////////
///////////////////////////////////////

int main(int argc, const char* argv[])
{
	// load arguments (12)
	// setup (12)
	//
	
	// set the program counter to its starting position
	// 0x3000 is the default
	enum { PC_START = 0x3000 };
	reg[R_PC] = PC_START;

	
	// then start main processing loop
	// (load instruction at address in pc reg, inc pc reg, lookup opcode, perform instruction, loop)
	int running = 1;
	while (running)
	{
		// FETCH
		uint16_t instr = mem_read(reg[R_PC]++);
		uint16_t op = instr >> 12;  // shift whole instruction right 12 bits so only 4 bit opcode remains
	
	
	
		switch (op)
		{
            
			case OP_ADD:
                // get destination register         .... xxx. .... ....
				uint16_t dr = (instr >> 9) & 0x7; 
                // get first operand (source reg 1) .... ...x xx.. ....
                uint16_t sr1 = (instr >> 6) & 0x7; 
                // see if we are in immediate mode  .... .... ..x. ....
                uint16_t imm_flag = (instr >> 5) & 0x1;
                
                if (imm_flag)
                {
                    // sign extend rightmost 5 bits .... .... ...x xxxx
                    // to get the given value to add
                    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                    reg[dr] = reg[sr1] + imm5;
                }
                else
                {
                    // get operand from reg 2       .... .... .... .111
                    uint16_t sr2 = instr & 0x7;
                    reg[dr] = reg[sr1] + reg[sr2];
                }
                
                update_flags(dr);
				break;
                
//////////////////////////////
                
			case OP_AND: //2 modes
                // 0101 | DR (3) | SR1 (3) | 0 | 00 | SR2 (3)
                // 0101 | DR (3) | SR1 (3) | 1 | imm5
                
				uint16_t dr = (instr >> 9) & 0x7; 
                uint16_t sr1 = (instr >> 6) & 0x7; 
                uint16_t imm_flag = (instr >> 5) & 0x1;
                
                if (imm_flag)
                {
                    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                    reg[dr] = reg[sr1] & imm5;
                }
                else
                {
                    uint16_t sr2 = instr & 0x7;
                    reg[dr] = reg[sr1] & reg[sr2];
                }
                update_flags(dr);
                
				break;
                
/////////////////////////////////
                
			case OP_NOT:
                // bitwise compliment
                // 1001 | DR (3) | SR (3) | 1 | 11111
                uint16_t dr = (instr >> 9) & 0x7;
                uint16_t sr = (instr >> 6) & 0x7;
                
                reg[dr] = ~reg[sr];
				
                update_flags(dr);
				
                break;
                
///////////////////////////// 
                
			case OP_BR:
                // branch
                // 0000 | n | z | p | PCoffset9
                uint16_t nzp = (instr >> 9) & 0x7;
                
                if (nzp & reg[R_COND])
                {
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    reg[R_PC] += pc_offset;
                }
                
				break;
                
 //////////////////////////////               
                
			case OP_JMP:
                // JMP:  1100 | 000 | BaseR (3) | 000000
                // RET:  1100 | 000 | 111 | 000000
                //      (ret automatically returns to pos stored in reg7 (111)
                             
                uint16_t br = (instr >> 6) & 0x7;
                reg[R_PC] = reg[br];
                
/////////////////////////////                
                
			case OP_JSR:
                // jump to subroutine
                // JSR:  0100 | 1 | PCoffset11
                // JSRR: 0100 | 0 | 00 | BaseR (3) | 000000
                
                reg[R_R7] = reg[R_PC];
                uint16_t flag = (instr >> 11) & 0x1;
                if (flag) // JSR
                {
                    uint16_t pc_offset = sign_extend(intsr & 0x7FF, 11);
                    reg[R_PC] += pc_offset;
                }
                else // JSRR
                {
                    uint16_t br = (instr >> 6) & 0x7;
                    reg[R_PC] = reg[br];
                }
				
				break;
                
/////////////////////////////                
            
			case OP_LD:
                // load:  0010 | DR | PCoffset9
				
                uint16_t dr = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[dr] = mem_read(reg[R_PC] + pc_offset);                
                update_flags(dr);
				break;
                
/////////////////////////////                
                
			case OP_LDI: 
                // load indirect
                // 0101 | DR (3 bits) | PCoffset9 (9 bits)
                
                uint16_t dr = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9); // imm val
                
                // pc + offset = address of address of data to put in dest reg
                reg[dr] = mem_read(mem_read(reg[R_PC] + pc_offset));
                update_flags(dr);
				break;
                
/////////////////////////////                
                
			case OP_LDR:
                // load base + offset
                // 0110 | DR | BaseR | offset6
				
                uint16_t dr = (instr >> 9) & 0x7;
                uint16_t br = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                
                reg[dr] = mem_read(reg[br] + offset);
                
                update_flags(dr);
				break;
                
/////////////////////////////                
                
			case OP_LEA:
				// load effective address
                // 1110 | DR | PCoffset9
                
                uint16_t dr = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                
                reg[dr] = reg[R_PC] + pc_offset;
                
                update_flags(dr);
				break;
                
/////////////////////////////                
                
			case OP_ST:
                // store
                // 0011 | SR | PCoffset9
                
                uint16_t sr = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                
                mem_write(reg[R_PC] + pc_offset, reg[sr]);
				
				break;
                
/////////////////////////////                
                
			case OP_STI:
                // store indirect
                // 1011 | SR |PCoffset9
                
                uint16_t sr = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                
                mem_write(mem_read(reg[R_PC] + pc_offset), reg[sr]);
				
				break;
                
/////////////////////////////                
                
			case OP_STR:
                // store base + offset
                // 0111 | sr | baseR | offset6
				
                uint16_t sr = (instr >> 9) & 0x7;
                uint16_t br = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                mem_write(reg[br] + offset, reg[sr]);
				break;
                
/////////////////////////////                
                
			case OP_TRAP:
                // trap - system call (helpful stuff, i/o, etc)
                // 1111 | 0000 | trapvect8
				
                
				break;
			
/////////////////////////////            
            
			case OP_RES:
			case OP_RTI:
			default:
				//bad opcode
                abort();
				break;
		}
	
	}

	// shutdown (12)
}	
