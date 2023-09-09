/* ======================================================================== */
/* ========================= LICENSING & COPYRIGHT ======================== */
/* ======================================================================== */
/*
 *                                  MUSASHI
 *                                Version 4.5
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




#ifndef M68KCPU__HEADER
#define M68KCPU__HEADER

#ifdef __cplusplus
extern "C" {
#endif

#include "m68k.h"
#include <limits.h>
#include <endian.h>
#include <stdio.h>
#include <stdbool.h>
#include "gpio/ps_protocol.h"

/* cryptodad Aug 2023 - CACHE_ON and T_CACHE_ON are slower if enabled */
//#define CACHE_ON // cryptodad
//#define T_CACHE_ON // cryptodad

/* cryptodad Aug 2023 - CHIP_FASTPATH is slower if enabled */
//#define CHIP_FASTPATH // cryptodad
#ifdef CHIP_FASTPATH
#include "config_file/config_file.h"
extern struct emulator_config *cfg;
#endif

/* ======================================================================== */
/* ==================== ARCHITECTURE-DEPENDANT DEFINES ==================== */
/* ======================================================================== */

/* Check for > 32bit sizes */
#if UINT_MAX > 0xffffffff
	#define M68K_INT_GT_32_BIT  1
#else
	#define M68K_INT_GT_32_BIT  0
#endif

/* Data types used in this emulation core */
#undef sint8
#undef sint16
#undef sint32
#undef sint64
#undef uint8
#undef uint16
#undef uint32
#undef uint64
#undef sint
#undef uint

typedef signed   char  sint8;  		/* ASG: changed from char to signed char */
typedef signed   short sint16;
typedef signed   int   sint32; 		/* AWJ: changed from long to int */
typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32; 			/* AWJ: changed from long to int */

/* signed and unsigned int must be at least 32 bits wide */
typedef signed   int sint;
typedef unsigned int uint;


#if M68K_USE_64_BIT
typedef signed   long long sint64;
typedef unsigned long long uint64;
#else
typedef sint32 sint64;
typedef uint32 uint64;
#endif /* M68K_USE_64_BIT */

/* U64 and S64 are used to wrap long integer constants. */
#ifdef __GNUC__
#define U64(val) val##ULL
#define S64(val) val##LL
#else
#define U64(val) val
#define S64(val) val
#endif

#include "softfloat/milieu.h"
#include "softfloat/softfloat.h"


/* Allow for architectures that don't have 8-bit sizes */
#if UCHAR_MAX == 0xff
	#define MAKE_INT_8(A) (sint8)(A)
#else
	#undef  sint8
	#define sint8  signed   int
	#undef  uint8
	#define uint8  unsigned int
	static inline sint MAKE_INT_8(uint value)
	{
		return (value & 0x80) ? value | ~0xff : value & 0xff;
	}
#endif /* UCHAR_MAX == 0xff */


/* Allow for architectures that don't have 16-bit sizes */
#if USHRT_MAX == 0xffff
	#define MAKE_INT_16(A) (sint16)(A)
#else
	#undef  sint16
	#define sint16 signed   int
	#undef  uint16
	#define uint16 unsigned int
	static inline sint MAKE_INT_16(uint value)
	{
		return (value & 0x8000) ? value | ~0xffff : value & 0xffff;
	}
#endif /* USHRT_MAX == 0xffff */


/* Allow for architectures that don't have 32-bit sizes */
#if UINT_MAX == 0xffffffff
	#define MAKE_INT_32(A) (sint32)(A)
#else
	#undef  sint32
	#define sint32  signed   int
	#undef  uint32
	#define uint32  unsigned int
	static inline sint MAKE_INT_32(uint value)
	{
		return (value & 0x80000000) ? value | ~0xffffffff : value & 0xffffffff;
	}
#endif /* UINT_MAX == 0xffffffff */

/* ======================================================================== */
/* ============================ GENERAL DEFINES =========================== */
/* ======================================================================== */

/* MMU constants */
#define MMU_ATC_ENTRIES 22    // 68851 has 64, 030 has 22

/* instruction cache constants */
#define M68K_IC_SIZE 128

/* Exception Vectors handled by emulation */
#define EXCEPTION_RESET                    0
#define EXCEPTION_BUS_ERROR                2 /* This one is not emulated! */
#define EXCEPTION_ADDRESS_ERROR            3 /* This one is partially emulated (doesn't stack a proper frame yet) */
#define EXCEPTION_ILLEGAL_INSTRUCTION      4
#define EXCEPTION_ZERO_DIVIDE              5
#define EXCEPTION_CHK                      6
#define EXCEPTION_TRAPV                    7
#define EXCEPTION_PRIVILEGE_VIOLATION      8
#define EXCEPTION_TRACE                    9
#define EXCEPTION_1010                    10
#define EXCEPTION_1111                    11
#define EXCEPTION_FORMAT_ERROR            14
#define EXCEPTION_UNINITIALIZED_INTERRUPT 15
#define EXCEPTION_SPURIOUS_INTERRUPT      24
#define EXCEPTION_INTERRUPT_AUTOVECTOR    24
#define EXCEPTION_TRAP_BASE               32
#define EXCEPTION_MMU_CONFIGURATION       56 // only on 020/030

/* Function codes set by CPU during data/address bus activity */
#define FUNCTION_CODE_USER_DATA          1
#define FUNCTION_CODE_USER_PROGRAM       2
#define FUNCTION_CODE_SUPERVISOR_DATA    5
#define FUNCTION_CODE_SUPERVISOR_PROGRAM 6
#define FUNCTION_CODE_CPU_SPACE          7

/* CPU types for deciding what to emulate */
#define CPU_TYPE_000    (0x00000001)
#define CPU_TYPE_008    (0x00000002)
#define CPU_TYPE_010    (0x00000004)
#define CPU_TYPE_EC020  (0x00000008)
#define CPU_TYPE_020    (0x00000010)
#define CPU_TYPE_EC030  (0x00000020)
#define CPU_TYPE_030    (0x00000040)
#define CPU_TYPE_EC040  (0x00000080)
#define CPU_TYPE_LC040  (0x00000100)
#define CPU_TYPE_040    (0x00000200)
#define CPU_TYPE_SCC070 (0x00000400)

/* Different ways to stop the CPU */
#define STOP_LEVEL_STOP 1
#define STOP_LEVEL_HALT 2

/* Used for 68000 address error processing */
#define INSTRUCTION_YES 0
#define INSTRUCTION_NO  0x08
#define MODE_READ       0x10
#define MODE_WRITE      0

#define RUN_MODE_NORMAL              0
#define RUN_MODE_BERR_AERR_RESET_WSF 1 // writing the stack frame
#define RUN_MODE_BERR_AERR_RESET     2 // stack frame done

#define M68K_CACR_IBE  0x10 // Instruction Burst Enable
#define M68K_CACR_CI   0x08 // Clear Instruction Cache
#define M68K_CACR_CEI  0x04 // Clear Entry in Instruction Cache
#define M68K_CACR_FI   0x02 // Freeze Instruction Cache
#define M68K_CACR_EI   0x01 // Enable Instruction Cache

#ifndef NULL
#define NULL ((void*)0)
#endif

/* ======================================================================== */
/* ================================ MACROS ================================ */
/* ======================================================================== */


/* ---------------------------- General Macros ---------------------------- */

/* Bit Isolation Macros */
#define BIT_0(A)  ((A) & 0x00000001)
#define BIT_1(A)  ((A) & 0x00000002)
#define BIT_2(A)  ((A) & 0x00000004)
#define BIT_3(A)  ((A) & 0x00000008)
#define BIT_4(A)  ((A) & 0x00000010)
#define BIT_5(A)  ((A) & 0x00000020)
#define BIT_6(A)  ((A) & 0x00000040)
#define BIT_7(A)  ((A) & 0x00000080)
#define BIT_8(A)  ((A) & 0x00000100)
#define BIT_9(A)  ((A) & 0x00000200)
#define BIT_A(A)  ((A) & 0x00000400)
#define BIT_B(A)  ((A) & 0x00000800)
#define BIT_C(A)  ((A) & 0x00001000)
#define BIT_D(A)  ((A) & 0x00002000)
#define BIT_E(A)  ((A) & 0x00004000)
#define BIT_F(A)  ((A) & 0x00008000)
#define BIT_10(A) ((A) & 0x00010000)
#define BIT_11(A) ((A) & 0x00020000)
#define BIT_12(A) ((A) & 0x00040000)
#define BIT_13(A) ((A) & 0x00080000)
#define BIT_14(A) ((A) & 0x00100000)
#define BIT_15(A) ((A) & 0x00200000)
#define BIT_16(A) ((A) & 0x00400000)
#define BIT_17(A) ((A) & 0x00800000)
#define BIT_18(A) ((A) & 0x01000000)
#define BIT_19(A) ((A) & 0x02000000)
#define BIT_1A(A) ((A) & 0x04000000)
#define BIT_1B(A) ((A) & 0x08000000)
#define BIT_1C(A) ((A) & 0x10000000)
#define BIT_1D(A) ((A) & 0x20000000)
#define BIT_1E(A) ((A) & 0x40000000)
#define BIT_1F(A) ((A) & 0x80000000)

/* Get the most significant bit for specific sizes */
#define GET_MSB_8(A)  ((A) & 0x80)
#define GET_MSB_9(A)  ((A) & 0x100)
#define GET_MSB_16(A) ((A) & 0x8000)
#define GET_MSB_17(A) ((A) & 0x10000)
#define GET_MSB_32(A) ((A) & 0x80000000)
#if M68K_USE_64_BIT
#define GET_MSB_33(A) ((A) & 0x100000000)
#endif /* M68K_USE_64_BIT */

/* Isolate nibbles */
#define LOW_NIBBLE(A)  ((A) & 0x0f)
#define HIGH_NIBBLE(A) ((A) & 0xf0)

/* These are used to isolate 8, 16, and 32 bit sizes */
#define MASK_OUT_ABOVE_2(A)  ((A) & 3)
#define MASK_OUT_ABOVE_8(A)  ((A) & 0xff)
#define MASK_OUT_ABOVE_16(A) ((A) & 0xffff)
#define MASK_OUT_BELOW_2(A)  ((A) & ~3)
#define MASK_OUT_BELOW_8(A)  ((A) & ~0xff)
#define MASK_OUT_BELOW_16(A) ((A) & ~0xffff)

/* No need to mask if we are 32 bit */
#if M68K_INT_GT_32_BIT || M68K_USE_64_BIT
	#define MASK_OUT_ABOVE_32(A) ((A) & 0xffffffff)
	#define MASK_OUT_BELOW_32(A) ((A) & ~0xffffffff)
#else
	#define MASK_OUT_ABOVE_32(A) (A)
	#define MASK_OUT_BELOW_32(A) 0
#endif /* M68K_INT_GT_32_BIT || M68K_USE_64_BIT */

/* Simulate address lines of 68k family */
#define ADDRESS_68K(A) ((A)&CPU_ADDRESS_MASK)
//#define ADDRESS_68K(A) ((A))

/* Shift & Rotate Macros. */
#define LSL(A, C) ((A) << (C))
#define LSR(A, C) ((A) >> (C))

/* Some > 32-bit optimizations */
#if M68K_INT_GT_32_BIT
	/* Shift left and right */
	#define LSR_32(A, C) ((A) >> (C))
	#define LSL_32(A, C) ((A) << (C))
#else
	/* We have to do this because the morons at ANSI decided that shifts
	 * by >= data size are undefined.
	 */
	#define LSR_32(A, C) ((C) < 32 ? (A) >> (C) : 0)
	#define LSL_32(A, C) ((C) < 32 ? (A) << (C) : 0)
#endif /* M68K_INT_GT_32_BIT */

#if M68K_USE_64_BIT
	#define LSL_32_64(A, C) ((A) << (C))
	#define LSR_32_64(A, C) ((A) >> (C))
	#define ROL_33_64(A, C) (LSL_32_64(A, C) | LSR_32_64(A, 33-(C)))
	#define ROR_33_64(A, C) (LSR_32_64(A, C) | LSL_32_64(A, 33-(C)))
#endif /* M68K_USE_64_BIT */

#define ROL_8(A, C)      MASK_OUT_ABOVE_8(LSL(A, C) | LSR(A, 8-(C)))
#define ROL_9(A, C)                      (LSL(A, C) | LSR(A, 9-(C)))
#define ROL_16(A, C)    MASK_OUT_ABOVE_16(LSL(A, C) | LSR(A, 16-(C)))
#define ROL_17(A, C)                     (LSL(A, C) | LSR(A, 17-(C)))
#define ROL_32(A, C)    MASK_OUT_ABOVE_32(LSL_32(A, C) | LSR_32(A, 32-(C)))
#define ROL_33(A, C)                     (LSL_32(A, C) | LSR_32(A, 33-(C)))

#define ROR_8(A, C)      MASK_OUT_ABOVE_8(LSR(A, C) | LSL(A, 8-(C)))
#define ROR_9(A, C)                      (LSR(A, C) | LSL(A, 9-(C)))
#define ROR_16(A, C)    MASK_OUT_ABOVE_16(LSR(A, C) | LSL(A, 16-(C)))
#define ROR_17(A, C)                     (LSR(A, C) | LSL(A, 17-(C)))
#define ROR_32(A, C)    MASK_OUT_ABOVE_32(LSR_32(A, C) | LSL_32(A, 32-(C)))
#define ROR_33(A, C)                     (LSR_32(A, C) | LSL_32(A, 33-(C)))



/* ------------------------------ CPU Access ------------------------------ */

/* Access the CPU registers */
#define CPU_TYPE         state->cpu_type

