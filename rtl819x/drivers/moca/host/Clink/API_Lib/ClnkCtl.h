/*******************************************************************************
 *      Copyright (C) 2008 Entropic Communications
 ******************************************************************************/

/**
 * @file
 *
 * @brief
 * Interface for the c.LINK Library.
 */
/*******************************************************************************
* This file is licensed under the terms of your license agreement(s) with      *
* Entropic covering this file. Redistribution, except as permitted by the      *
* terms of your license agreement(s) with Entropic, is strictly prohibited.    *
*******************************************************************************/

#ifndef __CLNKCTL_H__
#define __CLNKCTL_H__

/*******************************************************************************
*                             # i n c l u d e s                                *
********************************************************************************/
#include <ClnkDefs.h>
#include <inctypes.h>
#include <eclair.h>
#include <mrt.h>
#include <ClnkPswdSeed.h>

#include "ClnkCtl_commands.h"

/*******************************************************************************
*                             # d e f i n e s                                  *
********************************************************************************/

#define  MAX_PASSWORD_LEN_HEX  40
#define  MIN_MOCAPASSWORD_LEN 12
#define  MAX_MOCAPASSWORD_LEN 17
#define  MAX_MOCAPASSWORD_LEN_PADDED                                       \
            (MAX_MOCAPASSWORD_LEN + sizeof(SYS_UINT32) - 1) /              \
            sizeof(SYS_UINT32) * sizeof(SYS_UINT32)
#define  DEFAULT_MOCAPASSWORD               "99999999988888888"

/*******************************************************************************
*                       G l o b a l   D a t a   T y p e s                      *
********************************************************************************/
// Bridge Setup API parameters struct
typedef struct 
{
    SYS_UINT32 hostId;
    char       ipaddress[16]; /*xxx.xxx.xxx.xxx\0 = 16 bytes*/
    char       netmask[16];   /*xxx.xxx.xxx.xxx\0 = 16 bytes*/
    char       gateway[16];   /*xxx.xxx.xxx.xxx\0 = 16 bytes*/
    int        channel_index;
    char       password[MAX_PASSWORD_LEN_HEX];
    char       mocapassword[MAX_MOCAPASSWORD_LEN_PADDED];
    int        CMRatio;
    SYS_UINT32 TxPower;
    SYS_UINT32 PHY_margin;
    SYS_UINT32 PHY_mgn_bitmask;
    SYS_UINT32 SwConfig;
#if defined(L3_DONGLE_HIRF)
    SYS_UINT32 Diplexer;
#endif
    SYS_UINT32 NetworkType; /* ReadOnly */
    SYS_UINT32 dhcp;
    char       admin_pswd[40];
    SYS_UINT32 channelPlan; /* ReadOnly */
    SYS_UINT32 channelMask;
    SYS_UINT32 lof;
    SYS_UINT32 bias;
    SYS_UINT32 Target_PHY_rate;
    SYS_UINT32 PowerCtl_PHY_rate;
    SYS_UINT32 DistanceMode;
    SYS_UINT32 productMask;  /* ReadOnly */
    SYS_UINT32 scanMask; /* ReadOnly */
    SYS_UINT32 tabooMask;
    SYS_UINT32 tabooOffset;
    SYS_UINT32 BeaconPwrLevel;
    SYS_UINT32 MiiPausePriLvl;
    SYS_UINT32 PQoSClassifyMode;   /* PQoS Classification Mode */

#if FEATURE_FEIC_PWR_CAL
    SYS_INT32  feicProfileId;
#endif

#if FEATURE_PUPQ_NMS
    SYS_UINT32 mfrVendorId;        /* we only use 16 lsbs */
    SYS_UINT32 mfrHwVer;
    SYS_UINT32 mfrSwVer;
    SYS_UINT32 personality;
#endif /* FEATURE_PUPQ_NMS */

    SYS_UINT32  dbgMask;         // debug mask passed to SW for special debug purposes

#if FEATURE_IQM
    SYS_UINT32      iqmDebugMask1;
    SYS_UINT32      iqmDebugMask2;
    SYS_UINT32      iqmDebugMask3;
#endif

}
BridgeSetupStruct;

typedef union
{
    SYS_UINT8 value[MAX_MOCAPASSWORD_LEN_PADDED];
    SYS_UINT32 _force_alignment;
} clnk_nms_mocapassword_t;


