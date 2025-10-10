/** @file
 * Copyright (c) 2022-2023, 2025, Arm Limited or its affiliates. All rights reserved.
 * SPDX-License-Identifier : Apache-2.0

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

#define id_aa64mmfr3_el1 S3_0_C0_C7_3
#define sctlr2_el3       S3_6_C1_C0_3
#define mecid_rl_a_el3   S3_6_C10_C10_1

#include "val_el3_helpers.h"
#include "val_el3_memory.h"

        .globl val_el3_read_gpccr_el3
        .globl val_el3_read_gptbr_el3
        .globl val_el3_read_scr_el3
        .globl val_el3_read_tcr_el3
        .globl val_el3_read_ttbr_el3
        .globl val_el3_read_vtcr
        .globl val_el3_read_vttbr
        .globl val_el3_write_vtcr
        .globl val_el3_write_vttbr
        .globl val_el3_read_sctlr_el3
        .globl val_el3_read_sctlr_el2
        .globl val_el3_write_scr_el3
        .globl val_el3_save_vbar_el3
        .globl val_el3_program_vbar_el3
        .globl val_el3_branch_asm
        .globl val_el3_tlbi_paallos
        .globl val_el3_cln_and_invldt_cache
        .globl val_el3_clean_cache
        .globl val_el3_invalidate_cache
        .globl val_el3_at_s1e3w
        .globl val_el3_cmo_cipapa
        .globl val_el3_cmo_cipae
        .globl val_el3_exception_handler_user
        .globl val_el3_asm_eret
        .globl val_el3_asm_eret_smc
        .globl val_el3_read_elr_el3
        .globl val_el3_read_far
        .globl val_el3_read_esr_el3
        .globl val_el3_read_spsr_el3
        .globl val_el3_update_elr_el3
        .globl val_el3_update_spsr_el3
        .globl val_el3_read_sp_el0
        .globl val_el3_tlbi_vae3
        .globl val_el3_tlbi_alle3is
        .globl val_el3_isb
        .globl val_el3_acs_str
        .globl val_el3_acs_ldr_pas_filter
        .globl val_el3_write_mair_el3
        .globl val_el3_read_mair_el3
        .globl val_el3_write_cpsr
        .globl val_el3_read_cpsr
        .globl val_el3_set_daif
        .globl val_el3_mmio_read
        .globl val_el3_mmio_read64
        .globl val_el3_mmio_write64
        .globl val_el3_mmio_write
        .global val_el3_mem_barrier
        .globl val_el3_read_sctlr2_el3
        .globl val_el3_write_sctlr2_el3
        .globl val_el3_read_id_aa64mmfr3_el1
        .globl val_el3_write_mecid_rl_a_el3
        .globl val_el3_read_mecid_rl_a_el3
        .globl el3_cntps_program_ticks
        .globl el3_cntps_disable
        .globl el3_enable_irq_trap
        .globl el3_gic_cpuif_enable_grp0
        .globl read_icc_iar0_el1
        .globl write_icc_eoir0_el1
        .globl el3_handle_scr_smc
        .globl read_icc_sre_el3


        /* @brief  The function is called to prepare the ack_handler entry installed in
         *         vbar_el3 by saving 10 GPRs and SP_EL3 in SP_EL0 which is programmed
         *         to ACS_EL3_HANDLER_SAVED_POINTER address.
         * @param  None
         * @return None
         */
        .macro prepare_ack_handler_entry
           mrs    x8, sp_el0
           mov    x9, sp
           msr    spsel, #0
           ldr    x10, =ACS_EL3_HANDLER_SAVED_POINTER
           mov    sp, x10
           stp    x0,  x1,  [sp, #-0x10]!
           stp    x2,  x3,  [sp, #-0x10]!
           stp    x4,  x5,  [sp, #-0x10]!
           stp    x6,  x7,  [sp, #-0x10]!
           stp    x8,  x9,  [sp, #-0x10]!
        .endm

        /* @brief  The function is called to save the tf-a handler entry address in
         *         (ARM_TF_SHARED_ADDRESS + 8Byte).
         */
        .macro save_firmware_handler_entry_addr offset:req
            ldr    x10, =ARM_TF_SHARED_ADDRESS
            ldr    x11, [x10]
            mov    x12, #\offset
            add    x11, x11, x12
            str    x11, [x10, #0x8]
        .endm

        /* @brief  The function is called to set the SP_EL3 register to use ACS_EL3_STACK
         *         address before branching.
         * @param  None
         * @return None
         */
        .macro set_ack_el3_stack
           msr    spsel, #1
           ldr    x4, =ACS_EL3_STACK
           mov    sp, x4
        .endm

/**
 * @brief  This function is called when the MUT is to be accessed by
 *         storing operation and recovers the SP_EL3, SP_EL0 and 10
 *         GPRS and updates ELR_EL3 and SPSR_EL3.
 * @param1 Virtual address on which the Data is stored.
 * @param2 Data to be stored.
 * @return None
*/
val_el3_acs_str:
       msr    spsel, #0
       ldp    x10,  x11,  [sp],#0x10
       ldp    x6,  x7,  [sp],#0x10
       ldp    x4,  x5,  [sp],#0x10
       ldp    x2,  x3,  [sp],#0x10
       ldp    x8,  x9,  [sp],#0x10
       msr    spsel, #1
       mov    sp, x11
       msr    sp_el0, x10

       str    x1, [x0]
       dsb    sy
       ldr    x0, =PLAT_SHARED_ADDRESS
       ldr    x1, [x0, #0x8]
       ldr    x2, [x0, #0x10]
       msr    elr_el3, x1
       msr    spsr_el3, x2
       isb
       eret

/* @brief  The function is called to mask all the exceptions
 *         in the PSTATE.EL
 * @param  None
 * @return None
 */
val_el3_set_daif:
        mov  x0, #0x3c0
        msr  daif,x0
        isb
        ret

/* @brief  The function is called when the MUT protected by
 *         pas_filter is accessed by loading operation and
 *         recovers the SP_EL3, SP_EL0 and 10 GPRS and updates
 *         ELR_EL3 and SPSR_EL3.
 * @param1 Virtual address to be loaded.
 * @return Data loaded from the VA.
 */
val_el3_acs_ldr_pas_filter:
       msr    spsel, #0
       ldp    x10,  x11,  [sp],#0x10
       ldp    x6,  x7,  [sp],#0x10
       ldp    x4,  x5,  [sp],#0x10
       ldp    x2,  x3,  [sp],#0x10
       ldp    x8,  x9,  [sp],#0x10
       msr    spsel, #1
       mov    sp, x11
       msr    sp_el0, x10

       ldr    x1, [x0]
       dsb    sy
       ldr    x0, =PLAT_SHARED_ADDRESS
       ldr    x2, [x0, #0x8]
       ldr    x3, [x0, #0x10]
       msr    elr_el3, x2
       msr    spsr_el3, x3
       isb
       eret

/* @brief  TLBI by VA operation for EL3.
 * @param  Virtual Address by which Cached copies are invalidated from TLBs.
 * @return None
 */
val_el3_tlbi_vae3:
       tlbi vae3, x0
       dsb    sy
       isb
       ret

// Invalidate all TLB entries at EL3 (Inner Shareable) and synchronize
val_el3_tlbi_alle3is:
      tlbi alle3is
      dsb sy
      isb
      ret

// Instruction Synchronization Barrier (ISB) wrapper
val_el3_isb:
      isb
      ret

// Returns the SP_EL0 value
val_el3_read_sp_el0:
       msr   spsel, #0
       mov   x0, sp
       msr   spsel, #1
       ret

//Updates the ELR_EL3 with the given value
val_el3_update_elr_el3:
       msr    elr_el3,  x0
       ret

//Updates the SPSR_EL3 with the given value
val_el3_update_spsr_el3:
       msr    spsr_el3,  x0
       ret

// Returns the SPSR_EL3 value
val_el3_read_spsr_el3:
       mrs    x0,  spsr_el3
       ret

// Returns the FAR_EL3 value
val_el3_read_far:
       mrs    x0,  far_el3
       ret

// Returns the ESR_EL3 value
val_el3_read_esr_el3:
       mrs    x0,  esr_el3
       ret

// Returns the ELR_EL3 value
val_el3_read_elr_el3:
       mrs    x0,  elr_el3
       ret

/* @brief  The function is called to eret from SMC
 *         after recovering the SP_EL3 and SP_EL0
 *         and 10 GPRs.
 * @param  None
 * @return None
 */
val_el3_asm_eret_smc:
       msr    spsel, #0
       mrs    x0, elr_el3
       ldp    x8,  x9,  [sp],#0x10
       ldp    x6,  x7,  [sp],#0x10
       ldp    x4,  x5,  [sp],#0x10
       ldp    x2,  x3,  [sp],#0x10
       ldp    x0,  x1,  [sp],#0x10
       msr    spsel, #1
       mov    sp, x9
       msr    sp_el0, x8
       ERET

/* @brief  The function is called to obtain the translated
 *         address of the passed VA.
 * @param  VA whose translation is required.
 * @return Translated address from PAR_EL1 register.
 */
val_el3_at_s1e3w:
       at s1e3w, x0
       isb
       mrs    x0, par_el1
       ret

//Returns the GPCCR_EL3 value
val_el3_read_gpccr_el3:
       mrs    x0, s3_6_c2_c1_6
       ret

//Returns the GPTBR_EL3 value
val_el3_read_gptbr_el3:
         mrs    x0, s3_6_c2_c1_4          // read EL3 GPTBR
         ret

//Retruns the SCR_EL3 value
val_el3_read_scr_el3:
       mrs    x0, scr_el3
       ret

//Updates the SCR_EL3 with the given input value
val_el3_write_scr_el3:
       msr    scr_el3, x0
       isb
       ret

//Updates the SCR_EL3 with the given input value
write_scr_el3:
       msr    scr_el3, x0
       isb
       ret

/* --- Minimal EL3 SMC handlers for SCR_EL3 query/update --- */
/* FIDs:
 *  x0 = 0xC2001101  => GET_SCR_EL3  (returns: x0=0, x1=SCR_EL3)
 *  x0 = 0xC2001102  => UPDATE_SCR_EL3 (x1=set_bits, x2=clear_bits) (returns: x0=0)
 *  else: x0 = -1
 */
        .align  2
        .type   el3_handle_scr_smc, %function
el3_handle_scr_smc:
        /* x0=function ID, x1=set_bits/ret, x2=clear_bits */
        mov     x9, x0

        /* 0xC2001101 => GET_SCR_EL3 */
        mov     x10, #0xC2000000
        mov     x11, #0x1101       
        orr     x10, x10, x11
        cmp     x9,  x10
        b.ne    1f

        mrs     x1, scr_el3         // return SCR_EL3 in x1
        mov     x0, #0              // status = 0
        ret

1:      /* 0xC2001102 => UPDATE_SCR_EL3 */
        mov     x10, #0xC2000000
        mov     x11, #0x1102
        orr     x10, x10, x11
        cmp     x9,  x10
        b.ne    2f

        mrs     x11, scr_el3
        orr     x11, x11, x1        // set bits
        bic     x11, x11, x2        // clear bits
        msr     scr_el3, x11
        isb
        mov     x0, #0              // status = 0
        ret

2:      /* unknown FID */
        mov     x0, #-1
        ret

read_icc_sre_el3:
       mrs     x0, ICC_SRE_EL3
       ret

//Returns the TCR_El3 register value
val_el3_read_tcr_el3:
       mrs    x0, tcr_el3
       ret

//Rteurns the TTBR_EL3 register value
val_el3_read_ttbr_el3:
       mrs    x0, ttbr0_el3
       ret

//Returns the VTTBR_EL2 register value
val_el3_read_vttbr:
       mrs    x0, vttbr_el2
       ret

//Returns the VTCR_EL2 register value
val_el3_read_vtcr:
       mrs    x0, vtcr_el2
       ret

// Updates the VTCR_EL2 register
val_el3_write_vtcr:
       msr    vtcr_el2, x0
       dsb    sy
       isb
       ret

// Updates the VTTBR_EL2 register
val_el3_write_vttbr:
       msr    vttbr_el2, x0
       isb
       ret

//Returns theb SCTLR_EL3 register value
val_el3_read_sctlr_el3:
       mrs    x0, sctlr_el3
       ret

//Returns theb SCTLR_EL2 register value
val_el3_read_sctlr_el2:
       mrs    x0, sctlr_el2
       ret

//Updates the MAIR_EL3 register with the given value
val_el3_write_mair_el3:
       msr    mair_el3, x0
       isb
       ret

//Returns the MAIR_EL3 register value
val_el3_read_mair_el3:
       mrs    x0, mair_el3
       ret

// Read the ID_AA64MMFR3_EL1 system register
val_el3_read_id_aa64mmfr3_el1:
       mrs    x0,  id_aa64mmfr3_el1
       ret

// Read the MECID_RL_A_EL3 system register
val_el3_read_mecid_rl_a_el3:
       mrs    x0, mecid_rl_a_el3
       ret

// Write to the MECID_RL_A_EL3 system register and synchronize
val_el3_write_mecid_rl_a_el3:
       msr    mecid_rl_a_el3, x0
       isb
       ret

// Write to the SCTLR2_EL3 system register and synchronize
val_el3_write_sctlr2_el3:
       msr    sctlr2_el3, x0
       isb
       ret

// Read the SCTLR2_EL3 system register
val_el3_read_sctlr2_el3:
       mrs    x0, sctlr2_el3
       ret

// Saves the VBAR_EL3 register value in the given address
val_el3_save_vbar_el3:
       mrs    x1, vbar_el3
       str    x1, [x0]
       ret

//Programs the VBAR_EL3 with the given input address
val_el3_program_vbar_el3:
       msr    vbar_el3,x0
       isb
       ret

// TLB invalidation of all GPT entries by PA for Outer Shareable Domain
val_el3_tlbi_paallos:
       sys #6, c8, c1, #4  //tlbi paallos
       dsb    sy
       isb
       ret

// Clean and Invalidate data cache by address to Point of Coherency.
val_el3_cln_and_invldt_cache:
       dc civac, x0; // x0 contains VA which is updated
       dsb    sy
       isb
       ret

// Clean data cache by address to Point of Coherency.
val_el3_clean_cache:
       dc cvac, x0
       dsb    ish
       isb
       ret

// Invalidate data cache by address to Point of Coherency.
val_el3_invalidate_cache:
       dc ivac, x0
       dsb    ish
       isb
       ret

// Clean and Invalidate data cache by physical address to the Point of Physical Aliasing.
val_el3_cmo_cipapa:
       sys    #6, c7, c14, #1, x0 /* DC CIPAPA,<Xt> */
       dsb    sy
       isb
       ret

// Clean and Invalidate data cache by physical address to the Point of Encryption.
val_el3_cmo_cipae:
       sys    #4, c7, c14, #0, x0 /* DC CIPAPA,<Xt> */
       dsb    sy
       ret

// MMIO Write (32-bit)
val_el3_mmio_write:
    dsb st             // Data Synchronization Barrier
    str w1, [x0]       // Store 32-bit value (w1) at address (x0)
    dsb st             // Ensure write is completed
    ret

// MMIO Read (32-bit)
val_el3_mmio_read:
    dsb ld             // Data Synchronization Barrier before read
    ldr w0, [x0]       // Load 32-bit value from address (x0) into w0
    dsb ld             // Ensure read is completed
    ret

// MMIO Write (64-bit)
val_el3_mmio_write64:
    dsb st             // Data Synchronization Barrier
    str x1, [x0]       // Store 64-bit value (x1) at address (x0)
    dsb st             // Ensure write is completed
    ret

// MMIO Read (64-bit)
val_el3_mmio_read64:
    dsb ld             // Data Synchronization Barrier before read
    ldr w0, [x0]       // Load 64-bit value from address (x0) into w0
    dsb ld             // Ensure read is completed
    ret

// --- CNTPS helpers (EL3) ------------------------------------------
        .align  2
        .type   el3_cntps_program_ticks, %function
/* int el3_cntps_program_ticks(uint64_t delta_ticks)
 *   CVAL = CNTPCT_EL0 + delta_ticks
 *   CTL  = 1 (ENABLE=1, IMASK=0)
 * Returns 0 on success.
 */
el3_cntps_program_ticks:
        // x0 = delta_ticks
        mrs     x1, CNTPCT_EL0
        add     x1, x1, x0
        msr     CNTPS_CVAL_EL1, x1
        isb
        mov     x2, #1                // ENABLE=1, IMASK=0
        msr     CNTPS_CTL_EL1, x2
        isb
        mov     w0, #0                // return 0
        ret

        .align  2
        .type   el3_cntps_disable, %function
/* int el3_cntps_disable(void)
 *   CTL.ENABLE = 0
 * Returns 0 on success.
 */
el3_cntps_disable:
        mrs     x0, CNTPS_CTL_EL1
        bic     x0, x0, #1            // clear ENABLE
        msr     CNTPS_CTL_EL1, x0
        isb
        mov     w0, #0
        ret

el3_enable_irq_trap:
        mrs     x0, scr_el3
        orr     x0, x0, #(1 << 1)     // SCR_EL3.IRQ = 1 (route IRQs to EL3)
        msr     scr_el3, x0
        isb
        ret

el3_gic_cpuif_enable_grp0:
        mrs     x0, ICC_SRE_EL3
        orr     x0, x0, #1            // enable system register interface
        msr     ICC_SRE_EL3, x0
        isb
        mov     x0, #0xFF
        msr     ICC_PMR_EL1, x0       // unmask priorities
        mov     x0, #1
        msr     ICC_IGRPEN0_EL1, x0   // enable Group0
        isb
        ret

read_icc_iar0_el1:
        mrs     x0, ICC_IAR0_EL1      // read pending Group0 INTID
        ret

write_icc_eoir0_el1:
        msr     ICC_EOIR0_EL1, x0     // EOI Group0 INTID
        isb
        ret

/* ---------------------------------------------------------------------
 * acs_el3_sync_wrapper:
 *  - Reads ESR_EL3, checks EC == 0x17 (SMC64).
 *  - If FID is our SCR service (0xC2001101/0xC2001102), call el3_handle_scr_smc.
 *  - Writes return regs back into the saved frame, then asm_eret_smc.
 *  - Otherwise, tail-call original ack_handler_el3.
 * --------------------------------------------------------------------*/
        .align  4
acs_el3_sync_wrapper:
        /* Check EC bits [31:26] for SMC64 (0x17) */
        mrs     x14, esr_el3
        ubfx    x15, x14, #26, #6
        cmp     x15, #0x17
        b.ne    forward_to_tf

        /* Pull original x0/x1/x2 (SMC64 args) from saved frame:
           prepare_ack_handler_entry pushed:
             base-0x10: x0, base-0x08: x1, base-0x20: x2, ...   */
        ldr     x19, =ACS_EL3_HANDLER_SAVED_POINTER
        ldr     x0,  [x19, #-0x10]   // FID
        ldr     x1,  [x19, #-0x08]   // arg1 (or out for GET)
        ldr     x2,  [x19, #-0x20]   // arg2

        /* FID == 0xC2001101 ? (GET_SCR_EL3) */
        movz    x13, #0x1101
        movk    x13, #0xC200, lsl #16
        cmp     x0,  x13
        b.eq    do_service

        /* FID == 0xC2001102 ? (UPDATE_SCR_EL3) */
        movz    x13, #0x1102
        movk    x13, #0xC200, lsl #16
        cmp     x0,  x13
        b.ne    forward_to_tf

do_service:
        bl      el3_handle_scr_smc   // returns x0=status, x1 may hold SCR for GET

        /* Write returned x0/x1 back into saved frame so caller sees them */
        ldr     x19, =ACS_EL3_HANDLER_SAVED_POINTER
        str     x0,  [x19, #-0x10]   // ret x0
        str     x1,  [x19, #-0x08]   // ret x1 (only meaningful for GET)

        b       val_el3_asm_eret_smc         // restore regs & ERET to lower EL

forward_to_tf:
        b       val_el3_ack_handler      // not our FID: fall through to TF/PSCI/etc.


// Data Synchronization Barrier Across the system
val_el3_mem_barrier:
    dsb sy
    ret

/* @brief  This function is called to branch to firmware
 *         handler after recovering the SP_EL3 and SP_EL0.
 * @param  Pointer to Firmware handler
 * @return None
 */
val_el3_branch_asm:
       mov    x8,  x0
       msr    spsel, #0
       ldp    x10,  x11,  [sp],#0x10
       ldp    x6,  x7,  [sp],#0x10
       ldp    x4,  x5,  [sp],#0x10
       ldp    x2,  x3,  [sp],#0x10
       ldp    x0,  x1,  [sp],#0x10
       msr    spsel, #1
       mov    sp, x11
       msr    sp_el0, x10
       br    x8

/**
 * @brief  This function is called to handle the fault
 *         and move to next instruction and recovers
 *         SP_EL0, SP_EL3 and 10 GPRs.
 * @param  None
 * @return None
 **/
val_el3_asm_eret:
       msr    spsel, #0
       mrs    x0, elr_el3
       add    x0, x0, #4
       msr    elr_el3, x0
       ldp    x8,  x9,  [sp],#0x10
       ldp    x6,  x7,  [sp],#0x10
       ldp    x4,  x5,  [sp],#0x10
       ldp    x2,  x3,  [sp],#0x10
       ldp    x0,  x1,  [sp],#0x10
       msr    spsel, #1
       mov    sp, x9
       msr    sp_el0, x8
       ERET

.align 11, 0
val_el3_exception_handler_user:
       prepare_ack_handler_entry
       save_firmware_handler_entry_addr offset=0x0
       set_ack_el3_stack
       B  val_el3_ack_handler
.align 9, 0
       prepare_ack_handler_entry
       save_firmware_handler_entry_addr offset=0x200
       set_ack_el3_stack
       B  val_el3_ack_handler
.align 9, 0
       prepare_ack_handler_entry
       save_firmware_handler_entry_addr offset=0x400
       set_ack_el3_stack
       B  val_el3_ack_handler
.align 9, 0
       prepare_ack_handler_entry
       save_firmware_handler_entry_addr offset=0x600
       set_ack_el3_stack
       B  val_el3_ack_handler
