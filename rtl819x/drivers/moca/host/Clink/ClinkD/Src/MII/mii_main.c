/**
 * \file mii_main.c
 * \brief Entry point and mainloop for MII clinkd
 */
/*******************************************************************************
* This file is licensed under the terms of your license agreement(s) with      *
* Entropic covering this file. Redistribution, except as permitted by the      *
* terms of your license agreement(s) with Entropic, is strictly prohibited.    *
*******************************************************************************/

#define _GNU_SOURCE     // for getopt_long

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <time.h>
#include <syslog.h>
#if 1 /* ACTIONTEC_MOCA */
#include <math.h>
#endif

#include "HostOS_Common.h"
#include "eth_iface.h"
#include "ClnkDefs.h"
#include "ClnkEth.h"
#include "ClnkMbx_mii.h"
#include "ClnkCore.h"
#include "hw_z2.h"
#include "mii_common.h"
#include "netio.h"
#include "clnk_conf.h"
#include "driverversion.h"
#include "ClnkDrv_API.h"

static int nic_reset(struct mii_iface *iface);

int got_sighup = 0, got_sigterm = 0;

/* exists because HostOS_PrintLog does not pass in our context */
struct mii_iface *giface;

/* runtime LOF */
static uint32_t my_lof;
int mac_mode;
char unsigned MAC_address_bytes[6];
int MII_SPD_variable;
#define POLLING_TIME 300000
#define TWO_SECONDS 2000000


extern int elf_setup( char *path, fw_img_t **dst) ;
extern int uc_setup( char *path, fw_img_t **dst) ;


void vlog_msg(int prio, const char *fmt, va_list ap)
{
    struct mii_iface *iface = giface;
    int syslog_prio;

    if(prio < iface->min_log_prio)
    {
        return;
    }

    if(! iface->no_syslog)
    {
        switch(prio)
        {
            case L_DBG:
                syslog_prio = LOG_DEBUG;
                break;
            case L_VERBOSE:
                syslog_prio = LOG_DEBUG;
                break;
            case L_INFO:
                syslog_prio = LOG_INFO|LOG_LOCAL0;
                break;
            case L_ERR:
            default:
                syslog_prio = LOG_ERR|LOG_LOCAL0;
                break;
        }
        vsyslog(syslog_prio, fmt, ap);
    }
    else
    {
        if(iface->timestamp)
        {
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);
            char ts[32];

            strftime(ts, 31, "%m/%d %H:%M:%S", tm);
            printf("%s - ", ts);
        }
        vprintf(fmt, ap);
    }

}

/**
 * \brief Log an ASCII message for the user
 *
 * Logs a plain text message to stdout or to syslog, depending on how the
 * daemon was invoked.
 *
 * \param[in] iface MII clinkd context
 * \param[in] prio Priority: one of L_DBG, L_VERBOSE, L_INFO, or L_ERR .
 * \param[in] fmt printf-style format string
 */

void log_msg(clnk_iface_t *iface, int prio, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vlog_msg(prio, fmt, ap);
    va_end(ap);
}

/**
 * \brief POSIX signal handler for the MII clinkd
 *
 * Sets a global flag to indicate that a signal has been received.
 *
 * POSIX signals are interpreted as follows:
 * 
 * SIGHUP causes the daemon to reload the configuration file and restart the
 * device.
 *
 * SIGINT or SIGTERM causes the daemon to shut down the device and terminate.
 *
 * \param[in] sig Signal number received from the kernel.
 */

void signal_handler(int sig)
{
    switch(sig)
    {
        case SIGHUP:
            got_sighup = 1;
            break;
        case SIGTERM:
        case SIGINT:
            got_sigterm = 1;
            break;
    }
}

/**
 * \brief Save LOF
 *
 * This function is used to save the last operational frequency (LOF) only
 * if it is different from the last recorded LOF.
 *
 * \param[in] iface MII clinkd context
 * LINK_FORCE_DOWN.
 */

void save_lof(struct mii_iface *iface)
{
    struct clnk_io io;
    struct clnk_conf cfg;
    ClnkDef_MyNodeInfo_t my_node_info;

    /*
     * Get the new LOF value from the SoC via common driver.
     */
    io.in = NULL;
    io.in_len = 0;
    io.out = (void *)&my_node_info;
    io.out_len = sizeof(my_node_info);
    my_node_info.ClearStats = 0;
    if(ProcessCmd(iface, CLNK_CTL_GET_MY_NODE_INFO, &io))
    {
        log_msg(iface, L_ERR, "error getting soc node info\n");
        return;
    }
    /* read clink.conf, pass in user specified conf_path */
    if(conf_read(&cfg, iface->conf_path))
    {
        log_msg(iface, L_ERR, "Unable to read configuration file\n");
        return;
    }
    if (cfg.soc_opt.lof != my_lof)
    {
        my_lof = cfg.soc_opt.lof;
    }

    if (my_lof == my_node_info.RFChanFreq)
        return;

    my_lof = my_node_info.RFChanFreq;
        
    cfg.soc_opt.lof = my_lof;

#ifdef AEI_WECB
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cli -s Device.MoCA.Interface.1.LastOperFreq int %d >/dev/null", my_lof);
    system(cmd);
    system("cli -f");
#endif

    /* write clink.conf, pass in user specified conf_path */
    if(conf_write(&cfg))
    {
        log_msg(iface, L_ERR, "Unable to write to configuration file\n");
        return;
    }

    log_msg(iface, L_DBG, "New LOF = %u saved to config file\n", my_lof);

    /* Get the clink options in the common driver. */
    io.in = NULL;
    io.in_len = 0;
    io.out = (void *)&cfg.soc_opt;
    io.out_len = sizeof(cfg.soc_opt);

    if(Clnk_ETH_Control(iface->pvContext, CLNK_ETH_CTRL_DO_CLNK_CTL,  
       CLNK_CTL_GET_SOC_OPT, (SYS_UINTPTR)&io, 0) != CLNK_ETH_RET_CODE_SUCCESS)
    {
        log_msg(iface, L_ERR, "CLNK_CTL_GET_SOC_OPT failed\n");
        return;
    }

    /* Update LOF in the clink options */
    cfg.soc_opt.lof = my_lof;

    /* Write back the clink options in the common driver with updated LOF */
    io.in = (void *)&cfg.soc_opt;
    io.in_len = sizeof(cfg.soc_opt);
    io.out = NULL;
    io.out_len = 0;

    if(Clnk_ETH_Control(iface->pvContext, CLNK_ETH_CTRL_DO_CLNK_CTL,  
       CLNK_CTL_SET_SOC_OPT, (SYS_UINTPTR)&io, 0) != CLNK_ETH_RET_CODE_SUCCESS)
    {
        log_msg(iface, L_ERR, "CLNK_CTL_SET_SOC_OPT failed\n");
        return;
    }
}

/**
 * \brief Set the link status if it has changed
 *
 * This function is used to set the link status in two places: the MII
 * enhanced control register (which sets or clears the link bit in the
 * standard MII BMSR), and in the driver's mii_iface struct.
 *
 * If \a link_status is LINK_FORCE_DOWN, the link status is forced to 0.
 * This is only needed at boot time, when the MII link state might be unknown.
 *
 * \param[in] link_status New link status: LINK_UP, LINK_DOWN, or
 * LINK_FORCE_DOWN.
 */
void mii_set_link(struct mii_iface *iface, int link_status)
{
    int32_t enh_ctl;
    Context_t* pContext = iface->pvContext;

#if 0 /* ACTIONTEC_MOCA */
    int  mii_link=LINK_DOWN;
    if (mii_read(iface, MMI_REG_STATUS) & (1 << 2))
    {
        mii_link = LINK_UP;
    }
#endif

    if(iface->link_status == link_status)
        return;

#if 0 /* ACTIONTEC_MOCA */
    if (link_status == LINK_UP && mii_link != LINK_UP)
    {
        return;
    }
#endif

    enh_ctl = mii_read(iface, MMI_REG_ENH_CTL);
    if(enh_ctl < 0)
        return;

    if(link_status == LINK_FORCE_DOWN)
    {
        enh_ctl &= ~0x20;
        iface->link_status = LINK_DOWN;
    } else {
        iface->link_status = link_status;
        pContext->linkUpFlag = link_status;
	log_msg(iface, L_INFO, "Clink status 0x%x\n", iface->link_status);
        if(link_status == LINK_UP)
        {
            struct timeval       nowTime;
            HostOS_GetTime( &nowTime ) ;
            pContext->linkUpStartTime.tv_sec = nowTime.tv_sec;
            log_msg(iface, L_INFO, "Clink Link Up\n");
            system("cli -s Device.MoCA.Interface.1.X_ACTIONTEC_COM_Status string Up >/dev/null");
            enh_ctl |= 0x20;
            save_lof(iface);
        } 
        else 
        {
            log_msg(iface, L_INFO, "Clink Link Down\n");
            system("cli -s Device.MoCA.Interface.1.X_ACTIONTEC_COM_Status string Down >/dev/null");
            enh_ctl &= ~0x20;
        }
    }
#if 1 /* ACTIONTEC_MOCA */
    mii_write(iface, MMI_REG_ENH_CTL, enh_ctl | (1 << 9));
#else
    mii_write(iface, MMI_REG_ENH_CTL, enh_ctl);
#endif
}

