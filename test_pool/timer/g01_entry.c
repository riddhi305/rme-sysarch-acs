/** @file
 * Copyright ...
 * SPDX-License-Identifier: Apache-2.0
 **/

#include "val/include/val.h"
#include "val/include/val_interface.h"
#include "val/include/val_gic.h"
#include "val/include/val_gic_support.h"
#include "val/include/val_el32.h"
#include "val/include/val_timer.h"

#define TEST_NAME "cntps_secure_timer_irq_check"
#define TEST_DESC "Verify Secure Physical timer (CNTPS) interrupt"
#define TEST_RULE "B_PPI_03"

static volatile uint32_t irq_pending;
static uint32_t cntps_intid;

static 
void 
isr_cntps(void)
{
    irq_pending = 0;
    val_cntps_disable_el3();
    //val_print(ACS_PRINT_INFO, " Received CNTPS interrupt (INTID: %d) ", cntps_intid);
    val_gic_end_of_interrupt(cntps_intid);
}

static 
void 
payload(void)
{
    uint32_t index = val_pe_get_index_mpid(val_pe_get_mpid());
    uint32_t timeout = TIMEOUT_LARGE;

    cntps_intid = val_timer_get_info(TIMER_INFO_SEC_PHY_EL1_INTID, 0);

    if ((cntps_intid < 16 || cntps_intid > 31) && !val_gic_is_valid_eppi(cntps_intid)) {
        val_print(ACS_PRINT_ERR, " CNTPS not mapped to PPI/EPPI range, INTID: %d ", cntps_intid);
        val_set_status(index, "FAIL", 1);
        return;
    }

    if (val_gic_install_isr(cntps_intid, isr_cntps)) {
        val_print(ACS_PRINT_ERR, " GIC Install Handler Failed for INTID: %d ", cntps_intid);
        val_set_status(index, "FAIL", 2);
        return;
    }

    irq_pending = 1;
 
    if (val_cntps_program_el3(1000ULL)) {
        val_print(ACS_PRINT_ERR, " CNTPS program SMC failed ", 0);
        val_set_status(index, "FAIL", 3);
        return;
    }
 
    while ((--timeout > 0) && irq_pending) {
        /* spin */
        ; 
    }

    if (timeout == 0) {
        val_print(ACS_PRINT_ERR, " CNTPS interrupt not received on INTID: %d ", cntps_intid);
        val_cntps_disable_el3();
        val_set_status(index, "FAIL", 4);
        return;
    }
 
    val_set_status(index, "PASS", 1);
}

uint32_t 
g01_entry(uint32_t num_pe)
{
  uint32_t status = ACS_STATUS_FAIL;
  num_pe = 1;

  status = val_initialize_test(TEST_NAME, TEST_DESC, num_pe, TEST_RULE);
  if (status != ACS_STATUS_SKIP)
    val_run_test_payload(num_pe, payload, 0);

  status = val_check_for_error(num_pe);
  val_report_status(0, "END");
  return status;
}