typedef struct clnk_ctx
{
    char        ifname[IFNAME_MAX + 1];
    int         unix_fd;
    int         transid;
    int         clnkdvr;
}clnk_ctx_t;

struct clnk_io
{
    SYS_UINT32      *in;
    SYS_UINT32      in_len;
    SYS_UINT32      *out;
    SYS_UINT32      out_len;
};

struct clnk_netio
{
    SYS_UINT32      magic;
    SYS_UINT32      version;
    SYS_UINT32      cmd;
    SYS_UINT32      transid;
    SYS_UINT32      in_len;
    SYS_UINT32      out_len;
    SYS_UINT32      status;
    SYS_UINT32      reserved[5];
};

struct clnk_soc_opt
{
    /* copied from conf file */
    SYS_UINT32      CMRatio;
    SYS_UINT32      DistanceMode;
    SYS_UINT32      TxPower;
    SYS_UINT32      phyMargin;
    SYS_UINT32      phyMBitMask;
    SYS_UINT32      SwConfig;
#if defined(L3_DONGLE_HIRF)
    SYS_UINT32	    Diplexer;
#endif    
    SYS_UINT32      channelPlan;
    SYS_UINT32      scanMask;
    SYS_UINT32      productMask;
    SYS_UINT32      tabooMask;
    SYS_UINT32      tabooOffset;
    SYS_UINT32      channelMask;
    SYS_UINT32      lof;
    SYS_UINT32      bias;
    SYS_UINT32      PowerCtlPhyRate;
    SYS_UINT32      BeaconPwrLevel;
    SYS_UINT32      MiiPausePriLvl;
    SYS_UINT32      PQoSClassifyMode;  /* PQoS Classification Mode */

#if FEATURE_FEIC_PWR_CAL
    SYS_UINT32      feicProfileId;
#endif

#if FEATURE_PUPQ_NMS_CONF
    SYS_UINT32      mfrVendorId;        /* we only use 16 lsbs */
    SYS_UINT32      mfrHwVer;
    SYS_UINT32      mfrSwVer;
    SYS_UINT32      personality;
#endif /* FEATURE_PUPQ_NMS_CONF */

    SYS_UINT32      dbgMask;         // debug mask passed to SW for special debug purposes

#if FEATURE_IQM
    SYS_UINT32      iqmDebugMask1;
    SYS_UINT32      iqmDebugMask2;
    SYS_UINT32      iqmDebugMask3;
#endif

    SYS_UINT32      TargetPhyRate;

    /* derived from conf file */
    SYS_UINT32      pmki_lo;
    SYS_UINT32      pmki_hi;
    SYS_UINT32      mmk_lo;
    SYS_UINT32      mmk_hi;

    clnk_nms_mocapassword_t mocapassword;
};

struct clnk_conf
{
    SYS_UINT32      hostId;
    char            ipaddr[16];
    char            netmask[16];
    char            gateWay[16];
    char            password[40];
    SYS_UINT32      dhcp;
    char            admPswd[40];
#if FEATURE_FEIC_PWR_CAL
    SYS_INT32       feicProfileId;
    feic_cfg_t      feicProfiles;
#endif
    struct clnk_soc_opt soc_opt;
};

#define NETIO_MAGIC                 0x436c4e6b
#define NETIO_VERSION               0x1

#define CLNK_DES_KEYS               4
#define PMKI_LO                     0
#define PMKI_HI                     1
#define MMK_LO                      2
#define MMK_HI                      3

#define NETIO_SOCK                  "/tmp/clink.sock"

#define RET_GOOD                    0       /* ioctl: 0 */
#define RET_ERR                     -1      /* ioctl: -EIO */
#define RET_NODEV                   -2      /* ioctl: -ENODEV */
#define RET_WHICHDEV                -3      /* ioctl: N/A */
#define RET_TIMEOUT                 -4      /* ioctl: N/A */
#define RET_WRONGVER                -5      /* ioctl: -EACCES */
#define RET_PERM                    -6      /* ioctl: -EPERM */


#ifdef AEI_WECB
void aei_gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

/*******************************************************************************
 *      Interface API
 ******************************************************************************/