#if defined(L3_DONGLE_HIRF)
struct ethernet_ports_filter
{
    unsigned short int epf_linked_mask;
    unsigned short int epf_linked;
    unsigned short int epf_flow_mask;
    unsigned short int epf_flow;
};

// #########################################################################
// In order to sense the ports on the system we only check the link status
// "LS" and the duplex status "DS" and the flow control status "FCS." Ports
// that are sensed are checked for link status, then for flow control and
// duplex status. The results of all this sensing goes into a compuations
// that tells us how many ports are linked with flow control and how many
// ports are linked without flow control. If there is only one linked port
// without flow control then it is assumed that the ECB and all components
// inside the ECB are responsible for ensuring that high priority traffic
// is honered while low priority traffic can be dropped.
// #########################################################################

// Port Status Register page 156 of Link Street 88E6161/88E6123 Datasheet Part 2 of 3: Switch Core
#define P0LS (1 << 11)      // Link
#define P0FCS (1 << 15)     // PauseEn
#define P0DS (1 << 10)      // Duplex

#define PORT_SENSED P0LS,P0LS,P0FCS | P0DS,P0FCS | P0DS
#define PORT_IGNORED 0,P0LS,0,P0FCS | P0DS

#define PORT_NUM_OFFSET 0x10    // ports 0-5 map into port 0x10-0x15

struct ethernet_ports_filter ports_status_filter[] = {
    {PORT_SENSED},          // Port 0 is a 10/100 port
    {PORT_SENSED},          // Port 1 is a 10/100 port
    {PORT_SENSED},          // Port 2 is a 10/100 port
    {PORT_SENSED},          // Port 3 is a 10/100 port
    {PORT_IGNORED},         // Port 4 is connected to the IXP 420 host
    {PORT_IGNORED},         // Port 5 is connected to the SoC
};

//--------------------------------------------------------------------------
// Define the number of ports to sense on this system by talling the number
// of entries in the table above.
//--------------------------------------------------------------------------
#define EXTERNAL_PORTS (sizeof(ports_status_filter) / sizeof(struct ethernet_ports_filter))

//--------------------------------------------------------------------------
// Define some macros to make it easy to figure out if a port is linked,
// and if the port has flow controlled or not using the entries in the
// table above.
//--------------------------------------------------------------------------
#define IS_LINKED(port) ((ports_status_filter[port].epf_linked_mask & phy_status[port]) == ports_status_filter[port].epf_linked)
#define IS_FLOW_CONTROLLED(port) ((ports_status_filter[port].epf_flow_mask & phy_status[port]) == ports_status_filter[port].epf_flow)

//--------------------------------------------------------------------------
// This table holds the last sensed entries of the external ports on the
// system. This table is used to determine if there has been a change in
// link status or not.
//--------------------------------------------------------------------------
unsigned short int last_phy[EXTERNAL_PORTS];

// ########################################################################
// ##                  get_external_ethernet_phy_status                  ##
// ########################################################################
// ## Description:                                                       ##
// ##   This function is called frequently to poll the Tantos 3G switch  ##
// ##   to determine if we are in the mode where we wish the ECB to      ##
// ##   dispose of low priority traffic because it is coming into the    ##
// ##   ECB via port(s) that is(are) not flow controlled. If such case   ##
// ##   is detected, we indicate this info to the SoC who will get into  ##
// ##   a special mode where it will limit on-chip packet buffer usage   ##
// ##   by low priority traffic and also raise the pause priority level  ##
// ##   to 1 (mid priority) only if (low + mid) mixed priority traffic   ##
// ##   is active. This allows higher priority traffic to be processed   ##
// ##   reliably through the system even in the absence of flow control  ##
// ##   between the ECB and its ethernet end point.                      ##
// ##                                                                    ##
// ##   Phase 1:                                                         ##
// ##      Get the current DIC TC misc config from the SoC if we do not  ##
// ##      have it already.                                              ##
// ##                                                                    ##
// ##   Phase 2:                                                         ##
// ##      Obtain the current link and duplex and flow control status of ##
// ##      all the external ports on the switch.                         ##
// ##                                                                    ##
// ##   Phase 3:                                                         ##
// ##      Check the current link status with the previously known link  ##
// ##      status. If it is the same then exit this routine.             ##
// ##                                                                    ##
// ##   Phase 4:                                                         ##
// ##      Tally up the number of linked ports with flow control. Tally  ##
// ##      up the number of ports linked without flow control.           ##
// ##                                                                    ##
// ##   Phase 5:                                                         ##
// ##      If all linked-up ports are running without flow control,      ##
// ##      then                                                          ##
// ##          Setup the SoC for low priority discard mode.              ##
// ##      else                                                          ##
// ##          Setup the SoC to the default mode where it will honor     ##
// ##          user configured MII Pause Priority Level.                 ##
// ##                                                                    ##
// ##____________________________________________________________________##
// ## Input:                                                             ##
// ##   iface = the pointer to the interface structure for the SoC       ##
// ##____________________________________________________________________##
// ## Globals Used:                                                      ##
// ##                                                                    ##
// ##____________________________________________________________________##
// ## Return:                                                            ##
// ##   Nothing                                                          ##
// ##____________________________________________________________________##
// ## Side effects:                                                      ##
// ##                                                                    ##
// ########################################################################

void get_external_ethernet_phy_status(struct mii_iface *iface)
{
    int i;
    int change_found;
    int linked_with_flow, linked_without_flow;
    unsigned short int phy_status[EXTERNAL_PORTS];
    struct ifreq request;
    struct mii_ioctl_data *smidata = (struct mii_ioctl_data *)&request.ifr_data;

    for (i = 0; i < EXTERNAL_PORTS; i++)
    {
        memset(&request,0,sizeof(request));
        strcpy(request.ifr_ifrn.ifrn_name, "/dev/CandD");
        smidata->phy_id = 0;
        smidata->reg_num = 0x1000 + (i + PORT_NUM_OFFSET) * 0x20;
        smidata->val_in = 0;
        smidata->val_out = 0;
        if (ioctl(iface->fd, SIOCGMIIREG, &request) < 0)
            phy_status[i] = 0;
        else
            phy_status[i] = smidata->val_out;
    }

    change_found=0;
    for (i = 0; i < EXTERNAL_PORTS; i++)
    {
        if (last_phy[i] != phy_status[i])
        {
            last_phy[i]=phy_status[i];
            change_found=1;
        }
    }
    if (! change_found)
        return;

    linked_with_flow = linked_without_flow = 0;
    for (i = 0;i < EXTERNAL_PORTS; i++)
    {
        if (IS_LINKED(i))
        {
            if (IS_FLOW_CONTROLLED(i))
                linked_with_flow++;
            else
                linked_without_flow++;
        }
    }
    //HostOS_PrintLog(L_DBG, "Ports linked w/  flow ctl = %u\n",
    //                linked_with_flow);
    //HostOS_PrintLog(L_DBG, "Ports linked w/o flow ctl = %u\n",
    //                linked_without_flow);
    if (linked_with_flow == 0 && linked_without_flow >= 1)
    {
        socRegWrite(iface->iface_dev, DIC_D_TX_PRI_0_DISCARD, 1);
        //HostOS_PrintLog(L_DBG, "Tx priority 0 discard mode is ON\n");
    }
    else
    {
        socRegWrite(iface->iface_dev, DIC_D_TX_PRI_0_DISCARD, 0);
        //HostOS_PrintLog(L_DBG, "Tx priority 0 discard mode is OFF\n");
    }
}
#endif /* L3_DONGLE_HIRF */

/**
 * \brief Process incoming queries; return after 2 seconds.
 *
 * Processes incoming queries from other user processes (e.g. clnkstat).
 * This replaces the ioctl functionality in the standard driver.  All
 * queries for a particular clink device are routed through its respective
 * clinkd process.
 *
 * This function should return after 2 seconds, in normal operation.
 * Exceptions include: receipt of a signal, catastrophic SoC errors,
 * MII access errors.
 */

