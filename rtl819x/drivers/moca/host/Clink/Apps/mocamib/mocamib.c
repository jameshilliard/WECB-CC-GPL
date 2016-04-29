/*******************************************************************************
 * Copyright (c) 2001-2010 Entropic Communications, Inc. All rights reserved.
 ******************************************************************************/

/*******************************************************************************
 * This file is licensed under the terms of your license agreement(s) with
 * Entropic covering this file. Redistribution, except as permitted by the
 * terms of your license agreement(s) with Entropic, is strictly prohibited.
 ******************************************************************************/

/***************************************************************************//**
 * \file    mocamib.c
 *
 * \brief   MoCA MIB App
 *
 * The MoCA MIB App (MMA)is a utility that exercises the MoCA MIB objects.
 * It Gets and Sets the supported MIB objects using command line arguments.
 * This provides customers with a MoCA MIB program which they can use as a
 * reference when developing their own MoCA MIB objects.  The MMA is table
 * driven.  There is a separate Get and Set function for each object, as needed,
 * depending on the Read/Write or Read-only capabilities.
 *
 * \note    This file is subject to the \ref EntropicLicenseAgreement "Entropic"
 * and the \ref GPLLicenseAgreement "GPL" licenses.
 ******************************************************************************/


/*******************************************************************************
 *      Includes
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>     // exit()
#include <ctype.h>      // tolower()
#include <unistd.h>     // usleep()
#include <getopt.h>     // getopt()
#include <sys/time.h>   // timeval

#include "ClnkDefs.h"
#include "ClnkCtl.h"

/*******************************************************************************
 *      Constants
 ******************************************************************************/

/*! MoCA MIB App Version String. */
#define MMA_VERSION_STR                 "1.0"

/*! Max length of the Object Name. */
#define MAX_OBJECT_NAME_LEN             40      // "NodePacketsAggrCapability"=25 chars+NULL

/*! Max length of the Object Units string. */
#define MAX_UNITS_LEN                   20      // "packet aggregation"=18 chars+NULLCHAR

/*! Max length of the Object Value String. */
#define MAX_OBJECT_VALUE_STRING_LEN     80      // SoftwareVersion=80 chars+NULLCHAR

/*! Max length of a line in the FILENAME file. */
/*! Format is ObjectName " " ObjectValueString "\n" */
#define MAX_LINE_LEN                    (MAX_OBJECT_NAME_LEN+1+MAX_OBJECT_VALUE_STRING_LEN+1)

/*! Number of entries in the Object Info Table. */
#define NUM_OBJECT_INFO_TABLE_ENTRIES   (sizeof(ObjectInfoTable) / sizeof(ObjectInfo_t))

/*! ifIndex value. */
#define IF_INDEX                        1

/*! Max number of interfaces allowed. */
#define MAX_INTERFACES                  16

/*!
 * \brief   Configuration File Name
 *
 * This defines the file name of the configuration file for this App.
 * This configuration file contains lines consisting of
 *      ObjectName " " ObjectValueString "\n".
 * Only objects that don't have corresponding clnk_get_* type APIs are in this config file.
 * These objects must have defaults i.e. ObjectInfoTable[].DefaultValueStr[].
 */
#define FILENAME                        "/etc/mocamib.conf"

/*!
 * \brief   Backup Configuration File Name
 *
 * This defines the file name of the backup configuration file for this App.
 * This file is used as a temporary file.
 * In order to modify a value for an object name in FILENAME, we copy the lines from FILENAME
 * to FILENAMEBACKUP until we find the line that has the object name whose value we want to
 * modify then modify that value in the line, then write the modified line to FILENAMEBACKUP
 * then finish copying the remaining lines from FILENAME to FILENAMEBACKUP until EOF.
 * Upon completion of the copy, we close both files, delete FILENAME then rename
 * FILENAMEBACKUP to FILENAME.
 */
#define FILENAMEBACKUP                  "/etc/mocamib.backup"

/*!
 * \brief   Proc Tags File Name
 *
 * This defines the file name of the proc tags persistent file for this App.
 * This file is created in one of the /host/SnapRomFS/bridge*.sh script files.
 * And upon bootup, the proc tags persistent file is copied to FILENAME file.
 * When a change is made to the Configuration File for this App, the Configuration File is
 * copied to FILENAMEPROCTAGS file to make it persistent.
 */
#define FILENAMEPROCTAGS                "/proc/tags/MMAF"

/*!
 * \brief   moca and mocaif strings
 *
 * These strings allow users to type objectname or mocaobjectname or mocaifobjectname
 * when selecting the name of the object.
 */
#define MOCA_STR                        "moca"
#define MOCA_IF_STR                     "mocaif"
#define MOCA_STR_LEN                    (sizeof(MOCA_STR) - 1)
#define MOCA_IF_STR_LEN                 (sizeof(MOCA_IF_STR) - 1)

/*!
 * \brief   Indexing Arguments Required
 *
 * This defines the various indexing command line options required for a given Object.
 * This is used to define the Args field of the struct ObjectInfo_t of the ObjectInfoTable.
 */
#define NO_ARGS 0                   // no special args required
#define F_ARGS  (1 << 0)            // FlowIndex required arg
#define N_ARGS  (1 << 1)            // NodeIndex required arg
#define T_ARGS  (1 << 2)            // MeshTxNodeIndex required arg
#define R_ARGS  (1 << 3)            // MeshRxNodeIndex required arg
#define TR_ARGS (T_ARGS | R_ARGS)   // MeshTxNodeIndex and MeshRxNodeIndex required args

/*!
 * \brief   MyNetworkNodeInfo.NodeProtocolSupport masks and values
 *
 * This defines the NodeProtocolSupport masks.
 * After masking, comparisons with these values can be made.
 */
#define QAM256_SUPPORT_FLAG            0x00000010 // bit 4 of NodeProtocolSupport
#define PREFERRED_NC_SUPPORT_FLAG      0x00000040 // bit 6 of NodeProtocolSupport
#define COMMITTEE_SUPPORT_FLAG_MASK    0x00000180 // bit 8:7 MASK of following
#define    AGGR6_SUPPORT_FLAG_VAL           0x000 //   committee - IsMoca1p1Node ? 6 : 0
#define    AGGR10_SUPPORT_FLAG_VAL          0x100 //   committee - 10 pkt Aggr
#define    PROTEM11_SUPPORT_FLAG_VAL        0x080 //   committee - 1.1 Pro Tem
#define    UNUSED_SUPPORT_FLAG_VAL          0x180 //   committee - Unused
#define MOCA_VERSION_SUPPORT_FLAG      0xFF000000 // bits 31:24 - MoCA Version (2 nibbles)

/*!
 * \brief   MoCA Version and Network Version values
 *
 * This defines the MoCA Version and Network Version values.
 */
#define MOCA_1DOT0                             10
#define MOCA_1DOT1                             11
#define MOCA_1DOT1_PRO_TEM                     12

/*******************************************************************************
 *      Macros
 ******************************************************************************/


/*******************************************************************************
 *      Types
 ******************************************************************************/

/*!
 * \brief   Timer expiration type
 *
 * This defines the structure containing the logical timer expiration in secs and microsecs.
 */
typedef struct
{
    struct timeval expire;
} mocamib_tmr_instance_t;

/*!
 * \brief   Object Index enum
 *
 * This defines the index of each Object in the ObjectInfoTable.
 * Must align each entry in ObjectIndex_t enum with each entry in the ObjectInfoTable.
 */
typedef enum
{
    IF_ENABLE,                  // 00
    IF_PRIVACY_ENABLE,
    IF_PASSWORD,
    IF_PREFERRED_NC,
    IF_PHY_THRESHOLD,
    IF_PHY_THRESHOLD_ENABLE,
    IF_STATUS,
    IF_LINK_UP_TIME,
    IF_SOFTWARE_VERSION,
    IF_MOCA_VERSION,
    IF_NETWORK_VERSION,         // 10
    IF_MAC_ADDRESS,
    IF_NODE_ID,
    IF_NAME,
    IF_NUM_NODES,
    IF_NC,
    IF_BACKUP_NC,
    IF_RF_CHANNEL,
    IF_LOF,
    IF_TABOO_CHANNEL_MASK,
    IF_NODE_TABOO_CHANNEL_MASK, // 20
    IF_CAPABILITY_MASK,
    IF_QAM256_CAPABLE,
    IF_PACKETS_AGGR_CAPABILITY,
    IF_TX_GCD_RATE,
    IF_TX_PACKETS,
    IF_TX_DROPS,
    IF_RX_PACKETS,
    IF_RX_CORRECTED_ERRORS,
    IF_RX_DROPS,
    IF_EGRESS_NODE_NUM_FLOWS,   // 30
    IF_INGRESS_NODE_NUM_FLOWS,
    IF_FLOW_ID,
    IF_PACKET_DA,
    IF_PEAK_DATA_RATE,
    IF_BURST_SIZE,
    IF_LEASE_TIME,
    IF_FLOW_TAG,
    IF_NODE_MOCA_VERSION,
    IF_NODE_MAC_ADDRESS,
    IF_NODE_TX_GCD_RATE,        // 40
    IF_NODE_TX_POWER_REDUCTION,
    IF_NODE_PREFERRED_NC,
    IF_NODE_QAM256_CAPABLE,
    IF_NODE_PACKETS_AGGR_CAPABILITY,
    IF_MESH_TX_RATE,
    IF_TRAP_BELOW_PHY_THRESHOLD,
    IF_TRAP_ABOVE_PHY_THRESHOLD,
    NUM_OBJECTS                 // 48
} ObjectIndex_t;

/*!
 * \brief   Object Type enum
 *
 * This defines the various Object Types.
 * This is used to define the Type field of the struct ObjectInfo_t of the ObjectInfoTable.
 */
typedef enum
{
    BOOL_,
    STR_,
    UINT32_,
    INT32_,
    VARBIND_,
} ObjectType_t;

/*!
 * \brief   Link Status enum
 *
 * This defines the possible values for mocaIfStatus.
 * This is the status of the link.
 */
typedef enum
{
    LINKSTATUS_DISABLED=1,
    LINKSTATUS_NOLINK,
    LINKSTATUS_UP,
} LinkStatus_t;

/*!
 * \brief   Packets Aggregation Capability enum
 *
 * This defines the possible values for mocaIfPacketsAggrCapability and
 * mocaIfNodePacketsAggrCapability.
 */
typedef enum
{
    AGGR0 = 0,
    AGGR6 = 6,
    AGGR10 = 10,
} PacketsAggrCapability_t;

/*!
 * \brief   Get and Set function pointers
 *
 * This defines the function pointers for Get and Set.
 * The first index is used for FlowIndex or NodeIndex or MeshTxNodeIndex,
 * depending on which Get/Set.
 * The second index is used for MeshRxNodeIndex.
 * This is used to define the GetFunc and SetFunc fields of the
 * struct ObjectInfo_t of the ObjectInfoTable.
 */
typedef SYS_UINT32
(*GetFunc_t)(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1, SYS_UINT32 Index2, void* pObjectData);

typedef SYS_UINT32
(*SetFunc_t)(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1, SYS_UINT32 Index2, void* pObjectData);

/*!
 * \brief   Object Info type
 *
 * This defines the structure containing all the information required to describe an Object.
 */
typedef struct
{
    SYS_CHAR    Name[MAX_OBJECT_NAME_LEN + 1];
    GetFunc_t   GetFunc;        // Get function pointer
    SetFunc_t   SetFunc;        // Set function pointer
    SYS_UINT32  Type;
    SYS_UINT32  Args;
    SYS_CHAR    DefaultValueStr[MAX_OBJECT_VALUE_STRING_LEN + 1];
    SYS_CHAR    Units[MAX_UNITS_LEN + 1];
} ObjectInfo_t;

/*!
 * \brief   Variable Binding
 *
 * This defines the Variable Binding structure returned from traps.
 */
typedef struct
{
    SYS_CHAR * pOidString;      // Oid=ObjectId="mocaMeshTxRate.1.2.1" or "ifIndex"
    void *     pObjectData;     // &g_ObjectVar.ObjectInteger or &g_ObjectVar.ObjectUnsigned
} VarBind_t;

/*!
 * \brief   Command Line variables
 *
 * This defines the command line option variables that get setup while processing the
 * command line.
 *
 * ObjectName defines the Object Name that is derived from the command line non-option
 * argument.
 * Must copy it from argv[optind] since we are going to make it lowercase.
 *
 * GotAllGetObjects defines whether the All Get Objects (-a option) was specified in the
 * command line.
 *
 * GotGetObject defines whether the Get Object (-g option) was specified in the command line.
 *
 * GotSetObject defines whether the Set Object (-s option) was specified in the command line.
 *
 * SetObjectValueString defines the Object Value String arg following -s option.
 *
 * GotFlowIndex defines whether the Flow Index (-f option) was specified in the command line.
 *
 * FlowIndex defines the Flow Index arg following -f option.
 *
 * GotNodeIndex defines whether the Node Index (-n option) was specified in the command line.
 *
 * NodeIndex defines the Node Index arg following -n option.
 *
 * GotMeshTxNodeIndex defines whether the Mesh Tx Node Index (-t option) was specified in the
 * command line.
 *
 * MeshTxNodeIndex defines the Mesh Tx Node Index arg following -t option.
 *
 * GotMeshRxNodeIndex defines whether the Mesh Rx Node Index (-r option) was specified in the
 * command line.
 *
 * MeshRxNodeIndex defines the Mesh Rx Node Index arg following -r option.
 *
 * pIfaceStr defines the Interface Name string pointer.
 *
 * ppArgv defines the pointer to array of pointers that each point to the command line
 * argument strings.
 */
typedef struct
{
    SYS_CHAR     ObjectName[MAX_OBJECT_NAME_LEN + 1];
    SYS_BOOLEAN  GotAllGetObjects;
    SYS_BOOLEAN  GotGetObject;
    SYS_BOOLEAN  GotSetObject;
    SYS_CHAR     SetObjectValueString[MAX_OBJECT_VALUE_STRING_LEN + 1];
    SYS_BOOLEAN  GotFlowIndex;
    SYS_INT32    FlowIndex;
    SYS_BOOLEAN  GotNodeIndex;
    SYS_INT32    NodeIndex;
    SYS_BOOLEAN  GotMeshTxNodeIndex;
    SYS_INT32    MeshTxNodeIndex;
    SYS_BOOLEAN  GotMeshRxNodeIndex;
    SYS_INT32    MeshRxNodeIndex;
    SYS_CHAR *   pIfaceStr;
    SYS_CHAR **  ppArgv;
} CmdLineVars_t;

/*!
 * \brief   Object Value Variables
 *
 * This defines the object value variables (integer, unsigned, string, boolean, varbind).
 * There one of every type of object in this structure.
 * These are the object values returned from Get or assigned in Set.
 * Which object value is used is dependent on the type of the object.
 */
typedef struct
{
    SYS_BOOLEAN  Bool;
    SYS_CHAR     Str[MAX_OBJECT_VALUE_STRING_LEN + 1];
    SYS_UINT32   Uint;
    SYS_INT32    Int;
    VarBind_t    VarBind;
} ObjectVar_t;

/*******************************************************************************
 *      Private Function Declaration
 ******************************************************************************/

// These are the prototypes needed to build the ObjectInfoTable.
static SYS_UINT32 GetEnable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                            SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 SetEnable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                            SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetPrivacyEnable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                   SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 SetPrivacyEnable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                   SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetPassword(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                              SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 SetPassword(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                              SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetPreferredNC(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                 SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 SetPreferredNC(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                 SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetPhyThreshold(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                  SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 SetPhyThreshold(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                  SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetPhyThresholdEnable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                        SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 SetPhyThresholdEnable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                        SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetStatus(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                            SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetLinkUpTime(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetSoftwareVersion(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                     SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetMocaVersion(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetNetworkVersion(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                    SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetMacAddress(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetNodeID(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                            SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetName(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                          SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetNumNodes(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                              SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetNC(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                        SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetBackupNC(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                              SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetRFChannel(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                               SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetLOF(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                         SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetTabooChannelMask(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                      SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetNodeTabooChannelMask(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                          SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetCapabilityMask(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                    SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetQAM256Capable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                   SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetPacketsAggrCapability(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                           SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetTxGcdRate(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                               SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetTxPackets(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                               SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetTxDrops(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                             SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetRxPackets(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                               SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetRxCorrectedErrors(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                       SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetRxDrops(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                             SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetEgressNodeNumFlows(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                        SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetIngressNodeNumFlows(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                         SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetFlowID(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                            SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetPacketDA(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                              SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetPeakDataRate(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                  SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetBurstSize(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                               SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetLeaseTime(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                               SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetFlowTag(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                             SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetNodeMocaVersion(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                     SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetNodeMacAddress(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                    SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetNodeTxGcdRate(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                   SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetNodeTxPowerReduction(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                          SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetNodePreferredNC(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                     SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetNodeQAM256Capable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                       SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetNodePacketsAggrCapability(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                               SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetMeshTxRate(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetTrapBelowPhyThreshold(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                           SYS_UINT32 Index2, void* pObjectData);
static SYS_UINT32 GetTrapAbovePhyThreshold(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                           SYS_UINT32 Index2, void* pObjectData);
static SYS_BOOLEAN StringToInt(SYS_CHAR * pString, SYS_INT32 * pInt);
static void ListInterfaces(void);
static void ListObjects(void);
static void SaveObject(SYS_UINT32 ObjectIndex, void *pObjectData);
static void RestoreObject(SYS_UINT32 ObjectIndex, void *pObjectData);

/*******************************************************************************
 *      Public Data
 ******************************************************************************/


/*******************************************************************************
 *      Private Data
 ******************************************************************************/

/*!
 * \brief   Object Info Table
 *
 * This defines the table of MMA supported Objects and associated Object information.
 * Must align each entry in ObjectInfoTable with each entry in ObjectIndex_t enum.
 */
static const ObjectInfo_t ObjectInfoTable[] =
{
//ObjectName                  GetFunc                       SetFunc                Type     Args     Default  Units
//----------                  -------                       -------                ----     ----     -------  -----
{"Enable",                    GetEnable,                    SetEnable,             BOOL_,   NO_ARGS, "",      ""},
{"PrivacyEnable",             GetPrivacyEnable,             SetPrivacyEnable,      BOOL_,   NO_ARGS, "",      ""},
{"Password",                  GetPassword,                  SetPassword,           STR_,    NO_ARGS, "",      ""},
{"PreferredNC",               GetPreferredNC,               SetPreferredNC,        BOOL_,   NO_ARGS, "",      ""},
{"PhyThreshold",              GetPhyThreshold,              SetPhyThreshold,       UINT32_, NO_ARGS, "200",   "Mbps"},
{"PhyThresholdEnable",        GetPhyThresholdEnable,        SetPhyThresholdEnable, BOOL_,   NO_ARGS, "FALSE", ""},
{"Status",                    GetStatus,                    SYS_NULL,              INT32_,  NO_ARGS, "",      ""},
{"LinkUpTime",                GetLinkUpTime,                SYS_NULL,              UINT32_, NO_ARGS, "",      "seconds"},
{"SoftwareVersion",           GetSoftwareVersion,           SYS_NULL,              STR_,    NO_ARGS, "",      ""},
{"MocaVersion",               GetMocaVersion,               SYS_NULL,              INT32_,  NO_ARGS, "",      ""},
{"NetworkVersion",            GetNetworkVersion,            SYS_NULL,              INT32_,  NO_ARGS, "",      ""},
{"MacAddress",                GetMacAddress,                SYS_NULL,              STR_,    NO_ARGS, "",      ""},
{"NodeID",                    GetNodeID,                    SYS_NULL,              UINT32_, NO_ARGS, "",      ""},
{"Name",                      GetName,                      SYS_NULL,              STR_,    NO_ARGS, "",      ""},
{"NumNodes",                  GetNumNodes,                  SYS_NULL,              UINT32_, NO_ARGS, "",      "nodes"},
{"NC",                        GetNC,                        SYS_NULL,              UINT32_, NO_ARGS, "",      ""},
{"BackupNC",                  GetBackupNC,                  SYS_NULL,              UINT32_, NO_ARGS, "",      ""},
{"RFChannel",                 GetRFChannel,                 SYS_NULL,              INT32_,  NO_ARGS, "",      "MHz"},
{"LOF",                       GetLOF,                       SYS_NULL,              INT32_,  NO_ARGS, "",      "MHz"},
{"TabooChannelMask",          GetTabooChannelMask,          SYS_NULL,              UINT32_, NO_ARGS, "",      ""},
{"NodeTabooChannelMask",      GetNodeTabooChannelMask,      SYS_NULL,              UINT32_, NO_ARGS, "",      ""},
{"CapabilityMask",            GetCapabilityMask,            SYS_NULL,              UINT32_, NO_ARGS, "",      ""},
{"QAM256Capable",             GetQAM256Capable,             SYS_NULL,              BOOL_,   NO_ARGS, "",      ""},
{"PacketsAggrCapability",     GetPacketsAggrCapability,     SYS_NULL,              INT32_,  NO_ARGS, "",      "packet aggregation"},
{"TxGcdRate",                 GetTxGcdRate,                 SYS_NULL,              UINT32_, NO_ARGS, "",      "Mbps"},
{"TxPackets",                 GetTxPackets,                 SYS_NULL,              UINT32_, NO_ARGS, "",      "packets"},
{"TxDrops",                   GetTxDrops,                   SYS_NULL,              UINT32_, NO_ARGS, "",      "packets"},
{"RxPackets",                 GetRxPackets,                 SYS_NULL,              UINT32_, NO_ARGS, "",      "packets"},
{"RxCorrectedErrors",         GetRxCorrectedErrors,         SYS_NULL,              UINT32_, NO_ARGS, "",      ""},
{"RxDrops",                   GetRxDrops,                   SYS_NULL,              UINT32_, NO_ARGS, "",      "packets"},
{"EgressNodeNumFlows",        GetEgressNodeNumFlows,        SYS_NULL,              UINT32_, NO_ARGS, "",      "flows"},
{"IngressNodeNumFlows",       GetIngressNodeNumFlows,       SYS_NULL,              UINT32_, NO_ARGS, "",      "flows"},
{"FlowID",                    GetFlowID,                    SYS_NULL,              STR_,    F_ARGS,  "",      ""},
{"PacketDA",                  GetPacketDA,                  SYS_NULL,              STR_,    F_ARGS,  "",      ""},
{"PeakDataRate",              GetPeakDataRate,              SYS_NULL,              UINT32_, F_ARGS,  "",      "Kbps"},
{"BurstSize",                 GetBurstSize,                 SYS_NULL,              UINT32_, F_ARGS,  "",      "flow packets"},
{"LeaseTime",                 GetLeaseTime,                 SYS_NULL,              UINT32_, F_ARGS,  "",      "seconds"},
{"FlowTag",                   GetFlowTag,                   SYS_NULL,              UINT32_, F_ARGS,  "",      ""},
{"NodeMocaVersion",           GetNodeMocaVersion,           SYS_NULL,              INT32_,  N_ARGS,  "",      ""},
{"NodeMacAddress",            GetNodeMacAddress,            SYS_NULL,              STR_,    N_ARGS,  "",      ""},
{"NodeTxGcdRate",             GetNodeTxGcdRate,             SYS_NULL,              UINT32_, N_ARGS,  "",      "Mbps"},
{"NodeTxPowerReduction",      GetNodeTxPowerReduction,      SYS_NULL,              UINT32_, N_ARGS,  "",      "dB"},
{"NodePreferredNC",           GetNodePreferredNC,           SYS_NULL,              BOOL_,   N_ARGS,  "",      ""},
{"NodeQAM256Capable",         GetNodeQAM256Capable,         SYS_NULL,              BOOL_,   N_ARGS,  "",      ""},
{"NodePacketsAggrCapability", GetNodePacketsAggrCapability, SYS_NULL,              INT32_,  N_ARGS,  "",      "packet aggregation"},
{"MeshTxRate",                GetMeshTxRate,                SYS_NULL,              UINT32_, TR_ARGS, "",      "Mbps"},
{"TrapBelowPhyThreshold",     GetTrapBelowPhyThreshold,     SYS_NULL,              VARBIND_,NO_ARGS, "",      "Mbps"},
{"TrapAbovePhyThreshold",     GetTrapAbovePhyThreshold,     SYS_NULL,              VARBIND_,NO_ARGS, "",      ""},
};

/*!
 * \brief   Command Line variables
 *
 * This defines the command line option variables that get setup while processing the
 * command line and their default values.
 */
static CmdLineVars_t g_CmdLineVars =
{
    "",             // ObjectName
    SYS_FALSE,      // GotAllGetObjects
    SYS_FALSE,      // GotGetObject
    SYS_FALSE,      // GotSetObject
    "",             // SetObjectValueString
    SYS_FALSE,      // GotFlowIndex
    0,              // FlowIndex
    SYS_FALSE,      // GotNodeIndex
    0,              // NodeIndex
    SYS_FALSE,      // GotMeshTxNodeIndex
    0,              // MeshTxNodeIndex
    SYS_FALSE,      // GotMeshRxNodeIndex
    0,              // MeshRxNodeIndex
    SYS_NULL,       // pIfaceStr
    SYS_NULL,       // ppArgv
};

/*!
 * \brief   Object Value Variables
 *
 * This defines the object value variables.
 * There one of every type of object in this structure.
 */
static ObjectVar_t g_ObjectVar =
{
    SYS_FALSE,
    "",
    0,
    0,
    {SYS_NULL, SYS_NULL},
};

/*******************************************************************************
 *      Public Function Definitions
 ******************************************************************************/


/*******************************************************************************
 *      Private Function Definitions
 ******************************************************************************/

/***************************************************************************//**
 * \brief   Usage
 *
 * \param[in]   argv        Pointer to array of pointers that each point to the
 *                          command line argument strings.
 *
 * \return      None
 *
 * Display command line options and usage.
 *
 * \note
 *
 ******************************************************************************/
static void Usage(char** argv)
{
    printf("\nUsage: %s [OPTIONS] [ObjectName] [OPTIONS]...\n\n"
       "Options:\n"
       "-v                  Display MoCA MIB App version number string\n"
       "-L                  List all clink interfaces\n"
       "-l                  List all %s supported objects along with R/W capabilities,\n"
       "                    type, required indices, and units\n"
       "-a                  Performs Get for all %s supported objects that have no indices\n"
       "ObjectName          Name of MoCA MIB object with leading 'moca' or 'mocaIf' removed\n"
       "-g                  Performs Get for the specified object\n"
       "-s SetValue         Performs Set for specified object assigning value of SetValue\n"
       "                    SetValue may be string (abc),\n"
       "                    integer (1, -1, 0xFFFFFFFF),\n"
       "                    boolean (T/F/true/false), or unsigned\n"
       "                    depending on the Type of the Object you specify\n"
       "-f FlowIndex        Specify FlowIndex into FlowStatsTable (1..%d)\n"
       "-n NodeIndex        Specify NodeIndex into NodeTable (0..%d)\n"
       "-t MeshTxNodeIndex  Specify MeshTxNodeIndex into MeshTable (0..%d)\n"
       "-r MeshRxNodeIndex  Specify MeshRxNodeIndex into MeshTable (0..%d)\n"
       "-i IfaceName        Select which interface to use e.g. eth1\n"
       "-?                  Displays this help page\n"
       "\n",
       argv[0], argv[0], argv[0], QOS_MAX_FLOWS,
       MAX_NUM_NODES - 1, MAX_NUM_NODES - 1, MAX_NUM_NODES - 1);
    exit(1);
} // Usage()


/***************************************************************************//**
 * \brief   ProcessCmdLine
 *
 * \param[in]   argc        Argument count including mocamib.
 * \param[in]   argv        Pointer to array of pointers that each point to the
 *                          command line argument strings.
 *
 * \return      None
 *
 * Process the command line options.
 *
 * \note
 *  int opterr    - If the value of this variable is nonzero, then getopt prints
 *                  an error message to the standard error stream if it
 *                  encounters an unknown option character or an option with a
 *                  missing required argument. This is the default behavior.  If
 *                  you set this variable to zero, getopt does not print any
 *                  messages, but it still returns the character ? to indicate
 *                  an error.
 *  int optopt    - When getopt encounters an unknown option character or an
 *                  option with a missing required argument, it stores that
 *                  option character in this variable. You can use this for
 *                  providing your own diagnostic messages.
 *  int optind    - This variable is set by getopt to the index of the next
 *                  entry of the argv array to be processed. Once getopt has
 *                  found all of the option arguments, you can use this variable
 *                  to determine where the remaining non-option arguments begin.
 *                  The initial value of this variable is 1.
 *  char * optarg - This variable is set by getopt to point at the value of the
 *                  option argument, for those options that accept arguments.
 *
 *  We must be careful.
 *  The getopt() changes the order of argv[] args and places the
 *  non-option args at the end.
 *  e.g.
 *  Initially, argc=4, argv[0..3]=mocamib Enable -g
 *  After getarg()=-1, argc=4, argv[0..3]=mocamib -g Enable
 *  Therefore, after getarg()=-1, argv[optind] is always the non-option arg.
 *  You should not keep track of arg index or you can really get messed up.
 *
 ******************************************************************************/
static void ProcessCmdLine(int argc, char** argv)
{
    // can't have no args at all after mocamib
    if (argc == 1)
    {
        printf("Missing required args.\n");
        Usage(argv);
    }

    // save away ppArgv for later use throughout these functions
    g_CmdLineVars.ppArgv = argv;

    // don't have getopt() print error msg when invalid cmd line option specified
    opterr = 0;

    for (;;)
    {
        // extra arg required for -s -f -n -t -r -i
        SYS_INT32 c = getopt(argc, argv, "vLlags:f:n:t:r:i:?");

        // getopt() says no more args
        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 'v':
                printf("%s v" MMA_VERSION_STR "\n", argv[0]);
                exit(0);
                break;

            case 'L':
                ListInterfaces();
                exit(0);
                break;

            case 'l':
                ListObjects();
                exit(0);
                break;

            case 'a':
                g_CmdLineVars.GotAllGetObjects = SYS_TRUE;
                // make sure we do a Get, not a Set
                g_CmdLineVars.GotGetObject = SYS_TRUE;
                break;

            case 'g':
                g_CmdLineVars.GotGetObject = SYS_TRUE;
                break;

            case 's':
                g_CmdLineVars.GotSetObject = SYS_TRUE;
                // does -s have an arg?
                //      is arg SetObjectValueString able to fit in SetObjectValueString?
                if (optarg)
                {
                    if (strlen(optarg) < sizeof(g_CmdLineVars.SetObjectValueString))
                    {
                        // Don't know what type of object this is now.
                        // So we will decode this object value string later when we know
                        // what type of object it is.
                        strcpy(g_CmdLineVars.SetObjectValueString, optarg);
                    }
                    else
                    {
                        printf("Value string too large with -s Set option.\n");
                        Usage(argv);
                    }
                }
                else
                {
                    printf("Missing Value with -s Set option.\n");
                    Usage(argv);
                }
                break;

            case 'f':
                g_CmdLineVars.GotFlowIndex = SYS_TRUE;
                // does -f have an arg?
                if (optarg)
                {
                    if (StringToInt(optarg, &g_CmdLineVars.FlowIndex) == SYS_SUCCESS)
                    {
                        // FlowIndex starts at 1
                        if (g_CmdLineVars.FlowIndex < 1 ||
                            g_CmdLineVars.FlowIndex > QOS_MAX_FLOWS)
                        {
                            printf("FlowIndex out of range (1..%d).\n", QOS_MAX_FLOWS);
                            Usage(argv);
                        }
                    }
                    else
                    {
                        printf("Need valid FlowIndex with -f Flow option.\n");
                        Usage(argv);
                    }
                }
                else
                {
                    printf("Missing FlowIndex with -f Flow option.\n");
                    Usage(argv);
                }
                break;

            case 'n':
                g_CmdLineVars.GotNodeIndex = SYS_TRUE;
                // does -n have an arg?
                if (optarg)
                {
                    if (StringToInt(optarg, &g_CmdLineVars.NodeIndex) == SYS_SUCCESS)
                    {
                        // NodeIndex starts at 0
                        if (g_CmdLineVars.NodeIndex >= MAX_NUM_NODES)
                        {
                            printf("NodeIndex out of range (0..%d).\n", MAX_NUM_NODES - 1);
                            Usage(argv);
                        }
                    }
                    else
                    {
                        printf("Need valid NodeIndex with -n Node option.\n");
                        Usage(argv);
                    }
                }
                else
                {
                    printf("Missing NodeIndex with -n Node option.\n");
                    Usage(argv);
                }
                break;

            case 't':
                g_CmdLineVars.GotMeshTxNodeIndex = SYS_TRUE;
                // does -t have an arg?
                if (optarg)
                {
                    if (StringToInt(optarg, &g_CmdLineVars.MeshTxNodeIndex) == SYS_SUCCESS)
                    {
                        // MeshTxNodeIndex starts at 0
                        if (g_CmdLineVars.MeshTxNodeIndex >= MAX_NUM_NODES)
                        {
                            printf("MeshTxNodeIndex out of range (0..%d).\n",
                                   MAX_NUM_NODES - 1);
                            Usage(argv);
                        }
                    }
                    else
                    {
                        printf("Need valid MeshTxNodeIndex with -t MeshTxNode option.\n");
                        Usage(argv);
                    }
                }
                else
                {
                    printf("Missing MeshTxNodeIndex with -t MeshTxNode option.\n");
                    Usage(argv);
                }
                break;

            case 'r':
                g_CmdLineVars.GotMeshRxNodeIndex = SYS_TRUE;
                // does -r have an arg?
                if (optarg)
                {
                    if (StringToInt(optarg, &g_CmdLineVars.MeshRxNodeIndex) == SYS_SUCCESS)
                    {
                        // MeshRxNodeIndex starts at 0
                        if (g_CmdLineVars.MeshRxNodeIndex >= MAX_NUM_NODES)
                        {
                            printf("MeshRxNodeIndex out of range (0..%d).\n",
                                   MAX_NUM_NODES - 1);
                            Usage(argv);
                        }
                    }
                    else
                    {
                        printf("Need valid MeshRxNodeIndex with -r MeshRxNode option.\n");
                        Usage(argv);
                    }
                }
                else
                {
                    printf("Missing MeshRxNodeIndex with -r MeshRxNode option.\n");
                    Usage(argv);
                }
                break;

            case 'i':
                // does -i have an arg?
                if (optarg)
                {
                    g_CmdLineVars.pIfaceStr = optarg;
                }
                else
                {
                    printf("Missing InterfaceName with -i Interface option.\n");
                    Usage(argv);
                }
                break;

            case '?':
            // what the heck is that option?
            default:
                Usage(argv);
        }
    }
    // options -v -L -l -? have already caused the program to exit

    // is optind in the valid range?
    if (optind < argc)
    {
        // does the ObjectName arg exist?
        if (argv[optind])
        {
            //  is the arg ObjectName going to fit in the ObjectName?
            if (strlen(argv[optind]) < sizeof(g_CmdLineVars.ObjectName))
            {
                // need to copy it so we can later make it lowercase
                strcpy(g_CmdLineVars.ObjectName, argv[optind]);
            }
            else
            {
                // can't fit the Arg ObjectName into ObjectName
                g_CmdLineVars.ObjectName[0] = '\0';
            }
        }
        else
        {
            // ObjectName arg does not exist
            g_CmdLineVars.ObjectName[0] = '\0';
        }
    }
    else
    {
        // optind == argc so command line has no ObjectName
        g_CmdLineVars.ObjectName[0] = '\0';
    }

    // The ObjectName better not start with a '-'
    if (g_CmdLineVars.ObjectName[0] == '-')
    {
        printf("Internal Error: ObjectName=%s starts with a dash\n",
               g_CmdLineVars.ObjectName);
        exit(1);
    }

    // if it's not an AllGet then we better have a valid ObjectNameArgIndex
    if (!g_CmdLineVars.GotAllGetObjects && g_CmdLineVars.ObjectName[0] == '\0')
    {
        printf("Valid %s supported ObjectName required.\n", argv[0]);
        Usage(argv);
    }

    // can't have both Get and Set options at the same time
    if (g_CmdLineVars.GotGetObject && g_CmdLineVars.GotSetObject)
    {
        printf("Can't have both Get and Set options at the same time.\n");
        Usage(argv);
    }

    // must have either Get or Set option specified.
    if (!g_CmdLineVars.GotGetObject && !g_CmdLineVars.GotSetObject)
    {
        printf("Must have either Get or Set option specified.\n");
        Usage(argv);
    }

    // can't have more than one ObjectName
    if (optind + 1 < argc)
    {
        printf("Only one ObjectName allowed.\n");
        Usage(argv);
    }
} // ProcessCmdLine()


/***************************************************************************//**
 * \brief   DisplayObject
 *
 * \param[in]   ObjectIndex     Index into the ObjectInfoTable.
 * \param[in]   pObjectData     Pointer to Object's data to display.
 *
 * \return      None
 *
 * Display object's name, data and units.
 *
 * \note
 *      Format of Display:
 *          ObjectName=Value units
 *          or
 *          ObjectName={Oid="mocaMeshTxRate.1.2.1",Value=150}
 *      where value can be data of type Boolean, Integer, Unsigned, String or VarBind.
 *
 ******************************************************************************/
static void DisplayObject(SYS_UINT32 ObjectIndex, void *pObjectData)
{
    if (pObjectData == SYS_NULL)
    {
        printf("Internal Error: DisplayObject(pObjectData=NULL)\n");
        exit(1);
    }

    printf("%s=", ObjectInfoTable[ObjectIndex].Name);
    switch (ObjectInfoTable[ObjectIndex].Type)
    {
    case BOOL_:
        {
            SYS_BOOLEAN * const pBool = pObjectData;
            printf("%s\n", *pBool ? "TRUE" : "FALSE");
            break;
        }
    case STR_:
        {
            SYS_CHAR * const pStr = pObjectData;
            printf("\"%s\"\n", pStr);
            break;
        }
    case UINT32_:
        {
            SYS_UINT32 * const pUint = pObjectData;
            SYS_UINT32 Len = strlen(ObjectInfoTable[ObjectIndex].Name);
            const SYS_CHAR MaskStr[] = "Mask";
            const SYS_UINT32 MaskLen = sizeof(MaskStr) - 1;

            // Names ending in MaskStr use "0x%08x" format, non-mask Names use "%u" format.
            // Is the Name big enough to hold MaskStr?
            if (Len >= MaskLen)
            {
                // are the last 4 chars of Name == MaskStr?
                if (strcmp(&ObjectInfoTable[ObjectIndex].Name[Len - MaskLen], MaskStr) == 0)
                {
                    // use hex format
                    printf("0x%08x %s\n", *pUint, ObjectInfoTable[ObjectIndex].Units);
                }
                else
                {
                    // use unsigned decimal format
                    printf("%u %s\n", *pUint, ObjectInfoTable[ObjectIndex].Units);
                }
            }
            else
            {
                // use unsigned decimal format
                printf("%u %s\n", *pUint, ObjectInfoTable[ObjectIndex].Units);
            }
            break;
        }
    case INT32_:
        {
            SYS_INT32 * const pInt = pObjectData;
            // use integer decimal format
            printf("%d %s\n", *pInt, ObjectInfoTable[ObjectIndex].Units);
            break;
        }
    case VARBIND_:
        {
            VarBind_t * const pVarBind = pObjectData;
            switch (ObjectIndex)
            {
            case IF_TRAP_BELOW_PHY_THRESHOLD:
                {
                    SYS_UINT32 * pUint = pVarBind->pObjectData;
                    if (pVarBind->pOidString == SYS_NULL)
                    {
                        if (pUint == SYS_NULL)
                        {
                            printf("{Oid=NULL,Value=NULL}\n");
                        }
                        else
                        {
                            printf("{Oid=NULL,Value=%u %s}\n", *pUint,
                                   ObjectInfoTable[ObjectIndex].Units);
                        }
                    }
                    else
                    {
                        if (pUint == SYS_NULL)
                        {
                            printf("{Oid=\"%s\",Value=NULL}\n", pVarBind->pOidString);
                        }
                        else
                        {
                            printf("{Oid=\"%s\",Value=%u %s}\n", pVarBind->pOidString,
                                   *pUint, ObjectInfoTable[ObjectIndex].Units);
                        }
                    }
                    break;
                }
            case IF_TRAP_ABOVE_PHY_THRESHOLD:
                {
                    SYS_INT32 * pInt = pVarBind->pObjectData;
                    if (pVarBind->pOidString == SYS_NULL)
                    {
                        if (pInt == SYS_NULL)
                        {
                            printf("{Oid=NULL,Value=NULL}\n");
                        }
                        else
                        {
                            printf("{Oid=NULL,Value=%d}\n", *pInt);
                        }
                    }
                    else
                    {
                        if (pInt == SYS_NULL)
                        {
                            printf("{Oid=\"%s\",Value=NULL}\n", pVarBind->pOidString);
                        }
                        else
                        {
                            printf("{Oid=\"%s\",Value=%d}\n", pVarBind->pOidString, *pInt);
                        }
                    }
                    break;
                }
            default:
                printf("Internal Error: ObjectIndex=%d not a known trap\n", ObjectIndex);
                exit(1);
            }
        }
        break;
    default:
        printf("Internal Error: Invalid ObjectInfoTable[%d].Type=%d\n",
               ObjectIndex, ObjectInfoTable[ObjectIndex].Type);
        exit(1);
    }
} // DisplayObject()


/*!
 * \brief   Interface Configuration Table
 *
 * The Interface Configuration Table, or mocaIfConfigTable, supports the configuration of
 * RF frequency, transmit power, link privacy, and traps related parameters in the
 * managed node.  The managed node is identified by the interface index (ifIndex).
 * The support of the configuration table is optional, and all of its objects are writable.
 * The ability to disable the Transmit Power Control feature was not specified in the
 * previous MoCA 1.0 specification "MoCA-M/P-SPEC-V1.0-04052007" released on
 * April 5, 2007, but it has been determined that this capability can provide
 * diagnostic help to network operators.
 * The latest MoCA specification [6] has been revised to include this support,
 * but interoperation with previously certified products is not supported.
 * Note that a similar named management object may exist in previously deployed products,
 * but using that object may result in a different behavior.
 */


/***************************************************************************//**
 * \brief   GetEnable
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for Enable object.
 *
 * \note
 * The API clnk_get_my_node_info() returns field LinkStatus in struct ClnkDef_MyNodeInfo_t.
 * If clnk_get_my_node_info() fails then the Enable is FALSE.
 * Otherwise LinkStatus tells you the value of Enable.
 *
 ******************************************************************************/
static SYS_UINT32 GetEnable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                            SYS_UINT32 Index2, void* pObjectData)
{
    SYS_BOOLEAN * const pObjectDataBoolean = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    *pObjectDataBoolean = SYS_TRUE;
    // get my LinkStatus
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        // failure of clnk_get_my_node_info() implies link disabled (low power mode).
        *pObjectDataBoolean = SYS_FALSE;
    }
    else if (MyNodeInfo.LinkStatus)
    {
        *pObjectDataBoolean = SYS_TRUE;
    }
    else
    {
        *pObjectDataBoolean = SYS_FALSE;
    }
    return Status;
} // GetEnable()


/***************************************************************************//**
 * \brief   SetEnable
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to set value.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Set for Enable object.
 * Enable the MoCA interface if this value is 'true'.
 * Disable the MoCA interface if this value is 'false'.
 * If this managed object is disabled, it may not be possible to enable this object via the
 * SNMP protocol.
 * The mechanism to re-enable this object is out of the scope of this MIB definition draft.
 *
 * \note
 * To stop the SoC and enter low power mode, use clnk_stop_device().
 * After invoking clnk_stop_device(), device is no longer accessible to the MoCA network.
 * The clnk_reset_device() can be used to enable a disabled/stopped node but it has a
 * side effect of resetting the node.
 * If the managed object is disabled, it can only be re-enabled if there is an
 * Ethernet interface connected.
 *
 ******************************************************************************/
static SYS_UINT32 SetEnable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                            SYS_UINT32 Index2, void* pObjectData)
{
    SYS_BOOLEAN * const pObjectDataBoolean = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;

    if (*pObjectDataBoolean)
    {
        if (clnk_reset_device(pClnkCtx))
        {
            Status = ~SYS_SUCCESS;
        }
    }
    else
    {
        if (clnk_stop_device(pClnkCtx))
        {
            Status = ~SYS_SUCCESS;
        }
    }
    return Status;
} // SetEnable()


/***************************************************************************//**
 * \brief   GetPrivacyEnable
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for PrivacyEnable object.
 *
 * \note
 * The API clnk_get_soc_opt() returns field SwConfig (bit 12) in struct clnk_soc_opt.
 * When this bit is on, privacy is enabled.
 * Can't use clnk_get_privacy_info() using field enable in struct priv_info_t because it
 * returns in field enable four possibilities:
 * PRIV_DISABLED, PRIV_ENABLED, PRIV_SHUTDOWN, PRIV_STATUS_EOE.
 *
 ******************************************************************************/
static SYS_UINT32 GetPrivacyEnable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                   SYS_UINT32 Index2, void* pObjectData)
{
    SYS_BOOLEAN * const pObjectDataBoolean = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    struct clnk_soc_opt SocOpt;

    *pObjectDataBoolean = SYS_FALSE;
    // get SwConfig
    if (clnk_get_soc_opt(pClnkCtx, &SocOpt))
    {
        Status = ~SYS_SUCCESS;
    }
    else if (SocOpt.SwConfig & CLNK_DEF_SW_CONFIG_PRIVACY_BIT)
    {
        *pObjectDataBoolean = SYS_TRUE;
    }
    else
    {
        *pObjectDataBoolean = SYS_FALSE;
    }
    return Status;
} // GetPrivacyEnable()


/***************************************************************************//**
 * \brief   SetPrivacyEnable
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to set value.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Set for PrivacyEnable object.
 * Enable link privacy if this value is 'true', and use the 'mocaIfPassword' to generate
 * the MAC management key and initial privacy management key.
 * Disable link privacy and do not perform link encryption if this value is 'false'.
 * This node will drop from the network if this value changes.
 * If this managed object is disabled, it may not be possible to enable this object via the
 * SNMP protocol.
 * The mechanism to re-enable this object is out of the scope of this MIB definition draft.
 *
 * \note
 * The API clnk_set_soc_opt() sets field SwConfig (bit 12) in
 * struct clnk_soc_opt.
 * When this bit is on, privacy is enabled.
 * To write the new PrivacyEnable bit of the SwConfig field to the clink.conf file,
 * a clnk_read_cfg_file() call must be made first.
 * If the new PrivacyEnable bit of the SwConfig field is different than what is already
 * in the file, update the PrivacyEnable bit of the SwConfig field in Bss,
 * and call clnk_write_cfg_file().
 * The API clnk_set_soc_opt() sets the PrivacyEnable bit of the SwConfig field in
 * struct clnk_soc_opt.
 * To use the new SwConfig, a clnk_get_soc_opt() call must be made first.
 * If the new PrivacyEnable bit of the SwConfig field is different than what is
 * currently being used, update the PrivacyEnable bit of the SwConfig field in SocOpt,
 * and call clnk_set_soc_opt() and then reset the SoC by calling clnk_reset_device()
 * to apply the change.
 * The ability to change this parameter dynamically is not supported by our products.
 *
 ******************************************************************************/
static SYS_UINT32 SetPrivacyEnable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                   SYS_UINT32 Index2, void* pObjectData)
{
    SYS_BOOLEAN * const pObjectDataBoolean = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    struct clnk_soc_opt SocOpt;
    BridgeSetupStruct Bss;

    // read clink.conf and use default clink config filename
    // to get SwConfig from clink.conf
    if (!clnk_read_cfg_file(SYS_NULL, &Bss, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // is PrivacyEnable bit in clink.conf changed from the new PrivacyEnable?
    else if ((Bss.SwConfig & CLNK_DEF_SW_CONFIG_PRIVACY_BIT && !*pObjectDataBoolean) ||
            (!(Bss.SwConfig & CLNK_DEF_SW_CONFIG_PRIVACY_BIT) && *pObjectDataBoolean))
    {
        // is the new PrivacyEnable TRUE?
        if (*pObjectDataBoolean)
        {
            // off->on
            Bss.SwConfig |= CLNK_DEF_SW_CONFIG_PRIVACY_BIT;
        }
        else
        {
            // on->off
            Bss.SwConfig &= ~CLNK_DEF_SW_CONFIG_PRIVACY_BIT;
        }

        // write clink.conf and use default clink config filename and
        // make file persistent to set SwConfig in clink.conf
        if (!clnk_write_cfg_file(SYS_NULL, &Bss, SYS_TRUE))
        {
            Status = ~SYS_SUCCESS;
        }
    }

    // we go back to base indentation
    if (Status == SYS_SUCCESS)
    {
        // get SocOpt to get currently used SwConfig
        if (clnk_get_soc_opt(pClnkCtx, &SocOpt))
        {
            Status = ~SYS_SUCCESS;
        }
        // get mocapassword because clnk_get_soc_opt() has a bug in getting mocapassword
        else if (clnk_get_current_mocapassword(pClnkCtx, &SocOpt.mocapassword))
        {
            Status = ~SYS_SUCCESS;
        }
        // is PrivacyEnable bit currently used different than the new PrivacyEnable?
        else if ((SocOpt.SwConfig & CLNK_DEF_SW_CONFIG_PRIVACY_BIT &&
                    !*pObjectDataBoolean) ||
                (!(SocOpt.SwConfig & CLNK_DEF_SW_CONFIG_PRIVACY_BIT) &&
                    *pObjectDataBoolean))
        {
            // is the new PrivacyEnable TRUE?
            if (*pObjectDataBoolean)
            {
                // off->on
                SocOpt.SwConfig |= CLNK_DEF_SW_CONFIG_PRIVACY_BIT;
            }
            else
            {
                // on->off
                SocOpt.SwConfig &= ~CLNK_DEF_SW_CONFIG_PRIVACY_BIT;
            }

            // set SocOpt to make it the currently used SwConfig
            if (clnk_set_soc_opt(pClnkCtx, &SocOpt))
            {
                Status = ~SYS_SUCCESS;
            }
            // reset the SoC to apply the change
            else if (clnk_reset_device(pClnkCtx))
            {
                Status = ~SYS_SUCCESS;
            }
        }
    }
    return Status;
} // SetPrivacyEnable()


/***************************************************************************//**
 * \brief   GetPassword
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for Password object.
 * Return the MoCA password using an ASCII numeric string.
 * This value must be 12 to 17 decimal digits long.
 * Access is only allowed when used with SNMPv3 protocol, but not allowed when used with
 * SNMPv1 or SNMPv2c protocol.
 *
 * \note
 * The API clnk_get_current_mocapassword() returns field value[] in
 * struct clnk_nms_mocapassword_t.
 *
 ******************************************************************************/
static SYS_UINT32 GetPassword(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                              SYS_UINT32 Index2, void* pObjectData)
{
    SYS_CHAR * const pObjectDataString = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;

    pObjectDataString[0] = '\0';
    // get MocaPassword
    if (clnk_get_current_mocapassword(pClnkCtx,
                                      (clnk_nms_mocapassword_t *)pObjectDataString))
    {
        Status = ~SYS_SUCCESS;
    }
    return Status;
} // GetPassword()


/***************************************************************************//**
 * \brief   SetPassword
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to set value.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Set for Password object.
 * Specify the MoCA password using an ASCII numeric string.
 * This value must be 12 to 17 decimal digits long.
 * Access is only allowed when used with SNMPv3 protocol, but not allowed when used with
 * SNMPv1 or SNMPv2c protocol.
 * If this value changes, this node will drop from the network if 'mocaIfPrivacyEnable' is
 * 'true'.
 * If this managed object is changed, it may not be possible to change this object again via
 * the SNMP protocol.
 * The mechanism to change this object again is out of the scope of this
 * MIB definition draft.
 *
 * \note
 * To write the new mocapassword to the clink.conf file,
 * a clnk_read_cfg_file() call must be made first.
 * If the new mocapassword is different than what is already
 * in the file, update the mocapassword in Bss,
 * and call clnk_write_cfg_file().
 * The API clnk_send_key() generates and sends the privacy seed keys from
 * the 40 char password seed.
 * Lastly, reset the SoC by calling clnk_reset_device() to apply the change.
 * The ability to change this parameter dynamically is not supported by our products.
 * If the password string contains other than 0-9's or is too small or is too long,
 * then the MMA program will reject the command issued as invalid.
 *
 ******************************************************************************/
static SYS_UINT32 SetPassword(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                              SYS_UINT32 Index2, void* pObjectData)
{
    SYS_CHAR * const pObjectDataString = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 i;
    SYS_UINT32 Len;
    BridgeSetupStruct Bss;

    // see if invalid chars exist in mocapassword decimal digit string
    Len = strlen(pObjectDataString);
    for (i = 0; i < Len; i++)
    {
        if (pObjectDataString[i] < '0' || pObjectDataString[i] > '9')
        {
            break;
        }
    }

    // is password len too small or
    // password len too big or
    // password string has invalid chars?
    if (Len < MIN_MOCAPASSWORD_LEN || Len > MAX_MOCAPASSWORD_LEN || i != Len)
    {
        printf("Password invalid.\n");
        Usage(g_CmdLineVars.ppArgv);
    }

    // read clink.conf and use default clink config filename
    // to get mocapassword from clink.conf
    else if (!clnk_read_cfg_file(SYS_NULL, &Bss, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // is the new mocapassword changed from what is in the clink.conf file?
    else if (strcmp(pObjectDataString, Bss.mocapassword) != 0)
    {
        // set mocapassword in Bss
        strcpy(Bss.mocapassword, pObjectDataString);

        // write clink.conf and use default clink config filename and
        // make file persistent to set mocapassword in clink.conf
        if (!clnk_write_cfg_file(SYS_NULL, &Bss, SYS_TRUE))
        {
            Status = ~SYS_SUCCESS;
        }
    }

    // we go back to base indentation so strcmp above doesn't effect this
    if (Status == SYS_SUCCESS)
    {
        // generate and send the privacy seed keys from the 40 char password seed
        if (!clnk_send_key(pClnkCtx, Bss.password, Bss.mocapassword))
        {
            Status = ~SYS_SUCCESS;
        }
        // reset the SoC to apply the change
        else if (clnk_reset_device(pClnkCtx))
        {
            Status = ~SYS_SUCCESS;
        }
    }
    return Status;
} // SetPassword()


/***************************************************************************//**
 * \brief   GetPreferredNC
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for PreferredNC object.
 * Set this node as preferred NC if this value is 'true'.
 * Do not set this node as preferred NC if this value is 'false'.
 * This value can be ignored by this MoCA interface when operating in a MoCA 1.0 network.
 *
 * \note
 * The API clnk_get_soc_opt() returns field SwConfig (bit 26) in struct clnk_soc_opt.
 * When this bit is on, PreferredNC is enabled.
 *
 ******************************************************************************/
static SYS_UINT32 GetPreferredNC(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                 SYS_UINT32 Index2, void* pObjectData)
{
    SYS_BOOLEAN * const pObjectDataBoolean = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    struct clnk_soc_opt SocOpt;

    *pObjectDataBoolean = SYS_FALSE;
    // get SwConfig
    if (clnk_get_soc_opt(pClnkCtx, &SocOpt))
    {
        Status = ~SYS_SUCCESS;
    }
    else if (SocOpt.SwConfig & CLNK_DEF_SW_CONFIG_PREFERRED_NC_BIT)
    {
        *pObjectDataBoolean = SYS_TRUE;
    }
    else
    {
        *pObjectDataBoolean = SYS_FALSE;
    }
    return Status;
} // GetPreferredNC()


/***************************************************************************//**
 * \brief   SetPreferredNC
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to set value.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Set for PreferredNC object.
 * Set this node as preferred NC if this value is 'true'.
 * Do not set this node as preferred NC if this value is 'false'.
 * This value can be ignored by this MoCA interface when operating in a MoCA 1.0 network.
 *
 * \note
 * The API clnk_set_soc_opt() sets field SwConfig (bit 26) in
 * struct clnk_soc_opt.
 * When this bit is on, PreferredNC is enabled.
 * To write the new PreferredNC bit of the SwConfig field to the clink.conf file,
 * a clnk_read_cfg_file() call must be made first.
 * If the new PreferredNC bit of the SwConfig field is different than what is already
 * in the file, update the PreferredNC bit of the SwConfig field in Bss,
 * and call clnk_write_cfg_file().
 * The API clnk_set_soc_opt() sets the PreferredNC bit of the SwConfig field in
 * struct clnk_soc_opt.
 * To use the new SwConfig, a clnk_get_soc_opt() call must be made first.
 * If the new PreferredNC bit of the SwConfig field is different than what is
 * currently being used, update the PreferredNC bit of the SwConfig field in SocOpt,
 * and call clnk_set_soc_opt() and then reset the SoC by calling clnk_reset_device()
 * to apply the change.
 * The ability to change this parameter dynamically is not supported by our products.
 *
 ******************************************************************************/
static SYS_UINT32 SetPreferredNC(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                 SYS_UINT32 Index2, void* pObjectData)
{
    SYS_BOOLEAN * const pObjectDataBoolean = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    struct clnk_soc_opt SocOpt;
    BridgeSetupStruct Bss;

    // read clink.conf and use default clink config filename
    // to get SwConfig from clink.conf
    if (!clnk_read_cfg_file(SYS_NULL, &Bss, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // is PreferredNC bit in clink.conf changed from the new PreferredNC?
    else if ((Bss.SwConfig & CLNK_DEF_SW_CONFIG_PREFERRED_NC_BIT && !*pObjectDataBoolean) ||
            (!(Bss.SwConfig & CLNK_DEF_SW_CONFIG_PREFERRED_NC_BIT) && *pObjectDataBoolean))
    {
        // is the new PreferredNC TRUE?
        if (*pObjectDataBoolean)
        {
            // off->on
            Bss.SwConfig |= CLNK_DEF_SW_CONFIG_PREFERRED_NC_BIT;
        }
        else
        {
            // on->off
            Bss.SwConfig &= ~CLNK_DEF_SW_CONFIG_PREFERRED_NC_BIT;
        }

        // write clink.conf and use default clink config filename and
        // make file persistent to set SwConfig in clink.conf
        if (!clnk_write_cfg_file(SYS_NULL, &Bss, SYS_TRUE))
        {
            Status = ~SYS_SUCCESS;
        }
    }

    // we go back to base indentation
    if (Status == SYS_SUCCESS)
    {
        // get SocOpt to get currently used SwConfig
        if (clnk_get_soc_opt(pClnkCtx, &SocOpt))
        {
            Status = ~SYS_SUCCESS;
        }
        // get mocapassword because clnk_get_soc_opt() has a bug in getting mocapassword
        else if (clnk_get_current_mocapassword(pClnkCtx, &SocOpt.mocapassword))
        {
            Status = ~SYS_SUCCESS;
        }
        // is PreferredNC bit currently used different than the new PreferredNC?
        else if ((SocOpt.SwConfig & CLNK_DEF_SW_CONFIG_PREFERRED_NC_BIT &&
                    !*pObjectDataBoolean) ||
                (!(SocOpt.SwConfig & CLNK_DEF_SW_CONFIG_PREFERRED_NC_BIT) &&
                    *pObjectDataBoolean))
        {
            // is the new PreferredNC TRUE?
            if (*pObjectDataBoolean)
            {
                // off->on
                SocOpt.SwConfig |= CLNK_DEF_SW_CONFIG_PREFERRED_NC_BIT;
            }
            else
            {
                // on->off
                SocOpt.SwConfig &= ~CLNK_DEF_SW_CONFIG_PREFERRED_NC_BIT;
            }

            // set SocOpt to make it the currently used SwConfig
            if (clnk_set_soc_opt(pClnkCtx, &SocOpt))
            {
                Status = ~SYS_SUCCESS;
            }
            // reset the SoC to apply the change
            else if (clnk_reset_device(pClnkCtx))
            {
                Status = ~SYS_SUCCESS;
            }
        }
    }
    return Status;
} // SetPreferredNC()


/***************************************************************************//**
 * \brief   GetPhyThreshold
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for PhyThreshold object.
 * Used to set the PhyThreshold for mocaTrapBelowPhyThreshold and mocaTrapAbovePhyThreshold.
 * If at first transmit PHY rate between all pairs of MoCA nodes are greater than or equal to
 * 'mocaIfPhyThreshold', then later transmit PHY rate of one pair of MoCA node is less than
 * 'mocaIfPhyThreshold', send 'mocaTrapBelowPhyThreshold' if this value is 'true',
 * do not send 'mocaTrapBelowPhyThreshold' if this value is 'false'.
 * If at first transmit PHY rate between one or more pairs of MoCA nodes are less than
 * 'mocaIfPhyThreshold', then later transmit PHY rate between all pairs of MoCA nodes
 * are greater than or equal to 'mocaIfPhyThreshold',
 * send 'mocaTrapAbovePhyThreshold' if this value is 'true',
 * do not send 'mocaTrapAbovePhyThreshold' if this value is 'false'.
 *
 * \note
 * This object when read only returns the last PhyThreshold object set.
 * This is the PHY Threshold used in the mocaTrapBelowPhyThreshold and
 * mocaTrapAbovePhyThreshold traps.  Used in conjunction with mocaIfPhyThresholdEnable.
 * This mocaIfPhyThreshold object is referenced in mocaMeshTxRate.
 * Default is 200 Mbps.
 *
 ******************************************************************************/
static SYS_UINT32 GetPhyThreshold(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                  SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 Status = SYS_SUCCESS;

    // Restore this object's value from the FILENAME file.
    RestoreObject(IF_PHY_THRESHOLD, pObjectData);

    return Status;
} // GetPhyThreshold()


/***************************************************************************//**
 * \brief   SetPhyThreshold
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to set value.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Set for PhyThreshold object.
 * Used to set the PhyThreshold for mocaTrapBelowPhyThreshold and mocaTrapAbovePhyThreshold.
 * If at first transmit PHY rate between all pairs of MoCA nodes are greater than or equal to
 * 'mocaIfPhyThreshold', then later transmit PHY rate of one pair of MoCA node is less than
 * 'mocaIfPhyThreshold', send 'mocaTrapBelowPhyThreshold' if this value is 'true',
 * do not send 'mocaTrapBelowPhyThreshold' if this value is 'false'.
 * If at first transmit PHY rate between one or more pairs of MoCA nodes are less than
 * 'mocaIfPhyThreshold', then later transmit PHY rate between all pairs of MoCA nodes
 * are greater than or equal to 'mocaIfPhyThreshold',
 * send 'mocaTrapAbovePhyThreshold' if this value is 'true',
 * do not send 'mocaTrapAbovePhyThreshold' if this value is 'false'.
 *
 * \note
 * This is the PHY Threshold used in the mocaTrapBelowPhyThreshold and
 * mocaTrapAbovePhyThreshold traps.  Used in conjunction with mocaIfPhyThresholdEnable.
 * This mocaIfPhyThreshold object is referenced in mocaMeshTxRate.
 * Control over which node in the MoCA network sets the mocaIfPhyThreshold is done
 * at a higher level.
 * Default is 200 Mbps.
 *
 ******************************************************************************/
static SYS_UINT32 SetPhyThreshold(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                  SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 Status = SYS_SUCCESS;

    // Save this object's value in the FILENAME file.
    SaveObject(IF_PHY_THRESHOLD, pObjectData);

    return Status;
} // SetPhyThreshold()


/***************************************************************************//**
 * \brief   GetPhyThresholdEnable
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for PhyThresholdEnable object.
 * Used to enable mocaTrapBelowPhyThreshold and mocaTrapAbovePhyThreshold.
 * If at first transmit PHY rate between all pairs of MoCA nodes are greater than or equal to
 * 'mocaIfPhyThreshold', then later transmit PHY rate of one pair of MoCA node is less than
 * 'mocaIfPhyThreshold', send 'mocaTrapBelowPhyThreshold' if this value is 'true',
 * do not send 'mocaTrapBelowPhyThreshold' if this value is 'false'.
 * If at first transmit PHY rate between one or more pairs of MoCA nodes are less than
 * 'mocaIfPhyThreshold', then later transmit PHY rate between all pairs of MoCA nodes
 * are greater than or equal to 'mocaIfPhyThreshold',
 * send 'mocaTrapAbovePhyThreshold' if this value is 'true',
 * do not send 'mocaTrapAbovePhyThreshold' if this value is 'false'.
 *
 * \note
 * This object when read only returns the last PhyThresholdEnable object set.
 * This is the PHY Threshold Enable used in the mocaTrapBelowPhyThreshold and
 * mocaTrapAbovePhyThreshold traps.  Used in conjunction with mocaIfPhyThreshold.
 * This mocaIfPhyThresholdEnable object is referenced in mocaMeshTxRate.
 * Default is FALSE.
 *
 ******************************************************************************/
static SYS_UINT32 GetPhyThresholdEnable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                        SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 Status = SYS_SUCCESS;

    // Restore this object's value from the FILENAME file.
    RestoreObject(IF_PHY_THRESHOLD_ENABLE, pObjectData);

    return Status;
} // GetPhyThresholdEnable()


/***************************************************************************//**
 * \brief   SetPhyThresholdEnable
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to set value.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Set for PhyThresholdEnable object.
 * Used to enable mocaTrapBelowPhyThreshold and mocaTrapAbovePhyThreshold.
 * If at first transmit PHY rate between all pairs of MoCA nodes are greater than or equal to
 * 'mocaIfPhyThreshold', then later transmit PHY rate of one pair of MoCA node is less than
 * 'mocaIfPhyThreshold', send 'mocaTrapBelowPhyThreshold' if this value is 'true',
 * do not send 'mocaTrapBelowPhyThreshold' if this value is 'false'.
 * If at first transmit PHY rate between one or more pairs of MoCA nodes are less than
 * 'mocaIfPhyThreshold', then later transmit PHY rate between all pairs of MoCA nodes
 * are greater than or equal to 'mocaIfPhyThreshold',
 * send 'mocaTrapAbovePhyThreshold' if this value is 'true',
 * do not send 'mocaTrapAbovePhyThreshold' if this value is 'false'.
 * Only one node in the MoCA network should set 'mocaIfPhyThresholdEnable' to 'true'.
 *
 * \note
 * When this bit is on, PhyThreshold is enabled.
 * This is the PHY Threshold Enable used in the mocaTrapBelowPhyThreshold and
 * mocaTrapAbovePhyThreshold traps.  Used in conjunction with mocaIfPhyThreshold.
 * This mocaIfPhyThresholdEnable object is referenced in mocaMeshTxRate.
 * Control over which node in the MoCA network sets the mocaIfPhyThresholdEnable is done
 * at a higher level.
 * Default is FALSE.
 *
 ******************************************************************************/
static SYS_UINT32 SetPhyThresholdEnable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                        SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 Status = SYS_SUCCESS;

    // Save this object's value in the FILENAME file.
    SaveObject(IF_PHY_THRESHOLD_ENABLE, pObjectData);

    return Status;
} // SetPhyThresholdEnable()


/*!
 * \brief   Interface Status Table
 *
 * The Interface Status Table, or mocaIfStatusTable, provides status information related to
 * the operation of the managed node. The managed node is identified by the
 * interface index (ifIndex).
 * Status information includes software version, operating channel,
 * Last Operational Frequency (LOF), taboo channel mask, etc.
 */


/***************************************************************************//**
 * \brief   GetStatus
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for Status object.
 * Indicate the current status of the MoCA interface.
 * 'disabled' indicates interface is disabled.
 * If 'disabled', there must exist another local interface (ethernet) where SNMP objects
 * can be accessed.
 * 'noLink' indicates interface is enabled but not part of a network.
 * 'linkUp' indicates interface is enabled and in a MoCA network.
 * It will not be possible to read the 'disabled' status of the MoCA interface if this
 * managed node does not have another local port (e.g. Ethernet) where SNMP objects can be
 * accessed.
 *
 * \note
 * The API clnk_get_my_node_info() returns field LinkStatus in struct ClnkDef_MyNodeInfo_t.
 * If clnk_get_my_node_info() fails then the link is disabled.
 * Otherwise LinkStatus tells you whether the link is UP when TRUE (linkup)
 * or DOWN when FALSE (NoLink).
 * Possible values for Status are:
 *      disabled (1)
 *      noLink   (2)
 *      linkUp   (3)
 *
 ******************************************************************************/
static SYS_UINT32 GetStatus(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                            SYS_UINT32 Index2, void* pObjectData)
{
    SYS_INT32 * const pObjectDataInteger = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    *pObjectDataInteger = LINKSTATUS_DISABLED;
    // get my LinkStatus
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        // failure of clnk_get_my_node_info() implies link disabled (low power mode).
        *pObjectDataInteger = LINKSTATUS_DISABLED;
    }
    else if (MyNodeInfo.LinkStatus)
    {
        *pObjectDataInteger = LINKSTATUS_UP;
    }
    else
    {
        *pObjectDataInteger = LINKSTATUS_NOLINK;
    }
    return Status;
} // GetStatus()


/***************************************************************************//**
 * \brief   GetLinkUpTime
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for LinkUpTime object.
 * Indicate the total number of seconds that this interface is part of a MoCA network.
 *
 * \note
 * The API clnk_get_eth_stats() returns field linkUpTime in struct ClnkDef_EthStats_t.
 *
 ******************************************************************************/
static SYS_UINT32 GetLinkUpTime(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_EthStats_t    MyEthStats;

    *pObjectDataUnsigned = 0;
    // get linkUpTime
    if (clnk_get_eth_stats(pClnkCtx, &MyEthStats, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        *pObjectDataUnsigned = MyEthStats.linkUpTime;
    }
    return Status;
} // GetLinkUpTime()


/***************************************************************************//**
 * \brief   GetSoftwareVersion
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for SoftwareVersion object.
 * Indicate the software version of the MoCA device.
 * This should be the same as the product software version in the MoCA certificate.
 *
 * \note
 * The API clnk_get_my_node_info() returns field SwRevNum in struct ClnkDef_MyNodeInfo_t.
 * This matches what clnkstat reports.
 * This is the same as Driver SW Version in the MoCA certificate since there is
 * no product software version.
 * The possible software version variables are:
 *      SocOpt.mfrSwVer
 *      MyNodeInfo.SwRevNum
 *      EthStats.drvSwRevNum
 *      fware_ver in /home/httpd/version.js
 *
 ******************************************************************************/
static SYS_UINT32 GetSoftwareVersion(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                     SYS_UINT32 Index2, void* pObjectData)
{
    SYS_CHAR * const pObjectDataString = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    pObjectDataString[0] = '\0';
    // get my SwRevNum
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        sprintf(pObjectDataString, "%d.%d.%d.%d",
                (MyNodeInfo.SwRevNum >> 24) & 0x000000FF,
                (MyNodeInfo.SwRevNum >> 16) & 0x000000FF,
                (MyNodeInfo.SwRevNum >>  8) & 0x000000FF,
                (MyNodeInfo.SwRevNum >>  0) & 0x000000FF);
    }
    return Status;
} // GetSoftwareVersion()


/***************************************************************************//**
 * \brief   GetMocaVersion
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for MocaVersion object.
 * Indicate the MoCA version supported by this MoCA interface.
 * Returns moca1dot0 (10 decimal), moca1dot1 (11 decimal), or moca1dot1ProTem (12 decimal).
 *
 * \note
 * The API clnk_get_my_node_info() returns field MocaField in struct ClnkDef_MyNodeInfo_t.
 * The macro GET_MY_MOCA() gets the mocaIfMocaVersion from MocaField and
 * returns two hex nibbles.
 * To get the mocaIfMocaVersion, perform the following calculations:
 *      ((GET_MY_MOCA(MyNodeInfo.MocaField) >> 4) & 0x0f) * 10 +
 *      ((GET_MY_MOCA(MyNodeInfo.MocaField)     ) & 0x0f);
 * Mara does it like this:
 *      #define MOCA_FROM_NPS(_nps) ((moca_version_t)(((_nps) >> 24) & 0xFF))
 *      case CLNK_NMS_PARAM_NODE_MOCA_VERSION:
 *          eview.s.uint32 = MOCA_FROM_NPS(nni.NodeProtocolSupport);
 *          if (!eview.s.uint32) eview.s.uint32 += 0x10;
 *
 ******************************************************************************/
static SYS_UINT32 GetMocaVersion(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                 SYS_UINT32 Index2, void* pObjectData)
{
    SYS_INT32 * const pObjectDataInteger = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    *pObjectDataInteger = 0;
    if (MAX_NUM_NODES == 8)
    {
        *pObjectDataInteger = MOCA_1DOT1_PRO_TEM;
    }
    // get my MocaField
    else if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        *pObjectDataInteger = ((GET_MY_MOCA(MyNodeInfo.MocaField) >> 4) & 0x0f) * 10 +
                              ((GET_MY_MOCA(MyNodeInfo.MocaField)     ) & 0x0f);
    }
    return Status;
} // GetMocaVersion()


/***************************************************************************//**
 * \brief   GetNetworkVersion
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for NetworkVersion object.
 * Indicate the MoCA version used in this MoCA network.  MoCA version reported in the beacon.
 * If this interface is not part of a MoCA network, report 'mocaIfMocaVersion'.
 * This value uses the MOCA_VERSION in the Beacon Frame, but requires determining whether any
 * nodes in the network are MoCA 1.1 ProTem, which distinguishes a MoCA 1.1 network,
 * or 'moca1dot1', from a MoCA 1.1 ProTem network, or 'moca1dot1ProTem'.
 * Returns moca1dot0 (10 decimal), moca1dot1 (11 decimal), or moca1dot1ProTem (12 decimal).
 *
 * \note
 * The API clnk_get_my_node_info() returns field MocaField in struct ClnkDef_MyNodeInfo_t.
 * The macro GET_NET_MOCA() gets the mocaIfNetworkVersion from MocaField and
 * returns two hex nibbles.
 * To get the mocaIfMocaVersion, perform the following calculations:
 *      ((GET_NET_MOCA(MyNodeInfo.MocaField) >> 4) & 0x0f) * 10 +
 *      ((GET_NET_MOCA(MyNodeInfo.MocaField)     ) & 0x0f);
 * Entropic does support the using of mocaIfMocaVersion when this interface is
 * noLink (but fails when interface is disabled).
 *
 ******************************************************************************/
static SYS_UINT32 GetNetworkVersion(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                    SYS_UINT32 Index2, void* pObjectData)
{
    SYS_INT32 * const pObjectDataInteger = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;
    SYS_UINT32 AdvancedMask = 0;

    *pObjectDataInteger = 0;
    // get my NodeId and NetworkNodeBitMask and MocaField
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    // see if noLink
    else if (!MyNodeInfo.LinkStatus)
    {
        // Spec says "If this interface is not part of a MoCA network,
        // report 'mocaIfMocaVersion'."
        if (MAX_NUM_NODES == 8)
        {
            *pObjectDataInteger = MOCA_1DOT1_PRO_TEM;
        }
        // get my MocaField
        else if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
        {
            Status = ~SYS_SUCCESS;
        }
        else
        {
            *pObjectDataInteger = ((GET_MY_MOCA(MyNodeInfo.MocaField) >> 4) & 0x0f) * 10 +
                                  ((GET_MY_MOCA(MyNodeInfo.MocaField)     ) & 0x0f);
        }
    }
    else
    {
        SYS_UINT32 i;
        SYS_UINT32 Mask;
        for (i = 0, Mask = 0x1; i < MAX_NUM_NODES; i++, Mask <<= 1)
        {
            // does node exist?
            if ((MyNodeInfo.NetworkNodeBitMask & (1 << i)) &&
                 MyNodeInfo.NetworkNodeBitMask != 1)
            {
                // get NodeProtocolSupport
                if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, i))
                {
                    printf("clnk_get_network_node_info() failed\n");
                }
                else
                {
                    SYS_UINT32 Nps = MyNetworkNodeInfo.NodeProtocolSupport;
                    SYS_BOOLEAN IsMoca1p1Node =
                        (MOCA_FROM_NPS(Nps) > MOCA_10) ? SYS_TRUE : SYS_FALSE;
                    SYS_BOOLEAN IsMoca1p1ProtemNode =
                        (Nps & COMMITTEE_SUPPORT_FLAG_MASK) == PROTEM11_SUPPORT_FLAG_VAL;
                    SYS_BOOLEAN IsMoca1p1AdvancedNode =
                        IsMoca1p1Node && !IsMoca1p1ProtemNode;
                    if (IsMoca1p1AdvancedNode)
                    {
                        AdvancedMask |= Mask;
                    }
                }
            }
        }

        // do we have any MoCA 1.1 ProTem nodes?
        if (AdvancedMask != MyNodeInfo.NetworkNodeBitMask)
        {
            *pObjectDataInteger = MOCA_1DOT1_PRO_TEM;
        }
        else
        {
            *pObjectDataInteger = ((GET_NET_MOCA(MyNodeInfo.MocaField) >> 4) & 0x0f) * 10 +
                                  ((GET_NET_MOCA(MyNodeInfo.MocaField)     ) & 0x0f);
        }
    }
    return Status;
} // GetNetworkVersion()


/***************************************************************************//**
 * \brief   GetMacAddress
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for MacAddress object.
 * Indicate the MAC address of this MoCA interface.
 * This MAC address is encoded in the first six bytes of the
 * Globally Unique Identifier (GUID).
 * For example, MoCA interface with MAC address aa:bb:cc:dd:ee:ff
 * will have a GUID of aa:bb:cc:dd:ee:ff:00:00.
 *
 * \note
 * The API clnk_get_network_node_info() returns fields GUID64High and GUID64Low in
 * struct ClnkDef_NetNodeInfo_t.
 * The 48 bit mocaIfMacAddress consists of the 32 bits of GUID64High and
 * the 16 MSBs of GUID64Low.
 *
 ******************************************************************************/
static SYS_UINT32 GetMacAddress(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                SYS_UINT32 Index2, void* pObjectData)
{
    SYS_CHAR * const pObjectDataString = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    pObjectDataString[0] = '\0';
    // get my NodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    // get GUID64High and GUID64Low
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, MyNodeInfo.NodeId))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        sprintf(pObjectDataString, "%02x:%02x:%02x:%02x:%02x:%02x",
                (MyNetworkNodeInfo.GUID64High >> 24) & 0x000000FF,
                (MyNetworkNodeInfo.GUID64High >> 16) & 0x000000FF,
                (MyNetworkNodeInfo.GUID64High >>  8) & 0x000000FF,
                (MyNetworkNodeInfo.GUID64High >>  0) & 0x000000FF,
                (MyNetworkNodeInfo.GUID64Low  >> 24) & 0x000000FF,
                (MyNetworkNodeInfo.GUID64Low  >> 16) & 0x000000FF);
    }
    return Status;
} // GetMacAddress()


/***************************************************************************//**
 * \brief   GetNodeID
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for NodeID object.
 * Indicate the node ID of this MoCA interface.
 * If this interface is not part of a MoCA network, report a value of '0'.
 *
 * \note
 * The API clnk_get_my_node_info() returns field NodeId in struct ClnkDef_MyNodeInfo_t.
 *
 ******************************************************************************/
static SYS_UINT32 GetNodeID(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                            SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my NodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        // Spec implies don't fail, just return NodeId of 0.
        *pObjectDataUnsigned = 0;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        *pObjectDataUnsigned = MyNodeInfo.NodeId;
    }
    return Status;
} // GetNodeID()


/***************************************************************************//**
 * \brief   GetName
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for Name object.
 * Indicate the textual name of this MoCA interface.
 * The value of this object should be the name of the interface as
 * assigned by the MoCA device.
 * Same as ifName in IF-MIB.
 * Since MoCA has an Ethernet convergence layer, this name may be 'ethN' where 'N' is
 * the port number.
 *
 * \note
 * The API clnk_get_ifname() returns the interface name string in buf.
 *
 ******************************************************************************/
static SYS_UINT32 GetName(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                          SYS_UINT32 Index2, void* pObjectData)
{
    SYS_CHAR * const pObjectDataString = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;

    pObjectDataString[0] = '\0';
    // get interface name
    if (clnk_get_ifname(pClnkCtx, pObjectDataString))
    {
        Status = ~SYS_SUCCESS;
    }
    return Status;
} // GetName()


/***************************************************************************//**
 * \brief   GetNumNodes
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for NumNodes object.
 * Indicate the number of 1's in the GCD_BITMASK field reported in Type I Probe Reports.
 * This value corresponds to the number of nodes that this node communicates
 * to the MoCA network.
 * This value may be smaller than the number of nodes reported by the NC node.
 *
 * \note
 * NumNodes can be calculated from NetworkNodeBitMask.
 * The API clnk_get_my_node_info() returns field NetworkNodeBitMask in
 * struct ClnkDef_MyNodeInfo_t.
 * Sum the bits in the NetworkNodeBitMask to get the mocaIfNumNodes.
 * Nodes that have a PHY Rate of less than approximately 57 Mbps
 * (N[BAS] < 358 bits per ACMT symbol) between themselves and the NC node are by definition
 * disadvantaged nodes and are not included in the NetworkNodeBitMask.
 * Nodes with a status of disabled are not included in the NetworkNodeBitMask.
 *
 ******************************************************************************/
static SYS_UINT32 GetNumNodes(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                              SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my NetworkNodeBitMask
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        SYS_UINT32 i;
        SYS_UINT32 NumNodes = 0;
        for (i = 0; i < MAX_NUM_NODES; i++)
        {
            // does node exist?
            if ((MyNodeInfo.NetworkNodeBitMask & (1 << i)) &&
                 MyNodeInfo.NetworkNodeBitMask != 1)
            {
                NumNodes++;
            }
        }
        *pObjectDataUnsigned = NumNodes;
    }
    return Status;
} // GetNumNodes()


/***************************************************************************//**
 * \brief   GetNC
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for NC object.
 * Indicate the node ID of the Network Coordinator.
 * If this interface is not part of a MoCA network, report a value of '0'.
 *
 * \note
 * The API clnk_get_my_node_info() returns field CMNodeId in struct ClnkDef_MyNodeInfo_t.
 * Don't do this which is what clnkstat does:
 *      If this node is NC then use BestCMNodeId else use CMNodeId to get the mocaIfNC.
 *
 ******************************************************************************/
static SYS_UINT32 GetNC(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                        SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my CMNodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        // Spec implies don't fail, just return NC of 0.
        *pObjectDataUnsigned = 0;
    }
    else
    {
        *pObjectDataUnsigned = MyNodeInfo.CMNodeId;
    }
    return Status;
} // GetNC()


/***************************************************************************//**
 * \brief   GetBackupNC
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for BackupNC object.
 * Indicate the node ID of the backup Network Coordinator.
 * If this interface is not part of a MoCA network, report a value of '0'.
 *
 * \note
 * The API clnk_get_my_node_info() returns field BackupCMNodeId in
 * struct ClnkDef_MyNodeInfo_t.
 *
 ******************************************************************************/
static SYS_UINT32 GetBackupNC(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                              SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my BackupCMNodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        // Spec implies don't fail, just return BackupNC of 0.
        *pObjectDataUnsigned = 0;
    }
    else
    {
        *pObjectDataUnsigned = MyNodeInfo.BackupCMNodeId;
    }
    return Status;
} // GetBackupNC()


/***************************************************************************//**
 * \brief   GetRFChannel
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for RFChannel object.
 * Indicate the MoCA channel in MHz this interface is tuned to when part of a MoCA network.
 * When not part of a MoCA network this value may not reflect the actual tuned channel.
 * If 'mocaIfEnable' is 'false', report a value of 'unknown' (0).
 *
 * \note
 * The API clnk_get_my_node_info() returns field RFChanFreq in struct ClnkDef_MyNodeInfo_t.
 * Divide by 1000000 to get MHz.
 *
 ******************************************************************************/
static SYS_UINT32 GetRFChannel(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                               SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    GetFunc_t GetFunc = ObjectInfoTable[IF_ENABLE].GetFunc;

    *pObjectDataUnsigned = 0;
    g_ObjectVar.Bool = SYS_TRUE;

    // Do Get for mocaIfEnable.  If FALSE then we return 0 for Get of mocaIfRFChannel.
    if (GetFunc)
    {
        Index1 = 0;
        Index2 = 0;
        if (GetFunc(pClnkCtx, Index1, Index2, &g_ObjectVar.Bool) != SYS_SUCCESS)
        {
            printf("Get failed, ObjectIndex=%d\n", IF_ENABLE);
            exit(1);
        }
    }

    // is Get of mocaIfEnable = FALSE?
    if (!g_ObjectVar.Bool)
    {
        *pObjectDataUnsigned = 0;
    }
    // get my RFChanFreq
    else if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        // Spec implies don't fail, just return NodeId of 0.
        *pObjectDataUnsigned = 0;
    }
    else
    {
        // convert RFChanFreq to MHz
        *pObjectDataUnsigned = MyNodeInfo.RFChanFreq / 1000000;
    }
    return Status;
} // GetRFChannel()


/***************************************************************************//**
 * \brief   GetLOF
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for LOF object.
 * Indicate the MoCA channel in MHz this interface was tuned to when it was last in
 * the linkUp state.
 * If this interface is never part of a MoCA network, report the
 * factory default Last Operational Frequency (LOF).
 *
 * \note
 * The API clnk_get_lof() returns LOF.  Divide by 1000000 to get MHz.
 * If this interface is never part of a MoCA network and there is no factory default LOF,
 * then a LOF value of 0xFFFFFFFF is used.
 * Remember to not divide 0xFFFFFFFF by 1000000 to convert to Mbps.
 *
 ******************************************************************************/
static SYS_UINT32 GetLOF(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                         SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 Lof;

    *pObjectDataUnsigned = 0;
    // get my RFChanFreq
    if (!clnk_get_lof(pClnkCtx, &Lof))
    {
        // interface is not part of a MoCA network
        BridgeSetupStruct Setup;
        Setup.lof = 0xFFFFFFFF;
        // Spec implies don't fail, just return Lof value from
        // factory default file /etc/clink.backup.
        // If factory default file does not exist, then use Lof=0xFFFFFFFF.
        clnk_read_cfg_file("", &Setup, 3);
        // convert factory default Setup.lof to MHz
        if (Setup.lof == 0xFFFFFFFF)
        {
            *pObjectDataUnsigned = Setup.lof;
        }
        else
        {
            *pObjectDataUnsigned = Setup.lof / 1000000;
        }
    }
    else
    {
        // convert Lof to MHz
        if (Lof == 0xFFFFFFFF)
        {
            *pObjectDataUnsigned = Lof;
        }
        else
        {
            *pObjectDataUnsigned = Lof / 1000000;
        }
    }
    return Status;
} // GetLOF()


/***************************************************************************//**
 * \brief   GetTabooChannelMask
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for TabooChannelMask object.
 * Indicate the list of taboo channels in this MoCA network represented as a bitmask.
 * This value is derived from TABOO_MASK_START and TABOO_CHANNEL_MASK in the beacon,
 * but has a different data representation.
 * e.g. If taboo channels consists of 1300, 1350 and 1400 MHz, TABOO_MASK_START is 52,
 * TABOO_CHANNEL_MASK is 'A8000000'h, and 'mocaIfTabooChannelMask' is '01500000'h.
 * Note that in 'mocaIfTabooChannelMask', the lowest represented frequency of 800 MHz is
 * represented in bit 0, and increasing bit position represents increasingly
 * higher frequency.
 * While in TABOO_CHANNEL_MASK, the taboo channel with the lowest frequency is represented
 * in bit 31, and decreasing bit position represents increasingly higher frequency.
 * If this interface is not sending or receiving beacon, or there is no taboo channel in
 * this MoCA network, report a value of '0'.
 *
 * \note
 * The API clnk_get_network_node_info() returns field TabooChanMask in
 * struct ClnkDef_MyNodeInfo_t.
 * This is the taboo channel mask copied from the beacon.
 * See gcap09.
 *
 ******************************************************************************/
static SYS_UINT32 GetTabooChannelMask(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                      SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my TabooChanMask
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        // Spec implies don't fail, just return TabooChannelMask of 0.
        *pObjectDataUnsigned = 0;
    }
    else
    {
        *pObjectDataUnsigned = MyNodeInfo.TabooChanMask;
    }
    return Status;
} // GetTabooChannelMask()


/***************************************************************************//**
 * \brief   GetNodeTabooChannelMask
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for NodeTabooChannelMask object.
 * Indicate the list of taboo channels for this MoCA node as reported in the
 * TABOO_MASK_START and TABOO_CHANNEL_MASK fields in this node's Admission Request frame.
 *
 * \note
 * The API clnk_get_soc_opt() returns field tabooMask in struct clnk_soc_opt.
 *
 ******************************************************************************/
static SYS_UINT32 GetNodeTabooChannelMask(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                          SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    struct clnk_soc_opt SocOpt;

    *pObjectDataUnsigned = 0;
    // get tabooMask
    if (clnk_get_soc_opt(pClnkCtx, &SocOpt))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        *pObjectDataUnsigned = SocOpt.tabooMask;
    }
    return Status;
} // GetNodeTabooChannelMask()


/***************************************************************************//**
 * \brief   GetCapabilityMask
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for CapabilityMask object.
 * Indicate the list of RF frequency channels that this device can support represented by
 * a bitmask.
 *
 * \note
 * The API clnk_get_soc_opt() returns field productMask in struct clnk_soc_opt.
 *
 ******************************************************************************/
static SYS_UINT32 GetCapabilityMask(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                    SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    struct clnk_soc_opt SocOpt;

    *pObjectDataUnsigned = 0;
    // get productMask
    if (clnk_get_soc_opt(pClnkCtx, &SocOpt))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        *pObjectDataUnsigned = SocOpt.productMask;
    }
    return Status;
} // GetCapabilityMask()


/***************************************************************************//**
 * \brief   GetQAM256Capable
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for QAM256Capable object.
 * Indicate this MoCA node supports QAM256 if the value is 'true'.
 * Indicate this MoCA node does not support QAM256 if this value is 'false'.
 *
 * \note
 * The API clnk_get_network_node_info() returns field
 * NodeProtocolSupport (bit 4 for QAM256Capable) in struct ClnkDef_NetNodeInfo_t.
 *
 ******************************************************************************/
static SYS_UINT32 GetQAM256Capable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                   SYS_UINT32 Index2, void* pObjectData)
{
    SYS_BOOLEAN * const pObjectDataBoolean = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    *pObjectDataBoolean = SYS_FALSE;
    // get my NodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    // get NodeProtocolSupport
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, MyNodeInfo.NodeId))
    {
        Status = ~SYS_SUCCESS;
    }
    else if (MyNetworkNodeInfo.NodeProtocolSupport & QAM256_SUPPORT_FLAG)
    {
        *pObjectDataBoolean = SYS_TRUE;
    }
    else
    {
        *pObjectDataBoolean = SYS_FALSE;
    }
    return Status;
} // GetQAM256Capable()


/***************************************************************************//**
 * \brief   GetPacketsAggrCapability
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for PacketsAggrCapability object.
 * Represent the maximum number of Ethernet packets aggregated in a MoCA frame that
 * this MoCA interface supports at transmit/receive.
 * 'none' (0) represents packet aggregation is not supported, i.e. MoCA 1.0 or
 *            MoCA 1.1 ProTem devices.
 * 'aggr6' (6) represents maximum aggregation of 6 packets.
 * 'aggr10' (10) represents maximum aggregation of 10 packets.
 *
 * \note
 * The API clnk_get_network_node_info() returns field NodeProtocolSupport in
 * struct ClnkDef_NetNodeInfo_t.  Bits 8&7=10 (binary) for aggr10 (10),
 * bits 8&7=00 (binary) for aggr6 (6), otherwise none (0).
 *
 ******************************************************************************/
static SYS_UINT32 GetPacketsAggrCapability(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                           SYS_UINT32 Index2, void* pObjectData)
{
    SYS_INT32 * const pObjectDataInteger = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    *pObjectDataInteger = AGGR0;
    // get my NodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    // get NodeProtocolSupport
    else
    {
        SYS_INT32 RetryCount = 0;
        SYS_INT32 RetErr;

        // make sure we have a good NodeProtocolSupport in MyNetworkNodeInfo
        do
        {
            RetErr = clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, MyNodeInfo.NodeId);
            usleep(100000);     // units of usec
            RetryCount++;
        } while ((!MyNetworkNodeInfo.IsValid || RetErr != 0) && RetryCount < 50);

        if (RetryCount == 50)
        {
            Status = ~SYS_SUCCESS;
        }
        else
        {
            SYS_UINT32 Nps = MyNetworkNodeInfo.NodeProtocolSupport;
            SYS_BOOLEAN IsMoca1p1Node = (MOCA_FROM_NPS(Nps) > MOCA_10) ? SYS_TRUE : SYS_FALSE;
            // bit 8 on and bit 7 off?
            if ((MyNetworkNodeInfo.NodeProtocolSupport & COMMITTEE_SUPPORT_FLAG_MASK) ==
                AGGR10_SUPPORT_FLAG_VAL)
            {
                *pObjectDataInteger = AGGR10;
            }
            // bit 8 off and bit 7 off?
            else if ((MyNetworkNodeInfo.NodeProtocolSupport & COMMITTEE_SUPPORT_FLAG_MASK) ==
                     AGGR6_SUPPORT_FLAG_VAL)
            {
                // check if it is a MoCA 1.1 node
                if (IsMoca1p1Node)
                {
                    *pObjectDataInteger = AGGR6;
                }
                else
                {
                    *pObjectDataInteger = AGGR0;
                }
            }
            // bit 8 off and bit 7 on?
            // or
            // bit 8 on and bit 7 on?
            else
            {
                *pObjectDataInteger = AGGR0;
            }
        }
    }
    return Status;
} // GetPacketsAggrCapability()


/***************************************************************************//**
 * \brief   GetTxGcdRate
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for TxGcdRate object.
 * Indicate the PHY rate in Mbps for the transmit traffic broadcast from this node.
 *
 * \note
 * The API clnk_get_network_node_info() returns field GCDTxBitRate in
 * struct ClnkDef_NetNodeInfo_t.
 * Divide by 1,000,000 to get units of Mbps.
 *
 ******************************************************************************/
static SYS_UINT32 GetTxGcdRate(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                               SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 NodeIndex = Index1;  // Index1 starts at 0 when representing NodeIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my NetworkNodeBitMask
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        SYS_UINT32 i;
        for (i = 0; i < MAX_NUM_NODES; i++)
        {
            // can't be my NodeId
            if (i != MyNodeInfo.NodeId)
            {
                // does node exist?
                if ((MyNodeInfo.NetworkNodeBitMask & (1 << i)) &&
                     MyNodeInfo.NetworkNodeBitMask != 1)
                {
                    // get GCDTxBitRate for node i
                    if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, i))
                    {
                        printf("clnk_get_network_node_info() failed\n");
                    }
                    else
                    {
                        *pObjectDataUnsigned = MyNetworkNodeInfo.GCDTxBitRate / 1000000;
                        break;
                    }
                }
            }
        }
    }
    return Status;
} // GetTxGcdRate()


/*!
 * \brief   Interface Statistics Table
 *
 * The Interface Statistics Table, or mocaIfStatsTable, provides statistics information on
 * counters for transmission and reception of packets, and
 * Parameterized Quality of Service (PQoS) flows in a managed node.
 * The managed node is identified by the interface index (ifIndex).
 */


/***************************************************************************//**
 * \brief   GetTxPackets
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for TxPackets object.
 * Indicate the number of Ethernet packets transmitted by this MoCA interface.
 * Provided in the IF MIB ifOutUcastPkts.
 *
 * \note
 * The API clnk_get_my_node_info() returns field Stats.NumOfAsyncTx in
 * struct ClnkDef_MyNodeInfo_t.
 *
 ******************************************************************************/
static SYS_UINT32 GetTxPackets(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                               SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my Stats.NumOfAsyncTx
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        *pObjectDataUnsigned = MyNodeInfo.Stats.NumOfAsyncTx;
    }
    return Status;
} // GetTxPackets()


/***************************************************************************//**
 * \brief   GetTxDrops
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for TxDrops object.
 * Indicate the number of transmitted Ethernet packets dropped by this MoCA interface.
 * Provided in the IF MIB ifOutDiscards.
 *
 * \note
 * The API clnk_get_my_node_info() returns field Stats.NumOfAsyncTxErr in
 * struct ClnkDef_MyNodeInfo_t.
 * You can think of this error as late scheduling error or time too late error or
 * unable to Tx in time in hardware error or ECL dropped error.
 * These are the same between PCIe and xMII.
 *
 ******************************************************************************/
static SYS_UINT32 GetTxDrops(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                             SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my Stats.NumOfAsyncTxErr
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        *pObjectDataUnsigned = MyNodeInfo.Stats.NumOfAsyncTxErr;
    }
    return Status;
} // GetTxDrops()


/***************************************************************************//**
 * \brief   GetRxPackets
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for RxPackets object.
 * Indicate the number of good Ethernet packets received by this MoCA interface.
 * Provided in the IF MIB ifInUcastPkts.
 *
 * \note
 * The API clnk_get_my_node_info() returns field Stats.NumOfAsyncRx in
 * struct ClnkDef_MyNodeInfo_t.
 *
 ******************************************************************************/
static SYS_UINT32 GetRxPackets(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                               SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my Stats.NumOfAsyncRx
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        *pObjectDataUnsigned = MyNodeInfo.Stats.NumOfAsyncRx;
    }
    return Status;
} // GetRxPackets()


/***************************************************************************//**
 * \brief   GetRxCorrectedErrors
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for RxCorrectedErrors object.
 * Indicate the number of received Ethernet packets that have errors and are corrected
 * by this MoCA interface.
 * Provided in the IF MIB ifInErrors.
 *
 * \note
 * RS not performed on an Ethernet packet basis.
 * RS correction is only performed on a 192 byte RS block size basis.
 * Customers really just wanted to monitor corrected errors so RS corrections performed on
 * a 192 byte RS block size basis is ok.
 * The closest statistic is RSTotalCorrBytes.
 * The API clnk_get_node_phy_data(NodeIdInfo) passing options Rx and P2P returns
 * field RSTotalCorrBytes in struct ClnkDef_PhyData_t.
 * This represents the Reed-Solomon total corrected bytes.
 * The API clnk_get_node_phy_data(NodeIdInfo) accepts a special NodeIdInfo format:
 * NodeIdInfo   Note that the 25 bits in NodeID is encoded as follows:
 *              Bit[24..16] = ChannelType
 *              0 - P2P
 *              1 - GCD
 *              Bit[15..8] = Transmit/Receive (RxOn)
 *              0 - TX
 *              1 - RX
 *              Bit[7..0] = NodeId
 *
 ******************************************************************************/
static SYS_UINT32 GetRxCorrectedErrors(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                       SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_PhyData_t     RxPhyDataInfo;
    SYS_UINT32 RxOn = 1;            // Tx=0, Rx=1
    SYS_UINT32 ChannelType = 0;     // 0=P2P, 1=GCD

    *pObjectDataUnsigned = 0;
    // get my NodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    // get RSTotalCorrBytes
    else if (clnk_get_node_phy_data(pClnkCtx, &RxPhyDataInfo,
                                    MyNodeInfo.NodeId | (RxOn << 8) | (ChannelType << 16)))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        *pObjectDataUnsigned = RxPhyDataInfo.RSTotalCorrBytes;
    }
    return Status;
} // GetRxCorrectedErrors()


/***************************************************************************//**
 * \brief   GetRxDrops
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for RxDrops object.
 * Indicate the number of scheduled MoCA frames that are not detected or
 * have uncorrectable errors and are dropped by this MoCA interface.
 *
 * \note
 * The API clnk_get_my_node_info() returns field Stats.NumOfAsyncRxDropped in
 * struct ClnkDef_MyNodeInfo_t.
 *
 ******************************************************************************/
static SYS_UINT32 GetRxDrops(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                             SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my Stats.NumOfAsyncRxDropped
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        *pObjectDataUnsigned = MyNodeInfo.Stats.NumOfAsyncRxDropped;
    }
    return Status;
} // GetRxDrops()


/***************************************************************************//**
 * \brief   GetEgressNodeNumFlows
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for EgressNodeNumFlows object.
 * Indicate the number of PQoS flows in which this MoCA interface is an egress node for
 * these PQoS flows.
 *
 * \note
 * The API clnk_get_my_node_info() returns field NodeId in struct ClnkDef_MyNodeInfo_t.
 * The API clnk_get_network_node_info() passes NodeId and returns fields GUID64High and
 * GUID64Low in struct ClnkDef_NetNodeInfo_t.
 * The API clnk_query_nodes() returns field number_nodes and nodes[NodeIndex].hi and
 * nodes[NodeIndex].lo in struct qos_node_response_t.
 * For each NodeNumber in number_nodes, loop.
 *     The API clnk_list_ingress_flows() passes ingress_guid.hi and ingress_guid.lo and
 *     start_index and limit in struct qos_f_l_t and returns
 *     returned_flows and an array of flow_id's of type flow_id_t in
 *     struct qos_f_l_response_t.
 *     For each FlowIndex in returned_flows, loop.
 *         The API clnk_query_ingress_flow() passes flow_id and returns
 *         flow_desc.egress_guid.hi and flow_desc.egress_guid.lo in
 *         struct qos_q_f_response_t.
 *         If the flow_desc.egress_guid matches your GUID then increment the
 *         MyEgressGuidFlowCount.
 * The MyEgressGuidFlowCount is the EgressNodeNumFlows.
 * You can do the same in mara but you have to count them up, but info is there.
 *      mara qqueryall | grep "EgressGuid : $ThisNodeGuid" | wc -l
 *
 ******************************************************************************/
static SYS_UINT32 GetEgressNodeNumFlows(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                        SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my NodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    // get GUID64High and GUID64Low
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, MyNodeInfo.NodeId))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        flow_id_t MyGuid;
        SYS_UINT32 MyEgressGuidFlowCount = 0;
        qos_node_response_t QueryNodeResponse;

        MyGuid.hi = MyNetworkNodeInfo.GUID64High;
        MyGuid.lo = MyNetworkNodeInfo.GUID64Low;

        // get number_nodes and nodes[NodeIndex]
        if (clnk_query_nodes(pClnkCtx, &QueryNodeResponse))
        {
            Status = ~SYS_SUCCESS;
        }
        else if (QueryNodeResponse.qos_error == QOS_DRIVER_ERROR)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (QueryNodeResponse.qos_error == QOS_TIMEOUT)
        {
            printf("clnk_query_nodes() QOS_TIMEOUT\n");
            exit(1);
        }
        else if (QueryNodeResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
        {
            printf("IngressGuid=%02x:%02x:%02x:%02x:%02x:%02x not PQOS\n",
                    (MyNetworkNodeInfo.GUID64High >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >> 16) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  8) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  0) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 16) & 0x000000FF);
            exit(1);
        }
        else if (QueryNodeResponse.qos_error != QOS_SUCCESS)
        {
            Status = ~SYS_SUCCESS;
        }
        else
        {
            SYS_UINT32 NodeIndex;
            for (NodeIndex = 0; NodeIndex < QueryNodeResponse.number_nodes; NodeIndex++)
            {
                qos_f_l_t          ListIngressFlowsRequest;
                qos_f_l_response_t ListIngressFlowsResponse;

                ListIngressFlowsRequest.ingress_guid.hi =
                    QueryNodeResponse.nodes[NodeIndex].hi;
                ListIngressFlowsRequest.ingress_guid.lo =
                    QueryNodeResponse.nodes[NodeIndex].lo;
                ListIngressFlowsRequest.start_index     = 0;
                ListIngressFlowsRequest.limit           = QOS_MAX_FLOWS;

                // get returned_flows, total_flows and flows[0..returned_flows-1]
                if (clnk_list_ingress_flows(pClnkCtx, &ListIngressFlowsRequest,
                                            &ListIngressFlowsResponse))
                {
                    printf("clnk_list_ingress_flows() failed\n");
                }
                else if (ListIngressFlowsResponse.qos_error == QOS_DRIVER_ERROR)
                {
                    printf("clnk_list_ingress_flows() qos_error == QOS_DRIVER_ERROR\n");
                }
                else if (ListIngressFlowsResponse.qos_error == QOS_TIMEOUT)
                {
                    printf("clnk_list_ingress_flows() qos_error == QOS_TIMEOUT\n");
                }
                // Skip the node if it isn't PQoS capable
                else if (ListIngressFlowsResponse.qos_error ==
                         QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
                {
                    continue;
                }
                else if (ListIngressFlowsResponse.qos_error != QOS_SUCCESS)
                {
                    printf("clnk_list_ingress_flows() qos_error != QOS_SUCCESS\n");
                }
                else if (ListIngressFlowsResponse.total_flows > QOS_MAX_FLOWS)
                {
                    printf("clnk_list_ingress_flows() total_flows > QOS_MAX_FLOWS\n");
                }
                else if (ListIngressFlowsResponse.returned_flows > QOS_MAX_FLOWS)
                {
                    printf("clnk_list_ingress_flows() returned_flows > QOS_MAX_FLOWS\n");
                }
                else
                {
                    SYS_UINT32 FlowIndex;
                    for (FlowIndex = 0; FlowIndex < ListIngressFlowsResponse.returned_flows;
                         FlowIndex++)
                    {
                        flow_name_t         QueryIngressFlowRequest;
                        qos_q_f_response_t  QueryIngressFlowResponse;

                        QueryIngressFlowRequest.flow_id =
                            ListIngressFlowsResponse.flows[FlowIndex];

                        // get egress_guid
                        if (clnk_query_ingress_flow(pClnkCtx, &QueryIngressFlowRequest,
                                                    &QueryIngressFlowResponse))
                        {
                            printf("clnk_query_ingress_flow() failed\n");
                        }
                        else if (QueryIngressFlowResponse.qos_error == QOS_DRIVER_ERROR)
                        {
                            printf("clnk_query_ingress_flow() qos_error==QOS_DRIVER_ERROR\n");
                        }
                        else if (QueryIngressFlowResponse.qos_error == QOS_TIMEOUT)
                        {
                            printf("clnk_query_ingress_flow() qos_error == QOS_TIMEOUT\n");
                        }
                        else if (QueryIngressFlowResponse.qos_error ==
                                 QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
                        {
                            continue;
                        }
                        else if (QueryIngressFlowResponse.qos_error != QOS_SUCCESS)
                        {
                            printf("clnk_query_ingress_flow() qos_error != QOS_SUCCESS\n");
                        }
                        else
                        {
                            if (QueryIngressFlowResponse.flow_desc.egress_guid.hi == MyGuid.hi
                               &&
                               QueryIngressFlowResponse.flow_desc.egress_guid.lo == MyGuid.lo)
                            {
                                MyEgressGuidFlowCount++;
                            }
                        }
                    }
                }
            }
        }

        *pObjectDataUnsigned = MyEgressGuidFlowCount;
    }
    return Status;
} // GetEgressNodeNumFlows()


/***************************************************************************//**
 * \brief   GetIngressNodeNumFlows
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for IngressNodeNumFlows object.
 * Indicate the number of PQoS flows in which this MoCA interface is an ingress node for
 * these PQoS flows.
 *
 * \note
 * The API clnk_get_my_node_info() returns field NodeId in struct ClnkDef_MyNodeInfo_t.
 * The API clnk_get_network_node_info() passes NodeId and returns fields GUID64High and
 * GUID64Low in struct ClnkDef_NetNodeInfo_t.
 * The API clnk_list_ingress_flows() passes ingress_guid.hi and ingress_guid.lo and
 * start_index and limit in struct qos_f_l_t and returns
 * returned_flows in struct qos_f_l_response_t.
 * The returned_flows is the IngressNodeNumFlows.
 * You can do the same in mara.
 *      mara qlist  ig=$ThisNodeGuid in field returned_flows.
 *
 ******************************************************************************/
static SYS_UINT32 GetIngressNodeNumFlows(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                         SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my NodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    // get GUID64High and GUID64Low
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, MyNodeInfo.NodeId))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        qos_f_l_t          ListIngressFlowsRequest;
        qos_f_l_response_t ListIngressFlowsResponse;

        ListIngressFlowsRequest.ingress_guid.hi = MyNetworkNodeInfo.GUID64High;
        ListIngressFlowsRequest.ingress_guid.lo = MyNetworkNodeInfo.GUID64Low;
        ListIngressFlowsRequest.start_index     = 0;
        ListIngressFlowsRequest.limit           = QOS_MAX_FLOWS;

        // get returned_flows, total_flows and flows[0..returned_flows-1]
        if (clnk_list_ingress_flows(pClnkCtx, &ListIngressFlowsRequest,
                                    &ListIngressFlowsResponse))
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_DRIVER_ERROR)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_TIMEOUT)
        {
            printf("clnk_list_ingress_flows() QOS_TIMEOUT\n");
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
        {
            printf("IngressGuid=%02x:%02x:%02x:%02x:%02x:%02x not PQOS\n",
                    (MyNetworkNodeInfo.GUID64High >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >> 16) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  8) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  0) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 16) & 0x000000FF);
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error != QOS_SUCCESS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.total_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.returned_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else
        {
            *pObjectDataUnsigned = ListIngressFlowsResponse.returned_flows;
        }
    }
    return Status;
} // GetIngressNodeNumFlows()


/*!
 * \brief   Interface Flow Statistics Table
 *
 * The Interface Flow Statistics Table, or mocaIfFlowStatsTable, provides statistics
 * information for each Parameterized Quality of Service (PQoS) flow in which a managed node
 * is the ingress node.  This table is indexed by the interface index (ifIndex) and the
 * PQoS flow index (mocaIfFlowIndex).
 * This flow index is an arbitrary number assigned by the managed node.
 * For each MoCA interface, there is one entry for each PQoS flow in this table.
 * The "mocaIfFlowNumTx" is a proposed object that is not part of the MIB definition and
 * is not assigned an object identifier in the following table.
 */


/***************************************************************************//**
 * \brief   GetFlowID
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      FlowIndex a.k.a. mocaIfFlowIndex.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for FlowID object.
 * Multicast Ethernet address that identifies a PQoS flow.
 * Indicate the flow ID of a PQoS flow.
 * The flow ID is a valid multicast Ethernet MAC address that uniquely identifies a
 * PQoS flow in a MoCA Network.
 * The recommended rules for generation of the flow ID are specified in MoCA Spec 1.1.
 *
 * \note
 * The API clnk_get_my_node_info() returns field NodeId in struct ClnkDef_MyNodeInfo_t.
 * The API clnk_get_network_node_info() passes NodeId and returns fields GUID64High and
 * GUID64Low in struct ClnkDef_NetNodeInfo_t.
 * The API clnk_list_ingress_flows() passes ingress_guid.hi and ingress_guid.lo and
 * start_index and limit in struct qos_f_l_t and returns
 * returned_flows and an array of flows[0..returned_flows-1] each of type flow_id_t in
 * struct qos_f_l_response_t.
 * The flows[FlowIndex] is the FlowID.
 * The array is indexed by mocaIfFlowIndex.
 * The mocaIfFlowID is a multicast Ethernet MAC address.
 * Not to be confused with mocaIfFlowIndex.
 *
 ******************************************************************************/
static SYS_UINT32 GetFlowID(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                            SYS_UINT32 Index2, void* pObjectData)
{
    SYS_CHAR * const pObjectDataString = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 FlowIndex = Index1 - 1;  // Index1 starts at 1 when representing FlowIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    pObjectDataString[0] = '\0';
    // get my NodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    // does node not exist in network?
    else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << MyNodeInfo.NodeId)) &&
                MyNodeInfo.NetworkNodeBitMask != 1))
    {
        printf("NodeId=%d does not exist in network.\n", MyNodeInfo.NodeId);
        exit(1);
    }
    // get GUID64High and GUID64Low
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, MyNodeInfo.NodeId))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        qos_f_l_t          ListIngressFlowsRequest;
        qos_f_l_response_t ListIngressFlowsResponse;

        ListIngressFlowsRequest.ingress_guid.hi = MyNetworkNodeInfo.GUID64High;
        ListIngressFlowsRequest.ingress_guid.lo = MyNetworkNodeInfo.GUID64Low;
        ListIngressFlowsRequest.start_index     = 0;
        ListIngressFlowsRequest.limit           = QOS_MAX_FLOWS;

        // get returned_flows, total_flows and flows[0..returned_flows-1]
        if (clnk_list_ingress_flows(pClnkCtx, &ListIngressFlowsRequest,
                                    &ListIngressFlowsResponse))
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_DRIVER_ERROR)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_TIMEOUT)
        {
            printf("clnk_list_ingress_flows() QOS_TIMEOUT\n");
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
        {
            printf("IngressGuid=%02x:%02x:%02x:%02x:%02x:%02x not PQOS\n",
                    (MyNetworkNodeInfo.GUID64High >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >> 16) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  8) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  0) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 16) & 0x000000FF);
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error != QOS_SUCCESS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.total_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.returned_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (FlowIndex >= ListIngressFlowsResponse.returned_flows)
        {
            printf("FlowIndex=%d does not exist.\n", FlowIndex);
            exit(1);
        }
        else
        {
            sprintf(pObjectDataString, "%02x:%02x:%02x:%02x:%02x:%02x",
                    (ListIngressFlowsResponse.flows[FlowIndex].hi >> 24) & 0x000000FF,
                    (ListIngressFlowsResponse.flows[FlowIndex].hi >> 16) & 0x000000FF,
                    (ListIngressFlowsResponse.flows[FlowIndex].hi >>  8) & 0x000000FF,
                    (ListIngressFlowsResponse.flows[FlowIndex].hi >>  0) & 0x000000FF,
                    (ListIngressFlowsResponse.flows[FlowIndex].lo >> 24) & 0x000000FF,
                    (ListIngressFlowsResponse.flows[FlowIndex].lo >> 16) & 0x000000FF);
        }
    }
    return Status;
} // GetFlowID()


/***************************************************************************//**
 * \brief   GetPacketDA
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      FlowIndex a.k.a. mocaIfFlowIndex.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for PacketDA object.
 * Multicast Ethernet address that identifies a PQoS flow.
 * Indicate the flow ID of a PQoS flow.
 * The flow ID is a valid multicast Ethernet MAC address that uniquely identifies a
 * PQoS flow in a MoCA Network.
 * The recommended rules for generation of the flow ID are specified in MoCA Spec 1.1.
 *
 * \note
 * The API clnk_get_my_node_info() returns field NodeId in struct ClnkDef_MyNodeInfo_t.
 * The API clnk_get_network_node_info() passes NodeId and returns fields GUID64High and
 * GUID64Low in struct ClnkDef_NetNodeInfo_t.
 * The API clnk_list_ingress_flows() passes ingress_guid.hi and ingress_guid.lo and
 * start_index and limit in struct qos_f_l_t and returns
 * returned_flows and an array of flows[0..returned_flows-1] each of type flow_id_t in
 * struct qos_f_l_response_t.
 * The flows[FlowIndex] is the FlowID.
 * The array is indexed by mocaIfFlowIndex.
 * The mocaIfFlowID is a multicast Ethernet MAC address.
 * Not to be confused with mocaIfFlowIndex.
 * The API clnk_query_ingress_flow() passes flow_id (hi&lo) and returns
 * flow_desc.packet_da (hi&lo) of type flow_id_t in
 * struct qos_q_f_response_t.
 *
 ******************************************************************************/
static SYS_UINT32 GetPacketDA(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                              SYS_UINT32 Index2, void* pObjectData)
{
    SYS_CHAR * const pObjectDataString = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 FlowIndex = Index1 - 1;  // Index1 starts at 1 when representing FlowIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    pObjectDataString[0] = '\0';
    // get my NodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    // does node not exist in network?
    else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << MyNodeInfo.NodeId)) &&
                MyNodeInfo.NetworkNodeBitMask != 1))
    {
        printf("NodeId=%d does not exist in network.\n", MyNodeInfo.NodeId);
        exit(1);
    }
    // get GUID64High and GUID64Low
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, MyNodeInfo.NodeId))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        qos_f_l_t          ListIngressFlowsRequest;
        qos_f_l_response_t ListIngressFlowsResponse;

        ListIngressFlowsRequest.ingress_guid.hi = MyNetworkNodeInfo.GUID64High;
        ListIngressFlowsRequest.ingress_guid.lo = MyNetworkNodeInfo.GUID64Low;
        ListIngressFlowsRequest.start_index     = 0;
        ListIngressFlowsRequest.limit           = QOS_MAX_FLOWS;

        // get returned_flows, total_flows and flows[0..returned_flows-1]
        if (clnk_list_ingress_flows(pClnkCtx, &ListIngressFlowsRequest,
                                    &ListIngressFlowsResponse))
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_DRIVER_ERROR)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_TIMEOUT)
        {
            printf("clnk_list_ingress_flows() QOS_TIMEOUT\n");
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
        {
            printf("IngressGuid=%02x:%02x:%02x:%02x:%02x:%02x not PQOS\n",
                    (MyNetworkNodeInfo.GUID64High >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >> 16) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  8) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  0) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 16) & 0x000000FF);
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error != QOS_SUCCESS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.total_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.returned_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (FlowIndex >= ListIngressFlowsResponse.returned_flows)
        {
            printf("FlowIndex=%d does not exist.\n", FlowIndex);
            exit(1);
        }
        else
        {
            flow_name_t         QueryIngressFlowRequest;
            qos_q_f_response_t  QueryIngressFlowResponse;

            QueryIngressFlowRequest.flow_id = ListIngressFlowsResponse.flows[FlowIndex];

            // get packet_da
            if (clnk_query_ingress_flow(pClnkCtx, &QueryIngressFlowRequest,
                                        &QueryIngressFlowResponse))
            {
                Status = ~SYS_SUCCESS;
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_DRIVER_ERROR)
            {
                Status = ~SYS_SUCCESS;
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_TIMEOUT)
            {
                printf("clnk_query_ingress_flow() QOS_TIMEOUT\n");
                exit(1);
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
            {
                printf("flow_id=%02x:%02x:%02x:%02x:%02x:%02x not PQOS\n",
                        (QueryIngressFlowRequest.flow_id.hi >> 24) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >> 16) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >>  8) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >>  0) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.lo  >> 24) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.lo  >> 16) & 0x000000FF);
                exit(1);
            }
            else if (QueryIngressFlowResponse.qos_error != QOS_SUCCESS)
            {
                Status = ~SYS_SUCCESS;
            }
            else
            {
                sprintf(pObjectDataString, "%02x:%02x:%02x:%02x:%02x:%02x",
                       (QueryIngressFlowResponse.flow_desc.packet_da.hi >> 24) & 0x000000FF,
                       (QueryIngressFlowResponse.flow_desc.packet_da.hi >> 16) & 0x000000FF,
                       (QueryIngressFlowResponse.flow_desc.packet_da.hi >>  8) & 0x000000FF,
                       (QueryIngressFlowResponse.flow_desc.packet_da.hi >>  0) & 0x000000FF,
                       (QueryIngressFlowResponse.flow_desc.packet_da.lo >> 24) & 0x000000FF,
                       (QueryIngressFlowResponse.flow_desc.packet_da.lo >> 16) & 0x000000FF);
            }
        }
    }
    return Status;
} // GetPacketDA()


/***************************************************************************//**
 * \brief   GetPeakDataRate
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      FlowIndex a.k.a. mocaIfFlowIndex.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for PeakDataRate object.
 * T_PEAK_DATA_RATE of the supported PQoS Flow (run time stats).
 * Indicate the T_PEAK_DATA_RATE of the PQoS flow identified by 'mocaIfFlowID'
 * in which this MoCA interface is an ingress node for the PQoS flow.
 *
 * \note
 * The API clnk_get_my_node_info() returns field NodeId in struct ClnkDef_MyNodeInfo_t.
 * The API clnk_get_network_node_info() passes NodeId and returns fields GUID64High and
 * GUID64Low in struct ClnkDef_NetNodeInfo_t.
 * The API clnk_list_ingress_flows() passes ingress_guid.hi and ingress_guid.lo and
 * start_index and limit in struct qos_f_l_t and returns
 * returned_flows and an array of flows[0..returned_flows-1] each of type flow_id_t in
 * struct qos_f_l_response_t.
 * The flows[FlowIndex] is the FlowID.
 * The array is indexed by mocaIfFlowIndex.
 * The mocaIfFlowID is a multicast Ethernet MAC address.
 * Not to be confused with mocaIfFlowIndex.
 * The API clnk_query_ingress_flow() passes flow_id (hi&lo) and returns
 * flow_desc.t_peak_data_rate_kbps in struct qos_q_f_response_t.
 * It is assumed that since this is of type Unsigned32 instead of MocaPhyRate, that the
 * units will stay at Kbps.
 *
 ******************************************************************************/
static SYS_UINT32 GetPeakDataRate(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                  SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 FlowIndex = Index1 - 1;  // Index1 starts at 1 when representing FlowIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my NodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    // does node not exist in network?
    else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << MyNodeInfo.NodeId)) &&
                MyNodeInfo.NetworkNodeBitMask != 1))
    {
        printf("NodeId=%d does not exist in network.\n", MyNodeInfo.NodeId);
        exit(1);
    }
    // get GUID64High and GUID64Low
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, MyNodeInfo.NodeId))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        qos_f_l_t          ListIngressFlowsRequest;
        qos_f_l_response_t ListIngressFlowsResponse;

        ListIngressFlowsRequest.ingress_guid.hi = MyNetworkNodeInfo.GUID64High;
        ListIngressFlowsRequest.ingress_guid.lo = MyNetworkNodeInfo.GUID64Low;
        ListIngressFlowsRequest.start_index     = 0;
        ListIngressFlowsRequest.limit           = QOS_MAX_FLOWS;

        // get returned_flows, total_flows and flows[0..returned_flows-1]
        if (clnk_list_ingress_flows(pClnkCtx, &ListIngressFlowsRequest,
                                    &ListIngressFlowsResponse))
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_DRIVER_ERROR)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_TIMEOUT)
        {
            printf("clnk_list_ingress_flows() QOS_TIMEOUT\n");
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
        {
            printf("IngressGuid=%02x:%02x:%02x:%02x:%02x:%02x not PQOS\n",
                    (MyNetworkNodeInfo.GUID64High >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >> 16) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  8) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  0) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 16) & 0x000000FF);
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error != QOS_SUCCESS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.total_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.returned_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (FlowIndex >= ListIngressFlowsResponse.returned_flows)
        {
            printf("FlowIndex=%d does not exist.\n", FlowIndex);
            exit(1);
        }
        else
        {
            flow_name_t         QueryIngressFlowRequest;
            qos_q_f_response_t  QueryIngressFlowResponse;

            QueryIngressFlowRequest.flow_id = ListIngressFlowsResponse.flows[FlowIndex];

            // get t_peak_data_rate_kbps
            if (clnk_query_ingress_flow(pClnkCtx, &QueryIngressFlowRequest,
                                        &QueryIngressFlowResponse))
            {
                Status = ~SYS_SUCCESS;
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_DRIVER_ERROR)
            {
                Status = ~SYS_SUCCESS;
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_TIMEOUT)
            {
                printf("clnk_query_ingress_flow() QOS_TIMEOUT\n");
                exit(1);
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
            {
                printf("flow_id=%02x:%02x:%02x:%02x:%02x:%02x not PQOS\n",
                        (QueryIngressFlowRequest.flow_id.hi >> 24) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >> 16) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >>  8) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >>  0) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.lo  >> 24) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.lo  >> 16) & 0x000000FF);
                exit(1);
            }
            else if (QueryIngressFlowResponse.qos_error != QOS_SUCCESS)
            {
                Status = ~SYS_SUCCESS;
            }
            else
            {
                *pObjectDataUnsigned =
                    QueryIngressFlowResponse.flow_desc.t_peak_data_rate_kbps;
            }
        }
    }
    return Status;
} // GetPeakDataRate()


/***************************************************************************//**
 * \brief   GetBurstSize
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      FlowIndex a.k.a. mocaIfFlowIndex.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for BurstSize object.
 * T_BURST_SIZE of the supported PQoS flow.
 * Indicate the T_BURST_SIZE of the PQoS flow identified by 'mocaIfFlowID'
 * in which this MoCA interface is an ingress node for this PQoS flow.
 *
 * \note
 * The API clnk_get_my_node_info() returns field NodeId in struct ClnkDef_MyNodeInfo_t.
 * The API clnk_get_network_node_info() passes NodeId and returns fields GUID64High and
 * GUID64Low in struct ClnkDef_NetNodeInfo_t.
 * The API clnk_list_ingress_flows() passes ingress_guid.hi and ingress_guid.lo and
 * start_index and limit in struct qos_f_l_t and returns
 * returned_flows and an array of flows[0..returned_flows-1] each of type flow_id_t in
 * struct qos_f_l_response_t.
 * The flows[FlowIndex] is the FlowID.
 * The array is indexed by mocaIfFlowIndex.
 * The mocaIfFlowID is a multicast Ethernet MAC address.
 * Not to be confused with mocaIfFlowIndex.
 * The API clnk_query_ingress_flow() passes flow_id (hi&lo) and returns
 * flow_desc.t_burst_size in struct qos_q_f_response_t.
 * Units for BurstSize is flow packets.
 *
 ******************************************************************************/
static SYS_UINT32 GetBurstSize(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                               SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 FlowIndex = Index1 - 1;  // Index1 starts at 1 when representing FlowIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;
    const SYS_UINT32 MinBurstSize = 1;
    const SYS_UINT32 MaxBurstSize = 9;

    *pObjectDataUnsigned = 0;
    // get my NodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    // does node not exist in network?
    else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << MyNodeInfo.NodeId)) &&
                MyNodeInfo.NetworkNodeBitMask != 1))
    {
        printf("NodeId=%d does not exist in network.\n", MyNodeInfo.NodeId);
        exit(1);
    }
    // get GUID64High and GUID64Low
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, MyNodeInfo.NodeId))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        qos_f_l_t          ListIngressFlowsRequest;
        qos_f_l_response_t ListIngressFlowsResponse;

        ListIngressFlowsRequest.ingress_guid.hi = MyNetworkNodeInfo.GUID64High;
        ListIngressFlowsRequest.ingress_guid.lo = MyNetworkNodeInfo.GUID64Low;
        ListIngressFlowsRequest.start_index     = 0;
        ListIngressFlowsRequest.limit           = QOS_MAX_FLOWS;

        // get returned_flows, total_flows and flows[0..returned_flows-1]
        if (clnk_list_ingress_flows(pClnkCtx, &ListIngressFlowsRequest,
                                    &ListIngressFlowsResponse))
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_DRIVER_ERROR)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_TIMEOUT)
        {
            printf("clnk_list_ingress_flows() QOS_TIMEOUT\n");
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
        {
            printf("IngressGuid=%02x:%02x:%02x:%02x:%02x:%02x not PQOS\n",
                    (MyNetworkNodeInfo.GUID64High >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >> 16) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  8) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  0) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 16) & 0x000000FF);
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error != QOS_SUCCESS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.total_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.returned_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (FlowIndex >= ListIngressFlowsResponse.returned_flows)
        {
            printf("FlowIndex=%d does not exist.\n", FlowIndex);
            exit(1);
        }
        else
        {
            flow_name_t         QueryIngressFlowRequest;
            qos_q_f_response_t  QueryIngressFlowResponse;

            QueryIngressFlowRequest.flow_id = ListIngressFlowsResponse.flows[FlowIndex];

            // get t_burst_size
            if (clnk_query_ingress_flow(pClnkCtx, &QueryIngressFlowRequest,
                                        &QueryIngressFlowResponse))
            {
                Status = ~SYS_SUCCESS;
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_DRIVER_ERROR)
            {
                Status = ~SYS_SUCCESS;
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_TIMEOUT)
            {
                printf("clnk_query_ingress_flow() QOS_TIMEOUT\n");
                exit(1);
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
            {
                printf("flow_id=%02x:%02x:%02x:%02x:%02x:%02x not PQOS\n",
                        (QueryIngressFlowRequest.flow_id.hi >> 24) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >> 16) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >>  8) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >>  0) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.lo  >> 24) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.lo  >> 16) & 0x000000FF);
                exit(1);
            }
            else if (QueryIngressFlowResponse.qos_error != QOS_SUCCESS)
            {
                Status = ~SYS_SUCCESS;
            }
            else if (QueryIngressFlowResponse.flow_desc.t_burst_size < MinBurstSize ||
                     QueryIngressFlowResponse.flow_desc.t_burst_size > MaxBurstSize)
            {
                Status = ~SYS_SUCCESS;
            }
            else
            {
                *pObjectDataUnsigned = QueryIngressFlowResponse.flow_desc.t_burst_size;
            }
        }
    }
    return Status;
} // GetBurstSize()


/***************************************************************************//**
 * \brief   GetLeaseTime
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      FlowIndex a.k.a. mocaIfFlowIndex.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for LeaseTime object.
 * T_LEASE_TIME of the supported PQoS flow.
 * Indicate the T_LEASE_TIME of the PQoS flow identified by 'mocaIfFlowID'
 * in which this MoCA interface is an ingress node for this PQoS flow.
 *
 * \note
 * The API clnk_get_my_node_info() returns field NodeId in struct ClnkDef_MyNodeInfo_t.
 * The API clnk_get_network_node_info() passes NodeId and returns fields GUID64High and
 * GUID64Low in struct ClnkDef_NetNodeInfo_t.
 * The API clnk_list_ingress_flows() passes ingress_guid.hi and ingress_guid.lo and
 * start_index and limit in struct qos_f_l_t and returns
 * returned_flows and an array of flows[0..returned_flows-1] each of type flow_id_t in
 * struct qos_f_l_response_t.
 * The flows[FlowIndex] is the FlowID.
 * The array is indexed by mocaIfFlowIndex.
 * The mocaIfFlowID is a multicast Ethernet MAC address.
 * Not to be confused with mocaIfFlowIndex.
 * The API clnk_query_ingress_flow() passes flow_id (hi&lo) and returns
 * flow_desc.t_lease_time in struct qos_q_f_response_t.
 *
 ******************************************************************************/
static SYS_UINT32 GetLeaseTime(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                               SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 FlowIndex = Index1 - 1;  // Index1 starts at 1 when representing FlowIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my NodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    // does node not exist in network?
    else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << MyNodeInfo.NodeId)) &&
                MyNodeInfo.NetworkNodeBitMask != 1))
    {
        printf("NodeId=%d does not exist in network.\n", MyNodeInfo.NodeId);
        exit(1);
    }
    // get GUID64High and GUID64Low
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, MyNodeInfo.NodeId))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        qos_f_l_t          ListIngressFlowsRequest;
        qos_f_l_response_t ListIngressFlowsResponse;

        ListIngressFlowsRequest.ingress_guid.hi = MyNetworkNodeInfo.GUID64High;
        ListIngressFlowsRequest.ingress_guid.lo = MyNetworkNodeInfo.GUID64Low;
        ListIngressFlowsRequest.start_index     = 0;
        ListIngressFlowsRequest.limit           = QOS_MAX_FLOWS;

        // get returned_flows, total_flows and flows[0..returned_flows-1]
        if (clnk_list_ingress_flows(pClnkCtx, &ListIngressFlowsRequest,
                                    &ListIngressFlowsResponse))
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_DRIVER_ERROR)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_TIMEOUT)
        {
            printf("clnk_list_ingress_flows() QOS_TIMEOUT\n");
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
        {
            printf("IngressGuid=%02x:%02x:%02x:%02x:%02x:%02x not PQOS\n",
                    (MyNetworkNodeInfo.GUID64High >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >> 16) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  8) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  0) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 16) & 0x000000FF);
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error != QOS_SUCCESS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.total_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.returned_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (FlowIndex >= ListIngressFlowsResponse.returned_flows)
        {
            printf("FlowIndex=%d does not exist.\n", FlowIndex);
            exit(1);
        }
        else
        {
            flow_name_t         QueryIngressFlowRequest;
            qos_q_f_response_t  QueryIngressFlowResponse;

            QueryIngressFlowRequest.flow_id = ListIngressFlowsResponse.flows[FlowIndex];

            // get t_lease_time
            if (clnk_query_ingress_flow(pClnkCtx, &QueryIngressFlowRequest,
                                        &QueryIngressFlowResponse))
            {
                Status = ~SYS_SUCCESS;
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_DRIVER_ERROR)
            {
                Status = ~SYS_SUCCESS;
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_TIMEOUT)
            {
                printf("clnk_query_ingress_flow() QOS_TIMEOUT\n");
                exit(1);
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
            {
                printf("flow_id=%02x:%02x:%02x:%02x:%02x:%02x not PQOS\n",
                        (QueryIngressFlowRequest.flow_id.hi >> 24) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >> 16) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >>  8) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >>  0) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.lo  >> 24) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.lo  >> 16) & 0x000000FF);
                exit(1);
            }
            else if (QueryIngressFlowResponse.qos_error != QOS_SUCCESS)
            {
                Status = ~SYS_SUCCESS;
            }
            else
            {
                *pObjectDataUnsigned = QueryIngressFlowResponse.flow_desc.t_lease_time;
            }
        }
    }
    return Status;
} // GetLeaseTime()


/***************************************************************************//**
 * \brief   GetFlowTag
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      FlowIndex a.k.a. mocaIfFlowIndex.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for FlowTag object.
 * Indicate the FLOW_TAG of the PQoS flow identified by 'mocaIfFlowID'
 * in which this MoCA interface is an ingress node for this PQoS flow.
 * The FLOW_TAG carries application specific content of this PQoS flow.
 *
 * \note
 * The API clnk_get_my_node_info() returns field NodeId in struct ClnkDef_MyNodeInfo_t.
 * The API clnk_get_network_node_info() passes NodeId and returns fields GUID64High and
 * GUID64Low in struct ClnkDef_NetNodeInfo_t.
 * The API clnk_list_ingress_flows() passes ingress_guid.hi and ingress_guid.lo and
 * start_index and limit in struct qos_f_l_t and returns
 * returned_flows and an array of flows[0..returned_flows-1] each of type flow_id_t in
 * struct qos_f_l_response_t.
 * The flows[FlowIndex] is the FlowID.
 * The array is indexed by mocaIfFlowIndex.
 * The mocaIfFlowID is a multicast Ethernet MAC address.
 * Not to be confused with mocaIfFlowIndex.
 * The API clnk_query_ingress_flow() passes flow_id (hi&lo) and returns
 * flow_desc.opaque in struct qos_q_f_response_t.
 * The opaque field is the FlowTag.
 *
 ******************************************************************************/
static SYS_UINT32 GetFlowTag(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                             SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 FlowIndex = Index1 - 1;  // Index1 starts at 1 when representing FlowIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my NodeId
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // see if NodeId out of range
    else if (MyNodeInfo.NodeId >= MAX_NUM_NODES)
    {
        Status = ~SYS_SUCCESS;
    }
    // does node not exist in network?
    else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << MyNodeInfo.NodeId)) &&
                MyNodeInfo.NetworkNodeBitMask != 1))
    {
        printf("NodeId=%d does not exist in network.\n", MyNodeInfo.NodeId);
        exit(1);
    }
    // get GUID64High and GUID64Low
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, MyNodeInfo.NodeId))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        qos_f_l_t          ListIngressFlowsRequest;
        qos_f_l_response_t ListIngressFlowsResponse;

        ListIngressFlowsRequest.ingress_guid.hi = MyNetworkNodeInfo.GUID64High;
        ListIngressFlowsRequest.ingress_guid.lo = MyNetworkNodeInfo.GUID64Low;
        ListIngressFlowsRequest.start_index     = 0;
        ListIngressFlowsRequest.limit           = QOS_MAX_FLOWS;

        // get returned_flows, total_flows and flows[0..returned_flows-1]
        if (clnk_list_ingress_flows(pClnkCtx, &ListIngressFlowsRequest,
                                    &ListIngressFlowsResponse))
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_DRIVER_ERROR)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_TIMEOUT)
        {
            printf("clnk_list_ingress_flows() QOS_TIMEOUT\n");
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
        {
            printf("IngressGuid=%02x:%02x:%02x:%02x:%02x:%02x not PQOS\n",
                    (MyNetworkNodeInfo.GUID64High >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >> 16) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  8) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64High >>  0) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 24) & 0x000000FF,
                    (MyNetworkNodeInfo.GUID64Low  >> 16) & 0x000000FF);
            exit(1);
        }
        else if (ListIngressFlowsResponse.qos_error != QOS_SUCCESS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.total_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (ListIngressFlowsResponse.returned_flows > QOS_MAX_FLOWS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (FlowIndex >= ListIngressFlowsResponse.returned_flows)
        {
            printf("FlowIndex=%d does not exist.\n", FlowIndex);
            exit(1);
        }
        else
        {
            flow_name_t         QueryIngressFlowRequest;
            qos_q_f_response_t  QueryIngressFlowResponse;

            QueryIngressFlowRequest.flow_id = ListIngressFlowsResponse.flows[FlowIndex];

            // get opaque (a.k.a. FlowTag)
            if (clnk_query_ingress_flow(pClnkCtx, &QueryIngressFlowRequest,
                                        &QueryIngressFlowResponse))
            {
                Status = ~SYS_SUCCESS;
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_DRIVER_ERROR)
            {
                Status = ~SYS_SUCCESS;
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_TIMEOUT)
            {
                printf("clnk_query_ingress_flow() QOS_TIMEOUT\n");
                exit(1);
            }
            else if (QueryIngressFlowResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
            {
                printf("flow_id=%02x:%02x:%02x:%02x:%02x:%02x not PQOS\n",
                        (QueryIngressFlowRequest.flow_id.hi >> 24) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >> 16) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >>  8) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.hi >>  0) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.lo  >> 24) & 0x000000FF,
                        (QueryIngressFlowRequest.flow_id.lo  >> 16) & 0x000000FF);
                exit(1);
            }
            else if (QueryIngressFlowResponse.qos_error != QOS_SUCCESS)
            {
                Status = ~SYS_SUCCESS;
            }
            else
            {
                *pObjectDataUnsigned = QueryIngressFlowResponse.flow_desc.opaque;
            }
        }
    }
    return Status;
} // GetFlowTag()


/*!
 * \brief   Node Table
 *
 * The Node Table, or mocaNodeTable, is used by the managed node to report information about
 * other nodes in the MoCA network.
 * This table is indexed by the interface index (ifIndex) and MoCA node ID (mocaNodeIndex).
 * The managed node does not report itself, and hence node ID of the managed node does not
 * have an entry in this table.
 * All other nodes that the managed node has both transmission and reception usable channels
 * with (as indicated by CHANNEL_USABLE field of the Type 1 Probe Report) have an entry in
 * this table.
 */


/***************************************************************************//**
 * \brief   GetNodeMocaVersion
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      NodeIndex a.k.a. mocaNodeIndex.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for NodeMocaVersion object.
 * Indicate the MoCA version supported by the MoCA node identified by 'mocaNodeIndex'.
 * Returns moca1dot0 (10 decimal), moca1dot1 (11 decimal), or moca1dot1ProTem (12 decimal).
 *
 * \note
 * The API clnk_get_network_node_info(NodeIndex) returns
 * field NodeProtocolSupport (bit 24-31 for MocaVersion) in struct ClnkDef_NetNodeInfo_t.
 * To get the mocaIfNodeMocaVersion, perform the following calculations:
 *      ((MocaVersion >> 4) & 0x0f) * 10 +
 *      ((MocaVersion)      & 0x0f);
 *
 ******************************************************************************/
static SYS_UINT32 GetNodeMocaVersion(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                     SYS_UINT32 Index2, void* pObjectData)
{
    SYS_INT32 * const pObjectDataInteger = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 NodeIndex = Index1;  // Index1 starts at 0 when representing NodeIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    *pObjectDataInteger = 0;
    // get my NetworkNodeBitMask
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // does node not exist?
    else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << NodeIndex)) &&
                MyNodeInfo.NetworkNodeBitMask != 1))
    {
        printf("NodeIndex=%d does not exist.\n", NodeIndex);
        exit(1);
    }
    // get NodeProtocolSupport
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, NodeIndex))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        SYS_UINT32 Nps = MyNetworkNodeInfo.NodeProtocolSupport;
        SYS_INT32 MocaVersion = (Nps & MOCA_VERSION_SUPPORT_FLAG) >> 24;
        SYS_BOOLEAN IsMoca1p1Node = (MOCA_FROM_NPS(Nps) > MOCA_10) ? SYS_TRUE : SYS_FALSE;
        SYS_BOOLEAN IsMoca1p1ProtemNode =
             (Nps & COMMITTEE_SUPPORT_FLAG_MASK) == PROTEM11_SUPPORT_FLAG_VAL;
        SYS_BOOLEAN IsMoca1p1AdvancedNode = IsMoca1p1Node && !IsMoca1p1ProtemNode;
        // see if MocaVersion is zero
        if (MOCA_FROM_NPS(Nps) == 0)
        {
            *pObjectDataInteger = MOCA_1DOT0;
        }
        else if (IsMoca1p1AdvancedNode)
        {
            *pObjectDataInteger = ((MocaVersion >> 4) & 0x0f) * 10 +
                                  ((MocaVersion     ) & 0x0f);
        }
        else
        {
            *pObjectDataInteger = MOCA_1DOT1_PRO_TEM;
        }
    }
    return Status;
} // GetNodeMocaVersion()


/***************************************************************************//**
 * \brief   GetNodeMacAddress
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      NodeIndex a.k.a. mocaNodeIndex.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for NodeMacAddress object.
 * Indicate the MAC address of the MoCA node identified by 'mocaNodeIndex'.
 * This MAC address is encoded in the first six bytes of the
 * Globally Unique Identifier (GUID).
 * For example, MoCA node with MAC address aa:bb:cc:dd:ee:ff will have a GUID of
 * aa:bb:cc:dd:ee:ff:00:00.
 *
 * \note
 * The API clnk_get_network_node_info(NodeIndex) returns fields GUID64High and GUID64Low
 * in struct ClnkDef_NetNodeInfo_t.
 * The 48 bit mocaIfMacAddress consists of the 32 bits of GUID64High and
 * the 16 MSBs of GUID64Low.
 *
 ******************************************************************************/
static SYS_UINT32 GetNodeMacAddress(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                    SYS_UINT32 Index2, void* pObjectData)
{
    SYS_CHAR * const pObjectDataString = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 NodeIndex = Index1;  // Index1 starts at 0 when representing NodeIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    pObjectDataString[0] = '\0';
    // get my NetworkNodeBitMask
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // does node not exist?
    else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << NodeIndex)) &&
                MyNodeInfo.NetworkNodeBitMask != 1))
    {
        printf("NodeIndex=%d does not exist.\n", NodeIndex);
        exit(1);
    }
    // get GUID64High and GUID64Low
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, NodeIndex))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        sprintf(pObjectDataString, "%02x:%02x:%02x:%02x:%02x:%02x",
                (MyNetworkNodeInfo.GUID64High >> 24) & 0x000000FF,
                (MyNetworkNodeInfo.GUID64High >> 16) & 0x000000FF,
                (MyNetworkNodeInfo.GUID64High >>  8) & 0x000000FF,
                (MyNetworkNodeInfo.GUID64High >>  0) & 0x000000FF,
                (MyNetworkNodeInfo.GUID64Low  >> 24) & 0x000000FF,
                (MyNetworkNodeInfo.GUID64Low  >> 16) & 0x000000FF);
    }
    return Status;
} // GetNodeMacAddress()


/***************************************************************************//**
 * \brief   GetNodeTxGcdRate
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      NodeIndex a.k.a. mocaNodeIndex.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for NodeTxGcdRate object.
 * Indicate the PHY rate in Mbps for the transmit traffic broadcast from this managed node
 * to a node identified by the 'mocaNodeIndex'.
 *
 * \note
 * The API clnk_get_network_node_info(NodeIndex) returns field GCDTxBitRate in
 * struct ClnkDef_NetNodeInfo_t.
 * Units are in Mbps.
 *
 ******************************************************************************/
static SYS_UINT32 GetNodeTxGcdRate(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                   SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 NodeIndex = Index1;  // Index1 starts at 0 when representing NodeIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    *pObjectDataUnsigned = 0;
    // get my NetworkNodeBitMask
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        // is the node requested == my nodeid?
        if (NodeIndex == MyNodeInfo.NodeId)
        {
            // Can't use my GCDTxBitRate because it is zero.
            // Must use first valid node that is not me.
            SYS_UINT32 i;
            for (i = 0; i < MAX_NUM_NODES; i++)
            {
                // can't be my NodeId
                if (i != MyNodeInfo.NodeId)
                {
                    // does node exist?
                    if ((MyNodeInfo.NetworkNodeBitMask & (1 << i)) &&
                         MyNodeInfo.NetworkNodeBitMask != 1)
                    {
                        NodeIndex = i;
                        break;
                    }
                }
            }
        }
        // does node not exist?
        if (!((MyNodeInfo.NetworkNodeBitMask & (1 << NodeIndex)) &&
               MyNodeInfo.NetworkNodeBitMask != 1))
        {
            printf("NodeIndex=%d does not exist.\n", NodeIndex);
            exit(1);
        }
        // get GCDTxBitRate
        else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, NodeIndex))
        {
            Status = ~SYS_SUCCESS;
        }
        else
        {
            *pObjectDataUnsigned = MyNetworkNodeInfo.GCDTxBitRate / 1000000;
        }
    }
    return Status;
} // GetNodeTxGcdRate()


/***************************************************************************//**
 * \brief   GetNodeTxPowerReduction
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      NodeIndex a.k.a. mocaNodeIndex.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for NodeTxPowerReduction object.
 * The Transmit Power Control back-off in dB used for unicast transmissions from the
 * managed node to a node identified by 'mocaNodeIndex'.
 * The 'mocaNodeTxPowerReduction' is identical to the TPC back-off utilized for
 * transmission and determined from the TPC backoff parameters
 * TPC_BACKOFF_MAJOR and TPC_BACKOFF_MINOR as follows:
 *      mocaNodeTxPowerReduction = TPC_BACKOFF_MAJOR * 3 + TPC_BACKOFF_MINOR
 *
 * \note
 * The API clnk_get_node_phy_data(NodeIndex) returns field TxPwrAdj in
 * struct ClnkDef_PhyData_t.
 * Units are in dB.
 * The API clnk_get_node_phy_data(NodeIdInfo) accepts a special NodeIdInfo format:
 * NodeIdInfo   Note that the 25 bits in NodeID is encoded as follows:
 *              Bit[24..16] = ChannelType
 *              0 - P2P
 *              1 - GCD
 *              Bit[15..8] = Transmit/Receive (RxOn)
 *              0 - TX
 *              1 - RX
 *              Bit[7..0] = NodeId
 *
 ******************************************************************************/
static SYS_UINT32 GetNodeTxPowerReduction(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                          SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 NodeIndex = Index1;  // Index1 starts at 0 when representing NodeIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_PhyData_t     TxPhyDataInfo;
    SYS_UINT32 RxOn = 0;            // Tx=0, Rx=1
    SYS_UINT32 ChannelType = 0;     // 0=P2P, 1=GCD

    *pObjectDataUnsigned = 0;
    // get my NetworkNodeBitMask
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // does node not exist?
    else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << NodeIndex)) &&
                MyNodeInfo.NetworkNodeBitMask != 1))
    {
        printf("NodeIndex=%d does not exist.\n", NodeIndex);
        exit(1);
    }
    // get TxPwrAdj
    else if (clnk_get_node_phy_data(pClnkCtx, &TxPhyDataInfo,
                                    NodeIndex | (RxOn << 8) | (ChannelType << 16)))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        *pObjectDataUnsigned = TxPhyDataInfo.TxPwrAdj;
    }
    return Status;
} // GetNodeTxPowerReduction()


/***************************************************************************//**
 * \brief   GetNodePreferredNC
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      NodeIndex a.k.a. mocaNodeIndex.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for NodePreferredNC object.
 * Indicate the MoCA node identified by 'mocaNodeIndex' is set as preferred NC if
 * this value is 'true'.
 * Indicate the MoCA node identified by 'mocaNodeIndex' is not set as preferred NC if
 * this value is 'false'.
 *
 * \note
 * The API clnk_get_network_node_info(NodeIndex) returns
 * field NodeProtocolSupport (bit 6 for PreferredNC) in struct ClnkDef_NetNodeInfo_t.
 *
 ******************************************************************************/
static SYS_UINT32 GetNodePreferredNC(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                     SYS_UINT32 Index2, void* pObjectData)
{
    SYS_BOOLEAN * const pObjectDataBoolean = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 NodeIndex = Index1;  // Index1 starts at 0 when representing NodeIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    *pObjectDataBoolean = SYS_FALSE;
    // get my NetworkNodeBitMask
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // does node not exist?
    else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << NodeIndex)) &&
                MyNodeInfo.NetworkNodeBitMask != 1))
    {
        printf("NodeIndex=%d does not exist.\n", NodeIndex);
        exit(1);
    }
    // get NodeProtocolSupport
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, NodeIndex))
    {
        Status = ~SYS_SUCCESS;
    }
    else if (MyNetworkNodeInfo.NodeProtocolSupport & PREFERRED_NC_SUPPORT_FLAG)
    {
        *pObjectDataBoolean = SYS_TRUE;
    }
    else
    {
        *pObjectDataBoolean = SYS_FALSE;
    }
    return Status;
} // GetNodePreferredNC()


/***************************************************************************//**
 * \brief   GetNodeQAM256Capable
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      NodeIndex a.k.a. mocaNodeIndex.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for NodeQAM256Capable object.
 * Indicate the MoCA node identified by 'mocaNodeIndex' supports QAM256 if
 * the value is 'true'.
 * Indicate the MoCA node identified by 'mocaNodeIndex' does not support QAM256 if
 * this value is 'false'.
 *
 * \note
 * The API clnk_get_network_node_info(NodeIndex) returns
 * field NodeProtocolSupport (bit 4 for QAM256Capable) in struct ClnkDef_NetNodeInfo_t.
 *
 ******************************************************************************/
static SYS_UINT32 GetNodeQAM256Capable(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                       SYS_UINT32 Index2, void* pObjectData)
{
    SYS_BOOLEAN * const pObjectDataBoolean = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 NodeIndex = Index1;  // Index1 starts at 0 when representing NodeIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    *pObjectDataBoolean = SYS_FALSE;
    // get my NetworkNodeBitMask
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // does node not exist?
    else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << NodeIndex)) &&
                MyNodeInfo.NetworkNodeBitMask != 1))
    {
        printf("NodeIndex=%d does not exist.\n", NodeIndex);
        exit(1);
    }
    // get NodeProtocolSupport
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, NodeIndex))
    {
        Status = ~SYS_SUCCESS;
    }
    else if (MyNetworkNodeInfo.NodeProtocolSupport & QAM256_SUPPORT_FLAG)
    {
        *pObjectDataBoolean = SYS_TRUE;
    }
    else
    {
        *pObjectDataBoolean = SYS_FALSE;
    }
    return Status;
} // GetNodeQAM256Capable()


/***************************************************************************//**
 * \brief   GetNodePacketsAggrCapability
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      NodeIndex a.k.a. mocaNodeIndex.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for NodePacketsAggrCapability object.
 * Represent the maximum number of Ethernet packets aggregated in a MoCA frame that is
 * supported at transmit/receive.
 *      'none'   (0) represents packet aggregation is not supported,
 *                   i.e. MoCA 1.0 or MoCA 1.1 ProTem devices.
 *      'aggr6'  (6) represents maximum aggregation of 6 packets.
 *      'aggr10' (10) represents maximum aggregation of 10 packets.
 *
 * \note
 * The API clnk_get_network_node_info(NodeIndex) returns field NodeProtocolSupport in
 * struct ClnkDef_NetNodeInfo_t.
 * Bits 8&7=10 (binary) for aggr10 (10), bits 8&7=00 (binary) for aggr6 (6),
 * otherwise none (0).
 *
 ******************************************************************************/
static SYS_UINT32 GetNodePacketsAggrCapability(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                               SYS_UINT32 Index2, void* pObjectData)
{
    SYS_INT32 * const pObjectDataInteger = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 NodeIndex = Index1;  // Index1 starts at 0 when representing NodeIndex
    ClnkDef_MyNodeInfo_t  MyNodeInfo;
    ClnkDef_NetNodeInfo_t MyNetworkNodeInfo;

    *pObjectDataInteger = AGGR0;
    // get my NetworkNodeBitMask
    if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
    {
        Status = ~SYS_SUCCESS;
    }
    // does node not exist?
    else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << NodeIndex)) &&
                MyNodeInfo.NetworkNodeBitMask != 1))
    {
        printf("NodeIndex=%d does not exist.\n", NodeIndex);
        exit(1);
    }
    // get NodeProtocolSupport
    else if (clnk_get_network_node_info(pClnkCtx, &MyNetworkNodeInfo, NodeIndex))
    {
        Status = ~SYS_SUCCESS;
    }
    else
    {
        SYS_UINT32 Nps = MyNetworkNodeInfo.NodeProtocolSupport;
        SYS_BOOLEAN IsMoca1p1Node = (MOCA_FROM_NPS(Nps) > MOCA_10) ? SYS_TRUE : SYS_FALSE;
        // bit 8 on and bit 7 off?
        if ((MyNetworkNodeInfo.NodeProtocolSupport & COMMITTEE_SUPPORT_FLAG_MASK) ==
            AGGR10_SUPPORT_FLAG_VAL)
        {
            *pObjectDataInteger = AGGR10;
        }
        // bit 8 off and bit 7 off?
        else if ((MyNetworkNodeInfo.NodeProtocolSupport & COMMITTEE_SUPPORT_FLAG_MASK) ==
                 AGGR6_SUPPORT_FLAG_VAL)
        {
            // check if it is a MoCA 1.1 node
            if (IsMoca1p1Node)
            {
                *pObjectDataInteger = AGGR6;
            }
            else
            {
                *pObjectDataInteger = AGGR0;
            }
        }
        // bit 8 off and bit 7 on?
        // or
        // bit 8 on and bit 7 on?
        else
        {
            *pObjectDataInteger = AGGR0;
        }
    }
    return Status;
} // GetNodePacketsAggrCapability()


/*!
 * \brief   Mesh Table
 *
 * The Mesh Table, or mocaMeshTable, provides the PHY rates between any pair of nodes in the
 * MoCA network.
 * Each PHY rate is associated with an interface index (ifIndex),
 * transmit node ID (mocaMeshTxNodeIndex) and receive node ID (mocaMeshRxNodeIndex).
 * PHY rate is the transmit data rate from the node identified by the transmit node ID,
 * to the node identified by the receive node ID.
 */


/***************************************************************************//**
 * \brief   TimerStart
 *
 * \param[in]   pInstance       Pointer to timer expiration.
 * \param[in]   milliseconds    Milliseconds to wait.
 *
 * \return      None
 *
 * Starts a logical timer.  There is only one timer that can run at a time.
 *
 * \note
 *
 ******************************************************************************/
static void TimerStart(mocamib_tmr_instance_t*  pInstance,
                       SYS_UINT32               Milliseconds)
{
    struct timeval Delta;

    if (!pInstance)
    {
        return;
    }

    Delta.tv_sec = Milliseconds / 1000;
    Delta.tv_usec = (Milliseconds % 1000) * 1000;

#ifdef AEI_WECB
    aei_gettimeofday(&pInstance->expire, NULL);
#else
    gettimeofday(&pInstance->expire, NULL);
#endif

    pInstance->expire.tv_sec  += Delta.tv_sec;
    pInstance->expire.tv_usec += Delta.tv_usec;
    if (pInstance->expire.tv_usec > 1000 * 1000)
    {
        pInstance->expire.tv_sec  += 1;
        pInstance->expire.tv_usec -= 1000 * 1000;
    }
} // TimerStart()


/***************************************************************************//**
 * \brief   TimerMsRemaining
 *
 * \param[in]   pInstance   Pointer to timer expiration.
 *
 * \return      Number of milliseconds remaining on the logical timer.
 *
 * Queries the number of milliseconds remaining on the logical timer.
 *
 * \note
 *
 ******************************************************************************/
static SYS_UINT32 TimerMsRemaining(const mocamib_tmr_instance_t* pInstance)
{
    struct timeval Now;

    if (!pInstance)
    {
        return 0;
    }

#ifdef AEI_WECB
    aei_gettimeofday(&Now, NULL);
#else
    gettimeofday(&Now, NULL);
#endif
    if (pInstance->expire.tv_sec < Now.tv_sec)
    {
        return 0;
    }

    if (pInstance->expire.tv_sec == Now.tv_sec &&
        pInstance->expire.tv_usec < Now.tv_usec)
    {
        return 0;
    }

    return (pInstance->expire.tv_sec - Now.tv_sec) * 1000 +
           (pInstance->expire.tv_usec / 1000) -
           (Now.tv_usec               / 1000) ;
} // TimerMsRemaining()


/***************************************************************************//**
 * \brief   TimerSpin
 *
 * \param[in]   ms          Milliseconds to wait.
 *
 * \return      None
 *
 * Causes the processor to spin for at least designated number of
 * milliseconds using the logical timer.
 *
 * \note
 *
 ******************************************************************************/
static void TimerSpin(SYS_UINT32 Milliseconds)
{
    mocamib_tmr_instance_t Instance;

    struct timeval Now;
#ifdef AEI_WECB
    aei_gettimeofday(&Now, NULL);
#else
    gettimeofday(&Now, NULL);
#endif
    TimerStart(&Instance, Milliseconds);
    while (SYS_TRUE)
    {
        SYS_UINT32 Remaining = TimerMsRemaining(&Instance);

#if WATCH_TIMER_ODDITIES
        static SYS_UINT32 PrevRemaining = 0;
        static SYS_UINT32 Times = 0;
        if (PrevRemaining != Remaining)
        {
            printf("      %d  x%d\n", Remaining, Times);
            PrevRemaining = Remaining;
            Times=0;
        }
        else
        {
            Times++;
        }
#endif
        if (Remaining == 0)
        {
            break;
        }
    }
} // TimerSpin()


/***************************************************************************//**
 * \brief   SleepFunc
 *
 * \param[in]   SleepMilliseconds   Milliseconds to sleep (not used).
 *
 * \return      None
 *
 * Spins for 10 milliseconds.
 *
 * \note
 *
 ******************************************************************************/
static void SleepFunc(SYS_UINT32 SleepMilliseconds)
{
    const SYS_UINT32 Milliseconds = 10;
    TimerSpin(Milliseconds);
} // SleepFunc()


/***************************************************************************//**
 * \brief   UnpackPhyRate
 *
 * \param[in]   PackedVal    Pointer to c.LINK Library context.
 *
 * \return      Unpacked PHY Rate in bps.
 *
 * Unpack the PHY Rate received from the API clnk_get_peer_rates().
 *
 * \note
 *
 ******************************************************************************/
static SYS_UINT32 UnpackPhyRate(SYS_UINT16 PackedVal)
{
    SYS_UINT32 CpLen = ((PackedVal >> 11) << 1) + 10;
    SYS_UINT32 Bps = PackedVal & 0x7ff;
    Bps *= (50*1000*1000 * 12 / 13 / (256 + CpLen));
    return Bps;
} // UnpackPhyRate()


/***************************************************************************//**
 * \brief   GetMeshTxRate
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      MeshTxNodeIndex a.k.a. mocaMeshTxNodeIndex.
 * \param[in]   Index2      MeshRxNodeIndex a.k.a. mocaMeshRxNodeIndex.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for MeshTxRate object.
 * Indicate the transmit PHY rate in Mbps from the MoCA node identified by
 * 'mocaMeshTxNodeIndex' to the MoCA node identified by 'mocaMeshRxNodeIndex'.
 *
 * \note
 * The API clnk_get_peer_rates() returns array rates[MRT_MAX_NODES][MRT_MAX_NODES] in
 * struct peer_rates_t.
 * The rates are indexed by [Tx][Rx].
 * The peer rate to be returned is located at
 * rates[mocaMeshTxNodeIndex][mocaMeshRxNodeIndex].
 * The peer rate is packed into a 16 bit word.
 * The following subroutine shows how to convert (unpack) a single raw array element into a
 * PHY Rate in units of Mbps.
 *      static SYS_UINT32 UnpackPhyRate(SYS_UINT16 PackedVal)
 *      {
 *          SYS_UINT32 CpLen = ((PackedVal >> 11) << 1) + 10;
 *          SYS_UINT32 Bps = PackedVal & 0x7ff;
 *          Bps *= (50*1000*1000 * 12 / 13 / (256 + CpLen));
 *          return Bps;
 *      }
 * Units of Mbps.
 * The mocaMeshTxRate is also used in the mocaTrapBelowPhyThreshold and
 * mocaTrapAbovePhyThreshold traps.
 * Used in conjunction with mocaIfPhyThreshold and mocaIfPhyThresholdEnable.
 * If at first the PHY rate between all pairs of MoCA nodes is greater than or equal to
 * the passed PhyThreshold * 1000000.
 * Then later, if one of the transmit PHY rates falls below the PhyThreshold * 1000000 then
 * set mocaTrapBelowPhyThreshold to the lowest of all the PHY rates if
 * mocaIfPhyThresholdEnable is TRUE.
 * If at first the PHY rate between one or more pairs of MoCA nodes is less than the
 * passed PhyThreshold * 1000000.
 * Then later, if the PHY rate between all pairs of MoCA nodes is greater than or equal to
 * the passed PhyThreshold * 1000000 then set mocaTrapAbovePhyThreshold to ifIndex if
 * mocaIfPhyThresholdEnable is TRUE.
 * The PhyThreshold is in units of Mbps and the PHY rate when unpacked is in units of bps.
 *
 ******************************************************************************/
static SYS_UINT32 GetMeshTxRate(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                SYS_UINT32 Index2, void* pObjectData)
{
    SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 MeshTxNodeIndex = Index1;  // Index1 starts at 0 for MeshTxNodeIndex
    SYS_UINT32 MeshRxNodeIndex = Index2;  // Index2 starts at 0 for MeshRxNodeIndex
    peer_rates_t Peer;
    peer_rates_entry_status_t PeerRatesEntryStatus;
    ClnkDef_MyNodeInfo_t  MyNodeInfo;

    *pObjectDataUnsigned = 0;
    // are MeshTxNodeIndex and MeshRxNodeIndex addresses?
    if (MeshTxNodeIndex >= MAX_NUM_NODES && MeshRxNodeIndex >= MAX_NUM_NODES)
    {
        SYS_UINT32 * pMeshTxNodeIndex = (SYS_UINT32 *)MeshTxNodeIndex;
        SYS_UINT32 * pMeshRxNodeIndex = (SYS_UINT32 *)MeshRxNodeIndex;
        SYS_UINT32 SmallestPhyRate = 0xFFFFFFFF;
        SYS_UINT32 SmallestPhyRateTxIndex = 0;
        SYS_UINT32 SmallestPhyRateRxIndex = 0;
        SYS_UINT32 TxIndex = 0;
        SYS_UINT32 RxIndex = 0;

        if (clnk_get_peer_rates(pClnkCtx, &Peer, &PeerRatesEntryStatus, 5000, SleepFunc) !=
            SYS_SUCCESS)
        {
            Status = ~SYS_SUCCESS;
        }
        else
        {
            // must use MRT_MAX_NODES, not MAX_NUM_NODES
            for (TxIndex = 0; TxIndex < MRT_MAX_NODES; TxIndex++)
            {
                for (RxIndex = 0; RxIndex < MRT_MAX_NODES; RxIndex++)
                {
                    if (TxIndex != RxIndex)
                    {
                        if (PeerRatesEntryStatus.entry_status[TxIndex][RxIndex] ==
                            PEER_RATES_VALID)
                        {
                            SYS_UINT32 ThisPhyRate =
                                UnpackPhyRate(Peer.rates[TxIndex][RxIndex]);
                            if (ThisPhyRate < SmallestPhyRate)
                            {
                                SmallestPhyRate = ThisPhyRate;
                                SmallestPhyRateTxIndex = TxIndex;
                                SmallestPhyRateRxIndex = RxIndex;
                            }
                        }
                    }
                }
            }
            // were there no PeerRatesEntryStatus.entry_status[][] with PEER_RATES_VALID?
            if (SmallestPhyRate == 0xFFFFFFFF)
            {
                // use 0 for SmallestPhyRate
                SmallestPhyRate = 0;
            }
            *pObjectDataUnsigned = SmallestPhyRate / 1000000;
            *pMeshTxNodeIndex = SmallestPhyRateTxIndex;
            *pMeshRxNodeIndex = SmallestPhyRateRxIndex;
        }
    }
    else
    {
        // does TxNodeIndex = RxNodeIndex?
        if (MeshTxNodeIndex == MeshRxNodeIndex)
        {
            printf("MeshTxNodeIndex and MeshRxNodeIndex are the same.\n");
            Usage(g_CmdLineVars.ppArgv);
        }
        // get my NetworkNodeBitMask
        else if (clnk_get_my_node_info(pClnkCtx, &MyNodeInfo, 0))
        {
            Status = ~SYS_SUCCESS;
        }
        // does TxNodeIndex not exist?
        else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << MeshTxNodeIndex)) &&
                    MyNodeInfo.NetworkNodeBitMask != 1))
        {
            printf("MeshTxNodeIndex=%d does not exist.\n", MeshTxNodeIndex);
            exit(1);
        }
        // does RxNodeIndex not exist?
        else if (!((MyNodeInfo.NetworkNodeBitMask & (1 << MeshRxNodeIndex)) &&
                    MyNodeInfo.NetworkNodeBitMask != 1))
        {
            printf("MeshRxNodeIndex=%d does not exist.\n", MeshRxNodeIndex);
            exit(1);
        }
        else if (clnk_get_peer_rates(pClnkCtx, &Peer, &PeerRatesEntryStatus, 5000, SleepFunc)
                 != SYS_SUCCESS)
        {
            Status = ~SYS_SUCCESS;
        }
        else if (PeerRatesEntryStatus.entry_status[MeshTxNodeIndex][MeshRxNodeIndex] ==
                 PEER_RATES_VALID)
        {
            SYS_UINT32 ThisPhyRate =
                UnpackPhyRate(Peer.rates[MeshTxNodeIndex][MeshRxNodeIndex]);
            *pObjectDataUnsigned = ThisPhyRate / 1000000;
        }
    }
    return Status;
} // GetMeshTxRate()


/*!
 * \brief   Notification Events
 *
 * The asynchronous notification events or traps defined in the following table may be sent
 * from the MoCA device to the network management system (NMS).
 * A trap can contain any number of variable bindings (VarBind) or MIB object-value pairs.
 * The traps defined in this document must include the interface index (ifIndex) as one of
 * the VarBind, or as part of the instance of the VarBind.
 */


/***************************************************************************//**
 * \brief   GetTrapBelowPhyThreshold
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for TrapBelowPhyThreshold object.
 * Indicate the lowest transmit PHY rate in Mbps from any MoCA node to any other MoCA node.
 * Contain 1 VarBind for mocaMeshTxRate.
 * If transmit PHY rate between all pairs of MoCA nodes are greater than or equal to
 * 'mocaIfPhyThreshold', then transmit PHY rate of one pair of MoCA node is less than
 * 'mocaIfPhyThreshold', send this notification if 'mocaIfPhyThresholdEnable' is 'true'.
 * This notification sends the lowest PHY rate in the MoCA network, or 'mocaMeshTxRate',
 * which is the transmit PHY rate from the node specified with 'mocaMeshTxNodeIndex',
 * to the node specified with 'mocaMeshRxNodeIndex'.
 * If multiple links have the same lowest PHY rate, sends the PHY rate for the link with the
 * lowest 'mocaMeshTxNodeIndex'.
 *  If multiple links with the same 'mocaMeshTxNodeIndex' have the same lowest PHY rate,
 * sends the PHY rate for the link with the lowest 'mocaMeshRxNodeIndex'.
 * Network management system should access 'mocaMeshTable' to find if there are additional
 * links below the threshold bandwidth.
 * For example, MoCA interface with 'ifIndex' 1 detects transmit PHY rate from
 * 'mocaMeshTxNodeIndex' 2 to 'mocaMeshRxNodeIndex' 4 is 150 Mbps,
 * and this link is the first link to drop below the 'mocaIfPhyThreshold',
 * then this trap contains one variable binding of mocaMeshTxRate.1.2.4 = 150.
 *
 * \note
 * Return the lowest PHY rate in the network calculated during mocaIfPhyThreshold.
 * If at first the transmit PHY rate between all pairs of MoCA nodes are
 * greater than or equal to this value, then later the transmit PHY rate of
 * one pair of MoCA node is less than this value, set 'mocaTrapBelowPhyThreshold' to the
 * lowest PHY rate in the network if 'mocaIfPhyThresholdEnable' is 'true'.
 * Save the VarBind of lowest PHY rate in the network.
 * VarBindBelowPhyThreshold =
 * {Oid="mocaMeshTxRate.1.x,y",Value=rates[mocaMeshTxNodeIndex][mocaMeshRxNodeIndex] Mbps}
 * where x is the mocaMeshTxNodeIndex and y is the mocaMeshRxNodeIndex.
 * When mocaTrapBelowPhyThreshold is set, the mocaTrapAbovePhyThreshold is cleared.
 * This is implemented in the MMA program by the MMA program first calling the Get for
 * MeshTxRate object then the MMA program calling the Get for mocaTrapBelowPhyThreshold.
 * If multiple links have the same lowest PHY rate, sends the PHY rate for the link with
 * the lowest 'mocaMeshTxNodeIndex'.
 * If multiple links with the same 'mocaMeshTxNodeIndex' have the same lowest PHY rate,
 * sends the PHY rate for the link with the lowest 'mocaMeshRxNodeIndex'.
 * This is implemented by setting the trap VarBind every time the object Get is called.
 * It does not work only on transitions from above to below.
 *
 ******************************************************************************/
static SYS_UINT32 GetTrapBelowPhyThreshold(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                           SYS_UINT32 Index2, void* pObjectData)
{
    VarBind_t * const pObjectDataVarbind = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 * pSmallestPhyRate = &g_ObjectVar.Uint;
    SYS_BOOLEAN PhyThresholdEnable;
    SYS_UINT32 PhyThreshold;
    GetFunc_t GetFunc;

    // trap needs to do GetPhyThresholdEnable()
    PhyThresholdEnable = SYS_FALSE;
    GetFunc = ObjectInfoTable[IF_PHY_THRESHOLD_ENABLE].GetFunc;
    if (GetFunc)
    {
        if (GetFunc(pClnkCtx, 0, 0, &PhyThresholdEnable) != SYS_SUCCESS)
        {
            printf("Get failed, ObjectIndex=%d\n", IF_PHY_THRESHOLD_ENABLE);
            exit(1);
        }
    }

    // trap needs to do GetPhyThreshold()
    PhyThreshold = 0;
    GetFunc = ObjectInfoTable[IF_PHY_THRESHOLD].GetFunc;
    if (GetFunc)
    {
        if (GetFunc(pClnkCtx, 0, 0, &PhyThreshold) != SYS_SUCCESS)
        {
            printf("Get failed, ObjectIndex=%d\n", IF_PHY_THRESHOLD);
            exit(1);
        }
    }

    pObjectDataVarbind->pOidString = g_ObjectVar.Str;
    pObjectDataVarbind->pObjectData = pSmallestPhyRate;
    if (PhyThresholdEnable && *pSmallestPhyRate < PhyThreshold)
    {
        sprintf(pObjectDataVarbind->pOidString, "mocaMeshTxRate.%d.%d.%d",
                IF_INDEX, Index1, Index2);
    }
    else
    {
        pObjectDataVarbind->pOidString[0] = '\0';
    }
    return Status;
} // GetTrapBelowPhyThreshold()


/***************************************************************************//**
 * \brief   GetTrapAbovePhyThreshold
 *
 * \param[in]   pClnkCtx    Pointer to c.LINK Library context.
 * \param[in]   Index1      Not used for this object.
 * \param[in]   Index2      Not used for this object.
 * \param[out]  pObjectData Pointer to object's data to return result.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Get for TrapAbovePhyThreshold object.
 * Indicate the lowest transmit PHY rate in Mbps from any MoCA node to any other MoCA node.
 * Contain 1 VarBind for ifIndex.
 * If transmit PHY rate between one or more pairs of MoCA nodes are less than
 * 'mocaIfPhyThreshold', then transmit PHY rate between all pairs of MoCA nodes are
 * greater than or equal to 'mocaIfPhyThreshold', send this notification if
 * 'mocaIfPhyThresholdEnable' is 'true'.
 *
 * \note
 * Return the ifIndex calculated during mocaIfPhyThreshold.
 * If at first the transmit PHY rate between one or more pairs of MoCA nodes are
 * less than this value, then later the transmit PHY rate between all pairs of MoCA nodes are
 * greater than or equal to this value, set 'mocaTrapAbovePhyThreshold' to the ifIndex if
 * 'mocaIfPhyThresholdEnable' is 'true'.
 * Save the VarBind of ifIndex.
 * VarBindAbovePhyThreshold={Oid="ifIndex",Value=ifIndex}.
 * When mocaTrapAbovePhyThreshold is set, the mocaTrapBelowPhyThreshold is cleared.
 * This is implemented in the MMA program by the MMA program first calling the Get for
 * MeshTxRate object then the MMA program calling the Get for mocaTrapAbovePhyThreshold.
 * This is implemented by setting the trap VarBind every time the object Get is called.
 * It does not work only on transitions from below to above.
 *
 ******************************************************************************/
static SYS_UINT32 GetTrapAbovePhyThreshold(clnk_ctx_t *pClnkCtx, SYS_UINT32 Index1,
                                           SYS_UINT32 Index2, void* pObjectData)
{
    VarBind_t * const pObjectDataVarbind = pObjectData;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 * pSmallestPhyRate = &g_ObjectVar.Uint;
    SYS_BOOLEAN PhyThresholdEnable;
    SYS_UINT32 PhyThreshold;
    GetFunc_t GetFunc;

    // trap needs to do GetPhyThresholdEnable()
    PhyThresholdEnable = SYS_FALSE;
    GetFunc = ObjectInfoTable[IF_PHY_THRESHOLD_ENABLE].GetFunc;
    if (GetFunc)
    {
        if (GetFunc(pClnkCtx, 0, 0, &PhyThresholdEnable) != SYS_SUCCESS)
        {
            printf("Get failed, ObjectIndex=%d\n", IF_PHY_THRESHOLD_ENABLE);
            exit(1);
        }
    }

    // trap needs to do GetPhyThreshold()
    PhyThreshold = 0;
    GetFunc = ObjectInfoTable[IF_PHY_THRESHOLD].GetFunc;
    if (GetFunc)
    {
        if (GetFunc(pClnkCtx, 0, 0, &PhyThreshold) != SYS_SUCCESS)
        {
            printf("Get failed, ObjectIndex=%d\n", IF_PHY_THRESHOLD);
            exit(1);
        }
    }

    pObjectDataVarbind->pOidString = g_ObjectVar.Str;
    if (PhyThresholdEnable && *pSmallestPhyRate >= PhyThreshold)
    {
        strcpy(pObjectDataVarbind->pOidString, "ifIndex");
    }
    else
    {
        pObjectDataVarbind->pOidString[0] = '\0';
    }
    g_ObjectVar.Uint = IF_INDEX;
    pObjectDataVarbind->pObjectData = &g_ObjectVar.Uint;

    return Status;
} // GetTrapAbovePhyThreshold()


/***************************************************************************//**
 * \brief   ToLowerString
 *
 * \param[out]  pString     Pointer to string to make lowercase.
 *
 * \return      None
 *
 * Convert string to lowercase.
 *
 * \note
 *
 ******************************************************************************/
static void ToLowerString(char* pString)
{
    if (pString != SYS_NULL)
    {
        SYS_UINT32 i;
        SYS_UINT32 Len = strlen(pString);
        for (i = 0; i < Len; i++)
        {
            pString[i] = tolower(pString[i]);
        }
    }
} // ToLowerString()


/***************************************************************************//**
 * \brief   StringToInt
 *
 * \param[in]   pString     Pointer to string of input data.
 * \param[in]   pInt        Pointer to integer for converted results.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Convert string to signed number and return it.
 *
 * \note
 *      Formats accepted:
 *          0
 *          1
 *          -1
 *          +1
 *          2147483647
 *          +2147483647
 *          -2147483648
 *          0x0
 *          0x9
 *          0xA
 *          0xF
 *          0x00000000
 *          0x7FFFFFFF
 *          0x80000000
 *          0x99999999
 *          0xAAAAAAAA
 *          0xFFFFFFFF
 *
 *      Errors possible:
 *          pString == SYS_NULL
 *          pString[0] == '\0'
 *          strlen(pHexString) < MinHexLen
 *          strlen(pHexString) > MaxHexLen
 *          !(HexDigit >= '0' && HexDigit <= '9') && !(HexDigit >= 'a' || HexDigit <= 'f')
 *          strlen(pDecimalString) < MinDecimalLen
 *          strlen(pDecimalString) > MaxDecimalLen
 *          DecimalDigit < '0' || DecimalDigit > '9')
 *
 ******************************************************************************/
static SYS_BOOLEAN StringToInt(SYS_CHAR * pString, SYS_INT32 * pInt)
{
    SYS_INT32 IntNum = 0;
    SYS_UINT32 Status = SYS_SUCCESS;
    SYS_UINT32 MinHexLen = sizeof("0x0") - 1;
    SYS_UINT32 MaxHexLen = sizeof("0xFFFFFFFF") - 1;
    SYS_UINT32 MinDecimalLen = sizeof("0") - 1;
    SYS_UINT32 MaxDecimalLen = sizeof("2147483647") - 1;

    if (pString == SYS_NULL || pString[0] == '\0')
    {
        IntNum = 0;
        Status = ~SYS_SUCCESS;
    }
    else
    {
        SYS_UINT32 i;
        SYS_UINT32 Len;
        SYS_INT32 Digit;

        // 0x0 and 0xFFFFFFFF
        if (pString[0] == '0' && pString[1] == 'x')
        {
            // hex number
            Len = strlen(pString);
            if (Len < MinHexLen || Len > MaxHexLen)
            {
                IntNum = 0;
                Status = ~SYS_SUCCESS;
            }
            else
            {
                // skip over "0x"
                pString += 2;
                Len -= 2;
                for (i = 0; i < Len; i++)
                {
                    // get next lowercase hex digit
                    Digit = tolower(*pString++);
                    if (Digit >= '0' && Digit <= '9')
                    {
                        IntNum = (IntNum << 4) | (Digit - '0');
                    }
                    else if (Digit >= 'a' || Digit <= 'f')
                    {
                        IntNum = (IntNum << 4) | (Digit - 'a' + 10);
                    }
                    else
                    {
                        IntNum = 0;
                        Status = ~SYS_SUCCESS;
                        break;
                    }
                }
            }
        }
        else
        {
            // signed number
            SYS_INT32 SignOfNum = 1;
            if (pString[0] == '-')
            {
                SignOfNum = -1;
                // skip over '-'
                pString++;
            }
            else if (pString[0] == '+')
            {
                // skip over '+'
                pString++;
            }

            // should be decimal digit
            Len = strlen(pString);
            if (Len < MinDecimalLen || Len > MaxDecimalLen)
            {
                IntNum = 0;
                Status = ~SYS_SUCCESS;
            }
            else
            {
                for (i = 0; i < Len; i++)
                {
                    Digit = *pString++;
                    if (Digit < '0' || Digit > '9')
                    {
                        IntNum = 0;
                        Status = ~SYS_SUCCESS;
                        break;
                    }
                    else
                    {
                        IntNum = IntNum * 10 + Digit - '0';
                    }
                }
            }
            IntNum *= SignOfNum;
        }
    }
    *pInt = IntNum;

    return Status;
} // StringToInt()


/***************************************************************************//**
 * \brief   ListInterfaces
 *
 * \param[in]   None
 *
 * \return      None
 *
 * List all the interface names.
 *
 * \note
 *  Errors possible:
 *      Error listing c.LINK devices.
 *      No c.LINK devices found.
 *
 ******************************************************************************/
static void ListInterfaces(void)
{
    ClnkDef_ZipList_t ZipList[MAX_INTERFACES];
    SYS_INT32 NumDevicesFound;
    SYS_INT32 i;

    NumDevicesFound = clnk_list_devices(ZipList, MAX_INTERFACES);
    if (NumDevicesFound < 0)
    {
        printf("Error listing c.LINK devices.\n");
        exit(1);
    }
    if (NumDevicesFound == 0)
    {
        printf("No c.LINK devices found.\n");
    }
    else
    {
        for (i = 0; i < NumDevicesFound; i++)
        {
            printf("%s\n", ZipList[i].ifname);
        }
    }
} // ListInterfaces()


/***************************************************************************//**
 * \brief   ListObjects
 *
 * \param[in]   None
 *
 * \return      None
 *
 * List all the Object Names along with R/W capabilities, type,
 * required indices, and units supported by this App.
 *
 * \note
 *
 ******************************************************************************/
static void ListObjects(void)
{
    SYS_UINT32 ObjectIndex;
    SYS_UINT32 i;
    SYS_CHAR * pReadWriteCaps;
    SYS_CHAR * pType;
    SYS_CHAR * pIndices;
    SYS_UINT32 Len;
    SYS_CHAR Spaces[MAX_OBJECT_NAME_LEN + 1] = "";
    SYS_UINT32 LongestNameLen = 0;

    // calc longest Name length in ObjectInfoTable
    for (ObjectIndex = 0; ObjectIndex < NUM_OBJECT_INFO_TABLE_ENTRIES; ObjectIndex++)
    {
        Len = strlen(ObjectInfoTable[ObjectIndex].Name);
        if (Len > LongestNameLen)
        {
            LongestNameLen = Len;
        }
    }

    printf("%s supported Objects:\n\n", g_CmdLineVars.ppArgv[0]);

    // print header line
    printf("    ObjectName");
    for (i = 0; i < LongestNameLen - (sizeof("ObjectName") - 1); i++)
    {
        printf(" ");
    }
    printf(" Req Type    Opt   Units\n");

    // print dashed line
    printf("    ");
    for (i = 0; i < LongestNameLen; i++)
    {
        printf("-");
    }
    printf(" --- ------- ----- -------\n");

    for (ObjectIndex = 0; ObjectIndex < NUM_OBJECT_INFO_TABLE_ENTRIES; ObjectIndex++)
    {
        // determine num spaces needed behind Name to align columns
        Len = strlen(ObjectInfoTable[ObjectIndex].Name);
        for (i = 0; i < LongestNameLen - Len; i++)
        {
            Spaces[i] = ' ';
        }
        Spaces[i] = '\0';

        // get Read Write capabilities string
        if (ObjectInfoTable[ObjectIndex].GetFunc == SYS_NULL)
        {
            if (ObjectInfoTable[ObjectIndex].SetFunc == SYS_NULL)
            {
                pReadWriteCaps = "   ";
            }
            else
            {
                pReadWriteCaps = "WO ";
            }
        }
        else
        {
            if (ObjectInfoTable[ObjectIndex].SetFunc == SYS_NULL)
            {
                pReadWriteCaps = "RO ";
            }
            else
            {
                pReadWriteCaps = "W/R";
            }
        }

        // get Type string
        switch(ObjectInfoTable[ObjectIndex].Type)
        {
        case BOOL_:
            pType = "BOOL   ";
            break;
        case STR_:
            pType = "STR    ";
            break;
        case UINT32_:
            pType = "UINT32 ";
            break;
        case INT32_:
            pType = "INT32  ";
            break;
        case VARBIND_:
            pType = "VARBIND";
            break;
        default:
            break;
        }

        // get required indices string
        switch(ObjectInfoTable[ObjectIndex].Args)
        {
        case NO_ARGS:
            pIndices = "     ";
            break;
        case F_ARGS:
            pIndices = "-f   ";
            break;
        case N_ARGS:
            pIndices = "-n   ";
            break;
        case TR_ARGS:
            pIndices = "-t -r";
            break;
        default:
            break;
        }

        printf("    %s %s%s %s %s %s\n", ObjectInfoTable[ObjectIndex].Name, Spaces,
               pReadWriteCaps, pType, pIndices, ObjectInfoTable[ObjectIndex].Units);
    }
} // ListObjects()


/***************************************************************************//**
 * \brief   InitObjectDefaults
 *
 * \param[in]   None
 *
 * \return      None
 *
 * Assign default values to all objects that don't already have a previously assigned value
 * and that require the previously assigned value to be kept around persistently.
 * The FILENAME persistent file contains object names and the object's previously assigned
 * values for all objects that require the previously assigned value to be kept around
 * persistently.
 * If an object has a default in the ObjectInfoTable, then that object is required to have
 * the previously assigned value kept around persistently.
 * This function handles the case when there is no FILENAME file or when the FILENAME file
 * is missing some objects or when the FILENAME file has objects that should not be in file.
 * This function cleans up the FILENAME file adding new objects and default values or
 * (in rare cases) deleting objects that no longer exist in the MMA.
 * This function should not affect the FILENAME file 99% of the time.
 *
 * \note
 *
 ******************************************************************************/
static void InitObjectDefaults(void)
{
    SYS_UINT32 i;
    SYS_UINT32 ObjectIndex;
    FILE * pFileRead;
    FILE * pFileWrite;
    SYS_BOOLEAN ObjectWrittenTable[NUM_OBJECT_INFO_TABLE_ENTRIES];
    SYS_BOOLEAN ChangedFile = SYS_FALSE;
    SYS_CHAR Line[MAX_LINE_LEN + 1];

    // initialize table to no objects have been written yet to FILENAMEBACKUP file
    for (i = 0; i < NUM_OBJECT_INFO_TABLE_ENTRIES; i++)
    {
        ObjectWrittenTable[i] = SYS_FALSE;
    }

    // Open the FILENAMEBACKUP file for write.
    // Truncate to zero length or create file.
    if (pFileWrite = fopen(FILENAMEBACKUP, "w"))
    {
        // There is no guarantee that the FILENAME file exists or whether the file has all
        //      the Objects that require defaults.
        if (pFileRead = fopen(FILENAME, "r"))
        {
            // FILENAME file exists
            for (;;)
            {
                SYS_UINT32 Len;

                if (fgets(Line, MAX_LINE_LEN + 1, pFileRead) == SYS_NULL)
                {
                    // EOF reached
                    break;
                }

                Len = strlen(Line);
                // look for space char in line separating ObjectName and ObjectValueString
                for (i = 0; i < Len; i++)
                {
                    if (Line[i] == ' ')
                    {
                        // found space char
                        break;
                    }
                }

                // space char not found in Line
                if (i == Len)
                {
                    // Throw the line out if we can't find a space char in Line separating
                    // the ObjectName and the ObjectValueString.
                }
                else
                {
                    // does Line end in something other than '\n'?
                    if (Line[Len - 1] != '\n')
                    {
                        // make sure you are in bounds
                        if (Len < MAX_LINE_LEN)
                        {
                            // Line must end in '\n'
                            Line[Len] = '\n';
                            Line[Len + 1] = '\0';
                        }
                    }

                    // make Line contain only ObjectName
                    Line[i] = '\0';

                    // find LineObjectName in ObjectInfoTable
                    for (ObjectIndex = 0; ObjectIndex < NUM_OBJECT_INFO_TABLE_ENTRIES;
                         ObjectIndex++)
                    {
                        // does LineObjectName match ObjectInfoTable[].Name?
                        if (strcmp(Line, ObjectInfoTable[ObjectIndex].Name) == 0)
                        {
                            // matches
                            break;
                        }
                    }

                    // LineObjectName does not match any of the ObjectInfoTable[].Name's
                    if (ObjectIndex == NUM_OBJECT_INFO_TABLE_ENTRIES)
                    {
                        // Throw Line out if we can't find LineObjectName in ObjectInfoTable
                    }
                    else
                    {
                        // has the object already been written to FILENAMEBACKUP file?
                        if (ObjectWrittenTable[ObjectIndex])
                        {
                            // Throw the line out if object already been written to
                            //      FILENAMEBACKUP file.
                        }
                        else
                        {
                            // does the object NOT have a default value in ObjectInfoTable?
                            if (ObjectInfoTable[ObjectIndex].DefaultValueStr[0] == '\0')
                            {
                                // Throw the line out if object does NOT have a default value
                                // in ObjectInfoTable.
                            }
                            else
                            {
                                // We can only get here if FILENAME file exists and
                                // the Line has a space and
                                // LineObjectName matches one of the ObjectInfoTable[].Name's
                                // and object NOT already written to FILENAMEBACKUP file and
                                // object does have a default value in ObjectInfoTable

                                // make Line contain LineObjectName and ObjectValueString
                                Line[i] = ' ';

                                // finally, we can write the LineObjectName and
                                //      ObjectValueString to the FILENAMEBACKUP file
                                fputs(Line, pFileWrite);

                                ObjectWrittenTable[ObjectIndex] = SYS_TRUE;
                            }
                        }
                    }
                }
            }
            // FILENAME file EOF reached

            // Any remaining unwritten objects that have default values need to have their
            // ObjectName and ObjectDefaultValueStr written to FILENAMEBACKUP file.
            for (ObjectIndex = 0; ObjectIndex < NUM_OBJECT_INFO_TABLE_ENTRIES;
                 ObjectIndex++)
            {
                // does the object have a default value in ObjectInfoTable?
                if (ObjectInfoTable[ObjectIndex].DefaultValueStr[0] != '\0')
                {
                    // has the object NOT been written to FILENAMEBACKUP file?
                    if (!ObjectWrittenTable[ObjectIndex])
                    {
                        sprintf(Line, "%s %s\n", ObjectInfoTable[ObjectIndex].Name,
                                ObjectInfoTable[ObjectIndex].DefaultValueStr);
                        fputs(Line, pFileWrite);

                        ObjectWrittenTable[ObjectIndex] = SYS_TRUE;

                        // Note the file has been changed
                        ChangedFile = SYS_TRUE;
                    }
                }
            }

            fclose(pFileRead);
        }
        else
        {
            // FILENAME file does not exist

            // All objects that have default values need to have their
            // ObjectName and ObjectDefaultValueStr written to FILENAMEBACKUP file.
            for (ObjectIndex = 0; ObjectIndex < NUM_OBJECT_INFO_TABLE_ENTRIES;
                 ObjectIndex++)
            {
                // does the object have a default value in ObjectInfoTable?
                if (ObjectInfoTable[ObjectIndex].DefaultValueStr[0] != '\0')
                {
                    sprintf(Line, "%s %s\n", ObjectInfoTable[ObjectIndex].Name,
                            ObjectInfoTable[ObjectIndex].DefaultValueStr);
                    fputs(Line, pFileWrite);

                    // Note the file has been changed
                    ChangedFile = SYS_TRUE;
                }
            }
        }
        fclose(pFileWrite);
    }

    // Only want to keep FILENAMEBACKUP if file changed
    if (ChangedFile)
    {
        remove(FILENAME);
        rename(FILENAMEBACKUP, FILENAME);
        system("cp " FILENAME " " FILENAMEPROCTAGS);  // cp /etc/mocamib.conf /proc/tags/MMAF
    }
} // InitObjectDefaults()


/***************************************************************************//**
 * \brief   RestoreObject
 *
 * \param[in]   ObjectIndex     Index into the ObjectInfoTable.
 * \param[out]  pObjectData     Pointer to Object's data to restore.
 *
 * \return      None
 *
 * Restore object's data from FILENAME file.
 *
 * \note
 *      Format of FILENAME file is a series of lines like:
 *          ObjectName value
 *      where value can be data of type Boolean, Integer, Unsigned, String or VarBind.
 *
 ******************************************************************************/
static void RestoreObject(SYS_UINT32 ObjectIndex, void *pObjectData)
{
    FILE * pFileRead;

     // the InitObjectDefaults() function guarantees that this file will exist
     if (pFileRead = fopen(FILENAME, "r"))
     {
        // FILENAME file exists
        for (;;)
        {
            SYS_UINT32 i;
            SYS_UINT32 Len;
            SYS_CHAR Line[MAX_LINE_LEN + 1];

            if (fgets(Line, MAX_LINE_LEN + 1, pFileRead) == SYS_NULL)
            {
                // EOF reached
                break;
            }

            Len = strlen(Line);
            // look for space char in line separating ObjectName and ObjectValueString
            for (i = 0; i < Len; i++)
            {
                if (Line[i] == ' ')
                {
                    // found space char
                    break;
                }
            }

            // Space char found in Line.
            if (i != Len)
            {
                // make Line contain only ObjectName
                Line[i] = '\0';

                // does LineObjectName match ObjectInfoTable[ObjectIndex].Name?
                if (strcmp(Line, ObjectInfoTable[ObjectIndex].Name) == 0)
                {
                    // after the space char is the ObjectValueString
                    SYS_CHAR * pObjectValueString  = &Line[i + 1];      // already has "\n"

                    // We expect to have '\n' at end of line.
                    // Remove '\n' at end of line.
                    if (Line[Len - 1] == '\n')
                    {
                        Line[Len - 1] = '\0';
                    }

                    // Need to update pObjectData with latest data
                    //      BOOL, STR, UINT32, or INT32 from from ObjectValueString.
                    switch (ObjectInfoTable[ObjectIndex].Type)
                    {
                    case BOOL_:
                        {
                            // make g_CmdLineVars.SetObjectValueString lowercase
                            SYS_BOOLEAN * const pObjectDataBoolean = pObjectData;
                            ToLowerString(pObjectValueString);
                            if (strcmp(pObjectValueString, "true") == 0 ||
                                strcmp(pObjectValueString, "t") == 0)
                            {
                                *pObjectDataBoolean = SYS_TRUE;
                            }
                            else if (strcmp(pObjectValueString, "false") == 0 ||
                                     strcmp(pObjectValueString, "f") == 0)
                            {
                                *pObjectDataBoolean = SYS_FALSE;
                            }
                            else
                            {
                                printf("Invalid Boolean value in %s.\n", FILENAME);
                                Usage(g_CmdLineVars.ppArgv);
                            }
                            break;
                        }
                    case STR_:
                        {
                            SYS_CHAR * const pObjectDataString = pObjectData;
                            strcpy(pObjectDataString, pObjectValueString);
                            break;
                        }
                    case UINT32_:
                        {
                            SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
                            if (pObjectValueString[0] == '-' ||
                                StringToInt(pObjectValueString,
                                            pObjectDataUnsigned) != SYS_SUCCESS)
                            {
                                printf("Invalid Unsigned value in %s.\n", FILENAME);
                                Usage(g_CmdLineVars.ppArgv);
                            }
                            break;
                        }
                    case INT32_:
                        {
                            SYS_INT32 * const pObjectDataInteger = pObjectData;
                            if (StringToInt(pObjectValueString,
                                            pObjectDataInteger) != SYS_SUCCESS)
                            {
                                printf("Invalid Integer value in %s.\n", FILENAME);
                                Usage(g_CmdLineVars.ppArgv);
                            }
                            break;
                        }
                    case VARBIND_:
                    default:
                        {
                            printf("Internal Error: Invalid ObjectInfoTable[%d].Type=%d\n",
                                   ObjectIndex, ObjectInfoTable[ObjectIndex].Type);
                            exit(1);
                        }
                    }
                    // We can only get here if FILENAME file exists and
                    // the Line has a space and
                    // LineObjectName matches ObjectInfoTable[ObjectIndex].Name?

                    // We've got the data we wanted from the FILENAME file.
                    // No need to read any more of the FILENAME file.
                    break;
                }
                else
                {
                }
            }
        }
        fclose(pFileRead);
    }
} // RestoreObject()


/***************************************************************************//**
 * \brief   SaveObject
 *
 * \param[in]   ObjectIndex     Index into the ObjectInfoTable.
 * \param[in]   pObjectData     Pointer to Object's data to save.
 *
 * \return      None
 *
 * Save object's data in FILENAME file and make it persistent.
 *
 * \note
 *      Format of FILENAME file is a series of lines like:
 *          ObjectName value
 *      where value can be data of type Boolean, Integer, Unsigned, String or VarBind.
 *
 *      Currently, only PhyThreshold and PhyThresholdEnable objects require saving.
 *
 *      Example of FILENAME file:
 *      PhyThreshold 200
 *      PhyThresholdEnable FALSE
 *
 ******************************************************************************/
static void SaveObject(SYS_UINT32 ObjectIndex, void *pObjectData)
{
    FILE * pFileRead;
    FILE * pFileWrite;

    // Open the FILENAMEBACKUP file for write.
    // Truncate to zero length or create file.
    if (pFileWrite = fopen(FILENAMEBACKUP, "w"))
    {
        // the InitObjectDefaults() function guarantees that this file will exist
         if (pFileRead = fopen(FILENAME, "r"))
         {
            // FILENAME file exists
            for (;;)
            {
                SYS_UINT32 i;
                SYS_UINT32 Len;
                SYS_CHAR Line[MAX_LINE_LEN + 1];

                if (fgets(Line, MAX_LINE_LEN + 1, pFileRead) == SYS_NULL)
                {
                    // EOF reached
                    break;
                }

                Len = strlen(Line);
                // look for space char in line separating ObjectName and ObjectValueString
                for (i = 0; i < Len; i++)
                {
                    if (Line[i] == ' ')
                    {
                        // found space char
                        break;
                    }
                }

                // Space char found in Line.
                if (i != Len)
                {
                    // make Line contain only ObjectName
                    Line[i] = '\0';

                    // does LineObjectName match ObjectInfoTable[ObjectIndex].Name?
                    if (strcmp(Line, ObjectInfoTable[ObjectIndex].Name) == 0)
                    {
                        // after the space char is the ObjectValueString
                        SYS_CHAR * pObjectValueString = &Line[i + 1];     // already has "\n"

                        // We expect to have '\n' at end of line.
                        // Remove '\n' at end of line.
                        if (Line[Len - 1] == '\n')
                        {
                            Line[Len - 1] = '\0';
                        }

                        // Need to update ObjectValueString with latest data
                        //      BOOL, STR, UINT32, or INT32 from from pObjectData.
                        switch (ObjectInfoTable[ObjectIndex].Type)
                        {
                        case BOOL_:
                            {
                                SYS_BOOLEAN * const pObjectDataBoolean = pObjectData;
                                if (*pObjectDataBoolean)
                                {
                                    strcpy(pObjectValueString, "TRUE\n");
                                }
                                else
                                {
                                    strcpy(pObjectValueString, "FALSE\n");
                                }
                                break;
                            }
                        case STR_:
                            {
                                SYS_CHAR * const pObjectDataString = pObjectData;
                                strcpy(pObjectValueString, pObjectDataString);
                                strcat(pObjectValueString, "\n");
                                break;
                            }
                        case UINT32_:
                            {
                                SYS_UINT32 * const pObjectDataUnsigned = pObjectData;
                                sprintf(pObjectValueString, "%u\n", *pObjectDataUnsigned);
                                break;
                            }
                        case INT32_:
                            {
                                SYS_INT32 * const pObjectDataInteger = pObjectData;
                                sprintf(pObjectValueString, "%d\n", *pObjectDataInteger);
                                break;
                            }
                        case VARBIND_:
                        default:
                            printf("Internal Error: Invalid ObjectInfoTable[%d].Type=%d\n",
                                   ObjectIndex, ObjectInfoTable[ObjectIndex].Type);
                            exit(1);
                        }
                        // We can only get here if FILENAME file exists and
                        // the Line has a space and
                        // LineObjectName matches ObjectInfoTable[ObjectIndex].Name?
                    }
                    else
                    {
                    }

                    // make line contain LineObjectName and ObjectValueString
                    Line[i] = ' ';

                    // Finally, we can write the LineObjectName and ObjectValueString to
                    //      the FILENAMEBACKUP file.
                    // This will write either the original ObjectValueString or
                    //      the modified ObjectValueString,
                    //      depending if the LineObjectName matches the ObjectName
                    //      specified in the command line.
                    fputs(Line, pFileWrite);
                }
            }
            fclose(pFileRead);
        }
        fclose(pFileWrite);
    }

    remove(FILENAME);
    rename(FILENAMEBACKUP, FILENAME);
    system("cp " FILENAME " " FILENAMEPROCTAGS);  // cp /etc/mocamib.conf /proc/tags/MMAF
} // SaveObject()


/***************************************************************************//**
 * \brief   LookupObjectName
 *
 * \param[in]   ObjectName      Object Name.
 * \param[out]  pObjectIndex    Pointer to ObjectIndex into the ObjectInfoTable
 *                              that ObjectName was found.
 *
 * \return      SYS_SUCCESS or ~SYS_SUCCESS
 *
 * Lookup the ObjectName in the ObjectInfoTable and return the index.
 * Return ~SYS_SUCCESS if not found.
 *
 * \note
 *
 ******************************************************************************/
static SYS_UINT32 LookupObjectName(SYS_CHAR * pObjectName, SYS_UINT32 * pObjectIndex)
{
    SYS_UINT32 i;
    SYS_UINT32 Status = SYS_SUCCESS;

    // make ObjectName lowercase
    ToLowerString(pObjectName);

    // lookup ObjectName in ObjectInfoTable and return it's index in ObjectIndex
    for (i = 0; i < NUM_OBJECT_INFO_TABLE_ENTRIES; i++)
    {
        SYS_CHAR LowerObjectNameEntry[MAX_OBJECT_NAME_LEN + 1 + MOCA_IF_STR_LEN];
        strcpy(&LowerObjectNameEntry[MOCA_IF_STR_LEN], ObjectInfoTable[i].Name);
        ToLowerString(&LowerObjectNameEntry[MOCA_IF_STR_LEN]);
        if (strcmp(&LowerObjectNameEntry[MOCA_IF_STR_LEN], pObjectName) == 0)
        {
            *pObjectIndex = i;
            break;
        }

        // look for "moca" + ObjectName
        memcpy(&LowerObjectNameEntry[MOCA_IF_STR_LEN - MOCA_STR_LEN],    // [2]
               MOCA_STR, MOCA_STR_LEN);
        // [2]
        if (strcmp(&LowerObjectNameEntry[MOCA_IF_STR_LEN - MOCA_STR_LEN], pObjectName) == 0)
        {
            *pObjectIndex = i;
            break;
        }

        // look for "mocaIf" + ObjectName
        memcpy(&LowerObjectNameEntry[MOCA_IF_STR_LEN - MOCA_IF_STR_LEN],  // [0]
               MOCA_IF_STR, MOCA_IF_STR_LEN);
        // [0]
        if (strcmp(&LowerObjectNameEntry[MOCA_IF_STR_LEN - MOCA_IF_STR_LEN], pObjectName) ==
            0)
        {
            *pObjectIndex = i;
            break;
        }
    }

    // unable to find ObjectName in ObjectInfoTable
    if (i == NUM_OBJECT_INFO_TABLE_ENTRIES)
    {
        *pObjectIndex = 0;
        Status = ~SYS_SUCCESS;
    }
    return Status;
} // LookupObjectName()


/***************************************************************************//**
 * \brief   ProcessEachGetSet
 *
 * \param[in]   pClnkCtx        Pointer to c.LINK Library context.
 * \param[in]   ObjectIndex     Index into the ObjectInfoTable.
 *
 * \return      None
 *
 * This function provides the engine behind this table driven App.
 * Do the processing that needs to get done for just one Get/Set.
 *
 * \note
 *
 ******************************************************************************/
static void ProcessEachGetSet(clnk_ctx_t *pClnkCtx, SYS_UINT32 ObjectIndex)
{
    SYS_INT32   Index1;
    SYS_INT32   Index2;
    void *      pObjectData = SYS_NULL;
    GetFunc_t   GetFunc = ObjectInfoTable[ObjectIndex].GetFunc;
    SetFunc_t   SetFunc = ObjectInfoTable[ObjectIndex].SetFunc;

    // For Set only.
    if (g_CmdLineVars.GotSetObject)
    {
        SYS_UINT32 Status;
        // Need to convert the g_CmdLineVars.SetObjectValueString into
        //      BOOL, STR, UINT32, or INT32.
        switch (ObjectInfoTable[ObjectIndex].Type)
        {
        case BOOL_:
            // make g_CmdLineVars.SetObjectValueString lowercase
            ToLowerString(g_CmdLineVars.SetObjectValueString);
            if (strcmp(g_CmdLineVars.SetObjectValueString, "true") == 0 ||
                strcmp(g_CmdLineVars.SetObjectValueString, "t") == 0)
            {
                g_ObjectVar.Bool = SYS_TRUE;
            }
            else if (strcmp(g_CmdLineVars.SetObjectValueString, "false") == 0 ||
                     strcmp(g_CmdLineVars.SetObjectValueString, "f") == 0)
            {
                g_ObjectVar.Bool = SYS_FALSE;
            }
            else
            {
                printf("Invalid Boolean value specified in Set.\n");
                Usage(g_CmdLineVars.ppArgv);
            }
            break;
        case STR_:
            strcpy(g_ObjectVar.Str, g_CmdLineVars.SetObjectValueString);
            break;
        case UINT32_:
            if (g_CmdLineVars.SetObjectValueString[0] == '-' ||
                StringToInt(g_CmdLineVars.SetObjectValueString,
                            (SYS_INT32 *)&g_ObjectVar.Uint) != SYS_SUCCESS)
            {
                printf("Invalid Unsigned value specified in Set.\n");
                Usage(g_CmdLineVars.ppArgv);
            }
            break;
        case INT32_:
            if (StringToInt(g_CmdLineVars.SetObjectValueString,
                            &g_ObjectVar.Int) != SYS_SUCCESS)
            {
                printf("Invalid Integer value specified in Set.\n");
                Usage(g_CmdLineVars.ppArgv);
            }
            break;
        case VARBIND_:
        default:
            printf("Internal Error: Invalid ObjectInfoTable[%d].Type=%d\n",
                   ObjectIndex, ObjectInfoTable[ObjectIndex].Type);
            exit(1);
        }
    }

    // Set pObjectData to point to the Data.
    // Do this for Get or Set.
    switch (ObjectInfoTable[ObjectIndex].Type)
    {
    case BOOL_:
        pObjectData = &g_ObjectVar.Bool;
        break;
    case STR_:
        pObjectData = &g_ObjectVar.Str;
        break;
    case UINT32_:
        pObjectData = &g_ObjectVar.Uint;
        break;
    case INT32_:
        pObjectData = &g_ObjectVar.Int;
        break;
    case VARBIND_:
        pObjectData = &g_ObjectVar.VarBind;
        break;
    default:
        printf("Internal Error: Invalid ObjectInfoTable[%d].Type=%d\n",
               ObjectIndex, ObjectInfoTable[ObjectIndex].Type);
        exit(1);
    }

    // setup indices required for the Set or Get call
    switch (ObjectInfoTable[ObjectIndex].Args)
    {
    case NO_ARGS:
        if (g_CmdLineVars.GotFlowIndex || g_CmdLineVars.GotNodeIndex ||
            g_CmdLineVars.GotMeshTxNodeIndex || g_CmdLineVars.GotMeshRxNodeIndex)
        {
            printf("This option not allowed with this object.\n");
            Usage(g_CmdLineVars.ppArgv);
        }
        Index1 = 0;
        Index2 = 0;
        break;
    case F_ARGS:
        if (g_CmdLineVars.GotNodeIndex ||
            g_CmdLineVars.GotMeshTxNodeIndex || g_CmdLineVars.GotMeshRxNodeIndex)
        {
            printf("This option not allowed with this object.\n");
            Usage(g_CmdLineVars.ppArgv);
        }
        if (!g_CmdLineVars.GotFlowIndex)
        {
            printf("FlowIndex option required.\n");
            Usage(g_CmdLineVars.ppArgv);
        }
        Index1 = g_CmdLineVars.FlowIndex;
        Index2 = 0;
        break;
    case N_ARGS:
        if (g_CmdLineVars.GotFlowIndex ||
            g_CmdLineVars.GotMeshTxNodeIndex || g_CmdLineVars.GotMeshRxNodeIndex)
        {
            printf("This option not allowed with this object.\n");
            Usage(g_CmdLineVars.ppArgv);
        }
        if (!g_CmdLineVars.GotNodeIndex)
        {
            printf("NodeIndex option required.\n");
            Usage(g_CmdLineVars.ppArgv);
        }
        Index1 = g_CmdLineVars.NodeIndex;
        Index2 = 0;
        break;
    case TR_ARGS:
        if (g_CmdLineVars.GotFlowIndex || g_CmdLineVars.GotNodeIndex)
        {
            printf("This option not allowed with this object.\n");
            Usage(g_CmdLineVars.ppArgv);
        }
        if (!g_CmdLineVars.GotMeshTxNodeIndex)
        {
            printf("MeshTxNodeIndex option required.\n");
            Usage(g_CmdLineVars.ppArgv);
        }
        if (!g_CmdLineVars.GotMeshRxNodeIndex)
        {
            printf("MeshRxNodeIndex option required.\n");
            Usage(g_CmdLineVars.ppArgv);
        }
        Index1 = g_CmdLineVars.MeshTxNodeIndex;
        Index2 = g_CmdLineVars.MeshRxNodeIndex;
        break;
    default:
        printf("Internal Error: Invalid ObjectInfoTable[%d].Args=%d\n",
               ObjectIndex, ObjectInfoTable[ObjectIndex].Args);
        exit(1);
    }

    // If Get then call Get for this object.
    // This includes AllGet.
    if (g_CmdLineVars.GotGetObject)
    {
        SYS_UINT32 Index1ReturnValue;
        SYS_UINT32 Index2ReturnValue;

        // special processing required for traps
        switch (ObjectIndex)
        {
        case IF_TRAP_BELOW_PHY_THRESHOLD:
        case IF_TRAP_ABOVE_PHY_THRESHOLD:
            {
                // these traps are going to force a GetMeshTxRate() call
                //      to update the trap data
                GetFunc_t GetFunc = ObjectInfoTable[IF_MESH_TX_RATE].GetFunc;
                if (GetFunc)
                {
                    Index1 = (SYS_UINT32)&Index1ReturnValue;
                    Index2 = (SYS_UINT32)&Index2ReturnValue;
                    // Call the GetMeshTxRate() function to update the trap data.
                    // This can be optimized so you don't call GetMeshTxRate() for each trap.
                    // We want it to return the MeshTxNodeIndex and MeshRxNodeIndex of the
                    // lowest MeshRate in the
                    // MeshRateTable[MeshTxNodeIndex][MeshRxNodeIndex].
                    // We also want it to return the lowest MeshRate in the
                    // MeshRateTable[MeshTxNodeIndex][MeshRxNodeIndex].
                    // We just want GetMeshTxRate() to update the g_ObjectVar.Uint
                    // for traps.
                    if (GetFunc(pClnkCtx, (SYS_UINT32)&Index1ReturnValue,
                                (SYS_UINT32)&Index2ReturnValue, &g_ObjectVar.Uint) !=
                        SYS_SUCCESS)
                    {
                        printf("Get failed, ObjectIndex=%d\n", IF_MESH_TX_RATE);
                        exit(1);
                    }
                    Index1 = Index1ReturnValue;
                    Index2 = Index2ReturnValue;
                }
                break;
            }
        default:
            break;
        }

        // make sure function pointer is valid
        if (GetFunc)
        {
            // call the Get function for this object to get the ObjectData
            if (GetFunc(pClnkCtx, Index1, Index2, pObjectData) != SYS_SUCCESS)
            {
                printf("Get failed, ObjectIndex=%d\n", ObjectIndex);
                exit(1);
            }
        }
        else
        {
            printf("Get not valid for this Object.\n");
            Usage(g_CmdLineVars.ppArgv);
        }
    }
    // if Set then call Set for this object
    else if (g_CmdLineVars.GotSetObject)
    {
        // make sure function pointer is valid
        if (SetFunc)
        {
            // call the Set function for this object to set the ObjectData
            if (SetFunc(pClnkCtx, Index1, Index2, pObjectData) != SYS_SUCCESS)
            {
                printf("Set failed, ObjectIndex=%d\n", ObjectIndex);
                exit(1);
            }
        }
        else
        {
            printf("Set not valid for this Object.\n");
            Usage(g_CmdLineVars.ppArgv);
        }
    }

    // If Get then Display result.
    if (g_CmdLineVars.GotGetObject)
    {
        DisplayObject(ObjectIndex, pObjectData);
    }
} // ProcessEachGetSet()


/***************************************************************************//**
 * \brief   GetAllObjects
 *
 * \param[in]   pClnkCtx        Pointer to c.LINK Library context.
 *
 * \return      None
 *
 * Determine which Get objects don't require args and call ProcessEachGetSet() function for
 * each of those objects.
 * This will call ProcessEachGetSet() many times.
 *
 * \note
 *
 ******************************************************************************/
static void GetAllObjects(clnk_ctx_t *pClnkCtx)
{
    SYS_UINT32 ObjectIndex;

    for (ObjectIndex = 0; ObjectIndex < NUM_OBJECT_INFO_TABLE_ENTRIES; ObjectIndex++)
    {
        if (ObjectInfoTable[ObjectIndex].GetFunc != SYS_NULL &&
            ObjectInfoTable[ObjectIndex].Args == NO_ARGS)
        {
            ProcessEachGetSet(pClnkCtx, ObjectIndex);
        }
    }
} // GetAllObjects()


/***************************************************************************//**
 * \brief   main (Top Level MMA)
 *
 * \param[in]   argc        Argument count including mocamib.
 * \param[in]   argv        Pointer to array of pointers that each point to the
 *                          command line argument strings including mocamib.
 *
 * \return      0 for success, 1 for fail
 *
 * This function calls all the high level functions to run this App.
 *
 * \note
 *
 ******************************************************************************/
int main(int argc, char** argv)
{
    SYS_INT32   err;
    clnk_ctx_t  *pClnkCtx;

    // ProcessCmdLine is the first thing we do!
    ProcessCmdLine(argc, argv);

    if (NUM_OBJECT_INFO_TABLE_ENTRIES != NUM_OBJECTS)
    {
        printf("Internal Error: NUM_OBJECT_INFO_TABLE_ENTRIES != NUM_OBJECTS\n");
        exit(1);
    }

    // Initialize connection to driver.
    // Initializes the clink library context.
    // Attempts to establish communication with the clink device on interface iface.
    // If iface is NULL and there is only one clink device in the system,
    // it will be chosen by default.
    err = clnk_init(g_CmdLineVars.pIfaceStr, &pClnkCtx);
    switch (err)
    {
    case RET_GOOD:
        break;
    case RET_WHICHDEV:
        // multiple devices found
        printf("\nMultiple c.LINK devices exist.\n");
        printf("Use %s -L to list them, and %s -i to pick one.\n", argv[0], argv[0]);
        clnk_destroy(pClnkCtx);
        exit(1);
    case RET_NODEV:
    default:
        printf("\nError finding c.LINK device.\n");
        clnk_destroy(pClnkCtx);
        exit(1);
    }

    // if -a option specified then Get on all objects
    if (g_CmdLineVars.GotAllGetObjects)
    {
        // Initialize the default values for all objects that require defaults and write
        //      those object names and defaults to FILENAME if the object name is not
        //      already there.
        InitObjectDefaults();

        // Do a ProcessEachGetSet() for all Get objects that don't require args.
        // This will call ProcessEachGetSet() many times.
        GetAllObjects(pClnkCtx);
    }
    else
    {
        SYS_UINT32  ObjectIndex = 0;

        // lookup ObjectName in ObjectInfoTable and return ObjectIndex
        if (LookupObjectName(g_CmdLineVars.ObjectName, &ObjectIndex) != SYS_SUCCESS)
        {
            // can't find ObjectName or "moca" + ObjectName or "mocaif" + ObjectName
            //      in ObjectInfoTable
            printf("Unrecognized ObjectName.  Use -l to list %s supported objects.\n",
                   argv[0]);
            Usage(argv);
        }

        // does this object have a default value or does any other object depend on
        // an object that has a default value?
        if (ObjectInfoTable[ObjectIndex].DefaultValueStr[0] != '\0' ||
            ObjectIndex == IF_RF_CHANNEL ||                 // calls GetEnable()
            ObjectIndex == IF_TRAP_BELOW_PHY_THRESHOLD ||   // calls GetPhyThresholdEnable()
                                                            // and GetPhyThreshold()
            ObjectIndex == IF_TRAP_ABOVE_PHY_THRESHOLD)     // calls GetPhyThresholdEnable()
                                                            // and GetPhyThreshold()
        {
            // Initialize the default values for all objects that require defaults and write
            //      those object names and defaults to FILENAME if the object name is not
            //      already there.
            InitObjectDefaults();
        }
        // Do the processing that needs to get done for just one Get/Set.
        ProcessEachGetSet(pClnkCtx, ObjectIndex);
    }

    // Free the opened driver descriptor and resources.
    clnk_destroy(pClnkCtx);

    // The DisplayObject() is called at the very end of ProcessEachGetSet().
    // It can't be called at the end of main() because of the GetAllObjects() feature
    // which does a Get...() and DisplayObject() for each Getable Object that does not require
    // any indices.

    // That's all folks!
    exit(0);
} // main()


/* End of File: mocamib.c */