#define REG_DA           state->dar /* easy access to data and address regs */
#define REG_DA_SAVE      state->dar_save
#define REG_D            state->dar
#define REG_A            (state->dar+8)
#define REG_PPC          state->ppc
#define REG_PC           state->pc
#define REG_SP_BASE      state->sp
#define REG_USP          state->sp[0]
#define REG_ISP          state->sp[4]
#define REG_MSP          state->sp[6]
#define REG_SP           state->dar[15]
#define REG_VBR          state->vbr
#define REG_SFC          state->sfc
#define REG_DFC          state->dfc
#define REG_CACR         state->cacr
#define REG_CAAR         state->caar
#define REG_IR           state->ir

#define REG_FP           state->fpr
#define REG_FPCR         state->fpcr
#define REG_FPSR         state->fpsr
#define REG_FPIAR        state->fpiar

#define FLAG_T1          state->t1_flag
#define FLAG_T0          state->t0_flag
#define FLAG_S           state->s_flag
#define FLAG_M           state->m_flag
#define FLAG_X           state->x_flag
#define FLAG_N           state->n_flag
#define FLAG_Z           state->not_z_flag
#define FLAG_V           state->v_flag
#define FLAG_C           state->c_flag
#define FLAG_INT_MASK    state->int_mask

#define CPU_INT_LEVEL    m68ki_cpu.int_level /* ASG: changed from CPU_INTS_PENDING */
#define CPU_STOPPED      m68ki_cpu.stopped
#define CPU_PREF_ADDR    state->pref_addr
#define CPU_PREF_DATA    state->pref_data
#define CPU_ADDRESS_MASK state->address_mask
#define CPU_SR_MASK      state->sr_mask
#define CPU_INSTR_MODE   state->instr_mode
#define CPU_RUN_MODE     state->run_mode

#define CYC_INSTRUCTION  state->cyc_instruction
#define CYC_EXCEPTION    state->cyc_exception
#define CYC_BCC_NOTAKE_B state->cyc_bcc_notake_b
#define CYC_BCC_NOTAKE_W state->cyc_bcc_notake_w
#define CYC_DBCC_F_NOEXP state->cyc_dbcc_f_noexp
#define CYC_DBCC_F_EXP   state->cyc_dbcc_f_exp
#define CYC_SCC_R_TRUE   state->cyc_scc_r_true
#define CYC_MOVEM_W      state->cyc_movem_w
#define CYC_MOVEM_L      state->cyc_movem_l
#define CYC_SHIFT        state->cyc_shift
#define CYC_RESET        state->cyc_reset
#define HAS_PMMU         state->has_pmmu
#define HAS_FPU          state->has_fpu
#define PMMU_ENABLED     state->pmmu_enabled
#define RESET_CYCLES     state->reset_cycles


#define CALLBACK_INT_ACK      m68ki_cpu.int_ack_callback
#define CALLBACK_BKPT_ACK     m68ki_cpu.bkpt_ack_callback
#define CALLBACK_RESET_INSTR  m68ki_cpu.reset_instr_callback
#define CALLBACK_CMPILD_INSTR m68ki_cpu.cmpild_instr_callback
#define CALLBACK_RTE_INSTR    m68ki_cpu.rte_instr_callback
#define CALLBACK_TAS_INSTR    m68ki_cpu.tas_instr_callback
#define CALLBACK_ILLG_INSTR   m68ki_cpu.illg_instr_callback
#define CALLBACK_PC_CHANGED   m68ki_cpu.pc_changed_callback
#define CALLBACK_SET_FC       m68ki_cpu.set_fc_callback
#define CALLBACK_INSTR_HOOK   m68ki_cpu.instr_hook_callback



/* ----------------------------- Configuration ---------------------------- */

/* These defines are dependant on the configuration defines in m68kconf.h */

/* Disable certain comparisons if we're not using all CPU types */
#if M68K_EMULATE_040
#define CPU_TYPE_IS_040_PLUS(A)    ((A) & (CPU_TYPE_040 | CPU_TYPE_EC040 | CPU_TYPE_LC040))
	#define CPU_TYPE_IS_040_LESS(A)    1
#else
	#define CPU_TYPE_IS_040_PLUS(A)    0
	#define CPU_TYPE_IS_040_LESS(A)    1
#endif

#if M68K_EMULATE_030
#define CPU_TYPE_IS_030_PLUS(A)    ((A) & (CPU_TYPE_030 | CPU_TYPE_EC030 | CPU_TYPE_040 | CPU_TYPE_EC040 | CPU_TYPE_LC040))
#define CPU_TYPE_IS_030_LESS(A)    1
#else
#define CPU_TYPE_IS_030_PLUS(A)	0
#define CPU_TYPE_IS_030_LESS(A)    1
#endif

#if M68K_EMULATE_020
#define CPU_TYPE_IS_020_PLUS(A)    ((A) & (CPU_TYPE_020 | CPU_TYPE_030 | CPU_TYPE_EC030 | CPU_TYPE_040 | CPU_TYPE_EC040 | CPU_TYPE_LC040))
	#define CPU_TYPE_IS_020_LESS(A)    1
#else
	#define CPU_TYPE_IS_020_PLUS(A)    0
	#define CPU_TYPE_IS_020_LESS(A)    1
#endif

#if M68K_EMULATE_EC020
#define CPU_TYPE_IS_EC020_PLUS(A)  ((A) & (CPU_TYPE_EC020 | CPU_TYPE_020 | CPU_TYPE_030 | CPU_TYPE_EC030 | CPU_TYPE_040 | CPU_TYPE_EC040 | CPU_TYPE_LC040))
	#define CPU_TYPE_IS_EC020_LESS(A)  ((A) & (CPU_TYPE_000 | CPU_TYPE_010 | CPU_TYPE_EC020))
#else
	#define CPU_TYPE_IS_EC020_PLUS(A)  CPU_TYPE_IS_020_PLUS(A)
	#define CPU_TYPE_IS_EC020_LESS(A)  CPU_TYPE_IS_020_LESS(A)
#endif

#if M68K_EMULATE_010
	#define CPU_TYPE_IS_010(A)         ((A) == CPU_TYPE_010)
#define CPU_TYPE_IS_010_PLUS(A)    ((A) & (CPU_TYPE_010 | CPU_TYPE_EC020 | CPU_TYPE_020 | CPU_TYPE_EC030 | CPU_TYPE_030 | CPU_TYPE_040 | CPU_TYPE_EC040 | CPU_TYPE_LC040))
#define CPU_TYPE_IS_010_LESS(A)    ((A) & (CPU_TYPE_000 | CPU_TYPE_008 | CPU_TYPE_010))
#else
	#define CPU_TYPE_IS_010(A)         0
	#define CPU_TYPE_IS_010_PLUS(A)    CPU_TYPE_IS_EC020_PLUS(A)
	#define CPU_TYPE_IS_010_LESS(A)    CPU_TYPE_IS_EC020_LESS(A)
#endif

#if M68K_EMULATE_020 || M68K_EMULATE_EC020
	#define CPU_TYPE_IS_020_VARIANT(A) ((A) & (CPU_TYPE_EC020 | CPU_TYPE_020))
#else
	#define CPU_TYPE_IS_020_VARIANT(A) 0
#endif

#if M68K_EMULATE_040 || M68K_EMULATE_020 || M68K_EMULATE_EC020 || M68K_EMULATE_010
	#define CPU_TYPE_IS_000(A)         ((A) == CPU_TYPE_000)
#else
	#define CPU_TYPE_IS_000(A)         1
#endif


#if !M68K_SEPARATE_READS
#define m68k_read_immediate_16(state, A) m68ki_read_program_16(state, A)
#define m68k_read_immediate_32(state, A) m68ki_read_program_32(state, A)

#define m68k_read_pcrelative_8(state, A) m68ki_read_program_8(state, A)
#define m68k_read_pcrelative_16(state, A) m68ki_read_program_16(state, A)
#define m68k_read_pcrelative_32(state, A) m68ki_read_program_32(state, A)
#endif /* M68K_SEPARATE_READS */


/* Enable or disable callback functions */
#if M68K_EMULATE_INT_ACK
	#if M68K_EMULATE_INT_ACK == OPT_SPECIFY_HANDLER
		#define m68ki_int_ack(A) M68K_INT_ACK_CALLBACK(A)
	#else
		#define m68ki_int_ack(A) CALLBACK_INT_ACK(A)
	#endif
#else
	/* Default action is to used autovector mode, which is most common */
	#define m68ki_int_ack(A) M68K_INT_ACK_AUTOVECTOR
#endif /* M68K_EMULATE_INT_ACK */

#if M68K_EMULATE_BKPT_ACK
	#if M68K_EMULATE_BKPT_ACK == OPT_SPECIFY_HANDLER
		#define m68ki_bkpt_ack(A) M68K_BKPT_ACK_CALLBACK(A)
	#else
		#define m68ki_bkpt_ack(A) CALLBACK_BKPT_ACK(A)
	#endif
#else
	#define m68ki_bkpt_ack(A)
#endif /* M68K_EMULATE_BKPT_ACK */

#if M68K_EMULATE_RESET
	#if M68K_EMULATE_RESET == OPT_SPECIFY_HANDLER
		#define m68ki_output_reset() M68K_RESET_CALLBACK()
	#else
		#define m68ki_output_reset() CALLBACK_RESET_INSTR()
	#endif
#else
	#define m68ki_output_reset()
#endif /* M68K_EMULATE_RESET */

#if M68K_CMPILD_HAS_CALLBACK
	#if M68K_CMPILD_HAS_CALLBACK == OPT_SPECIFY_HANDLER
		#define m68ki_cmpild_callback(v,r) M68K_CMPILD_CALLBACK(v,r)
	#else
		#define m68ki_cmpild_callback(v,r) CALLBACK_CMPILD_INSTR(v,r)
	#endif
#else
	#define m68ki_cmpild_callback(v,r)
#endif /* M68K_CMPILD_HAS_CALLBACK */

#if M68K_RTE_HAS_CALLBACK
	#if M68K_RTE_HAS_CALLBACK == OPT_SPECIFY_HANDLER
		#define m68ki_rte_callback() M68K_RTE_CALLBACK()
	#else
		#define m68ki_rte_callback() CALLBACK_RTE_INSTR()
	#endif
#else
	#define m68ki_rte_callback()
#endif /* M68K_RTE_HAS_CALLBACK */

#if M68K_TAS_HAS_CALLBACK
	#if M68K_TAS_HAS_CALLBACK == OPT_SPECIFY_HANDLER
		#define m68ki_tas_callback() M68K_TAS_CALLBACK()
	#else
		#define m68ki_tas_callback() CALLBACK_TAS_INSTR()
	#endif
#else
	#define m68ki_tas_callback() 1
#endif /* M68K_TAS_HAS_CALLBACK */

#if M68K_ILLG_HAS_CALLBACK
	#if M68K_ILLG_HAS_CALLBACK == OPT_SPECIFY_HANDLER
		#define m68ki_illg_callback(opcode) M68K_ILLG_CALLBACK(opcode)
	#else
		#define m68ki_illg_callback(opcode) CALLBACK_ILLG_INSTR(opcode)
	#endif
#else
	#define m68ki_illg_callback(opcode) 0 // Default is 0 = not handled, exception will occur
#endif /* M68K_ILLG_HAS_CALLBACK */

#if M68K_INSTRUCTION_HOOK
	#if M68K_INSTRUCTION_HOOK == OPT_SPECIFY_HANDLER
		#define m68ki_instr_hook(pc) M68K_INSTRUCTION_CALLBACK(pc)
	#else
		#define m68ki_instr_hook(pc) CALLBACK_INSTR_HOOK(pc)
	#endif
#else
	#define m68ki_instr_hook(pc)
#endif /* M68K_INSTRUCTION_HOOK */

#if M68K_MONITOR_PC
	#if M68K_MONITOR_PC == OPT_SPECIFY_HANDLER
		#define m68ki_pc_changed(A) M68K_SET_PC_CALLBACK(ADDRESS_68K(A))
	#else
		#define m68ki_pc_changed(A) CALLBACK_PC_CHANGED(ADDRESS_68K(A))
	#endif
#else
	#define m68ki_pc_changed(A)
#endif /* M68K_MONITOR_PC */


/* Enable or disable function code emulation */
#if M68K_EMULATE_FC
	#if M68K_EMULATE_FC == OPT_SPECIFY_HANDLER
		#define m68ki_set_fc(A) M68K_SET_FC_CALLBACK(A)
	#else
		#define m68ki_set_fc(A) CALLBACK_SET_FC(A)
	#endif
	#define m68ki_use_data_space() m68ki_address_space = FUNCTION_CODE_USER_DATA
	#define m68ki_use_program_space() m68ki_address_space = FUNCTION_CODE_USER_PROGRAM
	#define m68ki_get_address_space() m68ki_address_space
#else
	#define m68ki_set_fc(A)
	#define m68ki_use_data_space()
	#define m68ki_use_program_space()
	#define m68ki_get_address_space() FUNCTION_CODE_USER_DATA
#endif /* M68K_EMULATE_FC */


/* Enable or disable trace emulation */
#if M68K_EMULATE_TRACE
	/* Initiates trace checking before each instruction (t1) */
	#define m68ki_trace_t1() m68ki_tracing = FLAG_T1
	/* adds t0 to trace checking if we encounter change of flow */
	#define m68ki_trace_t0() m68ki_tracing |= FLAG_T0
	/* Clear all tracing */
	#define m68ki_clear_trace() m68ki_tracing = 0
	/* Cause a trace exception if we are tracing */
	#define m68ki_exception_if_trace(a) if(m68ki_tracing) m68ki_exception_trace(a)
#else
	#define m68ki_trace_t1()
	#define m68ki_trace_t0()
	#define m68ki_clear_trace()
	#define m68ki_exception_if_trace(...)
#endif /* M68K_EMULATE_TRACE */



/* Address error */
#if M68K_EMULATE_ADDRESS_ERROR
	#include <setjmp.h>