void mii_process_queries(struct mii_iface *iface)
{
    struct timeval ret_tv, sel_tv;
    unsigned int ticks;

    /* set return time to "now + 2s" */
#ifdef AEI_WECB
    aei_gettimeofday(&ret_tv, NULL);
#else
    gettimeofday(&ret_tv, NULL);
#endif
    ret_tv.tv_sec += 2;

    while(1)
    {
        if(got_sighup || got_sigterm)
            return;

        /* calculate select() timeout based on return time */
#ifdef AEI_WECB
        aei_gettimeofday(&sel_tv, NULL);
#else
        gettimeofday(&sel_tv, NULL);
#endif

        /* return time has passed? */
        if(sel_tv.tv_sec > ret_tv.tv_sec)
            return;
        if((sel_tv.tv_sec == ret_tv.tv_sec) &&
           (sel_tv.tv_usec >= ret_tv.tv_usec))
        {
            return;
        }

        ticks = 1000000 * (ret_tv.tv_sec - sel_tv.tv_sec) +
            (ret_tv.tv_usec - sel_tv.tv_usec);
        sel_tv.tv_sec = ticks / 1000000;
        sel_tv.tv_usec = ticks % 1000000;

        /* allow network listener to wait up to (sel_tv) time for a request */
        if(netio_wait(iface->ioctx, &sel_tv) == 0)
        {
            netio_exec(iface->ioctx);
        }

#if defined(L3_DONGLE_HIRF)
        get_external_ethernet_phy_status(iface);
#endif /* L3_DONGLE_HIRF */
    }
}

/*
 * \brief Save a core file
 *
 * Writes out a "clnkcore" file (in the new format) to the filesystem, to
 * capture the state of the SoC for debugging purposes.
 */

int clnkcore_save(struct mii_iface *iface)
{
    int fd;

    if(! iface->core_buf)
    {
        iface->core_len = Clnk_ETH_UpdateDebugInfo(NULL, NULL);
        iface->core_buf = (void *)malloc(iface->core_len);
        if(! iface->core_buf)
        {
            log_msg(iface, L_ERR, "error: can't allocate memory for core dump\n");
            return(-1);
        }
    }

    fd = open(iface->core_path, O_WRONLY | O_CREAT, 0644);
    if(fd < 0)
    {
        log_msg(iface, L_ERR, "error: can't open core file for writing: %s\n", strerror(errno));
        return(-1);
    }

    if(! Clnk_ETH_UpdateDebugInfo(iface->pvContext, iface->core_buf))
    {
        log_msg(iface, L_ERR, "error fetching debug info\n");
        close(fd);
        return(-1);
    }

    if(write(fd, iface->core_buf, iface->core_len) != iface->core_len)
    {
        log_msg(iface, L_ERR, "error writing debug info: %s\n", strerror(errno));
        close(fd);
        return(-1);
    }
    close(fd);
    return(0);
}

/**
 * \brief Reset and boot the SoC
 *
 * Calls into ClnkEth to reset and boot the SoC.
 *
 * If the user has enabled verbose output, the download time will be logged.
 */

int mii_do_reset(struct mii_iface *iface)
{
    struct timeval t_start, t_done;
    int ret;
    int timediff;
    Context_t *pContext = iface->pvContext; 

#ifdef AEI_WECB
    aei_gettimeofday(&t_start, NULL);
#else
    gettimeofday(&t_start, NULL);
#endif
    log_msg(iface, L_VERBOSE, "Starting SoC reset sequence...\n");

    if(clnk_net_carrier_off(iface->iface_dev))
            log_msg(iface, L_VERBOSE, "ioctl: mbx de-init failed\n");

#ifdef ETH_SUPPLY_25M_FROM_XTAL
#if CLNK_CTRL_PATH != CTRLPATH_FLEXBUS
    /* wiggle hardware RESET pin */
//    if(nic_reset(iface))
//    {
//        return(-1);
//    }
#endif
#endif

    ret = Clnk_ETH_Control(iface->pvContext, CLNK_ETH_CTRL_RESET, 0, 0, 0);

    if (ret == 0 )
    { // 0 is success, SOC booted
        // clnk_eth_soc_booted is an io call to set socBooted and open Eth interface 

        ClnkDef_dataPlaneVars_t dp;
        dp.swConfig = 0xdead; 
        dp.unsol_msgbuf = pContext->pSwUnsolQueue;
        log_msg(iface, L_VERBOSE, "swConfig = %x, unsol_msgbuf = %x\n", dp.swConfig,dp.unsol_msgbuf );
        if(clnk_eth_soc_booted(iface->iface_dev, &dp))
            log_msg(iface, L_VERBOSE, "ioctl: ETH_SOC_BOOTED failed\n");
        else
            log_msg(iface, L_VERBOSE, "ioctl: ETH_SOC_BOOTED success\n");
    }
    else
    {
        log_msg(iface, L_ERR, "AEI failed Clnk_ETH_Control()\n");
    }

#ifdef AEI_WECB
    aei_gettimeofday(&t_done, NULL);
#else
    gettimeofday(&t_done, NULL);
#endif
    timediff = ((t_done.tv_sec - t_start.tv_sec) * 1000000) +
               (t_done.tv_usec - t_start.tv_usec);
    log_msg(iface, L_VERBOSE,
            "CLNK_ETH_CTRL_RESET returned status %d after %d.%03d seconds\n",
            ret, timediff / 1000000, (timediff % 1000000) / 1000);

#if ECFG_BOARD_ECB_4M_L3
	system("mbrg -i");
#endif

    return(ret);
}

/**
 * \brief Main clinkd processing loop.
 *
 * This is the main clinkd loop.  It monitors SoC status and link status,
 * processes incoming queries, and services unsolicited messages.
 *
 * It exits under the following circumstances:
 *
 * Catastrophic SoC or MII access problems
 * SIGHUP, SIGINT, or SIGTERM received
 */

int mii_mainloop(struct mii_iface *iface)
{
    uint status;
    Context_t* pContext = iface->pvContext;

    while(1)
    {
        if (!iface->stopped)
        {
           /*Here different with orignal ECB3M-L3 branch code*/
           /* check for unsolicited mailbox messages and replies to host messages */
          // Clnk_ETH_HandleMiscInterrupt(iface->pvContext);

            status = 0;
            /* check for resets, timeouts, etc. (unless we are in a test mode) */
            if(! iface->test_mode)
            {
                status = GetSocStatus(pContext);
            }

            if(iface->force_reset)
            {
                iface->force_reset = 0;
                status = CLNK_DEF_SOC_STATUS_FORCED_RESET;
            }
            if((pContext->socStatus = status) != 0)
            {
                uint32_t mbx_read_csr = 0xdeadbeef, debug_1 = 0xdeadbeef;

                mii_set_link(iface, LINK_DOWN);
                socRegRead(iface->iface_dev, CLNK_REG_MBX_READ_CSR, &mbx_read_csr);
                socRegRead(iface->iface_dev, CLNK_REG_DEBUG_1, &debug_1);
                log_msg(iface, L_INFO, "Clink Reset Cause :0x%x Reg:0x%x Dbg:0x%x, io_call_F:%d\n",
                        status, mbx_read_csr, debug_1, iface->io_call_F);

                if(iface->core_path)
                    clnkcore_save(iface);

                if((iface->no_reset) && (status != CLNK_DEF_SOC_STATUS_FORCED_RESET))
                {
                    log_msg(iface, L_INFO, "no_reset mode active; ^C to exit\n");
                    while(! (got_sighup || got_sigterm))
                    {
                        mii_process_queries(iface);
                    }
                    return(0);
                }

                if(iface->one_shot)
                {
                    log_msg(iface, L_INFO,
                            "One-shot mode requested; exiting without reset.\n");
                    return(0);
                }

                if(mii_do_reset(iface))
                {
                    log_msg(iface, L_ERR, "CLNK_ETH_CTRL_RESET failed\n");
                    return(-1);
                }
            }

            status = CLNK_ETH_LINK_DOWN;

            if (! iface->test_mode)
            {
                GET_LINK_STATUS_REG(pContext, &status);
                status &= 0xff;
            }

#if ! defined(USE_FLEX_CTRL)
#if 0 /* ACTIONTEC_MOCA */
            if (! iface->test_mode)
            {
               if (status != CLNK_ETH_LINK_UP)
               {
                  if (mii_read(iface, MMI_REG_STATUS) & (1 << 2))
                  {
                     //log_msg(iface, L_INFO, "Read MMI_REG_STATUS one more time\n");
                     status = CLNK_ETH_LINK_UP;
                  }
#if 0
                  else
                  {
                     log_msg(iface, L_INFO, "MMI_REG_STATUS detected LINK DOWN.\n");
                  }
#endif
               }
            }
#endif
            mii_set_link(iface, (status == CLNK_ETH_LINK_UP) ? LINK_UP : LINK_DOWN);
#else
            if (iface->link_status != status)
            {
                iface->link_status = status;
                log_msg(iface, L_INFO, "Clink Link Up\n");
                system("cli -s Device.MoCA.Interface.1.X_ACTIONTEC_COM_Status string Up >/dev/null");
            }
            log_msg(iface, L_INFO, "Clink status2 0x%x\n", status);
#endif
        }

        /*
         * check for user queries and process them
         *
         * NOTE: This function call must take 2 seconds.  It is used to ensure
         * that each steady state iteration through this loop takes 2 seconds.
         * This is because GET_SOC_STATUS is intended to be called from a
         * 2-second periodic timer.
         */

        mii_process_queries(iface);

#if defined(L3_DONGLE_HIRF)

        FILE *float_cancel;
        if ((float_cancel=fopen("mmi_d_exit","r")))
        {
            got_sigterm = 1;
            fclose(float_cancel);
            return(0);
        }
#endif
        if(got_sighup || got_sigterm)
        {
            mii_set_link(iface, LINK_DOWN);
            return(0);
        }
    }
}


