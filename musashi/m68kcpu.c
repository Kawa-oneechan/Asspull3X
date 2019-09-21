/* ======================================================================== */
/* ========================= LICENSING & COPYRIGHT ======================== */
/* ======================================================================== */
/*
 *                                  MUSASHI
 *                                Version 3.31
 *
 * A portable Motorola M680x0 processor emulation engine.
 * Copyright Karl Stenerud.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


/* ======================================================================== */
/* ================================= NOTES ================================ */
/* ======================================================================== */



/* ======================================================================== */
/* ================================ INCLUDES ============================== */
/* ======================================================================== */

extern void m68040_fpu_op0(void);
extern void m68040_fpu_op1(void);
extern unsigned char m68ki_cycles[][0x10000];
extern void (*m68ki_instruction_jump_table[0x10000])(void); /* opcode handler jump table */
extern void m68ki_build_opcode_table(void);

#include "m68kops.h"
#include "m68kcpu.h"
#include "m68kfpu.c"

/* ======================================================================== */
/* ================================= DATA ================================= */
/* ======================================================================== */

int  m68ki_initial_cycles;
int  m68ki_remaining_cycles = 0;                     /* Number of clocks remaining */
uint m68ki_tracing = 0;
uint m68ki_address_space;

#ifdef M68K_LOG_ENABLE
const char *const m68ki_cpu_names[] =
{
	"Invalid CPU",
	"M68000",
	"M68010",
	"Invalid CPU",
	"M68EC020"
	"Invalid CPU",
	"Invalid CPU",
	"Invalid CPU",
	"M68020"
};
#endif /* M68K_LOG_ENABLE */

/* The CPU core */
m68ki_cpu_core m68ki_cpu = {0};

#if M68K_EMULATE_ADDRESS_ERROR
jmp_buf m68ki_aerr_trap;
#endif /* M68K_EMULATE_ADDRESS_ERROR */

uint    m68ki_aerr_address;
uint    m68ki_aerr_write_mode;
uint    m68ki_aerr_fc;

/* Used by shift & rotate instructions */
const uint8 m68ki_shift_8_table[65] =
{
	0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff
};
const uint16 m68ki_shift_16_table[65] =
{
	0x0000, 0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00,
	0xff80, 0xffc0, 0xffe0, 0xfff0, 0xfff8, 0xfffc, 0xfffe, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff
};
const uint m68ki_shift_32_table[65] =
{
	0x00000000, 0x80000000, 0xc0000000, 0xe0000000, 0xf0000000, 0xf8000000,
	0xfc000000, 0xfe000000, 0xff000000, 0xff800000, 0xffc00000, 0xffe00000,
	0xfff00000, 0xfff80000, 0xfffc0000, 0xfffe0000, 0xffff0000, 0xffff8000,
	0xffffc000, 0xffffe000, 0xfffff000, 0xfffff800, 0xfffffc00, 0xfffffe00,
	0xffffff00, 0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0, 0xfffffff8,
	0xfffffffc, 0xfffffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};


/* Number of clock cycles to use for exception processing.
 * I used 4 for any vectors that are undocumented for processing times.
 */
