/* B_TIME_03 The system counter of the Generic
 * Timer must not roll over inside a 10-year period.
 */

#include "val/include/val.h"
#include "val/include/val_interface.h"
#include "val/include/val_timer.h"
#include "val/include/val_common.h"
#include "val/include/val_std_smc.h"
#include "val/include/val_test_entry.h"
#include "val/include/val_el32.h"

#define TEST_NAME  "sys_counter_no_rollover_10y"
#define TEST_DESC  "System counter must not roll over within 10 years"
#define TEST_RULE  "TIME_03"

#define CNTID_WIDTH_SHIFT   0
#define CNTID_WIDTH_MASK    0x3Fu   /* Width field mask */
#define FALLBACK_WIDTH_BITS_IF_NO_CNTID  64

/* 10 years in seconds (10 * 365.25 days * 24 * 60 * 60 ≈ 315576000 seconds)
 * The value 315576000 is based on the average length of a year (365.25 days).
 */
static const uint64_t TEN_YEARS_S = 315576000ULL;
/* Compute max safe frequency (Hz) for given width */
static 
inline 
uint64_t
fmax_for_width(uint32_t width_bits)
{
   if (width_bits >= 64) {
      // For 64 bits or more, the max value is UINT64_MAX + 1
      return UINT64_MAX / TEN_YEARS_S;
   }
   uint64_t ticks_before_wrap = ((uint64_t)1) << width_bits;
   return ticks_before_wrap / TEN_YEARS_S;
}

/* Read CNTID via MMIO or SMC */
static 
uint32_t
read_cntid_any(uint64_t cnt_ctl_base, uint32_t is_secure, uint32_t *out_cntid)
{
   if (!is_secure && (cnt_ctl_base != 0)) {
      uint64_t addr = cnt_ctl_base + CNTID_OFFSET;
      val_print(ACS_PRINT_DEBUG, " [MMIO] CNTID addr = 0x%lx", (unsigned long)addr);
      uint32_t v = val_mmio_read(addr);
      val_print(ACS_PRINT_DEBUG, " [MMIO] CNTID val  = 0x%x", v);
      *out_cntid = v;
      return 1;
   }

   (void)UserCallSMC(ARM_ACS_SMC_FID, RME_READ_CNTID, cnt_ctl_base, 0, 0);

   if (shared_data && shared_data->status_code == 0) {
      *out_cntid = (uint32_t)shared_data->shared_data_access[0].data;
      val_print(ACS_PRINT_DEBUG, " [SMC] CNTID val  = 0x%x", *out_cntid);
      return 1;
   }
   return 0;
}

/* Get implemented counter width */
static 
uint32_t
get_width_from_any_cntctl(uint32_t *out_width_bits)
{
   uint64_t num = val_timer_get_info(TIMER_INFO_NUM_PLATFORM_TIMERS, 0);

   while (num) {
      num--;
      uint64_t cnt_base_n   = val_timer_get_info(TIMER_INFO_SYS_CNT_BASE_N, num);
      uint64_t cnt_ctl_base = val_timer_get_info(TIMER_INFO_SYS_CNTL_BASE,  num);
      uint32_t is_secure    = (uint32_t)val_timer_get_info(TIMER_INFO_IS_PLATFORM_TIMER_SECURE, num);

      if ((cnt_ctl_base == 0) || (cnt_base_n == 0))
         continue;
      uint32_t cntid = 0;
      if (!read_cntid_any(cnt_ctl_base, is_secure, &cntid))
         continue;
      if (cntid == 0) {
         *out_width_bits = FALLBACK_WIDTH_BITS_IF_NO_CNTID;
         val_print(ACS_PRINT_WARN," CNTID not implemented (RES0) -> fallback width = %u",
                      *out_width_bits);
         return 1;
      }

      uint32_t width = (cntid & CNTID_WIDTH_MASK) >> CNTID_WIDTH_SHIFT;
      if (width == 0) {
         *out_width_bits = FALLBACK_WIDTH_BITS_IF_NO_CNTID;
         val_print(ACS_PRINT_WARN," CNTID.Width == 0 -> fallback width = %u",
                      *out_width_bits);
         return 1;
      }

      if (width < 56) width = 56;
      if (width > 64) width = 64;
      *out_width_bits = width;
      return 1;
   }

   *out_width_bits = FALLBACK_WIDTH_BITS_IF_NO_CNTID;
   val_print(ACS_PRINT_WARN, " No CNTCTLBase readable -> fallback width = %u",
              *out_width_bits);
   return 1;
}