/**
 * \brief Initialize RNG for --bus-test mode
 */
void rng_init(struct mii_iface *iface, int round)
{
    const uint32_t x_init[RNG_N] =
    {
        0x95f24dab, 0x0b685215, 0xe76ccae7, 0xaf3ec239, 0x715fad23,
        0x24a590ad, 0x69e4b5ef, 0xbf456141, 0x96bc1b7b, 0xa7bdf825,
        0xc1de75b7, 0x8858a9c9, 0x2da87693, 0xb657f9dd, 0xffdc8a9f,
        0x8121da71, 0x8b823ecb, 0x885d05f5, 0x4e20cd47, 0x5a9ad5d9,
        0x512c0c03, 0xea857ccd, 0x4cc1d30f, 0x8891a8a1, 0xa6b7aadb,
    };
    int i;

    for(i = 0; i < RNG_N; i++)
        iface->rng_x[i] = x_init[(i + round) % RNG_N];
    iface->rng_k = 0;
}

/**
 * \brief Get next random number for bus test
 *
 * Produces a 32-bit random number.
 * Based on the algorithm illustrated in TT800, Copyright (C) 1996, Makoto Matsumoto
 */
uint32_t rng_get(struct mii_iface *iface)
{
    const uint32_t mag01[2] = { 0, 0x8ebfd028 };
    uint32_t out;

    if(iface->rng_k == RNG_M)
    {
        /* reached the end of the "x" array; generate new version */
        int i;

        for(i = 0; i < RNG_N; i++)
        {
            if(i < (RNG_N - RNG_M))
            {
                iface->rng_x[i] = iface->rng_x[i + RNG_M] ^ (iface->rng_x[i] >> 1) ^
                    mag01[iface->rng_x[i] & 1];
            } else {
                iface->rng_x[i] = iface->rng_x[i + RNG_M - RNG_N] ^ (iface->rng_x[i] >> 1) ^
                    mag01[iface->rng_x[i] & 1];
            }
        }
        iface->rng_k = 0;
    }
    out = iface->rng_x[iface->rng_k];
    out ^= (out << 7) & 0x2b5b2500;
    out ^= (out << 15) & 0xdb8b0000;
    iface->rng_k++;
    return(out);
}

/**
 * \brief Run bus test loop; abort on signal from user
 */
int run_bus_test(struct mii_iface *iface)
{
    int round = 0;
    uint32_t *buf;
    uint32_t tmp, total0, total1;
    int i;

    buf = (uint32_t *)malloc(ISPRAM_SIZE);
    if(! buf)
    {
        log_msg(iface, L_ERR, "unable to allocate buffer for test\n");
        return(-1);
    }
    log_msg(iface, L_INFO, "Starting bus test\n");

    /* sanity check PHY registers */
    log_msg(iface, L_VERBOSE, "MDIO registers:");
    total1 = 0;
    total0 = 0xffff;
    for(i = 0; i < 32; i++)
    {
        if(! (i & 7))
            log_msg(iface, L_VERBOSE, "\n    ");
        tmp = mii_read(iface, i);
        log_msg(iface, L_VERBOSE, "%04x ", tmp);
        total1 |= tmp;
        total0 &= tmp;
    }
    log_msg(iface, L_VERBOSE, "\n");
    if((total1 == 0x0) || (total0 == 0xffff))
    {
        log_msg(iface, L_ERR, "MDIO interface appears to be stuck.  Aborting\n");
        return(-1);
    }

    /* reset the SoC */
    socRegWrite(iface->iface_dev, EHI_SYS_RESET_SET, 0xffffffff);
    socRegRead(iface->iface_dev, EHI_SYS_RESET_SET, &tmp);
    usleep(100000);
    socRegWrite(iface->iface_dev, EHI_SYS_RESET_SET, 0);
    socRegRead(iface->iface_dev, EHI_SYS_RESET_SET, &tmp);
    usleep(100000);
   
    /* test Sonics access until interrupted */
    while(1)
    {
        uint32_t wr, rd;
        uint32_t start, len;
        char name[16];
        struct timeval t_start, t_done;
        int timediff;

#ifdef AEI_WECB
        aei_gettimeofday(&t_start, NULL);
#else
        gettimeofday(&t_start, NULL);
#endif
        if(round & 2)
        {
            start = ISPRAM_START;
            len = ISPRAM_SIZE;
            strcpy(name, "ISPRAM");
        } else {
            start = DSPRAM_START;
            len = DSPRAM_SIZE;
            strcpy(name, "DSPRAM");
        }
        if(round & 1)
        {
            /* fast test (burst) */
            rng_init(iface, round);
            for(i = 0; i < len; i += 4)
                buf[i >> 2] = rng_get(iface);

            socMemWriteBurst(iface->iface_dev, start, buf, len);
            if(got_sighup || got_sigterm)
                goto out;
            memset(buf, 0, len);
            socMemReadBurst(iface->iface_dev, start, buf, len);
            if(got_sighup || got_sigterm)
                goto out;

            rng_init(iface, round);
            for(i = 0; i < len; i += 4)
            {
                wr = rng_get(iface);
                if(buf[i >> 2] != wr)
                {
                    log_msg(iface, L_ERR, "Round %d: fast write to %s: FAILED\n", round, name);
                    log_msg(iface, L_ERR, "Wrote %08x to %08x, got %08x\n", wr,
                            start + i, buf[i >> 2]);
                    return(-1);
                }
            }
            log_msg(iface, L_INFO, "Round %d: fast write to %s: PASSED", round, name);
        } else {
            /* slow test */
            rng_init(iface, round);
            for(i = 0; i < len; i += 4)
            {
                if(got_sighup || got_sigterm)
                    goto out;
                socRegWrite(iface->iface_dev, start + i, rng_get(iface));
            }
            rng_init(iface, round);
            for(i = 0; i < len; i += 4)
            {
                if(got_sighup || got_sigterm)
                    goto out;
                socRegRead(iface->iface_dev, start + i, &rd);
                wr = rng_get(iface);
                if(rd != wr)
                {
                    log_msg(iface, L_ERR, "Round %d: slow write to %s: FAILED\n", round, name);
                    log_msg(iface, L_ERR, "Wrote %08x to %08x, got %08x\n", wr,
                            start + i, rd);
                    return(-1);
                }
            }
            log_msg(iface, L_INFO, "Round %d: slow write to %s: PASSED", round, name);
        }
#ifdef AEI_WECB
        aei_gettimeofday(&t_done, NULL);
#else
        gettimeofday(&t_done, NULL);
#endif
        timediff = ((t_done.tv_sec - t_start.tv_sec) * 1000000) +
            (t_done.tv_usec - t_start.tv_usec);
        log_msg(iface, L_INFO, ", time=%d.%03d seconds\n", timediff / 1000000,
                (timediff % 1000000) / 1000);
        round++;
    }

out:
    log_msg(iface, L_INFO, "User aborted - exiting\n");
    return(0);
}

/**
 * \brief Set up POSIX signal handlers
 */
void sig_setup(void)
{
    signal(SIGHUP, &signal_handler);
    signal(SIGINT, &signal_handler);
    signal(SIGTERM, &signal_handler);
    signal(SIGPIPE, SIG_IGN);
}

/**
 * \brief Read a 16-bit word from the e1000 EEPROM on the GMII board
 */
static int nic_eep_read(struct mii_iface *iface, int offset)
{
    struct ifreq ifr;
    struct
    {
        struct ethtool_eeprom eeprom;
        uint8_t data[2];
    } edata;

    memset(&ifr, 0, sizeof(ifr));
    memset(&edata, 0, sizeof(edata));

    edata.eeprom.cmd = ETHTOOL_GEEPROM;
    edata.eeprom.len = 2;
    edata.eeprom.offset = offset << 1;

    strcpy(ifr.ifr_name, iface->iface_name);
    ifr.ifr_data = (caddr_t)&edata.eeprom;

    if(ioctl(iface->fd, SIOCETHTOOL, &ifr) < 0)
    {
        log_msg(iface, L_ERR, "ETHTOOL_GEEPROM failed: %s\n", strerror(errno));
        return(-1);
    }
    return((edata.data[1] << 8) | (edata.data[0]));
}

/**
 * \brief Perform a HW reset on the EN2210/EN2510
 *
 * Uses the RESET pin to reinitialize the EN2210/EN2510, then sets the MII BMCR
 * register to reflect the desired line speed.
 */
