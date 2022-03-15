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
};


/////////////////////////////////////////
// Trap Codes
enum
{
    TRAP_GETC = 0x20,   // get keyboard char input, no echo
    TRAP_OUT = 0x21,    // output character
    TRAP_PUTS = 0x22,   // output word string
    TRAP_IN = 0x23,    // get keyboard char, echoed to terminal
    TRAP_PUTSP = 0x24,  // output byte string
    TRAP_HALT = 0x25    // end program
};



////////////////////
// Memory Mapped Registers
enum
{
    MR_KBSR = 0xFE00, // keyboard status register
    MR_KBDR = 0xFE02, // keyboard data register
};



// 65536 (2^16) memory locations, each can store a 16 bit value (128kb total) 
uint16_t memory[UINT16_MAX];

// Store the registers in an array (16 bit value per register)
uint16_t reg[R_COUNT];



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

// swap endianness of 16bit number
uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}


// read lc-3 program into memory
void read_image_file(FILE* file)
{
    uint16_t origin; // origin is where in memory to place the image
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin); // lc3 programs are big endian 

    uint16_t max_read = UINT16_MAX - origin; // biggest size program can possibly be given that total mem is uint16_max
    uint16_t* p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    // go back through read data, swapping bytes to little endian
    while (read-- > 0)
    {
        *p = swap16(*p);
        ++p;
    }
}






// read image file given string path
int read_image(const char* image_path)
{
    FILE* file = fopen(image_path, "rb");
    if (!file) { return 0; }
    read_image_file(file);
    fclose(file);
    return 1;
}

uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

// setter/getter memory functions 
void mem_write(uint16_t address, uint16_t val)
{
    memory[address] = val;
}

uint16_t mem_read(uint16_t address)
{
    if (address == MR_KBSR)
    {
        if (check_key())
        {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
    }
    else
    {
        memory[MR_KBSR] = 0;
    }
    return memory[address];
}





////////////////////////////////
// UNIX terminal input setup (input buffering)

struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

//////////////////////////////////////



void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}


//////////////////////////////////////////
////////////////////////////////////////
///////////////////////////////////////
///////////////////////////////////////

