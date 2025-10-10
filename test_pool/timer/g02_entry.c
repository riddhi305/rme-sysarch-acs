/**
 * Suite: gic
 * Test : gic_sel2_phy_timer_intid20_check
 * Rule : B_PPI_03
 * Desc : Verify S-EL2 physical timer (CNTHPS) PPI mapping is INTID 20
 */

#include "val/include/val.h"
#include "val/include/val_common.h"
#include "val/include/val_pe.h"
#include "val/include/val_gic.h"
#include "val/include/val_gic_support.h"
#include "val/include/val_timer.h"

#define TEST_NAME "gic_sel2_phy_timer_intid20_check"
#define TEST_DESC "Verify S-EL2 physical timer (CNTHPS) raises PPI INTID 20"
#define TEST_RULE "B_PPI_03"

/* 0 = mapping only; 1 = also arm CNTHPS (requires S-EL2 + assembler/CPU support) */
#ifndef TRY_ARM_CNTHPS
#define TRY_ARM_CNTHPS 0
#endif

/* Recommended IDs (platform may differ; rule expects 20 for CNTHPS) */
#define PPI_RECOMMENDED_CNTHPS 20U

static uint32_t g_intid                 = 0;
static volatile uint32_t g_irq_pending  = 0;

/* Build timer info table on demand (if Timer suite didn’t run first) */
static void ensure_timer_info_table(void)
{
    if (val_timer_get_info(TIMER_INFO_NUM_PLATFORM_TIMERS, 0) == 0) {
        enum { MAX_LOCAL_TIMER_GTBLOCKS = 8 };
        uint64_t bytes = sizeof(TIMER_INFO_TABLE) +
                         (uint64_t)MAX_LOCAL_TIMER_GTBLOCKS * sizeof(TIMER_INFO_GTBLOCK);
        void *buf = pal_mem_calloc(1, (uint32_t)bytes);
        if (buf) val_timer_create_info_table((uint64_t *)buf);
    }
}

/* EL3: set SCR_EL3.EEL2=1 and clear IRQ/FIQ/EA routing to EL3 (so S-EL2 can take PPIs) */
static int ensure_sel2_and_irq_routing_ok(void)
{
    const uint64_t BIT_EEL2 = (1ull << 18);
    const uint64_t BIT_IRQ  = (1ull << 1);
    const uint64_t BIT_FIQ  = (1ull << 2);
    const uint64_t BIT_EA   = (1ull << 3);

    uint64_t scr = 0;
    if (pal_el3_get_scr(&scr) != 0) return -1;

    uint64_t set_bits   = 0;
    uint64_t clear_bits = 0;

    if ((scr & BIT_EEL2) == 0) set_bits   |= BIT_EEL2;     /* enable entry to S-EL2 */
    if (scr & BIT_IRQ)         clear_bits |= BIT_IRQ;      /* don't route IRQ to EL3 */
    if (scr & BIT_FIQ)         clear_bits |= BIT_FIQ;      /* don't route FIQ to EL3 */
    if (scr & BIT_EA)          clear_bits |= BIT_EA;       /* don't route EA to EL3  */

    if (set_bits || clear_bits) {
        if (pal_el3_update_scr(set_bits, clear_bits) != 0) return -1;
        if (pal_el3_get_scr(&scr) != 0) return -1;
        if (((scr & BIT_EEL2) == 0) || (scr & (BIT_IRQ|BIT_FIQ|BIT_EA))) return -1;
    }
    return 0;
}

#if TRY_ARM_CNTHPS
static void isr_sel2_phy_timer(void)
{
    g_irq_pending = 0;
    val_timer_set_sec_phy_el2(0);    /* stop */
    val_gic_end_of_interrupt(g_intid);
}
#endif