static int nic_reset(struct mii_iface *iface)
{
    struct ifreq ifr;
    int *flag = &ifr.ifr_ifru.ifru_ivalue;
#if 1 /* ACTIONTEC_MOCA */
    int moca_id = 0xDECA0; 
#endif

    memset(&ifr, 0, sizeof(ifr));


    strcpy(ifr.ifr_name, iface->iface_name);

    /**
     * EN2210 reset sequence: Hold PLL, Hold SoC, Run PLL, Run SoC 
     * EN2510 reset sequence: Hold SoC, Run SoC
     */
    if (ECFG_CHIP_ZIP2)
    {
        /* Hold PLL */
        *flag = 4;
#if 1 /* ACTIONTEC_MOCA */
        *flag |= moca_id;
#endif
        if(ioctl(iface->fd, SIOCHDRCMD, &ifr) < 0)
        {
            log_msg(iface, L_ERR, "Error holding PLL\n");
            return(-1);
        }
    }

    /* Hold SoC */
    *flag = 1;
#if 1 /* ACTIONTEC_MOCA */
    *flag |= moca_id;
#endif
    if(ioctl(iface->fd, SIOCHDRCMD, &ifr) < 0)
    {
        log_msg(iface, L_ERR, "Error holding SoC\n");
        return(-1);
    }

    /* deassert reset line (leave asserted for FPGA or external SoC) */
    if(! iface->external)
    {
        if (ECFG_CHIP_ZIP2)
        {   
            // assure the PLL reset is stable, hold PLL at least 1ms
            usleep(1000);

            /* Run PLL */
            *flag = 5;
#if 1 /* ACTIONTEC_MOCA */
            *flag |= moca_id;
#endif
            if(ioctl(iface->fd, SIOCHDRCMD, &ifr) < 0)
            {
                log_msg(iface, L_ERR, "Error clearing PLL reset line\n");
                return(-1);
            }
        }
        
        usleep(50000); // deassert SoC reset after 50ms

        /* Run SoC */
        *flag = 0;
#if 1 /* ACTIONTEC_MOCA */
        *flag |= moca_id;
#endif
        if(ioctl(iface->fd, SIOCHDRCMD, &ifr) < 0)
        {
            log_msg(iface, L_ERR, "Error clearing SoC reset line\n");
            return(-1);
        }
        /* wait for reset to clear */
        usleep(500000);
        mii_read(iface, MMI_REG_STATUS);
        mii_write(iface, MMI_REG_ENH_ADDR_MODE, 0);
    }

    if(ECFG_CHIP_MAVERICKS)
    {
        /*Wait for the clock signal (25Mhz) stable here.*/
        usleep(10000);
        //----------------------------------------------------------------------
        // Deassert the Marvell switch RESET line here.
        //----------------------------------------------------------------------
/*
	From entropic open issue 8720
	There is a benign write system call in c.LINK driver in the function 'nic_reset' in the
	CandD driver mii_main.c . The call is specific to the ECB and not necessary in the
	production code.
	if(ECFG_CHIP_MAVERICKS)
	{
	Wait for the clock signal (25Mhz) stable here.
	usleep(10000);
	//----------------------------------------------------------------------
	// Deassert the Marvell switch RESET line here.
	//----------------------------------------------------------------------
	write(iface->fd,"CPC8=|",6);
	}
	The E1000 and PCIe drivers do not support the write system call. The write system call
	will fail and the return code is not checked.
	Work around : remove the following line - write(iface->fd,"CPC8=|",6);
*/
        // write(iface->fd,"CPC8=|",6);
    }
    return(0);
}

/**
 * \brief Set ethtool options on NIC
 *
 * Performs ethtool setup: pause frames, speed, etc.
 */
static int nic_set_ethtool(struct mii_iface *iface)
{
    struct ifreq ifr;
    struct ethtool_pauseparam epause;
    struct ethtool_cmd ecmd;
    uint8_t *tmp;

    memset(&ifr, 0, sizeof(ifr));
    memset(&epause, 0, sizeof(epause));
    memset(&ecmd, 0, sizeof(ecmd));

    /* get current settings */
    ecmd.cmd = ETHTOOL_GSET;
    strcpy(ifr.ifr_name, iface->iface_name);
    ifr.ifr_data = (caddr_t)&ecmd;

    if(ioctl(iface->fd, SIOCETHTOOL, &ifr) < 0)
    {
        if(errno == ENODEV)
        {
            log_msg(iface, L_ERR, "No such network interface - '%s'\n", iface->iface_name);
        } else {
            log_msg(iface, L_ERR, "ETHTOOL_GSET failed\n");
        }
        return(-1);
    }

    /* set new speed and duplex; disable autonegotiation */
    ecmd.duplex = DUPLEX_MODE_USED;
    ecmd.speed = SPEED_10;

    if(iface->soc_hw)
    {
        switch(MII_SPD_variable)
        {
            case 100:
                ecmd.speed = SPEED_100;
                break;
            case 1000:
                ecmd.speed = SPEED_1000;
                break;
        }
    }
    ecmd.autoneg = AUTONEG_DISABLE;
    ecmd.cmd = ETHTOOL_SSET;

    if(ioctl(iface->fd, SIOCETHTOOL, &ifr) < 0)
    {
        log_msg(iface, L_ERR, "ETHTOOL_SSET failed\n");
        return(-1);
    }

    /* get current pause params */
    ifr.ifr_data = (caddr_t)&epause;
    epause.cmd = ETHTOOL_GPAUSEPARAM;

    if(ioctl(iface->fd, SIOCETHTOOL, &ifr) < 0)
    {
        log_msg(iface, L_ERR, "ETHTOOL_GPAUSEPARAM failed\n");
        return(-1);
    }

    /* set new pause params: RX on, TX off, no autoneg */
    epause.autoneg = AUTONEG_DISABLE;
    epause.rx_pause = 1;
    epause.tx_pause = 0;
    epause.cmd = ETHTOOL_SPAUSEPARAM;

    if(ioctl(iface->fd, SIOCETHTOOL, &ifr) < 0)
    {
        log_msg(iface, L_ERR, "ETHTOOL_SPAUSEPARAM failed\n");
        return(-1);
    }

    /* fetch MII MAC address to use as clink MAC address */
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, iface->iface_name);
    if(ioctl(iface->fd, SIOCGIFHWADDR, &ifr) < 0)
    {
        log_msg(iface, L_ERR, "SIOCGIFHWADDR failed\n");
        return(-1);
    }
    tmp = (uint8_t *)&ifr.ifr_hwaddr.sa_data;

#if defined(L3_DONGLE_HIRF)
    int mac_file;
    char wk[32];
    int amt;
    int ix;
    int s;
    int val;
    mac_file=open("/proc/otp/ECB_cLink_esa",O_RDONLY);
    if (mac_file >= 0)
    {
        amt=read(mac_file,wk,32);
        ix=0;
        val=0;
        for (s=0;s<amt;s++)
        {
            if (wk[s] == ':')
            {
                MAC_address_bytes[ix] = tmp[ix] = val;
                val=0;
                ix = ix < 5 ? ix+1 : ix;
            }
            else
            {
                if ('0' <= wk[s] && wk[s] <= '9')
                    val=(val << 4) | (wk[s] - '0');
                if ('a' <= wk[s] && wk[s] <= 'f')
                    val=(val << 4) | (wk[s] - 'a'+ 10);
                if ('F' <= wk[s] && wk[s] <= 'F')
                    val=(val << 4) | (wk[s] - 'A'+ 10);
            }
        }
        MAC_address_bytes[ix] = tmp[ix]=val;
        close(mac_file);
    }

/*
Open Issue #13739 get from entropic
The MAC address specified on the daemon¡¯s command line (--mac-addr) may be overwritten
 by the hardcoded value of 00:09:8b:30:40:50. This issue is caused by an unintended SIOCGIFHWADDR
 ioctl call in file mii_main.c when the SDK has been compiled for customer platforms.
*/
    iface->mac_addr_hi = (tmp[0] << 24) |
                         (tmp[1] << 16) |
                         (tmp[2] << 8) |
                         (tmp[3] << 0);
    iface->mac_addr_lo = (tmp[4] << 24) |
                         (tmp[5] << 16);
#endif

    return(0);
}

int nic_init(struct mii_iface *iface, char *iface_name, struct clnk_hw_params *hparms)
{
    int fd;
    struct ifreq ifr;
    struct ethtool_drvinfo drvinfo;
    struct mii_ioctl_data *miidata = (struct mii_ioctl_data *)&ifr.ifr_data;

    if (clnk_init_pci_xmii(iface->iface_name, &iface->iface_dev) < 0) 
    {
        log_msg(iface, L_ERR, "clnk_init_pci_xmii failed\n");
        return(-1);
    }
    // store valid file descriptor to iface
    clnk_ctx_t *ctx = (clnk_ctx_t *)iface->iface_dev;
    iface->fd = ctx->unix_fd;

    log_msg(iface, L_ERR, "iface->fd = %x, ctx->clnkdvr = %x \n",iface->fd,ctx->clnkdvr );
    strcpy(iface->iface_name, iface_name);
    
    fd = iface->fd;

#if (ECFG_BOARD_ECB_3M_L3==1)
    FILE *f100;
    if ((f100=fopen("/100","r")) != NULL)
    {
    
        fclose(f100);
        printf("DETECTED REQUST FOR 100 MBps operation\n");
        MII_SPD_variable=100;
    }
    else
    {
        if ((mii_read(iface,0x1000 + 0x102) & 0xc) == 0x00)
        {
            MII_SPD_variable = 200;
        }
        else
        {
            MII_SPD_variable = 1000;
        }
    }
    if (f100)
    {
        fclose(f100);
    }
//--------------------------------------------------------------------------
// Set the configuration of the Tantos switch port 5 to either GMII mode or
// MII mode
//--------------------------------------------------------------------------
    mii_write(iface,0x1000 + 0xf5,(mii_read(iface,0x1000 + 0xf5) & 0xf0f) |
    (MII_SPD_variable==1000 ? 0xb0 : 0x70));
//--------------------------------------------------------------------------
// Toggle the link up by forcing link down, then link up for a molment thus
// establishing the port speed
//--------------------------------------------------------------------------
    mii_write(iface,0x1000 + 0xa1,0x02);
    mii_write(iface,0x1000 + 0xa1,0x04);

#else
    MII_SPD_variable = MII_SPD;
#endif
    /* detect GMII reference board */
    memset(&ifr, 0, sizeof(ifr));
    memset(&drvinfo, 0, sizeof(drvinfo));

    drvinfo.cmd = ETHTOOL_GDRVINFO;
    strcpy(ifr.ifr_name, iface->iface_name);
    ifr.ifr_data = (caddr_t)&drvinfo;

    /* if this is a GMII reference board, read the settings from EEPROM */
    if(ioctl(fd, SIOCETHTOOL, &ifr) == 0)
    {
        if(! strcmp(drvinfo.driver, GMII_DRV_NAME))
        {
            int cfg;
            unsigned int flags;

            if(nic_eep_read(iface, EEPROM_Z2_SIG) != Z2_SIG_MAGIC)
            {
                log_msg(iface, L_ERR, "Bad 'Z2' magic in EEPROM - run eeprg to correct\n");
                return(-1);
            }
            cfg = nic_eep_read(iface, EEPROM_Z2_CFG);
            if(cfg == -1)
                return(-1);
            iface->phy_id = cfg & 0xff;
            flags = cfg >> 8;
            if(! (flags & Z2_FLAG_FPGA))
                iface->soc_hw = 1;
            if((flags & Z2_FLAG_EXTERNAL) || (flags & Z2_FLAG_FPGA))
                iface->external = 1;

            log_msg(iface, L_VERBOSE,
                    "GMII reference board detected: %s %s at PHY addr %d, speed %d\n",
                    ECFG_CHIP_MAVERICKS ? "EN2510" : "EN2210",
                    iface->soc_hw ? (iface->external ? "ext SoC" : "SoC") : "FPGA",
                    iface->phy_id, MII_SPD_variable);
            /*
             * GMII boards are strapped for either MII or GMII.  This
             * affects whether the MII TX clock is inbound or outbound,
             * relative to the Intel GMAC.
             *
             * When the EEPROM is programmed with eeprg, the "-G" switch is
             * used if the board is strapped for GMII.
             */
            if((cfg >> 8) & Z2_FLAG_GMII)
            {
                if(MII_SPD_variable != 1000)
                {
                    log_msg(iface, L_ERR, "Error: GMII board is configured "
                            "for 1000Mbps, but MII_SPD_variable is %d\n", MII_SPD_variable);
                    return(-1);
                }
            } else {
                if(MII_SPD_variable == 1000)
                {
                    log_msg(iface, L_ERR, "Error: GMII board is not configured "
                            "for 1000Mbps, but MII_SPD_variable is %d\n", MII_SPD_variable);
                    return(-1);
                }
            }
        } else {
            log_msg(iface, L_VERBOSE, "GMII reference board not detected\n");

            /* not a GMII reference board */
            iface->soc_hw = 1;

            /* get PHY ID from driver */
            memset(&ifr, 0, sizeof(ifr));

#if defined(FORCE_PHY_ID)
            iface->phy_id = FORCE_PHY_ID;
#else /* defined(FORCE_PHY_ID) */
            if(! iface->phy_id)
            {
                /* read default PHY ID for interface */
                strcpy(ifr.ifr_name, iface_name);
                if(ioctl(fd, SIOCGMIIPHY, &ifr) < 0)
                {
                    log_msg(iface, L_ERR, "SIOCGMIIPHY failed: %s\n", strerror(errno));
                    return(-1);
                }
                iface->phy_id = miidata->phy_id;
            }
#endif /* defined(FORCE_PHY_ID) */
            log_msg(iface, L_VERBOSE,
                    "SoC MII PHY address is 0x%x\n", iface->phy_id);
        }
    }

    /* set up driver params and hard-reset SoC */
    if(nic_set_ethtool(iface) || nic_reset(iface))
        return(-1);

    /* create initial BMCR and GPHY CSR settings to set MII speed */
    if(iface->soc_hw)
    {
        switch(MII_SPD_variable)
        {
            case 10:
                hparms->mii_bmcr = BMCR_FULLDPLX;
                hparms->mii_clk0 = CSC_CLK_CTL0_MII_PHY;
                hparms->mii_clk1 = 0x0;
                hparms->mii_mac_cfg = 0x880c;
                hparms->mii_enh_ctrl = 0xc000;
                hparms->gmii = 0;
                break;
            case 100:
                hparms->mii_bmcr = BMCR_SPEED100 | BMCR_FULLDPLX;
                hparms->mii_clk0 = CSC_CLK_CTL0_MII_PHY;
                hparms->mii_clk1 = 0x100100;
                hparms->mii_mac_cfg = 0xc80c;
                hparms->mii_enh_ctrl = 0xc000;
                hparms->gmii = 0;
                break;
            case 200:
                hparms->mii_bmcr = BMCR_SPEED100 | BMCR_FULLDPLX;
                hparms->mii_clk0 = CSC_CLK_CTL0_MII_PHY;
                hparms->mii_clk1 = 0x400200;
                hparms->mii_mac_cfg = 0xc80c;
                hparms->mii_enh_ctrl = 0xc000;
                hparms->gmii = 1;  /* EPHY config is same for TMII/GMII modes */
                break;
            case 1000:
                hparms->mii_bmcr = BMCR_SPEED1000 | BMCR_FULLDPLX;
                hparms->mii_clk0 = CSC_CLK_CTL0_GMII_PHY;
                hparms->mii_clk1 = 0x700100;
                hparms->mii_mac_cfg = 0x80c;
                hparms->mii_enh_ctrl = 0x4000;
                hparms->gmii = 1;
#if ECFG_DATAPATH_RGMII==1
                // gmii == 2, means RGMII is selected!
                hparms->gmii = 2;
                hparms->mii_clk0 = CSC_CLK_CTL0_RGMII_PHY;
#endif
                break;
        }
    } else {
        hparms->mii_clk0 = 0x0;
        hparms->mii_clk1 = 0x100100;
        hparms->mii_bmcr = BMCR_FULLDPLX;
        hparms->mii_mac_cfg = 0x880c;
        hparms->mii_enh_ctrl = 0xc000;
        hparms->gmii = 0;
    }
    return(0);
}

void print_usage(void)
{
    printf("\n");
    printf("Usage: clinkd [options] specifications interface\n");
    printf("  <interface>         Ethernet interface name, such as eth0\n");
    printf("\nSpecifications:\n");
    printf("  --mac-addr <addr>   Sets clink mac address, format: 00:00:00:00:00:00\n");
    printf("  --firmware <path>   Sets SoC firmware file path\n");
    printf("  --microcode <path>  Sets TC microcode file path\n");
    printf("\nNormal Options:\n");
    printf("  -D                  Don't fork, and write all messages to stdout\n");
    printf("  -t                  Timestamp output messages\n");
    printf("  -f <path>           Read clink config file from specified path. Applicable to Linux only.\n");
    printf("                      If path is not specified, clinkd will look in \"/etc\" folder.\n");
    printf("                      If clinkd can not locate the clink.conf file, program will exit.\n");
    printf("  -p <path>           Read feic config file from specified path. Applicable to Linux only.\n");
    printf("                      If path is not specified, clinkd will look in \"/etc\" folder.\n");
    printf("                      If clinkd can not locate the feic.conf file, program will exit.\n");
    printf("  -v[v]               Increase verbosity; use -vv for maximum output\n");
    printf("\nDebugging / Test Options:\n");
    printf("  --debug-soc         Pause during SoC initialization to debug firmware\n");
    printf("  --core <path>       Write SoC core file to path on crash\n");
    printf("  --one-shot          Exit after the first SoC reset or lockup (don't loop)\n");
    printf("  --no-reset          Don't reset SoC on error\n");
    printf("  --test-mode <mode>  Set test mode to mode (0=modulated, 2=CW)\n");
    printf("  --no-embedded       Do not boot the embedded or check status\n");
    printf("  --bus-test          Test the control path bus until interrupted\n");
    printf("\n");
}

/**
 * \brief Parse the user-supplied command line options
 *
 * \retval -1 Failure
 * \retval 0 Success
 */
