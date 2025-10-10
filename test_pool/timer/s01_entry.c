/** @file
 * Check Secure-EL2 SMMU Stage-2 (B_SMMU_25) using EL3 services to read S-banked regs
 */

#include "val/include/val.h"
#include "val/include/val_interface.h"

#include "val/include/val_pe.h"
#include "val/include/val_smmu.h"
#include "val/include/val_iovirt.h"
#include "val/include/val_el32.h"
#include "val/include/val_timer.h"

#define TEST_NAME  "smmu_secure_stage2_el3"
#define TEST_DESC  "Check Secure Stage-2 provided by SMMUv3.2+ (EL3 read of S-bank) "
#define TEST_RULE  "B_SMMU_25"

typedef enum {
  SMMU_REG_BANK_NS = 0,
  SMMU_REG_BANK_S  = 1
} smmu_reg_bank_e;

/* Extract minor version from AIDR (bits[3:0]) */
static inline 
uint32_t 
smmu_aidr_minor(uint64_t aidr) { 
    return (uint32_t)(aidr & 0xF); 
}

/* IDR0.S2P bit (LSB) reports Stage-2 present */
static inline 
uint32_t 
smmu_idr0_s2p(uint64_t idr0) { 
    return (uint32_t)(idr0 & 0x1); 
}

static void payload(void)
{
  uint32_t pe_index = val_pe_get_index_mpid(val_pe_get_mpid());
  uint64_t pfr0 = val_pe_reg_read(ID_AA64PFR0_EL1);
  uint32_t s_el2 = (uint32_t)VAL_EXTRACT_BITS(pfr0, 36, 39);

  if (!s_el2) {
    val_print(ACS_PRINT_ERR, "\n       Secure EL2 not implemented", 0);
    val_set_status(pe_index, "SKIP", 1);
    return;
  }

  /* Discover SMMUs */
  uint32_t num_smmu = val_iovirt_get_smmu_info(SMMU_NUM_CTRL, 0);
  if (num_smmu == 0) {
    val_print(ACS_PRINT_ERR, "\n  No SMMU controllers discovered", 0);
    val_set_status(pe_index, "SKIP", 2);
    return;
  }

  /* Check every controller */
  for (int32_t idx = (int32_t)num_smmu - 1; idx >= 0; --idx) {

    /* --- Non-secure bank: version >= v3.2 and Stage-2 present --- */
    uint32_t major = val_iovirt_get_smmu_info(SMMU_CTRL_ARCH_MAJOR_REV, idx);
    if (major < 3) {
      val_print(ACS_PRINT_ERR, "\n SMMU%2d  detected; need v3.2+", idx);
      val_print(ACS_PRINT_ERR, "\n v%u detected; need v3.2+", major);
      val_set_status(pe_index, "FAIL", 1);
      return;
    }

    uint64_t aidr_ns = val_smmu_read_cfg(SMMUv3_AIDR, idx);
    uint32_t minor_ns = smmu_aidr_minor(aidr_ns);
    if (minor_ns < 2) {
      val_print(ACS_PRINT_ERR, "\n SMMU%2d detected; need v3.2+", idx);
      val_print(ACS_PRINT_ERR, "\n SMMUv3.%u detected; need v3.2+", minor_ns);
      val_set_status(pe_index, "FAIL", 2);
      return;
    }

    uint64_t idr0_ns = val_smmu_read_cfg(SMMUv3_IDR0, idx);
    if (!smmu_idr0_s2p(idr0_ns)) {
      val_print(ACS_PRINT_ERR, "\n SMMU%2d but Stage-2 not supported", idx);
      val_print(ACS_PRINT_ERR, "\n v3.%u but Stage-2 not supported", minor_ns);
      val_set_status(pe_index, "FAIL", 3);
      return;
    }

    /* Secure bank via EL3 SMC: must exist and also indicate S2 present */
    uint64_t aidr_s = val_smmu_read_cfg_el3((uint32_t)idx, SMMUv3_AIDR, SMMU_REG_BANK_S);
    if (aidr_s == 0ULL) {
      val_print(ACS_PRINT_ERR, "\n SMMU%2d: Secure bank AIDR reads as 0 (RAZ/WI) -> no Secure SMMU", idx);
      val_set_status(pe_index, "FAIL", 4);
      return;
    }

    /* If Secure bank exists, it should be v3.2+ as well */
    uint32_t minor_s = smmu_aidr_minor(aidr_s);
    if (minor_s < 2) {
      val_print(ACS_PRINT_ERR, "\n SMMU%2d need v3.2+", idx);
      val_print(ACS_PRINT_ERR, "\n Secure bank v3.%u; need v3.2+", minor_s);
      val_set_status(pe_index, "FAIL", 5);
      return;
    }

    uint64_t idr0_s = val_smmu_read_cfg_el3((uint32_t)idx, SMMUv3_IDR0, SMMU_REG_BANK_S);
    if (!smmu_idr0_s2p(idr0_s)) {
      val_print(ACS_PRINT_ERR, "\n SMMU%2d: Secure bank present but Stage-2 not supported", idx);
      val_set_status(pe_index, "FAIL", 6);
      return;
    }

    /* (Optional) If you expose S_IDR1 via EL3, you can also check SECURE_IMPL bit here.
       If S_IDR1 == 0, that's also a strong indicator of RAZ/WI. */
    // uint64_t idr1_s = val_smmu_read_cfg_el3((uint32_t)idx, SMMUv3_IDR1, SMMU_REG_BANK_S);
    // if (!FIELD_GET(IDR1_SECURE_IMPL, idr1_s)) { ... }
  }

  val_set_status(pe_index, "PASS", 1);
}

uint32_t 
s01_entry(uint32_t num_pe)
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
