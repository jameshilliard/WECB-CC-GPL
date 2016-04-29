/**
 * \file ClnkNms.c
 * \brief Implements the Entropic c.LINK C API
 */
/*******************************************************************************
* This file is licensed under the terms of your license agreement(s) with      *
* Entropic covering this file. Redistribution, except as permitted by the      *
* terms of your license agreement(s) with Entropic, is strictly prohibited.    *
*******************************************************************************/

#include "ClnkCtl.h"
#include "ClnkNms.h"
#include "clnkdvrapi.h"

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <net/if.h>
#include <netinet/in.h>
#include <ctype.h>
#include <linux/sockios.h>
#include "qos.h"
#include "mrt.h"

#define BITS_PER_WORD       32
#define BITS_PER_NIBBLE     4
#define NIBBLES_PER_WORD    (BITS_PER_WORD / BITS_PER_NIBBLE)           // 8
#define NIBBLES_PER_PROFILE 256                                         // Modulation Profile, see MoCA Spec
#define WORDS_PER_PROFILE   (NIBBLES_PER_PROFILE / NIBBLES_PER_WORD)    // 32

/* 1 argument (typically either "clear stats" or "node ID"); some output struct */
#define CLNK_CTL_GET_1(ctx, cmd, out_type, outv, inv) \
    { \
        struct clnk_io io; \
        if (outv == NULL) \
            return RET_ERR; \
        io.in = (SYS_UINT32 *)(SYS_UINTPTR) inv; \
        io.in_len = 0; \
        io.out = (SYS_UINT32 *) outv; \
        io.out_len = sizeof(out_type); \
        return(clnk_ctl_passthru_sock(ctx, cmd, &io)); \
    }
/* null input argument ; some output struct */
#define CLNK_CTL_GET_0(ctx, cmd, out_type, outv) \
    { \
        struct clnk_io io; \
        if (outv == NULL) \
            return RET_ERR; \
        io.in = SYS_NULL; \
        io.in_len = 0; \
        io.out = (SYS_UINT32 *) outv; \
        io.out_len = sizeof(out_type); \
        return(clnk_ctl_passthru_sock(ctx, cmd, &io)); \
    }

/* some input struct argument ; null output value */
#define CLNK_CTL_SET(ctx, cmd, in_type, inv) \
    { \
        struct clnk_io io; \
        if (inv == NULL) \
            return RET_ERR; \
        io.in = (void *) inv; \
        io.in_len = sizeof(in_type); \
        io.out = SYS_NULL; \
        io.out_len = 0; \
        return(clnk_ctl_passthru_sock(ctx, cmd, &io)); \
    }
/* only command, null arguments */
#define CLNK_CTL_DO(ctx, cmd) \
    { \
        struct clnk_io io; \
        io.in = SYS_NULL; \
        io.in_len = 0; \
        io.out = SYS_NULL; \
        io.out_len = 0; \
        return(clnk_ctl_passthru_sock(ctx, cmd, &io)); \
    }
/* input arguments and out struct should be provided*/
#define CLNK_CTL_SET_GET(ctx, cmd, in_type,inv,out_type,outv) \
    { \
        struct clnk_io io; \
        if ((inv == NULL)||(outv == NULL)) \
            return RET_ERR; \
        io.in = (void *) inv; \
        io.in_len = sizeof(in_type); \
        io.out = (SYS_UINT32 *)outv; \
        io.out_len = sizeof(out_type); \
        return(clnk_ctl_passthru_sock(ctx, cmd, &io)); \
    }   


/*********************************************************************
 * Utilities for keeping track of time within the Application space.
 *********************************************************************/

/** State variable for logical timer */
typedef struct 
{
    struct timeval expire;
} clnk_impl_timer_t;

/**
 * Starts a logical timer.  There is only one timer that can run at a time.
 */
static void 
clnk_impl_timer_start(clnk_impl_timer_t*  timer_ptr, 
                      SYS_UINT32          milliseconds)
{
    struct timeval delta;

    if (!timer_ptr) return;

    delta.tv_sec = milliseconds / 1000;
    delta.tv_usec = (milliseconds % 1000) * 1000;

#ifdef AEI_WECB
    aei_gettimeofday(&timer_ptr->expire, NULL);
#else
    gettimeofday(&timer_ptr->expire, NULL);
#endif

    timer_ptr->expire.tv_sec  += delta.tv_sec;
    timer_ptr->expire.tv_usec += delta.tv_usec;
    if (timer_ptr->expire.tv_usec > 1000*1000)
    {
        timer_ptr->expire.tv_sec  += 1;
        timer_ptr->expire.tv_usec -= 1000*1000;
    }
}


/**
 * Queries the number of milliseconds remaining on the logical timer
 * or zero if the timer has expired.
 */
static SYS_UINT32 
clnk_impl_timer_ms_remaining( const clnk_impl_timer_t* timer_ptr )
{
    struct timeval now;

    if (!timer_ptr) return 0;

#ifdef AEI_WECB
    aei_gettimeofday(&now, NULL);
#else
    gettimeofday(&now, NULL);
#endif
    if (timer_ptr->expire.tv_sec < now.tv_sec) return 0;

    if (timer_ptr->expire.tv_sec == now.tv_sec &&
        timer_ptr->expire.tv_usec < now.tv_usec) return 0;

    return
    (timer_ptr->expire.tv_sec - now.tv_sec) * 1000 +
    (timer_ptr->expire.tv_usec / 1000) -
    (now.tv_usec               / 1000) ;
}


/**
 * Causes the processor to spin for at least the designated number of 
 * milliseconds using the logical timer.
 */
static void clnk_impl_timer_spin_sleep(SYS_UINT32   milliseconds,
                                       void       (*sleep_func)(SYS_UINT32 sleep_ms))
{
    clnk_impl_timer_t instance;

    struct timeval now;
#ifdef AEI_WECB
    aei_gettimeofday(&now, NULL);
#else
    gettimeofday(&now, NULL);
#endif

    clnk_impl_timer_start(&instance, milliseconds);
    while (SYS_TRUE)
    {
        SYS_UINT32 remain = clnk_impl_timer_ms_remaining( &instance );

        /** Call provided spin function if there is one. */
        if (sleep_func) sleep_func(remain);

#if WATCH_TIMER_ODDITIES
        static SYS_UINT32 prevRemain = 0;
        static SYS_UINT32 times = 0;
        if (prevRemain != remain)
        {
            printf("      %d  x%d\n", remain, times );
            prevRemain = remain;
            times=0;
        } else
        {
            times++;
        }
#endif
        if (remain == 0) break;
    }
}


#if FEATURE_PUPQ_NMS
/**
 * This is an internal operation that should never be exposed to a customer
 * API.  This isn't precisely a generic L2ME operation; rather, it is a means
 * of reducing ccpu code load by doing most of the decoding for simple L2ME 
 * transactions up in host memory space.
 */
int clnk_do_generic_l2me_trans(clnk_ctx_t        *pctx, 
                               Mrt_GenEntryReq_t *gen_entry_req, 
                               Mrt_GenEntryRsp_t *gen_entry_rsp)
{ 
    if((pctx == 0)||(gen_entry_req == 0)||(gen_entry_rsp == 0))
    {
        return RET_ERR;
    }      

    /** TURN THIS ON FOR DEBUGGING */
    if (0)
    {
        SYS_UINT32 i;
        for (i = 0; i < sizeof(*gen_entry_req)/4; i++)
        {
            printf("%s(%d): submit word %02d: %8.8x    (%d)\n", 
                   __FILE__, __LINE__,
                   i, 
                   gen_entry_req->submit_data[i],
                   gen_entry_req->submit_data[i]);
        }
    }

    CLNK_CTL_SET_GET(pctx, CLNK_CTL_DO_GENERIC_L2ME_TRANS,
                     Mrt_GenEntryReq_t, gen_entry_req,
                     Mrt_GenEntryRsp_t, gen_entry_rsp);
}


/**
 */
int clnk_get_nms_local_ccpu(clnk_ctx_t *pctx, 
                            clnkdefs_nms_local_ccpu_t *nlc_ptr)
{
    if((pctx == 0)||(nlc_ptr == 0))
    {
        return RET_ERR;
    }      

    CLNK_CTL_GET_0(pctx, CLNK_CTL_GET_NMS_LOCAL_CCPU,
                   clnkdefs_nms_local_ccpu_t, nlc_ptr);
}

/**
 */
int clnk_get_nms_local_daemon(clnk_ctx_t *pctx, 
                              clnkdefs_nms_local_daemon_t *nld_ptr)
{
    if((pctx == 0)||(nld_ptr == 0))
    {
        return RET_ERR;
    }      

    CLNK_CTL_GET_0(pctx, CLNK_CTL_GET_NMS_LOCAL_DAEMON,
                   clnkdefs_nms_local_daemon_t, nld_ptr);
}



/**
 */
int clnk_get_local_misc_info(clnk_ctx_t *pctx, 
                             clnkdefs_local_misc_info_t *lmi_ptr)
{
    if((pctx == 0)||(lmi_ptr == 0))
    {
        return RET_ERR;
    }      

    CLNK_CTL_GET_0(pctx, CLNK_CTL_GET_LOCAL_MISC_INFO,
                   clnkdefs_local_misc_info_t, lmi_ptr);
}



/**
 * The generic l2me response structure arrives from the CCPU in a 
 * quite scrambled way.  This structure exists so that a utility
 * defined in this file can decode the scramble into something
 * easy for host operations to decode.
 *
 * This structure represents the information that arrives at the
 * host as the result of a single response from a single node.
 */
typedef struct {
    SYS_UINT32  dir_resp_info;
    SYS_BOOLEAN is_valid_response;
    SYS_UINT32  response_words;
    union {
        SYS_UINT32                          response_array[MRT_BYTES_ACK_MAX/4];
        Mrt_L2ME_ApplicationWordResponse_t  response_l2me_app_word;
        Mrt_NmsGetPushAckResponse_t         response_nms_get_push_ack;
    };
} clnk_generic_l2me_node_info_t;


/**
 * This structure carries a decoded generic response.  There is less 
 * processing to decode one of these.  It has the same information as
 * Mrt_GenericEntryRsp_t but the payloads are cracked out by node_id.
 */
typedef struct {
    clnk_generic_l2me_node_info_t nodes[MRT_MAX_NODES];
} clnk_generic_l2me_response_t;


/** 
 * A utility for transforming from CCPU-centric encoding to use-centric
 * form for decoding concatenated payloads.
 */
void
clnk_decode_generic_l2me_response(const Mrt_GenEntryRsp_t*      src_ptr,
                                  clnk_generic_l2me_response_t* dest_ptr)
{
    clink_node_id_t node_id;
    SYS_UINT32 index = 0;

    /* printf("TODO_REMOVE %s(%d): here.\n", __FILE__, __LINE__);
     */
    if (!src_ptr || !dest_ptr) return;

    memset(dest_ptr, 0, sizeof(*dest_ptr));

    for (node_id = 0; node_id < MRT_MAX_NODES; node_id++)
    {
        SYS_UINT32 entry = src_ptr->dir_entries[node_id];

        clink_node_id_t dir_node_id       = (entry >> 8) & 0xFF;
        SYS_UINT32      dir_resp_info     = entry & 0xFF;
        SYS_UINT32      is_valid_response = (dir_resp_info <= MRT_BYTES_ACK_MAX/4);
        SYS_UINT32      resp_words        = is_valid_response ? dir_resp_info : 0;

        if (dir_node_id < MRT_MAX_NODES)
        {
            dest_ptr->nodes[dir_node_id].dir_resp_info     = dir_resp_info;
            dest_ptr->nodes[dir_node_id].is_valid_response = is_valid_response;
            dest_ptr->nodes[dir_node_id].response_words    = resp_words;

            if (is_valid_response)
            {
                memcpy(dest_ptr->nodes[dir_node_id].response_array,
                       &src_ptr->resp_data[index], resp_words * 4);
            }

            index += resp_words;
        }
    }

    /** Debugging printout */
    if (0)
    {
        for (node_id = 0; node_id < MRT_MAX_NODES; node_id++)
        {
            SYS_UINT32 word;
            printf("%s(%d): node_id:%02d .response_words:%02d .dir_resp_info:0x%2.2x .is_val:%d",
                   __FILE__, __LINE__,
                   node_id, 
                   dest_ptr->nodes[node_id].response_words,
                   dest_ptr->nodes[node_id].dir_resp_info,
                   dest_ptr->nodes[node_id].is_valid_response);
            for (word = 0; 
                 word < dest_ptr->nodes[node_id].response_words; 
                 word++)
            {
                if (!(word%8)) printf("\n   ");
                printf(" %8.8x", dest_ptr->nodes[node_id].response_array[word]);
            }
            printf("\n");
        }
    }
}


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/


/**
 * Static declaration of the context for the nms Library.
 */
clnk_nms_context_t clnk_nms_context = {NULL, NULL, NULL, NULL, NULL, NULL};


/**
 * Utility to count the number of bits set in a word array
 */
SYS_UINT8
clnk_util_count_set_bits(SYS_UINT32 word)
{
    SYS_UINT32 retval = 0;
    while(word)
    {
        SYS_UINT32 lowbit = INCTYPES_LOW_BIT(word);

        word = word & ~lowbit;
        retval++;
    }

    return retval;
}


SYS_UINT32
clnk_util_count_set_bits_array(const SYS_UINT32* array, 
                               SYS_UINT32        array_len)
{
    SYS_UINT32 retval = 0;
    SYS_UINT32 index;
    for (index = 0; index < array_len; index++)
    {
        retval += clnk_util_count_set_bits(array[index]);
    }

    return retval;
}


/** TODO DOC */
#define CLNK_NMS_ERROR_IF(condition, str, param1, param2) do { \
    if ((condition) && clnk_nms_context.unexpected)            \
    {                                                          \
        clnk_nms_context.unexpected(                           \
            SYS_FALSE, __FILE__, __LINE__,                     \
            str, param1, param2 );                             \
    }                                                          \
} while (0)


/** TODO DOC */
#define CLNK_NMS_FATAL_IF(condition, str, param1, param2) do { \
    if ((condition) && clnk_nms_context.unexpected)            \
    {                                                          \
        clnk_nms_context.unexpected(                           \
            SYS_FALSE, __FILE__, __LINE__,                     \
            str, param1, param2 );                             \
    }                                                          \
} while (0)


/** 
 * Map parameters retrievable through L2ME to bits in the submit
 * frame. 
 */
typedef struct 
{
    clnk_nms_param_t  param;
    SYS_UINT8         submit_section;
    SYS_UINT8         word_offset_in_section;
    SYS_UINT8         bit_offset_in_word;
} clnk_nmsimpl_psmap_entry_t;


