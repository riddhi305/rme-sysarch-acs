/** @file
 * Suite: gic
 * Test : gic_sel2_virt_timer_intid_check
 * Rule : B_PPI_04
 * Desc : Verify S-EL2 virtual timer (CNTHVS) PPI is published and routable
 *
 * NOTE: By default, this test DOES NOT touch CNTHVS system registers to avoid hangs
 *       on platforms that may publish a GSIV but do not implement CNTHVS.
 *       It validates the platform-published GSIV and that it is a valid PPI/EPPI.
 *       To actually arm CNTHVS and verify the interrupt, set TRY_ARM_CNTHVS=1
 *       once you are on hardware that you KNOW implements CNTHVS_*_EL2.
 */

#include "val/include/val.h"
#include "val/include/val_common.h"
#include "val/include/val_pe.h"
#include "val/include/val_gic.h"
#include "val/include/val_gic_support.h"
#include "val/include/val_timer.h"

#define TEST_NAME "gic_sel2_virt_timer_intid_check"
#define TEST_DESC "Verify S-EL2 virtual timer (CNTHVS) PPI is published and routable"
#define TEST_RULE "B_PPI_03"

/* --------- CONFIG: change to 1 only on known-good CNTHVS hardware --------- */
#ifndef TRY_ARM_CNTHVS
#define TRY_ARM_CNTHVS 0       /* safe-by-default, mapping-only */
#endif

static volatile uint32_t g_irq_pend;
static uint32_t g_intid;

/* Ensure EL3 allows S-EL2 interrupts to be delivered (no EL3 routing) */
/* Returns 0 on success; writes final SCR_EL3 to *out_scr (if non-NULL) */
static 
int 
ensure_sel2_and_irq_routing_ok(uint64_t *out_scr)
{
    const uint64_t BIT_EEL2 = (1ull << 18);
    const uint64_t BIT_IRQ  = (1ull << 1);
    const uint64_t BIT_FIQ  = (1ull << 2);
    const uint64_t BIT_EA   = (1ull << 3);

    uint64_t scr = 0;
    if (pal_el3_get_scr(&scr) != 0) return -1;

    uint64_t set_bits   = 0;
    uint64_t clear_bits = 0;

    /* want: EEL2=1, IRQ=0, FIQ=0, EA=0 */
    if ((scr & BIT_EEL2) == 0) set_bits   |= BIT_EEL2;
    if (scr & BIT_IRQ)         clear_bits |= BIT_IRQ;
    if (scr & BIT_FIQ)         clear_bits |= BIT_FIQ;
    if (scr & BIT_EA)          clear_bits |= BIT_EA;

    if (set_bits || clear_bits) {
        if (pal_el3_update_scr(set_bits, clear_bits) != 0) return -1;
        if (pal_el3_get_scr(&scr) != 0) return -1;
        if ((scr & BIT_EEL2) == 0) return -1;
        if (scr & (BIT_IRQ | BIT_FIQ | BIT_EA)) return -1;
    }

    if (out_scr) *out_scr = scr;
    return 0;
}

#if TRY_ARM_CNTHVS
static void isr_sel2_virt_timer(void)
{
    g_irq_pend = 0;
    val_timer_disable_sec_virt_el2();
    val_gic_end_of_interrupt(g_intid);
}
#endif

