/** @file
  * Copyright (c) 2022-2025, Arm Limited or its affiliates. All rights reserved.
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
#include <val_el3_debug.h>
#include <val_el3_exception.h>
#include <val_el3_memory.h>
#include <val_el3_pe.h>
#include <val_el3_pgt.h>
#include <val_el3_security.h>
#include <val_el3_smmu.h>
#include <val_el3_wd.h>
#include <val_el3_mec.h>

void plat_arm_acs_smc_handler(uint64_t services, uint64_t arg0, uint64_t arg1, uint64_t arg2);

/**
 *  @brief  This API is used to branch out to all the different functions in EL3
 *          1. Caller       -  Test Suite
 *  @param  services -  The type of service to carry out the EL3 operation
 *  @param  arg0     -  The argument is specific to the test requirement
 *  @param  arg1     -  The argument is specific to the test requirement
 *  @param  arg2     -  The argument is specific to the test requirement
 *  @return None
**/
void plat_arm_acs_smc_handler(uint64_t services, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{

  INFO("User SMC Call started for service = 0x%lx arg0 = 0x%lx arg1 = 0x%lx arg2 = 0x%lx \n",
        services, arg0, arg1, arg2);

  bool mapped = ((val_el3_at_s1e3w((uint64_t)shared_data)) & 0x1) != 0x1;

  if (mapped) {
    shared_data->status_code = 0;
    shared_data->error_code = 0;
    shared_data->error_msg[0] = '\0';
  }
  switch (services)
  {
    case RME_INSTALL_HANDLER:
      INFO("RME Handler Installing service \n");
      val_el3_rme_install_handler();
      break;
    case RME_ADD_GPT_ENTRY:
      INFO("RME GPT mapping service \n");
      val_el3_add_gpt_entry(arg0, arg1);
      val_el3_tlbi_paallos();
      break;
    case RME_ADD_MMU_ENTRY:
      INFO("RME MMU mapping service \n");
      if (val_el3_add_mmu_entry(arg0, arg1, arg2) == 0) {
          val_el3_tlbi_vae3(arg0);
          shared_data->status_code = 0;
          shared_data->error_code = 0;
          shared_data->error_msg[0] = '\0';
      } else {
          shared_data->status_code = 1;
          const char *msg = "EL3: MMU entry addition failed";
          int i = 0; while (msg[i] && i < sizeof(shared_data->error_msg) - 1) {
              shared_data->error_msg[i] = msg[i]; i++;
          }
          shared_data->error_msg[i] = '\0';
      }
      break;
    case RME_MAP_SHARED_MEM:
      val_el3_map_shared_mem(arg0);
      break;
    case RME_CMO_POPA:
      INFO("RME CMO to PoPA service \n");
      arg0 = val_el3_modify_desc(arg0, CIPOPA_NS_BIT, NS_SET(arg1), 1);
      arg0 = val_el3_modify_desc(arg0, CIPOPA_NSE_BIT, NSE_SET(arg1), 1);
      val_el3_cmo_cipapa(arg0);
      break;
    case RME_ACCESS_MUT:
      INFO("RME MEMORY ACCESS SERVICE\n");
      val_el3_access_mut();
      break;
    case RME_DATA_CACHE_OPS:
      INFO("RME data cache maintenance operation service \n");
      val_el3_data_cache_ops_by_va(arg0, arg1);
      break;
    case RME_MEM_SET:
      INFO("RME memory write service\n");
      val_el3_memory_set((uint64_t *)arg0, arg1, arg2);
      break;
    case RME_NS_ENCRYPTION:
      INFO("RME Non-secure Encryption Enable/Disable service\n");
      if (arg0 == SET)
        val_el3_enable_ns_encryption();
      else
        val_el3_disable_ns_encryption();
      break;
    case RME_READ_AND_CMPR_REG_MSD:
      INFO("RME Registers Read and Compare service\n");
      if (arg0 == SET) {
        val_el3_pe_reg_list_cmp_msd();
        INFO("Register comparision\n");
      } else {
        val_el3_pe_reg_read_msd();
        INFO("Register read\n");
      }
      break;
    case LEGACY_TZ_ENABLE:
      INFO("Legacy System Service\n");
      val_el3_prog_legacy_tz(arg0);
      break;
    case ROOT_WATCHDOG:
      INFO("Root watchdog service \n");
      if (shared_data->generic_flag) {
        val_el3_set_daif();
        shared_data->exception_expected = SET;
        shared_data->access_mut = CLEAR;
      }
      val_el3_wd_set_ws0(arg0, arg1, arg2);
      shared_data->generic_flag = CLEAR;
      break;
    case PAS_FILTER_SERVICE:
      INFO("PAS filter mode service \n");
      val_el3_pas_filter_active_mode(arg0);
      break;
    case SMMU_ROOT_SERVICE:
      INFO("ROOT SMMU service \n");
      if (arg1)
        val_el3_smmu_access_enable(arg0);
      else
        val_el3_smmu_access_disable(arg0);
      break;
    case SEC_STATE_CHANGE:
      INFO("Security STte change service \n");
      val_el3_security_state_change(arg0);
      break;
    case SMMU_CONFIG_SERVICE:
      INFO("SMMU ROOT Register Configuration validate \n");
      val_el3_smmu_root_config_service(arg0, arg1, arg2);
      break;
    case RME_PGT_CREATE:
      INFO("RME pgt_create service \n");
      if (val_el3_realm_pgt_create((memory_region_descriptor_t *)arg0,
                                   (pgt_descriptor_t *) arg1) != 0)
      {
          shared_data->status_code = 1;
          const char *msg = "EL3: PGT creation failed";
          int i = 0; while (msg[i] && i < sizeof(shared_data->error_msg) - 1) {
              shared_data->error_msg[i] = msg[i]; i++;
          }
          shared_data->error_msg[i] = '\0';
      }
      break;
    case RME_PGT_DESTROY:
      INFO("RME pgt_destroy service \n");
      val_el3_realm_pgt_destroy((pgt_descriptor_t *) arg0);
      break;
    case MEC_SERVICE:
      INFO("MEC Service");
      val_el3_mec_service(arg0, arg1, arg2);
      break;
    case RME_CMO_POE:
      INFO("RME CMO to PoE service \n");
      arg0 = val_el3_modify_desc(arg0, CIPAE_NS_BIT, 1, 1);
      arg0 = val_el3_modify_desc(arg0, CIPAE_NSE_BIT, 1, 1);
      val_el3_cmo_cipae(arg0);
      break;
    case RME_READ_CNTPCT:
      uintptr_t base = (uintptr_t)arg0;
      INFO("EL3: CNTCTL base = 0x%lx\n", (unsigned long)base);
      {
        uint32_t cntcr = *(volatile uint32_t *)(base + CNTCR_OFFSET);
        cntcr |= (CNTCR_EN | CNTCR_HDBG);
        *(volatile uint32_t *)(base + CNTCR_OFFSET) = cntcr;
      }
      /* Robust 64-bit read of CNTCV */
      uint64_t full = el3_read_cntcv_robust(base);
      INFO("EL3: CNTCV (64-bit) = 0x%lx\n", (unsigned long)full);
      if (mapped) {
        shared_data->shared_data_access[0].data = full;
        shared_data->status_code = 0;
        shared_data->error_code  = 0;
        shared_data->error_msg[0] = '\0';
      }
      break;
    case RME_READ_CNTID: 
      uintptr_t cntcl = (uintptr_t)arg0;
      uint32_t cntid = el3_read_cntid(cntcl);
      if ((cntid & 0xF) == 0) {
        /* FEAT_CNTSC not implemented (RES0) */
        if (mapped) {
          shared_data->shared_data_access[0].data = 0;
          shared_data->status_code = 0;       
          shared_data->error_code  = 0;
          shared_data->error_msg[0] = '\0';
        }
        INFO("CNTID: FEAT_CNTSC not implemented (RES0)\n");
      } 
      else if ((cntid & 0xF) == 0x1) {
        if (mapped) {
          shared_data->shared_data_access[0].data = (uint64_t)cntid;
          shared_data->status_code = 0;
          shared_data->error_code  = 0;
          shared_data->error_msg[0] = '\0';
        }
        INFO("CNTID: CNTSC implemented (0x%x)\n", cntid & 0xF);
      } 
      else {
        shared_data->status_code = 1;
        const char *msg = "EL3: CNTID returned reserved value";
        int i = 0; while (msg[i] && i < sizeof(shared_data->error_msg) - 1) 
            shared_data->error_msg[i] = msg[i], i++;
        shared_data->error_msg[i] = '\0';
      }
      break;
    case SEC_TIMER_SERVICE:
      INFO("Secure timer (CNTPS) service \n");
      if (arg0 == CNTPS_PROGRAM) {
        int rc = el3_cntps_program_ticks(arg1);
        if (mapped) { 
          shared_data->status_code = rc ? 1 : 0; 
          shared_data->error_code = 0; 
          shared_data->error_msg[0] = '\0'; 
        }
      } else if (arg0 == CNTPS_DISABLE) {
        int rc1 = el3_cntps_disable();
        if (mapped) { 
          shared_data->status_code = rc1 ? 1 : 0; 
          shared_data->error_code = 0; 
          shared_data->error_msg[0] = '\0'; 
        }
      } else {
        if (mapped) {
          shared_data->status_code = 1;
          const char *msg = "EL3: Invalid CNTPS sub-op";
          int i = 0; 
          while (msg[i] && i < sizeof(shared_data->error_msg)-1){
            shared_data->error_msg[i] = msg[i], 
            i++;
            shared_data->error_msg[i] = '\0';
          }
        }
      }
      break;
    case SMC_FID_GET_SCR_EL3:
      INFO("SCR_EL3 read service\n");
      uint64_t scrv = val_el3_read_scr_el3();
      if (mapped) {
        shared_data->shared_data_access[0].data = scrv;  /* return value */
        shared_data->status_code = 0;
        shared_data->error_code  = 0;
        shared_data->error_msg[0] = '\0';
      }
      break;
    case SMC_FID_UPDATE_SCR_EL3:
      INFO("SCR_EL3 update service (set_bits/clear_bits)\n");
      uint64_t set_bits   = arg0;   /* NOTE: using arg0/arg1 exactly as your pal_* APIs */
      uint64_t clear_bits = arg1;
      uint64_t oldv = val_el3_read_scr_el3();
      uint64_t newv = (oldv | set_bits) & ~clear_bits;
      val_el3_write_scr_el3(newv);
      /* Optional readback check */
      uint64_t rb = val_el3_read_scr_el3();
      if (mapped) {
        shared_data->shared_data_access[0].data = rb;  /* return post-update value */
        shared_data->status_code = (rb == newv) ? 0 : 1;
        if (shared_data->status_code) {
          const char *msg = "EL3: SCR update verify failed";
          int i=0; 
          while (msg[i] && i < sizeof(shared_data->error_msg)-1) {
            shared_data->error_msg[i]=msg[i];
            i++;
            shared_data->error_msg[i] = '\0';
          }
        }
      }
      break;
    case SMMU_READ_CFG_BANK:
      INFO("SMMU banked cfg read service \n");
      {
        uint32_t smmu_idx = UNPACK_IDX(arg1);  /* top 32 bits of arg1 */
        uint32_t reg_off  = UNPACK_OFF(arg1);  /* low  32 bits of arg1: SMMUv3 Page0 offset */
        uint32_t bank     = (uint32_t)arg2;    /* 0 = NS, 1 = Secure */
        uint64_t val = val_el3_smmu_read_cfg_bank(smmu_idx, reg_off, bank);
        if (mapped) {
          shared_data->shared_data_access[0].data = val;
          shared_data->status_code = 0;
          shared_data->error_code  = 0;
          shared_data->error_msg[0] = '\0';
        }
      }
      break;
    default:
      if (mapped) {
        shared_data->status_code = 0xFFFFFFFF;
        const char *msg = "EL3: Unknown SMC service";
        int i = 0;
        while (msg[i] && i < sizeof(shared_data->error_msg) - 1) {
              shared_data->error_msg[i] = msg[i]; i++;
        }
        shared_data->error_msg[i] = '\0';
      }
      INFO(" Service not present\n");
      break;
  }
}