static 
void
payload(void)
{
   uint32_t pe_index = val_pe_get_index_mpid(val_pe_get_mpid());
   val_print(ACS_PRINT_WARN, " PE index: %d", pe_index);

   /* 1) Read frequency */
   uint64_t f_hz = val_timer_get_info(TIMER_INFO_CNTFREQ, 0);
   val_print(ACS_PRINT_DEBUG, " CNTFRQ_EL0 = %ld", (long)f_hz);
   if (!f_hz) {
      val_print(ACS_PRINT_ERR, "\n CNTFRQ_EL0 is zero", 0);
      val_set_status(pe_index, "FAIL", 1);
      return;
   }

   uint64_t disp = f_hz; 
   int freq = 0; // 0=kHz, 1=MHz, 2=GHz
   disp /= 1000; // kHz
   if (disp > 1000) { 
      disp /= 1000; 
      freq = 1; // MHz
      if (disp > 1000) {
         disp /= 1000;
         freq = 2; // GHz
      }
   }

   if (freq == 0)
      val_print(ACS_PRINT_DEBUG, "\n Counter frequency is %ld KHz", (long)disp);
   else if (freq == 1)
      val_print(ACS_PRINT_DEBUG, "\n Counter frequency is %ld MHz", (long)disp);
   else
      val_print(ACS_PRINT_DEBUG, "\n Counter frequency is %ld GHz", (long)disp);

   /* 2) Get width */
   uint32_t width_bits = 0;
   if (!get_width_from_any_cntctl(&width_bits)) {
      val_print(ACS_PRINT_ERR, " Unable to determine counter width", 0);
      val_set_status(pe_index, "FAIL", 1);
      return;
   }
   val_print(ACS_PRINT_DEBUG, " Implemented width (bits): %u", width_bits);

   /* 3) Compute safe limit */
   uint64_t fmax_hz = fmax_for_width(width_bits);
   val_print(ACS_PRINT_DEBUG, " fmax_hz: %u", fmax_hz);
   uint64_t limit_disp = freq ? (fmax_hz / 1000000ULL) : (fmax_hz / 1000ULL);
   val_print(ACS_PRINT_DEBUG, " limit_disp: %u", limit_disp);
   if (f_hz <= fmax_hz) {
      val_set_status(pe_index, "PASS", 1);
      return;
   }

   /* 4) Failure reporting */
   val_print(ACS_PRINT_ERR,  " Using width (bits): %u", width_bits);
   if (freq == 0){
      val_print(ACS_PRINT_ERR,  " Freq (KHz): %ld", (long)disp);
      val_print(ACS_PRINT_ERR,  " 10y safe limit (KHz): %ld", (long)limit_disp);
   }
   else if (freq == 1){
      val_print(ACS_PRINT_ERR,  " Freq (MHz): %ld", (long)disp);
      val_print(ACS_PRINT_ERR,  " 10y safe limit (MHz): %ld", (long)limit_disp);
   }
   else{
      val_print(ACS_PRINT_ERR,  " Freq (GHz): %ld", (long)disp);
      val_print(ACS_PRINT_ERR,  " 10y safe limit (GHz): %ld", (long)limit_disp);
   }
   val_print(ACS_PRINT_ERR,  "\n Counter would wrap in < 10 years", 0);
   val_set_status(pe_index, "FAIL", 2);
}

uint32_t
t02_entry(uint32_t num_pe)
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