const uint8 m68ki_exception_cycle_table[4][256] =
{
	{ /* 000 */
		  4, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 34, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero -- ASG: changed from 42             */
		 40, /*  6: CHK -- ASG: chanaged from 44                       */
		 34, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 34, /*  9: Trace                                              */
		 34, /* 10: 1010                                               */
		 34, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 44, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 44, /* 24: Spurious Interrupt                                 */
		 44, /* 25: Level 1 Interrupt Autovector                       */
		 44, /* 26: Level 2 Interrupt Autovector                       */
		 44, /* 27: Level 3 Interrupt Autovector                       */
		 44, /* 28: Level 4 Interrupt Autovector                       */
		 44, /* 29: Level 5 Interrupt Autovector                       */
		 44, /* 30: Level 6 Interrupt Autovector                       */
		 44, /* 31: Level 7 Interrupt Autovector                       */
		 34, /* 32: TRAP #0 -- ASG: chanaged from 38                   */
		 34, /* 33: TRAP #1                                            */
		 34, /* 34: TRAP #2                                            */
		 34, /* 35: TRAP #3                                            */
		 34, /* 36: TRAP #4                                            */
		 34, /* 37: TRAP #5                                            */
		 34, /* 38: TRAP #6                                            */
		 34, /* 39: TRAP #7                                            */
		 34, /* 40: TRAP #8                                            */
		 34, /* 41: TRAP #9                                            */
		 34, /* 42: TRAP #10                                           */
		 34, /* 43: TRAP #11                                           */
		 34, /* 44: TRAP #12                                           */
		 34, /* 45: TRAP #13                                           */
		 34, /* 46: TRAP #14                                           */
		 34, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
	{ /* 010 */
		  4, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		126, /*  2: Bus Error                             (unemulated) */
		126, /*  3: Address Error                         (unemulated) */
		 38, /*  4: Illegal Instruction                                */
		 44, /*  5: Divide by Zero                                     */
		 44, /*  6: CHK                                                */
		 34, /*  7: TRAPV                                              */
		 38, /*  8: Privilege Violation                                */
		 38, /*  9: Trace                                              */
		  4, /* 10: 1010                                               */
		  4, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 44, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 46, /* 24: Spurious Interrupt                                 */
		 46, /* 25: Level 1 Interrupt Autovector                       */
		 46, /* 26: Level 2 Interrupt Autovector                       */
		 46, /* 27: Level 3 Interrupt Autovector                       */
		 46, /* 28: Level 4 Interrupt Autovector                       */
		 46, /* 29: Level 5 Interrupt Autovector                       */
		 46, /* 30: Level 6 Interrupt Autovector                       */
		 46, /* 31: Level 7 Interrupt Autovector                       */
		 38, /* 32: TRAP #0                                            */
		 38, /* 33: TRAP #1                                            */
		 38, /* 34: TRAP #2                                            */
		 38, /* 35: TRAP #3                                            */
		 38, /* 36: TRAP #4                                            */
		 38, /* 37: TRAP #5                                            */
		 38, /* 38: TRAP #6                                            */
		 38, /* 39: TRAP #7                                            */
		 38, /* 40: TRAP #8                                            */
		 38, /* 41: TRAP #9                                            */
		 38, /* 42: TRAP #10                                           */
		 38, /* 43: TRAP #11                                           */
		 38, /* 44: TRAP #12                                           */
		 38, /* 45: TRAP #13                                           */
		 38, /* 46: TRAP #14                                           */
		 38, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
	{ /* 020 */
		  4, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 20, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero                                     */
		 40, /*  6: CHK                                                */
		 20, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 25, /*  9: Trace                                              */
		 20, /* 10: 1010                                               */
		 20, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 30, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 30, /* 24: Spurious Interrupt                                 */
		 30, /* 25: Level 1 Interrupt Autovector                       */
		 30, /* 26: Level 2 Interrupt Autovector                       */
		 30, /* 27: Level 3 Interrupt Autovector                       */
		 30, /* 28: Level 4 Interrupt Autovector                       */
		 30, /* 29: Level 5 Interrupt Autovector                       */
		 30, /* 30: Level 6 Interrupt Autovector                       */
		 30, /* 31: Level 7 Interrupt Autovector                       */
		 20, /* 32: TRAP #0                                            */
		 20, /* 33: TRAP #1                                            */
		 20, /* 34: TRAP #2                                            */
		 20, /* 35: TRAP #3                                            */
		 20, /* 36: TRAP #4                                            */
		 20, /* 37: TRAP #5                                            */
		 20, /* 38: TRAP #6                                            */
		 20, /* 39: TRAP #7                                            */
		 20, /* 40: TRAP #8                                            */
		 20, /* 41: TRAP #9                                            */
		 20, /* 42: TRAP #10                                           */
		 20, /* 43: TRAP #11                                           */
		 20, /* 44: TRAP #12                                           */
		 20, /* 45: TRAP #13                                           */
		 20, /* 46: TRAP #14                                           */
		 20, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
	{ /* 040 */ // TODO: these values are not correct
		  4, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 20, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero                                     */
		 40, /*  6: CHK                                                */
		 20, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 25, /*  9: Trace                                              */
		 20, /* 10: 1010                                               */
		 20, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 30, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 30, /* 24: Spurious Interrupt                                 */
		 30, /* 25: Level 1 Interrupt Autovector                       */
		 30, /* 26: Level 2 Interrupt Autovector                       */
		 30, /* 27: Level 3 Interrupt Autovector                       */
		 30, /* 28: Level 4 Interrupt Autovector                       */
		 30, /* 29: Level 5 Interrupt Autovector                       */
		 30, /* 30: Level 6 Interrupt Autovector                       */
		 30, /* 31: Level 7 Interrupt Autovector                       */
		 20, /* 32: TRAP #0                                            */
		 20, /* 33: TRAP #1                                            */
		 20, /* 34: TRAP #2                                            */
		 20, /* 35: TRAP #3                                            */
		 20, /* 36: TRAP #4                                            */
		 20, /* 37: TRAP #5                                            */
		 20, /* 38: TRAP #6                                            */
		 20, /* 39: TRAP #7                                            */
		 20, /* 40: TRAP #8                                            */
		 20, /* 41: TRAP #9                                            */
		 20, /* 42: TRAP #10                                           */
		 20, /* 43: TRAP #11                                           */
		 20, /* 44: TRAP #12                                           */
		 20, /* 45: TRAP #13                                           */
		 20, /* 46: TRAP #14                                           */
		 20, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	}
};

const uint8 m68ki_ea_idx_cycle_table[64] =
{
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0, /* ..01.000 no memory indirect, base NULL             */
	 5, /* ..01..01 memory indirect,    base NULL, outer NULL */
	 7, /* ..01..10 memory indirect,    base NULL, outer 16   */
	 7, /* ..01..11 memory indirect,    base NULL, outer 32   */
	 0,  5,  7,  7,  0,  5,  7,  7,  0,  5,  7,  7,
	 2, /* ..10.000 no memory indirect, base 16               */
	 7, /* ..10..01 memory indirect,    base 16,   outer NULL */
	 9, /* ..10..10 memory indirect,    base 16,   outer 16   */
	 9, /* ..10..11 memory indirect,    base 16,   outer 32   */
	 0,  7,  9,  9,  0,  7,  9,  9,  0,  7,  9,  9,
	 6, /* ..11.000 no memory indirect, base 32               */
	11, /* ..11..01 memory indirect,    base 32,   outer NULL */
	13, /* ..11..10 memory indirect,    base 32,   outer 16   */
	13, /* ..11..11 memory indirect,    base 32,   outer 32   */
	 0, 11, 13, 13,  0, 11, 13, 13,  0, 11, 13, 13
};



/* ======================================================================== */
/* =========================== UTILITY FUNCTIONS ========================== */
/* ======================================================================== */


/* ---------------------------- Read Immediate ---------------------------- */

/* Handles all immediate reads, does address error check, function code setting,
 * and prefetching if they are enabled in m68kconf.h
 */
INLINE uint m68ki_read_imm_16(void)
{
	m68ki_set_fc(FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */
	m68ki_check_address_error(REG_PC, MODE_READ, FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */
#if M68K_EMULATE_PREFETCH
{
	uint result;
	if(REG_PC != CPU_PREF_ADDR)
	{
		CPU_PREF_ADDR = REG_PC;
		CPU_PREF_DATA = m68k_read_immediate_16(ADDRESS_68K(CPU_PREF_ADDR));
	}
	result = MASK_OUT_ABOVE_16(CPU_PREF_DATA);
	REG_PC += 2;
	CPU_PREF_ADDR = REG_PC;
	CPU_PREF_DATA = m68k_read_immediate_16(ADDRESS_68K(CPU_PREF_ADDR));
	return result;
}
#else
	REG_PC += 2;
	return m68k_read_immediate_16(ADDRESS_68K(REG_PC-2));
#endif /* M68K_EMULATE_PREFETCH */
}
INLINE uint m68ki_read_imm_32(void)
{
#if M68K_EMULATE_PREFETCH
	uint temp_val;

	m68ki_set_fc(FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */
	m68ki_check_address_error(REG_PC, MODE_READ, FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */

	if(REG_PC != CPU_PREF_ADDR)
	{
		CPU_PREF_ADDR = REG_PC;
		CPU_PREF_DATA = m68k_read_immediate_16(ADDRESS_68K(CPU_PREF_ADDR));
	}
	temp_val = MASK_OUT_ABOVE_16(CPU_PREF_DATA);
	REG_PC += 2;
	CPU_PREF_ADDR = REG_PC;
	CPU_PREF_DATA = m68k_read_immediate_16(ADDRESS_68K(CPU_PREF_ADDR));

	temp_val = MASK_OUT_ABOVE_32((temp_val << 16) | MASK_OUT_ABOVE_16(CPU_PREF_DATA));
	REG_PC += 2;
	CPU_PREF_ADDR = REG_PC;
	CPU_PREF_DATA = m68k_read_immediate_16(ADDRESS_68K(CPU_PREF_ADDR));

	return temp_val;
#else
	m68ki_set_fc(FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */
	m68ki_check_address_error(REG_PC, MODE_READ, FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */
	REG_PC += 4;
	return m68k_read_immediate_32(ADDRESS_68K(REG_PC-4));
#endif /* M68K_EMULATE_PREFETCH */
}



/* ------------------------- Top level read/write ------------------------- */

/* Handles all memory accesses (except for immediate reads if they are
 * configured to use separate functions in m68kconf.h).
 * All memory accesses must go through these top level functions.
 * These functions will also check for address error and set the function
 * code if they are enabled in m68kconf.h.
 */
INLINE uint m68ki_read_8_fc(uint address, uint fc)
{
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	return m68k_read_memory_8(ADDRESS_68K(address));
}
INLINE uint m68ki_read_16_fc(uint address, uint fc)
{
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	m68ki_check_address_error_010_less(address, MODE_READ, fc); /* auto-disable (see m68kcpu.h) */
	return m68k_read_memory_16(ADDRESS_68K(address));
}
INLINE uint m68ki_read_32_fc(uint address, uint fc)
{
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	m68ki_check_address_error_010_less(address, MODE_READ, fc); /* auto-disable (see m68kcpu.h) */
	return m68k_read_memory_32(ADDRESS_68K(address));
}

INLINE void m68ki_write_8_fc(uint address, uint fc, uint value)
{
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	m68k_write_memory_8(ADDRESS_68K(address), value);
}
INLINE void m68ki_write_16_fc(uint address, uint fc, uint value)
{
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	m68ki_check_address_error_010_less(address, MODE_WRITE, fc); /* auto-disable (see m68kcpu.h) */
	m68k_write_memory_16(ADDRESS_68K(address), value);
}
INLINE void m68ki_write_32_fc(uint address, uint fc, uint value)
{
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	m68ki_check_address_error_010_less(address, MODE_WRITE, fc); /* auto-disable (see m68kcpu.h) */
	m68k_write_memory_32(ADDRESS_68K(address), value);
}

#if M68K_SIMULATE_PD_WRITES
INLINE void m68ki_write_32_pd_fc(uint address, uint fc, uint value)
{
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	m68ki_check_address_error_010_less(address, MODE_WRITE, fc); /* auto-disable (see m68kcpu.h) */
	m68k_write_memory_32_pd(ADDRESS_68K(address), value);
}
#endif


/* --------------------- Effective Address Calculation -------------------- */

/* The program counter relative addressing modes cause operands to be
 * retrieved from program space, not data space.
 */
INLINE uint m68ki_get_ea_pcdi(void)
{
	uint old_pc = REG_PC;
	m68ki_use_program_space(); /* auto-disable */
	return old_pc + MAKE_INT_16(m68ki_read_imm_16());
}


INLINE uint m68ki_get_ea_pcix(void)
{
	m68ki_use_program_space(); /* auto-disable */
	return m68ki_get_ea_ix(REG_PC);
}

/* Indexed addressing modes are encoded as follows:
 *
 * Base instruction format:
 * F E D C B A 9 8 7 6 | 5 4 3 | 2 1 0
 * x x x x x x x x x x | 1 1 0 | BASE REGISTER      (An)
 *
 * Base instruction format for destination EA in move instructions:
 * F E D C | B A 9    | 8 7 6 | 5 4 3 2 1 0
 * x x x x | BASE REG | 1 1 0 | X X X X X X       (An)
 *
 * Brief extension format:
 *  F  |  E D C   |  B  |  A 9  | 8 | 7 6 5 4 3 2 1 0
 * D/A | REGISTER | W/L | SCALE | 0 |  DISPLACEMENT
 *
 * Full extension format:
 *  F     E D C      B     A 9    8   7    6    5 4       3   2 1 0
 * D/A | REGISTER | W/L | SCALE | 1 | BS | IS | BD SIZE | 0 | I/IS
 * BASE DISPLACEMENT (0, 16, 32 bit)                (bd)
 * OUTER DISPLACEMENT (0, 16, 32 bit)               (od)
 *
 * D/A:     0 = Dn, 1 = An                          (Xn)
 * W/L:     0 = W (sign extend), 1 = L              (.SIZE)
 * SCALE:   00=1, 01=2, 10=4, 11=8                  (*SCALE)
 * BS:      0=add base reg, 1=suppress base reg     (An suppressed)
 * IS:      0=add index, 1=suppress index           (Xn suppressed)
 * BD SIZE: 00=reserved, 01=NULL, 10=Word, 11=Long  (size of bd)
 *
 * IS I/IS Operation
 * 0  000  No Memory Indirect
 * 0  001  indir prex with null outer
 * 0  010  indir prex with word outer
 * 0  011  indir prex with long outer
 * 0  100  reserved
 * 0  101  indir postx with null outer
 * 0  110  indir postx with word outer
 * 0  111  indir postx with long outer
 * 1  000  no memory indirect
 * 1  001  mem indir with null outer
 * 1  010  mem indir with word outer
 * 1  011  mem indir with long outer
 * 1  100-111  reserved
 */
INLINE uint m68ki_get_ea_ix(uint An)
{
	/* An = base register */
	uint extension = m68ki_read_imm_16();
	uint Xn = 0;                        /* Index register */
	uint bd = 0;                        /* Base Displacement */
	uint od = 0;                        /* Outer Displacement */

	if(CPU_TYPE_IS_010_LESS(CPU_TYPE))
	{
		/* Calculate index */
		Xn = REG_DA[extension>>12];     /* Xn */
		if(!BIT_B(extension))           /* W/L */
			Xn = MAKE_INT_16(Xn);

		/* Add base register and displacement and return */
		return An + Xn + MAKE_INT_8(extension);
	}

	/* Brief extension format */
	if(!BIT_8(extension))
	{
		/* Calculate index */
		Xn = REG_DA[extension>>12];     /* Xn */
		if(!BIT_B(extension))           /* W/L */
			Xn = MAKE_INT_16(Xn);
		/* Add scale if proper CPU type */
		if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
			Xn <<= (extension>>9) & 3;  /* SCALE */

		/* Add base register and displacement and return */
		return An + Xn + MAKE_INT_8(extension);
	}

	/* Full extension format */

	USE_CYCLES(m68ki_ea_idx_cycle_table[extension&0x3f]);

	/* Check if base register is present */
	if(BIT_7(extension))                /* BS */
		An = 0;                         /* An */

	/* Check if index is present */
	if(!BIT_6(extension))               /* IS */
	{
		Xn = REG_DA[extension>>12];     /* Xn */
		if(!BIT_B(extension))           /* W/L */
			Xn = MAKE_INT_16(Xn);
		Xn <<= (extension>>9) & 3;      /* SCALE */
	}

	/* Check if base displacement is present */
	if(BIT_5(extension))                /* BD SIZE */
		bd = BIT_4(extension) ? m68ki_read_imm_32() : MAKE_INT_16(m68ki_read_imm_16());

	/* If no indirect action, we are done */
	if(!(extension&7))                  /* No Memory Indirect */
		return An + bd + Xn;

	/* Check if outer displacement is present */
	if(BIT_1(extension))                /* I/IS:  od */
		od = BIT_0(extension) ? m68ki_read_imm_32() : MAKE_INT_16(m68ki_read_imm_16());

	/* Postindex */
	if(BIT_2(extension))                /* I/IS:  0 = preindex, 1 = postindex */
		return m68ki_read_32(An + bd) + Xn + od;

	/* Preindex */
	return m68ki_read_32(An + bd + Xn) + od;
}


/* Fetch operands */
INLINE uint OPER_AY_AI_8(void)  {uint ea = EA_AY_AI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AY_AI_16(void) {uint ea = EA_AY_AI_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AY_AI_32(void) {uint ea = EA_AY_AI_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AY_PI_8(void)  {uint ea = EA_AY_PI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AY_PI_16(void) {uint ea = EA_AY_PI_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AY_PI_32(void) {uint ea = EA_AY_PI_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AY_PD_8(void)  {uint ea = EA_AY_PD_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AY_PD_16(void) {uint ea = EA_AY_PD_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AY_PD_32(void) {uint ea = EA_AY_PD_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AY_DI_8(void)  {uint ea = EA_AY_DI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AY_DI_16(void) {uint ea = EA_AY_DI_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AY_DI_32(void) {uint ea = EA_AY_DI_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AY_IX_8(void)  {uint ea = EA_AY_IX_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AY_IX_16(void) {uint ea = EA_AY_IX_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AY_IX_32(void) {uint ea = EA_AY_IX_32(); return m68ki_read_32(ea);}

INLINE uint OPER_AX_AI_8(void)  {uint ea = EA_AX_AI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AX_AI_16(void) {uint ea = EA_AX_AI_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AX_AI_32(void) {uint ea = EA_AX_AI_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AX_PI_8(void)  {uint ea = EA_AX_PI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AX_PI_16(void) {uint ea = EA_AX_PI_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AX_PI_32(void) {uint ea = EA_AX_PI_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AX_PD_8(void)  {uint ea = EA_AX_PD_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AX_PD_16(void) {uint ea = EA_AX_PD_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AX_PD_32(void) {uint ea = EA_AX_PD_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AX_DI_8(void)  {uint ea = EA_AX_DI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AX_DI_16(void) {uint ea = EA_AX_DI_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AX_DI_32(void) {uint ea = EA_AX_DI_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AX_IX_8(void)  {uint ea = EA_AX_IX_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AX_IX_16(void) {uint ea = EA_AX_IX_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AX_IX_32(void) {uint ea = EA_AX_IX_32(); return m68ki_read_32(ea);}

INLINE uint OPER_A7_PI_8(void)  {uint ea = EA_A7_PI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_A7_PD_8(void)  {uint ea = EA_A7_PD_8();  return m68ki_read_8(ea); }

INLINE uint OPER_AW_8(void)     {uint ea = EA_AW_8();     return m68ki_read_8(ea); }
INLINE uint OPER_AW_16(void)    {uint ea = EA_AW_16();    return m68ki_read_16(ea);}
INLINE uint OPER_AW_32(void)    {uint ea = EA_AW_32();    return m68ki_read_32(ea);}
INLINE uint OPER_AL_8(void)     {uint ea = EA_AL_8();     return m68ki_read_8(ea); }
INLINE uint OPER_AL_16(void)    {uint ea = EA_AL_16();    return m68ki_read_16(ea);}
INLINE uint OPER_AL_32(void)    {uint ea = EA_AL_32();    return m68ki_read_32(ea);}
INLINE uint OPER_PCDI_8(void)   {uint ea = EA_PCDI_8();   return m68ki_read_pcrel_8(ea); }
INLINE uint OPER_PCDI_16(void)  {uint ea = EA_PCDI_16();  return m68ki_read_pcrel_16(ea);}
INLINE uint OPER_PCDI_32(void)  {uint ea = EA_PCDI_32();  return m68ki_read_pcrel_32(ea);}
INLINE uint OPER_PCIX_8(void)   {uint ea = EA_PCIX_8();   return m68ki_read_pcrel_8(ea); }
INLINE uint OPER_PCIX_16(void)  {uint ea = EA_PCIX_16();  return m68ki_read_pcrel_16(ea);}
INLINE uint OPER_PCIX_32(void)  {uint ea = EA_PCIX_32();  return m68ki_read_pcrel_32(ea);}



/* ---------------------------- Stack Functions --------------------------- */

/* Push/pull data from the stack */
INLINE void m68ki_push_16(uint value)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP - 2);
	m68ki_write_16(REG_SP, value);
}

INLINE void m68ki_push_32(uint value)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP - 4);
	m68ki_write_32(REG_SP, value);
}

INLINE uint m68ki_pull_16(void)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP + 2);
	return m68ki_read_16(REG_SP-2);
}

INLINE uint m68ki_pull_32(void)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP + 4);
	return m68ki_read_32(REG_SP-4);
}


/* Increment/decrement the stack as if doing a push/pull but
 * don't do any memory access.
 */
INLINE void m68ki_fake_push_16(void)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP - 2);
}

INLINE void m68ki_fake_push_32(void)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP - 4);
}

INLINE void m68ki_fake_pull_16(void)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP + 2);
}

INLINE void m68ki_fake_pull_32(void)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP + 4);
}


/* ----------------------------- Program Flow ----------------------------- */

/* Jump to a new program location or vector.
 * These functions will also call the pc_changed callback if it was enabled
 * in m68kconf.h.
 */
INLINE void m68ki_jump(uint new_pc)
{
	REG_PC = new_pc;
	m68ki_pc_changed(REG_PC);
}

INLINE void m68ki_jump_vector(uint vector)
{
	REG_PC = (vector<<2) + REG_VBR;
	REG_PC = m68ki_read_data_32(REG_PC);
	m68ki_pc_changed(REG_PC);
}


/* Branch to a new memory location.
 * The 32-bit branch will call pc_changed if it was enabled in m68kconf.h.
 * So far I've found no problems with not calling pc_changed for 8 or 16
 * bit branches.
 */
INLINE void m68ki_branch_8(uint offset)
{
	REG_PC += MAKE_INT_8(offset);
}

INLINE void m68ki_branch_16(uint offset)
{
	REG_PC += MAKE_INT_16(offset);
}

INLINE void m68ki_branch_32(uint offset)
{
	REG_PC += offset;
	m68ki_pc_changed(REG_PC);
}



/* ---------------------------- Status Register --------------------------- */

/* Set the S flag and change the active stack pointer.
 * Note that value MUST be 4 or 0.
 */
INLINE void m68ki_set_s_flag(uint value)
{
	/* Backup the old stack pointer */
	REG_SP_BASE[FLAG_S | ((FLAG_S>>1) & FLAG_M)] = REG_SP;
	/* Set the S flag */
	FLAG_S = value;
	/* Set the new stack pointer */
	REG_SP = REG_SP_BASE[FLAG_S | ((FLAG_S>>1) & FLAG_M)];
}

/* Set the S and M flags and change the active stack pointer.
 * Note that value MUST be 0, 2, 4, or 6 (bit2 = S, bit1 = M).
 */
INLINE void m68ki_set_sm_flag(uint value)
{
	/* Backup the old stack pointer */
	REG_SP_BASE[FLAG_S | ((FLAG_S>>1) & FLAG_M)] = REG_SP;
	/* Set the S and M flags */
	FLAG_S = value & SFLAG_SET;
	FLAG_M = value & MFLAG_SET;
	/* Set the new stack pointer */
	REG_SP = REG_SP_BASE[FLAG_S | ((FLAG_S>>1) & FLAG_M)];
}

/* Set the S and M flags.  Don't touch the stack pointer. */
INLINE void m68ki_set_sm_flag_nosp(uint value)
{
	/* Set the S and M flags */
	FLAG_S = value & SFLAG_SET;
	FLAG_M = value & MFLAG_SET;
}


/* Set the condition code register */
INLINE void m68ki_set_ccr(uint value)
{
	FLAG_X = BIT_4(value)  << 4;
	FLAG_N = BIT_3(value)  << 4;
	FLAG_Z = !BIT_2(value);
	FLAG_V = BIT_1(value)  << 6;
	FLAG_C = BIT_0(value)  << 8;
}

/* Set the status register but don't check for interrupts */
INLINE void m68ki_set_sr_noint(uint value)
{
	/* Mask out the "unimplemented" bits */
	value &= CPU_SR_MASK;

	/* Now set the status register */
	FLAG_T1 = BIT_F(value);
	FLAG_T0 = BIT_E(value);
	FLAG_INT_MASK = value & 0x0700;
	m68ki_set_ccr(value);
	m68ki_set_sm_flag((value >> 11) & 6);
}

/* Set the status register but don't check for interrupts nor
 * change the stack pointer
 */
INLINE void m68ki_set_sr_noint_nosp(uint value)
{
	/* Mask out the "unimplemented" bits */
	value &= CPU_SR_MASK;

	/* Now set the status register */
	FLAG_T1 = BIT_F(value);
	FLAG_T0 = BIT_E(value);
	FLAG_INT_MASK = value & 0x0700;
	m68ki_set_ccr(value);
	m68ki_set_sm_flag_nosp((value >> 11) & 6);
}

/* Set the status register and check for interrupts */
INLINE void m68ki_set_sr(uint value)
{
	m68ki_set_sr_noint(value);
	m68ki_check_interrupts();
}


/* ------------------------- Exception Processing ------------------------- */

/* Initiate exception processing */
INLINE uint m68ki_init_exception(void)
{
	/* Save the old status register */
	uint sr = m68ki_get_sr();

	/* Turn off trace flag, clear pending traces */
	FLAG_T1 = FLAG_T0 = 0;
	m68ki_clear_trace();
	/* Enter supervisor mode */
	m68ki_set_s_flag(SFLAG_SET);

	return sr;
}

/* 3 word stack frame (68000 only) */
INLINE void m68ki_stack_frame_3word(uint pc, uint sr)
{
	m68ki_push_32(pc);
	m68ki_push_16(sr);
}

/* Format 0 stack frame.
 * This is the standard stack frame for 68010+.
 */
INLINE void m68ki_stack_frame_0000(uint pc, uint sr, uint vector)
{
	/* Stack a 3-word frame if we are 68000 */
	if(CPU_TYPE == CPU_TYPE_000)
	{
		m68ki_stack_frame_3word(pc, sr);
		return;
	}
	m68ki_push_16(vector<<2);
	m68ki_push_32(pc);
	m68ki_push_16(sr);
}

/* Format 1 stack frame (68020).
 * For 68020, this is the 4 word throwaway frame.
 */
INLINE void m68ki_stack_frame_0001(uint pc, uint sr, uint vector)
{
	m68ki_push_16(0x1000 | (vector<<2));
	m68ki_push_32(pc);
	m68ki_push_16(sr);
}

/* Format 2 stack frame.
 * This is used only by 68020 for trap exceptions.
 */
INLINE void m68ki_stack_frame_0010(uint sr, uint vector)
{
	m68ki_push_32(REG_PPC);
	m68ki_push_16(0x2000 | (vector<<2));
	m68ki_push_32(REG_PC);
	m68ki_push_16(sr);
}


/* Bus error stack frame (68000 only).
 */
INLINE void m68ki_stack_frame_buserr(uint sr)
{
	m68ki_push_32(REG_PC);
	m68ki_push_16(sr);
	m68ki_push_16(REG_IR);
	m68ki_push_32(m68ki_aerr_address);	/* access address */
	/* 0 0 0 0 0 0 0 0 0 0 0 R/W I/N FC
	 * R/W  0 = write, 1 = read
	 * I/N  0 = instruction, 1 = not
	 * FC   3-bit function code
	 */
	m68ki_push_16(m68ki_aerr_write_mode | CPU_INSTR_MODE | m68ki_aerr_fc);
}

/* Format 8 stack frame (68010).
 * 68010 only.  This is the 29 word bus/address error frame.
 */
void m68ki_stack_frame_1000(uint pc, uint sr, uint vector)
{
	/* VERSION
	 * NUMBER
	 * INTERNAL INFORMATION, 16 WORDS
	 */
	m68ki_fake_push_32();
	m68ki_fake_push_32();
	m68ki_fake_push_32();
	m68ki_fake_push_32();
	m68ki_fake_push_32();
	m68ki_fake_push_32();
	m68ki_fake_push_32();
	m68ki_fake_push_32();

	/* INSTRUCTION INPUT BUFFER */
	m68ki_push_16(0);

	/* UNUSED, RESERVED (not written) */
	m68ki_fake_push_16();

	/* DATA INPUT BUFFER */
	m68ki_push_16(0);

	/* UNUSED, RESERVED (not written) */
	m68ki_fake_push_16();

	/* DATA OUTPUT BUFFER */
	m68ki_push_16(0);

	/* UNUSED, RESERVED (not written) */
	m68ki_fake_push_16();

	/* FAULT ADDRESS */
	m68ki_push_32(0);

	/* SPECIAL STATUS WORD */
	m68ki_push_16(0);

	/* 1000, VECTOR OFFSET */
	m68ki_push_16(0x8000 | (vector<<2));

	/* PROGRAM COUNTER */
	m68ki_push_32(pc);

	/* STATUS REGISTER */
	m68ki_push_16(sr);
}

/* Format A stack frame (short bus fault).
 * This is used only by 68020 for bus fault and address error
 * if the error happens at an instruction boundary.
 * PC stacked is address of next instruction.
 */
void m68ki_stack_frame_1010(uint sr, uint vector, uint pc)
{
	/* INTERNAL REGISTER */
	m68ki_push_16(0);

	/* INTERNAL REGISTER */
	m68ki_push_16(0);

	/* DATA OUTPUT BUFFER (2 words) */
	m68ki_push_32(0);

	/* INTERNAL REGISTER */
	m68ki_push_16(0);

	/* INTERNAL REGISTER */
	m68ki_push_16(0);

	/* DATA CYCLE FAULT ADDRESS (2 words) */
	m68ki_push_32(0);

	/* INSTRUCTION PIPE STAGE B */
	m68ki_push_16(0);

	/* INSTRUCTION PIPE STAGE C */
	m68ki_push_16(0);

	/* SPECIAL STATUS REGISTER */
	m68ki_push_16(0);

	/* INTERNAL REGISTER */
	m68ki_push_16(0);

	/* 1010, VECTOR OFFSET */
	m68ki_push_16(0xa000 | (vector<<2));

	/* PROGRAM COUNTER */
	m68ki_push_32(pc);

	/* STATUS REGISTER */
	m68ki_push_16(sr);
}

/* Format B stack frame (long bus fault).
 * This is used only by 68020 for bus fault and address error
 * if the error happens during instruction execution.
 * PC stacked is address of instruction in progress.
 */
void m68ki_stack_frame_1011(uint sr, uint vector, uint pc)
{
	/* INTERNAL REGISTERS (18 words) */
	m68ki_push_32(0);
	m68ki_push_32(0);
	m68ki_push_32(0);
	m68ki_push_32(0);
	m68ki_push_32(0);
	m68ki_push_32(0);
	m68ki_push_32(0);
	m68ki_push_32(0);
	m68ki_push_32(0);

	/* VERSION# (4 bits), INTERNAL INFORMATION */
	m68ki_push_16(0);

	/* INTERNAL REGISTERS (3 words) */
	m68ki_push_32(0);
	m68ki_push_16(0);

	/* DATA INTPUT BUFFER (2 words) */
	m68ki_push_32(0);

	/* INTERNAL REGISTERS (2 words) */
	m68ki_push_32(0);

	/* STAGE B ADDRESS (2 words) */
	m68ki_push_32(0);

	/* INTERNAL REGISTER (4 words) */
	m68ki_push_32(0);
	m68ki_push_32(0);

	/* DATA OUTPUT BUFFER (2 words) */
	m68ki_push_32(0);

	/* INTERNAL REGISTER */
	m68ki_push_16(0);

	/* INTERNAL REGISTER */
	m68ki_push_16(0);

	/* DATA CYCLE FAULT ADDRESS (2 words) */
	m68ki_push_32(0);

	/* INSTRUCTION PIPE STAGE B */
	m68ki_push_16(0);

	/* INSTRUCTION PIPE STAGE C */
	m68ki_push_16(0);

	/* SPECIAL STATUS REGISTER */
	m68ki_push_16(0);

	/* INTERNAL REGISTER */
	m68ki_push_16(0);

	/* 1011, VECTOR OFFSET */
	m68ki_push_16(0xb000 | (vector<<2));

	/* PROGRAM COUNTER */
	m68ki_push_32(pc);

	/* STATUS REGISTER */
	m68ki_push_16(sr);
}


/* Used for Group 2 exceptions.
 * These stack a type 2 frame on the 020.
 */
INLINE void m68ki_exception_trap(uint vector)
{
	uint sr = m68ki_init_exception();

	if(CPU_TYPE_IS_010_LESS(CPU_TYPE))
		m68ki_stack_frame_0000(REG_PC, sr, vector);
	else
		m68ki_stack_frame_0010(sr, vector);

	m68ki_jump_vector(vector);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[vector] - CYC_INSTRUCTION[REG_IR]);
}

/* Trap#n stacks a 0 frame but behaves like group2 otherwise */
INLINE void m68ki_exception_trapN(uint vector)
{
	uint sr = m68ki_init_exception();
	m68ki_stack_frame_0000(REG_PC, sr, vector);
	m68ki_jump_vector(vector);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[vector] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for trace mode */
INLINE void m68ki_exception_trace(void)
{
	uint sr = m68ki_init_exception();

	if(CPU_TYPE_IS_010_LESS(CPU_TYPE))
	{
		#if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
		if(CPU_TYPE_IS_000(CPU_TYPE))
		{
			CPU_INSTR_MODE = INSTRUCTION_NO;
		}
		#endif /* M68K_EMULATE_ADDRESS_ERROR */
		m68ki_stack_frame_0000(REG_PC, sr, EXCEPTION_TRACE);
	}
	else
		m68ki_stack_frame_0010(sr, EXCEPTION_TRACE);

	m68ki_jump_vector(EXCEPTION_TRACE);

	/* Trace nullifies a STOP instruction */
	CPU_STOPPED &= ~STOP_LEVEL_STOP;

	/* Use up some clock cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_TRACE]);
}

/* Exception for privilege violation */
INLINE void m68ki_exception_privilege_violation(void)
{
	uint sr = m68ki_init_exception();

	#if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
	if(CPU_TYPE_IS_000(CPU_TYPE))
	{
		CPU_INSTR_MODE = INSTRUCTION_NO;
	}
	#endif /* M68K_EMULATE_ADDRESS_ERROR */

	m68ki_stack_frame_0000(REG_PPC, sr, EXCEPTION_PRIVILEGE_VIOLATION);
	m68ki_jump_vector(EXCEPTION_PRIVILEGE_VIOLATION);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_PRIVILEGE_VIOLATION] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for A-Line instructions */
INLINE void m68ki_exception_1010(void)
{
	uint sr;
#if M68K_LOG_1010_1111 == OPT_ON
	M68K_DO_LOG_EMU((M68K_LOG_FILEHANDLE "%s at %08x: called 1010 instruction %04x (%s)\n",
					 m68ki_cpu_names[CPU_TYPE], ADDRESS_68K(REG_PPC), REG_IR,
					 m68ki_disassemble_quick(ADDRESS_68K(REG_PPC))));
#endif

	sr = m68ki_init_exception();
	m68ki_stack_frame_0000(REG_PPC, sr, EXCEPTION_1010);
	m68ki_jump_vector(EXCEPTION_1010);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_1010] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for F-Line instructions */
INLINE void m68ki_exception_1111(void)
{
	uint sr;

#if M68K_LOG_1010_1111 == OPT_ON
	M68K_DO_LOG_EMU((M68K_LOG_FILEHANDLE "%s at %08x: called 1111 instruction %04x (%s)\n",
					 m68ki_cpu_names[CPU_TYPE], ADDRESS_68K(REG_PPC), REG_IR,
					 m68ki_disassemble_quick(ADDRESS_68K(REG_PPC))));
#endif

	sr = m68ki_init_exception();
	m68ki_stack_frame_0000(REG_PPC, sr, EXCEPTION_1111);
	m68ki_jump_vector(EXCEPTION_1111);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_1111] - CYC_INSTRUCTION[REG_IR]);
}

#if M68K_ILLG_HAS_CALLBACK == OPT_SPECIFY_HANDLER
extern int m68ki_illg_callback(int);
#endif

/* Exception for illegal instructions */
INLINE void m68ki_exception_illegal(void)
{
	uint sr;

	M68K_DO_LOG((M68K_LOG_FILEHANDLE "%s at %08x: illegal instruction %04x (%s)\n",
				 m68ki_cpu_names[CPU_TYPE], ADDRESS_68K(REG_PPC), REG_IR,
				 m68ki_disassemble_quick(ADDRESS_68K(REG_PPC))));
	if (m68ki_illg_callback(REG_IR))
	    return;

	sr = m68ki_init_exception();

	#if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
	if(CPU_TYPE_IS_000(CPU_TYPE))
	{
		CPU_INSTR_MODE = INSTRUCTION_NO;
	}
	#endif /* M68K_EMULATE_ADDRESS_ERROR */

	m68ki_stack_frame_0000(REG_PPC, sr, EXCEPTION_ILLEGAL_INSTRUCTION);
	m68ki_jump_vector(EXCEPTION_ILLEGAL_INSTRUCTION);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_ILLEGAL_INSTRUCTION] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for format errror in RTE */
INLINE void m68ki_exception_format_error(void)
{
	uint sr = m68ki_init_exception();
	m68ki_stack_frame_0000(REG_PC, sr, EXCEPTION_FORMAT_ERROR);
	m68ki_jump_vector(EXCEPTION_FORMAT_ERROR);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_FORMAT_ERROR] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for address error */
INLINE void m68ki_exception_address_error(void)
{
	uint sr = m68ki_init_exception();

	/* If we were processing a bus error, address error, or reset,
	 * this is a catastrophic failure.
	 * Halt the CPU
	 */
	if(CPU_RUN_MODE == RUN_MODE_BERR_AERR_RESET)
	{
m68k_read_memory_8(0x00ffff01);
		CPU_STOPPED = STOP_LEVEL_HALT;
		return;
	}
	CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET;

	/* Note: This is implemented for 68000 only! */
	m68ki_stack_frame_buserr(sr);

	m68ki_jump_vector(EXCEPTION_ADDRESS_ERROR);

	/* Use up some clock cycles. Note that we don't need to undo the
	instruction's cycles here as we've longjmp:ed directly from the
	instruction handler without passing the part of the excecute loop
	that deducts instruction cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_ADDRESS_ERROR]);
}


/* Service an interrupt request and start exception processing */
void m68ki_exception_interrupt(uint int_level)
{
	uint vector;
	uint sr;
	uint new_pc;

	#if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
	if(CPU_TYPE_IS_000(CPU_TYPE))
	{
		CPU_INSTR_MODE = INSTRUCTION_NO;
	}
	#endif /* M68K_EMULATE_ADDRESS_ERROR */

	/* Turn off the stopped state */
	CPU_STOPPED &= ~STOP_LEVEL_STOP;

	/* If we are halted, don't do anything */
	if(CPU_STOPPED)
		return;

	/* Acknowledge the interrupt */
	vector = m68ki_int_ack(int_level);

	/* Get the interrupt vector */
	if(vector == M68K_INT_ACK_AUTOVECTOR)
		/* Use the autovectors.  This is the most commonly used implementation */
		vector = EXCEPTION_INTERRUPT_AUTOVECTOR+int_level;
	else if(vector == M68K_INT_ACK_SPURIOUS)
		/* Called if no devices respond to the interrupt acknowledge */
		vector = EXCEPTION_SPURIOUS_INTERRUPT;
	else if(vector > 255)
	{
		M68K_DO_LOG_EMU((M68K_LOG_FILEHANDLE "%s at %08x: Interrupt acknowledge returned invalid vector $%x\n",
				 m68ki_cpu_names[CPU_TYPE], ADDRESS_68K(REG_PC), vector));
		return;
	}

	/* Start exception processing */
	sr = m68ki_init_exception();

	/* Set the interrupt mask to the level of the one being serviced */
	FLAG_INT_MASK = int_level<<8;

	/* Get the new PC */
	new_pc = m68ki_read_data_32((vector<<2) + REG_VBR);

	/* If vector is uninitialized, call the uninitialized interrupt vector */
	if(new_pc == 0)
		new_pc = m68ki_read_data_32((EXCEPTION_UNINITIALIZED_INTERRUPT<<2) + REG_VBR);

	/* Generate a stack frame */
	m68ki_stack_frame_0000(REG_PC, sr, vector);
	if(FLAG_M && CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		/* Create throwaway frame */
		m68ki_set_sm_flag(FLAG_S);	/* clear M */
		sr |= 0x2000; /* Same as SR in master stack frame except S is forced high */
		m68ki_stack_frame_0001(REG_PC, sr, vector);
	}

	m68ki_jump(new_pc);

	/* Defer cycle counting until later */
	USE_CYCLES(CYC_EXCEPTION[vector]);

#if !M68K_EMULATE_INT_ACK
	/* Automatically clear IRQ if we are not using an acknowledge scheme */
	CPU_INT_LEVEL = 0;
#endif /* M68K_EMULATE_INT_ACK */
}


/* ASG: Check for interrupts */
INLINE void m68ki_check_interrupts(void)
{
	if(m68ki_cpu.nmi_pending)
	{
		m68ki_cpu.nmi_pending = FALSE;
		m68ki_exception_interrupt(7);
	}
	else if(CPU_INT_LEVEL > FLAG_INT_MASK)
		m68ki_exception_interrupt(CPU_INT_LEVEL>>8);
}


/* ======================================================================== */
/* =============================== CALLBACKS ============================== */
/* ======================================================================== */

/* Default callbacks used if the callback hasn't been set yet, or if the
 * callback is set to NULL
 */

/* Interrupt acknowledge */
static int default_int_ack_callback_data;
static int default_int_ack_callback(int int_level)
{
	default_int_ack_callback_data = int_level;
	CPU_INT_LEVEL = 0;
	return M68K_INT_ACK_AUTOVECTOR;
}

/* Breakpoint acknowledge */
static unsigned int default_bkpt_ack_callback_data;
static void default_bkpt_ack_callback(unsigned int data)
{
	default_bkpt_ack_callback_data = data;
}

/* Called when a reset instruction is executed */
static void default_reset_instr_callback(void)
{
}

/* Called when a cmpi.l #v, dn instruction is executed */
static void default_cmpild_instr_callback(unsigned int val, int reg)
{
}

/* Called when a rte instruction is executed */
static void default_rte_instr_callback(void)
{
}

/* Called when a tas instruction is executed */
static int default_tas_instr_callback(void)
{
	return 1; // allow writeback
}

/* Called when an illegal instruction is encountered */
static int default_illg_instr_callback(int opcode)
{
	return 0; // not handled : exception will occur
}

/* Called when the program counter changed by a large value */
static unsigned int default_pc_changed_callback_data;
static void default_pc_changed_callback(unsigned int new_pc)
{
	default_pc_changed_callback_data = new_pc;
}

/* Called every time there's bus activity (read/write to/from memory */
static unsigned int default_set_fc_callback_data;
static void default_set_fc_callback(unsigned int new_fc)
{
	default_set_fc_callback_data = new_fc;
}

/* Called every instruction cycle prior to execution */
static void default_instr_hook_callback(unsigned int pc)
{
}


#if M68K_EMULATE_ADDRESS_ERROR
	#include <setjmp.h>
	jmp_buf m68ki_aerr_trap;
#endif /* M68K_EMULATE_ADDRESS_ERROR */


/* ======================================================================== */
/* ================================= API ================================== */
/* ======================================================================== */

/* Access the internals of the CPU */
unsigned int m68k_get_reg(void* context, m68k_register_t regnum)
{
	m68ki_cpu_core* cpu = context != NULL ?(m68ki_cpu_core*)context : &m68ki_cpu;

	switch(regnum)
	{
		case M68K_REG_D0:	return cpu->dar[0];
		case M68K_REG_D1:	return cpu->dar[1];
		case M68K_REG_D2:	return cpu->dar[2];
		case M68K_REG_D3:	return cpu->dar[3];
		case M68K_REG_D4:	return cpu->dar[4];
		case M68K_REG_D5:	return cpu->dar[5];
		case M68K_REG_D6:	return cpu->dar[6];
		case M68K_REG_D7:	return cpu->dar[7];
		case M68K_REG_A0:	return cpu->dar[8];
		case M68K_REG_A1:	return cpu->dar[9];
		case M68K_REG_A2:	return cpu->dar[10];
		case M68K_REG_A3:	return cpu->dar[11];
		case M68K_REG_A4:	return cpu->dar[12];
		case M68K_REG_A5:	return cpu->dar[13];
		case M68K_REG_A6:	return cpu->dar[14];
		case M68K_REG_A7:	return cpu->dar[15];
		case M68K_REG_PC:	return MASK_OUT_ABOVE_32(cpu->pc);
		case M68K_REG_SR:	return	cpu->t1_flag						|
									cpu->t0_flag						|
									(cpu->s_flag << 11)					|
									(cpu->m_flag << 11)					|
									cpu->int_mask						|
									((cpu->x_flag & XFLAG_SET) >> 4)	|
									((cpu->n_flag & NFLAG_SET) >> 4)	|
									((!cpu->not_z_flag) << 2)			|
									((cpu->v_flag & VFLAG_SET) >> 6)	|
									((cpu->c_flag & CFLAG_SET) >> 8);
		case M68K_REG_SP:	return cpu->dar[15];
		case M68K_REG_USP:	return cpu->s_flag ? cpu->sp[0] : cpu->dar[15];
		case M68K_REG_ISP:	return cpu->s_flag && !cpu->m_flag ? cpu->dar[15] : cpu->sp[4];
		case M68K_REG_MSP:	return cpu->s_flag && cpu->m_flag ? cpu->dar[15] : cpu->sp[6];
		case M68K_REG_SFC:	return cpu->sfc;
		case M68K_REG_DFC:	return cpu->dfc;
		case M68K_REG_VBR:	return cpu->vbr;
		case M68K_REG_CACR:	return cpu->cacr;
		case M68K_REG_CAAR:	return cpu->caar;
		case M68K_REG_PREF_ADDR:	return cpu->pref_addr;
		case M68K_REG_PREF_DATA:	return cpu->pref_data;
		case M68K_REG_PPC:	return MASK_OUT_ABOVE_32(cpu->ppc);
		case M68K_REG_IR:	return cpu->ir;
		case M68K_REG_CPU_TYPE:
			switch(cpu->cpu_type)
			{
				case CPU_TYPE_000:		return (unsigned int)M68K_CPU_TYPE_68000;
				case CPU_TYPE_010:		return (unsigned int)M68K_CPU_TYPE_68010;
				case CPU_TYPE_EC020:	return (unsigned int)M68K_CPU_TYPE_68EC020;
				case CPU_TYPE_020:		return (unsigned int)M68K_CPU_TYPE_68020;
				case CPU_TYPE_040:		return (unsigned int)M68K_CPU_TYPE_68040;
			}
			return M68K_CPU_TYPE_INVALID;
		default:			return 0;
	}
	return 0;
}

void m68k_set_reg(m68k_register_t regnum, unsigned int value)
{
	switch(regnum)
	{
		case M68K_REG_D0:	REG_D[0] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D1:	REG_D[1] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D2:	REG_D[2] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D3:	REG_D[3] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D4:	REG_D[4] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D5:	REG_D[5] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D6:	REG_D[6] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D7:	REG_D[7] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A0:	REG_A[0] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A1:	REG_A[1] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A2:	REG_A[2] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A3:	REG_A[3] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A4:	REG_A[4] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A5:	REG_A[5] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A6:	REG_A[6] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A7:	REG_A[7] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_PC:	m68ki_jump(MASK_OUT_ABOVE_32(value)); return;
		case M68K_REG_SR:	m68ki_set_sr_noint_nosp(value); return;
		case M68K_REG_SP:	REG_SP = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_USP:	if(FLAG_S)
								REG_USP = MASK_OUT_ABOVE_32(value);
							else
								REG_SP = MASK_OUT_ABOVE_32(value);
							return;
		case M68K_REG_ISP:	if(FLAG_S && !FLAG_M)
								REG_SP = MASK_OUT_ABOVE_32(value);
							else
								REG_ISP = MASK_OUT_ABOVE_32(value);
							return;
		case M68K_REG_MSP:	if(FLAG_S && FLAG_M)
								REG_SP = MASK_OUT_ABOVE_32(value);
							else
								REG_MSP = MASK_OUT_ABOVE_32(value);
							return;
		case M68K_REG_VBR:	REG_VBR = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_SFC:	REG_SFC = value & 7; return;
		case M68K_REG_DFC:	REG_DFC = value & 7; return;
		case M68K_REG_CACR:	REG_CACR = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_CAAR:	REG_CAAR = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_PPC:	REG_PPC = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_IR:	REG_IR = MASK_OUT_ABOVE_16(value); return;
		case M68K_REG_CPU_TYPE: m68k_set_cpu_type(value); return;
		default:			return;
	}
}

/* Set the callbacks */
void m68k_set_int_ack_callback(int  (*callback)(int int_level))
{
	CALLBACK_INT_ACK = callback ? callback : default_int_ack_callback;
}

void m68k_set_bkpt_ack_callback(void  (*callback)(unsigned int data))
{
	CALLBACK_BKPT_ACK = callback ? callback : default_bkpt_ack_callback;
}

void m68k_set_reset_instr_callback(void  (*callback)(void))
{
	CALLBACK_RESET_INSTR = callback ? callback : default_reset_instr_callback;
}

void m68k_set_cmpild_instr_callback(void  (*callback)(unsigned int, int))
{
	CALLBACK_CMPILD_INSTR = callback ? callback : default_cmpild_instr_callback;
}

void m68k_set_rte_instr_callback(void  (*callback)(void))
{
	CALLBACK_RTE_INSTR = callback ? callback : default_rte_instr_callback;
}

void m68k_set_tas_instr_callback(int  (*callback)(void))
{
	CALLBACK_TAS_INSTR = callback ? callback : default_tas_instr_callback;
}

void m68k_set_illg_instr_callback(int  (*callback)(int))
{
	CALLBACK_ILLG_INSTR = callback ? callback : default_illg_instr_callback;
}

void m68k_set_pc_changed_callback(void  (*callback)(unsigned int new_pc))
{
	CALLBACK_PC_CHANGED = callback ? callback : default_pc_changed_callback;
}

void m68k_set_fc_callback(void  (*callback)(unsigned int new_fc))
{
	CALLBACK_SET_FC = callback ? callback : default_set_fc_callback;
}

void m68k_set_instr_hook_callback(void  (*callback)(unsigned int pc))
{
	CALLBACK_INSTR_HOOK = callback ? callback : default_instr_hook_callback;
}

/* Set the CPU type. */
void m68k_set_cpu_type(unsigned int cpu_type)
{
	switch(cpu_type)
	{
		case M68K_CPU_TYPE_68000:
			CPU_TYPE         = CPU_TYPE_000;
			CPU_ADDRESS_MASK = 0x00ffffff;
			CPU_SR_MASK      = 0xa71f; /* T1 -- S  -- -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			CYC_INSTRUCTION  = m68ki_cycles[0];
			CYC_EXCEPTION    = m68ki_exception_cycle_table[0];
			CYC_BCC_NOTAKE_B = -2;
			CYC_BCC_NOTAKE_W = 2;
			CYC_DBCC_F_NOEXP = -2;
			CYC_DBCC_F_EXP   = 2;
			CYC_SCC_R_TRUE   = 2;
			CYC_MOVEM_W      = 2;
			CYC_MOVEM_L      = 3;
			CYC_SHIFT        = 1;
			CYC_RESET        = 132;
			return;
		case M68K_CPU_TYPE_68010:
			CPU_TYPE         = CPU_TYPE_010;
			CPU_ADDRESS_MASK = 0x00ffffff;
			CPU_SR_MASK      = 0xa71f; /* T1 -- S  -- -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			CYC_INSTRUCTION  = m68ki_cycles[1];
			CYC_EXCEPTION    = m68ki_exception_cycle_table[1];
			CYC_BCC_NOTAKE_B = -4;
			CYC_BCC_NOTAKE_W = 0;
			CYC_DBCC_F_NOEXP = 0;
			CYC_DBCC_F_EXP   = 6;
			CYC_SCC_R_TRUE   = 0;
			CYC_MOVEM_W      = 2;
			CYC_MOVEM_L      = 3;
			CYC_SHIFT        = 1;
			CYC_RESET        = 130;
			return;
		case M68K_CPU_TYPE_68EC020:
			CPU_TYPE         = CPU_TYPE_EC020;
			CPU_ADDRESS_MASK = 0x00ffffff;
			CPU_SR_MASK      = 0xf71f; /* T1 T0 S  M  -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			CYC_INSTRUCTION  = m68ki_cycles[2];
			CYC_EXCEPTION    = m68ki_exception_cycle_table[2];
			CYC_BCC_NOTAKE_B = -2;
			CYC_BCC_NOTAKE_W = 0;
			CYC_DBCC_F_NOEXP = 0;
			CYC_DBCC_F_EXP   = 4;
			CYC_SCC_R_TRUE   = 0;
			CYC_MOVEM_W      = 2;
			CYC_MOVEM_L      = 2;
			CYC_SHIFT        = 0;
			CYC_RESET        = 518;
			return;
		case M68K_CPU_TYPE_68020:
			CPU_TYPE         = CPU_TYPE_020;
			CPU_ADDRESS_MASK = 0xffffffff;
			CPU_SR_MASK      = 0xf71f; /* T1 T0 S  M  -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			CYC_INSTRUCTION  = m68ki_cycles[2];
			CYC_EXCEPTION    = m68ki_exception_cycle_table[2];
			CYC_BCC_NOTAKE_B = -2;
			CYC_BCC_NOTAKE_W = 0;
			CYC_DBCC_F_NOEXP = 0;
			CYC_DBCC_F_EXP   = 4;
			CYC_SCC_R_TRUE   = 0;
			CYC_MOVEM_W      = 2;
			CYC_MOVEM_L      = 2;
			CYC_SHIFT        = 0;
			CYC_RESET        = 518;
			return;
		case M68K_CPU_TYPE_68040:		// TODO: these values are not correct
			CPU_TYPE         = CPU_TYPE_040;
			CPU_ADDRESS_MASK = 0xffffffff;
			CPU_SR_MASK      = 0xf71f; /* T1 T0 S  M  -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			CYC_INSTRUCTION  = m68ki_cycles[2];
			CYC_EXCEPTION    = m68ki_exception_cycle_table[2];
			CYC_BCC_NOTAKE_B = -2;
			CYC_BCC_NOTAKE_W = 0;
			CYC_DBCC_F_NOEXP = 0;
			CYC_DBCC_F_EXP   = 4;
			CYC_SCC_R_TRUE   = 0;
			CYC_MOVEM_W      = 2;
			CYC_MOVEM_L      = 2;
			CYC_SHIFT        = 0;
			CYC_RESET        = 518;
			return;
	}
}

/* Execute some instructions until we use up num_cycles clock cycles */
/* ASG: removed per-instruction interrupt checks */
int m68k_execute(int num_cycles)
{
	/* Set our pool of clock cycles available */
	SET_CYCLES(num_cycles);
	m68ki_initial_cycles = num_cycles;

	/* See if interrupts came in */
	m68ki_check_interrupts();

	/* Make sure we're not stopped */
	if(!CPU_STOPPED)
	{
		/* Return point if we had an address error */
		m68ki_set_address_error_trap(); /* auto-disable (see m68kcpu.h) */

		/* Main loop.  Keep going until we run out of clock cycles */
		do
		{
			/* Set tracing accodring to T1. (T0 is done inside instruction) */
			m68ki_trace_t1(); /* auto-disable (see m68kcpu.h) */

			/* Set the address space for reads */
			m68ki_use_data_space(); /* auto-disable (see m68kcpu.h) */

			/* Call external hook to peek at CPU */
			m68ki_instr_hook(REG_PC); /* auto-disable (see m68kcpu.h) */

			/* Record previous program counter */
			REG_PPC = REG_PC;

			/* Read an instruction and call its handler */
			REG_IR = m68ki_read_imm_16();
			m68ki_instruction_jump_table[REG_IR]();
			USE_CYCLES(CYC_INSTRUCTION[REG_IR]);

			/* Trace m68k_exception, if necessary */
			m68ki_exception_if_trace(); /* auto-disable (see m68kcpu.h) */
		} while(GET_CYCLES() > 0);

		/* set previous PC to current PC for the next entry into the loop */
		REG_PPC = REG_PC;
	}
	else
		SET_CYCLES(0);

	/* return how many clocks we used */
	return m68ki_initial_cycles - GET_CYCLES();
}


int m68k_cycles_run(void)
{
	return m68ki_initial_cycles - GET_CYCLES();
}

int m68k_cycles_remaining(void)
{
	return GET_CYCLES();
}

/* Change the timeslice */
void m68k_modify_timeslice(int cycles)
{
	m68ki_initial_cycles += cycles;
	ADD_CYCLES(cycles);
}


void m68k_end_timeslice(void)
{
	m68ki_initial_cycles = GET_CYCLES();
	SET_CYCLES(0);
}


/* ASG: rewrote so that the int_level is a mask of the IPL0/IPL1/IPL2 bits */
/* KS: Modified so that IPL* bits match with mask positions in the SR
 *     and cleaned out remenants of the interrupt controller.
 */
void m68k_set_irq(unsigned int int_level)
{
	uint old_level = CPU_INT_LEVEL;
	CPU_INT_LEVEL = int_level << 8;

	/* A transition from < 7 to 7 always interrupts (NMI) */
	/* Note: Level 7 can also level trigger like a normal IRQ */
	if(old_level != 0x0700 && CPU_INT_LEVEL == 0x0700)
		m68ki_cpu.nmi_pending = TRUE;
}

void m68k_set_virq(unsigned int level, unsigned int active)
{
	uint state = m68ki_cpu.virq_state;
	uint blevel;

	if(active)
		state |= 1 << level;
	else
		state &= ~(1 << level);
	m68ki_cpu.virq_state = state;

	for(blevel = 7; blevel > 0; blevel--)
		if(state & (1 << blevel))
			break;
	m68k_set_irq(blevel);
}

unsigned int m68k_get_virq(unsigned int level)
{
	return (m68ki_cpu.virq_state & (1 << level)) ? 1 : 0;
}

void m68k_init(void)
{
	static uint emulation_initialized = 0;

	/* The first call to this function initializes the opcode handler jump table */
	if(!emulation_initialized)
		{
		m68ki_build_opcode_table();
		emulation_initialized = 1;
	}

	m68k_set_int_ack_callback(NULL);
	m68k_set_bkpt_ack_callback(NULL);
	m68k_set_reset_instr_callback(NULL);
	m68k_set_cmpild_instr_callback(NULL);
	m68k_set_rte_instr_callback(NULL);
	m68k_set_tas_instr_callback(NULL);
	m68k_set_illg_instr_callback(NULL);
	m68k_set_pc_changed_callback(NULL);
	m68k_set_fc_callback(NULL);
	m68k_set_instr_hook_callback(NULL);
}

/* Pulse the RESET line on the CPU */
void m68k_pulse_reset(void)
{
	/* Clear all stop levels and eat up all remaining cycles */
	CPU_STOPPED = 0;
	SET_CYCLES(0);

	CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET;
	CPU_INSTR_MODE = INSTRUCTION_YES;

	/* Turn off tracing */
	FLAG_T1 = FLAG_T0 = 0;
	m68ki_clear_trace();
	/* Interrupt mask to level 7 */
	FLAG_INT_MASK = 0x0700;
	CPU_INT_LEVEL = 0;
	m68ki_cpu.virq_state = 0;
	/* Reset VBR */
	REG_VBR = 0;
	/* Go to supervisor mode */
	m68ki_set_sm_flag(SFLAG_SET | MFLAG_CLEAR);

	/* Invalidate the prefetch queue */
#if M68K_EMULATE_PREFETCH
	/* Set to arbitrary number since our first fetch is from 0 */
	CPU_PREF_ADDR = 0x1000;
#endif /* M68K_EMULATE_PREFETCH */

	/* Read the initial stack pointer and program counter */
	m68ki_jump(0);
	REG_SP = m68ki_read_imm_32();
	REG_PC = m68ki_read_imm_32();
	m68ki_jump(REG_PC);

	CPU_RUN_MODE = RUN_MODE_NORMAL;
}

/* Pulse the HALT line on the CPU */
void m68k_pulse_halt(void)
{
	CPU_STOPPED |= STOP_LEVEL_HALT;
}


/* Get and set the current CPU context */
/* This is to allow for multiple CPUs */
unsigned int m68k_context_size()
{
	return sizeof(m68ki_cpu_core);
}

unsigned int m68k_get_context(void* dst)
{
	if(dst) *(m68ki_cpu_core*)dst = m68ki_cpu;
	return sizeof(m68ki_cpu_core);
}

void m68k_set_context(void* src)
{
	if(src) m68ki_cpu = *(m68ki_cpu_core*)src;
}



/* ======================================================================== */
/* ============================== MAME STUFF ============================== */
/* ======================================================================== */

#if M68K_COMPILE_FOR_MAME == OPT_ON

static struct {
	UINT16 sr;
	UINT8 stopped;
	UINT8 halted;
} m68k_substate;

static void m68k_prepare_substate(void)
{
	m68k_substate.sr = m68ki_get_sr();
	m68k_substate.stopped = (CPU_STOPPED & STOP_LEVEL_STOP) != 0;
	m68k_substate.halted  = (CPU_STOPPED & STOP_LEVEL_HALT) != 0;
}

static void m68k_post_load(void)
{
	m68ki_set_sr_noint_nosp(m68k_substate.sr);
	CPU_STOPPED = m68k_substate.stopped ? STOP_LEVEL_STOP : 0
		        | m68k_substate.halted  ? STOP_LEVEL_HALT : 0;
	m68ki_jump(REG_PC);
}

void m68k_state_register(const char *type, int index)
{
	/* Note, D covers A because the dar array is common, REG_A=REG_D+8 */
	state_save_register_item_array(type, index, REG_D);
	state_save_register_item(type, index, REG_PPC);
	state_save_register_item(type, index, REG_PC);
	state_save_register_item(type, index, REG_USP);
	state_save_register_item(type, index, REG_ISP);
	state_save_register_item(type, index, REG_MSP);
	state_save_register_item(type, index, REG_VBR);
	state_save_register_item(type, index, REG_SFC);
	state_save_register_item(type, index, REG_DFC);
	state_save_register_item(type, index, REG_CACR);
	state_save_register_item(type, index, REG_CAAR);
	state_save_register_item(type, index, m68k_substate.sr);
	state_save_register_item(type, index, CPU_INT_LEVEL);
	state_save_register_item(type, index, m68k_substate.stopped);
	state_save_register_item(type, index, m68k_substate.halted);
	state_save_register_item(type, index, CPU_PREF_ADDR);
	state_save_register_item(type, index, CPU_PREF_DATA);
	state_save_register_func_presave(m68k_prepare_substate);
	state_save_register_func_postload(m68k_post_load);
}

#endif /* M68K_COMPILE_FOR_MAME */

/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */
