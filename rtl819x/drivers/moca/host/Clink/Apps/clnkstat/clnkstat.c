/*******************************************************************************
*
* clnkstat.c
*
********************************************************************************
*
* Description:          Clink statistics helper application.
*
* Public Methods:
*
*                       main()
*
* Private Methods:
*
*                       DisplayStats()
*                       DisplayNetData()
*                       DisplayPhyData()
*                       DisplayBridgeTable()
*                       DisplayPrivacy()
*                       DisplayRFICStats()
*                       DisplayRxErrData()
*   
* Compilation and Linker Notes:
*
*
* Version History:      Version history is at the bottom of this file
*
********************************************************************************
*                        Entropic Communications, Inc.
*                         Copyright (c) 2001 - 2002
*                          All rights reserved.
********************************************************************************/
/***********************************************************************************
* This file is licensed under the terms of your license agreement(s) with          *
* Entropic covering this file. Redistribution, except as permitted by the terms of *
* your license agreement(s) with Entropic, is strictly prohibited.                 *
***********************************************************************************/

/*******************************************************************************
*                            # I n c l u d e s                                 *
********************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <net/if.h>
#include <errno.h>
#include <time.h>
#include <stddef.h>
#if FEATURE_IQM
#include <sys/time.h>
#endif

#define  PRIV_ENABLE_STATE_NAMES

#include "ClnkDefs.h"
#include "ClnkCtl.h"

/*******************************************************************************
*                             # D e f i n e s                                  *
********************************************************************************/

// General Macros
#define MAX(x,y)  ((x > y) ? x : y)

/*******************************************************************************
*                             D a t a   T y p e s                              *
********************************************************************************/

/* None */

/*******************************************************************************
*                             C o n s t a n t s                                *
********************************************************************************/

/* None */

/*******************************************************************************
*                             G l o b a l   D a t a                            *
********************************************************************************/

clnk_ctx_t *clnk_ctx;

SYS_BOOLEAN g_allInfo       = SYS_FALSE;
SYS_BOOLEAN g_bridgeTable   = SYS_FALSE;
SYS_BOOLEAN g_ephyStats     = SYS_FALSE;
SYS_BOOLEAN g_gcdInfo       = SYS_FALSE;
SYS_BOOLEAN g_privacy       = SYS_FALSE;
SYS_BOOLEAN g_detailedStats = SYS_FALSE;
SYS_BOOLEAN g_resetStats    = SYS_FALSE;
SYS_BOOLEAN g_rficStats     = SYS_FALSE;
SYS_BOOLEAN g_rxErrData     = SYS_FALSE;
SYS_BOOLEAN g_aggrStats     = SYS_FALSE;
SYS_BOOLEAN g_qosStats      = SYS_FALSE;
SYS_BOOLEAN g_feicStats     = SYS_FALSE;
#if FEATURE_IQM
SYS_BOOLEAN g_iqmData       = SYS_FALSE;
SYS_BOOLEAN g_iqmDataRx     = SYS_FALSE;
SYS_BOOLEAN g_iqmDataTx     = SYS_FALSE;
SYS_BOOLEAN g_iqmDataRxTest = SYS_FALSE;
SYS_BOOLEAN g_iqmDataTxTest = SYS_FALSE;
#endif
SYS_UINT32  g_nodeID        = 0xffffffff;

/*******************************************************************************
*                       M e t h o d   P r o t o t y p e s                      *
********************************************************************************/

static void DisplayStats(void);
static void DisplayNetData(SYS_UINT32 nodeID);
static void DisplayPhyData(SYS_UINT32 nodeID, SYS_UINT32 channelType);
static void DisplayAggrStats(void);
static void DisplayBridgeTable(void);
static void DisplayEPhyStats(void);
static void DisplayRFICStats(void);
static void DisplayRxErrData(void);
static void DisplayPrivacy(void);
static void DisplayQosStats(void);
static void DisplayFeicStats(void);
#if FEATURE_IQM
static void DisplayIqmData(void);
static void DisplayIqmDataRx(void);
static void DisplayIqmDataTx(void);
static void DisplayIqmDataRxTest(void);
static void DisplayIqmDataTxTest(void);
#endif
static void ResetStats(void);

/*******************************************************************************
*                      M e t h o d   D e f i n i t i o n s                     *
********************************************************************************/


/*
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@                                                                              @
@                        P u b l i c  M e t h o d s                            @
@                                                                              @
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
*/

static int list_interfaces(void)
{
    ClnkDef_ZipList_t zl[16];
    int n_dev, i;

    n_dev = clnk_list_devices(zl, 16);
    if(n_dev < 0)
    {
        printf("Error listing clink devices\n");
        exit(1);
    }
    if(n_dev == 0)
    {
        printf("No clink devices found.\n");
        return(0);
    }
    for(i = 0; i < n_dev; i++)
        printf("%s\n", zl[i].ifname);
    return(0);
}

/*******************************************************************************
*
* Public method:        main()
*
********************************************************************************
*
* Description:
*       Main function.
*
* Inputs:
*       int     Number of command-line argments
*       char**  Command-line arguments
*
* Outputs:
*       0 for success or -1 for failure
*
* Notes:
*       None
*
*
********************************************************************************/

int
main(int argc, char** argv)
{
    int            c;
    SYS_UINT32     i;
    ClnkDef_MyNodeInfo_t  myNodeInfo;
    char *iface = NULL;
    int err;

    // Parse command line
    for ( ; ; )
    {
        int optionIndex = 0;
        static const struct option longOptions[] =
        {
            { "allinfo",       2, 0, 'a' },
            { "bridgeinfo",    0, 0, 'b' },
            { "detailedstats", 0, 0, 'd' },
            { "ephy",          0, 0, 'e' },
            { "gcdinfo",       2, 0, 'g' },
            { "help",          0, 0, 'h' },
            { "p2pinfo",       2, 0, 'a' },
            { "resetstats",    0, 0, 'r' },
            { "privacy",       0, 0, 'p' },
            { "rficstats",     0, 0, 1   },
            { "rxerrdata",     0, 0, 2   },
            { "aggr",          0, 0, 3   },
            { "qos",           0, 0, 4   },
            { "feic",          0, 0, 5   },
#if FEATURE_IQM
            { "iq",            0, 0, 11  },
            { "iqrx",          0, 0, 12  },
            { "iqtx",          0, 0, 13  },
            { "iqrxtest",      0, 0, 14  },
            { "iqtxtest",      0, 0, 15  },
#endif
            { 0,               0, 0, 0   },
        };
        c = getopt_long(argc, argv, "a::bdeg::hrpi:L?", longOptions, &optionIndex);
        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 'a': 
                g_allInfo = SYS_TRUE;
                if (optarg)
                {
                    g_nodeID = atoi(optarg);
                }
                break;

            case 'b': 
                g_bridgeTable = SYS_TRUE;
                break;

            case 'e': 
                g_ephyStats = SYS_TRUE;
                break;

            case 'g': 
                g_gcdInfo = SYS_TRUE;
                if (optarg)
                {
                    g_nodeID = atoi(optarg);
                }
                break;

            case 'd': 
                g_detailedStats = SYS_TRUE;
                break;
                
            case 'p': 
                g_privacy = SYS_TRUE;
                break;

            case 'r': 
                g_resetStats = SYS_TRUE;
                break;
                
            case 'i':
                iface = optarg;
                break;
                
            case 'L':
                list_interfaces();
                exit(0);
                break;

            case 1:
                g_rficStats = SYS_TRUE;
                break;
                
            case 2:
                g_rxErrData = SYS_TRUE;
                break;
                
            case 3:
                g_aggrStats = SYS_TRUE;
                break;
                
            case 4:
                g_qosStats = SYS_TRUE;
                break;

            case 5:
                g_feicStats = SYS_TRUE;
                break;
#if FEATURE_IQM
            case 11:
                g_iqmData = SYS_TRUE;
                break;

            case 12:
                g_iqmDataRx = SYS_TRUE;
                break;

            case 13:
                g_iqmDataTx = SYS_TRUE;
                break;

            case 14:
                g_iqmDataRxTest = SYS_TRUE;
                break;

            case 15:
                g_iqmDataTxTest = SYS_TRUE;
                break;
#endif

            case 'h': 
            case '?':
            case 0:
            default:
                printf("\nUsage: %s [OPTION]...\n\n"
                       "Options:\n"
                       "-i <iface>            "
                       "Select which interface to use\n"
                       "-L                    "
                       "List all clink interfaces\n"
                       "-a, --allinfo=NODEID  "
                       "Displays point to point channel info\n"
                       "    --p2pinfo=NODEID\n"
                       "    --aggr            "
                       "Displays aggregation stats (accepts -r)\n"
                       "-b, --bridgeinfo      "
                       "Displays bridge table info\n"
                       "-d, --detailedstats   "
                       "Displays more detailed stats\n"
                       "-e, --ephy            "
                       "Displays Ethernet PHY stats (accepts -r)\n"
                       "    --feic            "
                       "Displays FEIC Power Calibration status\n"
                       "-g, --gcdinfo=NODEID  "
                       "Displays GCD channel info\n"
                       "-p, --privacy         "
                       "Displays Privacy info (accepts -d and -r)\n"
                       "    --qos             "
                       "Displays qos and L2ME protocol stats\n"
                       "-r, --resetstats      "
                       "Clears all stats to zero\n"
                       "    --rficstats       "
                       "Displays RFIC stats\n"
                       "    --rxerrdata       "
                       "Displays RX data for last error\n"
#if FEATURE_IQM
                       "    --iq              "
                       "Displays IQ Maintenance basic info\n"
                       "    --iqrx            "
                       "Displays IQ Maintenance data for RX IQ\n"
                       "    --iqtx            "
                       "Displays IQ Maintenance data for TX IQ\n"
                       "    --iqrxtest        "
                       "Displays IQ Maintenance data for RX IQ Test Plan\n"
                       "    --iqtxtest        "
                       "Displays IQ Maintenance data for TX IQ Test Plan\n"
#endif
                       "-?, --help            "
                       "Displays this help page\n"
                       "\n",
                       argv[0]);
                exit (0);
        }
    }

    // Initialize connection to driver
    err = clnk_init(iface, &clnk_ctx);
    if(err == RET_WHICHDEV)
    {
        printf("\nMultiple clink devices exist.\n");
        printf("Use clnkstat -L to list them, and clnkstat -i to pick one.\n");
        clnk_destroy(clnk_ctx);
        exit(1);
    }
    if (err)
    {
        printf("\nError finding C.link device\n");
        clnk_destroy(clnk_ctx);
        exit(1);
    }

    if (g_resetStats)
    {
        ResetStats();
    }

    if (g_allInfo || g_gcdInfo)
    {
        if(clnk_get_my_node_info(clnk_ctx, &myNodeInfo, 0))
        {
            printf("\nError getting SOC stats\n");
            clnk_destroy(clnk_ctx);
            exit(-1);
        }

        DisplayNetData(myNodeInfo.NodeId);
        printf("\n");
        for (i = 0; i < 32; i++)
        {
            if ((myNodeInfo.NetworkType == CLNK_DEF_NETWORK_TYPE_ACCESS) &&
                (!myNodeInfo.IsCyclemaster) &&
                (i != myNodeInfo.CMNodeId) &&
                ((g_nodeID == 0xffffffff) || (g_nodeID == i)))
            {
                printf("==================================================================\n");
                DisplayNetData(i);
            }
            else if ((i != myNodeInfo.NodeId) &&
                     (myNodeInfo.NetworkNodeBitMask & (1 << i)) &&
                     ((g_nodeID == 0xffffffff) || (g_nodeID == i)))
            {
                printf("==================================================================\n");
                DisplayNetData(i);
                printf("\n");
                DisplayPhyData(i, g_gcdInfo);
            }
        }
    }
    else if (g_aggrStats)
    {
        DisplayAggrStats();
    }
    else if (g_bridgeTable)
    {
        DisplayBridgeTable();
        printf("\n");
    }
    else if (g_ephyStats)
    {
        DisplayEPhyStats();
    }
    else if (g_rficStats)
    {
        DisplayRFICStats();
    }
    else if (g_rxErrData)
    {
        DisplayRxErrData();
    }
    else if (g_privacy)
    {
        DisplayPrivacy();
    }
    else if (g_qosStats)
    {
        DisplayQosStats();
    }
    else if (g_feicStats)
    {
        DisplayFeicStats();
    }
#if FEATURE_IQM
    else if (g_iqmData)
    {
        DisplayIqmData();
    }
    else if (g_iqmDataRx)
    {
        DisplayIqmDataRx();
    }
    else if (g_iqmDataTx)
    {
        DisplayIqmDataTx();
    }
    else if (g_iqmDataRxTest)
    {
        DisplayIqmDataRxTest();
    }
    else if (g_iqmDataTxTest)
    {
        DisplayIqmDataTxTest();
    }
#endif
    else
    {
        DisplayStats();
    }

    clnk_destroy(clnk_ctx);
    exit(0);
}

/*
$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
$                                                                              $
$                         P r i v a t e  M e t h o d s                         $
$                                                                              $
$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
*/

/*******************************************************************************
*
* Private method:       static DisplayStats()
*
********************************************************************************
*
* Description:
*       Displays statistics.
*
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/
#ifndef PKG_VERSION
  #define PKG_VERSION "<Undefined>"
#endif // PKG_VERSION

static void
DisplayStats(void)
{
    ClnkDef_EthStats_t    ethStats;
    FILE*                 fp;
    char                  line[200];
    char*                 pPackageVersion = "Unknown";
    ClnkDef_MyNodeInfo_t  myNodeInfo;
    SYS_UINT32            i, tmp;
    SYS_BOOLEAN           isFirst;
    ClnkDef_ZipInfo_t zi;

    if(clnk_get_eth_stats(clnk_ctx, &ethStats, SYS_FALSE))
    {
        printf("\nError getting driver stats\n");
        exit(-1);
    }
    if(clnk_get_my_node_info(clnk_ctx, &myNodeInfo, SYS_FALSE))
    {
        printf("\nError getting SOC stats\n");
        exit(-1);
    }

    // Display version numbers
    if ((fp = fopen("/etc/boa/public_html/helpconsole.html", "r")) ||
        (fp = fopen("/home/httpd/helpconsole.html", "r")))
    {
        for ( ; ; )
        {
            fgets(line, 200, fp);
            if (strstr(line, "Software Revision:"))
            {
                strtok(line, ":");
                pPackageVersion = strtok(NULL, " :<");
                break;
            }
        }
    }
    else if ((fp = fopen("/home/httpd/version.js", "r")))
    {
        // Currently of form "fware_ver='1.51';\n"
        int n = fscanf(fp, "%[-_a-zA-Z0-9]=\'%[a-zA-Z0-9._-]\';", line, line);
        if (n == 2)
        {
            pPackageVersion = line;
        }
    }
    else 
    {
        pPackageVersion = PKG_VERSION;
    }

    if(clnk_get_zip_info(clnk_ctx, &zi))
    {
        printf("\nError getting Zip info\n");
        exit(-1);
    }
    // Attach product name to version numbers 
    char * productName;
    if(zi.zip_major == 2 && zi.zip_minor ==0)
    {
        productName = "EN2210";
    }
    else if(zi.zip_major == 2 && zi.zip_minor == 5)
    { 
        productName = "EN2510";
    }
    else if (zi.zip_major == 1 && zi.zip_minor == 2)
    {
        productName = "EN2010";
    }        
    printf("Package Version:     %s.%s\n", productName, pPackageVersion);
    printf("Driver Version:      %s.%d.%d.%d.%d\n",
            productName,
           (myNodeInfo.SwRevNum >> 24),
           (myNodeInfo.SwRevNum >> 16) & 0xff,
           (myNodeInfo.SwRevNum >>  8) & 0xff,
           (myNodeInfo.SwRevNum      ) & 0xff);
    if (g_detailedStats)
    {
        printf("Chip option:         " ECFG_CHIP_STRING    "\n");
        printf("Board option:        " ECFG_BOARD_STRING   "\n");
        printf("Network option:      " ECFG_NETWORK_STRING "\n");
        printf("Nodetype option:     " ECFG_NODE_STRING    "\n");
        printf("OS option:           " ECFG_OS_STRING      "\n");
        printf("Flavor option:       " ECFG_FLAVOR_STRING  "\n");
    }

    /*
     * EmbSwRevNum is either a timestamp or a version number
     * If it is a timestamp, it will be between 2006/01/01 and
     * 2011/01/01
     */
    tmp = myNodeInfo.EmbSwRevNum;
    if((tmp > 1136102400) && (tmp < 1293868800))
    {
        printf("SOC Build time:      %s\n", ctime((const time_t *)&tmp));
    } else {
        printf("SOC Version:         %s.%d.%d.%d.%d\n",
                productName,
               (tmp >> 24),
               (tmp >> 16) & 0xff,
               (tmp >>  8) & 0xff,
               (tmp      ) & 0xff);
    }
    printf("MyMoCAVersion:       %d.%d\n", 
          (GET_MY_MOCA(myNodeInfo.MocaField) >> 4) & 0x0f,
          (GET_MY_MOCA(myNodeInfo.MocaField)     ) & 0x0f);
    printf("Network MoCAVersion: %d.%d\n", 
          (GET_NET_MOCA(myNodeInfo.MocaField) >> 4) & 0x0f,
          (GET_NET_MOCA(myNodeInfo.MocaField)     ) & 0x0f);
    printf("\n");

    // Display driver stats
    printf("Driver Stats:\n");
    printf("Uptime: %dh %dm %ds  ", ethStats.upTime/60/60,
           (ethStats.upTime/60)%60, ethStats.upTime%60);
    printf("Link Uptime: %dh %dm %ds  ", ethStats.linkUpTime/60/60,
           (ethStats.linkUpTime/60)%60, ethStats.linkUpTime%60);
    printf("Reset Count: %d\n", ethStats.socResetCount);
    printf("Network State: 0x%x  ", myNodeInfo.CurrNetworkState);
    printf("Reset History: 0x%x\n", ethStats.socResetHistory);

    if(zi.datapath != DATAPATH_MII)
    {
        /*
         * Ethernet stats not available in MII mode
         * NOTE: consider querying the Linux ethernet driver
         */

        printf("TX Packets:      %10u  ", ethStats.txPackets);
        printf("RX Packets:      %10u\n", ethStats.rxPackets);
        printf("TX Bytes:        %10u  ", ethStats.txBytes);
        printf("RX Bytes:        %10u\n", ethStats.rxBytes);
        printf("TX Packets Good: %10u  ", ethStats.txPacketsGood);
        printf("RX Packets Good: %10u\n", ethStats.rxPacketsGood);
        printf("TX Packet Errors:  %8u  ", ethStats.txPacketErrs);
        printf("RX Packet Errors:  %8u\n", ethStats.rxPacketErrs);
        printf("TX Packet Dropped: %8u  ", ethStats.txDroppedErrs);
        printf("RX Packet Dropped: %8u\n", ethStats.rxDroppedErrs);
        printf("TX Mcast Packets:%10u  ", ethStats.txMulticastPackets);
        printf("RX Mcast Packets:%10u\n", ethStats.rxMulticastPackets);
        printf("TX Mcast Bytes:%12u  ", ethStats.txMulticastBytes);
        printf("RX Mcast Bytes:%12u\n", ethStats.rxMulticastBytes);
        printf("TX CRC32 Errs:     %8u  ", ethStats.txCrc32Errs);
        printf("RX CRC32 Errs:     %8u\n", ethStats.rxCrc32Errs);
        printf("TX Frame Hdr Errs: %8u  ", ethStats.txFrameHeaderErrs);
        printf("RX Frame Hdr Errs: %8u\n", ethStats.rxFrameHeaderErrs);
        printf("TX Fifo Full Errs: %8u  ", ethStats.txFifoFullErrs);
        printf("RX Fifo Full Errs: %8u\n", ethStats.rxFifoFullErrs);
        printf("                             ");
        printf("RX Length Errs:    %8u\n", ethStats.rxLengthErrs);
    }

    printf("\n");

    // Display SOC stats
    printf("SOC Stats:\n");
    printf("LINK %s  %s  NodeID: %d\n",
           myNodeInfo.LinkStatus ? "UP" : "DOWN",
           myNodeInfo.IsCyclemaster ? "CM" : "SLAVE",
           myNodeInfo.NodeId);
    if (myNodeInfo.IsCyclemaster)
    {
        printf("BestCMNodeID: %d  BackupCMNodeID: %d  ",
               myNodeInfo.BestCMNodeId,
               myNodeInfo.BackupCMNodeId);
    }
    else
    {
        printf("CMNodeID: %d  BackupCMNodeID: %d  ",
               myNodeInfo.CMNodeId,
               myNodeInfo.BackupCMNodeId);
    }
    printf("OtherNodeIDs: ");
    isFirst = SYS_TRUE;
    for (i = 0; i < 32; i++)
    {
        if ((i != myNodeInfo.NodeId) &&
            (myNodeInfo.NetworkNodeBitMask & (1 << i)))
        {
            if (!isFirst)
            {
                printf(",");
            }
            printf("%d", i);
            isFirst = SYS_FALSE;
        }
    }
    printf("\n");
    printf("TX Maps:         %10u  ", myNodeInfo.Stats.NumOfMapTx);
    printf("RX Maps:         %10u\n", myNodeInfo.Stats.NumOfMapRx);
    printf("TX Map Errors:   %10u  ", myNodeInfo.Stats.NumOfMapTxErr);
    printf("RX Map Errors:   %10u\n", myNodeInfo.Stats.NumOfMapRxErr);
    printf("                             ");
    printf("RX Map Dropped:  %10u\n", myNodeInfo.Stats.NumOfMapRxDropped);
    printf("TX Rsrv:         %10u  ", myNodeInfo.Stats.NumOfRsrvTx);
    printf("RX Rsrv:         %10u\n", myNodeInfo.Stats.NumOfRsrvRx);
    printf("TX Rsrv Errors:  %10u  ", myNodeInfo.Stats.NumOfRsrvTxErr);
    printf("RX Rsrv Errors:  %10u\n", myNodeInfo.Stats.NumOfRsrvRxErr);
    printf("                             ");
    printf("RX Rsrv Dropped: %10u\n", myNodeInfo.Stats.NumOfRsrvRxDropped);
    printf("TX LC:           %10u  ", myNodeInfo.Stats.NumOfLCTx);
    printf("RX LC:           %10u\n", myNodeInfo.Stats.NumOfLCRx);
    printf("TX LC Errors:    %10u  ", myNodeInfo.Stats.NumOfLCTxErr);
    printf("RX LC Errors:    %10u\n", myNodeInfo.Stats.NumOfLCRxErr);
    printf("                             ");
    printf("RX LC Dropped:   %10u\n", myNodeInfo.Stats.NumOfLCRxDropped);
    printf("TX Adm:          %10u  ", myNodeInfo.Stats.NumOfAdmTx);
    printf("RX Adm:          %10u\n", myNodeInfo.Stats.NumOfAdmRx);
    printf("TX Adm Errors:   %10u  ", myNodeInfo.Stats.NumOfAdmTxErr);
    printf("RX Adm Errors:   %10u\n", myNodeInfo.Stats.NumOfAdmRxErr);
    printf("                             ");
    printf("RX Adm Dropped:  %10u\n", myNodeInfo.Stats.NumOfAdmRxDropped);
    printf("TX Probes:       %10u  ", myNodeInfo.Stats.NumOfProbeTx);
    printf("RX Probes:       %10u\n", myNodeInfo.Stats.NumOfProbeRx);
    printf("TX Probe Errors: %10u  ", myNodeInfo.Stats.NumOfProbeTxErr);
    printf("RX Probe Errors: %10u\n", myNodeInfo.Stats.NumOfProbeRxErr);
    printf("                             ");
    printf("RX Probe Dropped:%10u\n", myNodeInfo.Stats.NumOfProbeRxDropped);
    printf("TX Async:        %10u  ", myNodeInfo.Stats.NumOfAsyncTx);
    printf("RX Async:        %10u\n", myNodeInfo.Stats.NumOfAsyncRx);
    printf("TX Async Errors: %10u  ", myNodeInfo.Stats.NumOfAsyncTxErr);
    printf("RX Async Errors: %10u\n", myNodeInfo.Stats.NumOfAsyncRxErr);
    printf("                             ");
    printf("RX Async Dropped:  %8u\n", myNodeInfo.Stats.NumOfAsyncRxDropped);
    printf("Ctl Descr Failed:  %8u  ", myNodeInfo.Stats.NumOfCtlDescrFailed);
    printf("Upd Descr Failed:  %8u\n", myNodeInfo.Stats.NumOfUpdateDescrFailed);
    printf("Stat Descr Failed: %8u  ", myNodeInfo.Stats.NumOfStatDescrFailed);
    printf("Buf Alloc Failed:  %8u\n", myNodeInfo.Stats.NumOfBufferAllocFailed);
    printf("RS bytes corr:     %8u\n", myNodeInfo.Stats.NumOfRSCorrectedBytes);
    printf("Events:          %10u  ", myNodeInfo.Stats.Events);
    printf("Interrupts:      %10u\n", myNodeInfo.Stats.Interrupts);
    printf("\n");
    if (g_detailedStats)
    {
        int txI;
        int rxI;
        int txQ;
        int rxQ;
        int txD;
        int rxD;

        printf("Other Info:\n");
        printf("RF Frequency:    %d Mhz\n",
               myNodeInfo.RFChanFreq/1000000);
        printf("Network Type:  %10s\n",
               (myNodeInfo.NetworkType == CLNK_DEF_NETWORK_TYPE_FULLY_MESHED)?
               "Fully Meshed" : "Access");
        printf("Node Bit Mask:         0x%02x\n",
               myNodeInfo.NetworkNodeBitMask);
        printf("TX Channel Bit Mask:   0x%02x  ",
               myNodeInfo.TxChannelBitMask);
        printf("RX Channel Bit Mask:   0x%02x\n",
               myNodeInfo.RxChannelBitMask);

        txI = ((myNodeInfo.TxIQImbalance >> 18) & 0x3ff);
        if (txI > 511) txI -= 1024;
        rxI = ((myNodeInfo.RxIQImbalance >> 16) & 0x3ff);
        if (rxI > 511) rxI -= 1024;
        txQ = ((myNodeInfo.TxIQImbalance >> 8) & 0x3ff);
        if (txQ > 511) txQ -= 1024;
        rxQ = (myNodeInfo.RxIQImbalance & 0x3ff);
        if (rxQ > 511) rxQ -= 1024;
        txD = (myNodeInfo.TxIQImbalance & 0xff);
        if (txD > 127) txD -= 256;
        rxD = ((myNodeInfo.RxIQImbalance2 >> 16) & 0xff);
        if (rxD > 127) rxD -= 256;

        printf("TX IQ Imbalance (I):  %5d  ", txI);
        printf("RX IQ Imbalance (I):  %5d\n", rxI);
        printf("TX IQ Imbalance (Q):  %5d  ", txQ);
        printf("RX IQ Imbalance (Q):  %5d\n", rxQ);
        printf("TX IQ Imbalance (D):  %5d  ", txD);
        printf("RX IQ Imbalance (D):  %5d\n", rxD);
        printf("Internal Warnings:  %d\n", myNodeInfo.Stats.InternalWarnings);
        printf("Internal Errors:    %d\n", myNodeInfo.Stats.InternalErrors);
    }
}