/* sigjmp() on Mac OS X and *BSD in general saves signal contexts and is super-slow, use sigsetjmp() to tell it not to */
#ifdef _BSD_SETJMP_H
extern sigjmp_buf m68ki_aerr_trap;
#define m68ki_set_address_error_trap(state) \
	if(sigsetjmp(m68ki_aerr_trap, 0) != 0) \
	{ \
		m68ki_exception_address_error(state); \
		if(CPU_STOPPED) \
		{ \
			if (m68ki_remaining_cycles > 0) \
				m68ki_remaining_cycles = 0; \
			return m68ki_initial_cycles; \
		} \
	}

#define m68ki_check_address_error(state, ADDR, WRITE_MODE, FC) \
	if((ADDR)&1) \
	{ \
		m68ki_aerr_address = ADDR; \
		m68ki_aerr_write_mode = WRITE_MODE; \
		m68ki_aerr_fc = FC; \
		siglongjmp(m68ki_aerr_trap, 1); \
	}
#else
extern jmp_buf m68ki_aerr_trap;
	#define m68ki_set_address_error_trap(state) \
		if(setjmp(m68ki_aerr_trap) != 0) \
		{ \
			m68ki_exception_address_error(state); \
			if(CPU_STOPPED) \
			{ \
				SET_CYCLES(0); \
				return m68ki_initial_cycles; \
			} \
			/* ensure we don't re-enter execution loop after an
			   address error if there's no more cycles remaining */ \
			if(GET_CYCLES() <= 0) \
			{ \
				/* return how many clocks we used */ \
				return m68ki_initial_cycles - GET_CYCLES(); \
			} \
		}

	#define m68ki_check_address_error(state, ADDR, WRITE_MODE, FC) \
		if((ADDR)&1) \
		{ \
			m68ki_aerr_address = ADDR; \
			m68ki_aerr_write_mode = WRITE_MODE; \
			m68ki_aerr_fc = FC; \
			longjmp(m68ki_aerr_trap, 1); \
		}
#endif
	#define m68ki_bus_error(ADDR,WRITE_MODE) m68ki_aerr_address=ADDR;m68ki_aerr_write_mode=WRITE_MODE;m68ki_exception_bus_error()

	#define m68ki_check_address_error_010_less(state, ADDR, WRITE_MODE, FC) \
		if (CPU_TYPE_IS_010_LESS(CPU_TYPE)) \
		{ \
			m68ki_check_address_error(state, ADDR, WRITE_MODE, FC) \
		}
#else
	#define m68ki_set_address_error_trap(state)
	#define m68ki_check_address_error(state, ADDR, WRITE_MODE, FC)
	#define m68ki_check_address_error_010_less(state, ADDR, WRITE_MODE, FC)
#endif /* M68K_ADDRESS_ERROR */

/* Logging */
#if M68K_LOG_ENABLE
	#include <stdio.h>
//	extern FILE* M68K_LOG_FILEHANDLE;
	extern const char *const m68ki_cpu_names[];

	#define M68K_DO_LOG(A) do{printf("*************");printf A;}while(0) //if(M68K_LOG_FILEHANDLE) fprintf A
	#if M68K_LOG_1010_1111
		#define M68K_DO_LOG_EMU(A) printf A //if(M68K_LOG_FILEHANDLE) fprintf A
	#else
		#define M68K_DO_LOG_EMU(A)
	#endif
#else
	#define M68K_DO_LOG(A)
	#define M68K_DO_LOG_EMU(A)
#endif



/* -------------------------- EA / Operand Access ------------------------- */

/*
 * The general instruction format follows this pattern:
 * .... XXX. .... .YYY
 * where XXX is register X and YYY is register Y
 */
/* Data Register Isolation */
#define DX (REG_D[(REG_IR >> 9) & 7])
#define DY (REG_D[REG_IR & 7])
/* Address Register Isolation */
#define AX (REG_A[(REG_IR >> 9) & 7])
#define AY (REG_A[REG_IR & 7])


/* Effective Address Calculations */
#define EA_AY_AI_8()   AY                                    /* address register indirect */
#define EA_AY_AI_16()  EA_AY_AI_8()
#define EA_AY_AI_32()  EA_AY_AI_8()
#define EA_AY_PI_8()   (AY++)                                /* postincrement (size = byte) */
#define EA_AY_PI_16()  ((AY+=2)-2)                           /* postincrement (size = word) */
#define EA_AY_PI_32()  ((AY+=4)-4)                           /* postincrement (size = long) */
#define EA_AY_PD_8()   (--AY)                                /* predecrement (size = byte) */
#define EA_AY_PD_16()  (AY-=2)                               /* predecrement (size = word) */
#define EA_AY_PD_32()  (AY-=4)                               /* predecrement (size = long) */
#define EA_AY_DI_8()   (AY+MAKE_INT_16(m68ki_read_imm_16(state))) /* displacement */
#define EA_AY_DI_16()  EA_AY_DI_8()
#define EA_AY_DI_32()  EA_AY_DI_8()
#define EA_AY_IX_8()   m68ki_get_ea_ix(state, AY)                   /* indirect + index */
#define EA_AY_IX_16()  EA_AY_IX_8()
#define EA_AY_IX_32()  EA_AY_IX_8()

#define EA_AX_AI_8()   AX
#define EA_AX_AI_16()  EA_AX_AI_8()
#define EA_AX_AI_32()  EA_AX_AI_8()
#define EA_AX_PI_8()   (AX++)
#define EA_AX_PI_16()  ((AX+=2)-2)
#define EA_AX_PI_32()  ((AX+=4)-4)
#define EA_AX_PD_8()   (--AX)
#define EA_AX_PD_16()  (AX-=2)
#define EA_AX_PD_32()  (AX-=4)
#define EA_AX_DI_8()   (AX+MAKE_INT_16(m68ki_read_imm_16(state)))
#define EA_AX_DI_16()  EA_AX_DI_8()
#define EA_AX_DI_32()  EA_AX_DI_8()
#define EA_AX_IX_8()   m68ki_get_ea_ix(state, AX)
#define EA_AX_IX_16()  EA_AX_IX_8()
#define EA_AX_IX_32()  EA_AX_IX_8()

#define EA_A7_PI_8()   ((REG_A[7]+=2)-2)
#define EA_A7_PD_8()   (REG_A[7]-=2)

#define EA_AW_8()      MAKE_INT_16(m68ki_read_imm_16(state))      /* absolute word */
#define EA_AW_16()     EA_AW_8()
#define EA_AW_32()     EA_AW_8()
#define EA_AL_8()      m68ki_read_imm_32(state)                   /* absolute long */
#define EA_AL_16()     EA_AL_8()
#define EA_AL_32()     EA_AL_8()
#define EA_PCDI_8()    m68ki_get_ea_pcdi(state)                   /* pc indirect + displacement */
#define EA_PCDI_16()   EA_PCDI_8()
#define EA_PCDI_32()   EA_PCDI_8()
#define EA_PCIX_8()    m68ki_get_ea_pcix(state)                   /* pc indirect + index */
#define EA_PCIX_16()   EA_PCIX_8()
#define EA_PCIX_32()   EA_PCIX_8()


#define OPER_I_8(state)     m68ki_read_imm_8(state)
#define OPER_I_16(state)    m68ki_read_imm_16(state)
#define OPER_I_32(state)    m68ki_read_imm_32(state)



/* --------------------------- Status Register ---------------------------- */

/* Flag Calculation Macros */
#define CFLAG_8(A) (A)
#define CFLAG_16(A) ((A)>>8)

#if M68K_INT_GT_32_BIT
	#define CFLAG_ADD_32(S, D, R) ((R)>>24)
	#define CFLAG_SUB_32(S, D, R) ((R)>>24)
#else
	#define CFLAG_ADD_32(S, D, R) (((S & D) | (~R & (S | D)))>>23)
	#define CFLAG_SUB_32(S, D, R) (((S & R) | (~D & (S | R)))>>23)
#endif /* M68K_INT_GT_32_BIT */

#define VFLAG_ADD_8(S, D, R) ((S^R) & (D^R))
#define VFLAG_ADD_16(S, D, R) (((S^R) & (D^R))>>8)
#define VFLAG_ADD_32(S, D, R) (((S^R) & (D^R))>>24)

#define VFLAG_SUB_8(S, D, R) ((S^D) & (R^D))
#define VFLAG_SUB_16(S, D, R) (((S^D) & (R^D))>>8)
#define VFLAG_SUB_32(S, D, R) (((S^D) & (R^D))>>24)

#define NFLAG_8(A) (A)
#define NFLAG_16(A) ((A)>>8)
#define NFLAG_32(A) ((A)>>24)
#define NFLAG_64(A) ((A)>>56)

#define ZFLAG_8(A) MASK_OUT_ABOVE_8(A)
#define ZFLAG_16(A) MASK_OUT_ABOVE_16(A)
#define ZFLAG_32(A) MASK_OUT_ABOVE_32(A)


/* Flag values */
#define NFLAG_SET   0x80
#define NFLAG_CLEAR 0
#define CFLAG_SET   0x100
#define CFLAG_CLEAR 0
#define XFLAG_SET   0x100
#define XFLAG_CLEAR 0
#define VFLAG_SET   0x80
#define VFLAG_CLEAR 0
#define ZFLAG_SET   0
#define ZFLAG_CLEAR 0xffffffff

#define SFLAG_SET   4
#define SFLAG_CLEAR 0
#define MFLAG_SET   2
#define MFLAG_CLEAR 0

/* Turn flag values into 1 or 0 */
#define XFLAG_AS_1() ((FLAG_X>>8)&1)
#define NFLAG_AS_1() ((FLAG_N>>7)&1)
#define VFLAG_AS_1() ((FLAG_V>>7)&1)
#define ZFLAG_AS_1() (!FLAG_Z)
#define CFLAG_AS_1() ((FLAG_C>>8)&1)


/* Conditions */
#define COND_CS() (FLAG_C&0x100)
#define COND_CC() (!COND_CS())
#define COND_VS() (FLAG_V&0x80)
#define COND_VC() (!COND_VS())
#define COND_NE() FLAG_Z
#define COND_EQ() (!COND_NE())
#define COND_MI() (FLAG_N&0x80)
#define COND_PL() (!COND_MI())
#define COND_LT() ((FLAG_N^FLAG_V)&0x80)
#define COND_GE() (!COND_LT())
#define COND_HI() (COND_CC() && COND_NE())
#define COND_LS() (COND_CS() || COND_EQ())
#define COND_GT() (COND_GE() && COND_NE())
#define COND_LE() (COND_LT() || COND_EQ())

/* Reversed conditions */
#define COND_NOT_CS() COND_CC()
#define COND_NOT_CC() COND_CS()
#define COND_NOT_VS() COND_VC()
#define COND_NOT_VC() COND_VS()
#define COND_NOT_NE() COND_EQ()
#define COND_NOT_EQ() COND_NE()
#define COND_NOT_MI() COND_PL()
#define COND_NOT_PL() COND_MI()
#define COND_NOT_LT() COND_GE()
#define COND_NOT_GE() COND_LT()
#define COND_NOT_HI() COND_LS()
#define COND_NOT_LS() COND_HI()
#define COND_NOT_GT() COND_LE()
#define COND_NOT_LE() COND_GT()

/* Not real conditions, but here for convenience */
#define COND_XS() (FLAG_X&0x100)
#define COND_XC() (!COND_XS)


/* Get the condition code register */
#define m68ki_get_ccr(state) ((COND_XS() >> 4) | \
							 (COND_MI() >> 4) | \
							 (COND_EQ() << 2) | \
							 (COND_VS() >> 6) | \
							 (COND_CS() >> 8))

/* Get the status register */
#define m68ki_get_sr(state) ( FLAG_T1              | \
							 FLAG_T0              | \
							(FLAG_S        << 11) | \
							(FLAG_M        << 11) | \
							 FLAG_INT_MASK        | \
							 m68ki_get_ccr(state))



/* ---------------------------- Cycle Counting ---------------------------- */

#define ADD_CYCLES(A)    m68ki_remaining_cycles += (A)
#define USE_CYCLES(A)    m68ki_remaining_cycles -= (A)
#define SET_CYCLES(A)    m68ki_remaining_cycles = A
#define GET_CYCLES()     m68ki_remaining_cycles
#define USE_ALL_CYCLES() m68ki_remaining_cycles %= CYC_INSTRUCTION[REG_IR]



/* ----------------------------- Read / Write ----------------------------- */

/* Read from the current address space */
#define m68ki_read_8(state, A)  m68ki_read_8_fc (state, A, FLAG_S | m68ki_get_address_space())
#define m68ki_read_16(state, A) m68ki_read_16_fc(state, A, FLAG_S | m68ki_get_address_space())
#define m68ki_read_32(state, A) m68ki_read_32_fc(state, A, FLAG_S | m68ki_get_address_space())

/* Write to the current data space */
#define m68ki_write_8(state, A, V)  m68ki_write_8_fc (state, A, FLAG_S | FUNCTION_CODE_USER_DATA, V)
#define m68ki_write_16(state, A, V) m68ki_write_16_fc(state, A, FLAG_S | FUNCTION_CODE_USER_DATA, V)
#define m68ki_write_32(state, A, V) m68ki_write_32_fc(state, A, FLAG_S | FUNCTION_CODE_USER_DATA, V)

#if M68K_SIMULATE_PD_WRITES
#define m68ki_write_32_pd(A, V) m68ki_write_32_pd_fc(A, FLAG_S | FUNCTION_CODE_USER_DATA, V)
#else
#define m68ki_write_32_pd(state, A, V) m68ki_write_32_fc(state, A, FLAG_S | FUNCTION_CODE_USER_DATA, V)
#endif

/* Map PC-relative reads */
#define m68ki_read_pcrel_8(state, A) m68k_read_pcrelative_8(state, A)
#define m68ki_read_pcrel_16(state, A) m68k_read_pcrelative_16(state, A)
#define m68ki_read_pcrel_32(state, A) m68k_read_pcrelative_32(state, A)

/* Read from the program space */
#define m68ki_read_program_8(state, A) 	m68ki_read_8_fc(state, A, FLAG_S | FUNCTION_CODE_USER_PROGRAM)
#define m68ki_read_program_16(state, A) 	m68ki_read_16_fc(state, A, FLAG_S | FUNCTION_CODE_USER_PROGRAM)
#define m68ki_read_program_32(state, A) 	m68ki_read_32_fc(state, A, FLAG_S | FUNCTION_CODE_USER_PROGRAM)

/* Read from the data space */
#define m68ki_read_data_8(state, A) 	m68ki_read_8_fc(state, A, FLAG_S | FUNCTION_CODE_USER_DATA)
#define m68ki_read_data_16(state, A) 	m68ki_read_16_fc(state, A, FLAG_S | FUNCTION_CODE_USER_DATA)
#define m68ki_read_data_32(state, A) 	m68ki_read_32_fc(state, A, FLAG_S | FUNCTION_CODE_USER_DATA)



/* ======================================================================== */
/* =============================== PROTOTYPES ============================= */
/* ======================================================================== */

typedef union
{
	uint64 i;
	double f;
} fp_reg;

typedef struct
{
    unsigned int lower;
    unsigned int upper;
    unsigned char *offset;
} address_translation_cache;



typedef struct m68ki_cpu_core
{
	uint cpu_type;     /* CPU Type: 68000, 68008, 68010, 68EC020, 68020, 68EC030, 68030, 68EC040, or 68040 */
	uint dar[16];      /* Data and Address Registers */
	uint dar_save[16];  /* Saved Data and Address Registers (pushed onto the
						   stack when a bus error occurs)*/
	uint ppc;		   /* Previous program counter */
	uint pc;           /* Program Counter */
	uint sp[7];        /* User, Interrupt, and Master Stack Pointers */
	uint vbr;          /* Vector Base Register (m68010+) */
	uint sfc;          /* Source Function Code Register (m68010+) */
	uint dfc;          /* Destination Function Code Register (m68010+) */
	uint cacr;         /* Cache Control Register (m68020, unemulated) */
	uint caar;         /* Cache Address Register (m68020, unemulated) */
	uint ir;           /* Instruction Register */
	floatx80 fpr[8];     /* FPU Data Register (m68030/040) */
	uint fpiar;        /* FPU Instruction Address Register (m68040) */
	uint fpsr;         /* FPU Status Register (m68040) */
	uint fpcr;         /* FPU Control Register (m68040) */
	uint t1_flag;      /* Trace 1 */
	uint t0_flag;      /* Trace 0 */
	uint s_flag;       /* Supervisor */
	uint m_flag;       /* Master/Interrupt state */
	uint x_flag;       /* Extend */
	uint n_flag;       /* Negative */
	uint not_z_flag;   /* Zero, inverted for speedups */
	uint v_flag;       /* Overflow */
	uint c_flag;       /* Carry */
	uint int_mask;     /* I0-I2 */
	uint int_level;    /* State of interrupt pins IPL0-IPL2 -- ASG: changed from ints_pending */
	uint stopped;      /* Stopped state */
	uint pref_addr;    /* Last prefetch address */
	uint pref_data;    /* Data in the prefetch queue */
	uint address_mask; /* Available address pins */
	uint sr_mask;      /* Implemented status register bits */
	uint instr_mode;   /* Stores whether we are in instruction mode or group 0/1 exception mode */
	uint run_mode;     /* Stores whether we are processing a reset, bus error, address error, or something else */
	int    has_pmmu;   /* Indicates if a PMMU available (yes on 030, 040, no on EC030) */
	int    has_fpu;    /* Indicates if a FPU available */
	int    pmmu_enabled; /* Indicates if the PMMU is enabled */
	int    fpu_just_reset; /* Indicates the FPU was just reset */
	uint reset_cycles;

	/* Clocks required for instructions / exceptions */
	uint cyc_bcc_notake_b;
	uint cyc_bcc_notake_w;
	uint cyc_dbcc_f_noexp;
	uint cyc_dbcc_f_exp;
	uint cyc_scc_r_true;
	uint cyc_movem_w;
	uint cyc_movem_l;
	uint cyc_shift;
	uint cyc_reset;

	/* Virtual IRQ lines state */
	uint virq_state;
	uint nmi_pending;

	/* PMMU registers */
	uint mmu_crp_aptr, mmu_crp_limit;
	uint mmu_srp_aptr, mmu_srp_limit;
	uint mmu_tc;
	uint16 mmu_sr;

	uint mmu_urp_aptr;    /* 040 only */
	uint mmu_sr_040;
	uint mmu_atc_tag[MMU_ATC_ENTRIES], mmu_atc_data[MMU_ATC_ENTRIES];
	uint mmu_atc_rr;
	uint mmu_tt0, mmu_tt1;
	uint mmu_itt0, mmu_itt1, mmu_dtt0, mmu_dtt1;
	uint mmu_acr0, mmu_acr1, mmu_acr2, mmu_acr3;
	uint mmu_last_page_entry, mmu_last_page_entry_addr;

	uint16 mmu_tmp_sr;      /* temporary hack: status code for ptest and to handle write protection */
	uint16 mmu_tmp_fc;      /* temporary hack: function code for the mmu (moves) */
	uint16 mmu_tmp_rw;      /* temporary hack: read/write (1/0) for the mmu */
	uint8 mmu_tmp_sz;       /* temporary hack: size for mmu */

	uint mmu_tmp_buserror_address;   /* temporary hack: (first) bus error address */
	uint16 mmu_tmp_buserror_occurred;  /* temporary hack: flag that bus error has occurred from mmu */
	uint16 mmu_tmp_buserror_fc;   /* temporary hack: (first) bus error fc */
	uint16 mmu_tmp_buserror_rw;   /* temporary hack: (first) bus error rw */
	uint16 mmu_tmp_buserror_sz;   /* temporary hack: (first) bus error size` */

	uint8 mmu_tablewalk;             /* set when MMU walks page tables */
	uint mmu_last_logical_addr;
	uint ic_address[M68K_IC_SIZE];   /* instruction cache address data */
	uint ic_data[M68K_IC_SIZE];      /* instruction cache content data */
	uint8 ic_valid[M68K_IC_SIZE];     /* instruction cache valid flags */

	const uint8* cyc_instruction;
	const uint8* cyc_exception;

	/* Callbacks to host */
	uint16_t  (*int_ack_callback)(int int_line);           /* Interrupt Acknowledge */
	void (*bkpt_ack_callback)(unsigned int data);     /* Breakpoint Acknowledge */
	void (*reset_instr_callback)(void);               /* Called when a RESET instruction is encountered */
 	void (*cmpild_instr_callback)(unsigned int, int); /* Called when a CMPI.L #v, Dn instruction is encountered */
 	void (*rte_instr_callback)(void);                 /* Called when a RTE instruction is encountered */
	int  (*tas_instr_callback)(void);                 /* Called when a TAS instruction is encountered, allows / disallows writeback */
	int  (*illg_instr_callback)(int);                 /* Called when an illegal instruction is encountered, allows handling */
	void (*pc_changed_callback)(unsigned int new_pc); /* Called when the PC changes by a large amount */
	void (*set_fc_callback)(unsigned int new_fc);     /* Called when the CPU function code changes */
	void (*instr_hook_callback)(unsigned int pc);     /* Called every instruction cycle prior to execution */

	/* address translation caches */

	uint32 ovl;

	unsigned char read_ranges;
	unsigned int read_addr[8];
	unsigned int read_upper[8];
	unsigned char *read_data[8];
	unsigned char write_ranges;
	unsigned int write_addr[8];
	unsigned int write_upper[8];
	unsigned char *write_data[8];
	address_translation_cache code_translation_cache;
	address_translation_cache fc_read_translation_cache;
	address_translation_cache fc_write_translation_cache;

	volatile unsigned int *gpio;
} m68ki_cpu_core;


extern m68ki_cpu_core m68ki_cpu;
extern sint           m68ki_remaining_cycles;
extern uint           m68ki_tracing;
extern const uint8    m68ki_shift_8_table[];
extern const uint16   m68ki_shift_16_table[];
extern const uint     m68ki_shift_32_table[];
extern const uint8    m68ki_exception_cycle_table[][256];
extern uint           m68ki_address_space;
extern const uint8    m68ki_ea_idx_cycle_table[];

extern uint           m68ki_aerr_address;
extern uint           m68ki_aerr_write_mode;
extern uint           m68ki_aerr_fc;

/* Forward declarations to keep some of the macros happy */
static inline uint m68ki_read_16_fc(m68ki_cpu_core *state, uint address, uint fc);
static inline uint m68ki_read_32_fc(m68ki_cpu_core *state, uint address, uint fc);
static inline uint m68ki_get_ea_ix(m68ki_cpu_core *state, uint An);
static inline void m68ki_check_interrupts(m68ki_cpu_core *state);            /* ASG: check for interrupts */

/* quick disassembly (used for logging) */
char* m68ki_disassemble_quick(unsigned int pc, unsigned int cpu_type);


/* ======================================================================== */
/* =========================== UTILITY FUNCTIONS ========================== */
/* ======================================================================== */


/* ---------------------------- Read Immediate ---------------------------- */

// clear the instruction cache
inline void m68ki_ic_clear(m68ki_cpu_core *state)
{
	int i;
	for (i=0; i< M68K_IC_SIZE; i++) {
		state->ic_address[i] = ~0;
	}
}

extern uint32 pmmu_translate_addr(m68ki_cpu_core *state, uint32 addr_in, uint16 rw);

// read immediate word using the instruction cache

extern volatile int g_buserr;

static inline uint32 m68ki_ic_readimm16(m68ki_cpu_core *state, uint32 address)
{
	if (state->cacr & M68K_CACR_EI)
	{
		// 68020 series I-cache (MC68020 User's Manual, Section 4 - On-Chip Cache Memory)
		if (CPU_TYPE & (CPU_TYPE_EC020 | CPU_TYPE_020))
		{
			uint32 tag = (address >> 8) | (state->s_flag ? 0x1000000 : 0);
			int idx = (address >> 2) & 0x3f;    // 1-of-64 select

			// do a cache fill if the line is invalid or the tags don't match
			if ((!state->ic_valid[idx]) || (state->ic_address[idx] != tag))
			{
				// if the cache is frozen, don't update it
				if (state->cacr & M68K_CACR_FI)
				{
					return m68k_read_immediate_16(state, address);
					//m68ki_set_fc ( FLAG_S | FUNCTION_CODE_USER_PROGRAM );
					//return ps_read_16 ( ADDRESS_68K (address) );
				}

				uint32 data = m68ki_read_32(state, address & ~3);

				//printf("m68k: doing cache fill at %08x (tag %08x idx %d)\n", address, tag, idx);
				//state->mmu_tmp_buserror_occurred = g_buserr;
				// if no buserror occurred, validate the tag
				//if (!state->mmu_tmp_buserror_occurred)
				if ( !g_buserr )
				{
					state->ic_address[idx] = tag;
					state->ic_data[idx] = data;
					state->ic_valid[idx] = 1;
				}
				else
				{
					return m68k_read_immediate_16(state, address);
					//m68ki_set_fc ( FLAG_S | FUNCTION_CODE_USER_PROGRAM );
					//return ps_read_16 ( ADDRESS_68K (address) );
				}
			}

			// at this point, the cache is guaranteed to be valid, either as
			// a hit or because we just filled it.
			if (address & 2)
			{
				return state->ic_data[idx] & 0xffff;
			}
			else
			{
				return state->ic_data[idx] >> 16;
			}
		}
	}
	return m68k_read_immediate_16(state, address);
	//m68ki_set_fc ( FLAG_S | FUNCTION_CODE_USER_PROGRAM );
	//return ps_read_16 ( ADDRESS_68K (address) );
}

/* Handles all immediate reads, does address error check, function code setting,
 * and prefetching if they are enabled in m68kconf.h
 */
//uint m68ki_read_imm16_addr_slowpath(m68ki_cpu_core *state, uint32_t pc, address_translation_cache *cache);
uint m68ki_read_imm16_addr_slowpath ( m68ki_cpu_core *state, uint32_t pc );

static inline uint m68ki_read_imm_16(m68ki_cpu_core *state)
{
	
#ifdef CACHE_ON // cryptodad
	uint32_t pc = REG_PC;

	address_translation_cache *cache = &state->code_translation_cache;
	
	if(pc >= cache->lower && pc < cache->upper)
	{
		REG_PC += 2;
		return be16toh(((unsigned short *)(cache->offset + pc))[0]);
	}
#endif
	//return m68ki_read_imm16_addr_slowpath ( state, pc, cache );
	return m68ki_read_imm16_addr_slowpath ( state, REG_PC );
}

static inline uint m68ki_read_imm_8(m68ki_cpu_core *state)
{
	/* map read immediate 8 to read immediate 16 */
	return MASK_OUT_ABOVE_8(m68ki_read_imm_16(state));
}

static inline uint m68ki_read_imm_32(m68ki_cpu_core *state)
{
#if M68K_SEPARATE_READS
#if M68K_EMULATE_PMMU
//	if (PMMU_ENABLED)
//	    address = pmmu_translate_addr(address,1);
#endif
#endif
	uint32_t address = ADDRESS_68K(REG_PC);
	for (int i = 0; i < state->read_ranges; i++) {
		if(address >= state->read_addr[i] && address < state->read_upper[i]) {
			REG_PC += 4;
			return be32toh(((unsigned int *)(state->read_data[i] + (address - state->read_addr[i])))[0]);
		}
	}

#if M68K_EMULATE_PREFETCH
	uint temp_val;

	m68ki_set_fc(FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */
	//state->mmu_tmp_fc = FLAG_S | FUNCTION_CODE_USER_PROGRAM;
	//state->mmu_tmp_rw = 1;
	//state->mmu_tmp_sz = M68K_SZ_LONG;
	//m68ki_check_address_error(state, REG_PC, MODE_READ, FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */

	if(REG_PC != CPU_PREF_ADDR)
	{
		CPU_PREF_ADDR = REG_PC;
		CPU_PREF_DATA = m68ki_ic_readimm16(state, ADDRESS_68K(CPU_PREF_ADDR));
	}
	temp_val = MASK_OUT_ABOVE_16(CPU_PREF_DATA);
	REG_PC += 2;
	CPU_PREF_ADDR = REG_PC;
	CPU_PREF_DATA = m68ki_ic_readimm16(state, ADDRESS_68K(CPU_PREF_ADDR));

	temp_val = MASK_OUT_ABOVE_32((temp_val << 16) | MASK_OUT_ABOVE_16(CPU_PREF_DATA));
	REG_PC += 2;
	CPU_PREF_DATA = m68ki_ic_readimm16(state, REG_PC);

	//state->mmu_tmp_buserror_occurred = g_buserr;
	
	//CPU_PREF_ADDR = state->mmu_tmp_buserror_occurred ? ((uint32)~0) : REG_PC;
	CPU_PREF_ADDR = g_buserr ? ((uint32)~0) : REG_PC;

	return temp_val;
#else
	REG_PC += 4;

	return m68k_read_immediate_32(state, address);
#endif /* M68K_EMULATE_PREFETCH */
}

/* ------------------------- Top level read/write ------------------------- */

/* Handles all memory accesses (except for immediate reads if they are
 * configured to use separate functions in m68kconf.h).
 * All memory accesses must go through these top level functions.
 * These functions will also check for address error and set the function
 * code if they are enabled in m68kconf.h.
 */

#define SET_FC_TRANSLATION_CACHE_VALUES \
	cache->lower = state->read_addr[i]; \
	cache->upper = state->read_upper[i]; \
	cache->offset = state->read_data[i];

#define SET_FC_WRITE_TRANSLATION_CACHE_VALUES \
	cache->lower = state->write_addr[i]; \
	cache->upper = state->write_upper[i]; \
	cache->offset = state->write_data[i];

// M68KI_READ_8_FC
static inline uint m68ki_read_8_fc(m68ki_cpu_core *state, uint address, uint fc)
{
	//(void)fc;
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	//state->mmu_tmp_fc = fc;
	//state->mmu_tmp_rw = 1;
	//state->mmu_tmp_sz = M68K_SZ_BYTE;

#if M68K_EMULATE_PMMU
	if (PMMU_ENABLED)
	    address = pmmu_translate_addr(state,address,1);
#endif
	//address_translation_cache *cache = &state->fc_read_translation_cache;
#ifdef CACHE_ON  // cryptodad
	if(cache->offset && address >= cache->lower && address < cache->upper)
	{
		return cache->offset[address - cache->lower];
	}
#endif
#ifdef T_CACHE_ON
	for (int i = 0; i < state->read_ranges; i++) {
		if(address >= state->read_addr[i] && address < state->read_upper[i]) {
			SET_FC_TRANSLATION_CACHE_VALUES
			return state->read_data[i][address - state->read_addr[i]];
		}
	}
#endif

#ifdef CHIP_FASTPATH
	if ( address < cfg->mapped_low )
		return ps_read_8 ( (t_a32)address );
#endif

	return m68k_read_memory_8 ( ADDRESS_68K (address) );
	//return m68k_read_memory_8 ( address );
}

// M68KI_READ_16_FC
static inline uint m68ki_read_16_fc ( m68ki_cpu_core *state, uint address, uint fc )
{
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	//state->mmu_tmp_fc = fc;
	//state->mmu_tmp_rw = 1;
	//state->mmu_tmp_sz = M68K_SZ_WORD;
	//m68ki_check_address_error_010_less(state, address, MODE_READ, fc); /* auto-disable (see m68kcpu.h) */

#if M68K_EMULATE_PMMU
	if (PMMU_ENABLED)
	    address = pmmu_translate_addr(state,address,1);
#endif

	//address_translation_cache *cache = &state->fc_read_translation_cache;
#ifdef CACHE_ON  // cryptodad
	if(cache->offset && address >= cache->lower && address < cache->upper)
	{
		return be16toh(((unsigned short *)(cache->offset + (address - cache->lower)))[0]);
	}
#endif
#ifdef T_CACHE_ON
	for (int i = 0; i < state->read_ranges; i++) {
		if(address >= state->read_addr[i] && address < state->read_upper[i]) {
			SET_FC_TRANSLATION_CACHE_VALUES
			return be16toh(((unsigned short *)(state->read_data[i] + (address - state->read_addr[i])))[0]);
		}
	}
#endif

#ifdef CHIP_FASTPATH
	//if (!state->ovl && address < 0x200000) {
	//if (!state->ovl && address < FASTPATH_UPPER) 
	//{
	//	if (address & 0x01) {
	//	    return ((ps_read_8(address) << 8) | ps_read_8(address + 1));
	//	}
	//	return ps_read_16 ( (t_a32)address );
	//}
	if ( address < cfg->mapped_low )
		return ps_read_16 ( (t_a32)address );
#endif

	return m68k_read_memory_16 ( ADDRESS_68K (address) );
	//return m68k_read_memory_16 ( address );
}

// M68KI_READ_32_FC
static inline uint m68ki_read_32_fc(m68ki_cpu_core *state, uint address, uint fc)
{
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	//state->mmu_tmp_fc = fc;
	//state->mmu_tmp_rw = 1;
	//state->mmu_tmp_sz = M68K_SZ_LONG;
	//m68ki_check_address_error_010_less(state, address, MODE_READ, fc); /* auto-disable (see m68kcpu.h) */

#if M68K_EMULATE_PMMU
	if (PMMU_ENABLED)
	    address = pmmu_translate_addr(state,address,1);
#endif
	//address_translation_cache *cache = &state->fc_read_translation_cache;
#ifdef CACHE_ON  // cryptodad
	if(cache->offset && address >= cache->lower && address < cache->upper)
	{
		return be32toh(((unsigned int *)(cache->offset + (address - cache->lower)))[0]);
	}
#endif
#ifdef T_CACHE_ON
	for (int i = 0; i < state->read_ranges; i++) {
		if(address >= state->read_addr[i] && address < state->read_upper[i]) {
			SET_FC_TRANSLATION_CACHE_VALUES
			return be32toh(((unsigned int *)(state->read_data[i] + (address - state->read_addr[i])))[0]);
		}
	}
#endif

#ifdef CHIP_FASTPATH
	//if (!state->ovl && address < 0x200000) {
	//if (!state->ovl && address < FASTPATH_UPPER) 
	//{
	//	if (address & 0x01) {
	//		uint32_t c = ps_read_8(address);
	//		c |= (be16toh(ps_read_16(address+1)) << 8);
	//		c |= (ps_read_8(address + 3) << 24);
	//		return htobe32(c);
	//	}
	//	return ps_read_32 ( (t_a32)address );
	//}
	if ( address < cfg->mapped_low )
		return ps_read_32 ( (t_a32)address );
#endif

	return m68k_read_memory_32 ( ADDRESS_68K (address));
	//return m68k_read_memory_32 ( address );
}

// M68KI_WRITE_8_FC
static inline void m68ki_write_8_fc(m68ki_cpu_core *state, uint address, uint fc, uint value)
{
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	//state->mmu_tmp_fc = fc;
	//state->mmu_tmp_rw = 0;
	//state->mmu_tmp_sz = M68K_SZ_BYTE;

#if M68K_EMULATE_PMMU
	if (PMMU_ENABLED)
	    address = pmmu_translate_addr(state,address,0);
#endif
	//address_translation_cache *cache = &state->fc_read_translation_cache;
#ifdef CACHE_ON  // cryptodad
	if(cache->offset && address >= cache->lower && address < cache->upper)
	{
		cache->offset[address - cache->lower] = (unsigned char)value;
		return;
	}
#endif
#ifdef T_CACHE_ON
	for (int i = 0; i < state->write_ranges; i++) {
		if(address >= state->write_addr[i] && address < state->write_upper[i]) {
			SET_FC_WRITE_TRANSLATION_CACHE_VALUES
			state->write_data[i][address - state->write_addr[i]] = (unsigned char)value;
			return;
		}
	}
#endif

#ifdef CHIP_FASTPATH
	//if (!state->ovl && address < 0x200000) {
	//if (!state->ovl && address < FASTPATH_UPPER) 
	//{
	//	ps_write_8 ( (t_a32)address, value );
	//	return;
	//}
	if ( address < cfg->mapped_low )
	{
		ps_write_8 ( (t_a32)address, value );
		return;
	}
#endif

	m68k_write_memory_8(ADDRESS_68K(address), value);
	//m68k_write_memory_8( address, value);
}

// M68KI_WRITE_16_FC
static inline void m68ki_write_16_fc(m68ki_cpu_core *state, uint address, uint fc, uint value)
{
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	//state->mmu_tmp_fc = fc;
	//state->mmu_tmp_rw = 0;
	//state->mmu_tmp_sz = M68K_SZ_WORD;
	//m68ki_check_address_error_010_less(state, address, MODE_WRITE, fc); /* auto-disable (see m68kcpu.h) */

#if M68K_EMULATE_PMMU
	if (PMMU_ENABLED)
	    address = pmmu_translate_addr(state,address,0);
#endif
	//address_translation_cache *cache = &state->fc_read_translation_cache;
#ifdef CACHE_ON  // cryptodad
	if(cache->offset && address >= cache->lower && address < cache->upper)
	{
		((short *)(cache->offset + (address - cache->lower)))[0] = htobe16(value);
		return;
	}
#endif
#ifdef T_CACHE_ON
	for (int i = 0; i < state->write_ranges; i++) {
		if(address >= state->write_addr[i] && address < state->write_upper[i]) {
			SET_FC_WRITE_TRANSLATION_CACHE_VALUES
			((short *)(state->write_data[i] + (address - state->write_addr[i])))[0] = htobe16(value);
			return;
		}
	}
#endif

#ifdef CHIP_FASTPATH
	//if (!state->ovl && address < 0x200000) {
	//if (!state->ovl && address < FASTPATH_UPPER) 
	//{
	//	if (address & 0x01) {
			//ps_write_8 ( value & 0xFF, address );
			//ps_write_8 ( (value >> 8) & 0xFF, address + 1 );
	//		ps_write_8 ( address, value & 0xFF );
	//		ps_write_8 ( address + 1, (value >> 8) & 0xFF );
	//		return;
	//	}
	//	ps_write_16 ( (t_a32)address, value);
	//	return;
	//}
	if ( address < cfg->mapped_low )
	{
		ps_write_16 ( (t_a32)address, value );
		return;
	}
#endif

	m68k_write_memory_16(ADDRESS_68K(address), value);
	//m68k_write_memory_16( address, value);
}

// M68KI_WRITE_32_FC
static inline void m68ki_write_32_fc(m68ki_cpu_core *state, uint address, uint fc, uint value)
{
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	//state->mmu_tmp_fc = fc;
	//state->mmu_tmp_rw = 0;
	//state->mmu_tmp_sz = M68K_SZ_LONG;
	//m68ki_check_address_error_010_less(state, address, MODE_WRITE, fc); /* auto-disable (see m68kcpu.h) */

#if M68K_EMULATE_PMMU
	if (PMMU_ENABLED)
	    address = pmmu_translate_addr(state,address,0);
#endif
	//address_translation_cache *cache = &state->fc_read_translation_cache;
#ifdef CACHE_ON // cryptodad
	if(cache->offset && address >= cache->lower && address < cache->upper)
	{
		((int *)(cache->offset + (address - cache->lower)))[0] = htobe32(value);
		return;
	}
#endif
#ifdef T_CACHE_ON
	for (int i = 0; i < state->write_ranges; i++) {
		if(address >= state->write_addr[i] && address < state->write_upper[i]) {
			SET_FC_WRITE_TRANSLATION_CACHE_VALUES
			((int *)(state->write_data[i] + (address - state->write_addr[i])))[0] = htobe32(value);
			return;
		}
	}
#endif

#ifdef CHIP_FASTPATH
	//if (!state->ovl && address < 0x200000) 
	//if (!state->ovl && address < FASTPATH_UPPER) 
	//{
	//	if (address & 0x01) 
	//	{
			//ps_write_8 ( value & 0xFF, address );
			//ps_write_16 ( htobe16 ( ((value >> 8) & 0xFFFF) ), address + 1 );
			//ps_write_8 ( (value >> 24), address + 3 );
	//		ps_write_8 ( address, value & 0xFF );
	//		ps_write_16 ( (t_a32)(address + 1), htobe16 ( ((value >> 8) & 0xFFFF) ) );
	//		ps_write_8 ( address + 3, (value >> 24) );
	//		return;
	//	}
	//	ps_write_32 ( (t_a32)address, value );
	//	return;
	//}
	if ( address < cfg->mapped_low )
	{
		ps_write_32 ( (t_a32)address, value );
		return;
	}
#endif

	m68k_write_memory_32(ADDRESS_68K(address), value);
	//m68k_write_memory_32( address, value);
}

#if M68K_SIMULATE_PD_WRITES
/* Special call to simulate undocumented 68k behavior when move.l with a
 * predecrement destination mode is executed.
 * A real 68k first writes the high word to [address+2], and then writes the
 * low word to [address].
 */
static inline void m68ki_write_32_pd_fc(uint address, uint fc, uint value)
{
	m68ki_set_fc(fc); /* auto-disable (see m68kcpu.h) */
	//state->mmu_tmp_fc = fc;
	//state->mmu_tmp_rw = 0;
	//state->mmu_tmp_sz = M68K_SZ_LONG;
	//m68ki_check_address_error_010_less(state, address, MODE_WRITE, fc); /* auto-disable (see m68kcpu.h) */

#if M68K_EMULATE_PMMU
	if (PMMU_ENABLED)
	    address = pmmu_translate_addr(state,address,0);
#endif

	m68k_write_memory_32_pd(ADDRESS_68K(address), value);
}
#endif

/* --------------------- Effective Address Calculation -------------------- */

/* The program counter relative addressing modes cause operands to be
 * retrieved from program space, not data space.
 */
static inline uint m68ki_get_ea_pcdi(m68ki_cpu_core *state)
{
	uint old_pc = REG_PC;
	m68ki_use_program_space(); /* auto-disable */
	return old_pc + MAKE_INT_16(m68ki_read_imm_16(state));
}


static inline uint m68ki_get_ea_pcix(m68ki_cpu_core *state)
{
	m68ki_use_program_space(); /* auto-disable */
	return m68ki_get_ea_ix(state, REG_PC);
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
static inline uint m68ki_get_ea_ix(m68ki_cpu_core *state, uint An)
{
	/* An = base register */
	uint extension = m68ki_read_imm_16(state);
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
		bd = BIT_4(extension) ? m68ki_read_imm_32(state) : (uint32)MAKE_INT_16(m68ki_read_imm_16(state));

	/* If no indirect action, we are done */
	if(!(extension&7))                  /* No Memory Indirect */
		return An + bd + Xn;

	/* Check if outer displacement is present */
	if(BIT_1(extension))                /* I/IS:  od */
		od = BIT_0(extension) ? m68ki_read_imm_32(state) : (uint32)MAKE_INT_16(m68ki_read_imm_16(state));

	/* Postindex */
	if(BIT_2(extension))                /* I/IS:  0 = preindex, 1 = postindex */
		return m68ki_read_32(state, An + bd) + Xn + od;

	/* Preindex */
	return m68ki_read_32(state, An + bd + Xn) + od;
}


/* Fetch operands */
static inline uint OPER_AY_AI_8(m68ki_cpu_core *state) {uint ea = EA_AY_AI_8();  return m68ki_read_8(state, ea); }
static inline uint OPER_AY_AI_16(m68ki_cpu_core *state) {uint ea = EA_AY_AI_16(); return m68ki_read_16(state, ea);}
static inline uint OPER_AY_AI_32(m68ki_cpu_core *state) {uint ea = EA_AY_AI_32(); return m68ki_read_32(state, ea);}
static inline uint OPER_AY_PI_8(m68ki_cpu_core *state) {uint ea = EA_AY_PI_8();  return m68ki_read_8(state, ea); }
static inline uint OPER_AY_PI_16(m68ki_cpu_core *state) {uint ea = EA_AY_PI_16(); return m68ki_read_16(state, ea);}
static inline uint OPER_AY_PI_32(m68ki_cpu_core *state) {uint ea = EA_AY_PI_32(); return m68ki_read_32(state, ea);}
static inline uint OPER_AY_PD_8(m68ki_cpu_core *state) {uint ea = EA_AY_PD_8();  return m68ki_read_8(state, ea); }
static inline uint OPER_AY_PD_16(m68ki_cpu_core *state) {uint ea = EA_AY_PD_16(); return m68ki_read_16(state, ea);}
static inline uint OPER_AY_PD_32(m68ki_cpu_core *state) {uint ea = EA_AY_PD_32(); return m68ki_read_32(state, ea);}
static inline uint OPER_AY_DI_8(m68ki_cpu_core *state) {uint ea = EA_AY_DI_8();  return m68ki_read_8(state, ea); }
static inline uint OPER_AY_DI_16(m68ki_cpu_core *state) {uint ea = EA_AY_DI_16(); return m68ki_read_16(state, ea);}
static inline uint OPER_AY_DI_32(m68ki_cpu_core *state) {uint ea = EA_AY_DI_32(); return m68ki_read_32(state, ea);}
static inline uint OPER_AY_IX_8(m68ki_cpu_core *state) {uint ea = EA_AY_IX_8();  return m68ki_read_8(state, ea); }
static inline uint OPER_AY_IX_16(m68ki_cpu_core *state) {uint ea = EA_AY_IX_16(); return m68ki_read_16(state, ea);}
static inline uint OPER_AY_IX_32(m68ki_cpu_core *state) {uint ea = EA_AY_IX_32(); return m68ki_read_32(state, ea);}

static inline uint OPER_AX_AI_8(m68ki_cpu_core *state) {uint ea = EA_AX_AI_8();  return m68ki_read_8(state, ea); }
static inline uint OPER_AX_AI_16(m68ki_cpu_core *state) {uint ea = EA_AX_AI_16(); return m68ki_read_16(state, ea);}
static inline uint OPER_AX_AI_32(m68ki_cpu_core *state) {uint ea = EA_AX_AI_32(); return m68ki_read_32(state, ea);}
static inline uint OPER_AX_PI_8(m68ki_cpu_core *state) {uint ea = EA_AX_PI_8();  return m68ki_read_8(state, ea); }
static inline uint OPER_AX_PI_16(m68ki_cpu_core *state) {uint ea = EA_AX_PI_16(); return m68ki_read_16(state, ea);}
static inline uint OPER_AX_PI_32(m68ki_cpu_core *state) {uint ea = EA_AX_PI_32(); return m68ki_read_32(state, ea);}
static inline uint OPER_AX_PD_8(m68ki_cpu_core *state) {uint ea = EA_AX_PD_8();  return m68ki_read_8(state, ea); }
static inline uint OPER_AX_PD_16(m68ki_cpu_core *state) {uint ea = EA_AX_PD_16(); return m68ki_read_16(state, ea);}
static inline uint OPER_AX_PD_32(m68ki_cpu_core *state) {uint ea = EA_AX_PD_32(); return m68ki_read_32(state, ea);}
static inline uint OPER_AX_DI_8(m68ki_cpu_core *state) {uint ea = EA_AX_DI_8();  return m68ki_read_8(state, ea); }
static inline uint OPER_AX_DI_16(m68ki_cpu_core *state) {uint ea = EA_AX_DI_16(); return m68ki_read_16(state, ea);}
static inline uint OPER_AX_DI_32(m68ki_cpu_core *state) {uint ea = EA_AX_DI_32(); return m68ki_read_32(state, ea);}
static inline uint OPER_AX_IX_8(m68ki_cpu_core *state) {uint ea = EA_AX_IX_8();  return m68ki_read_8(state, ea); }
static inline uint OPER_AX_IX_16(m68ki_cpu_core *state) {uint ea = EA_AX_IX_16(); return m68ki_read_16(state, ea);}
static inline uint OPER_AX_IX_32(m68ki_cpu_core *state) {uint ea = EA_AX_IX_32(); return m68ki_read_32(state, ea);}

static inline uint OPER_A7_PI_8(m68ki_cpu_core *state) {uint ea = EA_A7_PI_8();  return m68ki_read_8(state, ea); }
static inline uint OPER_A7_PD_8(m68ki_cpu_core *state) {uint ea = EA_A7_PD_8();  return m68ki_read_8(state, ea); }

static inline uint OPER_AW_8(m68ki_cpu_core *state) {uint ea = EA_AW_8();     return m68ki_read_8(state, ea); }
static inline uint OPER_AW_16(m68ki_cpu_core *state) {uint ea = EA_AW_16();    return m68ki_read_16(state, ea);}
static inline uint OPER_AW_32(m68ki_cpu_core *state) {uint ea = EA_AW_32();    return m68ki_read_32(state, ea);}
static inline uint OPER_AL_8(m68ki_cpu_core *state) {uint ea = EA_AL_8();     return m68ki_read_8(state, ea); }
static inline uint OPER_AL_16(m68ki_cpu_core *state) {uint ea = EA_AL_16();    return m68ki_read_16(state, ea);}
static inline uint OPER_AL_32(m68ki_cpu_core *state) {uint ea = EA_AL_32();    return m68ki_read_32(state, ea);}
static inline uint OPER_PCDI_8(m68ki_cpu_core *state) {uint ea = EA_PCDI_8();   return m68ki_read_pcrel_8(state, ea);}
static inline uint OPER_PCDI_16(m68ki_cpu_core *state) {uint ea = EA_PCDI_16();  return m68ki_read_pcrel_16(state, ea);}
static inline uint OPER_PCDI_32(m68ki_cpu_core *state) {uint ea = EA_PCDI_32();  return m68ki_read_pcrel_32(state, ea);}
static inline uint OPER_PCIX_8(m68ki_cpu_core *state) {uint ea = EA_PCIX_8();   return m68ki_read_pcrel_8(state, ea);}
static inline uint OPER_PCIX_16(m68ki_cpu_core *state) {uint ea = EA_PCIX_16();  return m68ki_read_pcrel_16(state, ea);}
static inline uint OPER_PCIX_32(m68ki_cpu_core *state) {uint ea = EA_PCIX_32();  return m68ki_read_pcrel_32(state, ea);}



/* ---------------------------- Stack Functions --------------------------- */

/* Push/pull data from the stack */
static inline void m68ki_push_16(m68ki_cpu_core *state, uint value)
{
	//printf ( "_push16 REG_SP = $%X\n", REG_SP );
	REG_SP = MASK_OUT_ABOVE_32(REG_SP - 2);
	m68ki_write_16(state, REG_SP, value);
}

static inline void m68ki_push_32(m68ki_cpu_core *state, uint value)
{
	//printf ( "_push32 REG_SP = $%X\n", REG_SP );
	REG_SP = MASK_OUT_ABOVE_32(REG_SP - 4);
	m68ki_write_32(state, REG_SP, value);
}

static inline uint m68ki_pull_16(m68ki_cpu_core *state)
{
	//printf ( "_pull16 REG_SP = $%X\n", REG_SP );
	REG_SP = MASK_OUT_ABOVE_32(REG_SP + 2);
	return m68ki_read_16(state, REG_SP - 2);
}

static inline uint m68ki_pull_32(m68ki_cpu_core *state)
{
	//printf ( "_pull32 REG_SP = $%X\n", REG_SP );
	REG_SP = MASK_OUT_ABOVE_32(REG_SP + 4);
	return m68ki_read_32(state, REG_SP - 4);
}


/* Increment/decrement the stack as if doing a push/pull but
 * don't do any memory access.
 */
static inline void m68ki_fake_push_16(m68ki_cpu_core *state)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP - 2);
}

static inline void m68ki_fake_push_32(m68ki_cpu_core *state)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP - 4);
}

static inline void m68ki_fake_pull_16(m68ki_cpu_core *state)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP + 2);
}

static inline void m68ki_fake_pull_32(m68ki_cpu_core *state)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP + 4);
}


/* ----------------------------- Program Flow ----------------------------- */

/* Jump to a new program location or vector.
 * These functions will also call the pc_changed callback if it was enabled
 * in m68kconf.h.
 */
static inline void m68ki_jump ( m68ki_cpu_core *state, uint new_pc)
{
	REG_PC = new_pc;
	m68ki_pc_changed ( REG_PC );
}

static inline void m68ki_jump_vector(m68ki_cpu_core *state, uint vector)
{
	REG_PC = ( vector << 2 ) + REG_VBR;
	REG_PC = ps_read_32 ( REG_PC );//m68ki_read_data_32 ( state, REG_PC );

	m68ki_pc_changed ( REG_PC );
}


/* Branch to a new memory location.
 * The 32-bit branch will call pc_changed if it was enabled in m68kconf.h.
 * So far I've found no problems with not calling pc_changed for 8 or 16
 * bit branches.
 */
static inline void m68ki_branch_8(m68ki_cpu_core *state, uint offset)
{
	REG_PC += MAKE_INT_8(offset);
}

static inline void m68ki_branch_16(m68ki_cpu_core *state, uint offset)
{
	REG_PC += MAKE_INT_16(offset);
}

static inline void m68ki_branch_32(m68ki_cpu_core *state, uint offset)
{
	REG_PC += offset;
	m68ki_pc_changed(REG_PC);
}

/* ---------------------------- Status Register --------------------------- */

/* Set the S flag and change the active stack pointer.
 * Note that value MUST be 4 or 0.
 */
static inline void m68ki_set_s_flag(m68ki_cpu_core *state, uint value)
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
static inline void m68ki_set_sm_flag(m68ki_cpu_core *state, uint value)
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
static inline void m68ki_set_sm_flag_nosp(m68ki_cpu_core *state, uint value)
{
	/* Set the S and M flags */
	FLAG_S = value & SFLAG_SET;
	FLAG_M = value & MFLAG_SET;
}


/* Set the condition code register */
static inline void m68ki_set_ccr(m68ki_cpu_core *state, uint value)
{
	FLAG_X = BIT_4(value)  << 4;
	FLAG_N = BIT_3(value)  << 4;
	FLAG_Z = !BIT_2(value);
	FLAG_V = BIT_1(value)  << 6;
	FLAG_C = BIT_0(value)  << 8;
}

/* Set the status register but don't check for interrupts */
static inline void m68ki_set_sr_noint(m68ki_cpu_core *state, uint value)
{
	/* Mask out the "unimplemented" bits */
	value &= CPU_SR_MASK;

	/* Now set the status register */
	FLAG_T1 = BIT_F(value);
	FLAG_T0 = BIT_E(value);
	FLAG_INT_MASK = value & 0x0700;
	m68ki_set_ccr(state, value);
	m68ki_set_sm_flag(state, (value >> 11) & 6);
}

/* Set the status register but don't check for interrupts nor
 * change the stack pointer
 */
static inline void m68ki_set_sr_noint_nosp(m68ki_cpu_core *state, uint value)
{
	/* Mask out the "unimplemented" bits */
	value &= CPU_SR_MASK;

	/* Now set the status register */
	FLAG_T1 = BIT_F(value);
	FLAG_T0 = BIT_E(value);
	FLAG_INT_MASK = value & 0x0700;
	m68ki_set_ccr(state, value);
	m68ki_set_sm_flag_nosp(state, (value >> 11) & 6);
}