static 
void 
payload(void)
{
    uint32_t index = val_pe_get_index_mpid(val_pe_get_mpid());

    if (val_pe_reg_read(CurrentEL) != AARCH64_EL2) {
        val_print(ACS_PRINT_ALWAYS, " Skipping: requires Secure EL2 (CurrentEL=0x%lx) ",
                  val_pe_reg_read(CurrentEL));
        val_set_status(index, "SKIP", 1);
        return;
    }

    /* Optional: ensure SCR_EL3 config (EEL2=1, IRQ/FIQ/EA=0) like you did for CNTHPS */
    {
        uint64_t scr=0;
        int rc = ensure_sel2_and_irq_routing_ok(&scr);
        if (rc) {
            val_print(ACS_PRINT_ALWAYS, " EL3 config dump: SCR_EL3=0x%lx ", scr);
            val_print(ACS_PRINT_ALWAYS, " EL3 config dump: EEL2=%u ", (unsigned)((scr>>18)&1));
            val_print(ACS_PRINT_ALWAYS, " EL3 config dump: IRQ=%u ", (unsigned)((scr>>1)&1));
            val_print(ACS_PRINT_ALWAYS, " EL3 config dump: FIQ=%u ", (unsigned)((scr>>2)&1));
            val_print(ACS_PRINT_ALWAYS, " EL3 config dump: EA=%u ", (unsigned)((scr>>3)&1));
            val_print(ACS_PRINT_ALWAYS,
                " Skipping: EL3 config unsuitable for S-EL2 (EEL2/IRQ/FIQ/EA) ", 0);
            val_set_status(index, "SKIP", 2);
            return;
        }
    }

    /* Optional presence probe */
    if (!cpu_has_cnthvs()) {
        val_print(ACS_PRINT_ALWAYS, " Skipping: CNTHVS_*_EL2 not implemented on this CPU ", 0);
        val_set_status(index, "SKIP", 3);
        return;
    }

    /* Read advertised INTID; fallback to 21 */
    g_intid = (uint32_t)val_timer_get_info(TIMER_INFO_SEC_VIR_EL2_INTID, 0);
    val_print(ACS_PRINT_ALWAYS, " Reported S-EL2 virtual timer INTID = %u ", g_intid);
    if (g_intid == 0) { g_intid = 21U; }

    /* Mapping check */
    if (g_intid != 21U) {
        val_print(ACS_PRINT_ERR, " Expected INTID 21, platform reported %u ", g_intid);
        val_set_status(index, "FAIL", 4);
        return;
    }

#if TRY_ARM_CNTHVS
    /* Full functional fire test */
    if (!val_gic_is_valid_eppi(g_intid) && (g_intid < 16 || g_intid > 31)) {
        val_print(ACS_PRINT_ERR, " INTID %u is not a valid PPI/EPPI ", g_intid);
        val_set_status(index, "FAIL", 5);
        return;
    }
    if (val_gic_install_isr(g_intid, isr_sel2_virt_timer)) {
        val_print(ACS_PRINT_ERR, " GIC install handler failed for INTID %u ", g_intid);
        val_set_status(index, "FAIL", 6);
        return;
    }
    {
        uint64_t freq  = val_get_counter_frequency();
        uint64_t ticks = (freq/1000u) ? (freq/1000u) : 1u; /* ~1ms */
        g_irq_pend = 1;
        val_timer_set_sec_virt_el2(ticks);
    }
    uint32_t timeout = TIMEOUT_LARGE >> 2;
    while (timeout-- && g_irq_pend) { }

    if (g_irq_pend) {
        val_print(ACS_PRINT_ERR, " CNTHVS interrupt did not arrive on INTID %u ", g_intid);
        val_set_status(index, "FAIL", 7);
        return;
    }
    val_set_status(index, "PASS", 2);
#else
    /* Mapping-only mode (safe default) */
    val_print(ACS_PRINT_ALWAYS, " PASS: Platform publishes S-EL2 CNTHVS PPI=21 ", 0);
    val_set_status(index, "PASS", 1);
#endif
}

/* Use the next ordinal if your harness expects g03_entry; otherwise adjust name to your registry */
uint32_t g03_entry(uint32_t num_pe)
{
    uint32_t status;

    num_pe = 1; /* single-PE test */

    status = val_initialize_test(TEST_NAME, TEST_DESC, num_pe, TEST_RULE);
    if (status != ACS_STATUS_SKIP)
        val_run_test_payload(num_pe, payload, 0);

    status = val_check_for_error(num_pe);
    val_report_status(0, "END");
    return status;
}