/*******************************************************************************
*
* Private method:       static DisplayEPhyStats()
*
********************************************************************************
*
* Description:
*       Displays Ethernet PHY statistics.
*
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       Only applicable for MII datapath mode.
*
*
********************************************************************************/
static void
DisplayEPhyStats(void)
{
    ephy_stats_t ephy_stats = {0};
    ClnkDef_ZipInfo_t zi;

    if(clnk_get_zip_info(clnk_ctx, &zi))
    {
        printf("\nError getting Zip info\n");
        exit(-1);
    }

    if(zi.datapath == DATAPATH_MII)
    {
        if (clnk_get_ephy_stats(clnk_ctx, &ephy_stats, SYS_FALSE))
        {
            printf("\nError getting EPHY stats\n");
            exit(-1);
        }

        printf("Ethernet PHY Statistics:\n\n");
        printf("MII Speed:            %10u  ", ephy_stats.mii_speed);    
        printf("MII Pause Pri Level:  %10d\n", ephy_stats.mii_pause_pri_level);
        printf("RX Pkts Good:         %10u  ", ephy_stats.rx_good);
        printf("TX Pkts Good:         %10u\n", ephy_stats.tx_good);
        printf("RX Pkts Bad:          %10u  ", ephy_stats.rx_bad);
        printf("TX Pkts Bad:          %10u\n", ephy_stats.tx_bad);
        printf("RX Pkts Dropped:      %10u  ", ephy_stats.rx_dropped);
        printf("TX Pkts Dropped:      %10u\n", ephy_stats.tx_dropped);
    }
    else
    {
        printf("Ethernet PHY stats are available in xMII datapath mode only\n");
    }
}


/*******************************************************************************
*
* Private method:       static DisplayNetData()
*
********************************************************************************
*
* Description:
*       Displays net data.
*
* Inputs:
*       SYS_UINT32  Node ID to display
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/

static void
DisplayNetData(SYS_UINT32 nodeID)
{
    ClnkDef_NetNodeInfo_t myNetworkNodeInfo;

    if(clnk_get_network_node_info(clnk_ctx, &myNetworkNodeInfo, nodeID))
    {
        printf("\nError getting SOC stats\n");
        exit(-1);
    }

    printf("Info for node %d:\n", nodeID);
    printf("IsValid:        %16d  ", myNetworkNodeInfo.IsValid);
    printf("MAC Address:   %02x:%02x:%02x:%02x:%02x:%02x\n",
           (myNetworkNodeInfo.GUID64High >> 24),
           (myNetworkNodeInfo.GUID64High >> 16) & 0xff,
           (myNetworkNodeInfo.GUID64High >> 8) & 0xff,
           (myNetworkNodeInfo.GUID64High >> 0) & 0xff,
           (myNetworkNodeInfo.GUID64Low >> 24),
           (myNetworkNodeInfo.GUID64Low >> 16) & 0xff);
    printf("Node State:     %16d  ", myNetworkNodeInfo.NodeState);
    printf("NodeProtocolSupport:    %08x\n",
           myNetworkNodeInfo.NodeProtocolSupport);
    printf("                                  ");
    printf("RxPkts:         %16d\n", myNetworkNodeInfo.RxPkts);
    printf("                                  ");
    printf("RxPktErr:       %16d\n", myNetworkNodeInfo.RxPktErr);
    printf("TxBitRate:      %16d  ", myNetworkNodeInfo.TxBitRate);
    printf("RxBitRate:      %16d\n", myNetworkNodeInfo.RxBitRate);
    printf("GCDTxBitRate:   %16d  ", myNetworkNodeInfo.GCDTxBitRate);
    printf("GCDRxBitRate:   %16d\n", myNetworkNodeInfo.GCDRxBitRate);
}

/*******************************************************************************
*
* Private method:       static DisplayPhyData()
*
********************************************************************************
*
* Description:
*       Displays phy data.
*
* Inputs:
*       SYS_UINT32  Node ID to display
*       SYS_UINT32  Channel type (0 for P2P, 1 for GCD)
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/

static void
DisplayPhyData(SYS_UINT32 nodeID, SYS_UINT32 channelType)
{
    ClnkDef_PhyData_t rxPhyDataInfo;
    ClnkDef_PhyData_t txPhyDataInfo;
    SYS_UINT32     i;
    SYS_UINT32     j;

    if(clnk_get_node_phy_data(clnk_ctx, &txPhyDataInfo, (nodeID | (channelType << 16))))
    {
        printf("\nError getting SOC stats\n");
        exit(-1);
    }
    if(clnk_get_node_phy_data(clnk_ctx, &rxPhyDataInfo, (nodeID | 0x100 | (channelType << 16))))
    {
        printf("\nError getting SOC stats\n");
        exit(-1);
    }

    printf("TX %s to node %d:", channelType ? "GCD" : "P2P", nodeID);
    printf("                 ");
    printf("RX %s from node %d:\n", channelType ? "GCD" : "P2P", nodeID);
    for (i = 0; i < 8; i += 1)
    {
        for (j = 0; j < 4; j += 1)
        {
            printf("%01x", (txPhyDataInfo.ChRespData[i*4+j] >> 0) & 0xf);
            printf("%01x", (txPhyDataInfo.ChRespData[i*4+j] >> 4) & 0xf);
            printf("%01x", (txPhyDataInfo.ChRespData[i*4+j] >> 8) & 0xf);
            printf("%01x", (txPhyDataInfo.ChRespData[i*4+j] >> 12) & 0xf);
            printf("%01x", (txPhyDataInfo.ChRespData[i*4+j] >> 16) & 0xf);
            printf("%01x", (txPhyDataInfo.ChRespData[i*4+j] >> 20) & 0xf);
            printf("%01x", (txPhyDataInfo.ChRespData[i*4+j] >> 24) & 0xf);
            printf("%01x", (txPhyDataInfo.ChRespData[i*4+j] >> 28) & 0xf);
        }
        printf("  ");
        for (j = 0; j < 4; j += 1)
        {
            printf("%01x", (rxPhyDataInfo.ChRespData[i*4+j] >> 0) & 0xf);
            printf("%01x", (rxPhyDataInfo.ChRespData[i*4+j] >> 4) & 0xf);
            printf("%01x", (rxPhyDataInfo.ChRespData[i*4+j] >> 8) & 0xf);
            printf("%01x", (rxPhyDataInfo.ChRespData[i*4+j] >> 12) & 0xf);
            printf("%01x", (rxPhyDataInfo.ChRespData[i*4+j] >> 16) & 0xf);
            printf("%01x", (rxPhyDataInfo.ChRespData[i*4+j] >> 20) & 0xf);
            printf("%01x", (rxPhyDataInfo.ChRespData[i*4+j] >> 24) & 0xf);
            printf("%01x", (rxPhyDataInfo.ChRespData[i*4+j] >> 28) & 0xf);
        }
        printf("\n");
    }
    printf("Preamble Type:  %16d  ", txPhyDataInfo.PreambleType);
    printf("Preamble Type:  %16d\n", rxPhyDataInfo.PreambleType);
    printf("CP Len:         %16d  ", txPhyDataInfo.CPLen);
    printf("CP Len:         %16d\n", rxPhyDataInfo.CPLen);
    printf("TxPowerAdjust:  %16d  ", txPhyDataInfo.TxPwrAdj);
    printf("RSCorrBytesWin256:    %10d\n", rxPhyDataInfo.RSCorrBytes);
    printf("                                  ");
    printf("RSBytesWin256:  %16d\n", rxPhyDataInfo.RSBytes);
    printf("                                  ");
    printf("RSCorrCapacityWin256: %10d\n", rxPhyDataInfo.RSCorrCapacity);
    printf("                                  ");
    printf("RSTotCorrBytes: %16d\n", rxPhyDataInfo.RSTotalCorrBytes);
    printf("                                  ");
    printf("DigiGainMin:    %16d\n", rxPhyDataInfo.DigiGainMin);
    printf("                                  ");
    printf("DigiGainMax:    %16d\n", rxPhyDataInfo.DigiGainMax);
    printf("                                  ");
    printf("DigiGainMean:   %16d\n", rxPhyDataInfo.DigiGainMean);
    printf("                                  ");
    printf("LogGainMin:     %16d\n", rxPhyDataInfo.LogGainMin);
    printf("                                  ");
    printf("LogGainMax:     %16d\n", rxPhyDataInfo.LogGainMax);
    printf("                                  ");
    printf("LogGainMean:    %16d\n", rxPhyDataInfo.LogGainMean);
    printf("                                  ");
    printf("PhaseOffset:    %16d\n", rxPhyDataInfo.PhaseOffset);
    printf("                                  ");
    printf("SystemBias:     %16d\n", rxPhyDataInfo.SystemBias);
    printf("                                  ");
    printf("FtmgThresh:     %16d\n", rxPhyDataInfo.FtmgThresh);
    printf("                                  ");
    printf("FtmgBias:       %16d\n", rxPhyDataInfo.FtmgBias);
    printf("                                  ");
    printf("FtmgThresh2:    %16d\n", rxPhyDataInfo.FtmgThresh2);
}

/*******************************************************************************
*
* Private method:       static DisplayBridgeTable()
*
********************************************************************************
*
* Description:
*       Displays bridge table.
*
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/

#define CAM_IS_MCAST(x) ((x)->hi & 0x01000000)  // bit 0 of 1st byte MAC addr

static void
DisplayBridgeTable(void)
{
    SYS_UINT32 i;
    ClnkDef_ZipInfo_t zi;

    if(clnk_get_zip_info(clnk_ctx, &zi))
    {
        printf("\nError getting Zip info\n");
        exit(-1);
    }

    if(zi.datapath == DATAPATH_MII)
    {
        /* FIXME: use proper data types and macros in this section */

        SYS_UINT32 table1[192 * 2]; // Two SYS_UINT32 per entry

        SYS_UINT32 src_table[CAM_ENTRIES*2];
        SYS_UINT32 dst_table[CAM_ENTRIES*2];
        SYS_UINT32 mcast_table[CAM_ENTRIES*2];
        SYS_UINT32 unknown_table[CAM_ENTRIES*2];
        SYS_UINT32 flow_table[CAM_ENTRIES*2];
        SYS_UINT32 dst, src, mcast, unknown, flow;
        int c = 0;

        dst = src = mcast = unknown = flow = 0;
        memset(table1, 0, BRIDGE_ENTRIES*sizeof(ClnkDef_CamEntry_t));
        memset(dst_table, 0, CAM_ENTRIES*sizeof(ClnkDef_CamEntry_t));
        memset(src_table, 0, CAM_ENTRIES*sizeof(ClnkDef_CamEntry_t));
        memset(mcast_table, 0, CAM_ENTRIES*sizeof(ClnkDef_CamEntry_t));
        memset(unknown_table, 0, CAM_ENTRIES*sizeof(ClnkDef_CamEntry_t));
        memset(flow_table, 0, CAM_ENTRIES*sizeof(ClnkDef_CamEntry_t));

        for(i = 0; i < CAM_ENTRIES; i += DATABUF_CAM_ENTRIES)
        {
            int j;
            ClnkDef_CamEntry_t *ptr;

            if(clnk_get_cam(clnk_ctx, (void *)table1, i))
            {
                printf("\nError getting CAM entries, start=%d\n", i);
                exit(-1);
            }
            for(j = 0, ptr = (ClnkDef_CamEntry_t *)table1; j < DATABUF_CAM_ENTRIES; j++)
            {
                if(CAM_IS_VALID(ptr))
                {
                #if 0
                    printf("%03d: %08x%04x %02x %s \n", i + j, ptr->hi,
                        CAM_GET_LO(ptr), CAM_GET_NODEID(ptr),
                        CAM_IS_DST(ptr) ? "DST" : "SRC");
                #endif // 0
                    if ( CAM_IS_FLOW(ptr) )
                    {
                        memcpy(&flow_table[flow], ptr, sizeof(ClnkDef_CamEntry_t));
                        flow+=2;
                    }
                    else if ( CAM_IS_MCAST(ptr) )
                    {
                        memcpy(&mcast_table[mcast], ptr, sizeof(ClnkDef_CamEntry_t));
                        mcast+=2;
                    }
                    else if ( CAM_IS_DST(ptr) )
                    {
                        if (CAM_GET_NODEID(ptr) != CLNKMAC_BCAST_ADDR)
                        {
                            memcpy(&dst_table[dst], ptr, sizeof(ClnkDef_CamEntry_t));
                            dst+=2;
                        }
                        else
                        {
                            memcpy(&unknown_table[unknown], ptr, sizeof(ClnkDef_CamEntry_t));
                            unknown+=2;
                        }
                    }
                    else
                    {
                        memcpy(&src_table[src], ptr, sizeof(ClnkDef_CamEntry_t));
                        src+=2;
                    }

                    c++;
                }
                ptr++;
            }
        }

        //printf("total: %d valid entries out of %d\n", c, CAM_ENTRIES);

        printf("\nSource Bridge Table:          ");
        printf("Destination Bridge Table:\n");

        for (i = 0; i <  ((src >= dst) ? src : dst); i+=2) 
        {
            if (i < src)
            {
                printf("%02x:%02x:%02x:%02x:%02x:%02x             ",
                       (src_table[i] >> 24),
                       (src_table[i] >> 16) & 0xff,
                       (src_table[i] >> 8) & 0xff,
                       (src_table[i] >> 0) & 0xff,
                       (src_table[i+1] >> 24),
                       (src_table[i+1] >> 16) & 0xff);
            }
            else
                printf("                              ");

            if (i < dst)
            {
                printf("%02x:%02x:%02x:%02x:%02x:%02x (%d)",
                       (dst_table[i] >> 24),
                       (dst_table[i] >> 16) & 0xff,
                       (dst_table[i] >> 8) & 0xff,
                       (dst_table[i] >> 0) & 0xff,
                       (dst_table[i+1] >> 24),
                       (dst_table[i+1] >> 16) & 0xff,
                       (dst_table[i+1] >>  0) & 0xff);
            }
            printf("\n");
        }

        printf("\nUnknown Dest Bridge Table:");
        printf("    Multicast Bridge Table:\n");

        for (i = 0; i <  ((unknown >= mcast) ? unknown : mcast); i+=2) 
        {
            if (i < unknown)
            {
                printf("%02x:%02x:%02x:%02x:%02x:%02x             ",
                       (unknown_table[i] >> 24),
                       (unknown_table[i] >> 16) & 0xff,
                       (unknown_table[i] >> 8) & 0xff,
                       (unknown_table[i] >> 0) & 0xff,
                       (unknown_table[i+1] >> 24),
                       (unknown_table[i+1] >> 16) & 0xff);
            }
            else
                printf("                              ");
            if (i < mcast)
            {
                printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                       (mcast_table[i] >> 24),
                       (mcast_table[i] >> 16) & 0xff,
                       (mcast_table[i] >> 8) & 0xff,
                       (mcast_table[i] >> 0) & 0xff,
                       (mcast_table[i+1] >> 24),
                       (mcast_table[i+1] >> 16) & 0xff);
            }
            printf("\n");
        }

        printf("\nFlow Bridge Table:\n");
        for (i = 0; i < flow; i+=2)
        {
            printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                   (flow_table[i] >> 24),
                   (flow_table[i] >> 16) & 0xff,
                   (flow_table[i] >> 8) & 0xff,
                   (flow_table[i] >> 0) & 0xff,
                   (flow_table[i+1] >> 24),
                   (flow_table[i+1] >> 16) & 0xff);
        }
        return;
    } else {
        /* not using MII mode - read the SW bridge table instead */

        ClnkDef_BridgeTable_t tbl1, tbl2;

        // Display source and destination bridge tables
        if(clnk_get_bridge_table(clnk_ctx, &tbl1, CLNK_BRIDGE_SRC) ||
           clnk_get_bridge_table(clnk_ctx, &tbl2, CLNK_BRIDGE_DST))
        {
            printf("\nError getting src/dst bridge table\n");
            exit(-1);
        }

        printf("\nSource Bridge Table:");
        printf("              ");
        printf("Destination Bridge Table:\n");
        for (i = 0; i < MAX(tbl1.num_entries, tbl2.num_entries); i++)
        {
            if (i < tbl1.num_entries)
            {
                printf("%02x:%02x:%02x:%02x:%02x:%02x",
                       (tbl1.ent[i].macAddrHigh >> 24),
                       (tbl1.ent[i].macAddrHigh >> 16) & 0xff,
                       (tbl1.ent[i].macAddrHigh >> 8) & 0xff,
                       (tbl1.ent[i].macAddrHigh >> 0) & 0xff,
                       (tbl1.ent[i].macAddrLow >> 24),
                       (tbl1.ent[i].macAddrLow >> 16) & 0xff);
            }
            else
            {
                printf("                 ");
            }
            printf("                 ");

            if (i < tbl2.num_entries)
            {
                printf("%02x:%02x:%02x:%02x:%02x:%02x (%d)",
                       (tbl2.ent[i].macAddrHigh >> 24),
                       (tbl2.ent[i].macAddrHigh >> 16) & 0xff,
                       (tbl2.ent[i].macAddrHigh >> 8) & 0xff,
                       (tbl2.ent[i].macAddrHigh >> 0) & 0xff,
                       (tbl2.ent[i].macAddrLow >> 24),
                       (tbl2.ent[i].macAddrLow >> 16) & 0xff,
                       (tbl2.ent[i].macAddrLow & 0xffff));
            }
            printf("\n");
        }

        // Display broadcast and multicast bridge tables
        if(clnk_get_bridge_table(clnk_ctx, &tbl1, CLNK_BRIDGE_BCAST) ||
           clnk_get_bridge_table(clnk_ctx, &tbl2, CLNK_BRIDGE_MCAST))
        {
            printf("\nError getting bcast/mcast bridge table\n");
            exit(-1);
        }

        printf("\nUnknown Dest Bridge Table:");
        printf("        ");
        printf("Multicast Bridge Table:\n");
        for (i = 0; i < MAX(tbl1.num_entries, tbl2.num_entries); i++)
        {
            if (i < tbl1.num_entries)
            {
                printf("%02x:%02x:%02x:%02x:%02x:%02x",
                       (tbl1.ent[i].macAddrHigh >> 24),
                       (tbl1.ent[i].macAddrHigh >> 16) & 0xff,
                       (tbl1.ent[i].macAddrHigh >> 8) & 0xff,
                       (tbl1.ent[i].macAddrHigh >> 0) & 0xff,
                       (tbl1.ent[i].macAddrLow >> 24),
                       (tbl1.ent[i].macAddrLow >> 16) & 0xff);
            }
            else
            {
                printf("                 ");
            }
            printf("                 ");

            if (i < tbl2.num_entries)
            {
                printf("%02x:%02x:%02x:%02x:%02x:%02x",
                       (tbl2.ent[i].macAddrHigh >> 24),
                       (tbl2.ent[i].macAddrHigh >> 16) & 0xff,
                       (tbl2.ent[i].macAddrHigh >> 8) & 0xff,
                       (tbl2.ent[i].macAddrHigh >> 0) & 0xff,
                       (tbl2.ent[i].macAddrLow >> 24),
                       (tbl2.ent[i].macAddrLow >> 16) & 0xff);
            }
            printf("\n");
        }

        // Display flow bridge table
        if(clnk_get_bridge_table(clnk_ctx, &tbl1, CLNK_BRIDGE_FLOW))
        {
            printf("\nError getting flow bridge table\n");
            exit(-1);
        }

        printf("\nFlow Bridge Table:\n");
        for (i = 0; i < tbl1.num_entries; i++)
        {
                printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                       (tbl1.ent[i].macAddrHigh >> 24),
                       (tbl1.ent[i].macAddrHigh >> 16) & 0xff,
                       (tbl1.ent[i].macAddrHigh >> 8) & 0xff,
                       (tbl1.ent[i].macAddrHigh >> 0) & 0xff,
                       (tbl1.ent[i].macAddrLow >> 24),
                       (tbl1.ent[i].macAddrLow >> 16) & 0xff);
        }
    }
}

/*******************************************************************************
*
* Private method:       static DisplayPrivacy()
*
********************************************************************************
*
* Description:
*       Displays Privacy information.
*
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/

static void
DisplayPrivacy(void)
{
    priv_info_t              ptbl;
    priv_info_t              *priv = &ptbl;
    priv_ioctl_stat_t        priv_stats;
    priv_ioctl_stat_t        *pistat = &priv_stats;
    priv_stat_t              *pstat = &priv_stats.pstat;
    ClnkDef_MyNodeInfo_t     my_node;
    char                     ascbuf[64];
    int                      cm = 0;
    priv_node_info_t         nodeinfo[2];

    if(clnk_get_privacy_info(clnk_ctx, priv))
    {
        printf("\nError getting Privacy info\n");
        exit(-1);
    }
    if(clnk_get_privacy_stats(clnk_ctx, pistat, SYS_FALSE))
    {
        printf("\nError getting Privacy stats\n");
        exit(-1);
    }
    if(clnk_get_my_node_info(clnk_ctx, &my_node, SYS_FALSE))
    {
        printf("\nError getting MyNodeInfo\n");
        exit(-1);
    }
    if(clnk_get_privacy_node_info(clnk_ctx, &nodeinfo[0], PRIV_NODE_TEK))
    {
        printf("\nError getting Privacy node info\n");
        exit(-1);
    }
    if(clnk_get_privacy_node_info(clnk_ctx, &nodeinfo[1], PRIV_NODE_PMK))
    {
        printf("\nError getting Privacy node info\n");
        exit(-1);
    }

    if(my_node.IsCyclemaster)
        cm = 1;

    printf("--------------------------------------------------------\n");
    printf("%-16s %10s  ", "NC:", cm ? "YES" : "NO");
    printf("%-16s %10d\n", "NodeID:", my_node.NodeId);
    if(priv->enable >= PRIV_STATUS_EOE)
    {
        strcpy(ascbuf, "*BAD*");
    } else {
        strcpy(ascbuf, priv_status_names[priv->enable]);
    }
    printf("%-16s %10s  ", "Privacy status:", ascbuf);
    if(priv->state >= PRIV_STATE_EOE)
    {
        strcpy(ascbuf, "*BAD*");
    } else {
        strcpy(ascbuf, priv_state_names[priv->state]);
    }
    printf("%-16s %10s\n", "Node state:", ascbuf);
    printf("%-16s %10s  ", "Keyed:",
            priv->keyed ? "YES" : "NO");
#if ! NETWORK_TYPE_ACCESS
    printf("%-16s %10d\n", "Keyset:", priv->key_set);
    printf("%-16s %10s  ", "Active TEK:",
            (nodeinfo[0].state == 1) ? "ODD" : "EVEN");
    printf("%-16s %10s\n", "Active PMK:",
            (nodeinfo[1].state == 1) ? "ODD" : "EVEN");
    sprintf(ascbuf, "%08x%08x", nodeinfo[0].active_key[1], nodeinfo[0].active_key[0]);
    printf("%-8s %18s  ", "TEK(A):", ascbuf);
    sprintf(ascbuf, "%08x%08x", nodeinfo[1].active_key[1], nodeinfo[1].active_key[0]);
    printf("%-8s %18s\n", "PMK(A):", ascbuf);
    sprintf(ascbuf, "%08x%08x", nodeinfo[0].inactive_key[1], nodeinfo[0].inactive_key[0]);
    printf("%-8s %18s  ", "TEK(I):", ascbuf);
    sprintf(ascbuf, "%08x%08x", nodeinfo[1].inactive_key[1], nodeinfo[1].inactive_key[0]);
    printf("%-8s %18s\n", "PMK(I):", ascbuf);
#endif

    printf("--------------------------------------------------------\n");

    if(g_detailedStats)
    {
        if(! pistat->present)
        {
            printf("Privacy statistics were disabled in the SoC build.\n");
        } else {
            printf("%-16s %10d  ", "Clear pkts:", pstat->pkt_clear);
            printf("%-16s %10d\n", "Missing key:", pstat->pkt_missing);
            printf("%-16s %10d  ", "MMK pkts:", pstat->pkt_mmk);
            printf("%-16s %10d\n", "PMKinit pkts:", pstat->pkt_pmkinit);
#if ! NETWORK_TYPE_ACCESS
            printf("%-16s %10d  ", "TEK pkts:", pstat->pkt_tek);
            printf("%-16s %10d\n", "PMK pkts:", pstat->pkt_pmk_teks);
#else /* ! NETWORK_TYPE_ACCESS */
            printf("%-16s %10d  ", "TEKU pkts:", pstat->pkt_tek);
            printf("%-16s %10d\n", "TEKS pkts:", pstat->pkt_pmk_teks);
#endif /* ! NETWORK_TYPE_ACCESS */
        }
        printf("--------------------------------------------------------\n");
    }
}

/*******************************************************************************
*
* Private method:       static DisplayRFICStats()
*
********************************************************************************
*
* Description:
*       Displays RFIC statistics.
*
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/

static void
DisplayRFICStats(void)
{
        ClnkDef_RFICTuningTbl_t  tuningTable;
        ClnkDef_MyNodeInfo_t     myNodeInfo;
        SYS_UINT32               i;
        SYS_UINT32               channel;
    
        // Display RFIC tuning table
        if(clnk_get_rfic_tuning_data(clnk_ctx, &tuningTable))
        {
            printf("\nError getting RFIC tuning table\n");
            exit(-1);
        }
        if(clnk_get_my_node_info(clnk_ctx,&myNodeInfo,SYS_FALSE))
        {
            printf("\nError getting my node info\n");
            exit(-1);
        }
        channel = myNodeInfo.RFChanFreq;
        channel = ((channel/1000000)-800)/25;
        printf("RFIC Tuning Table:\n");
        printf("Channel     Lo_word     Hi_word      Status\n");
        for (i = 0; i < NUM_RF_CHANNELS; i++)
        {
            printf("%7d  0x%08x  0x%08x  0x%08x\n",
                   i, tuningTable.tbl[i].lo_word, tuningTable.tbl[i].hi_word,
                   tuningTable.tbl[i].status);
        }
    
        printf("\nCurrent Channel [%d]MHz:\n",myNodeInfo.RFChanFreq/1000000);
        printf("Status Flag:    0x%x\n", (tuningTable.tbl[channel].status>>24));
        printf("VCO Center ADC: 0x%x\n", (tuningTable.tbl[channel].status>>16)&0xff);
        printf("VCON ADC:       0x%x\n", (tuningTable.tbl[channel].status>>8)&0xff);
        printf("CP Gain Value : 0x%x\n", (tuningTable.tbl[channel].status&0xff));
        printf("D VCO Value :   0x%x\n", (tuningTable.tbl[channel].hi_word>>11)&0x1f);
}

/*******************************************************************************
*
* Private method:       static DisplayRxErrData()
*
********************************************************************************
*
* Description:
*       Displays RFIC statistics.
*
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/

static void
DisplayRxErrData(void)
{
    ClnkDef_RxErrData_t rxErrData;
    SYS_UINT8           profile;
    SYS_UINT8           seqNum;
    SYS_UINT8           type;
    SYS_UINT8           subtype;
    SYS_UINT8           nodeID;

    // Display receive error data
    if(clnk_get_rx_err_data(clnk_ctx, &rxErrData))
    {
        printf("\nError getting RX error data\n");
        exit(-1);
    }

    profile = (rxErrData.Profile >> 24) & 0x3f;
    seqNum  = (rxErrData.Profile >> 30) & 0x3;
    type    = (rxErrData.Profile >> 16) & 0xf;
    subtype = (rxErrData.Profile >> 20) & 0xf;
    nodeID  = (rxErrData.Profile >> 8)  & 0xff;

    printf("RX Error Data:\n");
    printf("Profile:      %s (0x%x)\n", (profile == 1) ? "BEACON MODE" :
                                        (profile == 2) ? "DIVERSITY" :
                                        (profile == 3) ? "CH EST PROBE" :
                                        (profile == 4) ? "IQ PROBE" :
                                        (profile == 5) ? "EP PROBE" :
                                        (profile == 6) ? "MAP" :
                                        (profile == 7) ? "P2P" :
                                        (profile == 8) ? "GCD" :
                                        "UNKNOWN", profile);
    printf("SeqNum:       0x%x\n", seqNum);
    printf("Type:         %s (0x%x)\n", (type == 0x0) ? "MAP" :
                                        (type == 0x1) ? "RSRV" :
                                        (type == 0x2) ? "LINK CONTROL" :
                                        (type == 0x3) ? "ETH" :
                                        (type == 0xa) ? "SILENT" :
                                        (type == 0xb) ? "PROBE" :
                                        (type == 0xc) ? "BEACON" :
                                        "UNKNOWN", type);
    printf("Subtype:      0x%x\n", subtype);
    printf("nodeID:       0x%x\n", nodeID);
    printf("\nRX Parameters:\n");
    printf("RxParams[0]:  0x%x\n", rxErrData.RxParams[0]);
    printf("RxParams[1]:  0x%x\n", rxErrData.RxParams[1]);
    printf("RxParams[2]:  0x%x\n", rxErrData.RxParams[2]);
    printf("RxParams[3]:  0x%x\n", rxErrData.RxParams[3]);
    printf("RxParams[4]:  0x%x\n", rxErrData.RxParams[4]);
    printf("RxParams[5]:  0x%x\n", rxErrData.RxParams[5]);
    printf("RxParams[6]:  0x%x\n", rxErrData.RxParams[6]);
    printf("RxParams[7]:  0x%x\n", rxErrData.RxParams[7]);
    printf("RxParams[8]:  0x%x\n", rxErrData.RxParams[8]);
    printf("RxParams[9]:  0x%x\n", rxErrData.RxParams[9]);
    printf("RxParams[10]:  0x%x\n", rxErrData.RxParams[10]);
    printf("RxParams[11]:  0x%x\n", rxErrData.RxParams[11]);
    printf("RxParams[12]:  0x%x\n", rxErrData.RxParams[12]);
    printf("RxParams[13]:  0x%x\n", rxErrData.RxParams[13]);
    printf("RxParams[14]:  0x%x\n", rxErrData.RxParams[14]);
    printf("RxParams[15]:  0x%x\n", rxErrData.RxParams[15]);
    printf("\nRX Status Descriptor:\n");
    printf("RxStatus[0]:  0x%x\n", rxErrData.RxStatus[0]);
    printf("RxStatus[1]:  0x%x\n", rxErrData.RxStatus[1]);
    printf("RxStatus[2]:  0x%x\n", rxErrData.RxStatus[2]);
    printf("RxStatus[3]:  0x%x\n", rxErrData.RxStatus[3]);
    printf("RxStatus[4]: 0x%x\n", rxErrData.RxStatus[4]);
    printf("RxStatus[5]: 0x%x\n", rxErrData.RxStatus[5]);
    printf("RxStatus[6]: 0x%x\n", rxErrData.RxStatus[6]);
    printf("RxStatus[7]: 0x%x\n", rxErrData.RxStatus[7]);
    printf("RxStatus[8]: 0x%x\n", rxErrData.RxStatus[8]);
    printf("RxStatus[9]: 0x%x\n", rxErrData.RxStatus[9]);
    printf("RxStatus[10]: 0x%x\n", rxErrData.RxStatus[10]);
    printf("RxStatus[11]: 0x%x\n", rxErrData.RxStatus[11]);
    printf("RxStatus[12]: 0x%x\n", rxErrData.RxStatus[12]);
    printf("RxStatus[13]: 0x%x\n", rxErrData.RxStatus[13]);
    printf("RxStatus[14]: 0x%x\n", rxErrData.RxStatus[14]);
}

/*******************************************************************************
*
* Private method:       static DisplayAggrStats()
*
********************************************************************************
*
* Description:
*       Displays Ethernet packet aggregation statistics
*
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/
static void
DisplayAggrStats(void)
{
    aggr_stats_t aggr;
    SYS_UINT32   i;
    SYS_UINT32   clnk_pkts_tx = 0;
    SYS_UINT32   clnk_pkts_rx = 0;
    SYS_UINT32   eth_pkts_tx = 0;
    SYS_UINT32   eth_pkts_rx = 0;
    float        txAggrFactor = 0;
    float        rxAggrFactor = 0;

    if (clnk_get_aggr_stats(clnk_ctx, &aggr, SYS_FALSE))
    {
        printf("\nError getting aggregation stats\n");
        exit(-1);
    }

    for (i = 0; i < aggr.max_pkts_per_aggr; i++)
    {
        clnk_pkts_tx += aggr.tx_pkts_aggr[i];
        clnk_pkts_rx += aggr.rx_pkts_aggr[i];
        eth_pkts_tx += aggr.tx_pkts_aggr[i] * (i + 1);
        eth_pkts_rx += aggr.rx_pkts_aggr[i] * (i + 1);
    }

    if (clnk_pkts_tx)
    {
        txAggrFactor = (float)eth_pkts_tx / clnk_pkts_tx;
    }

    if (clnk_pkts_rx)
    {
        rxAggrFactor = (float)eth_pkts_rx / clnk_pkts_rx;
    }

    printf("Aggregation Statistics:\n");
    printf("\nDriver Stats:\n");    
    printf("TX Pkts:              %10u  ", eth_pkts_tx);
    printf("RX Pkts:              %10u\n", eth_pkts_rx);
    printf("\nSOC Stats:\n");
    printf("TX Pkts (non-aggr):   %10u  ", aggr.tx_pkts_aggr[0]);
    printf("RX Pkts (non-aggr):   %10u\n", aggr.rx_pkts_aggr[0]);
    for (i = 1; i < aggr.max_pkts_per_aggr; i++)
    {
        printf("TX Pkts (%u-pkt aggr): %10u  ", i + 1, aggr.tx_pkts_aggr[i]);
        printf("RX Pkts (%u-pkt aggr): %10u\n", i + 1, aggr.rx_pkts_aggr[i]);
    }
    printf("TX Aggr Factor:       %10.2f  ", txAggrFactor);
    printf("RX Aggr Factor:       %10.2f\n", rxAggrFactor);
}

/*******************************************************************************
*
* Private method:       static DisplayQosStats()
*
********************************************************************************
*
* Description:
*       Displays QoS statistics
*
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/
static void
DisplayQosStats(void)
{
    ClnkDef_MyNodeInfo_t  myNodeInfo;

    if(clnk_get_my_node_info(clnk_ctx, &myNodeInfo, 0))
    {
        printf("\nError getting SOC stats\n");
        exit(-1);
    }

    printf("QOS statistics:\n");

    printf("Received Submits:     %10u  ", myNodeInfo.Stats.qosRcvdSubmits);
    printf("Issued Txns:          %10u\n", myNodeInfo.Stats.qosIssuedTxns);
    printf("Received Submits:     %10u  ", myNodeInfo.Stats.qosRcvdSubmits);
    printf("Issued Txns:          %10u\n", myNodeInfo.Stats.qosIssuedTxns);
    printf("Received Submits:     %10u  ", myNodeInfo.Stats.qosRcvdSubmits);
    printf("Issued Txns:          %10u\n", myNodeInfo.Stats.qosIssuedTxns);
    printf("Received Requests:    %10u  ", myNodeInfo.Stats.qosRcvdRequests);
    printf("Successful Txns:      %10u\n", myNodeInfo.Stats.qosSuccessTxns);
    printf("Received Responses:   %10u  ", myNodeInfo.Stats.qosRcvdResponses);
    printf("Early Ended Txns:     %10u\n", myNodeInfo.Stats.qosEarlyTerminatedTxns);
    printf("Accepted Submits:     %10u  ", myNodeInfo.Stats.qosAcceptedSubmits);
    printf("Entry Cncl Txns:      %10u\n", myNodeInfo.Stats.qosEntryCancelledTxns);
    printf("Dropped Submits:      %10u  ", myNodeInfo.Stats.qosDroppedSubmits);
    printf("Txn Errors:           %10u\n", myNodeInfo.Stats.qosTxnErrors);
    printf("Issued Waves:         %10u  ", myNodeInfo.Stats.qosIssuedWaves);
    printf("Performed Entry Cncl: %10u\n", myNodeInfo.Stats.qosPerformedEntryCancels);
    printf("Skipped Waves:        %10u  ", myNodeInfo.Stats.qosSkippedWaves);
    printf("Received Txn Errors:  %10u\n", myNodeInfo.Stats.qosRcvdTxnErrors);
    printf("Successful Waves:     %10u  ", myNodeInfo.Stats.qosSuccessWaves);
    printf("Total Egress Burst:   %10u\n", myNodeInfo.Stats.qosTotalEgressBurst); 

    printf("Failed Waves:         %10u  ", myNodeInfo.Stats.qosFailedWaves);
    printf("Max Egress Burst:     %10u\n", myNodeInfo.Stats.qosMaxEgressBurst); 
}

/*******************************************************************************
*
* Private method:       static DisplayFeicStats()
*
********************************************************************************
*
* Description:
*       Displays Feic statistics
*
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes: (sample output when profileId > 0 and when profileId <= 0)
*   clnkstat --feic
*   FEIC Profile Info:                  FEIC Cal Final Status:
*   FeicProfileID:    1                 RfFrequency:         1150 MHz
*   CalDisabled:      No                FeicAtt2:            1
*   CalBypassed:      No                FeicAtt1:            1
*   ProfileFound:     Yes               TxCalDecrN:          1
*   ProfileVer:       HRFv00            TxdCGainSel:         13
*   TempClass:        COM               FeicPwrEst:          3.3  dBm
*   TgtLsbBias:       0x00              CalErr:              No
 
*   clnkstat --feic
*   FEIC Profile Info:                  FEIC Cal Final Status:
*   FeicProfileID:    0                 RfFrequency:         800 MHz
*   CalDisabled:      No                FeicAtt2:            1
*   CalBypassed:      Yes               FeicAtt1:            0
*   ProfileFound:     Yes               TxCalDecrN:          1
*   ProfileVer:       N/A               TxdCGainSel:         15
*   TempClass:        N/A               FeicPwrEst:          N/A
*   TgtLsbBias:       N/A               CalErr:              N/A
*
********************************************************************************/
#if FEATURE_FEIC_PWR_CAL
#define FS_FREQ_START   800000000
#define FS_FREQ_STEP    25000000
#define IDX_TO_FREQ(i)  (FS_FREQ_START + (i) * FS_FREQ_STEP)

static void
DisplayFeicStats(void)
{
    feic_final_results_t Results;
    int i, j, lshift;
    int data_array[sizeof(feic_final_results_t)/4];
    unsigned char temp_array[4];
    int src;
        
    printf("FEIC Profile Info:                  FEIC Cal Final Status:\n");
    if (!clnk_get_feic_final_results(clnk_ctx, &Results))
    {   //success, so display final configuration status variables
        //First repack final feic status data (already in host) in a portable fashion,
        //such that we avoid any endianess issues.
        src = (int)(&Results);
        for (i = 0; i < sizeof(feic_final_results_t)/4; i++) 
        {
            memcpy(temp_array, (void *)src, 4); //copy one word at a time
            // we don't want to reverse rfBandStr so we skip over it
            if (i < offsetof(FinalConfigStatus_t, rfBandStr)/4 ||
                i > (offsetof(FinalConfigStatus_t, rfBandStr)+MAX_RF_BAND_STR_SIZE-1)/4)
            {
                for (j = 0; j < 4; j++) 
                {
                    lshift = 24 - 8 * (j - (j/4)*4);
                    data_array[i] &= ~(0xff << lshift); //clear byte location
                    data_array[i] |= ((temp_array[j] & 0xff) << lshift); //store value in location
                }
            }
            else
            {
                data_array[i] = *(SYS_UINT32 *)&temp_array[0];
            }
            src += 4;
        }
        memcpy(&Results, data_array, sizeof(data_array));

        if (!Results.finalConfigStatus.CalDisabled && !Results.finalConfigStatus.CalBypassed)
        {
            printf("FeicProfileID:    %-2d                RfFrequency:         %-d MHz\n",
                   Results.finalConfigStatus.feicProfileId,
                   IDX_TO_FREQ(Results.finalConfigStatus.freqIndex) / 1000000);
        }
        else
        {
            printf("FeicProfileID:    %-2d                RfFrequency:         N/A\n",
                   Results.finalConfigStatus.feicProfileId);
        }
        printf("CalDisabled:      %s               FeicAtt2:%13d\n",
               (Results.finalConfigStatus.CalDisabled ? "Yes" : "No "),
               Results.finalConfigStatus.feicAtt2);
        printf("CalBypassed:      %s               FeicAtt1:%13d\n",
               (Results.finalConfigStatus.CalBypassed ? "Yes" : "No "),
               Results.finalConfigStatus.feicAtt1);
        if (!Results.finalConfigStatus.CalDisabled)
        {
            printf("ProfileFound:     %s               TxCalDecrN:%11d\n",
                   (Results.finalConfigStatus.feicFileFound ? "Yes" : "No "),
                   Results.finalConfigStatus.rficTxCal);
        }
        else
        {
            printf("ProfileFound:     N/A               TxCalDecrN:%11d\n",
                   Results.finalConfigStatus.rficTxCal);
        }
        if (!Results.finalConfigStatus.CalDisabled && !Results.finalConfigStatus.CalBypassed)
        {
            printf("ProfileVer:       %-8s",
                   Results.finalConfigStatus.rfBandStr);
            printf("          TxdCGainSel:         %-d\n",
                   Results.finalConfigStatus.txdCGainSel);
            printf("TempClass:        %s               FeicPwrEst:          %-4.1f dBm\n",
                   (Results.finalConfigStatus.tempClass ? "COM" : "IND"),
                   Results.finalConfigStatus.pwrEstimateT10 / 10.0);
            printf("TgtLsbBias:       %-4d              CalErr:              %s\n",
                   Results.finalConfigStatus.targetLsbBias,
                   Results.finalConfigStatus.calErr ? "Yes" : "No");       
        }
        else
        {
            printf("ProfileVer:       N/A               TxdCGainSel:         %-2d\n",
                   Results.finalConfigStatus.txdCGainSel);
            printf("TempClass:        N/A               FeicPwrEst:          N/A\n");
            printf("TgtLsbBias:       N/A               CalErr:              N/A\n");
        }
    }
    else
        //bad
        printf("Error getting final Configuration status variables\n");
}
#endif

#if FEATURE_IQM

    #define SCANNING_INTERVAL 500000 /* default scanning interval of 500ms */

/*******************************************************************************
*
* Private method:       static DisplayIqmData()
*
********************************************************************************
*
* Description:
*       Displays IQ Maintenance Basic Information.
*
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/
static void
DisplayIqmData(void)
{
    ClnkDef_IqmData_t iqmDebugData;
    int pre_rxI, pre_rxQ, pre_rxD;
    int post_rxI, post_rxQ, post_rxD;
    int pre_txI, pre_txQ, pre_txD;
    int post_txI, post_txQ, post_txD;
    char ascbuf[30];
    struct timeval tv;
    time_t now;

    if(clnk_get_iqm_data(clnk_ctx, &iqmDebugData))
    {
        printf("\nError getting Zip info\n");
        exit(-1);
    }

#ifdef AEI_WECB
    aei_gettimeofday(&tv, NULL);
#else
    gettimeofday(&tv, NULL);
#endif
    now = tv.tv_sec;
    strftime(ascbuf, 30, "%H:%M:%S", gmtime(&now)); // time

    printf("IQ Maintenance Data:\n");
    printf("Time: %s \n", ascbuf);

    printf("IIRR_MAX_TX:              %10d  ", iqmDebugData.iqmIirrMaxTx);
    printf("IIRR_MAX_RX:              %10d\n", iqmDebugData.iqmIirrMaxRx);
    printf("DELTA_IRR_MAX_TX:         %10d  ", iqmDebugData.iqmDeltaIrrMaxTx);
    printf("DELTA_IRR_MAX_RX:         %10d\n", iqmDebugData.iqmDeltaIrrMaxRx);

    printf("RX Pre_IIRR:              %10d  ", iqmDebugData.iqmRxPreIirr);
    printf("RX Post_IIRR:             %10d\n", iqmDebugData.iqmRxPostIirr);
    printf("RX Pre_IIRR1:             %10d  ", iqmDebugData.iqmRxPreIirr1);
    printf("RX Post_IIRR1:            %10d\n", iqmDebugData.iqmRxPostIirr1);

    printf("TX Pre_IIRR:              %10d  ", iqmDebugData.iqmTxPreIirr);
    printf("TX Post_IIRR:             %10d\n", iqmDebugData.iqmTxPostIirr);
    printf("TX Pre_IIRR1:             %10d  ", iqmDebugData.iqmTxPreIirr1);
    printf("TX Post_IIRR1:            %10d\n", iqmDebugData.iqmTxPostIirr1);

    printf("Tone Bin 0:               %10d  ", iqmDebugData.iqmToneBin0);
    printf("Tone Bin 1:               %10d\n", iqmDebugData.iqmToneBin1);
    printf("CP Length:                %10d  ", iqmDebugData.iqmCpLen);
    printf("Symbols per Packet:       %10u\n", iqmDebugData.iqmSymbolsPerPacket);
    printf("Frequency Offset:         %10d  ", iqmDebugData.iqmFreqOffset1);
    printf("Frequency Offset (EVM):   %10d\n", iqmDebugData.iqmFreqOffset2);
    printf("TX Bit Rate:              %10u  ", iqmDebugData.iqmTxBitrate);
    printf("RX Bit Rate:              %10u\n", iqmDebugData.iqmRxBitrate);
    printf("IQM Cycle Counter:        %10u  ", iqmDebugData.iqmCycleCounter);
    printf("IQM Re-IQ Counter:        %10u\n", iqmDebugData.iqmReiqCounter);
    printf("Partner Node ID:          %10u\n", iqmDebugData.iqmPnNodeId);

    pre_rxI = ((iqmDebugData.iqmPreRxIQD >> 18) & 0x3ff);
    if (pre_rxI > 511) pre_rxI -= 1024;
    pre_rxQ = ((iqmDebugData.iqmPreRxIQD >> 8) & 0x3ff);
    if (pre_rxQ > 511) pre_rxQ -= 1024;
    pre_rxD = (iqmDebugData.iqmPreRxIQD & 0xff);
    if (pre_rxD > 127) pre_rxD -= 256;

    post_rxI = ((iqmDebugData.iqmPostRxIQD >> 18) & 0x3ff);
    if (post_rxI > 511) post_rxI -= 1024;
    post_rxQ = ((iqmDebugData.iqmPostRxIQD >> 8) & 0x3ff);
    if (post_rxQ > 511) post_rxQ -= 1024;
    post_rxD = (iqmDebugData.iqmPostRxIQD & 0xff);
    if (post_rxD > 127) post_rxD -= 256;

    pre_txI = ((iqmDebugData.iqmPreTxIQD >> 18) & 0x3ff);
    if (pre_txI > 511) pre_txI -= 1024;
    pre_txQ = ((iqmDebugData.iqmPreTxIQD >> 8) & 0x3ff);
    if (pre_txQ > 511) pre_txQ -= 1024;
    pre_txD = (iqmDebugData.iqmPreTxIQD & 0xff);
    if (pre_txD > 127) pre_txD -= 256;

    post_txI = ((iqmDebugData.iqmPostTxIQD >> 18) & 0x3ff);
    if (post_txI > 511) post_txI -= 1024;
    post_txQ = ((iqmDebugData.iqmPostTxIQD >> 8) & 0x3ff);
    if (post_txQ > 511) post_txQ -= 1024;
    post_txD = (iqmDebugData.iqmPostTxIQD & 0xff);
    if (post_txD > 127) post_txD -= 256;

    printf("PRE RX IQ Imbalance (I):  %10d  ", pre_rxI);
    printf("POST RX IQ Imbalance (I): %10d\n", post_rxI);
    printf("PRE RX IQ Imbalance (Q):  %10d  ", pre_rxQ);
    printf("POST RX IQ Imbalance (Q): %10d\n", post_rxQ);
    printf("PRE RX IQ Imbalance (D):  %10d  ", pre_rxD);
    printf("POST RX IQ Imbalance (D): %10d\n", post_rxD);

    printf("PRE TX IQ Imbalance (I):  %10d  ", pre_txI);
    printf("POST TX IQ Imbalance (I): %10d\n", post_txI);
    printf("PRE TX IQ Imbalance (Q):  %10d  ", pre_txQ);
    printf("POST TX IQ Imbalance (Q): %10d\n", post_txQ);
    printf("PRE TX IQ Imbalance (D):  %10d  ", pre_txD);
    printf("POST TX IQ Imbalance (D): %10d\n", post_txD);
}

/*******************************************************************************
*
* Private method:       static DisplayIqmDataRx()
*
********************************************************************************
*
* Description:
*       Displays IQ Maintenance internal data for RX IQ.
*
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/
static void
DisplayIqmDataRx(void)
{
    ClnkDef_IqmData_t iqmDebugData;
    int pre_rxI, pre_rxQ, pre_rxD;
    int post_rxI, post_rxQ, post_rxD;
    int pre_txI, pre_txQ, pre_txD;
    int post_txI, post_txQ, post_txD;
    char ascbuf[30];
    struct timeval tv;
    time_t now;

    if(clnk_get_iqm_data(clnk_ctx, &iqmDebugData))
    {
        printf("\nError getting Zip info\n");
        exit(-1);
    }

    pre_rxI = ((iqmDebugData.iqmPreRxIQD >> 18) & 0x3ff);
    if (pre_rxI > 511) pre_rxI -= 1024;
    pre_rxQ = ((iqmDebugData.iqmPreRxIQD >> 8) & 0x3ff);
    if (pre_rxQ > 511) pre_rxQ -= 1024;
    pre_rxD = (iqmDebugData.iqmPreRxIQD & 0xff);
    if (pre_rxD > 127) pre_rxD -= 256;

    post_rxI = ((iqmDebugData.iqmPostRxIQD >> 18) & 0x3ff);
    if (post_rxI > 511) post_rxI -= 1024;
    post_rxQ = ((iqmDebugData.iqmPostRxIQD >> 8) & 0x3ff);
    if (post_rxQ > 511) post_rxQ -= 1024;
    post_rxD = (iqmDebugData.iqmPostRxIQD & 0xff);
    if (post_rxD > 127) post_rxD -= 256;

#ifdef AEI_WECB
    aei_gettimeofday(&tv, NULL);
#else
    gettimeofday(&tv, NULL);
#endif
    now = tv.tv_sec;
    strftime(ascbuf, 30, "%H:%M:%S", gmtime(&now)); // time

    printf("IQ Maintenance Data for RX IQ:\n");
    printf("Time: %s \n", ascbuf);
    // RX IQ internal data for debugging
    printf("Total Number of Symbols:  %10u\n", iqmDebugData.iqmRxTotalSymbols);
    printf("-- Pre-IRR Stage --\n");
    printf("TIQratioR.I:              %10d  ", iqmDebugData.iqmRxPreTIQratioR_I);
    printf("TIQratioR1.I:             %10d\n", iqmDebugData.iqmRxPreTIQratioR1_I);
    printf("TIQratioR.Q:              %10d  ", iqmDebugData.iqmRxPreTIQratioR_Q);
    printf("TIQratioR1.Q:             %10d\n", iqmDebugData.iqmRxPreTIQratioR1_Q);
    printf("ratioR.I:                 %10d  ", iqmDebugData.iqmRxPreRatioR_I);
    printf("ratioR1.I:                %10d\n", iqmDebugData.iqmRxPreRatioR1_I);
    printf("ratioR.Q:                 %10d  ", iqmDebugData.iqmRxPreRatioR_Q);
    printf("ratioR1.Q:                %10d\n", iqmDebugData.iqmRxPreRatioR1_Q);
    printf("RX Pre_IIRR:              %10d  ", iqmDebugData.iqmRxPreIirr);
    printf("RX Pre_IIRR1:             %10d\n", iqmDebugData.iqmRxPreIirr1);

    printf("-- Post-IRR Stage --\n");
    printf("TIQratioR.I:              %10d  ", iqmDebugData.iqmRxPostTIQratioR_I);
    printf("TIQratioR1.I:             %10d\n", iqmDebugData.iqmRxPostTIQratioR1_I);
    printf("TIQratioR.Q:              %10d  ", iqmDebugData.iqmRxPostTIQratioR_Q);
    printf("TIQratioR1.Q:             %10d\n", iqmDebugData.iqmRxPostTIQratioR1_Q);
    printf("ratioR.I:                 %10d  ", iqmDebugData.iqmRxPostRatioR_I);
    printf("ratioR1.I:                %10d\n", iqmDebugData.iqmRxPostRatioR1_I);
    printf("ratioR.Q:                 %10d  ", iqmDebugData.iqmRxPostRatioR_Q);
    printf("ratioR1.Q:                %10d\n", iqmDebugData.iqmRxPostRatioR1_Q);
    printf("RX Post_IIRR:             %10d  ", iqmDebugData.iqmRxPostIirr);
    printf("RX Post_IIRR1:            %10d\n", iqmDebugData.iqmRxPostIirr1);

    printf("RX Delta_IIRR:            %10d  ", iqmDebugData.iqmRxDeltaIirr);
    printf("RX Delta_IIRR1:           %10d\n", iqmDebugData.iqmRxDeltaIirr1);
    // end of RX IQ internal data

    printf("PRE RX IQ Imbalance (I):  %10d  ", pre_rxI);
    printf("POST RX IQ Imbalance (I): %10d\n", post_rxI);
    printf("PRE RX IQ Imbalance (Q):  %10d  ", pre_rxQ);
    printf("POST RX IQ Imbalance (Q): %10d\n", post_rxQ);
    printf("PRE RX IQ Imbalance (D):  %10d  ", pre_rxD);
    printf("POST RX IQ Imbalance (D): %10d\n", post_rxD);

    printf("Tone Bin 0:               %10d  ", iqmDebugData.iqmToneBin0);
    printf("Tone Bin 1:               %10d\n", iqmDebugData.iqmToneBin1);
    printf("CP Length:                %10d  ", iqmDebugData.iqmCpLen);
    printf("Symbols per Packet:       %10u\n", iqmDebugData.iqmSymbolsPerPacket);
    printf("Frequency Offset:         %10d\n", iqmDebugData.iqmFreqOffset1);
    printf("TX Bit Rate:              %10u  ", iqmDebugData.iqmTxBitrate);
    printf("RX Bit Rate:              %10u\n", iqmDebugData.iqmRxBitrate);
    printf("IQM Cycle Counter:        %10u  ", iqmDebugData.iqmCycleCounter);
    printf("IQM Re-IQ Counter:        %10u\n", iqmDebugData.iqmReiqCounter);
    printf("Partner Node ID:          %10u\n", iqmDebugData.iqmPnNodeId);
}

/*******************************************************************************
*
* Private method:       static DisplayIqmDataTx()
*
********************************************************************************
*
* Description:
*       Displays IQ Maintenance internal data for TX IQ.
*
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/
static void
DisplayIqmDataTx(void)
{
    ClnkDef_IqmData_t iqmDebugData;
    int pre_rxI, pre_rxQ, pre_rxD;
    int post_rxI, post_rxQ, post_rxD;
    int pre_txI, pre_txQ, pre_txD;
    int post_txI, post_txQ, post_txD;
    char ascbuf[30];
    struct timeval tv;
    time_t now;

    if(clnk_get_iqm_data(clnk_ctx, &iqmDebugData))
    {
        printf("\nError getting Zip info\n");
        exit(-1);
    }

    pre_txI = ((iqmDebugData.iqmPreTxIQD >> 18) & 0x3ff);
    if (pre_txI > 511) pre_txI -= 1024;
    pre_txQ = ((iqmDebugData.iqmPreTxIQD >> 8) & 0x3ff);
    if (pre_txQ > 511) pre_txQ -= 1024;
    pre_txD = (iqmDebugData.iqmPreTxIQD & 0xff);
    if (pre_txD > 127) pre_txD -= 256;

    post_txI = ((iqmDebugData.iqmPostTxIQD >> 18) & 0x3ff);
    if (post_txI > 511) post_txI -= 1024;
    post_txQ = ((iqmDebugData.iqmPostTxIQD >> 8) & 0x3ff);
    if (post_txQ > 511) post_txQ -= 1024;
    post_txD = (iqmDebugData.iqmPostTxIQD & 0xff);
    if (post_txD > 127) post_txD -= 256;

#ifdef AEI_WECB
    aei_gettimeofday(&tv, NULL);
#else
    gettimeofday(&tv, NULL);
#endif
    now = tv.tv_sec;
    strftime(ascbuf, 30, "%H:%M:%S", gmtime(&now)); // time

    printf("IQ Maintenance Data for TX IQ:\n");
    printf("Time: %s \n", ascbuf);
    // TX IQ internal data for debugging
    printf("Total Number of Symbols:  %10u\n", iqmDebugData.iqmTxTotalSymbols);
    printf("-- Pre-IRR Stage --\n");
    printf("TIQratioR.I:              %10d  ", iqmDebugData.iqmTxPreTIQratioR_I);
    printf("TIQratioR1.I:             %10d\n", iqmDebugData.iqmTxPreTIQratioR1_I);
    printf("TIQratioR.Q:              %10d  ", iqmDebugData.iqmTxPreTIQratioR_Q);
    printf("TIQratioR1.Q:             %10d\n", iqmDebugData.iqmTxPreTIQratioR1_Q);
    printf("ratioR.I:                 %10d  ", iqmDebugData.iqmTxPreRatioR_I);
    printf("ratioR1.I:                %10d\n", iqmDebugData.iqmTxPreRatioR1_I);
    printf("ratioR.Q:                 %10d  ", iqmDebugData.iqmTxPreRatioR_Q);
    printf("ratioR1.Q:                %10d\n", iqmDebugData.iqmTxPreRatioR1_Q);
    printf("TX Pre_IIRR:              %10d  ", iqmDebugData.iqmTxPreIirr);
    printf("TX Pre_IIRR1:             %10d\n", iqmDebugData.iqmTxPreIirr1);

    printf("-- Post-IRR Stage --\n");
    printf("TIQratioR.I:              %10d  ", iqmDebugData.iqmTxPostTIQratioR_I);
    printf("TIQratioR1.I:             %10d\n", iqmDebugData.iqmTxPostTIQratioR1_I);
    printf("TIQratioR.Q:              %10d  ", iqmDebugData.iqmTxPostTIQratioR_Q);
    printf("TIQratioR1.Q:             %10d\n", iqmDebugData.iqmTxPostTIQratioR1_Q);
    printf("ratioR.I:                 %10d  ", iqmDebugData.iqmTxPostRatioR_I);
    printf("ratioR1.I:                %10d\n", iqmDebugData.iqmTxPostRatioR1_I);
    printf("ratioR.Q:                 %10d  ", iqmDebugData.iqmTxPostRatioR_Q);
    printf("ratioR1.Q:                %10d\n", iqmDebugData.iqmTxPostRatioR1_Q);
    printf("TX Post_IIRR:             %10d  ", iqmDebugData.iqmTxPostIirr);
    printf("TX Post_IIRR1:            %10d\n", iqmDebugData.iqmTxPostIirr1);

    printf("TX Delta_IIRR:            %10d  ", iqmDebugData.iqmTxDeltaIirr);
    printf("TX Delta_IIRR1:           %10d\n", iqmDebugData.iqmTxDeltaIirr1);
    // end of TX IQ internal data

    printf("PRE TX IQ Imbalance (I):  %10d  ", pre_txI);
    printf("POST TX IQ Imbalance (I): %10d\n", post_txI);
    printf("PRE TX IQ Imbalance (Q):  %10d  ", pre_txQ);
    printf("POST TX IQ Imbalance (Q): %10d\n", post_txQ);
    printf("PRE TX IQ Imbalance (D):  %10d  ", pre_txD);
    printf("POST TX IQ Imbalance (D): %10d\n", post_txD);

    printf("Tone Bin 0:               %10d  ", iqmDebugData.iqmToneBin0);
    printf("Tone Bin 1:               %10d\n", iqmDebugData.iqmToneBin1);
    printf("CP Length:                %10d  ", iqmDebugData.iqmCpLen);
    printf("Symbols per Packet:       %10u\n", iqmDebugData.iqmSymbolsPerPacket);
    printf("Frequency Offset:         %10d\n", iqmDebugData.iqmFreqOffset1);
    printf("TX Bit Rate:              %10u  ", iqmDebugData.iqmTxBitrate);
    printf("RX Bit Rate:              %10u\n", iqmDebugData.iqmRxBitrate);
    printf("IQM Cycle Counter:        %10u  ", iqmDebugData.iqmCycleCounter);
    printf("Partner Node ID:          %10u\n", iqmDebugData.iqmPnNodeId);
}

/*******************************************************************************
*
* Private method:       static DisplayIqmDataRxTest()
*
********************************************************************************
*
* Description:
*       Displays IQ Maintenance internal data for RX IQ under ramp test according
*       to Test Plan
*       It checks for new data update in loop forever until Ctrl-C pressed
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/
static void
DisplayIqmDataRxTest(void)
{
    ClnkDef_IqmData_t iqmDebugData;
    int pre_rxI, pre_rxQ, pre_rxD;
    int post_rxI, post_rxQ, post_rxD;
    int pre_txI, pre_txQ, pre_txD;
    int post_txI, post_txQ, post_txD;
    SYS_UINT32 timeStamp_iqmCycle = 0;
    SYS_UINT32 timeStamp_lmoCycle = 0;
    SYS_UINT32 timeStamp_evmCycle = 0;
    char ascbuf[30];
    struct timeval tv;
    time_t now;


    while (1)
    {
        if(clnk_get_iqm_data(clnk_ctx, &iqmDebugData))
        {
            printf("\nError getting Zip info\n");
            exit(-1);
        }

        if(iqmDebugData.iqmTimeStampIqmCycle != timeStamp_iqmCycle)
        {
            timeStamp_iqmCycle = iqmDebugData.iqmTimeStampIqmCycle;
            pre_rxI = ((iqmDebugData.iqmPreRxIQD >> 18) & 0x3ff);
            if (pre_rxI > 511) pre_rxI -= 1024;
            pre_rxQ = ((iqmDebugData.iqmPreRxIQD >> 8) & 0x3ff);
            if (pre_rxQ > 511) pre_rxQ -= 1024;
            pre_rxD = (iqmDebugData.iqmPreRxIQD & 0xff);
            if (pre_rxD > 127) pre_rxD -= 256;

            post_rxI = ((iqmDebugData.iqmPostRxIQD >> 18) & 0x3ff);
            if (post_rxI > 511) post_rxI -= 1024;
            post_rxQ = ((iqmDebugData.iqmPostRxIQD >> 8) & 0x3ff);
            if (post_rxQ > 511) post_rxQ -= 1024;
            post_rxD = (iqmDebugData.iqmPostRxIQD & 0xff);
            if (post_rxD > 127) post_rxD -= 256;

#ifdef AEI_WECB
            aei_gettimeofday(&tv, NULL);
#else
            gettimeofday(&tv, NULL);
#endif
            now = tv.tv_sec;
            strftime(ascbuf, 30, "%H:%M:%S", gmtime(&now)); // time

            printf("\nIQ Maintenance Data for RX IQ Test Plan:\n");
            printf("Time: %s \n", ascbuf);
            // RX IQ internal data for debugging
            printf("Total Number of Symbols:  %10u\n", iqmDebugData.iqmRxTotalSymbols);
            printf("-- Pre-IRR Stage --\n");
            printf("TIQratioR.I:              %10d  ", iqmDebugData.iqmRxPreTIQratioR_I);
            printf("TIQratioR1.I:             %10d\n", iqmDebugData.iqmRxPreTIQratioR1_I);
            printf("TIQratioR.Q:              %10d  ", iqmDebugData.iqmRxPreTIQratioR_Q);
            printf("TIQratioR1.Q:             %10d\n", iqmDebugData.iqmRxPreTIQratioR1_Q);
            printf("ratioR.I:                 %10d  ", iqmDebugData.iqmRxPreRatioR_I);
            printf("ratioR1.I:                %10d\n", iqmDebugData.iqmRxPreRatioR1_I);
            printf("ratioR.Q:                 %10d  ", iqmDebugData.iqmRxPreRatioR_Q);
            printf("ratioR1.Q:                %10d\n", iqmDebugData.iqmRxPreRatioR1_Q);
            printf("RX Pre_IIRR:              %10d  ", iqmDebugData.iqmRxPreIirr);
            printf("RX Pre_IIRR1:             %10d\n", iqmDebugData.iqmRxPreIirr1);

            printf("-- Post-IRR Stage --\n");
            printf("TIQratioR.I:              %10d  ", iqmDebugData.iqmRxPostTIQratioR_I);
            printf("TIQratioR1.I:             %10d\n", iqmDebugData.iqmRxPostTIQratioR1_I);
            printf("TIQratioR.Q:              %10d  ", iqmDebugData.iqmRxPostTIQratioR_Q);
            printf("TIQratioR1.Q:             %10d\n", iqmDebugData.iqmRxPostTIQratioR1_Q);
            printf("ratioR.I:                 %10d  ", iqmDebugData.iqmRxPostRatioR_I);
            printf("ratioR1.I:                %10d\n", iqmDebugData.iqmRxPostRatioR1_I);
            printf("ratioR.Q:                 %10d  ", iqmDebugData.iqmRxPostRatioR_Q);
            printf("ratioR1.Q:                %10d\n", iqmDebugData.iqmRxPostRatioR1_Q);
            printf("RX Post_IIRR:             %10d  ", iqmDebugData.iqmRxPostIirr);
            printf("RX Post_IIRR1:            %10d\n", iqmDebugData.iqmRxPostIirr1);

            printf("RX Delta_IIRR:            %10d  ", iqmDebugData.iqmRxDeltaIirr);
            printf("RX Delta_IIRR1:           %10d\n", iqmDebugData.iqmRxDeltaIirr1);
            // end of RX IQ internal data

            printf("PRE RX IQ Imbalance (I):  %10d  ", pre_rxI);
            printf("POST RX IQ Imbalance (I): %10d\n", post_rxI);
            printf("PRE RX IQ Imbalance (Q):  %10d  ", pre_rxQ);
            printf("POST RX IQ Imbalance (Q): %10d\n", post_rxQ);
            printf("PRE RX IQ Imbalance (D):  %10d  ", pre_rxD);
            printf("POST RX IQ Imbalance (D): %10d\n", post_rxD);

            printf("Tone Bin 0:               %10d  ", iqmDebugData.iqmToneBin0);
            printf("Tone Bin 1:               %10d\n", iqmDebugData.iqmToneBin1);
            printf("CP Length:                %10d  ", iqmDebugData.iqmCpLen);
            printf("Symbols per Packet:       %10u\n", iqmDebugData.iqmSymbolsPerPacket);
            printf("Frequency Offset:         %10d\n", iqmDebugData.iqmFreqOffset1);
            printf("TX Bit Rate:              %10u  ", iqmDebugData.iqmTxBitrate);
            printf("RX Bit Rate:              %10u\n", iqmDebugData.iqmRxBitrate);
            printf("IQM Cycle Counter:        %10u  ", iqmDebugData.iqmCycleCounter);
            printf("IQM Re-IQ Counter:        %10u\n", iqmDebugData.iqmReiqCounter);
            printf("Partner Node ID:          %10u\n", iqmDebugData.iqmPnNodeId);
            printf("\n");
        }

        if(iqmDebugData.iqmTimeStampLmoSession!= timeStamp_lmoCycle)
        {
            timeStamp_lmoCycle = iqmDebugData.iqmTimeStampLmoSession;
            printf("TX Bit Rate (LMO):        %10u  ", iqmDebugData.iqmTxBitrate);
            printf("RX Bit Rate (LMO):        %10u\n", iqmDebugData.iqmRxBitrate);
        }

        if(iqmDebugData.iqmTimeStampEvmCycle != timeStamp_evmCycle)
        {
            timeStamp_evmCycle = iqmDebugData.iqmTimeStampEvmCycle;
            printf("Frequency Offset (EVM):   %10d\n", iqmDebugData.iqmFreqOffset2);
        }

        usleep(SCANNING_INTERVAL);
    }/* while loop */
}


/*******************************************************************************
*
* Private method:       static DisplayIqmDataTxTest()
*
********************************************************************************
*
* Description:
*       Displays IQ Maintenance internal data for RX IQ under ramp test according
*       to Test Plan
*       It checks for new data update in loop forever until Ctrl-C pressed
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/
static void
DisplayIqmDataTxTest(void)
{
    ClnkDef_IqmData_t iqmDebugData;
    int pre_rxI, pre_rxQ, pre_rxD;
    int post_rxI, post_rxQ, post_rxD;
    int pre_txI, pre_txQ, pre_txD;
    int post_txI, post_txQ, post_txD;
    SYS_UINT32 timeStamp_iqmCycle = 0;
    SYS_UINT32 timeStamp_lmoCycle = 0;
    char ascbuf[30];
    struct timeval tv;
    time_t now;


    while (1)
    {
        if(clnk_get_iqm_data(clnk_ctx, &iqmDebugData))
        {
            printf("\nError getting Zip info\n");
            exit(-1);
        }

        if(iqmDebugData.iqmTimeStampIqmCycle != timeStamp_iqmCycle)
        {
            timeStamp_iqmCycle = iqmDebugData.iqmTimeStampIqmCycle;
            pre_rxI = ((iqmDebugData.iqmPreRxIQD >> 18) & 0x3ff);
            if (pre_rxI > 511) pre_rxI -= 1024;
            pre_rxQ = ((iqmDebugData.iqmPreRxIQD >> 8) & 0x3ff);
            if (pre_rxQ > 511) pre_rxQ -= 1024;
            pre_rxD = (iqmDebugData.iqmPreRxIQD & 0xff);
            if (pre_rxD > 127) pre_rxD -= 256;

            post_rxI = ((iqmDebugData.iqmPostRxIQD >> 18) & 0x3ff);
            if (post_rxI > 511) post_rxI -= 1024;
            post_rxQ = ((iqmDebugData.iqmPostRxIQD >> 8) & 0x3ff);
            if (post_rxQ > 511) post_rxQ -= 1024;
            post_rxD = (iqmDebugData.iqmPostRxIQD & 0xff);
            if (post_rxD > 127) post_rxD -= 256;

            pre_txI = ((iqmDebugData.iqmPreTxIQD >> 18) & 0x3ff);
            if (pre_txI > 511) pre_txI -= 1024;
            pre_txQ = ((iqmDebugData.iqmPreTxIQD >> 8) & 0x3ff);
            if (pre_txQ > 511) pre_txQ -= 1024;
            pre_txD = (iqmDebugData.iqmPreTxIQD & 0xff);
            if (pre_txD > 127) pre_txD -= 256;

            post_txI = ((iqmDebugData.iqmPostTxIQD >> 18) & 0x3ff);
            if (post_txI > 511) post_txI -= 1024;
            post_txQ = ((iqmDebugData.iqmPostTxIQD >> 8) & 0x3ff);
            if (post_txQ > 511) post_txQ -= 1024;
            post_txD = (iqmDebugData.iqmPostTxIQD & 0xff);
            if (post_txD > 127) post_txD -= 256;

#ifdef AEI_WECB
            aei_gettimeofday(&tv, NULL);
#else
            gettimeofday(&tv, NULL);
#endif
            now = tv.tv_sec;
            strftime(ascbuf, 30, "%H:%M:%S", gmtime(&now)); // time

            printf("\nIQ Maintenance Data for TX IQ Test Plan:\n");
            printf("Time: %s \n", ascbuf);
            // TX IQ internal data for debugging
            printf("Total Number of Symbols:  %10u\n", iqmDebugData.iqmTxTotalSymbols);
            printf("-- Pre-IRR Stage --\n");
            printf("TIQratioR.I:              %10d  ", iqmDebugData.iqmTxPreTIQratioR_I);
            printf("TIQratioR1.I:             %10d\n", iqmDebugData.iqmTxPreTIQratioR1_I);
            printf("TIQratioR.Q:              %10d  ", iqmDebugData.iqmTxPreTIQratioR_Q);
            printf("TIQratioR1.Q:             %10d\n", iqmDebugData.iqmTxPreTIQratioR1_Q);
            printf("ratioR.I:                 %10d  ", iqmDebugData.iqmTxPreRatioR_I);
            printf("ratioR1.I:                %10d\n", iqmDebugData.iqmTxPreRatioR1_I);
            printf("ratioR.Q:                 %10d  ", iqmDebugData.iqmTxPreRatioR_Q);
            printf("ratioR1.Q:                %10d\n", iqmDebugData.iqmTxPreRatioR1_Q);
            printf("TX Pre_IIRR:              %10d  ", iqmDebugData.iqmTxPreIirr);
            printf("TX Pre_IIRR1:             %10d\n", iqmDebugData.iqmTxPreIirr1);

            printf("-- Post-IRR Stage --\n");
            printf("TIQratioR.I:              %10d  ", iqmDebugData.iqmTxPostTIQratioR_I);
            printf("TIQratioR1.I:             %10d\n", iqmDebugData.iqmTxPostTIQratioR1_I);
            printf("TIQratioR.Q:              %10d  ", iqmDebugData.iqmTxPostTIQratioR_Q);
            printf("TIQratioR1.Q:             %10d\n", iqmDebugData.iqmTxPostTIQratioR1_Q);
            printf("ratioR.I:                 %10d  ", iqmDebugData.iqmTxPostRatioR_I);
            printf("ratioR1.I:                %10d\n", iqmDebugData.iqmTxPostRatioR1_I);
            printf("ratioR.Q:                 %10d  ", iqmDebugData.iqmTxPostRatioR_Q);
            printf("ratioR1.Q:                %10d\n", iqmDebugData.iqmTxPostRatioR1_Q);
            printf("TX Post_IIRR:             %10d  ", iqmDebugData.iqmTxPostIirr);
            printf("TX Post_IIRR1:            %10d\n", iqmDebugData.iqmTxPostIirr1);

            printf("TX Delta_IIRR:            %10d  ", iqmDebugData.iqmTxDeltaIirr);
            printf("TX Delta_IIRR1:           %10d\n", iqmDebugData.iqmTxDeltaIirr1);
            // end of TX IQ internal data

            printf("RX Pre_IIRR:              %10d  ", iqmDebugData.iqmRxPreIirr);
            printf("RX Post_IIRR:             %10d\n", iqmDebugData.iqmRxPostIirr);
            printf("RX Pre_IIRR1:             %10d  ", iqmDebugData.iqmRxPreIirr1);
            printf("RX Post_IIRR1:            %10d\n", iqmDebugData.iqmRxPostIirr1);

            ///printf("TX Pre_IIRR:              %10d  ", iqmDebugData.iqmTxPreIirr);
            ///printf("TX Post_IIRR:             %10d\n", iqmDebugData.iqmTxPostIirr);
            ///printf("TX Pre_IIRR1:             %10d  ", iqmDebugData.iqmTxPreIirr1);
            ///printf("TX Post_IIRR1:            %10d\n", iqmDebugData.iqmTxPostIirr1);

            printf("PRE RX IQ Imbalance (I):  %10d  ", pre_rxI);
            printf("POST RX IQ Imbalance (I): %10d\n", post_rxI);
            printf("PRE RX IQ Imbalance (Q):  %10d  ", pre_rxQ);
            printf("POST RX IQ Imbalance (Q): %10d\n", post_rxQ);
            printf("PRE RX IQ Imbalance (D):  %10d  ", pre_rxD);
            printf("POST RX IQ Imbalance (D): %10d\n", post_rxD);

            printf("PRE TX IQ Imbalance (I):  %10d  ", pre_txI);
            printf("POST TX IQ Imbalance (I): %10d\n", post_txI);
            printf("PRE TX IQ Imbalance (Q):  %10d  ", pre_txQ);
            printf("POST TX IQ Imbalance (Q): %10d\n", post_txQ);
            printf("PRE TX IQ Imbalance (D):  %10d  ", pre_txD);
            printf("POST TX IQ Imbalance (D): %10d\n", post_txD);

            printf("Loopback Tone Bin 0:      %10d  ", iqmDebugData.iqmLoopbackToneBin0);
            printf("Loopback Tone Bin 1:      %10d\n", iqmDebugData.iqmLoopbackToneBin1);
            printf("Loopback CP Length:       %10d  ", iqmDebugData.iqmLoopbackCpLen);
            printf("Loopback Symbols/Packet:  %10u\n", iqmDebugData.iqmLoopbackSymbolsPerPacket);
            printf("Frequency Offset:         %10d\n", iqmDebugData.iqmFreqOffset1);
            printf("TX Bit Rate:              %10u  ", iqmDebugData.iqmTxBitrate);
            printf("RX Bit Rate:              %10u\n", iqmDebugData.iqmRxBitrate);
            printf("IQM Cycle Counter:        %10u  ", iqmDebugData.iqmCycleCounter);
            printf("Partner Node ID:          %10u\n", iqmDebugData.iqmPnNodeId);
            printf("\n");
        }

        if(iqmDebugData.iqmTimeStampLmoSession!= timeStamp_lmoCycle)
        {
            timeStamp_lmoCycle = iqmDebugData.iqmTimeStampLmoSession;
            printf("TX Bit Rate (LMO):        %10u  ", iqmDebugData.iqmTxBitrate);
            printf("RX Bit Rate (LMO):        %10u\n", iqmDebugData.iqmRxBitrate);
        }

        usleep(SCANNING_INTERVAL);
    }/* while loop */
}

#endif

/*******************************************************************************
*
* Private method:       static ResetStats()
*
********************************************************************************
*
* Description:
*       Resets Ethernet, node info, privacy and aggregation statistics
*
* Inputs:
*       None
*
* Outputs:
*       None
*
* Notes:
*       None
*
*
********************************************************************************/
static void
ResetStats(void)
{
    ClnkDef_MyNodeInfo_t myNodeInfo;
    ClnkDef_EthStats_t   ethStats;
    priv_ioctl_stat_t    priv_stats;
    aggr_stats_t aggr;
    ephy_stats_t ephy_stats;
    
    if (clnk_get_aggr_stats(clnk_ctx, &aggr, SYS_TRUE))
    {
        printf("\nError resetting aggregation stats\n");
        exit(-1);
    }
    if (clnk_get_ephy_stats(clnk_ctx, &ephy_stats, SYS_TRUE))
    {
        printf("\nError resetting Ethernet PHY stats\n");
        exit(-1);
    }
    if (clnk_get_eth_stats(clnk_ctx, &ethStats, SYS_TRUE))
    {
        printf("\nError resetting driver stats\n");
        exit(-1);
    }
    if(clnk_get_my_node_info(clnk_ctx, &myNodeInfo, SYS_TRUE))
    {
        printf("\nError resetting SOC stats\n");
        exit(-1);
    }
    if(clnk_get_privacy_stats(clnk_ctx, &priv_stats, SYS_TRUE))
    {
        printf("\nError resetting Privacy stats\n");
        exit(-1);
    }
}

/* End of File: clnkstat.c */