int main(int argc, const char* argv[])
{

    // load arguments
	if (argc < 2)
    {
        // show usage string
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }

    for (int j = 1; j < argc; ++j) 
    {
        if (!read_image(argv[j])) 
        {
            printf("failed to load image %s\n", argv[j]);
            exit(1);
        }
    }




	// setup 
	signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    // start with condition flag set to 0
    update_flags(0);
	
	
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
	
	



//fixme - cases need braces if variables declared inside them
// (but this is not necessarily best practice?  add braces or find better solution)


    
		switch (op)
		{
            
			case OP_ADD: // 2 modes (register or immediate)
            {
                // 0001 | DR (3) | SR1 (3) | 0 | 00 | SR2 (3)
                // 0001 | DR (3) | SR1 (3) | 0 | imm5 (5)
            
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
            }
			break;
                
//////////////////////////////
                
			case OP_AND: //2 modes
            {
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
            }    
			break;
                
/////////////////////////////////
                
			case OP_NOT:
            {
                // bitwise compliment
                // 1001 | DR (3) | SR (3) | 1 | 11111
                uint16_t dr = (instr >> 9) & 0x7;
                uint16_t sr = (instr >> 6) & 0x7;
                
                reg[dr] = ~reg[sr];
				
                update_flags(dr);
            }	
            break;
                
///////////////////////////// 
                
			case OP_BR:
            {
                // branch
                // 0000 | n | z | p | PCoffset9
                uint16_t nzp = (instr >> 9) & 0x7;
                
                if (nzp & reg[R_COND])
                {
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    reg[R_PC] += pc_offset;
                }
            }   
			break;
                
 //////////////////////////////               
                
			case OP_JMP:
            {
                // JMP:  1100 | 000 | BaseR (3) | 000000
                // RET:  1100 | 000 | 111 | 000000
                //      (ret automatically returns to pos stored in reg7 (111)
                             
                uint16_t br = (instr >> 6) & 0x7;
                reg[R_PC] = reg[br];
            }
            break;
                
/////////////////////////////                
                
			case OP_JSR:
            {
                // jump to subroutine
                // JSR:  0100 | 1 | PCoffset11
                // JSRR: 0100 | 0 | 00 | BaseR (3) | 000000
                
                reg[R_R7] = reg[R_PC];
                uint16_t flag = (instr >> 11) & 0x1;
                if (flag) // JSR
                {
                    uint16_t pc_offset = sign_extend(instr & 0x7FF, 11);
                    reg[R_PC] += pc_offset;
                }
                else // JSRR
                {
                    uint16_t br = (instr >> 6) & 0x7;
                    reg[R_PC] = reg[br];
                }
            }	
			break;
                
/////////////////////////////                
            
			case OP_LD:
            {
                // load:  0010 | DR | PCoffset9
				
                uint16_t dr = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[dr] = mem_read(reg[R_PC] + pc_offset);                
                update_flags(dr);
            }
            break;
                
/////////////////////////////                
                
			case OP_LDI: 
            {
                // load indirect
                // 0101 | DR (3 bits) | PCoffset9 (9 bits)
                
                uint16_t dr = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9); // imm val
                
                // pc + offset = address of address of data to put in dest reg
                reg[dr] = mem_read(mem_read(reg[R_PC] + pc_offset));
                update_flags(dr);
            }
            break;
                
/////////////////////////////                
                
			case OP_LDR:
            {
                // load base + offset
                // 0110 | DR | BaseR | offset6
				
                uint16_t dr = (instr >> 9) & 0x7;
                uint16_t br = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                
                reg[dr] = mem_read(reg[br] + offset);
                
                update_flags(dr);
            }
            break;
                
/////////////////////////////                
                
			case OP_LEA:
            {
				// load effective address
                // 1110 | DR | PCoffset9
                
                uint16_t dr = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                
                reg[dr] = reg[R_PC] + pc_offset;
                
                update_flags(dr);
            }
            break;
                
/////////////////////////////                
                
			case OP_ST:
            {
                // store
                // 0011 | SR | PCoffset9
                
                uint16_t sr = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                
                mem_write(reg[R_PC] + pc_offset, reg[sr]);
            }	
			break;
                
/////////////////////////////                
                
			case OP_STI:
            {
                // store indirect
                // 1011 | SR |PCoffset9
                
                uint16_t sr = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                
                mem_write(mem_read(reg[R_PC] + pc_offset), reg[sr]);
            }
			break;
                
/////////////////////////////                
                
			case OP_STR:
            {
                // store base + offset
                // 0111 | sr | baseR | offset6
				
                uint16_t sr = (instr >> 9) & 0x7;
                uint16_t br = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                mem_write(reg[br] + offset, reg[sr]);
            }
            break;
                
/////////////////////////////                
                
			case OP_TRAP:
            {
                // trap - system call (helpful stuff, i/o, etc)
                // 1111 | 0000 | trapvect8
				
                switch (instr & 0xFF)
                {
                    case TRAP_GETC:
                    {
                        // read single ASCII char into R0
                        reg[R_R0] = (uint16_t)getchar();
                        update_flags(R_R0);
                    }
                    break;
                    
                    case TRAP_OUT:
                    {
                        // output single character at R_R0
                        putc((char)reg[R_R0], stdout);
                        fflush(stdout);
                    }
                    break;
                    
                    case TRAP_PUTS:
                    {
                        //Write a string of ASCII characters to the console display.
                        // The characters are contained in consecutive memory locations, 
                        // one character per memory location, starting with the address specified in R0.
                        // Writing terminates with the occurrence of x0000 in a memory location
                        
                        uint16_t* c = memory + reg[R_R0];  // address of mem (external from vm?) + address of r0
                        while (*c) // until 0
                        {
                            putc((char)*c, stdout); // convert to char and output 
                            ++c; // increment pointer
                        }
                        fflush(stdout);
                    } 
                    break;
                    
                    case TRAP_IN:
                    { 
                        // prompt for input
                        printf("Enter a character: ");
                        char c = getchar();
                        putc(c, stdout); // echo input to terminal
                        fflush(stdout);
                        reg[R_R0] = (uint16_t)c;
                        update_flags(R_R0);
                    }
                    break;
                    
                    case TRAP_PUTSP:
                    {
                        // output byte string
                        // one char per byte (2 bytes per word)
                        // need to swap back to big endian
                        uint16_t* c = memory + reg[R_R0];
                        while(*c)
                        {
                            char char1 = (*c) & 0xFF;
                            putc(char1, stdout);
                            char char2 = (*c) >> 8;
                            if (char2) putc(char2, stdout);
                            ++c;
                            fflush(stdout);
                        }
                    }
                    break;
                    
                    case TRAP_HALT:
                    {
                        puts("HALT");
                        fflush(stdout);
                        running = 0;
                    }
                    break;
                }
            }
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

	// shutdown
    restore_input_buffering();
}	