#define CLNK_NMSIMPL_PSMAP_ENTRY(NAME, SECTION, WORD)  \
    {  CLNK_NMS_PARAM_##NAME,                          \
       MRT_QBL_SECTION_##SECTION,     WORD,            \
           MRT_QBL_DEF_##SECTION##_W##WORD##_##NAME }  \

/** A master table used for mapping between parameters and their respective
 *  bits in the submit. 
 *
 *  RULE:  Entries MUST be in their natural order in the submit message
 *         or else a lot will break!  This ordering is used to extract the
 *         values embedded in the responses prepared by nodes.
 */
const clnk_nmsimpl_psmap_entry_t clnk_nmsimpl_psmap[] = {
#if NMS_ADD_NEW_PARAMETER_LOCATION
    /** When adding a new parameter, come here. */
#endif
   
    CLNK_NMSIMPL_PSMAP_ENTRY(PHY_PROFILE_TX_P2P,                LINK_SZ32, 0),
    CLNK_NMSIMPL_PSMAP_ENTRY(PHY_PROFILE_TX_GCD,                NODE_SZ32, 0),
    CLNK_NMSIMPL_PSMAP_ENTRY(PHY_EST_P2P_RX_DBM,                LINK_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(PHY_POWER_REDUCTION_P2P_TX,        LINK_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(PHY_MTRANS_P2P_RX,                 LINK_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(PHY_MTRANS_P2P_RX_ERRORS,          LINK_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(PHY_MTRANS_TX_PKT_CTR,             NODE_SZ1,  0), 
    CLNK_NMSIMPL_PSMAP_ENTRY(NODE_PQOS_CLASSIFY_MODE,           NODE_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(PHY_POWER_CTL_PHY_TARGET,          NODE_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(NODE_LINK_UP_EVENT,                NODE_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(NODE_PERSONALITY,                  NODE_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(NODE_ADM_REQ_RX,                   NODE_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(NODE_FREQ_CAPABILITY_MASK,         NODE_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(NODE_FREQ_CHANNEL_MASK,            NODE_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(NODE_MFR_VENDOR_ID,                NODE_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(NODE_MFR_HW_VER,                   NODE_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(NODE_MFR_SW_VER,                   NODE_SZ1,  0),
    CLNK_NMSIMPL_PSMAP_ENTRY(NODE_LINK_TIME,                    NODE_SZ1,  31),
    CLNK_NMSIMPL_PSMAP_ENTRY(NODE_LINK_TIME_BAD,                NODE_SZ1,  31),
};


/**
 * A utility that verifies the fundamental rule for this table of
 * entries.
 */
void
clnk_nmsimpl_verify_psmap_integrity(void)
{
    const clnk_nmsimpl_psmap_entry_t* prev_ptr = &clnk_nmsimpl_psmap[0];
    SYS_UINT32 i;
    for (i = 1; i < INCTYPES_ARRAY_LEN(clnk_nmsimpl_psmap); i++)
    {
        const clnk_nmsimpl_psmap_entry_t* this_ptr = 
             &clnk_nmsimpl_psmap[i];

        CLNK_NMS_ERROR_IF(
            this_ptr->submit_section < prev_ptr->submit_section,
            "Bad psmap ordering: %d %d",
            i, this_ptr->submit_section);

        CLNK_NMS_ERROR_IF(
            this_ptr->submit_section         == prev_ptr->submit_section  &&
            this_ptr->word_offset_in_section <  prev_ptr->word_offset_in_section,
            "Bad psmap ordering: %d %d",
            i, this_ptr->word_offset_in_section);

        CLNK_NMS_ERROR_IF(
            this_ptr->submit_section         == prev_ptr->submit_section         &&
            this_ptr->word_offset_in_section == prev_ptr->word_offset_in_section &&
            this_ptr->bit_offset_in_word     >= prev_ptr->bit_offset_in_word,
            "Bad psmap ordering: %d %d", i, this_ptr->bit_offset_in_word);

        prev_ptr = this_ptr;
    }
}


void
clnk_nms_init(const clnk_nms_context_t*    context_ptr)
{
    clnk_nms_stats_t* sptr;

    // printf("TODO_REMOVE %s(%d): here.\n", __FILE__, __LINE__);

    /**
     * There are a number of things that need verifying at compile 
     * time in the event of maintenance problems.
     */
    {
        INCTYPES_VERIFY_STRUCT_SIZE(clnk_nms_element_small_t  , CLNK_NMS_ELEMENT_BLOCK_SIZE_SMALL_4B   );
        INCTYPES_VERIFY_STRUCT_SIZE(clnk_nms_element_medium_t , CLNK_NMS_ELEMENT_BLOCK_SIZE_MEDIUM_8B  );
        INCTYPES_VERIFY_STRUCT_SIZE(clnk_nms_element_large_t  , CLNK_NMS_ELEMENT_BLOCK_SIZE_LARGE_256B );
    }

    /* Services to handle fatal errors not initialized yet, so
     * signal api abuse with permanent spin */
    while(!context_ptr);

    /* Initialize the services needed to report errors. */
    INCTYPES_SAFE_PTR_COPY(&clnk_nms_context, context_ptr);

    /* A check that technically only needs to be performed if 
     * the set of L2ME responder bits changes, but its too hard
     * to devise that. */
    clnk_nmsimpl_verify_psmap_integrity();

    /* Perform some checks that compile to zero code if no issues found-
     * preprocessor evaluates this all to nothing.
     */
    {
        CLNK_NMS_FATAL_IF(
            CLNKDEFS_SUBSTITUTE_FIELD(0, 7, 4, 0xFFFF) != 0xF0,
            "Utility failure", 0, 0);
        CLNK_NMS_FATAL_IF(
            CLNKDEFS_SUBSTITUTE_FIELD(0, 4, 4, 0xFFFF) != 0x10,
            "Utility failure", 0, 0);
        CLNK_NMS_FATAL_IF(
            CLNKDEFS_SUBSTITUTE_FIELD(0, 0, 0, 0xFFFF) != 0x1,
            "Utility failure", 0, 0);
        CLNK_NMS_FATAL_IF(
            CLNKDEFS_SUBSTITUTE_FIELD(0, 1, 1, 0xFFFF) != 0x2,
            "Utility failure", 0, 0);
        CLNK_NMS_FATAL_IF(
            CLNKDEFS_SUBSTITUTE_FIELD(0, 31, 30, 0xFFFFFFFF) != 0xC0000000,
            "Utility failure", 0, 0);
        CLNK_NMS_FATAL_IF(
            CLNKDEFS_SUBSTITUTE_FIELD(0xFFFFFFFF, 31, 30, 0) != 0x3FFFFFFF,
            "Utility failure", 0, 0);
        CLNK_NMS_FATAL_IF(
            CLNKDEFS_SUBSTITUTE_FIELD(0xFFFFFFFF, 31, 0, 0)  != 0x0,
            "Utility failure", 0, 0);
        CLNK_NMS_FATAL_IF(
            CLNKDEFS_SUBSTITUTE_FIELD(0xFFFFFFFF,  1,  1, 0) != 0xFFFFFFFD,
            "Utility failure", 0, 0);
        CLNK_NMS_FATAL_IF(
            CLNKDEFS_SUBSTITUTE_FIELD(0xFFFFFFFF,  16,  11, 0) != 0xFFFE07FF,
            "Utility failure", 0, 0);
        CLNK_NMS_FATAL_IF(
            CLNKDEFS_EXTRACT_FIELD(0x12345678,  11,  4) != 0x67,
            "Utility failure", 0, 0);
        CLNK_NMS_FATAL_IF(
            CLNKDEFS_EXTRACT_FIELD(0x12345678,  31,  0) != 0x12345678,
            "Utility failure", 0, 0);
        CLNK_NMS_FATAL_IF(
            CLNKDEFS_EXTRACT_FIELD(0x12345678,  31,  28) != 0x1,
            "Utility failure", 0, 0);
        CLNK_NMS_FATAL_IF(
            CLNKDEFS_EXTRACT_FIELD(0x12345678,  15,  0) != 0x5678,
            "Utility failure", 0, 0);
    }

    sptr = clnk_nms_context.stats_ptr;

    if (sptr)
    {
        INCTYPES_SAFE_PTR_ZERO(sptr);

        sptr->pools[CLNK_NMS_ELEMENT_POOL_NAME_SMALL_4B].block_size =
                   CLNK_NMS_ELEMENT_BLOCK_SIZE_SMALL_4B;

        sptr->pools[CLNK_NMS_ELEMENT_POOL_NAME_MEDIUM_8B].block_size =
                   CLNK_NMS_ELEMENT_BLOCK_SIZE_MEDIUM_8B;

        sptr->pools[CLNK_NMS_ELEMENT_POOL_NAME_LARGE_256B].block_size =
                   CLNK_NMS_ELEMENT_BLOCK_SIZE_LARGE_256B;
    }

#if FEATURE_PUPQ_GQ
    {
        /** Compile time test case for bit counter. */
        CLNK_NMS_FATAL_IF(0  != clnk_util_count_set_bits(0),              "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(18 != clnk_util_count_set_bits(0xFFFFC),        "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(20 != clnk_util_count_set_bits(0xFFFFF),        "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(1  != clnk_util_count_set_bits(0x10000),        "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(2  != clnk_util_count_set_bits(0x10010),        "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(3  != clnk_util_count_set_bits(0x10101),        "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(3  != clnk_util_count_set_bits(0x10110),        "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(4  != clnk_util_count_set_bits(0x10111),        "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(32 != clnk_util_count_set_bits((SYS_UINT32)-1), "Test case fail.", 0, 0);

        CLNK_NMS_FATAL_IF(0x00000004 != INCTYPES_LOW_BIT(0x000FFFFC), "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(0x00010000 != INCTYPES_LOW_BIT(0x00010000), "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(0x00000400 != INCTYPES_LOW_BIT(0x00010400), "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(0x00000080 != INCTYPES_LOW_BIT(0x00010480), "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(0x00000080 != INCTYPES_LOW_BIT(0x000FFF80), "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(0x00000001 != INCTYPES_LOW_BIT(0x000FFFFF), "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(0x00000001 != INCTYPES_LOW_BIT(0xFFFFFFFF), "Test case fail.", 0, 0);
        CLNK_NMS_FATAL_IF(0x00000100 != INCTYPES_LOW_BIT(0xFFFFFF00), "Test case fail.", 0, 0);
    }
#endif 
}


/** TODO DOC */
enum {
    CLNK_NMS_SOURCE_MY_NODE,
    CLNK_NMS_SOURCE_MY_NODE_NEW,
    CLNK_NMS_SOURCE_NW_NODE,
    CLNK_NMS_SOURCE_QOS_EVENT,
    CLNK_NMS_SOURCE_GENERIC_L2ME,
    CLNK_NMS_SOURCE_NEW_CONF,
    CLNK_NMS_SOURCE_HOST,

    CLNK_NMS_SOURCE__MAX  /** MUST BE LAST */
};
INCTYPES_VERIFY_TOKEN_LESS_THAN(CLNK_NMS_SOURCE__MAX, 32);

#define CLNK_NMS_SOURCE_IS_BATCH_APIS(source)                    \
        ((1UL << source) & ( 0                                   \
                           | (1UL << CLNK_NMS_SOURCE_MY_NODE)    \
                           | (1UL << CLNK_NMS_SOURCE_QOS_EVENT)  \
                           | (1UL << CLNK_NMS_SOURCE_NW_NODE)    \
        )                )

const clnk_nms_pdef_t*
clnk_nms_get_pdef(clnk_nms_param_t param)
{
    static const clnk_nms_pdef_t defs[CLNK_NMS_PARAM__MAX] = 
        {
#define DEFINE_PARAMETER(param, ecard, etype, econst, esource, strname, zero) \
             { param, ecard, econst, etype, esource, strname }, 
#include "ClnkNmsParams.h"
#undef  DEFINE_PARAMETER
        };

    CLNK_NMS_FATAL_IF(param != defs[param].param,
                      "Structure corruption", 0, 0);
    CLNK_NMS_FATAL_IF(param >= CLNK_NMS_PARAM__MAX, "Bad range", 0, 0);

    return &defs[param];
}


SYS_BOOLEAN 
clnk_nms_eset_test_single(const clnk_nms_eset_t* eset_ptr,
                          const clnk_nms_pdef_t* pdef_ptr,
                          clink_node_id_t        queried_node_id,
                          clink_node_id_t        remote_node_id)
{
    CLNK_NMS_ERROR_IF(!eset_ptr, "Null pointer", 0, 0);
    CLNK_NMS_ERROR_IF(!pdef_ptr, "Null pointer", 0, 0);
    CLNK_NMS_ERROR_IF(queried_node_id >= MAX_NUM_NODES, "Bad range", 0, 0);
    CLNK_NMS_ERROR_IF(remote_node_id  >= MAX_NUM_NODES, "Bad range", 0, 0);

    if (!eset_ptr || !pdef_ptr) return SYS_FALSE;

    switch (pdef_ptr->cardinality)
    {
    case CLNK_NMS_CARDINALITY_NETWORK:
        CLNK_NMS_ERROR_IF(queried_node_id, "queried_node_id should be 0 for param %d", 
                          pdef_ptr->param, 0);
        queried_node_id = 0;
        /* DELIBERATE CASE STATEMENT FALL THROUGH */
    case CLNK_NMS_CARDINALITY_NODE:
        CLNK_NMS_ERROR_IF(remote_node_id, "remote_node_id should be 0 for param %d", 
                          pdef_ptr->param, 0);
        remote_node_id = 0;
        break;
    case CLNK_NMS_CARDINALITY_LINK:
        break;
    default:
        CLNK_NMS_ERROR_IF(1, "Unhandled case", 0, 0);
    }

    return INCTYPES_NODE_IN_NODEMASK(
                queried_node_id,
                eset_ptr->included[pdef_ptr->param][remote_node_id]);
}


/**
 * This internal utility returns SYS_TRUE if the specified
 * Eset includes an Element defined within a specified Parameter
 * once some masks are applied.  Use of the masks depends 
 * upon Parameter cardinality.
 * 
 * Network cardinality: the masks are completely ignored.
 * 
 * Node cardinality: this function only returns true if the
 *   specified Eset includes at least one Element whose 
 *   queried_node is in the queried_wildcard.  The remote_wildcard
 *   is ignored.
 * 
 * Link cardinality: this function only returns true if the
 *   specified Eset includes at least one Element whose 
 *   queried_node is in the queried_wildcard AND whose
 *   remote_node is also in the remote_wildcard.
 */
SYS_BOOLEAN 
clnk_nms_eset_test_any(const clnk_nms_eset_t* eset_ptr,
                       const clnk_nms_pdef_t* pdef_ptr,
                       clink_nodemask_t       queried_wildcard,
                       clink_nodemask_t       remote_wildcard)
{
    CLNK_NMS_ERROR_IF(!eset_ptr, "Null pointer", 0, 0);
    if (!eset_ptr) return SYS_FALSE;

    CLNK_NMS_ERROR_IF(!pdef_ptr, "Null pointer", 0, 0);
    if (!pdef_ptr) return SYS_FALSE;

    switch (pdef_ptr->cardinality)
    {
    case CLNK_NMS_CARDINALITY_NETWORK:
        return eset_ptr->included[pdef_ptr->param][0] ? SYS_TRUE : SYS_FALSE;
    case CLNK_NMS_CARDINALITY_NODE:
        return (eset_ptr->included[pdef_ptr->param][0] & queried_wildcard) 
                  ? SYS_TRUE : SYS_FALSE;
    case CLNK_NMS_CARDINALITY_LINK:
        {
            clink_node_id_t node_id;

            if (!remote_wildcard) return SYS_FALSE;

            for (node_id = 0; node_id < MAX_NUM_NODES; node_id++)
            {
                if (!INCTYPES_NODE_IN_NODEMASK(node_id, remote_wildcard)) continue;

                if (eset_ptr->included[pdef_ptr->param][node_id] & 
                    queried_wildcard) return SYS_TRUE;
            }
            return SYS_FALSE;
        }
    default:
        CLNK_NMS_ERROR_IF(1, "Unhandled case", 0, 0);
        return SYS_FALSE;
    }
}


/** TODO IMPL */
void
clnk_nms_eset_include_wildcard(clnk_nms_eset_t*       eset_ptr,
                               const clnk_nms_pdef_t* pdef_ptr,
                               clink_nodemask_t       queried_nodemask,
                               clink_nodemask_t       remote_nodemask)
{
    switch (pdef_ptr->cardinality)
    {
    case CLNK_NMS_CARDINALITY_NETWORK:
        eset_ptr->included[pdef_ptr->param][0] |= queried_nodemask ? 1 : 0;
        break;
    case CLNK_NMS_CARDINALITY_NODE:
        eset_ptr->included[pdef_ptr->param][0] |= queried_nodemask;
        break;
    case CLNK_NMS_CARDINALITY_LINK:
        {
            clink_node_id_t remote_node_id;
            for (remote_node_id = 0;
                 remote_node_id < MAX_NUM_NODES;
                 remote_node_id++)
            {
                if (!INCTYPES_NODE_IN_NODEMASK(remote_node_id, remote_nodemask))
                {
                    continue;
                }
                eset_ptr->included[pdef_ptr->param][remote_node_id] |= queried_nodemask;
            }
        }
        break;
    default:
        CLNK_NMS_ERROR_IF(1, "Unhandled case", 0, 0);
    }
}


/** TODO DOC */
void
clnk_nms_eset_include_single(clnk_nms_eset_t*       eset_ptr,
                             const clnk_nms_pdef_t* pdef_ptr,
                             clink_node_id_t        queried_node_id,
                             clink_node_id_t        remote_node_id)
{
   CLNK_NMS_ERROR_IF(!eset_ptr, "Null pointer", 0, 0);
   CLNK_NMS_ERROR_IF(!pdef_ptr, "Null pointer", 0, 0);
   CLNK_NMS_ERROR_IF(pdef_ptr->param >= CLNK_NMS_PARAM__MAX, "Bad argument", 0, 0);
   CLNK_NMS_ERROR_IF(queried_node_id >= MAX_NUM_NODES,       "Bad range",    0, 0);
   CLNK_NMS_ERROR_IF(remote_node_id  >= MAX_NUM_NODES,       "Bad range",    0, 0);

   if (!eset_ptr || !pdef_ptr) return;
   if (queried_node_id >= MAX_NUM_NODES) return;
   if (remote_node_id  >= MAX_NUM_NODES) return;

   switch (pdef_ptr->cardinality)
   {
   case CLNK_NMS_CARDINALITY_LINK:
       break;
   case CLNK_NMS_CARDINALITY_NODE:
       CLNK_NMS_ERROR_IF(remote_node_id, "remote_node_id should be 0 for param %d", 
                         pdef_ptr->param, 0);
       remote_node_id = 0;
       /* DELIBERATE CASE STATEMENT FALL THROUGH */
   case CLNK_NMS_CARDINALITY_NETWORK:
       CLNK_NMS_ERROR_IF(queried_node_id, "queried_node_id should be 0 for param %d", 
                         pdef_ptr->param, 0);
       queried_node_id = 0;
       break;
   default:
       CLNK_NMS_ERROR_IF(1, "Unhandled case", 0, 0);
   }
   eset_ptr->included[pdef_ptr->param][remote_node_id] |= 
            1UL << queried_node_id;
}


void
clnk_nms_init_eset(clnk_nms_eset_t*            eset_ptr,
                   enum clnk_nms_named_eset_t  named_eset)
{
    SYS_BOOLEAN param_array[CLNK_NMS_PARAM__MAX];

    CLNK_NMS_ERROR_IF(!eset_ptr, "Null pointer", 0, 0);
    if (!eset_ptr) return;

    INCTYPES_SAFE_PTR_ZERO(eset_ptr);
    INCTYPES_SAFE_VAR_ZERO(param_array);

    switch (named_eset)
    {

#if NMS_ADD_NEW_PARAMETER_LOCATION
    /** When adding a new parameter, come here. */
#endif

    case CLNK_NMS_NAMED_ESET_EMPTY:
        break;
    case CLNK_NMS_NAMED_ESET_IN_NETWORK:           
        param_array[CLNK_NMS_PARAM_NW_NODEMASK]      = SYS_TRUE;
        param_array[CLNK_NMS_PARAM_NW_NODE_ID_NC]    = SYS_TRUE;
        param_array[CLNK_NMS_PARAM_NW_NODE_ID_LOCAL] = SYS_TRUE;
        param_array[CLNK_NMS_PARAM_NODE_MOCA_GUID]   = SYS_TRUE;
        break;
    case CLNK_NMS_NAMED_ESET_ADMISSION_CONSTANT: 
        {
            clnk_nms_param_t  param;
            for (param = 0;
                 param < CLNK_NMS_PARAM__MAX;
                 param++)
            {
                param_array[param] = clnk_nms_get_pdef(param)->admission_constant;
            }
        }
        break;
    case CLNK_NMS_NAMED_ESET_FULL_MESH_RATES:
        param_array[CLNK_NMS_PARAM_PHY_RATE_TX_GCD]          = SYS_TRUE;
        param_array[CLNK_NMS_PARAM_PHY_RATE_TX_P2P]          = SYS_TRUE;
        param_array[CLNK_NMS_PARAM_PHY_CPLEN_TX_GCD]         = SYS_TRUE;
        param_array[CLNK_NMS_PARAM_PHY_CPLEN_TX_P2P]         = SYS_TRUE;
        break;
    case CLNK_NMS_NAMED_ESET_LINK_UP:
        param_array[CLNK_NMS_PARAM_NODE_LINK_TIME]           = SYS_TRUE;
        break;
    case CLNK_NMS_NAMED_ESET_MTRANS:
        param_array[CLNK_NMS_PARAM_PHY_MTRANS_P2P_RX]        = SYS_TRUE;
        param_array[CLNK_NMS_PARAM_PHY_MTRANS_P2P_RX_ERRORS] = SYS_TRUE;
        break;
    case CLNK_NMS_NAMED_ESET_ALL:                
        {
            clnk_nms_param_t  param;
            for (param = 0;
                 param < CLNK_NMS_PARAM__MAX;
                 param++)
            {
                param_array[param] = SYS_TRUE;
            }
        }
        break;
    default: /* nothing */
        CLNK_NMS_ERROR_IF(1, "Unhandled case", 0, 0);
    }

    /* Now set properly */
    {
        clnk_nms_param_t  param;
        for (param = 0;
             param < CLNK_NMS_PARAM__MAX;
             param++)
        {
            const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(param);
            if (!param_array[param]) continue;

            switch (clnk_nms_get_pdef(param)->cardinality)
            {
            case CLNK_NMS_CARDINALITY_NETWORK: 
                clnk_nms_eset_include_single(eset_ptr, pdef_ptr, 0, 0);
                break;
            case CLNK_NMS_CARDINALITY_NODE: 
                eset_ptr->included[param][0] = CLINK_NODEMASK_ALL; 
                break;
            case CLNK_NMS_CARDINALITY_LINK:
                {
                    clink_node_id_t remote_node_id;
                    for (remote_node_id = 0;
                         remote_node_id < MAX_NUM_NODES;
                         remote_node_id++)
                    {
                        eset_ptr->included[param][remote_node_id] = 
                            CLINK_NODEMASK_ALL;
                    }
                }
                break;
            default:
                CLNK_NMS_ERROR_IF(1, "Unhandled case", 0, 0);
            }
        }
    }
}


/** 
 * Element Index
 * 
 * A Cache uses this type to refer to an offset within an array of Elements
 * that it maintains.
 * 
 * A Negative value indicates an illegal offset, i.e. no valid Element
 * is indicated.
 * 
 * 
 */
typedef SYS_INT16 clnk_nmsimpl_eindex_t;


/** TODO DOC 
 * 
 * CUST CONFIG!!!???
 * 
 * May vary with number of nodes
 */
#define CLNK_NMSIMPL_CACHE_EINDEX__MAX  \
   ((clnk_nmsimpl_eindex_t)CLNK_NMS_MAX_NUMBER_ELEMENTS_IN_CACHE)


/** TODO DOC */
typedef struct
{
    /** NEGATIVE MEANS NO STORAGE AVAILABLE. Nonnegative is offset
     *  into the array.
     */

    /** For each Parameter, an initialized Cache stores an index 
     *  here for the first array entry in the 'elements' and 'dispositions'
     *  arrays that has been assigned to that Parameter.  After 
     *  initialization, this index value NEVER changes.
     */
    clnk_nmsimpl_eindex_t      param_start_index[CLNK_NMS_PARAM__MAX];

    /** For each Parameter, an initialized Cache stores a count here defining 
     *  how many entries in the 'elements' and 'dispositions' arrays that have
     *  been assigned to that Parameter.  After initialization, this index 
     *  value NEVER changes. */
    clnk_nmsimpl_eindex_t      param_count_index[CLNK_NMS_PARAM__MAX];

    /** An array that stores the most recent dispositions for the Elements
     *  in the 'elements' member.  This is only changed during Cache fill
     *  operations, copy operations, and release operations. */
    clnk_nms_disposition_t     dispositions[CLNK_NMSIMPL_CACHE_EINDEX__MAX];

    /** An array that stores Application allocation handles for Elements
     *  currently in the Cache. */
    clnk_nms_element_handle_t  elements[CLNK_NMSIMPL_CACHE_EINDEX__MAX];

} clnk_nmsimpl_cache_t;
/** Make absolutely sure that the external cache type is smaller than the
 *  internal cache type. */
INCTYPES_VERIFY_STRUCT_LESS_OR_EQUAL(clnk_nmsimpl_cache_t, sizeof(clnk_nms_cache_t));


clnk_nmsimpl_eindex_t 
clnk_nmsimpl_find_eindex(const clnk_nmsimpl_cache_t* icache_ptr,
                         const clnk_nms_pdef_t*      pdef_ptr,
                         clink_node_id_t             queried_node_id,
                         clink_node_id_t             remote_node_id)
{
    clnk_nmsimpl_eindex_t local_eindex = 0;
    clnk_nmsimpl_eindex_t retval;

    CLNK_NMS_ERROR_IF(!icache_ptr, "Null pointer", 0, 0);
    CLNK_NMS_ERROR_IF(!pdef_ptr,   "Null pointer", 0, 0);
    CLNK_NMS_ERROR_IF(queried_node_id >= MAX_NUM_NODES, "Bad range", 0, 0);
    CLNK_NMS_ERROR_IF(remote_node_id  >= MAX_NUM_NODES, "Bad range", 0, 0);

    switch (pdef_ptr->cardinality)
    {
    case CLNK_NMS_CARDINALITY_NETWORK:
        CLNK_NMS_ERROR_IF(queried_node_id, "queried_node_id must be zero.", 0, 0);
        CLNK_NMS_ERROR_IF(remote_node_id,  "remote_node_id  must be zero.", 0, 0);
        local_eindex = 0;
        break;
    case CLNK_NMS_CARDINALITY_NODE:
        CLNK_NMS_ERROR_IF(remote_node_id,  "remote_node_id must be zero.", 0, 0);
        local_eindex = queried_node_id;
        break;
    case CLNK_NMS_CARDINALITY_LINK:
        /** SOMEDAY this could be optimized.  At the time of this writing,
         * I am taking an implementation shortcut where items on the diagonal
         * have allocated space but never can contain values.  With a smarter
         * mapping function, you could eliminate this. */
        local_eindex = queried_node_id * MAX_NUM_NODES + remote_node_id;
        break;
    default:
        CLNK_NMS_ERROR_IF(1, "Unhandled case", 0, 0);
    }

    CLNK_NMS_ERROR_IF(local_eindex >= icache_ptr->param_count_index[pdef_ptr->param], 
                      "local_eindex %d >= parameter %d size", 
                      local_eindex, pdef_ptr->param);

    retval = icache_ptr->param_start_index[pdef_ptr->param] + local_eindex;

    if (retval >= CLNK_NMSIMPL_CACHE_EINDEX__MAX)
    {
        /* If it comes to this case we will be wrong.  At least
         * we can return an index that won't corrupt other Application
         * data. */
        CLNK_NMS_ERROR_IF(1, "calculated index %d greater than cache max %d", 
                          retval, CLNK_NMSIMPL_CACHE_EINDEX__MAX);

        retval = 0;
    }

    return retval;
}



void
clnk_nmsimpl_get_element_block_info(
                             const clnk_nms_pdef_t*              pdef_ptr,
                             enum clnk_nms_element_pool_name_t*  poolname_ptr,
                             enum clnk_nms_element_block_size_t* size_ptr)
{
    CLNK_NMS_ERROR_IF(!pdef_ptr,     "Null pointer", 0, 0);
    CLNK_NMS_ERROR_IF(!poolname_ptr, "Null pointer", 0, 0);
    CLNK_NMS_ERROR_IF(!size_ptr,     "Null pointer", 0, 0);

    switch (pdef_ptr->type)
    {
    case CLNK_NMS_TYPE_UINT32:
    case CLNK_NMS_TYPE_INT32:
    case CLNK_NMS_TYPE_CENTI_DECIBELS:
        *poolname_ptr = CLNK_NMS_ELEMENT_POOL_NAME_SMALL_4B;
        *size_ptr =    CLNK_NMS_ELEMENT_BLOCK_SIZE_SMALL_4B;
        break;
    case CLNK_NMS_TYPE_MAC_ADDR:
        *poolname_ptr = CLNK_NMS_ELEMENT_POOL_NAME_MEDIUM_8B;
        *size_ptr =    CLNK_NMS_ELEMENT_BLOCK_SIZE_MEDIUM_8B;
        break;
    case CLNK_NMS_TYPE_PROFILE:
    case CLNK_NMS_TYPE_PASSWORD:
        *poolname_ptr = CLNK_NMS_ELEMENT_POOL_NAME_LARGE_256B;
        *size_ptr =    CLNK_NMS_ELEMENT_BLOCK_SIZE_LARGE_256B;
        break;
    default:
        CLNK_NMS_ERROR_IF(1, "Unhandled case", 0, 0);
        break;
    }
}


void
clnk_nmsimpl_delete_element(const clnk_nms_pdef_t*     pdef_ptr,
                            clnk_nms_element_handle_t* delete_element_ptr)
{
    enum clnk_nms_element_pool_name_t   poolname;
    enum clnk_nms_element_block_size_t  size;

    CLNK_NMS_ERROR_IF(!pdef_ptr,           "Null pointer", 0, 0);
    CLNK_NMS_ERROR_IF(!delete_element_ptr, "Null pointer", 0, 0);

    clnk_nmsimpl_get_element_block_info(pdef_ptr, &poolname, &size);

    if (CLNK_NMS_ELEMENT_HANDLE_INVALID != *delete_element_ptr)
    {
        SYS_BOOLEAN was_deleted =
            clnk_nms_context.decrement_maybe_delete(poolname, size, 
                                                    *delete_element_ptr);
    
        if (clnk_nms_context.stats_ptr && was_deleted)
        {
            clnk_nms_context.stats_ptr->pools[poolname].num_allocated--;
        }

        *delete_element_ptr = CLNK_NMS_ELEMENT_HANDLE_INVALID;
    }
}


/** NOTE THAT THIS FUNC TESTS THE ESET FOR INCLUSION IF YOU PROVIDE ONE! 
 *  IT IS NOT NECESSARY TO TEST THE ESET BEFORE CALLING THIS FUNC! */
void
clnk_nmsimpl_set_element_in_cache(clnk_nmsimpl_cache_t*   icache_ptr,
                                  const clnk_nms_pdef_t*  pdef_ptr,
                                  const clnk_nms_eset_t*  eset_ptr,
                                  clink_node_id_t         queried_node_id,
                                  clink_node_id_t         remote_node_id,
                                  const clnk_nms_eview_t* const_eview_ptr,
                                  clnk_nms_disposition_t  disposition)
{
    clnk_nmsimpl_eindex_t eindex;

    CLNK_NMS_ERROR_IF(!icache_ptr,      "Null pointer", 0, 0);
    CLNK_NMS_ERROR_IF(!pdef_ptr,        "Null pointer", 0, 0);

    /* We do no work if this element is not in the eset */
    if (eset_ptr && !clnk_nms_eset_test_single(eset_ptr, pdef_ptr,
                                               queried_node_id,
                                               remote_node_id)) return;

    CLNK_NMS_ERROR_IF(queried_node_id >= MAX_NUM_NODES, "Bad range", 0, 0);
    CLNK_NMS_ERROR_IF(remote_node_id  >= MAX_NUM_NODES, "Bad range", 0, 0);

    if (!icache_ptr || !pdef_ptr) return;

    /** In some future implementation where an Application may use a cache
     * which doesn't store Elements for all Parameters, this will make sense */
    if (icache_ptr->param_count_index[pdef_ptr->param] == 0) 
    {
        CLNK_NMS_ERROR_IF(1, "Bad set with param %d having count %d",
                          pdef_ptr->param,
                          icache_ptr->param_count_index[pdef_ptr->param]);
        return;
    }

    eindex = clnk_nmsimpl_find_eindex(icache_ptr, pdef_ptr,
                                      queried_node_id,
                                      remote_node_id);

    /** A very bad condition: must prevent all bad memory corruption! */
    if (eindex >= CLNK_NMSIMPL_CACHE_EINDEX__MAX)
    {
        CLNK_NMS_ERROR_IF(1, "Integrity failure: %d %d", 
                          eindex, CLNK_NMSIMPL_CACHE_EINDEX__MAX);
        return;
    }

    /** 
     * At this point we are ready for the replacement.  We do things 
     * differently if we are called with a null element.
     */
    if (const_eview_ptr == NULL)
    {
        /* verify new disposition is appropriate for null eview */
        switch (disposition)
        {
        case CLNK_NMS_DISPOSITION_UNKNOWN:
        case CLNK_NMS_DISPOSITION_NIN:
        case CLNK_NMS_DISPOSITION_UNRETRIEVABLE:
            break;
        default:
            CLNK_NMS_ERROR_IF(1, "Disposition does not allow null element", 
                              0, 0);
        }

        /* delete existing element and sub in new eview */
        clnk_nmsimpl_delete_element(pdef_ptr, 
                                    &icache_ptr->elements[eindex]);
        icache_ptr->elements[eindex]     = CLNK_NMS_ELEMENT_HANDLE_INVALID;
        icache_ptr->dispositions[eindex] = disposition;
    }
    else
    {
        /* The new element is real; we must install it */
        enum clnk_nms_element_pool_name_t   poolname;
        enum clnk_nms_element_block_size_t  size;

        /* The element already in the cache at the time of this call */
        clnk_nms_element_handle_t old_element = icache_ptr->elements[eindex];

        /* The element created during this call to hold a new value */
        clnk_nms_element_handle_t new_element = CLNK_NMS_ELEMENT_HANDLE_INVALID;

        clnk_nms_eview_t* new_eview_ptr;

        /* verify new disposition is appropriate for null eview */
        switch (disposition)
        {
        case CLNK_NMS_DISPOSITION_UNKNOWN:
        case CLNK_NMS_DISPOSITION_NIN:
        case CLNK_NMS_DISPOSITION_UNRETRIEVABLE:
            CLNK_NMS_ERROR_IF(!const_eview_ptr, 
                              "Disposition does not allow nonnull element", 
                              0, 0);
            break;
        }

        clnk_nmsimpl_get_element_block_info(pdef_ptr, &poolname, &size);

        /* Allocate a new element */
        new_element = clnk_nms_context.allocate(poolname, size);

        if (CLNK_NMS_ELEMENT_HANDLE_INVALID == new_element)
        {
            CLNK_NMS_ERROR_IF(1, "Unexpected alloc fail pool:%d size:%d", 
                              poolname, size);
            return;
        }
        else
        {
            if (clnk_nms_context.stats_ptr)
            {
                clnk_nms_context.stats_ptr->pools[poolname].num_allocated++;
                clnk_nms_context.stats_ptr->pools[poolname].peak_allocated = 
                    INCTYPES_MAX(
                        clnk_nms_context.stats_ptr->pools[poolname].num_allocated,
                        clnk_nms_context.stats_ptr->pools[poolname].peak_allocated);
            }
        }

        /* Put data into new element */
        new_eview_ptr = (clnk_nms_eview_t*) 
                         clnk_nms_context.dereference(poolname, size, new_element);

        if (new_eview_ptr)
        {
            /* install the value */
            memcpy(new_eview_ptr, const_eview_ptr, size);

            icache_ptr->elements[eindex]     = new_element;
            icache_ptr->dispositions[eindex] = disposition;

            clnk_nmsimpl_delete_element(pdef_ptr, &old_element);
        }
        else
        {
            CLNK_NMS_ERROR_IF(1, "Unexpected derefrence failure", 0, 0);

            icache_ptr->elements[eindex]     = old_element;
            icache_ptr->dispositions[eindex] = CLNK_NMS_DISPOSITION_STALE;

            clnk_nmsimpl_delete_element(pdef_ptr, &new_element);
        }
    }
}


void
clnk_nms_init_cache(clnk_nms_cache_t* cache_ptr)
{
    /* Convert from Application opaque cache to internal form */
    clnk_nmsimpl_cache_t* icache_ptr = 
        (clnk_nmsimpl_cache_t*)cache_ptr;

    clnk_nms_param_t      param;
    clnk_nmsimpl_eindex_t eindex_next = 0;

    CLNK_NMS_ERROR_IF(!cache_ptr, "Null pointer", 0, 0);

    if (!cache_ptr) return;

    /* First, initialize the parameter pointers */
    for (param = 0;
          param < CLNK_NMS_PARAM__MAX;
          param++)
    {
        const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(param);

        clnk_nmsimpl_eindex_t eindex_count_this;

        switch (pdef_ptr->cardinality)
        {
        case                                                        CLNK_NMS_CARDINALITY_NETWORK:
            eindex_count_this = CLNK_NMS_ELEMENTS_GIVEN_CARDINALITY(CLNK_NMS_CARDINALITY_NETWORK);
            break;
        case                                                        CLNK_NMS_CARDINALITY_NODE:
            eindex_count_this = CLNK_NMS_ELEMENTS_GIVEN_CARDINALITY(CLNK_NMS_CARDINALITY_NODE);
            break;
        case                                                        CLNK_NMS_CARDINALITY_LINK:
            eindex_count_this = CLNK_NMS_ELEMENTS_GIVEN_CARDINALITY(CLNK_NMS_CARDINALITY_LINK);
            break;
        default:
            eindex_count_this = CLNK_NMSIMPL_CACHE_EINDEX__MAX;
            CLNK_NMS_ERROR_IF(1, "Unhandled case", 0, 0);
        }

        if (eindex_count_this + eindex_next > CLNK_NMSIMPL_CACHE_EINDEX__MAX)
        {
            CLNK_NMS_ERROR_IF(1, "Unexpected cache exhaustion %d %d", 
                              param, eindex_count_this);
            icache_ptr->param_start_index[param] = -1;
            icache_ptr->param_count_index[param] = 0;
        }
        else
        {
            icache_ptr->param_start_index[param] = eindex_next;
            icache_ptr->param_count_index[param] = eindex_count_this;
            eindex_next += eindex_count_this;
        }
    }

    /* Finally, initialize all the element locations */
    for (eindex_next = 0;
          eindex_next < CLNK_NMSIMPL_CACHE_EINDEX__MAX;
          eindex_next++)
    {
        icache_ptr->elements[eindex_next]     = CLNK_NMS_ELEMENT_HANDLE_INVALID;
        icache_ptr->dispositions[eindex_next] = CLNK_NMS_DISPOSITION_UNKNOWN;
    }
}


/** TODO DOC
 * Adjusts the specified element depending upon the current 
 * set of nodes in the network.  Should the Element be 
 * eliminated since it doesnt exist in the network?  Should 
 * an Element have a value in the network? 
 */
void
clnk_nmsimpl_adjust_cache_element(clnk_nmsimpl_cache_t*   icache_ptr,
                                  const clnk_nms_pdef_t*  pdef_ptr,
                                  clink_node_id_t         queried_node_id,
                                  clink_node_id_t         remote_node_id,
                                  clink_nodemask_t        nodes_in_network)
{
    clnk_nmsimpl_eindex_t eindex = 
        clnk_nmsimpl_find_eindex(icache_ptr, pdef_ptr, 
                                 queried_node_id, remote_node_id);

    SYS_BOOLEAN element_has_value_in_current_nw = SYS_TRUE;

    /* First figure out if the designated link is present in the network */
    switch (pdef_ptr->cardinality)
    {
    case CLNK_NMS_CARDINALITY_LINK:
        element_has_value_in_current_nw = 
            element_has_value_in_current_nw       &&
            (queried_node_id != remote_node_id)   &&
            INCTYPES_NODE_IN_NODEMASK(remote_node_id, nodes_in_network);
    case CLNK_NMS_CARDINALITY_NODE:
        element_has_value_in_current_nw = 
            element_has_value_in_current_nw &&
            INCTYPES_NODE_IN_NODEMASK(queried_node_id, nodes_in_network);
        break;
    case CLNK_NMS_CARDINALITY_NETWORK:
        element_has_value_in_current_nw = SYS_TRUE;
        break;
    default:
        CLNK_NMS_ERROR_IF(1, "Unhandled case: cardinality is %d for param %d", 
                          pdef_ptr->cardinality, pdef_ptr->param);
    }

    if (element_has_value_in_current_nw)
    {
        if (icache_ptr->dispositions[eindex] == CLNK_NMS_DISPOSITION_NIN)
        {
            clnk_nmsimpl_set_element_in_cache(icache_ptr, pdef_ptr, NULL,
                                              queried_node_id, remote_node_id,
                                              NULL, CLNK_NMS_DISPOSITION_UNKNOWN);
        }
    }
    else
    {
        clnk_nmsimpl_set_element_in_cache(icache_ptr, pdef_ptr, NULL,
                                          queried_node_id, remote_node_id,
                                          NULL, CLNK_NMS_DISPOSITION_NIN);
    }
}

void
clnk_nmsimpl_consistify_cache(clnk_nmsimpl_cache_t*  icache_ptr, 
                              clink_nodemask_t       nodes_in_network)
{
    clnk_nms_param_t param;

    for (param = 0; param < CLNK_NMS_PARAM__MAX; param++)
    {
        clink_node_id_t queried_node_id;
        clink_node_id_t remote_node_id;
        const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(param);

        /* A network attribute is already consistent */
        if (pdef_ptr->cardinality == CLNK_NMS_CARDINALITY_NETWORK) continue;

        for (queried_node_id = 0;
             queried_node_id < MAX_NUM_NODES;
             queried_node_id++)
        {
            for (remote_node_id = 0;
                 remote_node_id < MAX_NUM_NODES;
                 remote_node_id++)
            {
                clnk_nmsimpl_adjust_cache_element(icache_ptr, pdef_ptr,
                                                  queried_node_id,
                                                  remote_node_id,
                                                  nodes_in_network);

                if (pdef_ptr->cardinality == CLNK_NMS_CARDINALITY_NODE) break;
            }
        }
    }
}


clnk_nms_eview_cptr
clnk_nms_view_element(const clnk_nms_cache_t*  cache_ptr,
                      clnk_nms_param_t         param,
                      clink_node_id_t          queried_node_id,
                      clink_node_id_t          remote_node_id,
                      clnk_nms_disposition_t*  disposition_ptr)
{
    /* Convert from Application opaque cache to internal form */
    const clnk_nmsimpl_cache_t* icache_ptr = 
        (const clnk_nmsimpl_cache_t*)cache_ptr;

    enum clnk_nms_element_pool_name_t   poolname;
    enum clnk_nms_element_block_size_t  size;

    clnk_nmsimpl_eindex_t     eindex;
    clnk_nms_eview_cptr       retval;
    clnk_nms_element_handle_t handle;

    const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(param);

    CLNK_NMS_ERROR_IF(!cache_ptr, "Null pointer", 0, 0);
    if (!cache_ptr) return NULL;

    eindex = clnk_nmsimpl_find_eindex(icache_ptr, pdef_ptr, 
                                      queried_node_id,     
                                      remote_node_id);     

    clnk_nmsimpl_get_element_block_info(pdef_ptr, &poolname, &size);

    handle = icache_ptr->elements[eindex];

    if (handle == CLNK_NMS_ELEMENT_HANDLE_INVALID)
    {
        retval = NULL;
    }
    else
    {
        retval = clnk_nms_context.dereference(poolname, size, handle);
    }

    if (disposition_ptr)
    {
        *disposition_ptr = icache_ptr->dispositions[eindex];
    }

    return retval;
}


void 
clnk_unpack_fmr_element(SYS_UINT16   packed_val,
                        SYS_UINT32*  cplen_ptr,
                        SYS_UINT32*  bits_ptr,
                        SYS_UINT32*  rate_ptr)
{
    /**
     *  = BitsPerOFDM * (((1,000,000,000/((FFT_LENGTH+CPLen) * SLOTLEN_IN_NANOSECS)) * 192)/208);
     */
    SYS_UINT32 cplen = ((packed_val >> 11) << 1) + 10;
    SYS_UINT32 bits = packed_val & 0x7ff;
    SYS_UINT32 rate = bits * (50*1000*1000 * 12 / 13 / (256+cplen));

    if (cplen_ptr) *cplen_ptr = cplen;
    if (bits_ptr)  *bits_ptr  = bits;
    if (rate_ptr)  *rate_ptr  = rate;
}


/**
 * Used during the first stage of filling a Cache.  This routine 
 * makes all Dispositions for current Elements in the Cache stale.
 * This also changes the Dispositions from ignored to unknown
 * for all Elements that are requested.
 */
void
clnk_nmsimpl_staleify_cache( clnk_nms_cache_t*       cache_ptr,
                             const clnk_nms_eset_t*  eset_ptr)
{
    clnk_nmsimpl_cache_t* icache_ptr = 
        (clnk_nmsimpl_cache_t*)cache_ptr;

    clnk_nms_param_t param;
    clink_node_id_t  queried_node_id;
    clink_node_id_t  remote_node_id;

    for (param = 0; param < CLNK_NMS_PARAM__MAX; param++)
    {
        const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(param);

        for (queried_node_id = 0; 
             queried_node_id < MAX_NUM_NODES; 
             queried_node_id++)
        {
            for (remote_node_id = 0; 
                 remote_node_id < MAX_NUM_NODES; 
                 remote_node_id++)
            {
                clnk_nms_disposition_t prev_disposition;
                clnk_nms_disposition_t new_disposition;

                clnk_nms_view_element(cache_ptr, param,
                                      queried_node_id,
                                      remote_node_id,
                                      &prev_disposition);

                switch (prev_disposition)
                {
                case CLNK_NMS_DISPOSITION_FRESH:
                    new_disposition = CLNK_NMS_DISPOSITION_STALE;
                    break;

                case CLNK_NMS_DISPOSITION_UNRETRIEVABLE:
                case CLNK_NMS_DISPOSITION_UNKNOWN:
                case CLNK_NMS_DISPOSITION_NIN:
                case CLNK_NMS_DISPOSITION_STALE:
                    new_disposition = prev_disposition;
                    break;

                default:
                    new_disposition = CLNK_NMS_DISPOSITION_UNKNOWN;
                    CLNK_NMS_ERROR_IF(1, "Unhandled case", 0, 0);
                    break;
                }

                if (prev_disposition != new_disposition)
                {
                    clnk_nmsimpl_eindex_t eindex = 
                        clnk_nmsimpl_find_eindex(icache_ptr, pdef_ptr,
                                                 queried_node_id,
                                                 remote_node_id);
                    icache_ptr->dispositions[eindex] = new_disposition;
                }

                /* Only loop for nonzero remote nodes if this is LINK */
                if (pdef_ptr->cardinality == CLNK_NMS_CARDINALITY_LINK) continue;

                break;
            }

            /* Only loop for nonzero queried nodes if NODE or LINK */
            if (pdef_ptr->cardinality == CLNK_NMS_CARDINALITY_NETWORK) break;
        }
    }
}


/**
 * RETRIEVE INFORMATION FROM LOCAL API CALLS.
 * 
 * This routine is responsible for populating the cache with refreshed
 * information that comes from local API calls.  No remote queries
 * such as L2ME collections are needed here.
 */
void
clnk_nmsimpl_fill_cache_local_apis(clnk_ctx_t*             pctx,
                                   clnk_nmsimpl_cache_t*   icache_ptr,
                                   const clnk_nms_eset_t*  eset_ptr,
                                   clink_nodemask_t*       nw_nodemask_ptr)
{
    /* Our clink interface can fail under unknown circumstances. 
     * If encountered, set this value and 'break' from the 
     * 'do while (0)' block. */
    SYS_BOOLEAN cannot_continue_local_apis = SYS_FALSE;

    ClnkDef_MyNodeInfo_t  my_node_info;

    qos_events_t          qos_events;

    clnk_nms_param_t  param;

    clink_node_id_t   queried_node_id;

    /* Collect the actual information from my node: always */
    if (clnk_get_my_node_info(pctx, &my_node_info, SYS_FALSE))
    {
        CLNK_NMS_ERROR_IF(1, "Failed get my node info!", 0, 0);
        cannot_continue_local_apis = SYS_TRUE;
    }
    if (cannot_continue_local_apis) return;
    *nw_nodemask_ptr = my_node_info.NetworkNodeBitMask;

    /* Collect qos event information: always */
    if (clnk_get_event_counts(pctx,&qos_events))
    {
        CLNK_NMS_ERROR_IF(1, "Failed get qos events!", 0, 0);
        cannot_continue_local_apis = SYS_TRUE;
    }
    if (cannot_continue_local_apis) return;

    /* Now we have all the information to alter the Cache at least for 
     * Elements retrievable with straight API calls.  Start with Network
     * cardinality. */
    for (param = 0;
         param < CLNK_NMS_PARAM__MAX;
         param++)
    {
        const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(param);
        clnk_nms_eview_t  eview;

        if(pdef_ptr->cardinality != CLNK_NMS_CARDINALITY_NETWORK) continue;

        /** Skip parameters that were not collected during this
         *  phase of local host query */
        if (!CLNK_NMS_SOURCE_IS_BATCH_APIS(pdef_ptr->source)) continue;

        INCTYPES_SAFE_VAR_ZERO(eview.m);

        switch (pdef_ptr->param)
        {
        case CLNK_NMS_PARAM_NW_FREQUENCY:                eview.s.uint32 = my_node_info.RFChanFreq;              break;
        case CLNK_NMS_PARAM_NW_NODE_ID_NC:               eview.s.uint32 = my_node_info.CMNodeId;                break;
        case CLNK_NMS_PARAM_NW_NODE_ID_LOCAL:            eview.s.uint32 = my_node_info.NodeId;                  break;
        case CLNK_NMS_PARAM_NW_NODEMASK:                 eview.s.uint32 = my_node_info.NetworkNodeBitMask;      break;
        case CLNK_NMS_PARAM_NW_PRIVACY_ENABLED:          eview.s.uint32 = my_node_info.PrivacyStat;             break;
        case CLNK_NMS_PARAM_NW_STATUS:                   eview.s.uint32 = my_node_info.LinkStatus;              break;
        case CLNK_NMS_PARAM_NW_QOS_BW_EXCEEDED_ERRORS:   eview.s.uint32 = qos_events.count_bw_exceeded;         break;
        case CLNK_NMS_PARAM_NW_QOS_ENABLE_DISABLE_COUNT: eview.s.uint32 = qos_events.count_qos_enabled;         break;
        case CLNK_NMS_PARAM_NW_TOPOLOGY_EVENT:           eview.s.uint32 = qos_events.count_topology_changed;    break;
        case CLNK_NMS_PARAM_NW_VERSION:                  eview.s.uint32 = GET_NET_MOCA(my_node_info.MocaField); break;
        default:
            CLNK_NMS_ERROR_IF(1, "Unhandled case: param is %d", 
                              pdef_ptr->param, 0);
        }

        clnk_nmsimpl_set_element_in_cache(icache_ptr, pdef_ptr, eset_ptr,
                                          0, 0, &eview,
                                          CLNK_NMS_DISPOSITION_FRESH);
    }

    /* Now fill in Node Cardinality information. */
    for (queried_node_id = 0; 
         queried_node_id < MAX_NUM_NODES; 
         queried_node_id++)
    {

        SYS_BOOLEAN            valid_net_node = SYS_FALSE;
        ClnkDef_NetNodeInfo_t  nni;

        for (param = 0;
             param < CLNK_NMS_PARAM__MAX;
             param++)
        {

            const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(param);
    
            if(pdef_ptr->cardinality != CLNK_NMS_CARDINALITY_NODE) continue;
    
            /** Skip parameters that were not collected during this
             *  phase of local host query */
            if (!CLNK_NMS_SOURCE_IS_BATCH_APIS(pdef_ptr->source)) continue;

            if (!clnk_nms_eset_test_single(eset_ptr, pdef_ptr,
                                           queried_node_id, 0)) continue;

            /** Current element is in the eset.  Must get a value */
            if (!valid_net_node && 
                clnk_get_network_node_info(pctx,&nni,queried_node_id))
            {
                CLNK_NMS_ERROR_IF(1, "Failed get network node info %d!", 
                                  queried_node_id, 0);
                cannot_continue_local_apis = SYS_TRUE;
            }
            if (cannot_continue_local_apis) return;
            valid_net_node = SYS_TRUE;

            /** Now we can start the cache add process */
            {
                clnk_nms_eview_t  eview;
                SYS_BOOLEAN is_moca_1p1_node;
                SYS_BOOLEAN is_moca_1p1_pro_tem_node;
                SYS_BOOLEAN is_moca_1p1_advanced_node;

                is_moca_1p1_node = 
                    ( MOCA_FROM_NPS(nni.NodeProtocolSupport) > MOCA_10 ) ? 
                    SYS_TRUE : SYS_FALSE;

                is_moca_1p1_pro_tem_node = 
                    (nni.NodeProtocolSupport & 0x180) == 0x080;

                is_moca_1p1_advanced_node = 
                    is_moca_1p1_node && !is_moca_1p1_pro_tem_node;

                switch (pdef_ptr->param)
                {
                case CLNK_NMS_PARAM_NODE_CAN_AGGREGATE:  
                    eview.s.uint32 = ( 
                                       is_moca_1p1_advanced_node && 
                                       (nni.NodeProtocolSupport & 0x80) == 0x00
                                     ) ? 1 : 0 ;
                    break;
                case CLNK_NMS_PARAM_NODE_CAN_TX_256_QAM:
                    eview.s.uint32 = ( 
                                        is_moca_1p1_node &&
                                        ((nni.NodeProtocolSupport & 0x10) == 0x10)
                                     ) ? 1 : 0 ;
                    break;
                case CLNK_NMS_PARAM_NODE_IS_PREFERRED_NC:
                    eview.s.uint32 = (
                                        is_moca_1p1_node &&
                                        ((nni.NodeProtocolSupport & 0x40) == 0x40)
                                     ) ? 1 : 0 ;
                    break;
                case CLNK_NMS_PARAM_NODE_MAX_NW_SIZE:
                    eview.s.uint32 = is_moca_1p1_advanced_node ? 16 : 8 ;
                    break;
                case CLNK_NMS_PARAM_NODE_MOCA_GUID:
                    eview.m.mac_address[0] = (nni.GUID64High >> 24) & 0xFF;
                    eview.m.mac_address[1] = (nni.GUID64High >> 16) & 0xFF;
                    eview.m.mac_address[2] = (nni.GUID64High >>  8) & 0xFF;
                    eview.m.mac_address[3] = (nni.GUID64High >>  0) & 0xFF;
                    eview.m.mac_address[4] = (nni.GUID64Low  >> 24) & 0xFF;
                    eview.m.mac_address[5] = (nni.GUID64Low  >> 16) & 0xFF;
                    break;
                case CLNK_NMS_PARAM_NODE_MOCA_VERSION:
                    eview.s.uint32 = MOCA_FROM_NPS(nni.NodeProtocolSupport);
                    if (!eview.s.uint32) eview.s.uint32 += 0x10;
                    break;
                default:
                    CLNK_NMS_ERROR_IF(1, "Unhandled case", 0, 0);
                }

                clnk_nmsimpl_set_element_in_cache(icache_ptr, pdef_ptr, eset_ptr,
                                                  queried_node_id, 0,
                                                  &eview, 
                                                  CLNK_NMS_DISPOSITION_FRESH);
            }
        }
    }
}


/**
 * GETTING FULL MESH RATES
 * A utility used typically during filling a Cache that first checks
 * to see if the caller wants to populate the full mesh rates in the Cache
 * and then, if so, retrieves those values.
 * 
 * This is an operation backed by L2ME operations.
 */
void
clnk_nmsimpl_fill_cache_fmr(clnk_ctx_t*             pctx,
                            clnk_nmsimpl_cache_t*   icache_ptr,
                            const clnk_nms_eset_t*  eset_ptr,
                            SYS_UINT32              timeout_ms,
                            void   (*sleep_func)(SYS_UINT32 sleep_ms))
{
    /* First figure out if ANY request exists for full mesh data */
    peer_rates_t              peer;
    peer_rates_entry_status_t peer_entry;

    const clnk_nms_pdef_t*    pdef_rate_tx_gcd_ptr = 
        clnk_nms_get_pdef(CLNK_NMS_PARAM_PHY_RATE_TX_GCD);

    const clnk_nms_pdef_t*    pdef_rate_tx_p2p_ptr = 
        clnk_nms_get_pdef(CLNK_NMS_PARAM_PHY_RATE_TX_P2P);

    const clnk_nms_pdef_t*    pdef_cplen_tx_gcd_ptr = 
        clnk_nms_get_pdef(CLNK_NMS_PARAM_PHY_CPLEN_TX_GCD);

    const clnk_nms_pdef_t*    pdef_cplen_tx_p2p_ptr = 
        clnk_nms_get_pdef(CLNK_NMS_PARAM_PHY_CPLEN_TX_P2P);

    clink_node_id_t queried_node_id;
    clink_node_id_t remote_node_id;

    clnk_impl_timer_t     interface_timer;
    clnk_impl_timer_start(&interface_timer, timeout_ms);

    /* If none of the FMR attributes are requested, then exit. */
    if (!(0 
          || clnk_nms_eset_test_any(eset_ptr, pdef_rate_tx_gcd_ptr, 
                                    CLINK_NODEMASK_ALL, CLINK_NODEMASK_ALL)
          || clnk_nms_eset_test_any(eset_ptr, pdef_rate_tx_p2p_ptr, 
                                    CLINK_NODEMASK_ALL, CLINK_NODEMASK_ALL)
          || clnk_nms_eset_test_any(eset_ptr, pdef_cplen_tx_gcd_ptr, 
                                    CLINK_NODEMASK_ALL, CLINK_NODEMASK_ALL)
          || clnk_nms_eset_test_any(eset_ptr, pdef_cplen_tx_p2p_ptr, 
                                    CLINK_NODEMASK_ALL, CLINK_NODEMASK_ALL)
       ) )     return;

    /* We must collect FMR now */

    if (clnk_get_peer_rates(pctx, &peer, &peer_entry, 
                            clnk_impl_timer_ms_remaining(&interface_timer),
                            sleep_func))
    {
        /** This is an error case. We tried and failed to collect
         * all of the information.  Thus it is up to the Application
         * to deduce the problem. */

        /* CLNK_NMS_ERROR_IF(1, "Failed get peer rates!", 0, 0);
             I commented the above out since it is known that  
             peer rates can fail for valid L2ME reasons. */
        return;
    }

    /* The FMR query worked so time to install the values into 
     * the Cache. */
    for (queried_node_id = 0; 
         queried_node_id < MAX_NUM_NODES; 
         queried_node_id++)
    {
        for (remote_node_id = 0; 
             remote_node_id < MAX_NUM_NODES; 
             remote_node_id++)
        {
            SYS_BOOLEAN skip = SYS_FALSE;

            clnk_nms_eview_t       eview_gcd_tx_cplen;
            clnk_nms_eview_t       eview_gcd_tx_rate;
            clnk_nms_disposition_t disp;

            switch (peer_entry.entry_status[queried_node_id][remote_node_id])
            {
            case PEER_RATES_NO_LINK_TO_QUERY:  disp = CLNK_NMS_DISPOSITION_NIN;           break;
            case PEER_RATES_CANNOT_QUERY_LINK: disp = CLNK_NMS_DISPOSITION_UNRETRIEVABLE; break;
            case PEER_RATES_VALID:             disp = CLNK_NMS_DISPOSITION_FRESH;         break;
            case PEER_RATES_QUERY_FAILED:      skip = SYS_TRUE;                           break;
            default:
                CLNK_NMS_ERROR_IF(1, "Unhandled case", 0, 0);
                skip = SYS_TRUE;
            }

            if (skip) continue;

            clnk_unpack_fmr_element(peer.rates[queried_node_id][remote_node_id],
                                    &eview_gcd_tx_cplen.s.uint32,
                                    NULL,
                                    &eview_gcd_tx_rate.s.uint32);

            if (queried_node_id == remote_node_id)
            {
                clnk_nmsimpl_set_element_in_cache(icache_ptr, 
                                                  pdef_cplen_tx_gcd_ptr, 
                                                  eset_ptr, queried_node_id, 0,
                                                  &eview_gcd_tx_cplen, disp);

                clnk_nmsimpl_set_element_in_cache(icache_ptr, 
                                                  pdef_rate_tx_gcd_ptr, 
                                                  eset_ptr, queried_node_id, 0,
                                                  &eview_gcd_tx_rate, disp);
            }
            else
            {
                clnk_nmsimpl_set_element_in_cache(icache_ptr, 
                                                  pdef_cplen_tx_p2p_ptr, 
                                                  eset_ptr, queried_node_id, 
                                                  remote_node_id,
                                                  &eview_gcd_tx_cplen, disp);

                clnk_nmsimpl_set_element_in_cache(icache_ptr, 
                                                  pdef_rate_tx_p2p_ptr, 
                                                  eset_ptr, queried_node_id, 
                                                  remote_node_id,
                                                  &eview_gcd_tx_rate, disp);
            }
        }
    }
}


#if FEATURE_PUPQ_GQ

/** Create ungathered_eset with all needed l2me items; filter by current
 *  nodes.  Includes all regardless of size.
 */


/**
 * Populate an Eset where each set bit indicates an Element that was
 * requested by another eset and must be retrievable via L2ME gather
 * queryables only.  Also, the nodes must be in the network.
 */
void
clnk_nmsimpl_eset_filter_gq_only(const clnk_nms_eset_t* requested_eset_ptr, 
                                 clink_nodemask_t       l2me_nodemask,
                                 clnk_nms_eset_t*       needed_eset_ptr)
{
    clnk_nms_param_t param;

    INCTYPES_SAFE_PTR_ZERO(needed_eset_ptr);
    for (param = 0;
         param < CLNK_NMS_PARAM__MAX;
         param++)
    {
        const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(param);

        /** Will delete this block later once all are implemented. */
        SYS_BOOLEAN is_implemented_l2me = SYS_FALSE;

        switch (param)
        {
#if NMS_ADD_NEW_PARAMETER_LOCATION
    /** When adding a new parameter, come here. */
#endif
        /** Order is functional specification order */
        case CLNK_NMS_PARAM_NODE_ADM_REQ_RX:
        case CLNK_NMS_PARAM_NODE_LINK_TIME:
        case CLNK_NMS_PARAM_NODE_LINK_UP_EVENT:
        case CLNK_NMS_PARAM_NODE_MFR_HW_VER:
        case CLNK_NMS_PARAM_NODE_MFR_SW_VER:
        case CLNK_NMS_PARAM_NODE_MFR_VENDOR_ID:
        case CLNK_NMS_PARAM_NODE_PERSONALITY:
        case CLNK_NMS_PARAM_PHY_MTRANS_P2P_RX:
        case CLNK_NMS_PARAM_PHY_MTRANS_P2P_RX_ERRORS:
        case CLNK_NMS_PARAM_PHY_PROFILE_TX_GCD:
        case CLNK_NMS_PARAM_PHY_PROFILE_TX_P2P:
        case CLNK_NMS_PARAM_PHY_EST_P2P_RX_DBM:
        case CLNK_NMS_PARAM_PHY_POWER_REDUCTION_P2P_TX:
        case CLNK_NMS_PARAM_NODE_LINK_TIME_BAD:
        case CLNK_NMS_PARAM_PHY_POWER_CTL_PHY_TARGET:
        case CLNK_NMS_PARAM_NODE_FREQ_CAPABILITY_MASK:
        case CLNK_NMS_PARAM_NODE_FREQ_CHANNEL_MASK:
        case CLNK_NMS_PARAM_NODE_PQOS_CLASSIFY_MODE:
        case CLNK_NMS_PARAM_PHY_MTRANS_TX_PKT_CTR:
            is_implemented_l2me = SYS_TRUE;
        }

        if (!is_implemented_l2me) continue;

        if (pdef_ptr->source != CLNK_NMS_SOURCE_GENERIC_L2ME) continue;

        /** At this point we know that the parameter is a supported
         *  one. */

        if (pdef_ptr->cardinality == CLNK_NMS_CARDINALITY_NODE)
        {
            needed_eset_ptr->included[param][0] = 
                requested_eset_ptr->included[param][0] & l2me_nodemask;
        }
        else if (pdef_ptr->cardinality == CLNK_NMS_CARDINALITY_LINK)
        {
            clink_node_id_t remote_node_id;

            for (remote_node_id = 0;
                 remote_node_id < MAX_NUM_NODES;
                 remote_node_id++)
            {
                if (INCTYPES_NODE_IN_NODEMASK(remote_node_id,l2me_nodemask)) 
                {
                    needed_eset_ptr->included[param][remote_node_id] = 
                        requested_eset_ptr->included[param][remote_node_id] & 
                        l2me_nodemask;
                }
            }
        }
    }
}


/** 
 * This operation takes two esets, one representing the items that
 * are desired and the other representing the items that can actually
 * be retrieved, and then fills the Cache with dispositions marking
 * the difference.
 */
void
clnk_nmsimpl_fill_unavailable(clnk_nmsimpl_cache_t*   icache_ptr,  
                              const clnk_nms_eset_t*  requested_eset_ptr,
                              const clnk_nms_eset_t*  available_eset_ptr)
{
    clnk_nms_param_t param;
    clink_node_id_t  queried_node_id;
    clink_node_id_t  remote_node_id;

    for (param = 0; 
         param < CLNK_NMS_PARAM__MAX; 
         param++)
    {
        const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(param);

        if (pdef_ptr->source != CLNK_NMS_SOURCE_GENERIC_L2ME) continue;

        for (queried_node_id = 0;
             queried_node_id < MAX_NUM_NODES;
             queried_node_id++)
        {
            for (remote_node_id = 0;
                 remote_node_id < MAX_NUM_NODES;
                 remote_node_id++)
            {
                if (INCTYPES_NODE_IN_NODEMASK(
                        queried_node_id,
                        requested_eset_ptr->included[param][remote_node_id])) continue;

                /* We know the current element is NOT available. */

                if (!INCTYPES_NODE_IN_NODEMASK(
                        queried_node_id,
                        requested_eset_ptr->included[param][remote_node_id])) continue;

                /* This op will only sub in a value if the value is requested 
                 *  by the specified eset. */
                clnk_nmsimpl_set_element_in_cache(icache_ptr,pdef_ptr,
                                                  requested_eset_ptr,
                                                  queried_node_id,
                                                  remote_node_id,
                                                  NULL,
                                                  CLNK_NMS_DISPOSITION_UNRETRIEVABLE);

                /* Only continue the loop if this is a LINK cardinality. */
                if (pdef_ptr->cardinality == CLNK_NMS_CARDINALITY_LINK) continue;
                break;
            }

            /* We are done if the parameter is NETWORK cardinality */
            if (pdef_ptr->cardinality == CLNK_NMS_CARDINALITY_NETWORK) break;
        }
    }
}


/**
 * Statistics maintained by functions that manipulate the contents
 * of a submit.
 */
typedef struct {
    /** The number of words each node will attempt to pack into
     *  its response to the submit.  This won't actually happen
     *  as expected if the L2ME limit is exceeded. */
    SYS_UINT32 response_words[MAX_NUM_NODES];
    /** A nodemask where each set bit corresponds to a node 
     *  whose response as directed by this submit WILL exceed
     *  L2ME limits and therefore will not work as expected. */
    clink_nodemask_t  nodes_too_big;
    /** The size of the actual concatenation expected as a 
     *  result of sending this submit.  This won't actually happen
     *  as expected if the L2ME limit is exceeded. */
    SYS_UINT32 concat_words;
} clnk_nmsimpl_gqs_stats_t;


/**
 * Utility that evaluates a submit for retrieving information
 * about queryable items to determine the exact sizes of the
 * individual node responses as well as the size of the total
 * expected concatenation.  Be aware it is possible that even
 * if the submit is correctly formed, fewer items may be returned
 * due to nodes failing to provide correct responses.  
 * 
 * This operation appears to have several loops but finally its
 * running time scales with the number of set bits in the Submit.
 * In a first generation release of this service, a few extra loops
 * won't matter nearly as much as a solid implementation.
 */
void
clnk_nmsimpl_calc_gqs_stats(
        const Mrt_Qbl_GetQueryablesSubmit_t* sub_ptr,
        clnk_nmsimpl_gqs_stats_t*            gqs_ptr)
{
    const SYS_UINT32                    bits_in_LINK_SZ32_QUERIED = 
        clnk_util_count_set_bits      (sub_ptr->LINK_SZ32_QUERIED);

    const SYS_UINT32                    bits_in_LINK_SZ32_REMOTE = 
        clnk_util_count_set_bits      (sub_ptr->LINK_SZ32_REMOTE);

    const SYS_UINT32                    bits_in_LINK_SZ32_W = 
        clnk_util_count_set_bits_array(sub_ptr->LINK_SZ32_W,
                    INCTYPES_ARRAY_LEN(sub_ptr->LINK_SZ32_W));

    const SYS_UINT32                    bits_in_NODE_SZ32_QUERIED = 
        clnk_util_count_set_bits      (sub_ptr->NODE_SZ32_QUERIED);

    const SYS_UINT32                    bits_in_NODE_SZ32_W = 
        clnk_util_count_set_bits_array(sub_ptr->NODE_SZ32_W,
                    INCTYPES_ARRAY_LEN(sub_ptr->NODE_SZ32_W));

    const SYS_UINT32                    bits_in_LINK_SZ1_QUERIED = 
        clnk_util_count_set_bits      (sub_ptr->LINK_SZ1_QUERIED);

    const SYS_UINT32                    bits_in_LINK_SZ1_REMOTE = 
        clnk_util_count_set_bits      (sub_ptr->LINK_SZ1_REMOTE);

    const SYS_UINT32                    bits_in_LINK_SZ1_W = 
        clnk_util_count_set_bits_array(sub_ptr->LINK_SZ1_W,
                    INCTYPES_ARRAY_LEN(sub_ptr->LINK_SZ1_W));

    const SYS_UINT32                    bits_in_NODE_SZ1_QUERIED = 
        clnk_util_count_set_bits      (sub_ptr->NODE_SZ1_QUERIED);

    const SYS_UINT32                    bits_in_NODE_SZ1_W = 
        clnk_util_count_set_bits_array(sub_ptr->NODE_SZ1_W,
                    INCTYPES_ARRAY_LEN(sub_ptr->NODE_SZ1_W));

    const SYS_UINT32    sz1_count = 
        (  bits_in_LINK_SZ1_QUERIED *
           bits_in_LINK_SZ1_REMOTE  * 
           bits_in_LINK_SZ1_W          ) + 
        (  bits_in_NODE_SZ1_QUERIED *
           bits_in_NODE_SZ1_W          );

    const SYS_UINT32    sz32_count = 
        (  bits_in_LINK_SZ32_QUERIED *
           bits_in_LINK_SZ32_REMOTE  * 
           bits_in_LINK_SZ32_W          ) + 
        (  bits_in_NODE_SZ32_QUERIED *
           bits_in_NODE_SZ32_W          );

    clink_node_id_t  node_id;


    /** Simplistic input validation */
    CLNK_NMS_FATAL_IF(!(sub_ptr && gqs_ptr), "Null pointer", 0, 0);

    INCTYPES_SAFE_PTR_ZERO(gqs_ptr);

    for (node_id = 0; node_id < MAX_NUM_NODES; node_id++)
    {
        const SYS_UINT32 row = sz1_count + 32 * sz32_count;

        gqs_ptr->response_words[node_id] = row;
        if (row > (MRT_BYTES_ACK_MAX/4)) gqs_ptr->nodes_too_big |= 1UL << node_id;
        gqs_ptr->concat_words += row;
    }
}


/**
 * Given an Eset, return SYS_TRUE if and only if there is at least
 * one element in the eset that needs to be retrieved through L2ME.
 */
SYS_BOOLEAN
clnk_nmsimpl_eset_has_l2me_elements(const clnk_nms_eset_t* eset_ptr)
{
    SYS_UINT32 psmap_index;
    for (psmap_index = 0;
         psmap_index < INCTYPES_ARRAY_LEN(clnk_nmsimpl_psmap);
         psmap_index++)
    {
        const clnk_nmsimpl_psmap_entry_t* entry_ptr = 
             &clnk_nmsimpl_psmap[psmap_index];

        const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(entry_ptr->param);

        if (clnk_nms_eset_test_any(eset_ptr,
                                   pdef_ptr,
                                   CLINK_NODEMASK_ALL, 
                                   CLINK_NODEMASK_ALL)) return SYS_TRUE;
    }

    return SYS_FALSE;
}


/**
 * Given an Eset, return SYS_TRUE if and only if there is only
 * one element in the eset that needs to be retrieved through L2ME.
 */
SYS_BOOLEAN
clnk_nmsimpl_eset_has_only_one_l2me_element(const clnk_nms_eset_t* eset_ptr, clnk_nms_param_t* param_ptr)
{
    SYS_UINT32 psmap_index;
    SYS_UINT32 num_l2me_elements = 0;
    clnk_nms_pdef_t* element_found_pdef_ptr;
    SYS_BOOLEAN retval;

    for (psmap_index = 0;
         psmap_index < INCTYPES_ARRAY_LEN(clnk_nmsimpl_psmap);
         psmap_index++)
    {
        const clnk_nmsimpl_psmap_entry_t* entry_ptr = 
             &clnk_nmsimpl_psmap[psmap_index];

        const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(entry_ptr->param);

        if (clnk_nms_eset_test_any(eset_ptr,
                                   pdef_ptr,
                                   CLINK_NODEMASK_ALL, 
                                   CLINK_NODEMASK_ALL))
        {
            element_found_pdef_ptr = pdef_ptr;
            num_l2me_elements++;
            *param_ptr = pdef_ptr->param;
        }
    }

    // return TRUE only if only one l2me element exists and
    //      source == GENERIC_L2ME and
    //      cardinality == NODE and
    //      (type == UINT32 or type == INT32)
    if ((num_l2me_elements == 1) &&
        (element_found_pdef_ptr->source == CLNK_NMS_SOURCE_GENERIC_L2ME) &&
        (element_found_pdef_ptr->cardinality == CLNK_NMS_CARDINALITY_NODE) &&
        ((element_found_pdef_ptr->type == CLNK_NMS_TYPE_UINT32) ||
            (element_found_pdef_ptr->type == CLNK_NMS_TYPE_INT32)))
    {
        retval = SYS_TRUE;
    }
    else
    {
        retval = SYS_FALSE;
    }
    return retval;
}


/** Add elements to a submit by parameter id, specific queried node, and
 *  also remote node.  Keep statistics up to date.
 * 
 *  After consideration, I am taking a processor inefficient approach for
 *  this the first deployment.  Though it creates another multiple dimensional
 *  looping, I will brute force recalculate the statistics after blindly adding
 *  the element(s).  There are lots of ways to make this more efficient.
 */
void
clnk_nmsimpl_add_elements_to_submit(Mrt_Qbl_GetQueryablesSubmit_t* submit_ptr,
                                    clnk_nmsimpl_gqs_stats_t*      gqs_stats_ptr,
                                    const clnk_nms_pdef_t*         pdef_ptr,
                                    clink_nodemask_t               queried_mask,
                                    clink_nodemask_t               remote_mask)
{
    const clnk_nmsimpl_psmap_entry_t* entry_ptr = NULL;
    SYS_UINT32 index;
    SYS_UINT32* ptr;

    for (index = 0; 
         index < INCTYPES_ARRAY_LEN(clnk_nmsimpl_psmap); 
         index++)
    {
        if (pdef_ptr->param == clnk_nmsimpl_psmap[index].param)
        {
            entry_ptr       = &clnk_nmsimpl_psmap[index];
            break;
        }
    }

    if (!entry_ptr)
    {
        CLNK_NMS_FATAL_IF(1, "Unexpected %d %d", pdef_ptr->param, 0);
        return;
    }

    switch (entry_ptr->submit_section)
    {
    case   MRT_QBL_SECTION_LINK_SZ32:
        submit_ptr  ->     LINK_SZ32_QUERIED |= queried_mask;
        submit_ptr  ->     LINK_SZ32_REMOTE  |= remote_mask;
        ptr = &submit_ptr->LINK_SZ32_W[entry_ptr->word_offset_in_section];
        break;
    case   MRT_QBL_SECTION_NODE_SZ32:
        submit_ptr  ->     NODE_SZ32_QUERIED |= queried_mask;
        ptr = &submit_ptr->NODE_SZ32_W[entry_ptr->word_offset_in_section];
        break;
    case   MRT_QBL_SECTION_LINK_SZ1:
        submit_ptr  ->     LINK_SZ1_QUERIED |= queried_mask;
        submit_ptr  ->     LINK_SZ1_REMOTE  |= remote_mask;
        ptr = &submit_ptr->LINK_SZ1_W[entry_ptr->word_offset_in_section];
        break;
    case   MRT_QBL_SECTION_NODE_SZ1:
        submit_ptr  ->     NODE_SZ1_QUERIED |= queried_mask;
        ptr = &submit_ptr->NODE_SZ1_W[entry_ptr->word_offset_in_section];
        break;
    default:
        CLNK_NMS_FATAL_IF(1, "Unexpected %d %d", entry_ptr->submit_section, 0);
        return;
    }

    /** Now set the bit */
    *ptr |= 1UL << entry_ptr->bit_offset_in_word;

    clnk_nmsimpl_calc_gqs_stats(submit_ptr, gqs_stats_ptr);
}


/**
 * Function that takes current needed eset and updates 
 * the candidate submit such UFUNC still passes and at least one 
 * needed element is returned.  
 * 
 * This maximization is performed ONLY for nodes with link cardinality
 * of 1 word size.
 * 
 * Returns SYS_TRUE if any bits were set in the submit; SYS_FALSE
 *    otherwise.
 */
SYS_BOOLEAN
clnk_nmsimpl_add_link_sz1_to_submit(
    Mrt_Qbl_GetQueryablesSubmit_t* qb_submit_ptr,
    const clnk_nms_eset_t*         eset_ptr)
{
    clnk_nmsimpl_gqs_stats_t gqs_stats;
    clnk_nms_param_t         param;
    clink_node_id_t          queried_node_id = MAX_NUM_NODES;
    SYS_BOOLEAN              added_to_submit = SYS_FALSE;

    /** No queried node has been picked yet. */

    /**
     * Build some statistics.  
     */
    clnk_nmsimpl_calc_gqs_stats(qb_submit_ptr, &gqs_stats);

    /**
     * No use continuing to add bits to the submit if an error 
     * already created a submit too large to be realized.
     */
    if (gqs_stats.nodes_too_big || 
        gqs_stats.concat_words >= MRT_GEN_ENTRY_RSP_MAX_CONCAT_WORDS)
    {
        CLNK_NMS_ERROR_IF(1, "Unexpected too large: %d, %x", 
                          gqs_stats.concat_words, gqs_stats.nodes_too_big);
        return SYS_FALSE;
    }

    for (param = 0; param < CLNK_NMS_PARAM__MAX; param++)
    {
        const clnk_nms_pdef_t* pdef_ptr;
        clink_nodemask_t remote_nodemask = CLINK_NODEMASK_NONE;

        pdef_ptr = clnk_nms_get_pdef(param);

        if (pdef_ptr->cardinality != CLNK_NMS_CARDINALITY_LINK) continue;

        if (!CLNK_NMS_TYPE_IS_SMALL(pdef_ptr->type)) continue;

        if ((MAX_NUM_NODES + gqs_stats.concat_words) >=
             MRT_GEN_ENTRY_RSP_MAX_CONCAT_WORDS) continue;

        /** This is true if we haven't yet picked a queried node */
        if (queried_node_id >= MAX_NUM_NODES)
        {
            remote_nodemask = CLINK_NODEMASK_NONE;

            /* Now pick a node */
            for (queried_node_id = 0; 
                 queried_node_id < MAX_NUM_NODES; 
                 queried_node_id++)
            {
                clink_node_id_t  remote_node_id;

                /** A super optimization may use just the number of bits 
                 *  which were set in the superset remote nodemask rather than
                 *  the maximum number of nodes.  Later. */
                if ((gqs_stats.response_words[queried_node_id] + 
                    MAX_NUM_NODES) >= (MRT_BYTES_ACK_MAX/4)) continue;

                /** Find a node that has the current parameter selected. */
                for (remote_node_id = 0;
                     remote_node_id < MAX_NUM_NODES;
                     remote_node_id++)
                {
                    clink_nodemask_t nm = eset_ptr->included[param][remote_node_id];

                    if (!INCTYPES_NODE_IN_NODEMASK(queried_node_id, nm))
                    {
                        continue;
                    }

                    remote_nodemask |= 1UL << remote_node_id;
                }

                if (remote_nodemask) break;
            }
        }

        if (queried_node_id < MAX_NUM_NODES)
        {
            /** A queried node has been selected and remote nodes designated */

            /** At this point we know there is room to add the specified 
             *  parameter to the request for the specified node. 
             */

            clnk_nmsimpl_add_elements_to_submit(qb_submit_ptr, &gqs_stats,
                                                pdef_ptr, 
                                                1UL << queried_node_id, 
                                                remote_nodemask);
            added_to_submit = SYS_TRUE;

            /**
             * FOR FIRST IMPLEMENTATION: NO BATCHING OF LINK SZ1 
             * QUERYABLES!  BETTER TESTING REQUIRED!
             * 
             * If execution gets here, at least one Element has been added
             * to the submit which the Eset required.  Technically, everything
             * should work all right if we continue to loop.  There are some
             * odd cases that open up.  It might not be as bad as I fear but
             * securing a robust first delivery is vital.
             */
            break;
        }
    }

    return added_to_submit;
}


/**
 * Function that takes current needed eset and updates 
 * the candidate submit such UFUNC still passes and at least one 
 * needed element is returned.  
 * 
 * This maximization is performed ONLY for nodes with link cardinality
 * of 32 word size.
 * 
 * Returns SYS_TRUE if any bits were set in the submit; SYS_FALSE
 *    otherwise.
 */
SYS_BOOLEAN
clnk_nmsimpl_add_link_sz32_to_submit(
    Mrt_Qbl_GetQueryablesSubmit_t* qb_submit_ptr,
    const clnk_nms_eset_t*         eset_ptr)
{
    clnk_nmsimpl_gqs_stats_t gqs_stats;
    clnk_nms_param_t         param;
    clink_node_id_t          queried_node_id = MAX_NUM_NODES;
    SYS_BOOLEAN              added_to_submit = SYS_FALSE;

    /** No queried node has been picked yet. */

    /**
     * Build some statistics.  
     */
    clnk_nmsimpl_calc_gqs_stats(qb_submit_ptr, &gqs_stats);

    /**
     * No use continuing to add bits to the submit if an error 
     * already created a submit too large to be realized.
     */
    if (gqs_stats.nodes_too_big || 
        gqs_stats.concat_words >= MRT_GEN_ENTRY_RSP_MAX_CONCAT_WORDS)
    {
        CLNK_NMS_ERROR_IF(1, "Unexpected too large: %d, %x", 
                          gqs_stats.concat_words, gqs_stats.nodes_too_big);
        return SYS_FALSE;
    }

    for (param = 0; param < CLNK_NMS_PARAM__MAX; param++)
    {
        const clnk_nms_pdef_t* pdef_ptr;
        clink_node_id_t  remote_node_id = MAX_NUM_NODES;

        pdef_ptr = clnk_nms_get_pdef(param);

        if (pdef_ptr->cardinality != CLNK_NMS_CARDINALITY_LINK) continue;

        if (pdef_ptr->type != CLNK_NMS_TYPE_PROFILE) continue;

        if ((MAX_NUM_NODES + gqs_stats.concat_words) >=
             MRT_GEN_ENTRY_RSP_MAX_CONCAT_WORDS) continue;

        /** This is true if we haven't yet picked a queried node */
        if (queried_node_id >= MAX_NUM_NODES)
        {
            /* Now pick a node */
            for (queried_node_id = 0; 
                 queried_node_id < MAX_NUM_NODES; 
                 queried_node_id++)
            {
                /** A super optimization may use just the number of bits 
                 *  which were set in the superset remote nodemask rather than
                 *  the maximum number of nodes.  Later. */
                if ((gqs_stats.response_words[queried_node_id] + 
                    MAX_NUM_NODES) >= (MRT_BYTES_ACK_MAX/4)) continue;

                /** Find a node that has the current parameter selected. */
                for (remote_node_id = 0;
                     remote_node_id < MAX_NUM_NODES;
                     remote_node_id++)
                {
                    clink_nodemask_t nm = eset_ptr->included[param][remote_node_id];

                    if (INCTYPES_NODE_IN_NODEMASK(queried_node_id, nm)) break;
                }

                if (remote_node_id < MAX_NUM_NODES) break;
            }
        }

        if (queried_node_id < MAX_NUM_NODES)
        {
            /** A queried node has been selected and remote nodes designated */

            /** At this point we know there is room to add the specified 
             *  parameter to the request for the specified node. 
             */

            clnk_nmsimpl_add_elements_to_submit(qb_submit_ptr, &gqs_stats,
                                                pdef_ptr, 
                                                1UL << queried_node_id, 
                                                1UL << remote_node_id);
            added_to_submit = SYS_TRUE;

            /**
             * FOR FIRST IMPLEMENTATION: NO BATCHING OF LINK SZ1 
             * QUERYABLES!  BETTER TESTING REQUIRED!
             * 
             * If execution gets here, at least one Element has been added
             * to the submit which the Eset required.  Technically, everything
             * should work all right if we continue to loop.  There are some
             * odd cases that open up.  It might not be as bad as I fear but
             * securing a robust first delivery is vital.
             */
            break;
        }
    }

    return added_to_submit;

#if 0 /** OLD UGLY BROKEN */
    clnk_nmsimpl_gqs_stats_t gqs_stats;
    clnk_nms_param_t         param;
    clink_node_id_t          sel_q_node_id = MAX_NUM_NODES;
    SYS_BOOLEAN              added_to_submit = SYS_FALSE;

    /** No queried node has been picked yet. */

    /**
     * Build some statistics.  
     */
    clnk_nmsimpl_calc_gqs_stats(qb_submit_ptr, &gqs_stats);

    /**
     * No use continuing to add bits to the submit if an error 
     * already created a submit too large to be realized.
     */
    if (gqs_stats.nodes_too_big || 
        gqs_stats.concat_words >= MRT_GEN_ENTRY_RSP_MAX_CONCAT_WORDS)
    {
        CLNK_NMS_ERROR_IF(1, "Unexpected too large: %d, %x", 
                          gqs_stats.concat_words, gqs_stats.nodes_too_big);
        return SYS_FALSE;
    }

    /**
     * This is not the most efficient algorithm but what it lacks in
     * efficiency it makes up for in validation simplicity
     * 
     * Loop through parameters until finding first that meets following
     *  criteria:
     *    + LINK Cardinality
     *    + SMALL
     *    + --- now build aggregate queried + remote nodemasks.  Count bits.
     *    + remote bitcount would fit in node response
     *    + remote bitcount would fit in total aggregation
     *    SELECT A QUERIED NODE and FORM REQUEST FOR REMOTE NODEMASK.
     */
    for (param = 0; param < CLNK_NMS_PARAM__MAX; param++)
    {
        const clnk_nms_pdef_t* pdef_ptr;
        clink_nodemask_t agg_queried_nm = CLINK_NODEMASK_NONE;
        clink_nodemask_t agg_remote_nm  = CLINK_NODEMASK_NONE;
        clink_node_id_t  node_id;

        pdef_ptr = clnk_nms_get_pdef(param);

        if (pdef_ptr->cardinality != CLNK_NMS_CARDINALITY_LINK) continue;

        if (pdef_ptr->type != CLNK_NMS_TYPE_PROFILE) continue;

        for (node_id = 0; node_id < MAX_NUM_NODES; node_id++)
        {
            const clink_nodemask_t nm = eset_ptr->included[param][node_id];
            if (nm)
            {
                agg_queried_nm |= nm;
                agg_remote_nm  |= 1 << node_id;
            }
        }

        if (!agg_queried_nm) continue;

        if (!agg_remote_nm) continue;

        if (sel_q_node_id >= MAX_NUM_NODES)
        {
            /* Now pick a node */
            for (sel_q_node_id = 0; 
                 sel_q_node_id < MAX_NUM_NODES; 
                 sel_q_node_id++)
            {
                if (!(INCTYPES_NODE_IN_NODEMASK(sel_q_node_id,
                                                agg_queried_nm))) continue;

                break;
            }
        }

        if (sel_q_node_id < MAX_NUM_NODES)
        {
            /** A node has been selected. */

            /** At this point we know there is room to add the specified 
             *  parameter to the request for the specified node. 
             */

            clnk_nmsimpl_add_elements_to_submit(qb_submit_ptr, &gqs_stats,
                                                pdef_ptr, 
                                                1 << sel_q_node_id, 
                                                INCTYPES_LOW_BIT(agg_remote_nm));
            added_to_submit = SYS_TRUE;
        }

        /**
         * FOR FIRST IMPLEMENTATION: NO BATCHING OF LINK SZ32 
         * QUERYABLES!  BETTER TESTING REQUIRED!
         * 
         * If execution gets here, at least one Element has been added
         * to the submit which the Eset required.  Technically, everything
         * should work all right if we continue to loop.  There are some
         * odd cases that open up.  It might not be as bad as I fear but
         * securing a robust first delivery is vital.
         */
        break;
    }

    return added_to_submit;
#endif
}


/**
 * Function that takes current needed eset and updates 
 * the candidate submit such UFUNC still passes and at least one 
 * needed element is returned.  
 * 
 * This maximization is performed ONLY for nodes with node cardinality
 * of 1 word size.
 * 
 * Initial algorithm will maximize request for remaining parameters 
 * for a single node and if still space then get same items from
 * other nodes.
 * 
 * Maybe there'll be some optimization of which other nodes are
 * added based on best bang for buck.
 * 
 * Returns SYS_TRUE if this function added any bits in the submit; SYS_FALSE
 *    otherwise.
 */
SYS_BOOLEAN
clnk_nmsimpl_add_node_sz1_to_submit(
    Mrt_Qbl_GetQueryablesSubmit_t* qb_submit_ptr,
    const clnk_nms_eset_t*         eset_ptr)
{
    clnk_nmsimpl_gqs_stats_t gqs_stats;
    clnk_nms_param_t         param;
    clink_node_id_t          sel_node_id;
    SYS_BOOLEAN              added_to_submit = SYS_FALSE;

    /** No queried node has been picked yet. */

    /**
     * Build some statistics.  
     */
    clnk_nmsimpl_calc_gqs_stats(qb_submit_ptr, &gqs_stats);

    /**
     * No use continuing to add bits to the submit if an error 
     * already created a submit too large to be realized.
     */
    if (gqs_stats.nodes_too_big || 
        gqs_stats.concat_words >= MRT_GEN_ENTRY_RSP_MAX_CONCAT_WORDS)
    {
        CLNK_NMS_ERROR_IF(1, "Unexpected too large: %d, %x", 
                          gqs_stats.concat_words, gqs_stats.nodes_too_big);
        return SYS_FALSE;
    }

    /**
     * Pick deterministic node_id that has a set bit in a NODE_SZ1 
     * Element. WHILE the total concatenation fits AND the response 
     * from the specified node fits, add needed NODE_SZ parameters 
     * to the set bits in the submit.
     */
    sel_node_id = MAX_NUM_NODES;
    for (param = 0; param < CLNK_NMS_PARAM__MAX; param++)
    {
        const clnk_nms_pdef_t* pdef_ptr;

        pdef_ptr = clnk_nms_get_pdef(param);

        if (pdef_ptr->cardinality != CLNK_NMS_CARDINALITY_NODE) continue;

        if (!CLNK_NMS_TYPE_IS_SMALL(pdef_ptr->type)) continue;

        /** Only works because not LINK cardinality!!! */
        if (!eset_ptr->included[param][0]) continue;

        if (sel_node_id >= MAX_NUM_NODES)
        {
            /** We haven't selected a node yet: select now */
            for (sel_node_id = 0;
                 sel_node_id < MAX_NUM_NODES;
                 sel_node_id++)
            {
                if (INCTYPES_NODE_IN_NODEMASK(sel_node_id,
                                              eset_ptr->included[param][0])) break;
            }
        }

        if (sel_node_id < MAX_NUM_NODES)
        {
            /** A node has been selected. */

            /** No more additions to submit possible if the 
             * response is already full */
            if (gqs_stats.response_words[sel_node_id] >= 
                MRT_BYTES_ACK_MAX) break;

            /** No more additions to submit possible if the concatenation 
             * is full */
            if (gqs_stats.concat_words >=
                MRT_GEN_ENTRY_RSP_MAX_CONCAT_WORDS) break;

            /** If the current parameter is not needed by the selected node,
             *  skip it. */
            if (!clnk_nms_eset_test_single(eset_ptr,pdef_ptr,sel_node_id,0)) break;

            /** At this point we know there is room to add the specified 
             * parameter to the request for the specified node. */
            clnk_nmsimpl_add_elements_to_submit(qb_submit_ptr, &gqs_stats,
                                                pdef_ptr, 1UL << sel_node_id, 
                                                CLINK_NODEMASK_NONE);
            added_to_submit = SYS_TRUE;
        }
    }

    return added_to_submit;
}


/**
 * Function that takes current needed eset and updates 
 * the candidate submit such UFUNC still passes and at least one 
 * needed element is returned.  
 * 
 * This maximization is performed ONLY for nodes with node cardinality
 * of 32 word size.
 * 
 * Initial algorithm will maximize request for remaining parameters 
 * for a single node and if still space then get same items from
 * other nodes.
 * 
 * Maybe there'll be some optimization of which other nodes are
 * added based on best bang for buck.
 * 
 * Returns SYS_TRUE if this function added any bits in the submit; SYS_FALSE
 *    otherwise.
 */
SYS_BOOLEAN
clnk_nmsimpl_add_node_sz32_to_submit(
    Mrt_Qbl_GetQueryablesSubmit_t* qb_submit_ptr,
    const clnk_nms_eset_t*         eset_ptr)
{
    clnk_nmsimpl_gqs_stats_t gqs_stats;
    clnk_nms_param_t         param;
    clink_node_id_t          sel_node_id;
    SYS_BOOLEAN              added_to_submit = SYS_FALSE;

    /** No queried node has been picked yet. */

    /**
     * Build some statistics.  
     */
    clnk_nmsimpl_calc_gqs_stats(qb_submit_ptr, &gqs_stats);

    /**
     * No use continuing to add bits to the submit if an error 
     * already created a submit too large to be realized.
     */
    if (gqs_stats.nodes_too_big || 
        gqs_stats.concat_words >= MRT_GEN_ENTRY_RSP_MAX_CONCAT_WORDS)
    {
        CLNK_NMS_ERROR_IF(1, "Unexpected too large: %d, %x", 
                          gqs_stats.concat_words, gqs_stats.nodes_too_big);
        return SYS_FALSE;
    }

    /**
     * Pick deterministic node_id that has a set bit in a NODE_SZ32 
     * Element. WHILE the total concatenation fits AND the response 
     * from the specified node fits, add needed NODE_SZ parameters 
     * to the set bits in the submit.
     */
    sel_node_id = MAX_NUM_NODES;
    for (param = 0; param < CLNK_NMS_PARAM__MAX; param++)
    {
        const clnk_nms_pdef_t* pdef_ptr;

        pdef_ptr = clnk_nms_get_pdef(param);

        if (pdef_ptr->cardinality != CLNK_NMS_CARDINALITY_NODE) continue;

        if (pdef_ptr->type != CLNK_NMS_TYPE_PROFILE) continue;

        /** Only works because not LINK cardinality!!! */
        if (!eset_ptr->included[param][0]) continue;

        if (sel_node_id >= MAX_NUM_NODES)
        {
            /** We haven't selected a node yet: select now */
            for (sel_node_id = 0;
                 sel_node_id < MAX_NUM_NODES;
                 sel_node_id++)
            {
                if (INCTYPES_NODE_IN_NODEMASK(sel_node_id,
                                              eset_ptr->included[param][0])) break;
            }
        }

        if (sel_node_id < MAX_NUM_NODES)
        {
            /** A node has been selected. */

            /** No more additions to submit possible if the 
             * response is already full */
            if (gqs_stats.response_words[sel_node_id] >= 
                MRT_BYTES_ACK_MAX) break;

            /** No more additions to submit possible if the concatenation 
             * is full */
            if (gqs_stats.concat_words >=
                MRT_GEN_ENTRY_RSP_MAX_CONCAT_WORDS) break;

            /** At this point we know there is room to add the specified 
             * parameter to the request for the specified node. */
            clnk_nmsimpl_add_elements_to_submit(qb_submit_ptr, &gqs_stats,
                                                pdef_ptr, 1 << sel_node_id, 
                                                CLINK_NODEMASK_NONE);

            added_to_submit = SYS_TRUE;

            /* As of today, NO batching together of more than one Element
             *  per Submit for large Parameters. */
            break;
        }
    }

    return added_to_submit;
}


/**
 * The bit that gets cleared is a function of the cardinality of 
 * the node.  If it is a NODE cardinality, we clear the queried
 * node bit.  If it is a LINK cardinality, we clear the remote 
 * node bit.
 */
void
clnk_nmsimpl_clear_eset_bit(clnk_nms_eset_t*        eset_ptr,
                            const clnk_nms_pdef_t*  pdef_ptr,
                            clink_node_id_t         queried_node_id,
                            clink_node_id_t         remote_node_id)
{
    const clink_nodemask_t qmask = ~(1UL << queried_node_id);

    /** Could remove this check after first big successful delivery */
    CLNK_NMS_ERROR_IF(!clnk_nms_eset_test_single(eset_ptr, pdef_ptr,
                                                 queried_node_id,
                                                 remote_node_id), 
                                                 "Bit already clear %d %8.8x",
                                                 pdef_ptr->param, 
                                                 queried_node_id << 16 | 
                                                 remote_node_id);


    switch (pdef_ptr->cardinality)
    {
    case CLNK_NMS_CARDINALITY_NODE:
        eset_ptr->included[pdef_ptr->param][0] &= qmask;
        break;
    case CLNK_NMS_CARDINALITY_LINK:
        eset_ptr->included[pdef_ptr->param][remote_node_id] &= qmask;
        break;
    default:
        CLNK_NMS_ERROR_IF(1, "Unexpected cardinality", 0, 0);
    }
}


/**
 * This operation is called while processing the responses that came
 * back in a concatenation.  It presumes the caller has correctly
 * specified the identity of the next parameter required by the submit.
 * This decodes node cardinality elements.
 */
void
clnk_nmsimpl_extract_node_element(
    /** The cache where we will put the Element */
    clnk_nmsimpl_cache_t*                 icache_ptr,
    /** The Eset that was used to create the submit; will clear bits on 
     *  Element creation. */
    clnk_nms_eset_t*                      eset_ptr,
    /** What parameter to read. */
    const clnk_nms_pdef_t*                pdef_ptr,
    /** The node that provided the response */
    clink_node_id_t                       queried_node_id,
    /** The massaged structure containing the response payload */
    const clnk_generic_l2me_node_info_t*  node_info_ptr,
    /** The index in the response payload where to extract the 
     *  Element from; also, this func increments this value as it 
     *  "consumes" the value there.  Element sizes differ. */
    SYS_UINT32*                           response_word_offset_ptr )
{
    clnk_nms_eview_t        eview;
    clnk_nms_eview_cptr     eview_cptr = NULL;
    clnk_nms_disposition_t  disposition = CLNK_NMS_DISPOSITION_UNRETRIEVABLE;
    SYS_UINT32              i;
    SYS_UINT32              bitlow;
    SYS_UINT32              bithigh;
    SYS_UINT32              index;

    /** Only works for node cardinality. */
    CLNK_NMS_ERROR_IF(pdef_ptr->cardinality != CLNK_NMS_CARDINALITY_NODE,
                      "Unexpected cardinality", 0, 0);

    /** Entering a do while(0) construct that is easy to break out of: for
     *  many reasons the required element may not be available. */
    do
    {
        if (!node_info_ptr->is_valid_response) 
        {
            /* There is no new element to get from the response.  we
               are done trying to get one.  Given we created a correct
               submit, and given that the l2me operation returned with 
               'success', the fact we do not see here decodable responses
               is not expected unless the node was incapable of providing
               one. */

            break;
        }

        switch (pdef_ptr->type)
        {
        case CLNK_NMS_TYPE_UINT32:
        case CLNK_NMS_TYPE_INT32:
        case CLNK_NMS_TYPE_CENTI_DECIBELS:
            if (*response_word_offset_ptr + 1 > node_info_ptr->response_words) 
            {
                /** Not enough words to extract an element from a response.  
                 *  Do not prepare a valid element value. */
                break;
            }

            /** There are enough bytes.  Create the element. */
            disposition = CLNK_NMS_DISPOSITION_FRESH;
            eview_cptr = &eview;

            eview.s.uint32 = 
                node_info_ptr->response_array[*response_word_offset_ptr];

            (*response_word_offset_ptr)++;
            break;
        case CLNK_NMS_TYPE_PROFILE:

            /** Not enough words to extract if the below is true.  Do not 
             *  prepare a valid element value. */
            if (*response_word_offset_ptr + WORDS_PER_PROFILE > node_info_ptr->response_words) break;

            disposition = CLNK_NMS_DISPOSITION_FRESH;
            eview_cptr = &eview;

            bitlow = 0;
            bithigh = BITS_PER_NIBBLE - 1;

            // copy each nibble in node_info_ptr->response_array[0..69 UINT32] to eview.l.profile_array[0..255 byte]
            for (i = 0; i < NIBBLES_PER_PROFILE; i++)
            {
                index = *response_word_offset_ptr + (i / NIBBLES_PER_WORD);
                // first nibble is in 4 LSBs as defined in
                // MoCA MAC/PHY Spec v1.0 Table 3.8:SC_MOD:First 32 bits: Nibbles for subcarriers 7,6,5,4,3,2,1,0
                eview.l.profile_array[i] = CLNKDEFS_EXTRACT_FIELD(
                    node_info_ptr->response_array[index], bithigh, bitlow);
                // bitlow 0=00, 1=04, 2=08, 3=12, 4=16, 5=20, 6=24, 7=28, 8=00, etc...
                bitlow = (bitlow + BITS_PER_NIBBLE) % BITS_PER_WORD;
                // bithigh 0=03, 1=07, 2=11, 3=15, 4=19, 5=23, 6=27, 7=31, 8=03, etc...
                bithigh = (bithigh + BITS_PER_NIBBLE) % BITS_PER_WORD;
            }

            (*response_word_offset_ptr) += WORDS_PER_PROFILE;
            break;

        default:
            CLNK_NMS_ERROR_IF(1, "Unhandled type", 0, 0);
        }

    } while (0);

    clnk_nmsimpl_set_element_in_cache(icache_ptr, pdef_ptr, eset_ptr,
                                      queried_node_id, 0, 
                                      eview_cptr, 
                                      disposition);

    clnk_nmsimpl_clear_eset_bit(eset_ptr, pdef_ptr, queried_node_id, 0);
}


/**
 * This operation is called while processing the responses that came
 * back in a concatenation.  It presumes the caller has correctly
 * specified the identity of the next parameter required by the submit.
 * This decodes link cardinality elements.
 */
void
clnk_nmsimpl_extract_link_elements(
    /** The cache where we will put the Element */
    clnk_nmsimpl_cache_t*                 icache_ptr,
    /** The Eset that was used to create the submit; will clear bits on 
     *  Element creation. */
    clnk_nms_eset_t*                      eset_ptr,
    /** What parameter to read. */
    const clnk_nms_pdef_t*                pdef_ptr,
    /** The node that provided the response */
    clink_node_id_t                       queried_node_id,
    /** The set of nodes from the remote nodemask of the submit */
    clink_nodemask_t                      remote_nodemask,
    /** The massaged structure containing the response payload */
    const clnk_generic_l2me_node_info_t*  node_info_ptr,
    /** The index in the response payload where to extract the 
     *  Element from; also, this func increments this value as it 
     *  "consumes" the value there.  Element sizes differ. */
    SYS_UINT32*                           response_word_offset_ptr )
{
    clink_node_id_t         remote_node_id;
    SYS_UINT32              i;
    SYS_UINT32              bitlow;
    SYS_UINT32              bithigh;
    SYS_UINT32              index;

    /** Only works for link cardinality. */
    CLNK_NMS_ERROR_IF(pdef_ptr->cardinality != CLNK_NMS_CARDINALITY_LINK,
                      "Unexpected cardinality", 0, 0);

    for (remote_node_id = 0;
         remote_node_id < MAX_NUM_NODES;
         remote_node_id++)
    {
        clnk_nms_eview_t        eview;
        clnk_nms_eview_cptr     eview_cptr = NULL;
        clnk_nms_disposition_t  disposition = CLNK_NMS_DISPOSITION_UNRETRIEVABLE;

        if (!INCTYPES_NODE_IN_NODEMASK(remote_node_id, 
                                       remote_nodemask)) continue;

        /** Entering a do while(0) construct that is easy to break out of: for
         *  many reasons the expected elements may not be available. */
        do
        {

            if (!node_info_ptr->is_valid_response) 
            {
                /* There is no new element to get from the response.  we
                   are done trying to get one.  Given we created a correct
                   submit, and given that the l2me operation returned with 
                   'success', the fact we do not see here decodable responses
                   is not expected unless the node was incapable of providing
                   one. */

                break;
            }

            switch (pdef_ptr->type)
            {
            case CLNK_NMS_TYPE_UINT32:
            case CLNK_NMS_TYPE_INT32:
            case CLNK_NMS_TYPE_CENTI_DECIBELS:

                if (*response_word_offset_ptr + 1 > node_info_ptr->response_words) 
                {
                    /** Not enough words to extract an element from a response.  
                     *  Do not prepare a valid element value. */
                    break;
                }

                /** Create the element. */
                disposition = CLNK_NMS_DISPOSITION_FRESH;
                eview_cptr = &eview;

                eview.s.uint32 = 
                    node_info_ptr->response_array[*response_word_offset_ptr];

                (*response_word_offset_ptr)++;
                break;
            case CLNK_NMS_TYPE_PROFILE:
                /** need to do some unpacking of profile to array */

                if (!node_info_ptr->is_valid_response) break;
                if (*response_word_offset_ptr + WORDS_PER_PROFILE > node_info_ptr->response_words) break;

                disposition = CLNK_NMS_DISPOSITION_FRESH;
                eview_cptr = &eview;

                bitlow = 0;
                bithigh = BITS_PER_NIBBLE - 1;

                // copy each nibble in node_info_ptr->response_array[0..69 UINT32] to eview.l.profile_array[0..255 byte]
                for (i = 0; i < NIBBLES_PER_PROFILE; i++)
                {
                    index = *response_word_offset_ptr + (i / NIBBLES_PER_WORD);
                    // first nibble is in 4 LSBs as defined in
                    // MoCA MAC/PHY Spec v1.0 Table 3.8:SC_MOD:First 32 bits: Nibbles for subcarriers 7,6,5,4,3,2,1,0
                    eview.l.profile_array[i] = CLNKDEFS_EXTRACT_FIELD(
                        node_info_ptr->response_array[index], bithigh, bitlow);
                    // bitlow 0=00, 1=04, 2=08, 3=12, 4=16, 5=20, 6=24, 7=28, 8=00, etc...
                    bitlow = (bitlow + BITS_PER_NIBBLE) % BITS_PER_WORD;
                    // bithigh 0=03, 1=07, 2=11, 3=15, 4=19, 5=23, 6=27, 7=31, 8=03, etc...
                    bithigh = (bithigh + BITS_PER_NIBBLE) % BITS_PER_WORD;
                }

                (*response_word_offset_ptr) += WORDS_PER_PROFILE;
                break;
            default:
                CLNK_NMS_ERROR_IF(1, "Unhandled type", 0, 0);
            }
        } while (0);

        clnk_nmsimpl_set_element_in_cache(icache_ptr, pdef_ptr, eset_ptr,
                                          queried_node_id, 
                                          remote_node_id, 
                                          eview_cptr, 
                                          disposition);

        clnk_nmsimpl_clear_eset_bit(eset_ptr, pdef_ptr, queried_node_id, remote_node_id);
    }
}


/** Parse l2me results, tolerant that not all requested items arrived
 *  for various overflow or concatenation reasons.  
 * 
 *  For each properly received word, create correct element in cache and 
 * then clear the corresponding bit right out of the eset.
 *    ->  corresponding bit for NODE cardinality is QUERIED!
 *    ->  corresponding bit for LINK cardinality is REMOTE!
 * 
 *  Any decoding error whatsoever creates an ERROR.
 */
void
clnk_nmsimpl_process_gq_results(
    clnk_nmsimpl_cache_t*                 icache_ptr,
    clnk_nms_eset_t*                      eset_ptr,
    const Mrt_Qbl_GetQueryablesSubmit_t*  sub_ptr,
    const Mrt_GenEntryRsp_t*              raw_response_ptr)
{
    clink_node_id_t queried_node_id;
    clnk_generic_l2me_response_t  response;

    clnk_decode_generic_l2me_response(raw_response_ptr, &response);

    for (queried_node_id=0;
         queried_node_id < MAX_NUM_NODES;
         queried_node_id++)
    {
        SYS_UINT32 response_word_index = 0;
        const clnk_nmsimpl_psmap_entry_t* entry_ptr;

        /** Note this presumes entry ordering correctness ! */
        for (entry_ptr =                         clnk_nmsimpl_psmap;
             entry_ptr < INCTYPES_ARRAY_TERMINAL(clnk_nmsimpl_psmap);
             entry_ptr++)
        {                     
            clink_nodemask_t  remote_nm  = CLINK_NODEMASK_NONE;
            clink_nodemask_t  queried_nm = CLINK_NODEMASK_NONE;
            SYS_UINT32        submit_word = 0;
            const SYS_UINT32  wois = entry_ptr->word_offset_in_section;
            const clnk_nms_pdef_t* pdef_ptr;

            switch (entry_ptr->submit_section)
            {
            case       MRT_QBL_SECTION_LINK_SZ32:
                queried_nm  = sub_ptr->LINK_SZ32_QUERIED;
                submit_word = sub_ptr->LINK_SZ32_W[wois];
                remote_nm   = sub_ptr->LINK_SZ32_REMOTE;
                break;
            case       MRT_QBL_SECTION_NODE_SZ32:
                queried_nm  = sub_ptr->NODE_SZ32_QUERIED;
                submit_word = sub_ptr->NODE_SZ32_W[wois];
                remote_nm   = 0;
                break;
            case       MRT_QBL_SECTION_LINK_SZ1:
                queried_nm  = sub_ptr->LINK_SZ1_QUERIED;
                submit_word = sub_ptr->LINK_SZ1_W[wois];
                remote_nm   = sub_ptr->LINK_SZ1_REMOTE;
                break;
            case       MRT_QBL_SECTION_NODE_SZ1:
                queried_nm  = sub_ptr->NODE_SZ1_QUERIED;
                submit_word = sub_ptr->NODE_SZ1_W[wois];
                remote_nm   = 0;
                break;
            default:
                CLNK_NMS_ERROR_IF(1, "Unhandled case", 0, 0);
            }

            /** FOR DEBUGGING ONLY */
            if (0)
            {
                printf("TODO_REMOVE %s(%d): qn:%02d wois:%02d qnm:%8.8x rnm:%8.8x sw:%8.8x boiw:%02d p:%d\n", 
                       __FILE__, __LINE__,
                       queried_node_id,
                       wois,
                       queried_nm,
                       remote_nm,
                       submit_word,
                       entry_ptr->bit_offset_in_word,
                       entry_ptr->param
                       );
            }

            /** If the submit didn't check this queried node, no data */
            if (!INCTYPES_NODE_IN_NODEMASK(queried_node_id,
                                           queried_nm)) continue;

            /** If the submit didn't contain the relevant bit, no data */
            if (!(submit_word & (1UL << entry_ptr->bit_offset_in_word)))
            {
                continue;
            }

            pdef_ptr = clnk_nms_get_pdef(entry_ptr->param);

            /** At this point we know that the submit requested data
             *  for a particular parameter from the current node. */
            /** FOR DEBUGGING ONLY */
            if (0)
            {
                printf("TODO_REMOVE %s(%d): queried_nm:0x%x remote_nm:0x%x boiw:%d\n", 
                       __FILE__, __LINE__, queried_nm, remote_nm, entry_ptr->bit_offset_in_word);
                printf("TODO_REMOVE %s(%d): Create Elem(s) for Param %d from node %d at response offset %d.\n", 
                       __FILE__, __LINE__, pdef_ptr->param, queried_node_id, response_word_index);
            }

            /** The cardinality of the parameter specifies how words are
             *  extracted from the payload for element creation. */
            switch (pdef_ptr->cardinality)
            {
            case CLNK_NMS_CARDINALITY_NODE:
                clnk_nmsimpl_extract_node_element(icache_ptr,
                                                  eset_ptr,
                                                  pdef_ptr,
                                                  queried_node_id,
                                                  &response.nodes[queried_node_id],
                                                  &response_word_index);
                break;
            case CLNK_NMS_CARDINALITY_LINK:
                clnk_nmsimpl_extract_link_elements(icache_ptr,
                                                   eset_ptr,
                                                   pdef_ptr,
                                                   queried_node_id,
                                                   remote_nm,
                                                   &response.nodes[queried_node_id],
                                                   &response_word_index);
                break;
            default:
                CLNK_NMS_ERROR_IF(1, "Unexpected cardinality", 0, 0);
            }
        }
    }
}

/** Minimum number of milliseconds to space out L2ME operations.  This is
 *  not a hard and fast number but a value that should be adequate if only
 *  one node in the network is generating batch L2ME requests.
 */
#define CLNK_IMPL_SAFE_L2ME_SPACING_MS   150

/**
 * GETTING REMOTE INFORMATION THROUGH L2ME QUERIES
 * 
 * This section is responsible for retrieving information from remote
 * nodes.  With lots of work we can probably really optimize the number
 * of L2ME transactions that go on; the algorithm here will reflect 
 * some simplifying trade-offs.
 * 
 */
void
clnk_nmsimpl_fill_cache_l2me_gq(
    clnk_ctx_t*             pctx,                              
    clnk_nmsimpl_cache_t*   icache_ptr,                                                    
    const clnk_nms_eset_t*  requested_eset_ptr,                                            
    clink_nodemask_t        l2me_capable_nodemask,                                         
    SYS_UINT32              timeout_ms,                                                    
    void                    (*sleep_func)(SYS_UINT32 sleep_ms))
{
    clnk_nms_eset_t   needed_eset;

    SYS_INT32         allowed_serious_failures = 3;

    clnk_impl_timer_t timer;
    clnk_impl_timer_start(&timer, timeout_ms);

    /**
     * Create eset based on the requested one that has a bit set for
     * every item that can be retrieved through L2ME Get Queryables.
     */
    clnk_nmsimpl_eset_filter_gq_only(requested_eset_ptr,
                                     l2me_capable_nodemask,
                                     &needed_eset);

    /**
     * For every element in eset where queried node is a current network
     * node but is not l2me capable, replace with an unretrievable disposition
     * and clear the bit.
     */
    clnk_nmsimpl_fill_unavailable(icache_ptr, requested_eset_ptr, &needed_eset);

    /** while ungathered_eset has elements to retriev via L2ME... */
    while (clnk_nmsimpl_eset_has_l2me_elements(&needed_eset))
    {
        Mrt_GenEntryReq_t gen_entry_request;
        Mrt_GenEntryRsp_t gen_entry_response;
        SYS_BOOLEAN restart_iteration = SYS_FALSE;
        SYS_BOOLEAN submit_added = SYS_FALSE;
        clnk_nms_param_t param;

        /** If interface timeout then break. */
        if (!clnk_impl_timer_ms_remaining(&timer)) break;

        /** If too many failed L2ME queries then take up no more
         *  than a few more seconds to let the situation clear 
         *  and then give up collection this cycle. */
        if (allowed_serious_failures < 0)
        {
            const SYS_UINT32 MAX_FAIL_SPIN = 2000;
            SYS_UINT32 failureSpin = clnk_impl_timer_ms_remaining(&timer);
            if (failureSpin > MAX_FAIL_SPIN) failureSpin = MAX_FAIL_SPIN;

            clnk_impl_timer_spin_sleep(failureSpin, sleep_func);

            return;
        }

        /** Initialize a submit payload that requests nothing. */
        INCTYPES_SAFE_VAR_ZERO(gen_entry_request.gq_submit);

        /** 
         * Call funcs that take current needed eset and updates 
         * the candidate submit such UFUNC still passes and at least one 
         * needed element is returned (if there is any room left at all
         * in the L2ME concatenation).
         * 
         * Initial algorithm will maximize request for remaining parameters 
         * for a single node and if still space then get multiple nodes.  
         * Maybe there'll be some optimization of which other nodes are
         * added based on best bang for buck.
         */
        submit_added = 0
          || clnk_nmsimpl_add_link_sz1_to_submit(&gen_entry_request.gq_submit,
                                                 &needed_eset)
          || clnk_nmsimpl_add_node_sz1_to_submit(&gen_entry_request.gq_submit, 
                                                 &needed_eset)
          || clnk_nmsimpl_add_link_sz32_to_submit(&gen_entry_request.gq_submit,
                                                  &needed_eset)
          || clnk_nmsimpl_add_node_sz32_to_submit(&gen_entry_request.gq_submit, 
                                                  &needed_eset)
          ;

        CLNK_NMS_ERROR_IF(!submit_added, "Unexpected failure", 0, 0);

        /** If only one L2ME element found then send all (up to 16) requests out in one request. */
        if (clnk_nmsimpl_eset_has_only_one_l2me_element(&needed_eset, &param))
        {
            gen_entry_request.gq_submit.NODE_SZ1_QUERIED = needed_eset.included[param][0];
        }

        /** Issue the l2me operation. */
        while (1) 
        {
            int retval; 

            /** If interface timeout then break. */
            if (clnk_impl_timer_ms_remaining(&timer) < 
                  CLNK_IMPL_SAFE_L2ME_SPACING_MS) break;

            /** For now, this is the rate control algorithm: someday this
             *  will be a function of the ccpu busyness but at the moment 
             *  we are being conservative and simple. */
            clnk_impl_timer_spin_sleep(CLNK_IMPL_SAFE_L2ME_SPACING_MS, 
                                       sleep_func);

            /** Some header fields have not yet been populated.  Populate
             *  them now. */
            gen_entry_request.mrt_gen_entry_op = MRT_GEN_ENTRY_OP_GET_QUERYABLES;
            gen_entry_request.wave0_nodemask = 0
                | gen_entry_request.gq_submit.LINK_SZ32_QUERIED
                | gen_entry_request.gq_submit.NODE_SZ32_QUERIED
                | gen_entry_request.gq_submit.LINK_SZ1_QUERIED
                | gen_entry_request.gq_submit.NODE_SZ1_QUERIED
                ;

            retval = !clnk_do_generic_l2me_trans(pctx, 
                                                 &gen_entry_request, 
                                                 &gen_entry_response);

            /** Exit early if the operation failed */
            if (!retval) 
            {
                allowed_serious_failures--;
                CLNK_NMS_ERROR_IF(1, "Unexpected failure %d %d", 
                                  retval, allowed_serious_failures);
                restart_iteration = SYS_TRUE;
                break;
            }

            /** A timeout is a legitimate L2ME phenomenon.  If this
             * happens, we will retry again. */
            if (gen_entry_response.l2me_error == MRT_ERRNO_TIMEOUT_FAIL)
            { 
                /** Somewhat arbitrarily chosen value, just keeps 
                 *  submit spam down in bad cases */
                clnk_impl_timer_spin_sleep(50, sleep_func);

                continue;
            }

            /** Other than a timeout, any other failure is bad and must
             *  be calculated against the fatal error. */
            if (gen_entry_response.l2me_error != MRT_ERRNO_SUCCESS)
            {
                allowed_serious_failures--;
                restart_iteration = SYS_TRUE;
                break;
            }

            /** If we got here then there is no more looping needed */
            break;
        }

        /** Something happened that means we must start the iteration
         *  over again. */
        if (restart_iteration) continue;

        /** Parse l2me results */
        clnk_nmsimpl_process_gq_results(icache_ptr,
                                        &needed_eset,
                                        &gen_entry_request.gq_submit,
                                        &gen_entry_response);
    }
}

#endif /* FEATURE_PUPQ_GQ */


void
clnk_nmsimpl_fill_cache_network_items(
                    clnk_ctx_t*             pctx,
                    clnk_nmsimpl_cache_t*   icache_ptr,
                    const clnk_nms_eset_t*  eset_ptr,
                    clink_nodemask_t*       l2me_nodemask_ptr)
{
    clnkdefs_local_misc_info_t local_misc_info;
    clnk_nms_eview_t           eview;
    const clnk_nms_pdef_t*     pdef_ptr;
	ClnkDef_MyNodeInfo_t 	   my_node_info;

    if (clnk_get_local_misc_info(pctx,&local_misc_info))
    {
        CLNK_NMS_ERROR_IF(1, "Failed get l2me info", 0, 0);
        return;
    }

    pdef_ptr = clnk_nms_get_pdef(CLNK_NMS_PARAM_NW_TABOO_MASK_START);
    eview.s.uint32 = local_misc_info.taboo_mask_start;
    clnk_nmsimpl_set_element_in_cache(icache_ptr, pdef_ptr,
                                      eset_ptr, 0, 0, &eview, 
                                      CLNK_NMS_DISPOSITION_FRESH);

    pdef_ptr = clnk_nms_get_pdef(CLNK_NMS_PARAM_NW_TABOO_CHANNEL_MASK);
    eview.s.uint32 = local_misc_info.taboo_channel_mask;
    clnk_nmsimpl_set_element_in_cache(icache_ptr, pdef_ptr,
                                      eset_ptr, 0, 0, &eview, 
                                      CLNK_NMS_DISPOSITION_FRESH);

    if (clnk_get_lof(pctx, &eview.s.uint32))
    {
        pdef_ptr = clnk_nms_get_pdef(CLNK_NMS_PARAM_NW_LOF);
        clnk_nmsimpl_set_element_in_cache(icache_ptr, pdef_ptr,
                                          eset_ptr, 0, 0, &eview, 
                                          CLNK_NMS_DISPOSITION_FRESH);
    }
    else
    {
        CLNK_NMS_ERROR_IF(1, "Failed clnk_get_lof", 0, 0);
    }

    if (!clnk_get_lmo_advanced_ctr(pctx,&eview.s.uint32)) 
    {
        pdef_ptr = clnk_nms_get_pdef(CLNK_NMS_PARAM_LMO_ADVANCED_CTR);
        clnk_nmsimpl_set_element_in_cache(icache_ptr, pdef_ptr, 
                                          eset_ptr, 0, 0, &eview,
                                          CLNK_NMS_DISPOSITION_FRESH);
    }
    else
    {
        CLNK_NMS_ERROR_IF(1, "Failed clnk_get_lmo_advanced_ctr", 0, 0);
    }

	//Get network status information
	if(!clnk_get_my_node_info(pctx, &my_node_info, SYS_FALSE))
	{
		eview.s.uint32 = my_node_info.LinkStatus;
		pdef_ptr = clnk_nms_get_pdef(CLNK_NMS_PARAM_NW_STATUS);
		clnk_nmsimpl_set_element_in_cache(icache_ptr, pdef_ptr, 
                                         eset_ptr, 0, 0, &eview,
                                         CLNK_NMS_DISPOSITION_FRESH);
	}
	else
    {
        CLNK_NMS_ERROR_IF(1, "Failed clnk_get_my_node_info", 0, 0);
    }

    /** We pass the nodemask out of this function for use later
     *  since we don't really want to call more API operations 
     *  than are strictly needed. */
    if (l2me_nodemask_ptr) 
    {
        *l2me_nodemask_ptr = local_misc_info.l2me_capable_nodemask;
    }
}

void
clnk_nms_fill_cache(clnk_ctx_t*             pctx,
                    clnk_nms_cache_t*       cache_ptr,
                    const clnk_nms_eset_t*  eset_ptr,
                    SYS_UINT32              timeout_ms,
                    void                    (*sleep_func)(SYS_UINT32 sleep_ms),
                    clink_nodemask_t*       final_nodemask_ptr)
{
    /* Convert from Application opaque cache to internal form */
    clnk_nmsimpl_cache_t* icache_ptr = (clnk_nmsimpl_cache_t*)cache_ptr;

    /** The set of nodes in the network can change during this operation 
     *  since a collection takes so long.  This value caches the set 
     *  at the beginning. */
    clink_nodemask_t      starting_nw_nodemask;

    /** The set of nodes in the network can change */
    clink_nodemask_t      starting_l2me_nodemask;

    /** At the end of the process, the nodes in the network are...  */
    clink_nodemask_t      final_nw_nodemask;

    clnk_impl_timer_t     interface_timer;

    /** Always need a timeout! */
    if (!timeout_ms) timeout_ms = 15000;

    clnk_impl_timer_start(&interface_timer, timeout_ms);

    CLNK_NMS_ERROR_IF(!pctx,      "Null pointer", 0, 0);
    CLNK_NMS_ERROR_IF(!cache_ptr, "Null pointer", 0, 0);
    CLNK_NMS_ERROR_IF(!eset_ptr,  "Null pointer", 0, 0);

    if (!pctx || !cache_ptr || !eset_ptr) 
        return;

    clnk_nmsimpl_staleify_cache(cache_ptr, eset_ptr);

    clnk_nmsimpl_fill_cache_local_apis(pctx, icache_ptr, eset_ptr, &starting_nw_nodemask);

    final_nw_nodemask = starting_nw_nodemask;

    clnk_nmsimpl_fill_cache_fmr(pctx, icache_ptr, eset_ptr, 
                                clnk_impl_timer_ms_remaining(&interface_timer),
                                sleep_func);

    {
        const clnk_nms_pdef_t* pdef_ptr = 
            clnk_nms_get_pdef(CLNK_NMS_PARAM_NW_PRIVACY_PASSWORD);

        if (clnk_nms_eset_test_single(eset_ptr,pdef_ptr,0,0)) 
        {
            clnk_nms_eview_t eview;
            clnk_get_current_mocapassword(pctx, &eview.l.mocapassword);

            clnk_nmsimpl_set_element_in_cache(icache_ptr, pdef_ptr, eset_ptr,
                                              0, 0, &eview,
                                              CLNK_NMS_DISPOSITION_FRESH);
        }
    }

    clnk_nmsimpl_fill_cache_network_items(pctx, icache_ptr, eset_ptr,
                                          &starting_l2me_nodemask);

#if FEATURE_PUPQ_GQ
    clnk_nmsimpl_fill_cache_l2me_gq(pctx, icache_ptr, eset_ptr, 
                                    starting_l2me_nodemask, 
                                    clnk_impl_timer_ms_remaining(&interface_timer),
                                    sleep_func);
#endif /* FEATURE_PUPQ_GQ */

    /**
     * MAKE CACHE VIEW CONSISTENT WITH NETWORK VIEW
     * 
     * Some of these gathering operations can take a long time.  Therefore,
     * this stage is responsible for making the cache self consistent.  Here,
     * we remove elements that are not in the network.  Additionally, if
     * nodes are in the network that were not at the time of collection, 
     * we will mark those elements appropriately.
     */
    do {
        ClnkDef_MyNodeInfo_t  my_node_info;

        if (clnk_get_my_node_info(pctx,&my_node_info,SYS_FALSE))
        {
            CLNK_NMS_ERROR_IF(1, "Failed get my node info!", 0, 0);
            break;
        }

        final_nw_nodemask = my_node_info.NetworkNodeBitMask;

        clnk_nmsimpl_consistify_cache(icache_ptr, final_nw_nodemask);
    } while (0);

    if (final_nodemask_ptr) 
    {
        *final_nodemask_ptr = final_nw_nodemask;
    }
}


void
clnk_nms_release_cache(clnk_nms_cache_t* cache_ptr)
{
    /* Convert from Application opaque cache to internal form */
    clnk_nmsimpl_cache_t* icache_ptr = 
        (clnk_nmsimpl_cache_t*)cache_ptr;

    CLNK_NMS_ERROR_IF(!cache_ptr, "Null pointer", 0, 0);
    clnk_nms_param_t param;

    if (!cache_ptr) return;
    for (param = 0; param < CLNK_NMS_PARAM__MAX; param++)
    {
        clnk_nmsimpl_eindex_t eindex_local;

        const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(param);

        for (eindex_local = 0; 
             eindex_local < icache_ptr->param_count_index[param]; 
             eindex_local++)
        {
            clnk_nmsimpl_eindex_t eindex_absolute = 
                eindex_local + icache_ptr->param_start_index[param];

            CLNK_NMS_ERROR_IF(eindex_absolute >= CLNK_NMSIMPL_CACHE_EINDEX__MAX,
                              "eindex_absolute %d beyond range %d", 
                              eindex_absolute, CLNK_NMSIMPL_CACHE_EINDEX__MAX);

            clnk_nmsimpl_delete_element(pdef_ptr, 
                                        &icache_ptr->elements[eindex_absolute]);
            icache_ptr->dispositions[eindex_absolute] = CLNK_NMS_DISPOSITION_UNKNOWN;
        }
    }
}

void
clnk_nms_copy_cache(const clnk_nms_cache_t*  source_cache_ptr,
                    clnk_nms_cache_t*        dest_cache_ptr)
{
    clnk_nms_param_t       param;
    clnk_nmsimpl_eindex_t  eindex;

    const clnk_nmsimpl_cache_t* source_ptr = 
        (const clnk_nmsimpl_cache_t*)source_cache_ptr;

    clnk_nmsimpl_cache_t* dest_ptr = 
        (clnk_nmsimpl_cache_t*)dest_cache_ptr;

    CLNK_NMS_ERROR_IF(!source_ptr, "Null pointer", 0, 0);
    CLNK_NMS_ERROR_IF(!dest_ptr,   "Null pointer", 0, 0);

    if (!source_ptr || !dest_ptr) return;

    clnk_nms_release_cache(dest_cache_ptr);

    for (param = 0;
         param < CLNK_NMS_PARAM__MAX;
         param++)
    {
        const clnk_nmsimpl_eindex_t source_count = 
            source_ptr->param_count_index[param];

        const clnk_nmsimpl_eindex_t dest_count = 
            dest_ptr->param_count_index[param];

        if (source_count != dest_count)
        {
            CLNK_NMS_ERROR_IF(1, "Surprise: source_ptr param %d size %d not same in dest_ptr",
                              param, source_count);
            continue;
        }

        CLNK_NMS_ERROR_IF(!source_count, 
                          "Unexpected zero count in source_ptr param %d",
                          param, 0);

        CLNK_NMS_ERROR_IF(!dest_count, 
                          "Unexpected zero count in dest_ptr param %d",
                          param, 0);

        for (eindex = 0; eindex < source_count; eindex++)
        {
            const clnk_nmsimpl_eindex_t source_eindex = 
                source_ptr->param_start_index[param] + eindex;

            const clnk_nmsimpl_eindex_t dest_eindex = 
                dest_ptr->param_start_index[param] + eindex;

            clnk_nms_element_handle_t handle = 
                source_ptr->elements[source_eindex];

            dest_ptr->dispositions[dest_eindex] = 
                source_ptr->dispositions[source_eindex];

            if (handle != CLNK_NMS_ELEMENT_HANDLE_INVALID)
            {
                clnk_nms_context.increment(handle);
            }

            dest_ptr->elements[dest_eindex] = 
                source_ptr->elements[source_eindex];
        }
    }
}

SYS_BOOLEAN
clnk_nms_push_hard_reset(clnk_ctx_t*       pctx,
                         clink_nodemask_t  nodes_to_reset)
{
    SYS_UINT32         retval;
    Mrt_GenEntryReq_t  gen_entry_req;
    Mrt_GenEntryRsp_t  gen_entry_rsp;

    gen_entry_req.mrt_gen_entry_op     = MRT_GEN_ENTRY_OP_PUSH;
    gen_entry_req.wave0_nodemask       = nodes_to_reset;
    gen_entry_req.p_submit.vendor_id   = MRT_VENDOR_ID_ENTROPIC;

    /* Initialization only at this point.  ClinkD
     * will overwrite the request_num with the proper value.
     */
    gen_entry_req.p_submit.request_num = 1;

    gen_entry_req.p_submit.dword_array[0] = CLNKDEFS_PUSH_CODE_HARD_RESET;
    gen_entry_req.p_submit.dword_array[1] = CLNK_NMS_HARD_RESET;

    retval = clnk_do_generic_l2me_trans(pctx, 
                                        &gen_entry_req, 
                                        &gen_entry_rsp);

    if (retval)
    {
        return SYS_FALSE;
    }

    if (gen_entry_rsp.l2me_error != MRT_ERRNO_SUCCESS)
    {
        return SYS_FALSE;
    }
    return SYS_TRUE;
}


SYS_BOOLEAN
clnk_nms_get_hard_reset(clnk_ctx_t* pctx, clnk_nms_reset_types_t *forced_reset)
{
    if(pctx == 0)
    {
        return RET_ERR;
    }
    // this macro does a return 0 if good
    CLNK_CTL_GET_0(pctx, CLNK_CTL_GET_HARD_RESET, clnk_nms_reset_types_t, forced_reset);
}

#endif

#if FEATURE_PUPQ_NMS_QUICK_APIS

/* PQoS config via L2ME messaging */
SYS_BOOLEAN
clnk_nms_push_pqos_mode(clnk_ctx_t*           pctx,
                        clink_nodemask_t      nodes_to_set,
                        clnk_pqos_mode_t      pqos_mode)
{
    SYS_UINT32         retval;
    Mrt_GenEntryReq_t  gen_entry_req;
    Mrt_GenEntryRsp_t  gen_entry_rsp;

    gen_entry_req.mrt_gen_entry_op     = MRT_GEN_ENTRY_OP_PUSH;
    gen_entry_req.wave0_nodemask       = nodes_to_set;
    gen_entry_req.p_submit.vendor_id   = MRT_VENDOR_ID_ENTROPIC;

    /* Initialization only at this point.  ClinkD
     * will overwrite the request_num with the proper value.
     */
    gen_entry_req.p_submit.request_num = 1;

    gen_entry_req.p_submit.dword_array[0] = CLNKDEFS_PUSH_CODE_PQOS_MODE;
    gen_entry_req.p_submit.dword_array[1] = pqos_mode;

    retval = clnk_do_generic_l2me_trans(pctx, 
                                        &gen_entry_req, 
                                        &gen_entry_rsp);

    if (retval)
    {
        return SYS_FALSE;
    }

    if (gen_entry_rsp.l2me_error != MRT_ERRNO_SUCCESS)
    {
        return SYS_FALSE;
    }
    return SYS_TRUE;
}

/* NMS push messages via L2ME messaging */
SYS_BOOLEAN
clnk_nms_get_push_acknowledged(clnk_ctx_t*       pctx,
                               clink_nodemask_t  nodes_to_get,
                               clink_nodemask_t* nodes_ack)
{
    SYS_UINT32                   retval = 0;
    Mrt_GenEntryReq_t            gen_entry_req;
    Mrt_GenEntryRsp_t            gen_entry_rsp;
    clnk_generic_l2me_response_t decoded_response;
    SYS_UINT32                   i;

    gen_entry_req.mrt_gen_entry_op          = MRT_GEN_ENTRY_OP_GET_PUSH_ACK;
    gen_entry_req.wave0_nodemask            = nodes_to_get;

    /* Send the push ack query to the nodes specified via L2ME messaging. */
    retval = clnk_do_generic_l2me_trans(pctx, 
                                        &gen_entry_req, 
                                        &gen_entry_rsp);

    /* If the private API call to clnk_do_generic_l2me_trans() was
     * NOT successfull, return FALSE right away.
     */
    if (retval)
    {
        return SYS_FALSE;
    }

    /* If L2ME transaction was NOT successfull, return FALSE 
     * right away.
     */
    if (gen_entry_rsp.l2me_error != MRT_ERRNO_SUCCESS)
    {
        return SYS_FALSE;
    }

    /* Use this to simplify the response processing */
    clnk_decode_generic_l2me_response(&gen_entry_rsp, &decoded_response);

    /* Look through the node list in the response to determine
     * which nodes replied with an acknowledgment that they
     * have received AND processed the last push command.
     */

    *nodes_ack = 0;
    for(i = 0; i < MAX_NUM_NODES; i++)
    {
        if( decoded_response.nodes[i].response_nms_get_push_ack.push_ack_resp == 1)
        {
            *nodes_ack |= 1 << i;
        }
    }
    return SYS_TRUE;
}

/** We need this little stublet in order to flip the sense 
 *  of the return generated from the macro */
int
clnk_nms_inner_query(clnk_ctx_t* pctx,                     
                     clnk_nms_query_msg_local_t* vendor_msg)
{
    CLNK_CTL_GET_0(pctx, CLNK_CTL_GET_NMS_MSG_DAEMON, clnk_nms_query_msg_local_t, vendor_msg);
}


/* NMS push message local query */
SYS_BOOLEAN 
clnk_nms_query_message_local(clnk_ctx_t*                 pctx,
                             clnk_nms_query_msg_local_t* vendor_msg)
{
    SYS_INT32 retval = SYS_TRUE;

    if( (pctx == 0) || (vendor_msg == 0) )
    {
        retval = SYS_FALSE;
    }

    if (clnk_nms_inner_query(pctx, vendor_msg)) retval = SYS_FALSE;

    return retval;
}


SYS_BOOLEAN 
clnk_nms_push_message(clnk_ctx_t*           pctx,
                      SYS_UINT32            vendor_id,
                      SYS_UINT32*           pdword_array,
                      clink_nodemask_t      nodes_to_set)
{
    SYS_UINT32         retval;
    Mrt_GenEntryReq_t  gen_entry_req;
    Mrt_GenEntryRsp_t  gen_entry_rsp;

    gen_entry_req.mrt_gen_entry_op     = MRT_GEN_ENTRY_OP_PUSH;
    gen_entry_req.wave0_nodemask       = nodes_to_set;
    gen_entry_req.p_submit.vendor_id   = vendor_id;

    /* Initialization only at this point.  ClinkD
     * will overwrite the request_num with the proper value.
     */
    gen_entry_req.p_submit.request_num = 1;

    gen_entry_req.p_submit.dword_array[0] = pdword_array[0];
    gen_entry_req.p_submit.dword_array[1] = pdword_array[1];
    gen_entry_req.p_submit.dword_array[2] = pdword_array[2];
    gen_entry_req.p_submit.dword_array[3] = pdword_array[3];
    gen_entry_req.p_submit.dword_array[4] = pdword_array[4];
    gen_entry_req.p_submit.dword_array[5] = pdword_array[5];

    retval = clnk_do_generic_l2me_trans(pctx, &gen_entry_req, &gen_entry_rsp);

    if (retval)
    {
        return SYS_FALSE;
    }

    if (gen_entry_rsp.l2me_error != MRT_ERRNO_SUCCESS)
    {
        return SYS_FALSE;
    }
    return SYS_TRUE;
}


SYS_BOOLEAN 
clnk_nms_push_frequency_settings(clnk_ctx_t*      pctx,
                                 SYS_UINT32       last_operating_frequency,
                                 SYS_UINT32       product_mask,
                                 SYS_UINT32       channel_mask,
                                 clink_nodemask_t nodes_to_set)
{
    SYS_UINT32         retval;
    Mrt_GenEntryReq_t  gen_entry_req;
    Mrt_GenEntryRsp_t  gen_entry_rsp;

    gen_entry_req.mrt_gen_entry_op     = MRT_GEN_ENTRY_OP_PUSH;
    gen_entry_req.wave0_nodemask       = nodes_to_set;
    gen_entry_req.p_submit.vendor_id   = MRT_VENDOR_ID_ENTROPIC;

    /* Initialization only at this point.  ClinkD
     * will overwrite the request_num with the proper value.
     */
    gen_entry_req.p_submit.request_num = 1;

    gen_entry_req.p_submit.dword_array[0] = CLNKDEFS_PUSH_CODE_FREQUENCY_SETTINGS;
    gen_entry_req.p_submit.dword_array[1] = last_operating_frequency;
    gen_entry_req.p_submit.dword_array[2] = product_mask;
    gen_entry_req.p_submit.dword_array[3] = channel_mask;

    retval = clnk_do_generic_l2me_trans(pctx, 
                                        &gen_entry_req, 
                                        &gen_entry_rsp);

    if (retval)
    {
        return SYS_FALSE;
    }

    if (gen_entry_rsp.l2me_error != MRT_ERRNO_SUCCESS)
    {
        return SYS_FALSE;
    }
    return SYS_TRUE;
}


SYS_BOOLEAN 
clnk_nms_push_privacy_password(clnk_ctx_t*           pctx,
                               SYS_UCHAR*            pprivacy_password,
                               clink_nodemask_t      nodes_to_set)
{
    SYS_UINT32         retval;
    Mrt_GenEntryReq_t  gen_entry_req;
    Mrt_GenEntryRsp_t  gen_entry_rsp;

    gen_entry_req.mrt_gen_entry_op     = MRT_GEN_ENTRY_OP_PUSH;
    gen_entry_req.wave0_nodemask       = nodes_to_set;
    gen_entry_req.p_submit.vendor_id   = MRT_VENDOR_ID_ENTROPIC;

    /* Initialization only at this point.  ClinkD
     * will overwrite the request_num with the proper value.
     */
    gen_entry_req.p_submit.request_num = 1;

    gen_entry_req.p_submit.dword_array[0] = CLNKDEFS_PUSH_CODE_MOCAPASSWORD;
    memcpy(&gen_entry_req.p_submit.dword_array[1], pprivacy_password,
           MAX_MOCAPASSWORD_LEN_PADDED);

    retval = clnk_do_generic_l2me_trans(pctx, 
                                        &gen_entry_req, 
                                        &gen_entry_rsp);

    if (retval)
    {
        return SYS_FALSE;
    }

    if (gen_entry_rsp.l2me_error != MRT_ERRNO_SUCCESS)
    {
        return SYS_FALSE;
    }
    return SYS_TRUE;
}


SYS_BOOLEAN 
clnk_nms_push_target_phy_rate(clnk_ctx_t*           pctx,
                              SYS_UINT32            target_phy_rate_mbps,
                              clink_nodemask_t      nodes_to_set)
{
    SYS_UINT32         retval;
    Mrt_GenEntryReq_t  gen_entry_req;
    Mrt_GenEntryRsp_t  gen_entry_rsp;

    gen_entry_req.mrt_gen_entry_op     = MRT_GEN_ENTRY_OP_PUSH;
    gen_entry_req.wave0_nodemask       = nodes_to_set;
    gen_entry_req.p_submit.vendor_id   = MRT_VENDOR_ID_ENTROPIC;

    /* Initialization only at this point.  ClinkD
     * will overwrite the request_num with the proper value.
     */
    gen_entry_req.p_submit.request_num = 1;

    gen_entry_req.p_submit.dword_array[0] = CLNKDEFS_PUSH_CODE_TARGET_PHY_RATE;
    gen_entry_req.p_submit.dword_array[1] = target_phy_rate_mbps;

    retval = clnk_do_generic_l2me_trans(pctx, 
                                        &gen_entry_req, 
                                        &gen_entry_rsp);

    if (retval)
    {
        return SYS_FALSE;
    }

    if (gen_entry_rsp.l2me_error != MRT_ERRNO_SUCCESS)
    {
        return SYS_FALSE;
    }
    return SYS_TRUE;
}


SYS_BOOLEAN
clnk_nms_get_pqos_mode(clnk_ctx_t*          pctx,
                       clink_nodemask_t     nodes_to_query,
                       clnk_pqos_mode_t     pqosmode_array[MAX_NUM_NODES])
{
    Mrt_GenEntryReq_t entryReq = {0};
    Mrt_GenEntryRsp_t entryRsp = {0};
    SYS_BOOLEAN       retval;

    /** Basic initializations */
    entryRsp.l2me_error = MRT_ERRNO_IMPLEMENT_ERROR;

    /** Form the generic request */
    entryReq.mrt_gen_entry_op           = MRT_GEN_ENTRY_OP_GET_QUERYABLES;
    entryReq.wave0_nodemask             = nodes_to_query;
    entryReq.gq_submit.NODE_SZ1_QUERIED = nodes_to_query;
    entryReq.gq_submit.NODE_SZ1_W[0]    = 
           1 << MRT_QBL_DEF_NODE_SZ1_W0_NODE_PQOS_CLASSIFY_MODE;

    /** Issue the command */
    retval = !clnk_do_generic_l2me_trans(pctx, &entryReq, &entryRsp) &&
             entryRsp.l2me_error == MRT_ERRNO_SUCCESS;


    /** Exit early if the operation failed */
    if (!retval) return retval;

    /** Decode the generic response */
    {
        clnk_generic_l2me_response_t decodedEntryRsp;
        clink_node_id_t  node_id;

        clnk_decode_generic_l2me_response(&entryRsp, &decodedEntryRsp);

        for (node_id = 0; node_id < MAX_NUM_NODES; node_id++)
        {
            const Mrt_Qbl_GetQueryablesResponse_t* rsp_ptr =
                (Mrt_Qbl_GetQueryablesResponse_t *)
                decodedEntryRsp.nodes[node_id].response_array;

            if (!pqosmode_array) break;
            pqosmode_array[node_id] = CLNK_PQOS_MODE_UNKNOWN;
            if (decodedEntryRsp.nodes[node_id].response_words != 1) continue;
            pqosmode_array[node_id] = rsp_ptr->words[0];
        }
    }

    return(retval);
}

#endif