int parse_cmdline(struct mii_iface *iface, int argc, char **argv)
{
    const struct option longopt[] =
    {
        /* short/long options */
        { "dont-fork",      0, NULL, 'D' },
        { "conf",           1, NULL, 'f' },
        { "feicconf",       1, NULL, 'p' },
        { "verbose",        0, NULL, 'v' },
        { "help",           0, NULL, 'h' },
        /* long options */
        { "debug-soc",      0, NULL, 1 },
        { "firmware",       1, NULL, 2 },
        { "core",           1, NULL, 3 },
        { "one-shot",       0, NULL, 4 },
        { "test-mode",      1, NULL, 5 },
        { "no-embedded",    0, NULL, 6 },
        { "bus-test",       0, NULL, 7 },
        { "no-reset",       0, NULL, 8 },
        { "mac-addr",       1, NULL, 9 },
        { "microcode",      1, NULL,10 },
#if 0
    { "evm-tags",       0, NULL, 11 },
#endif    
        { "mac-mode",       0, NULL, 12 },
        { NULL,             0, NULL, 0 },
    };
    int ret;
    int verb = 0, show_help = 0, mc_seen = 0, fw_seen = 0;
    char *progname = argv[0] ;

    while((ret = getopt_long(argc, argv, "Df:p:vht", longopt, NULL)) != -1)
    {
        switch(ret)
        {
            case 1:     /* --debug-soc */
                iface->wait_for_debugger = 1;
                iface->no_syslog = 1;
                iface->no_fork = 1;
                break;
            case 2:     /* --firmware /path/to/firmware */
                iface->firmware_path = optarg;
                fw_seen = 1 ;
                break;
            case 3:     /* --core /path/to/clnkcore */
                iface->core_path = optarg;
                break;
            case 4:     /* --one-shot */
                iface->one_shot = 1;
                break;
            case 5:     /* --test-mode */
                iface->test_mode = atoi(optarg) | TEST_MODE_SEL;
                break;
            case 6:     /* --no-embedded */
                iface->no_embedded = 1;
                iface->test_mode = NO_EMB_SEL;
                break;
            case 7:     /* --bus-test */
                iface->bus_test = 1;
                iface->no_fork = 1;
                iface->no_syslog = 1;
                break;
            case 8:     /* --no-reset */
                iface->no_reset = 1;
                break;
            case 9:     /* --mac-addr */
            {   // mac format: 00:09:8b:0f:f0:16
                static char  *mac_addr;
                unsigned char my_mac_addr[6];
                unsigned char myaddr[6];
                int i;
                memset(my_mac_addr, 0, 6);
                mac_addr = optarg;
                for (i=0; i < 6; i++) 
                {
                    char c1 = mac_addr[i*3];
                    char c2 = mac_addr[i*3+1];
                    if (i != 5 && mac_addr[i*3+2] != ':') break;
                    if (!isxdigit(c1) || !isxdigit(c2)) break;
                    c1 = isdigit(c1) ? c1 - '0' : tolower(c1) - 'a' + 10;
                    c2 = isdigit(c2) ? c2 - '0' : tolower(c2) - 'a' + 10;
                    myaddr[i] = (unsigned char) c1 * 16 + (unsigned char) c2;
                }   
                if (i == 6)
                    memcpy(my_mac_addr, myaddr, 6);

                iface->mac_addr_hi  = (my_mac_addr[0] << 24) | (my_mac_addr[1] << 16) |
                                      (my_mac_addr[2] << 8)  | (my_mac_addr[3]);
                iface->mac_addr_lo  = (my_mac_addr[4] << 24) | (my_mac_addr[5] << 16);
            }
                break;
            case 10:     /* --microcode /path/to/microcode/file */
                iface->microcode_path = optarg;
                mc_seen = 1 ;
                break;
            case 12:
                mac_mode=1;
                break;
            case 'D':   /* don't fork */
                iface->no_fork = 1;
                iface->no_syslog = 1;
                break;
            case 'f':   /* path to clink.conf */
                iface->conf_path = optarg;
                break;
            case 'p':   /* path to feic.conf */
                iface->feicconf_path = optarg;
                break;
            case 't':   /* enable timestamps */
                iface->timestamp = 1;
                break;
            case 'v':
                verb++;
                break;
            case 'h':
            default:
                show_help = 1;
                break;
        }
    }

    // error checking - missing items

    if( argc != 1 ) {
        // mac-addr is an optional argument in xMII, unlike PCI where it is required
        if( !mc_seen )
        {
#if (COMPILED_IN_BINARY_CODE == 0)
            printf("%s: --microcode <path> is required.\n", progname);
            show_help = 1;
#endif
        } 
        if( !fw_seen )
        {
#if (COMPILED_IN_BINARY_CODE == 0)
            printf("%s: --firmware <path> is required.\n", progname);
            show_help = 1;
#endif
        } 

        /* we take exactly one non-option parameter: the interface name */
        if( (optind + 1) != argc)
        {
            printf("%s: interface must be last on the command line.\n", progname );
            show_help = 1;
        } else {
            iface->iface_name = argv[optind];
            if( strlen(iface->iface_name) >= MAX_IFACE_NAME ) {
                printf("%s: interface name longer than %d characters.\n", progname, MAX_IFACE_NAME );
                show_help = 1;
            }
        }
    } else {
        show_help = 1;      // no args get the help
    }

    /* compute minimum loglevel based on verbosity */
    switch(verb)
    {
        case 0:
            iface->min_log_prio = L_INFO;
            break;
        case 1:
            iface->min_log_prio = L_VERBOSE;
            break;
        default:
            iface->min_log_prio = L_DBG;
            break;
    }

    if(show_help)
    {
        print_usage();
        return(-1);
    } else {
        return(0);
    }
}