static inline void m68ki_exception_interrupt(m68ki_cpu_core *state, uint int_level);
extern volatile int g_irq;
extern bool RTG_enabled;
/* Set the status register and check for interrupts */
static inline void m68ki_set_sr(m68ki_cpu_core *state, uint value)
{
	m68ki_set_sr_noint(state, value);
	
	//m68ki_check_interrupts(state); // cryptodad commented out for performance 

	//if ( RTG_enabled )
	//	m68ki_exception_interrupt ( state, 0 ); // cryptodad use this instead AND ONLY with 
											// emulator.c m68k_execute_bef () - if ( GET_CYCLES () < 1 || g_irq )
}


/* ------------------------- Exception Processing ------------------------- */

/* Initiate exception processing */
static inline uint m68ki_init_exception(m68ki_cpu_core *state)
{
	/* Save the old status register */
	uint sr = m68ki_get_sr();

	/* Turn off trace flag, clear pending traces */
	FLAG_T1 = FLAG_T0 = 0;
	m68ki_clear_trace();
	/* Enter supervisor mode */
	m68ki_set_s_flag(state, SFLAG_SET);

	return sr;
}

/* 3 word stack frame (68000 only) */
static inline void m68ki_stack_frame_3word(m68ki_cpu_core *state, uint pc, uint sr)
{
	m68ki_push_32(state, pc);
	m68ki_push_16(state, sr);
}

/* Format 0 stack frame.
 * This is the standard stack frame for 68010+.
 */
static inline void m68ki_stack_frame_0000(m68ki_cpu_core *state, uint pc, uint sr, uint vector)
{
	/* Stack a 3-word frame if we are 68000 */
	if(CPU_TYPE == CPU_TYPE_000)
	{
		m68ki_stack_frame_3word(state, pc, sr);
		return;
	}
	m68ki_push_16(state, vector << 2);
	m68ki_push_32(state, pc);
	m68ki_push_16(state, sr);
}

/* Format 1 stack frame (68020).
 * For 68020, this is the 4 word throwaway frame.
 */
static inline void m68ki_stack_frame_0001(m68ki_cpu_core *state, uint pc, uint sr, uint vector)
{
	m68ki_push_16(state, 0x1000 | (vector << 2));
	m68ki_push_32(state, pc);
	m68ki_push_16(state, sr);
}

/* Format 2 stack frame.
 * This is used only by 68020 for trap exceptions.
 */
static inline void m68ki_stack_frame_0010(m68ki_cpu_core *state, uint sr, uint vector)
{
	m68ki_push_32(state, REG_PPC);
	m68ki_push_16(state, 0x2000 | (vector << 2));
	m68ki_push_32(state, REG_PC);
	m68ki_push_16(state, sr);
}


/* Bus error stack frame (68000 only).
 */
static inline void m68ki_stack_frame_buserr_orig(m68ki_cpu_core *state, uint sr)
//static inline void m68ki_stack_frame_buserr ( m68ki_cpu_core *state, uint sr )
{
	//printf("m68k_stack_frame_buserr()\n");
	//printf("Pushing REG_PC (%x)\n", REG_PC );
	//printf("Pushing sr (%x)\n", sr );
	//printf("Pushing REG_IR (%x)\n", REG_IR );
	//printf("Pushing m68ki_aerr_address (%x)\n", m68ki_aerr_address  );

	m68ki_push_32(state, REG_PC);
	m68ki_push_16(state, sr);
	m68ki_push_16(state, REG_IR);
	m68ki_push_32(state, m68ki_aerr_address);	/* access address */
	/* 0 0 0 0 0 0 0 0 0 0 0 R/W I/N FC
	 * R/W  0 = write, 1 = read
	 * I/N  0 = instruction, 1 = not
	 * FC   3-bit function code
	 */
	m68ki_push_16(state, m68ki_aerr_write_mode | CPU_INSTR_MODE | m68ki_aerr_fc);
}

#define IDLE_DEBUG //printf
#if (1)
static inline void m68ki_stack_frame_buserr(m68ki_cpu_core *state, uint sr)
{


    uint32 stacked_pc;
//    IDLE_INIT_FUNC("m68ki_stack_frame_buserr()");
    switch (REG_IR) {
    // Some instruction are special when a bus or address error occurs
    // since the next opcode is (pre)fetched, the PC value is near the JMP, JSR or RTS
    // and not near the destination address. 
    // The REG_PPPC macro is the previous instruction (always valid)
    // The REG_PPC is the current instruction address
    // The REG_PC is more or less the PC value (but wrong since prefetch is not emulated)


    // these are the opcodes that were controled on real hardware

    // RTS
    case 0x4E75:                                
   	    stacked_pc=(REG_PPC+2)&CPU_ADDRESS_MASK;
	    break;
    // JMP immediate32
    case 0x4EF9:
	    stacked_pc=(REG_PPC+2)&CPU_ADDRESS_MASK;
	    break;

    // these are the theorical opcodes (may be wrong)
    // (this one in particular (see german version of LOS2.0))
    // JMP (Reg)
    case 0x4ED0:
    case 0x4ED1:
    case 0x4ED2:
    case 0x4ED3:
    case 0x4ED4:
    case 0x4ED5:
    case 0x4ED6:
    case 0x4ED7:
    case 0x4ED8:
    case 0x4ED9:
    case 0x4EDa:
    case 0x4EDb:
    case 0x4EDc:
    case 0x4EDd:
    case 0x4EDe:
    case 0x4EDf:
  	    stacked_pc=(REG_PPC+2)&CPU_ADDRESS_MASK;
   	    break;
        // RTE
    case 0x4E73:
  	    stacked_pc=(REG_PPC+2)&CPU_ADDRESS_MASK;
   	    break;
	    // JSR 
	    // when a bus error occur on JSR long opcode, the stack is not modified
	    // whe should need to compensate
    case 0x4Eb9:                                
    case 0x4E90:
    case 0x4Ea8:
        stacked_pc=(REG_PPC+2)&CPU_ADDRESS_MASK;
   	    // correct the stack (user or supervisor)
   	    if (sr&0x2000) {
             	m68ki_fake_pull_32(state);
                IDLE_DEBUG("correct SSP for RTS val=%06x\n",m68ki_cpu.dar[15]);
              }
       	else {
       	    m68ki_cpu.sp[0]+=4;
            IDLE_DEBUG("correct USP for RTS val=%06x\n",m68ki_cpu.sp[0]);
        }	  
	    break;

	case 0x4A2F:                                
        stacked_pc=(REG_PPC+2)&CPU_ADDRESS_MASK;
		break;
    default:
	        stacked_pc=(REG_PC)&CPU_ADDRESS_MASK;
    }
    m68ki_push_32(state, stacked_pc);IDLE_DEBUG("stacking PC x%08x\n",stacked_pc);
	m68ki_push_16(state, sr);IDLE_DEBUG("stacking SR x%04x\n",sr);
	m68ki_push_16(state, REG_IR);IDLE_DEBUG("stacking IR x%04x\n",REG_IR);
	m68ki_push_32(state, m68ki_aerr_address);	/* access address */
	IDLE_DEBUG("stacking AERR x%08x\n",m68ki_aerr_address);
	/* 0 0 0 0 0 0 0 0 0 0 0 R/W I/N FC
	 * R/W  0 = write, 1 = read
	 * I/N  0 = instruction, 1 = not
	 * FC   3-bit function code
	 */
	m68ki_push_16(state, m68ki_aerr_write_mode | CPU_INSTR_MODE | m68ki_aerr_fc);
	IDLE_DEBUG("stacking INFO x%04x\n",m68ki_aerr_write_mode | CPU_INSTR_MODE | m68ki_aerr_fc);
    IDLE_DEBUG("NEW STACK x%08x\n",REG_SP);
}	
#endif



/* Format 8 stack frame (68010).
 * 68010 only.  This is the 29 word bus/address error frame.
 */
static inline void m68ki_stack_frame_1000(m68ki_cpu_core *state, uint pc, uint sr, uint vector)
{
	/* VERSION
	 * NUMBER
	 * INTERNAL INFORMATION, 16 WORDS
	 */
	m68ki_fake_push_32(state);
	m68ki_fake_push_32(state);
	m68ki_fake_push_32(state);
	m68ki_fake_push_32(state);
	m68ki_fake_push_32(state);
	m68ki_fake_push_32(state);
	m68ki_fake_push_32(state);
	m68ki_fake_push_32(state);

	/* INSTRUCTION INPUT BUFFER */
	m68ki_push_16(state, 0);

	/* UNUSED, RESERVED (not written) */
	m68ki_fake_push_16(state);

	/* DATA INPUT BUFFER */
	m68ki_push_16(state, 0);

	/* UNUSED, RESERVED (not written) */
	m68ki_fake_push_16(state);

	/* DATA OUTPUT BUFFER */
	m68ki_push_16(state, 0);

	/* UNUSED, RESERVED (not written) */
	m68ki_fake_push_16(state);

	/* FAULT ADDRESS */
	m68ki_push_32(state, 0);

	/* SPECIAL STATUS WORD */
	m68ki_push_16(state, 0);

	/* 1000, VECTOR OFFSET */
	m68ki_push_16(state, 0x8000 | (vector << 2));

	/* PROGRAM COUNTER */
	m68ki_push_32(state, pc);

	/* STATUS REGISTER */
	m68ki_push_16(state, sr);
}

/* Format A stack frame (short bus fault).
 * This is used only by 68020 for bus fault and address error
 * if the error happens at an instruction boundary.
 * PC stacked is address of next instruction.
 */
static inline void m68ki_stack_frame_1010(m68ki_cpu_core *state, uint sr, uint vector, uint pc, uint fault_address)
{
	int orig_rw = state->mmu_tmp_buserror_rw;    // this gets splatted by the following pushes, so save it now
	int orig_fc = state->mmu_tmp_buserror_fc;
	int orig_sz = state->mmu_tmp_buserror_sz;

	/* INTERNAL REGISTER */
	m68ki_push_16(state, 0);

	/* INTERNAL REGISTER */
	m68ki_push_16(state, 0);

	/* DATA OUTPUT BUFFER (2 words) */
	m68ki_push_32(state, 0);

	/* INTERNAL REGISTER */
	m68ki_push_16(state, 0);

	/* INTERNAL REGISTER */
	m68ki_push_16(state, 0);

	/* DATA CYCLE FAULT ADDRESS (2 words) */
	m68ki_push_32(state, fault_address);

	/* INSTRUCTION PIPE STAGE B */
	m68ki_push_16(state, 0);

	/* INSTRUCTION PIPE STAGE C */
	m68ki_push_16(state, 0);

	/* SPECIAL STATUS REGISTER */
	// set bit for: Rerun Faulted bus Cycle, or run pending prefetch
	// set FC
	m68ki_push_16(state, 0x0100 | orig_fc | orig_rw << 6 | orig_sz << 4);

	/* INTERNAL REGISTER */
	m68ki_push_16(state, 0);

	/* 1010, VECTOR OFFSET */
	m68ki_push_16(state, 0xa000 | (vector << 2));

	/* PROGRAM COUNTER */
	m68ki_push_32(state, pc);

	/* STATUS REGISTER */
	m68ki_push_16(state, sr);
}

/* Format B stack frame (long bus fault).
 * This is used only by 68020 for bus fault and address error
 * if the error happens during instruction execution.
 * PC stacked is address of instruction in progress.
 */
static inline void m68ki_stack_frame_1011(m68ki_cpu_core *state, uint sr, uint vector, uint pc, uint fault_address)
{
	int orig_rw = state->mmu_tmp_buserror_rw;    // this gets splatted by the following pushes, so save it now
	int orig_fc = state->mmu_tmp_buserror_fc;
	int orig_sz = state->mmu_tmp_buserror_sz;

	/* INTERNAL REGISTERS (18 words) */
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);

	/* VERSION# (4 bits), INTERNAL INFORMATION */
	m68ki_push_16(state, 0);

	/* INTERNAL REGISTERS (3 words) */
	m68ki_push_32(state, 0);
	m68ki_push_16(state, 0);

	/* DATA INTPUT BUFFER (2 words) */
	m68ki_push_32(state, 0);

	/* INTERNAL REGISTERS (2 words) */
	m68ki_push_32(state, 0);

	/* STAGE B ADDRESS (2 words) */
	m68ki_push_32(state, 0);

	/* INTERNAL REGISTER (4 words) */
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);

	/* DATA OUTPUT BUFFER (2 words) */
	m68ki_push_32(state, 0);

	/* INTERNAL REGISTER */
	m68ki_push_16(state, 0);

	/* INTERNAL REGISTER */
	m68ki_push_16(state, 0);

	/* DATA CYCLE FAULT ADDRESS (2 words) */
	m68ki_push_32(state, fault_address);

	/* INSTRUCTION PIPE STAGE B */
	m68ki_push_16(state, 0);

	/* INSTRUCTION PIPE STAGE C */
	m68ki_push_16(state, 0);

	/* SPECIAL STATUS REGISTER */
	m68ki_push_16(state, 0x0100 | orig_fc | (orig_rw << 6) | (orig_sz << 4));

	/* INTERNAL REGISTER */
	m68ki_push_16(state, 0);

	/* 1011, VECTOR OFFSET */
	m68ki_push_16(state, 0xb000 | (vector << 2));

	/* PROGRAM COUNTER */
	m68ki_push_32(state, pc);

	/* STATUS REGISTER */
	m68ki_push_16(state, sr);
}

/* Type 7 stack frame (access fault).
 * This is used by the 68040 for bus fault and mmu trap
 * 30 words
 */