int clnk_init(char *iface, clnk_ctx_t **ppctx);
int clnk_init_pci_xmii(char *iface, void **ppctx);
int clnk_destroy(clnk_ctx_t *ppctx);
int clnk_get_ifname(clnk_ctx_t *pctx, char *buf);
int clnk_list_devices(ClnkDef_ZipList_t *zl, int max);
int clnk_reset_device(clnk_ctx_t *pctx);
int clnk_stop_device(clnk_ctx_t *pctx); 
int clnk_get_eth_reg_val(clnk_ctx_t *pctx,SYS_UINT32 offset, SYS_UINT32 *val);
int clnk_set_eth_reg_val(clnk_ctx_t *pctx,SYS_UINT32 offset, SYS_UINT32 *val);
int clnk_get_mem_ext(clnk_ctx_t *pctx, SYS_UINT32 addr, SYS_UINT8 *dst,
                     SYS_UINT32 len, int flip);
int clnk_set_mem_ext(clnk_ctx_t *pctx, SYS_UINT32 addr, SYS_UINT8 *src,
                     SYS_UINT32 len, int flip);
int clnk_ctl_passthru_sock(clnk_ctx_t *pctx, int cmd, struct clnk_io *io);

/*******************************************************************************
 *      Configuration File API
 ******************************************************************************/

int clnk_read_cfg_file(char * fname,BridgeSetupStruct *setup, int flag);
int clnk_write_cfg_file(char * fname,BridgeSetupStruct *setup, SYS_BOOLEAN update);

/*******************************************************************************
 *      Initial/Run-time Configuration API
 ******************************************************************************/

int clnk_get_soc_opt(clnk_ctx_t *pctx, struct clnk_soc_opt *soc_opt); 
int clnk_set_soc_opt(clnk_ctx_t *pctx, struct clnk_soc_opt *soc_opt); 
int clnk_send_key(clnk_ctx_t *pctx,char *password, char *mocapassword);
int clnk_set_cmratio(clnk_ctx_t *pctx, SYS_UINT32 cmratio);
int clnk_set_distance_mode(clnk_ctx_t *pctx, SYS_UINT32 distance_mode);
int clnk_set_tx_power(clnk_ctx_t *pctx, SYS_UINT32 tx_power);
int clnk_set_phy_margin(clnk_ctx_t *pctx, SYS_UINT32 margin,SYS_UINT32 bitmask);
int clnk_set_swconfig(clnk_ctx_t *pctx, SYS_UINT32 sw_config);
int clnk_set_channel_plan(clnk_ctx_t *pctx, SYS_UINT32 plan,SYS_UINT32 mask);
int clnk_set_scan_mask(clnk_ctx_t *pctx, SYS_UINT32 scan_mask);
int clnk_set_channel_mask(clnk_ctx_t *pctx, SYS_UINT32 channel_mask);
int clnk_set_bias(clnk_ctx_t *pctx, SYS_UINT32 bias);
int clnk_get_lof(clnk_ctx_t *pctx, SYS_UINT32 *lof);
int clnk_set_lof(clnk_ctx_t *pctx, SYS_UINT32 lof);
int clnk_set_taboo_info(clnk_ctx_t *pctx, SYS_UINT32 mask,SYS_UINT32 offset);
int clnk_set_pwr_ctl_phy_rate(clnk_ctx_t *pctx, SYS_UINT32 phy_rate);
int clnk_set_beacon_pwr_level(clnk_ctx_t *pctx,SYS_UINT32 option, SYS_UINT32 power);
int clnk_set_eth_fifo_size(clnk_ctx_t *pctx, SYS_UINT32 * pEthFifoSz, int numPrio) ;
#if defined(L3_DONGLE_HIRF)
int clnk_set_diplexer(clnk_ctx_t *pctx, SYS_UINT32 diplexer_val);
int clnk_set_pause_pri_override(clnk_ctx_t *pctx, SYS_UINT32 pri_override);
#endif
int clnk_set_mii_pause_pri_lvl(clnk_ctx_t *pctx, SYS_UINT32 mii_prilvl);
#if FEATURE_PUPQ_NMS_CONF
int clnk_set_mfr_info(clnk_ctx_t *pctx, SYS_UINT32 mfrVendorId, 
                      SYS_UINT32 mfrHwVer, SYS_UINT32 mfrSwVer);