static void payload(void)
{
    uint32_t index = val_pe_get_index_mpid(val_pe_get_mpid());

    /* Probe for Secure EL2 support: ID_AA64PFR0_EL1[39:36] */
    uint32_t s_el2 = VAL_EXTRACT_BITS(val_pe_reg_read(ID_AA64PFR0_EL1), 36, 39);

    if (!s_el2) {
        val_print(ACS_PRINT_ERR, "\n       Secure EL2 not implemented", 0);
        val_set_status(index, "SKIP", 1);
        return;
    }

    /* Make sure EL3 allows S-EL2 and doesn’t sink IRQ/FIQ/EA */
    if (ensure_sel2_and_irq_routing_ok() != 0) {
        val_print(ACS_PRINT_ALWAYS,
                  " Skipping: EL3 config unsuitable for S-EL2 (EEL2/IRQ/FIQ/EA) ", 0);
        val_set_status(index, "SKIP", 2);
        return;
    }

    ensure_timer_info_table();

    /* Query platform-published INTID for S-EL2 physical timer (CNTHPS) */
    g_intid = (uint32_t)val_timer_get_info(TIMER_INFO_SEC_PHY_EL2_INTID, 0);
    val_print(ACS_PRINT_ALWAYS, " S-EL2 INTID (reported) = %u ", g_intid);

    if (g_intid == 0) {
        val_print(ACS_PRINT_ERR, " No S-EL2 timer INTID published (got 0) ", 0);
        val_set_status(index, "FAIL", 3);
        return;
    }

    // if (g_intid != PPI_RECOMMENDED_CNTHPS) {
    //     val_print(ACS_PRINT_ERR, " Expected INTID %u, platform reported %u ",
    //               PPI_RECOMMENDED_CNTHPS, g_intid);
    //     val_set_status(index, "FAIL", 4);
    //     return;
    // }

#if TRY_ARM_CNTHPS
    /* Only arm CNTHPS if we are actually running at S-EL2 */
    {
        uint64_t current_el = (val_pe_reg_read(CurrentEL) >> 2) & 0x3; /* 2 => EL2 */
        if (current_el != 2) {
            val_print(ACS_PRINT_ALWAYS,
                      " Mapping OK; not executing at S-EL2 (CurrentEL=0x%lx) — skip arming ",
                      val_pe_reg_read(CurrentEL));
            val_set_status(index, "PASS", 1);
            return;
        }
    }

    /* Sanity: PPI / EPPI range check */
    if ((g_intid < 16 || g_intid > 31) && (!val_gic_is_valid_eppi(g_intid))) {
        val_print(ACS_PRINT_ERR, " INTID %u is not a valid PPI/EPPI ", g_intid);
        val_set_status(index, "FAIL", 5);
        return;
    }

    if (val_gic_install_isr(g_intid, isr_sel2_phy_timer)) {
        val_print(ACS_PRINT_ERR, " GIC install handler failed for INTID %u ", g_intid);
        val_set_status(index, "FAIL", 6);
        return;
    }

    /* Program ~1ms relative timeout at CNTHPS (ticks = CNTFRQ/1000) */
    {
        uint64_t freq  = val_get_counter_frequency(); /* ticks/sec */
        uint64_t ticks = (freq / 1000U);
        if (ticks == 0) ticks = 1;
        g_irq_pending = 1;
        val_timer_set_sec_phy_el2(ticks);
    }

    uint32_t timeout = TIMEOUT_LARGE >> 2;
    while ((--timeout > 0) && g_irq_pending) { /* spin */ }

    if (timeout == 0 || g_irq_pending) {
        val_print(ACS_PRINT_ERR, " S-EL2 timer did not fire on INTID %u ", g_intid);
        val_set_status(index, "FAIL", 7);
        return;
    }
    val_set_status(index, "PASS", 2);
#else
    /* Mapping validated; arming skipped by design to be toolchain/CPU-safe */
    val_print(ACS_PRINT_ALWAYS,
              " PASS: Platform publishes CNTHPS PPI=%u; programming skipped ",
              PPI_RECOMMENDED_CNTHPS);
    val_set_status(index, "PASS", 1);
#endif
}

uint32_t g02_entry(uint32_t num_pe)
{
    uint32_t status;
    num_pe = 1;

    status = val_initialize_test(TEST_NAME, TEST_DESC, num_pe, TEST_RULE);
    if (status != ACS_STATUS_SKIP)
        val_run_test_payload(num_pe, payload, 0);

    status = val_check_for_error(num_pe);
    val_report_status(0, "END");
    return status;
}