int main(int argc, char **argv)
{
    int                 ret;
    void                *pvContext;
    Clnk_ETH_Firmware_t fw;
    clnk_iface_t        iface;
    struct fw_img       *elf_fw;
    struct fw_img       *uc_fw;
    struct netio_cfg    iocfg;
    struct netio_ctx    *ioctx;
    struct clnk_hw_params hparms;
    char                syslog_tag[32];
    Context_t           *pContext;

    memset((void *)&iface, 0, sizeof(iface));
    memset((void *)&hparms, 0, sizeof(hparms));

    if(parse_cmdline(&iface, argc, argv))
    {
        return(1);
    }

    giface = &iface;

    /* open syslog */
    sprintf(syslog_tag, "clinkd-%s", iface.iface_name);
    if(! iface.no_syslog)
        openlog(syslog_tag, LOG_CONS, LOG_DAEMON);

    /* fork into the background */
    if(! iface.no_fork)
    {
        close(0);
        close(1);
        close(2);
        switch(fork())
        {
            case -1:
                log_msg(&iface, L_ERR, "can't fork, exiting\n");
                break;
            case 0:
                /* child process started */
                break;
            default:
                /* parent process must exit */
                _exit(0);
        }
    }

#if CLNK_CTRL_PATH==CTRLPATH_FLEXBUS
    log_msg(&iface, L_INFO, "Starting %s flexbus clinkd v%s\n",
            ECFG_CHIP_MAVERICKS ? "EN2510" : "EN2210", MOCA_DRV_VERSION);
#else
    log_msg(&iface, L_INFO, "Starting %s MII clinkd v%s\n",
            ECFG_CHIP_MAVERICKS ? "EN2510" : "EN2210", MOCA_DRV_VERSION);

    if(nic_init(&iface, iface.iface_name, &hparms))
        return(1);

    if(bus_init(&iface))
        return(1);
#endif
    if(iface.bus_test)
    {
        sig_setup();
        return(run_bus_test(&iface));
    }

    /* initialize UNIX socket interface */
    memset((void *)&iocfg, 0, sizeof(iocfg));
    strcpy(iocfg.ifname, iface.iface_name);
    iocfg.clnk_ctx = (void *)&iface;

    ioctx = netio_init(&iocfg);
    if(! ioctx)
        return(1);

    log_msg(&iface, L_INFO, "netio_init returns %lx\n", ioctx);
    iface.ioctx = ioctx;

    /* set up signal handling */
    sig_setup();

    /* initialize ClnkEth */
    memset((void *)&fw, 0, sizeof(fw));

    /* initialize elf FW */
    elf_fw = 0 ;
    if( elf_setup( iface.firmware_path, &elf_fw ))
    {
        log_msg(&iface, L_ERR, "elf_setup() failed\n");
        return(1);
    }
    log_msg(&iface, L_ERR, "Returned from elf_setup(), path=%s\n", iface.firmware_path );
    fw.pFirmware = NULL;

    /* initialize microcode FW */
    uc_fw  = 0 ;
    if( uc_setup( iface.microcode_path, &uc_fw ))
    {
        log_msg(&iface, L_ERR, "uc_setup() failed\n");
        return(1);
    }
    log_msg(&iface, L_ERR, "Returned from uc_setup(), path=%s\n", iface.microcode_path );
    fw.pMicrocode = NULL;

    /*
     * &iface is passed in as the hostos context and as the baseaddr,
     * because CLNK_REG_READ/WRITE can only see baseaddr and HostOS_*
     * can only see pHostOSContext.
     */
    ret = Clnk_ETH_Initialize(&pvContext, &iface, iface.iface_dev,
                              (SYS_UINTPTR)&iface, &fw, &hparms);
    log_msg(&iface, L_DBG, "Clnk_ETH_Initialize returned %d\n", ret);

    if(ret)
    {
        log_msg(&iface, L_ERR, "ETH initialization failed %d\n", ret); // FIXME
        return(1);
    }

    pContext = (Context_t *)pvContext ;  // set context for local use
    iface.pvContext = pvContext;

    /* set MII link status down */
    mii_set_link(&iface, LINK_FORCE_DOWN);

    ret = 1;        /* exit code */

    if(! iface.no_embedded)
    {
        /*
         * set firmware pointer (not set above since we didn't want to boot
         * the SoC at initialization time)
         */
        if( elf_fw ) {
            // arm firmware load
            pContext->fw.pFirmware    = (void *)elf_fw;
            // arm firmware load
            if (Clnk_ETH_Control(pvContext, CLNK_ETH_CTRL_SET_FIRMWARE, 0,
                                (SYS_UINTPTR)elf_fw, 0)  != SYS_SUCCESS)
            {
                log_msg(&iface, L_ERR, "*** Unable to SET_FIRMWARE ***\n");
                return(1);
            }
        }

        if( uc_fw ) {
            // arm microcode load
            pContext->fw.pMicrocode   = (void *)uc_fw;
        }

        /*
         * set clink MAC address
         */
        // MAC address is programmed here, but also in shared memory 
        if(Clnk_ETH_Control(pvContext, CLNK_ETH_CTRL_SET_MAC_ADDR, iface.mac_addr_hi,
                            iface.mac_addr_lo, 0) != SYS_SUCCESS)
        {
            log_msg(&iface, L_ERR, "*** Unable to SET_MAC_ADDR ***\n");
            return(1);
        }
    } else {
        log_msg(&iface, L_INFO, "no_embedded mode set - skipping SoC boot\n");
    }


    /* reread the conf file and reboot the SoC each time we receive SIGHUP */
    while(1)
    {
        struct clnk_io io;
        struct clnk_conf cfg;

        /* read clink.conf, pass in user specified conf_path */
        if(conf_read(&cfg, iface.conf_path))
        {
            log_msg(&iface, L_ERR, "Unable to read configuration file\n");
            break;
        }

        /* read feic.conf, pass in user specified feicconf_path */
        if(feicconf_read(&cfg, iface.feicconf_path))
        {
            log_msg(&iface, L_ERR, "Unable to read feic configuration file\n");
            break;
        }

#if defined(L3_DONGLE_HIRF)
        /*ECB3M-L3 has Diplexer on board, here init the Diplexer with parameter readed from clink.conf*/
        struct ifreq request;
        memset(&request,0,sizeof(request));
        strcpy(request.ifr_name,"/dev/CandD");
        request.ifr_data = (caddr_t) ( 0x2 | (cfg.soc_opt.Diplexer ? 1 : 0));
        if (ioctl(iface.fd, SIOCHDRCMD, &request) < 0)
        {
            log_msg(&iface, L_ERR,"Unable to set Diplexer mode\n");
            break;
        }
#endif
        if(iface.test_mode & TEST_MODE_SEL)
        {
            cfg.soc_opt.phyMBitMask = iface.test_mode & 0xff;
            switch(cfg.soc_opt.phyMBitMask)
            {
                case TEST_MODE_XMIT:
                    log_msg(&iface, L_INFO, "Now entering CONSTANT TRANSMIT test mode\n");
                    break;
                case TEST_MODE_CW:
                    log_msg(&iface, L_INFO, "Now entering CONSTANT WAVE test mode\n");
                    break;
                default:
                    log_msg(&iface, L_INFO, "Now entering test mode %d\n",
                            cfg.soc_opt.phyMBitMask);
            }
        }

        /* set clink options from conf file */
        io.in = (void *)&cfg.soc_opt;
        io.in_len = sizeof(cfg.soc_opt);
        io.out_len = 0;

        /* initialize taboo info */
        if (((cfg.soc_opt.tabooMask << 24) | cfg.soc_opt.tabooOffset) == 0)
        {
            cfg.soc_opt.tabooMask = 0x00ee0000;
            cfg.soc_opt.tabooOffset = 0x03; 
        }

        if(Clnk_ETH_Control(pvContext, CLNK_ETH_CTRL_DO_CLNK_CTL, CLNK_CTL_SET_SOC_OPT,
                    (SYS_UINTPTR)&io, 0) != CLNK_ETH_RET_CODE_SUCCESS)
        {
            log_msg(&iface, L_ERR, "CLNK_CTL_SET_SOC_OPT failed\n");
            break;
        }

#if FEATURE_FEIC_PWR_CAL
        /* Set feicProfileId from clink.conf file into pContext so clnkrst_store_feic_profile() can use it later */
        io.in = (void *)&cfg.soc_opt.feicProfileId;
        io.in_len = sizeof(cfg.soc_opt.feicProfileId);
        io.out_len = 0;

        if(Clnk_ETH_Control(pvContext, CLNK_ETH_CTRL_DO_CLNK_CTL,  
                            CLNK_CTL_SET_FEIC_PID, (SYS_UINTPTR)&io, 0) != CLNK_ETH_RET_CODE_SUCCESS)
        {
            log_msg(&iface, L_ERR, "Problem setting feicProfileId\n");
            break;
        }

        /* Set all feicProfiles[0..MAX_FEIC_PROFILES-1] from feic.conf file into pContext so clnkrst_store_feic_profile() can use it later */
        int i;
        for (i=0; i < MAX_FEIC_PROFILES; i++)
        {
            io.in = (void *)&cfg.feicProfiles.profiles[i];
            io.in_len = sizeof(cfg.feicProfiles.profiles[i]);
            io.out_len = 0;
            
            if(Clnk_ETH_Control(pvContext, CLNK_ETH_CTRL_DO_CLNK_CTL,  
                   CLNK_CTL_SET_FEIC_PROFILE, (SYS_UINTPTR)&io, 0) != CLNK_ETH_RET_CODE_SUCCESS)
            {
                log_msg(&iface, L_ERR, "Problem setting feicProfiles.profile[%d] configuration\n", i);
                break;
            }
        }
#endif /* FEATURE_FEIC_PWR_CAL */

        /* initialize LOF from config file */
        my_lof = cfg.soc_opt.lof;

        /* reset and boot the SoC */
        if(mii_do_reset(&iface))
        {
            log_msg(&iface, L_ERR, "CLNK_ETH_CTRL_RESET failed\n");
            break;
        }
#if defined(L3_DONGLE_HIRF)
        //----------------------------------------------------------------------
        // Bring up the LED
        //----------------------------------------------------------------------
        write(iface.fd,"C0PC0=~&",8);
#endif

        /*
         * enter the clinkd mainloop
         *
         * this function only returns in the case of:
         *  - UNIX signal (return code 0)
         *  - debug modes (return code 0)
         *  - catastrophic error (return code 1)
         */
        if(mii_mainloop(&iface))
        {
            log_msg(&iface, L_INFO, "Unrecoverable error - exiting\n");
            break;
        }

        if(got_sighup)
        {
            log_msg(&iface, L_INFO, "Got SIGHUP - reloading configuration\n");
            got_sighup = 0;
            continue;
        }

        /* successful exit */
        ret = 0;
        if(got_sigterm)
            log_msg(&iface, L_INFO, "Got termination signal - exiting\n");
        else
            return(0);      /* debug exit - don't reset SoC */
        break;
    }

    /*
     * Put ccpu in low power mode when the daemon exits, close network device, 
     * turn off CLNK interrupts
     */
    Clnk_ETH_Terminate(iface.pvContext);
    {
        struct clnk_io io;

        io.in = SYS_NULL;
        io.in_len = 0;
        io.out = SYS_NULL;
        io.out_len = 0;

/*
	Get from entropic open issue 8630
	There is an ¡° if ¡± statement terminated with a semi-colon, ' ; ', in mii_main.c. , line 1956.
	if (send_drv_ioctl(iface.iface_dev, SIOCCLINKDRV, CLNK_CTL_STOP_DEVICE,
	&io) != SYS_SUCCESS);
	This causes the function to always executes the next line,
	return(-1);
	Returning with an error (-1) in all cases regardless of the ¡° if ¡± statement result.
	Work around: delete the ' ; ' (semi-colon) following the ¡° if ¡± statement.

*/
        if (send_drv_ioctl(iface.iface_dev, SIOCCLINKDRV, CLNK_CTL_STOP_DEVICE, &io) != SYS_SUCCESS)
		{
            return(-1);
		}
    }

    if(! iface.no_syslog)
        closelog();

    return(ret);
}