int clnk_set_personality(clnk_ctx_t *pctx, SYS_UINT32 personality);
#endif /* FEATURE_PUPQ_NMS_CONF */
int clnk_set_debug_mask(clnk_ctx_t *pctx, SYS_UINT32 dbg_mask);
int clnk_set_pqos_mode(clnk_ctx_t *pctx, SYS_UINT32 pqos_mode);

int clnk_get_dyn_params(clnk_ctx_t *pctx, ClnkDef_DynParams_t *dyn_params); 
int clnk_set_dyn_params(clnk_ctx_t *pctx, ClnkDef_DynParams_t *dny_params); 

/*******************************************************************************
 *      Statistics API
 ******************************************************************************/

int clnk_get_aggr_stats(clnk_ctx_t *pctx, aggr_stats_t *aggr_stats, int clear); 
int clnk_get_bridge_table(clnk_ctx_t *pctx, ClnkDef_BridgeTable_t *brg_table, int mode); 
int clnk_get_cam(clnk_ctx_t *pctx, ClnkDef_CamTable_t *cam_table, int index); 
int clnk_get_ephy_stats(clnk_ctx_t *pctx, ephy_stats_t *ephy_stats, int clear); 
int clnk_get_eth_stats(clnk_ctx_t *pctx, ClnkDef_EthStats_t *eth_stats, int clear); 
int clnk_get_lmo_advanced_ctr(clnk_ctx_t *pctx, SYS_UINT32 *lac_ptr);
int clnk_get_my_node_info(clnk_ctx_t *pctx, ClnkDef_MyNodeInfo_t *my_node, int clear);
int clnk_get_network_node_info(clnk_ctx_t *pctx, ClnkDef_NetNodeInfo_t *net_node, int node_id); 
int clnk_get_node_phy_data(clnk_ctx_t *pctx, ClnkDef_PhyData_t *phy_data, int node_id); 
int clnk_get_peer_rates(clnk_ctx_t *pctx, 
                        peer_rates_t *peer, 
                        peer_rates_entry_status_t *status,
                        SYS_UINT32  timeout_ms,
                        void (*sleep_func)(SYS_UINT32 milliseconds)) ;
int clnk_get_privacy_info(clnk_ctx_t *pctx, priv_info_t *priv_info); 
int clnk_get_privacy_node_info(clnk_ctx_t *pctx, priv_node_info_t *priv_node_info, int mode); 
int clnk_get_privacy_stats(clnk_ctx_t *pctx, priv_ioctl_stat_t *priv_stat, int clear); 
int clnk_get_rfic_tuning_data(clnk_ctx_t *pctx, ClnkDef_RFICTuningTbl_t *rfic_data); 
int clnk_get_zip_info(clnk_ctx_t *pctx, ClnkDef_ZipInfo_t *zip_info); 

/*******************************************************************************
 *      Privacy API
 ******************************************************************************/

int clnk_gen_sha_key(clnk_ctx_t *pctx,char *password, char *seed);
int clnk_pass_to_seed(clnk_ctx_t *pctx, SYS_UINT8 *seed_out, SYS_UINT8 *pass_in);
int clnk_seed_to_keys(clnk_ctx_t *pctx, SYS_UINT32 *keys_out, SYS_UINT8 *seed_in);
int clnk_get_current_mocapassword(clnk_ctx_t *pctx, clnk_nms_mocapassword_t *ps_ptr);
int clnk_save_viewable(char *password);

/*******************************************************************************
 *      PQoS API
 ******************************************************************************/

const char* clnk_qos_get_error_string(int qos_error);
void clnk_qos_perror(char *str, int err_num);

#if FEATURE_QOS
int clnk_query_nodes(clnk_ctx_t *pctx, qos_node_response_t *qos_resp); 
int clnk_get_event_counts(clnk_ctx_t *pctx, qos_events_t *qos_event);
int clnk_create_flow(clnk_ctx_t *pctx, flow_desc_t *flow_desc,
                     qos_cu_response_t *qos_resp); 
int clnk_update_flow(clnk_ctx_t *pctx, flow_desc_t *flow_desc,
                     qos_cu_response_t *qos_resp); 