static inline void
m68ki_stack_frame_0111(m68ki_cpu_core *state, uint sr, uint vector, uint pc, uint fault_address, uint8 in_mmu)
{
	int orig_rw = state->mmu_tmp_buserror_rw;    // this gets splatted by the following pushes, so save it now
	int orig_fc = state->mmu_tmp_buserror_fc;

	/* INTERNAL REGISTERS (18 words) */
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);
	m68ki_push_32(state, 0);

	/* FAULT ADDRESS (2 words) */
	m68ki_push_32(state, fault_address);

	/* INTERNAL REGISTERS (3 words) */
	m68ki_push_32(state, 0);
	m68ki_push_16(state, 0);

	/* SPECIAL STATUS REGISTER (1 word) */
	m68ki_push_16(state, (in_mmu ? 0x400 : 0) | orig_fc | (orig_rw << 8));

	/* EFFECTIVE ADDRESS (2 words) */
	m68ki_push_32(state, fault_address);

	/* 0111, VECTOR OFFSET (1 word) */
	m68ki_push_16(state, 0x7000 | (vector << 2));

	/* PROGRAM COUNTER (2 words) */
	m68ki_push_32(state, pc);

	/* STATUS REGISTER (1 word) */
	m68ki_push_16(state, sr);
}

/* Used for Group 2 exceptions.
 * These stack a type 2 frame on the 020.
 */
static inline void m68ki_exception_trap(m68ki_cpu_core *state, uint vector)
{
	uint sr = m68ki_init_exception(state);

	if(CPU_TYPE_IS_010_LESS(CPU_TYPE))
		m68ki_stack_frame_0000(state, REG_PC, sr, vector);
	else
		m68ki_stack_frame_0010(state, sr, vector);

	m68ki_jump_vector(state, vector);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[vector] - CYC_INSTRUCTION[REG_IR]);
}

/* Trap#n stacks a 0 frame but behaves like group2 otherwise */
static inline void m68ki_exception_trapN(m68ki_cpu_core *state, uint vector)
{
	uint sr = m68ki_init_exception(state);
	m68ki_stack_frame_0000(state, REG_PC, sr, vector);
	m68ki_jump_vector(state, vector);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[vector] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for trace mode */
static inline void m68ki_exception_trace(m68ki_cpu_core *state)
{
	uint sr = m68ki_init_exception(state);

	if(CPU_TYPE_IS_010_LESS(CPU_TYPE))
	{
		#if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
		if(CPU_TYPE_IS_000(CPU_TYPE))
		{
			CPU_INSTR_MODE = INSTRUCTION_NO;
		}
		#endif /* M68K_EMULATE_ADDRESS_ERROR */
		m68ki_stack_frame_0000(state, REG_PC, sr, EXCEPTION_TRACE);
	}
	else
		m68ki_stack_frame_0010(state, sr, EXCEPTION_TRACE);

	m68ki_jump_vector(state, EXCEPTION_TRACE);

	/* Trace nullifies a STOP instruction */
	CPU_STOPPED &= ~STOP_LEVEL_STOP;

	/* Use up some clock cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_TRACE]);
}

/* Exception for privilege violation */
static inline void m68ki_exception_privilege_violation(m68ki_cpu_core *state)
{
	uint sr = m68ki_init_exception(state);

	#if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
	if(CPU_TYPE_IS_000(CPU_TYPE))
	{
		CPU_INSTR_MODE = INSTRUCTION_NO;
	}
	#endif /* M68K_EMULATE_ADDRESS_ERROR */

	m68ki_stack_frame_0000(state, REG_PPC, sr, EXCEPTION_PRIVILEGE_VIOLATION);
	m68ki_jump_vector(state, EXCEPTION_PRIVILEGE_VIOLATION);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_PRIVILEGE_VIOLATION] - CYC_INSTRUCTION[REG_IR]);
}

//extern jmp_buf m68ki_bus_error_jmp_buf;

//#define m68ki_check_bus_error_trap() setjmp(m68ki_bus_error_jmp_buf)

/* Exception for bus error */
static inline void m68ki_exception_bus_error(m68ki_cpu_core *state)
{
	//int i;
	/* If we were processing a bus error, address error, or reset,
	 * this is a catastrophic failure.
	 * Halt the CPU
	 */
	/*
	if(CPU_RUN_MODE == RUN_MODE_BERR_AERR_RESET)
	{
		m68k_read_memory_8(0x00ffff01);
		CPU_STOPPED = STOP_LEVEL_HALT;
		return;
	}
	*/
/*
	if(CPU_RUN_MODE != RUN_MODE_NORMAL ) {
    	printf("Double fault!\n"); 
		CPU_STOPPED = STOP_LEVEL_HALT;
		return;		
	}
	CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET;
*/
	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_BUS_ERROR] - CYC_INSTRUCTION[REG_IR]);

	//for (i = 15; i >= 0; i--){
	//	REG_DA[i] = REG_DA_SAVE[i];
	//}

	uint sr = m68ki_init_exception(state);
	//m68ki_stack_frame_1000(state, REG_PPC, sr, EXCEPTION_BUS_ERROR); // 68010 only
	m68ki_stack_frame_buserr(state, sr);

	m68ki_jump_vector ( state, EXCEPTION_BUS_ERROR );
	//longjmp(m68ki_bus_error_jmp_buf, 1);
	//longjmp(m68ki_aerr_trap, 2);
}

extern int cpu_log_enabled;

/* Exception for A-Line instructions */
static inline void m68ki_exception_1010(m68ki_cpu_core *state)
{
	uint sr;
#if M68K_LOG_1010_1111 == OPT_ON
	M68K_DO_LOG_EMU((M68K_LOG_FILEHANDLE "%s at %08x: called 1010 instruction %04x (%s)\n",
					 m68ki_cpu_names[CPU_TYPE], ADDRESS_68K(REG_PPC), REG_IR,
					 m68ki_disassemble_quick(ADDRESS_68K(REG_PPC),CPU_TYPE)));
#endif

	sr = m68ki_init_exception(state);
	m68ki_stack_frame_0000(state, REG_PPC, sr, EXCEPTION_1010);
	m68ki_jump_vector(state, EXCEPTION_1010);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_1010] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for F-Line instructions */
static inline void m68ki_exception_1111(m68ki_cpu_core *state)
{
	uint sr;

#if M68K_LOG_1010_1111 == OPT_ON
	M68K_DO_LOG_EMU((M68K_LOG_FILEHANDLE "%s at %08x: called 1111 instruction %04x (%s)\n",
					 m68ki_cpu_names[CPU_TYPE], ADDRESS_68K(REG_PPC), REG_IR,
					 m68ki_disassemble_quick(ADDRESS_68K(REG_PPC),CPU_TYPE)));
#endif

	sr = m68ki_init_exception(state);
	m68ki_stack_frame_0000(state, REG_PPC, sr, EXCEPTION_1111);
	m68ki_jump_vector(state, EXCEPTION_1111);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_1111] - CYC_INSTRUCTION[REG_IR]);
}

#if M68K_ILLG_HAS_CALLBACK == OPT_SPECIFY_HANDLER
extern int m68ki_illg_callback(int);
#endif

/* Exception for illegal instructions */
static inline void m68ki_exception_illegal(m68ki_cpu_core *state)
{
	uint sr;

	M68K_DO_LOG((M68K_LOG_FILEHANDLE "%s at %08x: illegal instruction %04x (%s)\n",
				 m68ki_cpu_names[CPU_TYPE], ADDRESS_68K(REG_PPC), REG_IR,
				 m68ki_disassemble_quick(ADDRESS_68K(REG_PPC),CPU_TYPE)));
	if (m68ki_illg_callback(REG_IR))
	    return;

	sr = m68ki_init_exception(state);

	#if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
	if(CPU_TYPE_IS_000(CPU_TYPE))
	{
		CPU_INSTR_MODE = INSTRUCTION_NO;
	}
	#endif /* M68K_EMULATE_ADDRESS_ERROR */

	m68ki_stack_frame_0000(state, REG_PPC, sr, EXCEPTION_ILLEGAL_INSTRUCTION);
	m68ki_jump_vector(state, EXCEPTION_ILLEGAL_INSTRUCTION);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_ILLEGAL_INSTRUCTION] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for format errror in RTE */
static inline void m68ki_exception_format_error(m68ki_cpu_core *state)
{
	uint sr = m68ki_init_exception(state);
	m68ki_stack_frame_0000(state, REG_PC, sr, EXCEPTION_FORMAT_ERROR);
	m68ki_jump_vector(state, EXCEPTION_FORMAT_ERROR);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_FORMAT_ERROR] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for address error */
static inline void m68ki_exception_address_error(m68ki_cpu_core *state)
{
	uint32 sr = m68ki_init_exception(state);

	/* If we were processing a bus error, address error, or reset,
	 * this is a catastrophic failure.
	 * Halt the CPU
	 */
	if(CPU_RUN_MODE == RUN_MODE_BERR_AERR_RESET_WSF)
	{
		m68k_read_memory_8(0x00ffff01); /* cryptodad - what is this supposed to do? */
		CPU_STOPPED = STOP_LEVEL_HALT;
		return;
	}

	CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET_WSF;

	if (CPU_TYPE_IS_000(CPU_TYPE))
	{
		/* Note: This is implemented for 68000 only! */
		m68ki_stack_frame_buserr(state, sr);
	}
	else if (CPU_TYPE_IS_010(CPU_TYPE))
	{
		/* only the 68010 throws this unique type-1000 frame */
		m68ki_stack_frame_1000(state, REG_PPC, sr, EXCEPTION_BUS_ERROR);
	}
	else if (state->mmu_tmp_buserror_address == REG_PPC)
	{
		m68ki_stack_frame_1010(state, sr, EXCEPTION_BUS_ERROR, REG_PPC, state->mmu_tmp_buserror_address);
	}
	else
	{
		m68ki_stack_frame_1011(state, sr, EXCEPTION_BUS_ERROR, REG_PPC, state->mmu_tmp_buserror_address);
	}

	m68ki_jump_vector(state, EXCEPTION_ADDRESS_ERROR);

	state->run_mode = RUN_MODE_BERR_AERR_RESET;

	/* Use up some clock cycles. Note that we don't need to undo the
	instruction's cycles here as we've longjmp:ed directly from the
	instruction handler without passing the part of the excecute loop
	that deducts instruction cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_ADDRESS_ERROR]);
}


/* Service an interrupt request and start exception processing */
static inline void m68ki_exception_interrupt(m68ki_cpu_core *state, uint int_level)
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


	/* cryptodad moved this section here - ignores passed-in int_level */
	if ( state->nmi_pending )
	{
		state->nmi_pending = FALSE;
		int_level = 7;
	}

	else if ( CPU_INT_LEVEL > FLAG_INT_MASK ) 
		int_level = CPU_INT_LEVEL >> 8;

	else
		return;
	/* cryptodad end section */


	/* Acknowledge the interrupt */
	vector = m68ki_int_ack ( int_level );
//printf ("%s: vector = %x, int_level = %d\n", __func__, vector, int_level );

	/* Get the interrupt vector */
	if ( vector == M68K_INT_ACK_AUTOVECTOR )
		/* Use the autovectors.  This is the most commonly used implementation */
		vector = EXCEPTION_INTERRUPT_AUTOVECTOR+int_level;
	else if(vector == M68K_INT_ACK_SPURIOUS)
		/* Called if no devices respond to the interrupt acknowledge */
		vector = EXCEPTION_SPURIOUS_INTERRUPT;

	/* cryptodad can never get here as vector is masked byte */
	else if ( vector > 255 )
	{
	//	//M68K_DO_LOG_EMU((M68K_LOG_FILEHANDLE "%s at %08x: Interrupt acknowledge returned invalid vector $%x\n",
		printf ( "PC: 0x%X: Interrupt acknowledge returned invalid vector $%x\n",
				 ADDRESS_68K(REG_PC), vector );
		return;
	}

	/* Start exception processing */
	sr = m68ki_init_exception(state);

	/* Set the interrupt mask to the level of the one being serviced */
	FLAG_INT_MASK = int_level << 8;

	/* Get the new PC */
	new_pc = m68ki_read_data_32(state, (vector << 2) + REG_VBR);

	/* If vector is uninitialized, call the uninitialized interrupt vector */
	if(new_pc == 0)
		new_pc = m68ki_read_data_32(state, (EXCEPTION_UNINITIALIZED_INTERRUPT << 2) + REG_VBR);

	/* Generate a stack frame */
	m68ki_stack_frame_0000(state, REG_PC, sr, vector);
	if(FLAG_M && CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
	{
		/* Create throwaway frame */
		m68ki_set_sm_flag(state, FLAG_S);	/* clear M */
		sr |= 0x2000; /* Same as SR in master stack frame except S is forced high */
		m68ki_stack_frame_0001(state, REG_PC, sr, vector);
	}

	m68ki_jump ( state, new_pc );

	/* Defer cycle counting until later */
	USE_CYCLES(CYC_EXCEPTION[vector]);

#if !M68K_EMULATE_INT_ACK
	/* Automatically clear IRQ if we are not using an acknowledge scheme */
	CPU_INT_LEVEL = 0;
#endif /* M68K_EMULATE_INT_ACK */
}



/* ASG: Check for interrupts */
/* 
 * Int 2 = H-BLANK
 * Int 4 = V-BLANK
 * Int 6 = MFP
 */
static inline void m68ki_check_interrupts ( m68ki_cpu_core *state )
{
	//if ( CPU_INT_LEVEL != 0x0600 )
	//	return;

	if ( state->nmi_pending )
	{
		state->nmi_pending = FALSE;
		m68ki_exception_interrupt ( state, 7 );
	}
	
	else if ( CPU_INT_LEVEL > FLAG_INT_MASK ) 
	{
		m68ki_exception_interrupt ( state, CPU_INT_LEVEL >> 8 );
	}

	//else
	//	printf ( "why here? CPU_INT_LEVE: = 0x%X\n", CPU_INT_LEVEL );

	//m68ki_exception_interrupt ( state, CPU_INT_LEVEL >> 8 );
}



/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* M68KCPU__HEADER */