int clnk_delete_flow(clnk_ctx_t *pctx, flow_name_t *flow_name,
                     qos_d_response_t *qos_resp); 
int clnk_query_ingress_flow(clnk_ctx_t *pctx, flow_name_t *flow_name,
                            qos_q_f_response_t *qos_resp); 
int clnk_list_ingress_flows(clnk_ctx_t *pctx, qos_f_l_t *flow_list,
                            qos_f_l_response_t *qos_resp); 
int clnk_query_interface_capabilities(clnk_ctx_t *pctx, clink_guid_t *clnk_guid,
                                      qos_i_c_response_t *qos_resp); 
SYS_UINT32 clnk_qos_query_capacity(clnk_ctx_t * pctx,
                                   qos_cap_response_t *cap_resp_ptr);
int clnk_qos_get_maint_cache(clnk_ctx_t *pctx, qos_maint_cache_t *qmc_ptr);
#endif

/*******************************************************************************
 *      Debug API
 ******************************************************************************/

int clnk_get_epp_data(clnk_ctx_t *pctx, ClnkDef_EppData_t *epp_data); 
int clnk_get_evm_data(clnk_ctx_t *pctx, ClnkDef_EvmData_t *evm_data); 
#if FEATURE_FEIC_PWR_CAL
// The following functions can be used to manage the FEIC profile
int clnk_get_feic_final_results(clnk_ctx_t *pctx, feic_final_results_t *pResults);
int clnk_set_feic_profile(clnk_ctx_t *pctx, feic_profile_t *profile, int pid);
int clnk_get_feic_profile_id(clnk_ctx_t *pctx, int *pid);
int clnk_set_feic_profile_id(clnk_ctx_t *pctx, int pid);

// The following functions can be used to manage the FEIC profile config
int clnk_read_feic_cfg_file(feic_cfg_t *cfg, const char *fname);
int clnk_write_feic_cfg_file(feic_cfg_t *cfg, const char *fname);
#endif
int clnk_get_rx_err_data(clnk_ctx_t *pctx, ClnkDef_RxErrData_t *rxerr_data); 
int clnk_get_tracebuf(clnk_ctx_t *pctx, ClnkDef_TraceBuf_t *trace_buf); 
int clnk_send_gcap(clnk_ctx_t *pctx, ClnkDef_Gcap_t *gcap); 

#if FEATURE_ECLAIR
int clnk_eclair_get_hinfo(clnk_ctx_t *pctx, eclair_Hinfo_t *hinfo);
int clnk_eclair_set_tweakable(clnk_ctx_t *pctx, eclair_Tweakable_t *tweakable);
int clnk_eclair_reset_test_context(clnk_ctx_t *pctx, eclair_Reset_t *resetPtr);

#if NEVER_USE_AGAIN_ECLAIR_LEGACY_TESTING
int clnk_eclair_push_command(clnk_ctx_t *pctx, eclair_PushCommandEntryRequest_t *dictateRequest, eclair_PushCommandEntryResult_t *dictateResult);
int clnk_eclair_get_command(clnk_ctx_t *pctx, eclair_Command_t *command);
int clnk_eclair_set_receipt(clnk_ctx_t *pctx, eclair_Receipt_t *receipt);
int clnk_eclair_pull_receipt(clnk_ctx_t *pctx, eclair_PullReceiptEntryRequest_t *request, eclair_PullReceiptEntryResult_t *result);
#endif /* NEVER_USE_AGAIN_ECLAIR_LEGACY_TESTING */

#if FEATURE_ECLAIR_WHITE_BOX_TEST
#include "ClnkCtl_eclair_wbt.h"
#endif /* FEATURE_ECLAIR_WHITE_BOX_TEST */

#if ECFG_FLAVOR_VALIDATION==1
int clnk_val_get_mbox_host_counts(clnk_ctx_t *pctx, eclair_ValMboxHostCounts_t *hostCounts);
int clnk_val_get_mbox_ccpu_counts(clnk_ctx_t *pctx, eclair_ValMboxCcpuCounts_t *ccpuCounts);
int clnk_val_trigger_mbox_event(struct clnk_ctx *vctx, eclair_ValTriggerMboxEvent_t *triggerMboxEvent);
#endif
#endif /* FEATURE_ECLAIR */


#endif /* __CLNKCTL_H__ */

