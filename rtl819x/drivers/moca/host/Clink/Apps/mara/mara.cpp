/***************************************************************************** 
 *      Copyright (c) 2006 Entropic Communications
 *****************************************************************************/

/** @file 
 *
 * @brief
 *     Mesh Api Reference Application- command line access to mesh api 
 *     operations.
 *
 * This application allows a human user working at a console of linux-hosted
 * Entropic Reference Software to directly trigger API operations.  The 
 * syntax for entering commands as well as the outputs of these commands 
 * will be controlled such that test infrastructures issuing commands through
 * this should survive from version to version with minimal changes.
 * 
 */
/***********************************************************************************
* This file is licensed under the terms of your license agreement(s) with          *
* Entropic covering this file. Redistribution, except as permitted by the terms of *
* your license agreement(s) with Entropic, is strictly prohibited.                 *
***********************************************************************************/

#include "clnkethapilnx.h"
#include "clnkdvrapi.h"

#include "errno.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/time.h"
#include "unistd.h"

extern "C" {
#include "ClnkDefs.h"
#include "ClnkCtl.h"
#include "ClnkNms.h"
}

typedef int mara_unix_exit_status_t;

#define MARA_APP_NAME "mara"
#define MARA_PRE " "

#define MARA_PARAM_COUNT_MAX 20

#define STRING_COMMA_VALUE(x) #x,x

#define NUM_RETRIES 5

/**
 * Enumerations that can be used to index all command arrays
 */
enum mara_command_enum_t
{
#define COMMAND_START(_token, _name, _defined)   MARA_COMMAND_##_token,
#define PARAM_LONG(a,b,c,d,e,f,g,h)
#define PARAM_STRING(a,b,c,d,e)
#define PARAM_MAC(a,b,c,d,e,f)
#define COMMAND_END(x)
#include "mara.pp"
#undef COMMAND_START
#undef PARAM_LONG
#undef PARAM_STRING
#undef PARAM_MAC
#undef COMMAND_END
    MARA_COMMAND__MAX
};

/**
 * 
 */
enum mara_param_format_t
{
    MARA_PARAM_FORMAT_NUMERIC,
    MARA_PARAM_FORMAT_STRING,
    MARA_PARAM_FORMAT_MAC,
};


/**
 * Definition of a parameter type.  This describes a parameter and its
 * validation rules for a particular command.
 */
typedef struct
{
    const char*          token;
    const char*          name;
    const char*          unitPostfix;
    const char*          def;
    const char*          scanfString;
    mara_param_format_t  format;
    size_t               storageSize;
    const char*          printfStrng;
    SYS_BOOLEAN          isRequired;
    long                 numericDefault;
    const char*          stringDefault;
    mac_addr_t           macDefault;
} mara_param_type_t;


/**
 * Definition of a command type.  This describes a command for command
 * line meaning extraction.
 */
typedef struct
{
    const char*             token;
    const char*             name;
    const char*             def;
    mara_command_enum_t     commandEnum;
    const mara_param_type_t  paramTypeArr[MARA_PARAM_COUNT_MAX];
}  mara_cmd_type_t;


/**
 * Table of commands supported by this application.  Note the unconventional
 * use of the preprocessor to improve maintainability.
 */
const mara_cmd_type_t mara_cmdTypes[] = {
#define COMMAND_START(_token, _name, _defined) { #_token, _name, _defined, MARA_COMMAND_##_token, {
#define COMMAND_END(_token)                } },
#define PARAM_LONG(_token, _name, _unit, _defined, _type, _printf, _isRequired, _default) \
   {                                                   \
       #_token, _name, _unit,                          \
       _defined,                                       \
       #_token "=%Li",                                 \
       MARA_PARAM_FORMAT_NUMERIC,                      \
       sizeof(_type),                                  \
       _printf,                                        \
       _isRequired?1:0, _default, "",                  \
       {0,0}                                           \
   },
#define PARAM_STRING(_token, _name, _defined, _strlen, _default) \
   {                                                   \
       #_token, _name, "",                             \
       _defined,                                       \
       #_token "=%" #_strlen "s",                     \
       MARA_PARAM_FORMAT_STRING,                       \
       _strlen,                                        \
       "%s",                                           \
       0, -1, _default,                                \
       {0,0}                                           \
   },
#define PARAM_MAC(_token, _name, _defined, _required, _defaultHi, _defaultLo) \
   {                                                   \
       #_token, _name, "",                             \
       _defined,                                       \
       #_token "=%lx:%lx:%lx:%lx:%lx:%lx",             \
       MARA_PARAM_FORMAT_MAC,                          \
       sizeof(mac_addr_t),                             \
       "ERROR",                                        \
       _required, -1, "",                              \
       {_defaultHi,_defaultLo}                         \
   },
#include "mara.pp"
#undef COMMAND_START
#undef PARAM_LONG
#undef PARAM_STRING
#undef PARAM_MAC
#undef COMMAND_END
};

/**
 * Each element in this array corresponds to an element in the mara_commands
 * array.  The element is the number of valid parameters in the co-indexed
 * command.
 */
const int mara_paramLongCountArray[] = {
#define COMMAND_START(x,y,z)  0
#define PARAM_LONG(a,b,c,d,e,f,g,h) + 1
#define PARAM_STRING(a,b,c,d,e) + 1
#define PARAM_MAC(a,b,c,d,e,f) + 1
#define COMMAND_END(x)  ,
#include "mara.pp"
#undef COMMAND_START
#undef PARAM_LONG
#undef PARAM_STRING
#undef PARAM_MAC
#undef COMMAND_END
    -1};


/**
 * Defines structures for commands supported by this program.
 */
#define COMMAND_START(_token, _name, _defined) struct mara_cmd_##_token##_struct {
#define PARAM_LONG(_token, _name, _unit, _defined, _type, _printf, _isRequired, _default) \
         _type _token;
#define PARAM_STRING(_token, _name, _defined, _strlen, _default) \
         const char* _token;
#define PARAM_MAC(_token, _name, _defined, _required, _defaultHi, _defaultLo) \
         mac_addr_t _token;
#define COMMAND_END(_token) int _unused; };
#include "mara.pp"
#undef COMMAND_START
#undef PARAM_LONG
#undef PARAM_STRING
#undef PARAM_MAC
#undef COMMAND_END


/**
 * Defines typedefs for command structures supported by this program.
 */
#define COMMAND_START(_token, _name, _defined) typedef struct mara_cmd_##_token##_struct mara_cmd_##_token##_t;
#define PARAM_LONG(a,b,c,d,e,f,g,h)
#define PARAM_STRING(a,b,c,d,e)
#define PARAM_MAC(a,b,c,d,e,f)
#define COMMAND_END(_token)
#include "mara.pp"
#undef COMMAND_START
#undef PARAM_LONG
#undef PARAM_STRING
#undef PARAM_MAC
#undef COMMAND_END


/**
 * Define a specification of a specific command.
 */
typedef struct
{
    const mara_cmd_type_t* cmdTypePtr;
    long long paramNumericValue[MARA_PARAM_COUNT_MAX];
    const char* paramStringValue[MARA_PARAM_COUNT_MAX];
    mac_addr_t  paramMacValue[MARA_PARAM_COUNT_MAX];
    int paramFound[MARA_PARAM_COUNT_MAX];
    int paramValid[MARA_PARAM_COUNT_MAX];
} mara_cmd_inst_t;

char * g_StringBuffer[MARA_PARAM_COUNT_MAX];
/** ***************************************************************************
 */
void
mara_printCommandLineSample(const mara_cmd_type_t* cmdTypePtr)
{
    printf(MARA_APP_NAME " %s", cmdTypePtr->token);
    for (int paramIndex = 0; 
        paramIndex < mara_paramLongCountArray[cmdTypePtr->commandEnum]; 
        paramIndex++)
    {
        const mara_param_type_t* paramTypePtr = 
        &cmdTypePtr->paramTypeArr[paramIndex];

        printf(" ");

        if (!paramTypePtr->isRequired) printf("[");

        switch (paramTypePtr->format)
        {
        case MARA_PARAM_FORMAT_NUMERIC:
            printf("%s=#", paramTypePtr->token);
            break;
        case MARA_PARAM_FORMAT_STRING:
            printf("%s=String", paramTypePtr->token);
            break;
        case MARA_PARAM_FORMAT_MAC:
            printf("%s=xx:xx:xx:xx:xx:xx", paramTypePtr->token);
            break;
        default:
            printf("%s=???", paramTypePtr->token);
            break;
        }

        if (!paramTypePtr->isRequired) printf("]");
    }
}


/** ***************************************************************************
 */
const char*
mara_macToString(SYS_UINT32 hi, SYS_UINT32 lo)
{
    enum
    {
        SIZE=(2*6)/*digits*/ + 5/*colons*/ + 1/*null*/
    };
    static char printMac[SIZE];


    snprintf(printMac, SIZE, "%02x:%02x:%02x:%02x:%02x:%02x",
             ((hi >> 24) & 0xff), 
             ((hi >> 16) & 0xff),
             ((hi >> 8) & 0xff),
             (hi & 0xff),
             ((lo >> 24) & 0xff),
             ((lo >> 16) & 0xff));

    return printMac;
}


/** ***************************************************************************
 *  ***************************************************************************
 * 
 *                            LOGICAL TIMER 
 * 
 *  ***************************************************************************
 *  ***************************************************************************
 */

/** State variable for logical timer */
typedef struct 
{
    struct timeval expire;
} mara_tmr_instance_t;


/**
 * Starts a logical timer.  There is only one timer that can run at a time.
 */
static void mara_tmr_start( mara_tmr_instance_t*  instancePtr, 
                            SYS_UINT32            milliseconds )
{
    struct timeval delta;

    if (!instancePtr) return;

    delta.tv_sec = milliseconds / 1000;
    delta.tv_usec = (milliseconds % 1000) * 1000;

#ifdef AEI_WECB
    aei_gettimeofday(&instancePtr->expire, NULL);
#else
    gettimeofday(&instancePtr->expire, NULL);
#endif

    instancePtr->expire.tv_sec  += delta.tv_sec;
    instancePtr->expire.tv_usec += delta.tv_usec;
    if (instancePtr->expire.tv_usec > 1000*1000)
    {
        instancePtr->expire.tv_sec  += 1;
        instancePtr->expire.tv_usec -= 1000*1000;
    }
}


/**
 * Queries the number of milliseconds remaining on the logical timer
 */
static SYS_UINT32 mara_tmr_msRemaining( const mara_tmr_instance_t* instancePtr )
{
    struct timeval now;

    if (!instancePtr) return 0;

#ifdef AEI_WECB
    aei_gettimeofday(&now, NULL);
#else
    gettimeofday(&now, NULL);
#endif
    if (instancePtr->expire.tv_sec < now.tv_sec) return 0;

    if (instancePtr->expire.tv_sec == now.tv_sec &&
        instancePtr->expire.tv_usec < now.tv_usec) return 0;

    return
    (instancePtr->expire.tv_sec - now.tv_sec) * 1000 +
    (instancePtr->expire.tv_usec / 1000) -
    (now.tv_usec                 / 1000) ;
}


/**
 * Causes the processor to spin for at least designated number of 
 * milliseconds using the logical timer.
 */
static void mara_tmr_spin( SYS_UINT32 ms )
{
    mara_tmr_instance_t instance;

    struct timeval now;
#ifdef AEI_WECB
    aei_gettimeofday( &now, NULL);
#else
    gettimeofday( &now, NULL);
#endif
    mara_tmr_start(&instance, ms);
    while (SYS_TRUE)
    {
        SYS_UINT32 remain = mara_tmr_msRemaining( &instance );

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

/** ***************************************************************************
 */
static void mara_freeStringMemory(void)
{
    int i;
    for(i=0; i<MARA_PARAM_COUNT_MAX; i++)
    {
        if(g_StringBuffer[i])
        {
            free((void *)g_StringBuffer[i]);
        }
    }
    return;
}

/** ***************************************************************************
 */
void
mara_listCommands(void)
{
    printf(MARA_PRE " Mesh Api Reference Application\n");
    printf(MARA_PRE " Options: \n");
    printf(MARA_PRE "    -i iface - Select interface iface when more than 1 is available.\n");
    printf(MARA_PRE "    -L       - List available interfaces and quit.\n");
    printf(MARA_PRE " Command summary: \n");

    int longest = 0;

    for (int cmdIndex = 0; cmdIndex < MARA_COMMAND__MAX; cmdIndex++)
    {
        const mara_cmd_type_t* cmdTypePtr = &mara_cmdTypes[cmdIndex];
        int len = strlen(cmdTypePtr->token);
        longest = len > longest ? len : longest;
    }

    enum
    {
        SIZE=20
    };
    char fmt[SIZE];
    snprintf(fmt, SIZE, MARA_PRE "    %s%ds %s\n", "%-", 
             longest, "%s");

    for (int cmdIndex = 0; cmdIndex < MARA_COMMAND__MAX; cmdIndex++)
    {
        const mara_cmd_type_t* cmdTypePtr = &mara_cmdTypes[cmdIndex];

        printf(fmt, cmdTypePtr->token, cmdTypePtr->name);
    }

    return;
}


/** ***************************************************************************
 */
void
mara_printCommandSyntax(void)
{
    for (int cmdIndex = 0; cmdIndex < MARA_COMMAND__MAX; cmdIndex++)
    {
        printf(MARA_PRE "   ");
        mara_printCommandLineSample(&mara_cmdTypes[cmdIndex]);
        printf("\n\n");
    }

    return;
}


/** ***************************************************************************
 */
void
mara_printCommandHelp(const mara_cmd_type_t* cmdTypePtr)
{
    printf(MARA_PRE "SYNOPSIS\n");
    printf(MARA_PRE "      %s- %s\n", cmdTypePtr->token, cmdTypePtr->name);

    printf(MARA_PRE "      %s\n", cmdTypePtr->def);

    printf(MARA_PRE "\n");
    printf(MARA_PRE "SYNTAX\n");
    printf(MARA_PRE "      ");
    mara_printCommandLineSample(cmdTypePtr);
    printf(MARA_PRE "\n");
    printf(MARA_PRE "\n");

    printf(MARA_PRE "PARAMETERS\n");

    switch (cmdTypePtr->commandEnum)
    {
#define COMMAND_START(_token, _name, _defined)                  \
    case MARA_COMMAND_##_token:                                 \
        {                                                       

#define PARAM_LONG(_token, _name, _unit, _defined, _type, _printf, _isRequired, _default)    \
            printf(MARA_PRE "    %-30s\n", #_token "- " _name );                             \
            printf(MARA_PRE "       Definition: " _defined "\n");                            \
            if (_isRequired)                                                                 \
            {                                                                                \
            printf(MARA_PRE "       Default:    None- Required\n");                          \
            }                                                                                \
            else                                                                             \
            {                                                                                \
            printf(MARA_PRE "       Default:    " #_default "\n");                           \
            }                                                                                \
            printf(MARA_PRE "       Type:       Numeric (e.g. 10, 15, 0x20)\n");             \
            if (_unit[0])                                                                    \
            {                                                                                \
            printf(MARA_PRE "       Unit:       " _unit "\n");                               \
            }                                                                                \
            printf(MARA_PRE "\n");

#define PARAM_STRING(_token, _name, _defined, _strlen, _default)                             \
            printf(MARA_PRE "    %-30s\n", #_token "- " _name );                             \
            printf(MARA_PRE "       Definition: " _defined "\n");                            \
            printf(MARA_PRE "       Default:    " _default " \n");                           \
            printf(MARA_PRE "       Type:       String of length " #_strlen "\n");           \
            printf(MARA_PRE "\n");

#define PARAM_MAC(_token, _name, _defined, _strlen, _defaultHi, _defaultLo)                  \
            printf(MARA_PRE "    %-30s\n", #_token "- " _name );                             \
            printf(MARA_PRE "       Definition: " _defined "\n");                            \
            printf(MARA_PRE "       Default:    %s\n",                                       \
                   mara_macToString(_defaultHi, _defaultLo));                                \
            printf(MARA_PRE "       Type:       MAC Address format\n");                      \
            printf(MARA_PRE "\n");

#define COMMAND_END(_token)                                                                  \
        }                                                                                    \
        break;
#include "mara.pp"
#undef COMMAND_START
#undef PARAM_LONG
#undef PARAM_STRING
#undef PARAM_MAC
#undef COMMAND_END
    default:
        printf(MARA_PRE "DEVELOPER ERROR ON LINE %d\n", __LINE__);
        mara_freeStringMemory();
        exit(-1);
    }

    return;
}

/** ***************************************************************************
 */
const mara_cmd_type_t*
mara_findCommandType(const char* tokenPtr)
{
    for (int i = 0; i < MARA_COMMAND__MAX; i++)
    {
        const mara_cmd_type_t* cmdTypePtr = &mara_cmdTypes[i];
        if (!strcmp(tokenPtr, cmdTypePtr->token)) return cmdTypePtr;
    }

    return 0;
}

/** ***************************************************************************
 */
void
mara_parseCommand(mara_cmd_inst_t* cmdInstPtr, 
                  char* commandPtr, int argCount, char** argPtr)
{
    /* element nonzero if corresponding argument found */
    int argFoundCount[MARA_PARAM_COUNT_MAX] = {0};

    // Find specific command
    cmdInstPtr->cmdTypePtr = mara_findCommandType(commandPtr);
    if (!cmdInstPtr->cmdTypePtr)
    {
        printf(MARA_PRE "ERROR- could not find command '%s'\n", 
               commandPtr);
        exit(1);

        return;
    }

    for (int param = 0; param < MARA_PARAM_COUNT_MAX; param++)
    {
        const mara_param_type_t* paramTypePtr = 
        &cmdInstPtr->cmdTypePtr->paramTypeArr[param];

        // Skip null parameters
        if (!paramTypePtr->token) continue;

        // Fill in defaults
        cmdInstPtr->paramFound[param]         = 0;
        cmdInstPtr->paramNumericValue[param]  = paramTypePtr->numericDefault;
        cmdInstPtr->paramStringValue[param]   = paramTypePtr->stringDefault;
        cmdInstPtr->paramMacValue[param]      = paramTypePtr->macDefault;
        cmdInstPtr->paramValid[param]         = 1;

        // Search arguments for value, earliest counts
        for (int arg = 0; arg < argCount && arg < MARA_PARAM_COUNT_MAX; arg++)
        {
            const char* thisArg = argPtr[arg];
            char * stringValue = SYS_NULL;
            long long numericValue;
            mac_addr_t  macValue = {0, 0};
            int decodeSucceeded = 0;

            switch (paramTypePtr->format)
            {
            case MARA_PARAM_FORMAT_NUMERIC:
                decodeSucceeded = sscanf(thisArg, paramTypePtr->scanfString, &numericValue);
                break;
            case MARA_PARAM_FORMAT_STRING:
                stringValue = (char *)malloc(strlen(thisArg));
                // Save the allocated memory pointer here, so that it can be freed before exit.
                g_StringBuffer[arg]=(char *)stringValue;
                decodeSucceeded = sscanf(thisArg, paramTypePtr->scanfString, stringValue);
                break;
            case MARA_PARAM_FORMAT_MAC:
                {
                    long a1, a2, a3, a4, a5, a6;
                    int nparse = sscanf(thisArg, paramTypePtr->scanfString,
                                        &a1, &a2, &a3, &a4, &a5, &a6);

                    if (nparse == 6)
                    {
                        decodeSucceeded = 1;
                        macValue.hi  = (a1 & 0xff) << 24;
                        macValue.hi += (a2 & 0xff) << 16;
                        macValue.hi += (a3 & 0xff) <<  8;
                        macValue.hi += (a4 & 0xff) <<  0;
                        macValue.lo  = (a5 & 0xff) << 24;
                        macValue.lo += (a6 & 0xff) << 16;
                    }
                }
                break;
            default:
                printf(MARA_PRE "DEVELOPER ERROR ON LINE %d\n", __LINE__);
                mara_freeStringMemory();
                exit(1);
                break;
            }

            if (decodeSucceeded)
            {
                if (cmdInstPtr->paramFound[param])
                {
                    printf(MARA_PRE "ERROR- param '%s' shows up more than once in args\n", 
                           paramTypePtr->token);
                    mara_printCommandLineSample(cmdInstPtr->cmdTypePtr);
                    printf(MARA_PRE "\n");
                    mara_freeStringMemory();
                    exit(1);
                }
                else
                {
                    cmdInstPtr->paramFound[param] = 1;
                    switch (paramTypePtr->format)
                    {
                    case MARA_PARAM_FORMAT_NUMERIC: 
                        cmdInstPtr->paramNumericValue[param]  = numericValue;
                        break;
                    case MARA_PARAM_FORMAT_STRING:
                        cmdInstPtr->paramStringValue[param]   = stringValue;
                        break;
                    case MARA_PARAM_FORMAT_MAC:
                        cmdInstPtr->paramMacValue[param].hi   = macValue.hi;
                        cmdInstPtr->paramMacValue[param].lo   = macValue.lo;
                        break;
                    default:
                        printf(MARA_PRE "ERROR- param '%s' format not supported\n", 
                               paramTypePtr->token);
                        mara_printCommandLineSample(cmdInstPtr->cmdTypePtr);
                        printf(MARA_PRE "\n");
                        mara_freeStringMemory();
                        exit(1);
                    }
                    argFoundCount[arg]++;
                }
            }
        }

        if (!cmdInstPtr->paramFound[param] && 
            cmdInstPtr->cmdTypePtr->paramTypeArr[param].isRequired)
        {
            printf(MARA_PRE "ERROR- param '%s' not found and is required\n", 
                   paramTypePtr->token);
            mara_printCommandLineSample(cmdInstPtr->cmdTypePtr);
            printf(MARA_PRE "\n");
            mara_freeStringMemory();
            exit(1);
        }
    }

    // If illegal args present, fail
    for (int arg = 0; arg < argCount; arg++)
    {
        if (!argFoundCount[arg])
        {
            printf(MARA_PRE "ERROR- arg '%s' not properly formed for this command.\n", argPtr[arg]);
            mara_printCommandLineSample(cmdInstPtr->cmdTypePtr);
            printf(MARA_PRE "\n");
            mara_freeStringMemory();
            exit(1);
        }
    }

    return;
}


/** ***************************************************************************
 */
void
mara_echoCommand(mara_cmd_inst_t* cmdInstPtr)
{
    printf(MARA_PRE MARA_APP_NAME " %s", cmdInstPtr->cmdTypePtr->token );
    for (int i = 0; i < MARA_PARAM_COUNT_MAX; i++)
    {
        if (!(cmdInstPtr->paramValid[i])) continue;

        const mara_param_type_t* paramTypePtr = 
        &cmdInstPtr->cmdTypePtr->paramTypeArr[i];

        switch (paramTypePtr->format)
        {
        case MARA_PARAM_FORMAT_NUMERIC:
            {
                enum
                {
                    SIZE=30
                };
                char str[SIZE];
                int value = cmdInstPtr->paramNumericValue[i];
                snprintf(str, SIZE, " %s=%s", "%s", paramTypePtr->printfStrng);
                printf(str, 
                       cmdInstPtr->cmdTypePtr->paramTypeArr[i].token,
                       value );
            }
            break;
        case MARA_PARAM_FORMAT_STRING:
            printf(" %s=%s", 
                   cmdInstPtr->cmdTypePtr->paramTypeArr[i].token,
                   cmdInstPtr->paramStringValue[i] );
            break;
        case MARA_PARAM_FORMAT_MAC:
            printf(" %s=%s", 
                   cmdInstPtr->cmdTypePtr->paramTypeArr[i].token,
                   mara_macToString(cmdInstPtr->paramMacValue[i].hi,
                                    cmdInstPtr->paramMacValue[i].lo));
            break;
        default:
            printf(" %s=???", 
                   cmdInstPtr->cmdTypePtr->paramTypeArr[i].token);
            break;
        }
    }
    printf( "\n" );

    return;
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_syntax(mara_cmd_syntax_t* cmdPtr)
{
    mara_printCommandSyntax();

    return 0;
}


/** ***************************************************************************
 */

#if NETWORK_TYPE_MESH == 1

Clink mara_mesh;

void
mara_openMesh(void)
{
    int err ;

    err = mara_mesh.InitClinkEthDvrInt(mara_mesh.iface) ;
    if( err )
    {
        if( err == RET_WHICHDEV ) {
            printf(MARA_PRE "ERROR- Multiple devices available. Use -i iface to select.\n");
        } else {
            printf(MARA_PRE "ERROR- Can't open device! err=%d.\n", err);
        }
        mara_freeStringMemory();
        exit(1);
    }

    return;
}

#endif

/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_reset(const mara_cmd_reset_t* cmdPtr)
{

#if NETWORK_TYPE_MESH == 1

    BridgeSetupStruct        setup;
    FILE*                    fp;

#if 0
    ClnkDef_RFICTuningData_t Default_RFIC_Tuning_Table[NUM_RF_CHANNELS], *rfic;
    ClnkDef_AGCGainTable_t   AGC_Gain_Table[NUM_AGC_ENTRIES], *agc;
#endif

    if (!mara_mesh.ReadConfigure(&setup , 1))
    {
        printf("Can't read configure!\n");
        return -1;
    }

    if ((fp = fopen("/etc/bandwidth.conf", "r")) ||
        (fp = fopen("/etc/bandwidth.backup", "r")) )
    {
        char * line = NULL;
        size_t len = 0;
        size_t read;
        while ((read = getline(&line, &len, fp)) != (size_t) -1)
        {
             long hi = 0, lo = 0, gt = 0, pk = 0;
             int sts = 0;

             if (*line == '#') continue;
             if (2 == sscanf(line, "broadcast %ld %ld\n", &gt, &pk))
             {
                sts = 1; hi = 0xffffffff; lo = 0xffff0000;
             } else if (2 == sscanf(line, "default %ld %ld\n", &gt, &pk))
             {
                sts = 1; hi = 0xffffff00; lo = 0x00000000;
             }
             else
             {
                long a1, a2, a3, a4, a5, a6;
                int nparse = sscanf(line, "%lx:%lx:%lx:%lx:%lx:%lx %ld %ld\n",
                             &a1, &a2, &a3, &a4, &a5, &a6, &gt, &pk);
                if (nparse == 8 || nparse == 6)
                {
                    sts = 1;
                    hi  = (a1 & 0xff) << 24;
                    hi += (a2 & 0xff) << 16;
                    hi += (a3 & 0xff) <<  8;
                    hi += (a4 & 0xff) <<  0;
                    lo  = (a5 & 0xff) << 24;
                    lo += (a6 & 0xff) << 16;
                }
            }
            if ( gt > 0 && gt < 100) gt *= 1000 * 1000;
            if ( pk > 0 && pk < 100) pk *= 1000 * 1000;
            if (sts) mara_mesh.NodeManageAdd (hi, lo, gt, pk);
            //if (sts) printf("Adding %08x %04x %d %d\n", hi, lo>>16, gt, pk);
        }
        fclose(fp);  // close config file after reading
        if (line) free(line);
    }

    if ((fp = fopen("/etc/priority_queue.conf", "r")))
    {   // Setup VLAN Priority queue sizes as percentages
        //  equivalent of "apitest 11 20 22 8 7"
        SYS_UINT32 fifo[4], total = 0;
        int nret, loop;
        memset(fifo, 0, sizeof(fifo));
        nret = fscanf(fp,"%d,%d,%d,%d\n", &fifo[0],&fifo[1],&fifo[2],&fifo[3]);
        do {
            if (nret <= 0 || nret >= 4) break;
            for (loop = 0; loop < nret; loop++) total += fifo[loop];
            if (total <= 0 || total > 32000) break;
            // printf("Setting Fifo Sizes %d %d %d %d\n", fifo[0], fifo[1], fifo[2], fifo[3]);
            mara_mesh.SetEthFifoSize(fifo, 4);
        } while (0);
        fclose(fp);  // close config file after reading
    }

#if 0 /* Builtin tables are wonderful */
    // open Active or Default RFIC Tuning Table file (none is also ok)
    rfic = ParseRFICTable("/etc/RFIC_tuning_table.conf",
                          Default_RFIC_Tuning_Table);

    if ( !rfic )
        rfic = ParseRFICTable("/etc/RFIC_tuning_table.backup",
                              Default_RFIC_Tuning_Table);
#endif

#if 0 /* Builtin tables are wonderful */
    // open AGC Gain Table file
    agc = ParseAGCTable("/etc/AGC_gain_table.conf", AGC_Gain_Table);

    if (!agc)
        agc = ParseAGCTable("/etc/AGC_gain_table.backup", AGC_Gain_Table);
#endif

    // assumes that insmod has been done for the driver and bridge has been initialized
    if (!mara_mesh.SendKey(setup.password, setup.mocapassword))
    {
        printf("Can't set password\n");
        return -1;
    }

    if (!mara_mesh.SetCMRatio((SYS_UINT32) setup.CMRatio))
    {
        printf("Can't set CMRatio\n");
        return -1;
    }

#if 0
    if (agc && !mara_mesh.SetAGCTable(agc))
    {
        printf("Can't set AGC gain table\n");
        return -1;
    }
#endif

    if (!mara_mesh.SetTxPower(setup.TxPower))
    {
        printf("Can't set Tx Power\n");
        return -1;
    }

    if(setup.BeaconPwrLevel<7 || setup.BeaconPwrLevel>10 )
    {   // if someone writes evil values in clink.conf
        fprintf(stderr, "BeaconPwrLevel should be 7-10, setting to default max. 10\n");
        setup.BeaconPwrLevel = 10;
    }

    if (!mara_mesh.SetBeaconPwrLevel(SET_PARAMETER_AT_BOOTUP, setup.BeaconPwrLevel))
    {
        printf("Can't set Beacon Power Level\n");
        return -1;
    }

    if (!mara_mesh.SetPHYMargin(setup.PHY_margin, setup.PHY_mgn_bitmask))
    {
        printf("Can't set PHY margin\n");
        return -1;
    }

    if (!mara_mesh.SetSwConfig(setup.SwConfig))
    {
        printf("Can't set Sw Config\n");
        return -1;
    }

    if (!mara_mesh.SetPQoSMode(setup.PQoSClassifyMode))
    {
        printf("Can't set PQoSMode\n");
        return -1;
    }

    if (!mara_mesh.SetChannelMask (setup.channelMask))
    {
        printf("Can't set Channel Mask\n");
        return -1;
    }

    if (!mara_mesh.SetScanMask (setup.scanMask))
    {
        printf("Can't set scan mask\n");
        return -1;
    }

    if (!mara_mesh.SetChannelPlan(setup.channelPlan, setup.productMask))
    {
        printf("Can't set channel plan and product mask\n");
        return -1;
    }

#if 0
    // set RFIC channel table values
    if (rfic && !mara_mesh.SetRFICTable(rfic))
    {
        printf("Can't set RFIC table\n");
        return -1;
    }
#endif

    if (!mara_mesh.SetLOF(setup.lof))
    {
        printf("Can't set LOF\n");
        return -1;
    }

    if (!mara_mesh.SetBias(setup.bias))
    {
        printf("Can't set Bias\n");
        return -1;
    }

    if (!mara_mesh.SetDistanceMode(setup.DistanceMode))
    {
        printf("Can't set distance mode\n");
        return -1;
    }

    if (!mara_mesh.SetPowerCtlPhyRate(setup.PowerCtl_PHY_rate))
    {
        printf("Can't set PowerCtlPhyRate\n");
        return -1;
    }

    if (!mara_mesh.SetTabooInfo(setup.tabooMask, setup.tabooOffset))
    {
        printf("Can't set taboo info\n");
        return -1;
    }

#if FEATURE_PUPQ_NMS_CONF
    if (!mara_mesh.SetMfrInfo(setup.mfrVendorId, setup.mfrHwVer, setup.mfrSwVer))
    {
        printf("Can't set manufacturer info\n");
        return -1;
    }

    if (!mara_mesh.SetPersonality(setup.personality))
    {
        printf("Can't set personality\n");
        return -1;
    }
#endif /* FEATURE_PUPQ_NMS_CONF */

    // then, reset the driver to take the parameters
    if (!mara_mesh.ResetDevice())
    {
        printf("Can't Reset Device\n");
        return -1;
    }

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");

#endif
    return 0;
}

/** ***************************************************************************
 */

#define MARA_CONF_DO_LONG(clink_conf_str, cmd_member, bridge_member, fmt) do  \
{                                                                             \
    printf(" Item %-20s is currently " fmt "\n",                              \
           clink_conf_str, setup.bridge_member);                              \
                                                                              \
    if (cmdPtr->cmd_member < 0) break;                                        \
                                                                              \
    change_seen = SYS_TRUE;                                                   \
    printf("   CHANGING " clink_conf_str "  from " fmt " to " fmt "...\n",    \
           setup.bridge_member, cmdPtr->cmd_member);                          \
    setup.bridge_member = cmdPtr->cmd_member;                                 \
} while (0)


        mara_unix_exit_status_t
mara_handler_conf(const mara_cmd_conf_t* cmdPtr)
{
    BridgeSetupStruct setup;
    SYS_BOOLEAN       change_seen = SYS_FALSE;

    if (!mara_mesh.ReadConfigure(&setup, 1))
    {
        printf("Can't read configure!\n");
        return -1;
    }

    MARA_CONF_DO_LONG("phyMargin",        phymargin,        PHY_margin,        "%d");
    MARA_CONF_DO_LONG("phyMBitMask",      phymbitmask,      PHY_mgn_bitmask,   "%d");
    MARA_CONF_DO_LONG("SwConfig",         swconfig,         SwConfig,          "0x%x");
    MARA_CONF_DO_LONG("channelPlan",      channelplan,      channelPlan,       "%d");
    MARA_CONF_DO_LONG("scanMask",         scanmask,         scanMask,          "0x%x");
    MARA_CONF_DO_LONG("productMask",      productmask,      productMask,       "0x%x");
    MARA_CONF_DO_LONG("channelMask",      channelmask,      channelMask,       "0x%x");
    MARA_CONF_DO_LONG("lof",              lof,              lof,               "%d");
    MARA_CONF_DO_LONG("TargetPhyRate",    targetphyrate,    Target_PHY_rate,   "%d");
    MARA_CONF_DO_LONG("PowerCtlPhyRate",  powerctlphyrate,  PowerCtl_PHY_rate, "%d");
    MARA_CONF_DO_LONG("BeaconPwrLevel",   beaconpwrlevel,   BeaconPwrLevel,    "%d");
    MARA_CONF_DO_LONG("MiiPausePriLevel", miipauseprilevel, MiiPausePriLvl,    "%d");
    MARA_CONF_DO_LONG("PQoSClassifyMode", pqosmode,         PQoSClassifyMode,  "%d");

#if FEATURE_PUPQ_NMS_CONF
    MARA_CONF_DO_LONG("mfrVendorId",      mfrvendorid,      mfrVendorId,       "%d");
    MARA_CONF_DO_LONG("mfrHwVer",         mfrhwver,         mfrHwVer,          "%d");
    MARA_CONF_DO_LONG("mfrSwVer",         mfrswver,         mfrSwVer,          "%d");
    MARA_CONF_DO_LONG("personality",      personality,      personality,       "%d");
#endif /* FEATURE_PUPQ_NMS_CONF */

    if (cmdPtr->mask >= 0)
    {
        change_seen = SYS_TRUE;
        printf("   CHANGING all frequency masks to %x and plan to 1\n", cmdPtr->mask);
        setup.productMask = setup.scanMask = setup.channelMask = cmdPtr->mask;
        setup.channelPlan = 1;
    }

    if (cmdPtr->preferrednc == 0 && 
        (setup.SwConfig & CLNK_DEF_SW_CONFIG_PREFERRED_NC_BIT))
    {
        change_seen = SYS_TRUE;
        printf("   CLEARING preferred nc setting.\n");

        setup.SwConfig &= ~CLNK_DEF_SW_CONFIG_PREFERRED_NC_BIT;
    }

    if (cmdPtr->preferrednc == 1 && 
        (setup.SwConfig & (~CLNK_DEF_SW_CONFIG_PREFERRED_NC_BIT)))
    {
        change_seen = SYS_TRUE;
        printf("   SETTING preferred nc setting.\n");

        setup.SwConfig |= CLNK_DEF_SW_CONFIG_PREFERRED_NC_BIT;
    }

    if (change_seen)
    {
        if (!mara_mesh.WriteConfigure(&setup, 1))
        {
            printf("Writing file failed!\n");
            return -1;
        }
        else
        {
            if (!mara_mesh.ReadConfigure(&setup, 1))
            {
                printf("Reread failed after write.\n");
                return -1;
            }
            else
            {
                printf("Success.\n");
            }
        }
    }

    return 0;
}

/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_guids(const mara_cmd_guids_t* cmdPtr)
{

#if NETWORK_TYPE_MESH == 1

    qos_node_response_t output;

    mara_mesh.qos_query_nodes(&output);
    if (output.qos_error != QOS_SUCCESS)
    {
        printf(MARA_PRE "Operation failure.\n");
        return -1;
    }

    printf(MARA_PRE "# Number of nodes is  %d\n", output.number_nodes);
    for (SYS_UINT32 index = 0; index < output.number_nodes; index++)
    {
        printf(MARA_PRE "   export %c=%s\n", 'A' + index,
               mara_macToString(output.nodes[index].hi,
                                output.nodes[index].lo));
    }
    return 0;

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");
    return 0;

#endif
}

/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_qcreate(const mara_cmd_qcreate_t* cmdPtr)
{

#if NETWORK_TYPE_MESH == 1

    flow_desc_t flow_desc;
    qos_cu_response_t output = {{QOS_SUCCESS}};

    INCTYPES_SAFE_VAR_ZERO(flow_desc);

    flow_desc.ingress_guid.hi = cmdPtr->ig.hi;
    flow_desc.ingress_guid.lo = cmdPtr->ig.lo;

    flow_desc.egress_guid.hi = cmdPtr->eg.hi;
    flow_desc.egress_guid.lo = cmdPtr->eg.lo;

    flow_desc.packet_da.hi = flow_desc.flow_id.hi = cmdPtr->flow_id.hi;
    flow_desc.packet_da.lo = flow_desc.flow_id.lo = cmdPtr->flow_id.lo;

    if (!(cmdPtr->unicast_da.lo & 0xFFFF))
    {
        flow_desc.packet_da.hi = cmdPtr->unicast_da.hi;
        flow_desc.packet_da.lo = cmdPtr->unicast_da.lo;
        printf(MARA_PRE "  input.da OVERRIDE:     %08x%04x\n", 
               flow_desc.packet_da.hi, 
               flow_desc.packet_da.lo >> 16);
    }

    flow_desc.opaque                = cmdPtr->opaque;
    flow_desc.t_peak_data_rate_kbps = cmdPtr->peak;
    flow_desc.t_nom_pkt_size        = cmdPtr->psize;
    flow_desc.t_lease_time          = cmdPtr->time;
    flow_desc.t_burst_size          = cmdPtr->burst;

    mara_mesh.qos_create_flow(&flow_desc, &output);
    printf(MARA_PRE "  output.qos_error:      %d\n", output.qos_error);
    printf(MARA_PRE "  qos_error_string:      %s\n", 
           mara_mesh.qos_get_error_string(output.qos_error));
    if (output.qos_error != QOS_SUCCESS)
    {
        printf(MARA_PRE "  is_aggr_stps_limited:  %d\n", output.is_aggr_stps_limited);
        printf(MARA_PRE "  is_aggr_txps_limited:  %d\n", output.is_aggr_txps_limited);
        printf(MARA_PRE "  is_ingress_limited:    %d\n", output.is_ingress_limited);
        printf(MARA_PRE "  is_egress_limited:     %d\n", output.is_egress_limited);
    }
    printf(MARA_PRE "  available_bw_kbps:     %d\n", output.remain_kbps);
    printf(MARA_PRE "  available_burst:       %d\n", output.remain_burst);
    printf(MARA_PRE "  post_stps:             %d\n", output.post_stps);
    printf(MARA_PRE "  post_txps:             %d\n", output.post_txps);

    return output.qos_error;

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");
    printf(MARA_PRE "ingress=%s\n", mara_macToString(cmdPtr->ig.hi, cmdPtr->ig.lo));
    printf(MARA_PRE "egress=%s\n", mara_macToString(cmdPtr->eg.hi, cmdPtr->eg.lo));
    return -1;

#endif
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_qupdate(mara_cmd_qupdate_t* cmdPtr)
{

#if NETWORK_TYPE_MESH == 1

    flow_desc_t flow_desc;
    qos_cu_response_t output;

    INCTYPES_SAFE_VAR_ZERO(flow_desc);

    flow_desc.flow_id.hi = cmdPtr->flow_id.hi;
    flow_desc.flow_id.lo = cmdPtr->flow_id.lo;

    flow_desc.packet_da.hi = cmdPtr->flow_id.hi;
    flow_desc.packet_da.lo = cmdPtr->flow_id.lo;

    flow_desc.opaque                = cmdPtr->opaque;
    flow_desc.t_peak_data_rate_kbps = cmdPtr->peak;
    flow_desc.t_nom_pkt_size        = cmdPtr->psize;
    flow_desc.t_lease_time          = cmdPtr->time;
    flow_desc.t_burst_size          = cmdPtr->burst;

    mara_mesh.qos_update_flow(&flow_desc, &output);
    printf(MARA_PRE "  output.qos_error:      %d\n", output.qos_error);
    printf(MARA_PRE "  qos_error_string:      %s\n", 
           mara_mesh.qos_get_error_string(output.qos_error));
    if (output.qos_error != QOS_SUCCESS)
    {
        printf(MARA_PRE "  is_aggr_stps_limited:  %d\n", output.is_aggr_stps_limited);
        printf(MARA_PRE "  is_aggr_txps_limited:  %d\n", output.is_aggr_txps_limited);
        printf(MARA_PRE "  is_ingress_limited:    %d\n", output.is_ingress_limited);
        printf(MARA_PRE "  is_egress_limited:     %d\n", output.is_egress_limited);
    }
    printf(MARA_PRE "  available_bw_kbps:     %d\n", output.remain_kbps);
    printf(MARA_PRE "  available_burst:       %d\n", output.remain_burst);
    printf(MARA_PRE "  post_stps:             %d\n", output.post_stps);
    printf(MARA_PRE "  post_txps:             %d\n", output.post_txps);

    return output.qos_error;

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");
    return 0;

#endif
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_qdelete(mara_cmd_qdelete_t* cmdPtr)
{
#if NETWORK_TYPE_MESH == 1

    flow_name_t flowName;
    qos_d_response_t output;

    flowName.flow_id.hi = cmdPtr->flow_id.hi;
    flowName.flow_id.lo = cmdPtr->flow_id.lo;

    mara_mesh.qos_delete_flow(&flowName, &output);
    if (output.qos_error != QOS_SUCCESS)
    {
        printf(MARA_PRE "  output.qos_error: %d\n", output.qos_error);
        printf(MARA_PRE "  qos_error_string: %s\n", 
               mara_mesh.qos_get_error_string(output.qos_error));
    }

    return output.qos_error;

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");
    return 0;

#endif
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_qcapacity(mara_cmd_qcapacity_t* cmdPtr)
{
#if NETWORK_TYPE_MESH == 1

    qos_cap_response_t output;

    mara_mesh.qos_query_capacity(&output);
    printf(MARA_PRE "  qos_cap_response_t.qos_error: %d\n", output.qos_error);
    printf(MARA_PRE "              qos_error_string: %s\n", 
           mara_mesh.qos_get_error_string(output.qos_error));

    if (output.qos_error == QOS_SUCCESS)
    {
        printf(MARA_PRE "  qos_cap_response_t.committed_stps:   %10d\n", output.committed_stps);
        printf(MARA_PRE "  qos_cap_response_t.current_max_stps: %10d\n", output.current_max_stps);
        printf(MARA_PRE "  qos_cap_response_t.committed_txps:    %10d\n", output.committed_txps);
        printf(MARA_PRE "  qos_cap_response_t.current_max_txps:  %10d\n", output.current_max_txps);
    }

    return output.qos_error;

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");
    return 0;

#endif
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_qmaintcache(mara_cmd_qmaintcache_t* cmdPtr)
{
#if NETWORK_TYPE_MESH == 1

    qos_maint_cache_t output;
    SYS_UINT32 retval;

    retval = mara_mesh.qos_get_maint_cache(&output);

    printf(MARA_PRE "  qos_maint_cache_t.ioc_nodemask:              0x%8.8x\n", 
           output.ioc_nodemask);
    printf(MARA_PRE "  qos_maint_cache_t.allocated_stps:            %10d\n", 
           output.allocated_stps);
    printf(MARA_PRE "  qos_maint_cache_t.allocated_txps:            %10d\n", 
           output.allocated_txps);
    printf(MARA_PRE "  qos_maint_cache_t.maintenance_cycle_counter: %10d\n", 
           output.maintenance_cycle_counter);

    return !(retval == RET_GOOD);

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");
    return 0;

#endif
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_qlist(mara_cmd_qlist_t* cmdPtr)
{
#if NETWORK_TYPE_MESH == 1

    qos_f_l_t          input;
    qos_f_l_response_t output;

    input.ingress_guid.hi = cmdPtr->ig.hi;
    input.ingress_guid.lo = cmdPtr->ig.lo;
    input.start_index     = cmdPtr->start;
    input.limit           = cmdPtr->limit;

    mara_mesh.qos_list_ingress_flows(&input, &output);
    if (output.qos_error != QOS_SUCCESS)
    {
        printf(MARA_PRE "  output.qos_error: %d\n", output.qos_error);
        printf(MARA_PRE "  qos_error_string: %s\n", 
               mara_mesh.qos_get_error_string(output.qos_error));
    }
    else
    {
        printf(MARA_PRE " %-30s: %s\n", "ingress_guid",
               mara_macToString(input.ingress_guid.hi, input.ingress_guid.lo));
        printf(MARA_PRE " %-30s: %d\n", "start_index",       input.start_index);
        printf(MARA_PRE " %-30s: %d\n", "limit",             input.limit);
        printf(MARA_PRE " %-30s: %d\n", "flow_update_count", output.flow_update_count);
        printf(MARA_PRE " %-30s: %d\n", "total_flows",       output.total_flows);
        printf(MARA_PRE " %-30s: %d\n", "returned_flows",    output.returned_flows);

        for (SYS_UINT32 index = 0; index < output.returned_flows; index++)
        {
            printf(MARA_PRE 
                   " output.flows[%02d]              : %s\n", 
                   index,
                   mara_macToString(output.flows[index].hi, 
                                    output.flows[index].lo));
        }
    }
    return output.qos_error;

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");

#endif
    return 0;
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_qquery(mara_cmd_qquery_t* cmdPtr)
{

#if NETWORK_TYPE_MESH == 1

    flow_name_t        input;
    qos_q_f_response_t output;

    input.flow_id.hi = cmdPtr->flow_id.hi;
    input.flow_id.lo = cmdPtr->flow_id.lo;

    mara_mesh.qos_query_ingress_flow(&input, &output);
    if (output.qos_error != QOS_SUCCESS)
    {
        printf(MARA_PRE "  output.qos_error: %d\n", output.qos_error);
        printf(MARA_PRE "  qos_error_string: %s\n", 
               mara_mesh.qos_get_error_string(output.qos_error));
    }
    else
    {
        printf(MARA_PRE " %-30s: %s\n", "flow_id",
               mara_macToString(output.flow_desc.flow_id.hi, 
                                output.flow_desc.flow_id.lo));
        printf(MARA_PRE " %-30s: %s\n", "packet_da",
               mara_macToString(output.flow_desc.packet_da.hi, 
                                output.flow_desc.packet_da.lo));
        printf(MARA_PRE " %-30s: %s\n", "ingress_guid", 
               mara_macToString(output.flow_desc.ingress_guid.hi, 
                                output.flow_desc.ingress_guid.lo));
        printf(MARA_PRE " %-30s: %s\n", "egress_guid", 
               mara_macToString(output.flow_desc.egress_guid.hi, 
                                output.flow_desc.egress_guid.lo));

        printf(MARA_PRE " %-30s: 0x%08x\n", "opaque",                output.flow_desc.opaque);
        printf(MARA_PRE " %-30s: %d\n",     "t_nom_pkt_size",        output.flow_desc.t_nom_pkt_size);
        printf(MARA_PRE " %-30s: %d\n",     "t_peak_data_rate_kbps", output.flow_desc.t_peak_data_rate_kbps);
        printf(MARA_PRE " %-30s: %d\n",     "t_lease_time",          output.flow_desc.t_lease_time);
        printf(MARA_PRE " %-30s: %d\n",     "t_burst_size",          output.flow_desc.t_burst_size);
        printf(MARA_PRE " %-30s: %u\n",     "time_remaining",        output.time_remaining);
    }
    return output.qos_error;

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");
    return 0;

#endif
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_qevents(mara_cmd_qevents_t* cmdPtr)
{
#if NETWORK_TYPE_MESH == 1

    qos_events_t   events;

    SYS_UINT32 drvrOpReturn = mara_mesh.qos_get_event_counts(&events);

    if (!drvrOpReturn)
    {
        printf(MARA_PRE "Driver failure.\n");
        return drvrOpReturn;
    }

    printf(MARA_PRE "  %-48s: %d\n", STRING_COMMA_VALUE(events.count_bw_exceeded));
    printf(MARA_PRE "  %-48s: %d\n", STRING_COMMA_VALUE(events.count_ingress_flow_changed));
    printf(MARA_PRE "  %-48s: %d\n", STRING_COMMA_VALUE(events.count_qos_enabled));
    printf(MARA_PRE "  %-48s: %d\n", STRING_COMMA_VALUE(events.count_topology_changed));
    printf(MARA_PRE "  %-48s: %d\n", STRING_COMMA_VALUE(events.count_ingress_routing_conflict_detected));

    return 0;

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");
    return 0;

#endif
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_qlistall(mara_cmd_qlistall_t* cmdPtr)
{
#if NETWORK_TYPE_MESH == 1

    qos_node_response_t nodeResponse;
    while (!mara_mesh.qos_query_nodes(&nodeResponse));

    for (SYS_UINT32 nodeIndex = 0; 
        nodeIndex < nodeResponse.number_nodes; 
        nodeIndex++)
    {
        qos_f_l_t          fl;
        qos_f_l_response_t flResponse;
        SYS_UINT32         listTries = (SYS_UINT32)cmdPtr->tries;

        fl.ingress_guid.hi = nodeResponse.nodes[nodeIndex].hi;
        fl.ingress_guid.lo = nodeResponse.nodes[nodeIndex].lo;
        fl.start_index     = 0;
        fl.limit           = QOS_MAX_FLOWS;

        for ( listTries = cmdPtr->tries; 
              listTries;
              listTries-- )
        {
            mara_mesh.qos_list_ingress_flows(&fl, &flResponse);
            if (0
                || flResponse.qos_error == QOS_SUCCESS
                || flResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE
                ) break;
        }

        if ( !listTries )
        {
            printf(MARA_PRE "ERROR: List failed repeatedly, last error number was %d (%s)\n",
                   flResponse.qos_error, mara_mesh.qos_get_error_string(flResponse.qos_error));
            mara_freeStringMemory();
            exit(-1);
        }

        if (flResponse.total_flows > QOS_MAX_FLOWS)
        {
            printf(MARA_PRE "ERROR: Developer shortcut prevents query of >32 flows, log a CR\n");
            mara_freeStringMemory();
            exit(-1);
        }

        /** Skip the node if it isn't pqos capable */
        if (flResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
        {
            printf(MARA_PRE "  IngressGuid: %s   No PQOS\n", 
                   mara_macToString(fl.ingress_guid.hi, fl.ingress_guid.lo));
            printf(MARA_PRE "\n");
            continue;
        }

        printf(MARA_PRE "  IngressGuid: %s   Flows: %2d\n", 
               mara_macToString(fl.ingress_guid.hi, fl.ingress_guid.lo),
               flResponse.returned_flows);

        for (SYS_UINT32 flowIndex = 0; 
            flowIndex < flResponse.returned_flows; 
            flowIndex++)
        {
            printf(MARA_PRE "     FlowId: %s\n", 
                   mara_macToString(flResponse.flows[flowIndex].hi, 
                                    flResponse.flows[flowIndex].lo) );
        }
        printf(MARA_PRE "\n");
    }
    return 0;

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");
    return 0;

#endif
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_qqueryall(mara_cmd_qqueryall_t* cmdPtr)
{
#if NETWORK_TYPE_MESH == 1

    qos_node_response_t nodeResponse;
    SYS_UINT32 queryTries;
    for ( queryTries = (SYS_UINT32)cmdPtr->tries;
          queryTries && !mara_mesh.qos_query_nodes(&nodeResponse);
          queryTries-- ) ;
    if (! queryTries )
    {
        printf(MARA_PRE "ERROR: qos_query_nodes failed repeatedly, last error number was %d (%s)\n",
               nodeResponse.qos_error, mara_mesh.qos_get_error_string(nodeResponse.qos_error));
        mara_freeStringMemory();
        exit(-1);
    }

#define MARA_QQUERYALL_HEADER "%-18s%-18s%-10s : "
#define MARA_QQUERYALL        "%-18s%-18s%-10s : "

    printf(MARA_PRE MARA_QQUERYALL_HEADER "%-20s\n", 
           "IngressGuid", "FlowId", "Item", "Value");

    for (SYS_UINT32 nodeIndex = 0; 
        nodeIndex < nodeResponse.number_nodes; 
        nodeIndex++)
    {
        qos_f_l_t          fl;
        qos_f_l_response_t flResponse;

        fl.ingress_guid.hi = nodeResponse.nodes[nodeIndex].hi;
        fl.ingress_guid.lo = nodeResponse.nodes[nodeIndex].lo;
        fl.start_index     = 0;
        fl.limit           = QOS_MAX_FLOWS;

        for ( queryTries = (SYS_UINT32)cmdPtr->tries;
              queryTries;
              queryTries-- ) 
        {
            mara_mesh.qos_list_ingress_flows(&fl, &flResponse);
            if (0 
                || flResponse.qos_error == QOS_SUCCESS
                || flResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE
                ) break;
        }
        if (! queryTries )
        {
            printf(MARA_PRE "ERROR: qos_list_ingress_flows failed repeatedly, last error number was %d (%s)\n",
                   flResponse.qos_error, mara_mesh.qos_get_error_string(flResponse.qos_error));
            mara_freeStringMemory();
            exit(-1);
        }

        /** Skip the node if it isn't pqos capable */
        if (flResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE) continue;

        if (flResponse.total_flows > QOS_MAX_FLOWS)
        {
            printf(MARA_PRE "ERROR: Developer shortcut prevents query of >32 flows, log a CR\n");
            mara_freeStringMemory();
            exit(-1);
        }

        for (SYS_UINT32 flowIndex = 0; 
            flowIndex < flResponse.returned_flows; 
            flowIndex++)
        {
            flow_name_t         name;
            qos_q_f_response_t  qfResponse;

            name.flow_id = flResponse.flows[flowIndex];

            for ( queryTries = (SYS_UINT32)cmdPtr->tries;
                  queryTries && !mara_mesh.qos_query_ingress_flow(&name, &qfResponse);
                  queryTries-- ) ;

            if (!queryTries)
            {
                printf(MARA_PRE "ERROR: qos_query_ingress_flow failed repeatedly, last error number was %d (%s)\n",
                       nodeResponse.qos_error, mara_mesh.qos_get_error_string(nodeResponse.qos_error));
                mara_freeStringMemory();
                exit(-1);
            }

            char ingressGuidLsbArray[20];
            strncpy(ingressGuidLsbArray, 
                    mara_macToString(qfResponse.flow_desc.ingress_guid.hi, 
                                     qfResponse.flow_desc.ingress_guid.lo),
                    sizeof(ingressGuidLsbArray));
            ingressGuidLsbArray[sizeof(ingressGuidLsbArray)-1] = 0;

            char egressGuidArray[20];
            strncpy(egressGuidArray, 
                    mara_macToString(qfResponse.flow_desc.egress_guid.hi, 
                                     qfResponse.flow_desc.egress_guid.lo),
                    sizeof(egressGuidArray));
            egressGuidArray[sizeof(egressGuidArray)-1] = 0;

            char flowIdArray[20];
            strncpy(flowIdArray, 
                    mara_macToString(qfResponse.flow_desc.flow_id.hi, 
                                     qfResponse.flow_desc.flow_id.lo),
                    sizeof(flowIdArray));
            flowIdArray[sizeof(flowIdArray)-1] = 0;

            char packetDaArray[20];
            strncpy(packetDaArray, 
                    mara_macToString(qfResponse.flow_desc.packet_da.hi, 
                                     qfResponse.flow_desc.packet_da.lo),
                    sizeof(packetDaArray));
            packetDaArray[sizeof(packetDaArray)-1] = 0;

            // packetDa
            printf(MARA_PRE MARA_QQUERYALL, ingressGuidLsbArray, packetDaArray, 
                "PacketDA" );
            printf("%s\n", packetDaArray);

            // egress
            printf(MARA_PRE MARA_QQUERYALL, ingressGuidLsbArray, flowIdArray, 
                "EgressGuid" );
            printf("%s\n", egressGuidArray);

            // peak
            printf(MARA_PRE MARA_QQUERYALL, ingressGuidLsbArray, flowIdArray, 
                   "PeakRateKbps" );
            printf("%d\n", qfResponse.flow_desc.t_peak_data_rate_kbps);

            // psize
            printf(MARA_PRE MARA_QQUERYALL, ingressGuidLsbArray, flowIdArray, 
                   "PacketSize" );
            printf("%d\n", qfResponse.flow_desc.t_nom_pkt_size);

            // lease
            printf(MARA_PRE MARA_QQUERYALL, ingressGuidLsbArray, flowIdArray, 
                   "LeaseTime" );
            printf("%u\n", qfResponse.flow_desc.t_lease_time);

            // burst
            printf(MARA_PRE MARA_QQUERYALL, ingressGuidLsbArray, flowIdArray, 
                   "BurstSize" );
            printf("%u\n", qfResponse.flow_desc.t_burst_size);

            // remaining
            printf(MARA_PRE MARA_QQUERYALL, ingressGuidLsbArray, flowIdArray, 
                   "Remaining" );
            printf("%u\n", qfResponse.time_remaining);
        }
    }

#undef MARA_QQUERYALL

    return 0;

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");
    return 0;

#endif
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_qdeleteall(mara_cmd_qdeleteall_t* cmdPtr)
{
#if NETWORK_TYPE_MESH == 1

    SYS_UINT32 deleteErrors = 0;

    qos_node_response_t nodeResponse;
    while (!mara_mesh.qos_query_nodes(&nodeResponse));

    for (SYS_UINT32 nodeIndex = 0; 
        nodeIndex < nodeResponse.number_nodes; 
        nodeIndex++)
    {
        qos_f_l_t          fl;
        qos_f_l_response_t flResponse;
        SYS_UINT32         listTries;

        fl.ingress_guid.hi = nodeResponse.nodes[nodeIndex].hi;
        fl.ingress_guid.lo = nodeResponse.nodes[nodeIndex].lo;
        fl.start_index     = 0;
        fl.limit           = QOS_MAX_FLOWS;

        for ( listTries = cmdPtr->tries; 
              listTries;
              listTries-- )
        {
            mara_mesh.qos_list_ingress_flows(&fl, &flResponse);
            if (0
                || flResponse.qos_error == QOS_SUCCESS
                || flResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE
                ) break;
        }

        if ( !listTries )
        {
            printf(MARA_PRE "ERROR: List failed repeatedly, last error number was %d (%s)\n",
                   flResponse.qos_error, mara_mesh.qos_get_error_string(flResponse.qos_error));
            mara_freeStringMemory();
            exit(-1);
        }

        if (flResponse.qos_error == QOS_INGRESS_NODE_NOT_PQOS_CAPABLE)
        {
            printf(MARA_PRE "  IngressGuid: %s   No PQOS\n", 
                   mara_macToString(fl.ingress_guid.hi, fl.ingress_guid.lo));
            continue;
        }

        if (flResponse.total_flows > QOS_MAX_FLOWS)
        {
            printf(MARA_PRE "ERROR: Developer shortcut prevents query of this many flows, log a CR\n");
            mara_freeStringMemory();
            exit(-1);
        }

        printf(MARA_PRE "  IngressGuid: %s   Flows: %2d\n", 
               mara_macToString(fl.ingress_guid.hi, fl.ingress_guid.lo),
               flResponse.returned_flows);

        for (SYS_UINT32 flowIndex = 0; 
            flowIndex < flResponse.returned_flows; 
            flowIndex++)
        {
            flow_name_t name;
            qos_d_response_t response;
            SYS_UINT32  retries = 3;

            INCTYPES_SAFE_VAR_COPY(name.flow_id, flResponse.flows[flowIndex]);

            do
            {
                if (mara_mesh.qos_delete_flow(&name,&response))
                {
                    printf( MARA_PRE "     Deleted FlowID: %s\n", 
                            mara_macToString(flResponse.flows[flowIndex].hi, 
                                            flResponse.flows[flowIndex].lo) );
                    break;
                }

                mara_tmr_spin(100);

                if (retries && (response.qos_error == QOS_TIMEOUT))
                {
                    retries--;
                    printf( MARA_PRE "     Saw timeout while deleting FlowID %s; retrying\n", 
                            mara_macToString(name.flow_id.hi, name.flow_id.lo));
                    continue;
                }

                deleteErrors++;
                printf( MARA_PRE "     Error %d while deleting FlowID %s; remaining tries = %d\n", 
                        response.qos_error,
                        mara_macToString(name.flow_id.hi, name.flow_id.lo),
                        retries );

            } while (SYS_TRUE);
        }
    }
    return deleteErrors;

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");
    return 0;

#endif
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_qcapability(mara_cmd_qcapability_t* cmdPtr)
{
#if NETWORK_TYPE_MESH == 1

    clink_guid_t guid = {0,0};
    qos_i_c_response_t  output;

    SYS_UINT32 drvrOpReturn = mara_mesh.qos_query_interface_capabilities(&guid, &output);

    if (!drvrOpReturn)
    {
        printf(MARA_PRE "Driver failure.\n");
        return drvrOpReturn;
    }

    if (output.qos_error != QOS_SUCCESS)
    {
        printf(MARA_PRE "  output.qos_error: %d\n", output.qos_error);
        printf(MARA_PRE "  qos_error_string: %s\n", 
               mara_mesh.qos_get_error_string(output.qos_error));
    }
    else
    {
        printf(MARA_PRE "  %-30s: %s\n", "segment_id", 
               mara_macToString(output.segment_id.hi, 
                                output.segment_id.lo));

        printf(MARA_PRE "  %-30s: 0x%08x\n", "iana_technology_type", output.iana_technology_type);
        printf(MARA_PRE "  %-30s: 0x%08x\n", "capabilities",    output.capabilities);

        printf(MARA_PRE "  %-30s: 0x%08x\n", "t_max_phy_rate",  output.t_max_phy_rate);
        printf(MARA_PRE "  %-30s: 0x%08x\n", "t_max_data_rate", output.t_max_data_rate);
        printf(MARA_PRE "  %-30s: 0x%08x\n", "t_channel_freq",  output.t_channel_freq);
    }
    return output.qos_error;

#else

    printf(MARA_PRE "Command unavailable on this platform.\n");
    return 0;

#endif
}


/** ***************************************************************************
 */

void
mara_sleep_func(SYS_UINT32 ms_to_sleep)
{
    SYS_UINT32 MS = 10;
    mara_tmr_spin(MS);
}


static SYS_UINT32 mara_unpack_phyrate(SYS_UINT16 packed_val)
{
    // = BitsPerOFDM * (((1,000,000,000/((FFT_LENGTH+CPLen) * SLOTLEN_IN_NANOSECS)) * 192)/208);
    SYS_UINT32 cplen = ((packed_val >> 11) << 1) + 10;
    SYS_UINT32 bits = packed_val & 0x7ff;
    bits *= (50*1000*1000 * 12 / 13 / (256+cplen));
    return bits;
}


mara_unix_exit_status_t
mara_handler_fmr(mara_cmd_fmr_t* cmdPtr)
{
    peer_rates_t              peer;
    peer_rates_entry_status_t status;
    unsigned short            *valp;
    peer_rates_entry_enum     *enump;
    int k, m;

    if (!mara_mesh.GetPeerRates(&peer, &status, mara_sleep_func))
    {
        printf("GetPeerRates() failed\n");
        return 1;
    }

    /* Had to change the way the display algorithm was written 
     * because the valp and enump pointers were not being 
     * incremented properly for a two dimensional array
     * when MAX_NUM_NODES is not equal to MRT_MAX_NODES.
     *
     * For PROTEM Builds:
     *   MAX_NUM_NODES == 8
     *   MRT_MAX_NODES == 16 (always MoCA 1.1)
     *
     * For ADVANCED Builds:
     *   MAX_NUM_NODES == 16
     *   MRT_MAX_NODES == 16 (always MoCA 1.1)
     */

    for ( k = 0; k < MAX_NUM_NODES; k++ )
    {
        valp  = &(peer.rates[k][0]);
        enump = &(status.entry_status[k][0]);
        for ( m = 0; m < MAX_NUM_NODES; m++ )
        {
            SYS_UINT32 bits = (mara_unpack_phyrate(*valp) + 500000) / 1000000;
            switch( *enump )
            {
                case PEER_RATES_VALID:
                    printf("%3d ", bits);
                    break;
                case PEER_RATES_NO_LINK_TO_QUERY:
                    printf("--- ");
                    break;
                case PEER_RATES_CANNOT_QUERY_LINK:
                    printf("xxx ");
                    break;
                default:
                    printf("??? ");
                    break;
            }
            enump++;
            valp++;    
        }
        printf("\n");
    }
    return 0;
}


mara_unix_exit_status_t
mara_handler_fmrraw(mara_cmd_fmrraw_t* cmdPtr)
{
    peer_rates_t                peer;
    peer_rates_entry_status_t   status;
    unsigned short              *valp;
    peer_rates_entry_enum       *enump;
    int k, m;

    if (!mara_mesh.GetPeerRates(&peer, &status, mara_sleep_func))
    {
        printf("GetPeerRates failed\n");
        return 1;
    }

    /* Had to change the way the display algorithm was written
     * because the valp and enump pointers were not being incremented
     * properly for a two dimensional array when MAX_NUM_NODES is 
     * not equal to MRT_MAX_NODES.
     *
     * For PROTEM Builds:
     *   MAX_NUM_NODES == 8
     *   MRT_MAX_NODES == 16 (always MoCA 1.1)
     *
     * For ADVANCED Builds:
     *   MAX_NUM_NODES == 16
     *   MRT_MAX_NODES == 16 (always MoCA 1.1)
     */

    for ( k = 0; k < MAX_NUM_NODES; k++ )
    {
        valp  = &(peer.rates[k][0]);
        enump = &(status.entry_status[k][0]);
        for ( m = 0; m < MAX_NUM_NODES; m++ )
        {
            switch( *enump )               
            {
                case PEER_RATES_VALID:
                    printf("%4d/%2d  ", (*valp) & 0x7ff, (((*valp) >> 11) << 1) + 10);
                    break;
        
                case PEER_RATES_CANNOT_QUERY_LINK:
                    printf("xxxx/xx  ");
                    break;
        
                case PEER_RATES_NO_LINK_TO_QUERY:
                    printf("----/--  ");
                    break;

                default:
                    printf("\?\?\?\?/\?\?  ");
                    break;
            }
            valp++;
            enump++;
        }
        printf("\n");
    }
    return 0;
}


mara_unix_exit_status_t
mara_handler_fmrphy(mara_cmd_fmrphy_t* cmdPtr)
{
    peer_rates_t                peer;
    peer_rates_entry_status_t   status;
    unsigned short              *valp;
    peer_rates_entry_enum       *enump;
    int k, m;

    if (!mara_mesh.GetPeerRates(&peer, &status, mara_sleep_func))
    {
        printf("GetPeerRates failed\n");
        return 1;
    }

    /*Had to change the way the display algorithm was written
     * because the valp and enump pointers were not being incremented 
     * properly for a two dimensional array when MAX_NUM_NODES is not
     * equal to MRT_MAX_NODES.
     *
     * For PROTEM Builds:
     *   MAX_NUM_NODES == 8
     *   MRT_MAX_NODES == 16 (always MoCA 1.1)
     *
     * For ADVANCED Builds:
     *   MAX_NUM_NODES == 16
     *   MRT_MAX_NODES == 16 (always MoCA 1.1)
     */

    for ( k = 0; k < MAX_NUM_NODES; k++ )
    {
        valp  = &(peer.rates[k][0]);
        enump = &(status.entry_status[k][0]);
        for ( m = 0; m < MAX_NUM_NODES; m++)
        {
            switch( *enump )
            {
                case PEER_RATES_VALID:
                    printf("%9d ", mara_unpack_phyrate(*valp));
                    break;
                case PEER_RATES_NO_LINK_TO_QUERY:
                    printf("     ---  ");
                    break;
                case PEER_RATES_CANNOT_QUERY_LINK:
                    printf("     xxx  ");
                    break;
                default: 
                    printf("%9s ", "???");
                    break;
            }
            enump++;
            valp++;
        }
        printf("\n");
    }
    return 0;
}


mara_unix_exit_status_t
mara_handler_fmrctp(mara_cmd_fmrctp_t* cmdPtr)
{
    peer_rates_t                peer;
    peer_rates_entry_status_t   status;
    unsigned short              *valp;
    peer_rates_entry_enum       *enump;
    int k, m;

    if (!mara_mesh.GetPeerRates(&peer, &status, mara_sleep_func))
    {
        printf("GetPeerRates failed\n");
        return 1;
    }

    /* Had to change the way the display algorithm was written
     * because the valp and enump pointers were not being incremented
     * properly for a two dimensional array when MAX_NUM_NODES is not 
     * equal to MRT_MAX_NODES.
     *
     * For PROTEM Builds:
     *   MAX_NUM_NODES == 8
     *   MRT_MAX_NODES == 16 (always MoCA 1.1)
     *
     * For ADVANCED Builds:
     *   MAX_NUM_NODES == 16
     *   MRT_MAX_NODES == 16 (always MoCA 1.1)
     */

    for ( k = 0; k < MAX_NUM_NODES; k++ )
    {
        valp  = &(peer.rates[k][0]);
        enump = &(status.entry_status[k][0]);
        for ( m = 0; m < MAX_NUM_NODES; m++)
        {
            switch( *enump )
            {
                case PEER_RATES_VALID:
                    printf("%4d~%2d  ", (*valp) & 0x7ff, (((*valp) >> 11)));
                    break;

                case PEER_RATES_CANNOT_QUERY_LINK:
                    printf("xxxx~xx  ");
                    break;

                case PEER_RATES_NO_LINK_TO_QUERY:
                    printf("----~--  ");
                    break;

                default:
                    printf("\?\?\?\?~\?\?  ");
                    break;
            }
            valp++;
            enump++;
        }
        printf("\n");
    }
    return 0;
}


/** ***************************************************************************
 */

/** For best validation, need some counters that increment inside of spoofed
 *  customer interface functions */
SYS_INT32 mara_nms_internal_allocation_counts[CLNK_NMS_ELEMENT_POOL_NAME_MAX] = {0};


SYS_UINT32 mara_nms_unexpected_count = 0;

SYS_BOOLEAN mara_show_exports = SYS_FALSE;

void
mara_nms_context_unexpected(SYS_BOOLEAN  is_fatal,
                            const char*  file,
                            SYS_UINT32   line,
                            const char*  string_ptr,
                            SYS_UINT32   param1,
                            SYS_UINT32   param2)
{
    const int BUFFER_SIZE=200;
    char      buffer[BUFFER_SIZE];
    const char* type = is_fatal ? "FATAL" : "Error";

    mara_nms_unexpected_count++;

    snprintf(buffer, BUFFER_SIZE, 
             "UNEXPECTED %s(%u) %s: %s\n", 
             file, line, type, string_ptr);

    if (mara_show_exports) printf("export ERROR=%d\n", mara_nms_unexpected_count);

    printf(buffer, param1, param2);
    if (is_fatal)
    {
        printf("IMMEDIATE EXIT.\n");
        mara_freeStringMemory();
        exit(-1);
    }
}

void
mara_nms_context_verify_pool_args(const char* file,
                                  SYS_UINT32  line,
                                  enum clnk_nms_element_pool_name_t   poolname,
                                  enum clnk_nms_element_block_size_t  size)
{
    if (poolname >= CLNK_NMS_ELEMENT_POOL_NAME_MAX)
    {
        mara_nms_context_unexpected(SYS_TRUE, __FILE__, __LINE__, 
                                    "bad pool arg", 0, 0);
    }

    if (poolname == CLNK_NMS_ELEMENT_POOL_NAME_SMALL_4B &&
                                             size != 4)
    {
        mara_nms_context_unexpected(SYS_TRUE, __FILE__, __LINE__, 
                                    "bad pool size %d %d", poolname, size);
    }

    if (poolname == CLNK_NMS_ELEMENT_POOL_NAME_MEDIUM_8B &&
                                              size != 8)
    {
        mara_nms_context_unexpected(SYS_TRUE, __FILE__, __LINE__, 
                                    "bad pool size %d %d", poolname, size);
    }

    if (poolname == CLNK_NMS_ELEMENT_POOL_NAME_LARGE_256B &&
                                             size != 256)
    {
        mara_nms_context_unexpected(SYS_TRUE, __FILE__, __LINE__, 
                                    "bad pool size %d %d", poolname, size);
    }
}

clnk_nms_element_handle_t
mara_nms_context_allocate(enum clnk_nms_element_pool_name_t   pool,
                          enum clnk_nms_element_block_size_t  size)
{
    SYS_INT32* allocated;
    clnk_nms_element_handle_t retval = CLNK_NMS_ELEMENT_HANDLE_INVALID;

    INCTYPES_VERIFY_STRUCT_SIZE(clnk_nms_element_handle_t,sizeof(allocated));

    mara_nms_context_verify_pool_args(__FILE__, __LINE__, pool, size);

    allocated = (SYS_INT32*)malloc(size + 4);

    if (allocated)
    {
        // Set a reference count initial value
        allocated[0] = 1;

        retval = (clnk_nms_element_handle_t)allocated;
    }
    mara_nms_internal_allocation_counts[pool]++;

    return retval;
}


void 
mara_nms_context_increment(clnk_nms_element_handle_t handle)
{
    SYS_INT32* value = (SYS_INT32*)handle;
    value[0]++;
}


SYS_BOOLEAN
mara_nms_context_decrement_maybe_delete(enum clnk_nms_element_pool_name_t   poolname,
                                        enum clnk_nms_element_block_size_t  size, 
                                        clnk_nms_element_handle_t           handle)
{
    SYS_INT32* value = (SYS_INT32*)handle;

    mara_nms_context_verify_pool_args(__FILE__, __LINE__, poolname, size);

    value[0]--;
    if (value[0] <= 0)
    {
        free(value);
        mara_nms_internal_allocation_counts[poolname]--;
        return SYS_TRUE;
    }

    return SYS_FALSE;
}

void*
mara_nms_context_dereference(enum clnk_nms_element_pool_name_t   poolname,
                             enum clnk_nms_element_block_size_t  size, 
                             clnk_nms_element_handle_t           handle)
{
    SYS_INT32* value = (SYS_INT32*)handle;
    void*      retval = &value[1];

    mara_nms_context_verify_pool_args(__FILE__, __LINE__, poolname, size);

    return retval;
}

clnk_nms_stats_t mara_nms_context_stats;


void
mara_nms_show_stats(SYS_UINT32 display_stats_option)
{
    SYS_UINT32 poolname;
    const char* pre_ptr = "      ";
    static int report_num = -1;

    report_num++;

    if (display_stats_option == 1)
    {
        for (poolname = 0; poolname < CLNK_NMS_ELEMENT_POOL_NAME_MAX; poolname++)
        {
            printf("%s nms stats report %2.2d: size:%3.3d count:%5.5d peak:%5.5d\n", 
                   pre_ptr,
                   report_num,
                   mara_nms_context_stats.pools[poolname].block_size,
                   mara_nms_context_stats.pools[poolname].num_allocated,
                   mara_nms_context_stats.pools[poolname].peak_allocated);

            if (mara_nms_internal_allocation_counts[poolname] != 
                mara_nms_context_stats.pools[poolname].num_allocated)
            {
                mara_nms_context_unexpected(SYS_TRUE, __FILE__, __LINE__, 
                                            "API statistics do not tell truth about allocation %d %d",
                                            poolname,
                                            mara_nms_internal_allocation_counts[poolname]);
            }
        }
    }

    if (display_stats_option == 2)
    {
        for (poolname = 0; poolname < CLNK_NMS_ELEMENT_POOL_NAME_MAX; poolname++)
        {
            printf("export STATS%3.3d_%3.3d=%d\n", 
                   report_num,
                   mara_nms_context_stats.pools[poolname].block_size,
                   mara_nms_context_stats.pools[poolname].num_allocated);
        }
    }
}

const char*
mara_nms_str_disposition(clnk_nms_disposition_t disposition)
{
    switch (disposition)
    {
    case CLNK_NMS_DISPOSITION_UNRETRIEVABLE: return "UNRETRIEVABLE";
    case CLNK_NMS_DISPOSITION_FRESH:         return "FRESH";
    case CLNK_NMS_DISPOSITION_STALE:         return "STALE";
    case CLNK_NMS_DISPOSITION_NIN:           return "NIN";
    case CLNK_NMS_DISPOSITION_UNKNOWN:       return "UNKNOWN";
    }
    return "!!ERROR!!";
}

const char*
mara_nms_str_type(clnk_nms_type_t type)
{
    switch (type)
    {
    case CLNK_NMS_TYPE_UINT32:         return "UINT32";
    case CLNK_NMS_TYPE_INT32:          return "INT32";
    case CLNK_NMS_TYPE_CENTI_DECIBELS: return "CENTI_DECIBELS";
    case CLNK_NMS_TYPE_MAC_ADDR:       return "MAC_ADDR";
    case CLNK_NMS_TYPE_PROFILE:        return "PROFILE";
    case CLNK_NMS_TYPE_PASSWORD:       return "MOCAPASSWORD";
    }
    return "!!ERROR!!";
}

const char*
mara_nms_str_cardinality(clnk_nms_cardinality_t cardinality)
{
    switch (cardinality)
    {
    case CLNK_NMS_CARDINALITY_NETWORK: return "NETWORK";
    case CLNK_NMS_CARDINALITY_NODE:    return "NODE";
    case CLNK_NMS_CARDINALITY_LINK:    return "LINK";
    }
    return "!!ERROR!!";
}

void
mara_nms_show_element_contents(const clnk_nms_pdef_t*  pdef_ptr,
                               clnk_nms_disposition_t  disposition,
                               clnk_nms_eview_cptr     eview_cptr,
                               const char*             prefix_ptr,
                               clink_node_id_t         queried_node_id,
                               clink_node_id_t         remote_node_id,
                               SYS_BOOLEAN             show_all)
{
    printf("%s",prefix_ptr);
    printf("/%3.3d ", pdef_ptr->param);
    switch (pdef_ptr->cardinality)
    {
    case CLNK_NMS_CARDINALITY_NETWORK: 
        printf("[--][--]"); 
        break;
    case CLNK_NMS_CARDINALITY_NODE: 
        printf("[%2.2d][--]", queried_node_id); 
        break;
    case CLNK_NMS_CARDINALITY_LINK: 
        printf("[%2.2d][%2.2d]", queried_node_id, remote_node_id); 
        break;
    default: 
        printf("!!!!");
    }

    printf("  %-15s", mara_nms_str_disposition(disposition));

    if (eview_cptr)
    {
        switch (pdef_ptr->type)
        {
        case CLNK_NMS_TYPE_UINT32:         
            printf("%11u  -  0x%8.8x", 
                   eview_cptr->s.uint32, 
                   eview_cptr->s.uint32); 
            break;
        case CLNK_NMS_TYPE_INT32:          
            printf("%11u", 
                   eview_cptr->s.int32);
            break;
        case CLNK_NMS_TYPE_CENTI_DECIBELS: 
            {
                SYS_INT32 cdb = eview_cptr->s.int32; 
                printf("%d.%02d", cdb / 100, abs(cdb) % 100);  
            }
            break;
        case CLNK_NMS_TYPE_MAC_ADDR:       
            printf("%02x:%02x:%02x:%02x:%02x:%02x",
                   eview_cptr->m.mac_address[0],
                   eview_cptr->m.mac_address[1],
                   eview_cptr->m.mac_address[2],
                   eview_cptr->m.mac_address[3],
                   eview_cptr->m.mac_address[4],
                   eview_cptr->m.mac_address[5]);
            break;
        case CLNK_NMS_TYPE_PROFILE:
            {
                SYS_UINT32 bin;

                for (bin = 0; bin < 256; bin++)
                {
                    if (!(bin%32))
                    {
                        printf("\n%s    ", prefix_ptr);
                    }
                    printf("%d", eview_cptr->l.profile_array[bin]);
                }
            }
            break;
        case CLNK_NMS_TYPE_PASSWORD:       
            {
                SYS_UINT32 passwd_char;

                for (passwd_char = 0; 
                     passwd_char < INCTYPES_ARRAY_LEN(eview_cptr->l.mocapassword.value); 
                     passwd_char++)
                {
                    printf("%c", eview_cptr->l.mocapassword.value[passwd_char]);
                    if (!eview_cptr->l.mocapassword.value[passwd_char]) break;
                }
            }
            break;
        default:
            printf("!!!!!");
        }
    }
    else
    {
        printf("NULL"); 
    }

    printf("\n");
}


void
mara_nms_show_element(const clnk_nms_cache_t* cache_ptr,
                      const clnk_nms_pdef_t*  pdef_ptr,
                      const char*             prefix_ptr,
                      clink_node_id_t         queried_node_id,
                      clink_node_id_t         remote_node_id,
                      SYS_UINT32              show_all)
{
    clnk_nms_disposition_t disposition;

    clnk_nms_eview_cptr eview_cptr =
        clnk_nms_view_element(cache_ptr, pdef_ptr->param,
                              queried_node_id, remote_node_id,
                              &disposition);

    /* Do not show elements with null pointers unless forced */
    if (!show_all && !eview_cptr) return;

    mara_nms_show_element_contents(pdef_ptr, disposition, eview_cptr, prefix_ptr,
                                   queried_node_id, remote_node_id,
                                   show_all);
}

SYS_UINT32
mara_nms_count_nonnull_elements(const clnk_nms_cache_t* cache_ptr,
                                const clnk_nms_pdef_t*  pdef_ptr)
{
    SYS_UINT32 retval = 0;
    clink_node_id_t  queried_node_id;
    clink_node_id_t  remote_node_id;

    for (queried_node_id = 0;
         queried_node_id < MAX_NUM_NODES;
         queried_node_id++)
    {
        for (remote_node_id = 0;
             remote_node_id < MAX_NUM_NODES;
             remote_node_id++)
        {
            if (NULL != clnk_nms_view_element(cache_ptr, pdef_ptr->param,
                                              queried_node_id, remote_node_id,
                                              NULL)) retval++;

            if (pdef_ptr->cardinality == CLNK_NMS_CARDINALITY_LINK) continue;
            break;
        }

        if (pdef_ptr->cardinality == CLNK_NMS_CARDINALITY_NETWORK) break;
    }
    return retval;
}

void
mara_nms_show_param_header(const clnk_nms_pdef_t* pdef_ptr)
{
    printf(" param:%-30s cardinality:%-10s type:%-10s\n", 
           pdef_ptr->name_ptr, 
           mara_nms_str_cardinality(pdef_ptr->cardinality), 
           mara_nms_str_type(pdef_ptr->type)
           );
}

void
mara_nms_show_cache_contents(const clnk_nms_cache_t* cache_ptr,
                             SYS_BOOLEAN show_all)
{
    clnk_nms_param_t param_loop;

    for (param_loop = 0; 
          param_loop < CLNK_NMS_PARAM__MAX; 
          param_loop++)
    {
        clink_node_id_t queried;
        clink_node_id_t remote;
        const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(param_loop);

        if (!show_all && 
            !mara_nms_count_nonnull_elements(cache_ptr,pdef_ptr)) continue;

        mara_nms_show_param_header(pdef_ptr);

        switch (pdef_ptr->cardinality)
        {
        case CLNK_NMS_CARDINALITY_NETWORK:
            mara_nms_show_element(cache_ptr, pdef_ptr, "     ",
                                  0, 0, show_all);
            break;

        case CLNK_NMS_CARDINALITY_NODE:
            for (queried = 0; queried < MAX_NUM_NODES; queried++)
            {
                mara_nms_show_element(cache_ptr, pdef_ptr, "     ",
                                      queried, 0, show_all);
            }
            break;

        case CLNK_NMS_CARDINALITY_LINK:
            for (queried = 0; queried < MAX_NUM_NODES; queried++)
            {
                for (remote = 0; remote < MAX_NUM_NODES; remote++)
                {
                    mara_nms_show_element(cache_ptr, pdef_ptr, "     ",  
                                          queried, remote, show_all);
                }
            }

            break;
        }
    }
}

const clnk_nms_context_t  
        mara_nms_context =
            {
                mara_nms_context_allocate,
                mara_nms_context_increment,
                mara_nms_context_decrement_maybe_delete,
                mara_nms_context_dereference,
                mara_nms_context_unexpected,
                &mara_nms_context_stats
            };


#define MARA_NMS_ESETSPEC_PARSE_RETURN_FALSE_IF(condition, description, string_ptr, next_ptr) \
do                                                                                            \
{                                                                                             \
    if (condition)                                                                            \
    {                                                                                         \
        SYS_UINT32 count = (*next_ptr);                                                       \
        printf(__FILE__ "(%5d) :Parser fail: %s\n", __LINE__, (description));                 \
        printf(__FILE__ "        :  condition was: %s\n", #condition);                        \
        printf(__FILE__ "        :   %s\n", (string_ptr));                                    \
        printf(__FILE__ "        :   ");                                                      \
        while (count--)                                                                       \
        {                                                                                     \
            printf(" ");                                                                      \
        }                                                                                     \
        printf("^\n");                                                                        \
                                                                                              \
        return SYS_FALSE;                                                                     \
    }                                                                                         \
} while (0)


SYS_BOOLEAN
mara_nms_extract_wildcard(const char*        string_ptr,
                          SYS_UINT32*        next_ptr,
                          SYS_UINT32         last, 
                          clink_nodemask_t*  queried_nodemask_ptr,
                          clink_nodemask_t*  remote_nodemask_ptr )
{
    *queried_nodemask_ptr = CLINK_NODEMASK_ALL;
    *remote_nodemask_ptr  = CLINK_NODEMASK_ALL;

    if (string_ptr[*next_ptr] == '.')
    {
        (*next_ptr) += 1;
    }
    else
    {
        long int number;
        char*    end_ptr;

        number = strtol(string_ptr + *next_ptr, &end_ptr, 10);

        MARA_NMS_ESETSPEC_PARSE_RETURN_FALSE_IF(errno, 
                                                "expected queried node id or wildcard '.'",
                                                string_ptr, next_ptr);

        MARA_NMS_ESETSPEC_PARSE_RETURN_FALSE_IF(number >= MAX_NUM_NODES, 
                                                "node_id out of range",
                                                string_ptr, next_ptr);

        *queried_nodemask_ptr = 1UL << number;
        *next_ptr = end_ptr - string_ptr;
    }

    if (string_ptr[*next_ptr] == ':')
    {
        (*next_ptr) += 1;
    }

    if (string_ptr[*next_ptr] == '.')
    {
        (*next_ptr) += 1;
    }
    else
    {
        long int number;
        char*    end_ptr;

        number = strtol(string_ptr + *next_ptr, &end_ptr, 10);

        MARA_NMS_ESETSPEC_PARSE_RETURN_FALSE_IF(errno, 
                                                "expected remote node id",
                                                string_ptr, next_ptr);

        MARA_NMS_ESETSPEC_PARSE_RETURN_FALSE_IF(number >= MAX_NUM_NODES, 
                                                "node_id out of range",
                                                string_ptr, next_ptr);

        *remote_nodemask_ptr = 1UL << number;
        *next_ptr = end_ptr - string_ptr;
    }

    return SYS_TRUE;
}


SYS_BOOLEAN
mara_nms_extract_param_complex(clnk_nms_eset_t*        eset_ptr,
                               const clnk_nms_pdef_t*  pdef_ptr,
                               const char*             string_ptr,
                               SYS_UINT32*             next_ptr,
                               SYS_UINT32              last)
{
    while (*next_ptr < last && string_ptr[*next_ptr] != '/')
    {
        clink_nodemask_t  queried_nodemask;
        clink_nodemask_t  remote_nodemask;

        if (string_ptr[*next_ptr] == '-' ||
            string_ptr[*next_ptr] == ',' )
        {
            (*next_ptr) += 1;
            continue;
        }

        /** Read a wildcard */
        if (!mara_nms_extract_wildcard(string_ptr,next_ptr,last,
                                       &queried_nodemask,
                                       &remote_nodemask))
        {
            /** Error already reported. */
            return SYS_FALSE;
        }

        clnk_nms_eset_include_wildcard(eset_ptr, pdef_ptr,
                                       queried_nodemask,
                                       remote_nodemask);
    }

    return SYS_TRUE;
}


SYS_BOOLEAN
mara_nms_extract_eset_param(clnk_nms_eset_t*   eset_ptr,
                            const char*        string_ptr,
                            SYS_UINT32*        next_ptr,
                            SYS_UINT32         last)
{
    long int param;
    char *end_ptr;
    const clnk_nms_pdef_t* pdef_ptr;

    errno = 0;

    MARA_NMS_ESETSPEC_PARSE_RETURN_FALSE_IF(string_ptr[*next_ptr] != '/', 
                                            "Expected parameter start delimiter '/'",
                                            string_ptr, next_ptr);
    (*next_ptr)++;

    MARA_NMS_ESETSPEC_PARSE_RETURN_FALSE_IF(*next_ptr > last, 
                                            "Unexpected string end",
                                            string_ptr, next_ptr);

    param = strtol(string_ptr + *next_ptr, &end_ptr, 10);

    MARA_NMS_ESETSPEC_PARSE_RETURN_FALSE_IF(errno, 
                                            "Expected valid numerical parameter id",
                                            string_ptr, next_ptr);

    (*next_ptr) = end_ptr - string_ptr;

    MARA_NMS_ESETSPEC_PARSE_RETURN_FALSE_IF(param >= CLNK_NMS_PARAM__MAX, 
                                            "Parameter number is out of range",
                                            string_ptr, next_ptr);

    pdef_ptr = clnk_nms_get_pdef((clnk_nms_param_t)param);

    MARA_NMS_ESETSPEC_PARSE_RETURN_FALSE_IF(!pdef_ptr, 
                                            "Parameter definition lookup fail",
                                            string_ptr, next_ptr);

    if (*next_ptr == last || string_ptr[*next_ptr] == '/')
    {
        clnk_nms_eset_include_wildcard(eset_ptr, pdef_ptr,
                                       CLINK_NODEMASK_ALL, 
                                       CLINK_NODEMASK_ALL);

        return SYS_TRUE;
    }

    return mara_nms_extract_param_complex(eset_ptr, pdef_ptr, string_ptr, 
                                          next_ptr, last);
}


SYS_BOOLEAN
mara_nms_extract_eset(clnk_nms_eset_t*   eset_ptr,
                      const char*        string_ptr)
{
    const SYS_UINT32 last = strlen(string_ptr);
    SYS_UINT32       next = 0;

    while (next < last)
    {
        /** Extract a single parameter */
        if (!mara_nms_extract_eset_param(eset_ptr, string_ptr, &next, last))
        {
            return SYS_FALSE;
        }
    }

    return SYS_TRUE;
}


void
mara_nms_show_eset(const clnk_nms_eset_t* eset_ptr)
{
    clnk_nms_param_t param;
    for (param = 0; param < CLNK_NMS_PARAM__MAX; param++)
    {
        const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(param);

        if (!clnk_nms_eset_test_any(eset_ptr,pdef_ptr,
                                   CLINK_NODEMASK_ALL, 
                                   CLINK_NODEMASK_ALL))  continue;

        printf("   /%03d", param);

        switch (pdef_ptr->cardinality)
        {
        case CLNK_NMS_CARDINALITY_NETWORK:
            break;
        case CLNK_NMS_CARDINALITY_NODE:
            {
                clink_nodemask_t queried_nodemask = eset_ptr->included[param][0];
                SYS_BOOLEAN is_first = SYS_TRUE;
                clink_node_id_t node_id;

                if (queried_nodemask == CLINK_NODEMASK_ALL) break;

                for (node_id = 0; node_id < MAX_NUM_NODES; node_id++)
                {
                    if (!INCTYPES_NODE_IN_NODEMASK(node_id, queried_nodemask)) 
                    {
                        continue;
                    }

                    if (is_first)
                    {
                        printf("-");
                        is_first = SYS_FALSE;
                    }
                    else
                    {
                        printf(",");
                    }

                    printf("%d", node_id);
                }

                break;
            }
        case CLNK_NMS_CARDINALITY_LINK:
            {
                clink_node_id_t   queried_node_id;
                clink_node_id_t   remote_node_id;

                /** Indexed first by queried then remote */
                SYS_BOOLEAN       matrix[MAX_NUM_NODES][MAX_NUM_NODES];

                /** Indexed by queried node id. This array answers the 
                 *  question: does the set of eset specified links involving 
                 *  this queried node id include the full set of remote 
                 *  node ids in this network? */
                SYS_BOOLEAN       all_remote_nodes_included[MAX_NUM_NODES];

                /** Indexed by queried node id. This array answers the 
                 *  question: does the set of eset specified links not include
                 *  the indexing queried node id? */
                SYS_BOOLEAN       all_remote_nodes_excluded[MAX_NUM_NODES];

                /** only SYS_TRUE if absolutely every link in this network
                 *  is part of the query */
                SYS_BOOLEAN       is_all_inclusive_wildcard = SYS_TRUE;

                /** true if the next entry needs a preceeding comma separator */
                SYS_BOOLEAN       needs_comma = SYS_FALSE;

                INCTYPES_SAFE_VAR_ZERO(matrix);
                INCTYPES_SAFE_VAR_ZERO(all_remote_nodes_included);
                INCTYPES_SAFE_VAR_ZERO(all_remote_nodes_excluded);

                /** Check for wildcard and early exit */
                for (remote_node_id = 0;
                     remote_node_id < MAX_NUM_NODES;
                     remote_node_id++)
                {
                    clink_nodemask_t  queried_nodemask = 
                        eset_ptr->included[param][remote_node_id];

                    for (queried_node_id = 0;
                         queried_node_id < MAX_NUM_NODES;
                         queried_node_id++)
                    {
                        if (queried_node_id == remote_node_id) continue;

                        if (INCTYPES_NODE_IN_NODEMASK(queried_node_id, 
                                                      queried_nodemask))
                        {
                            matrix[queried_node_id][remote_node_id] = SYS_TRUE;
                        }
                    }
                }

                for (queried_node_id = 0;
                     queried_node_id < MAX_NUM_NODES;
                     queried_node_id++)
                {
                    all_remote_nodes_included[queried_node_id] = SYS_TRUE;
                    all_remote_nodes_excluded[queried_node_id] = SYS_TRUE;

                    for (remote_node_id = 0;
                         remote_node_id < MAX_NUM_NODES;
                         remote_node_id++)
                    {
                        SYS_BOOLEAN entry = matrix[queried_node_id][remote_node_id];

                        /** Always ignore diagonals since they are meaningless */
                        if (queried_node_id == remote_node_id) continue;

                        if (entry)
                        {
                            all_remote_nodes_excluded[queried_node_id] = SYS_FALSE;
                        }
                        else
                        {
                            all_remote_nodes_included[queried_node_id] = SYS_FALSE;
                            is_all_inclusive_wildcard = SYS_FALSE;
                        }
                    }
                }

                if (is_all_inclusive_wildcard) break;

                printf("-");

                for (queried_node_id = 0;
                     queried_node_id < MAX_NUM_NODES;
                     queried_node_id++)
                {
                    if (all_remote_nodes_excluded[queried_node_id]) continue;

                    if (all_remote_nodes_included[queried_node_id])
                    {
                        if (needs_comma) printf(",");
                        printf("%d:.", queried_node_id);
                        needs_comma = SYS_TRUE;
                        continue;
                    }

                    for (remote_node_id = 0;
                         remote_node_id < MAX_NUM_NODES;
                         remote_node_id++)
                    {
                        if (matrix[queried_node_id][remote_node_id])
                        {
                            if (needs_comma) printf(",");
                            printf("%d:%d", queried_node_id, remote_node_id);
                            needs_comma = SYS_TRUE;
                        }
                    }
                }
                break;
            }
        default:
            printf("???");
        }

        /** DELETE THIS! DEBUG ! */
        {
            clink_node_id_t node_id;
            for (node_id = 0; node_id < MAX_NUM_NODES; node_id++)
            {
                printf(" %8.8x", eset_ptr->included[param][node_id]);
            }
        }

        printf("\n");
    }
}


mara_unix_exit_status_t
mara_handler_nmsget(mara_cmd_nmsget_t* cmdPtr)
{
    /* Note that this function doesn't actually use the C++ api. */
    clnk_ctx_t*                clnk_ctx;

    /* The specification of elements to fill the cache during even iterations */
    clnk_nms_eset_t            eset_0;

    /* The specification of elements to fill the cache during odd iterations */
    clnk_nms_eset_t            eset_1;

    /** This represents the cache an application keeps for serving requests */
    clnk_nms_cache_t           cache_master;

    SYS_UINT32 iteration;

    const SYS_UINT32  massive_timeout = 0xFFFFFFFF;

    mara_tmr_instance_t  timer;

    mara_tmr_start(&timer, massive_timeout);

    mara_show_exports = cmdPtr->ex;

    if (mara_show_exports) printf("export ERROR=0\n");

    if (cmdPtr->verbose) 
    {
        printf("verbose:  CLNK_NMS_MAX_NUMBER_ELEMENTS_IN_CACHE = %d\n",
                          CLNK_NMS_MAX_NUMBER_ELEMENTS_IN_CACHE);
        printf("verbose:  CLNK_NMS_CACHE_DWORDS = %d\n",
                          CLNK_NMS_CACHE_DWORDS);
        printf("verbose:  sizeof(clnk_nms_cache_t) = %d\n",
                         (SYS_UINT32) sizeof(clnk_nms_cache_t));
    }

    if (cmdPtr->verbose) printf("Opening driver...\n");
    if(clnk_init(NULL, &clnk_ctx))
    {
        printf("Could not open driver.\n");
        return -1;
    }

    if (cmdPtr->verbose) printf("Initializing NMS service...\n");
    clnk_nms_init(&mara_nms_context);

    if (cmdPtr->verbose) printf("Preparing Even Iteration Eset...\n");
    if (strlen(cmdPtr->e0) > 0)
    {
        SYS_BOOLEAN retval;
        clnk_nms_init_eset(&eset_0, CLNK_NMS_NAMED_ESET_EMPTY);
        if (cmdPtr->verbose) printf("Adding elements from esetspec '%s'\n", cmdPtr->e0); 
        retval = mara_nms_extract_eset(&eset_0, cmdPtr->e0);
        if (!retval)
        {
            mara_nms_context_unexpected(SYS_TRUE, __FILE__, __LINE__,
                                        "Bad esetspec decode",
                                        0, 0);
        }
    }
    else
    {
        if (cmdPtr->verbose) printf("Using named eset %d\n", cmdPtr->ne0);
        clnk_nms_init_eset(&eset_0, (enum clnk_nms_named_eset_t)cmdPtr->ne0);
    }

    if (cmdPtr->verbose) printf("Preparing Odd Iteration Eset...\n");
    if (strlen(cmdPtr->e1) > 0)
    {
        SYS_BOOLEAN retval;
        clnk_nms_init_eset(&eset_1, CLNK_NMS_NAMED_ESET_EMPTY);
        if (cmdPtr->verbose) printf("Adding elements from esetspec '%s'\n", cmdPtr->e1);
        retval = mara_nms_extract_eset(&eset_1, cmdPtr->e1);
        if (!retval)
        {
            mara_nms_context_unexpected(SYS_TRUE, __FILE__, __LINE__,
                                        "Bad esetspec decode",
                                        0, 0);
        }
    }
    else
    {
        if (cmdPtr->verbose) printf("Using named eset %d\n", cmdPtr->ne1);
        clnk_nms_init_eset(&eset_1, (enum clnk_nms_named_eset_t)cmdPtr->ne1);
    }

    if (!cmdPtr->quiet)
    {
        if (cmdPtr->verbose) printf("Displaying eset_0...\n");
        if (cmdPtr->verbose) mara_nms_show_eset(&eset_0);
        if (cmdPtr->i > 1)
        {
            if (cmdPtr->verbose) printf("Displaying eset_1...\n");
            if (cmdPtr->verbose) mara_nms_show_eset(&eset_1);
        }
    }

    if (cmdPtr->verbose) printf("Preparing master cache...\n");
    clnk_nms_init_cache(&cache_master);

    if (cmdPtr->s)
    {
        for (iteration = 0; iteration < cmdPtr->i; iteration++)
        {
            clink_nodemask_t  nw_nodemask;

            if (cmdPtr->verbose) printf("Starting loop iteration %d of %d on 1 cache model...\n", 
                   iteration, cmdPtr->i);

            mara_nms_show_stats(cmdPtr->ds);

            if (cmdPtr->verbose) printf("  Fill the master cache with new elements...\n");
            clnk_nms_fill_cache(clnk_ctx, &cache_master, 
                                (iteration&1) ? &eset_1 : &eset_0, 
                                cmdPtr->fillms,
                                cmdPtr->a ? mara_sleep_func : NULL,
                                &nw_nodemask);

            mara_nms_show_stats(cmdPtr->ds);

            if (cmdPtr->verbose) printf("  Showing Cache Contents:\n");
            mara_nms_show_cache_contents(&cache_master, cmdPtr->a);

            if (iteration + 1 < cmdPtr->i) mara_tmr_spin(cmdPtr->iterms);
        }
    }
    else
    {
        for (iteration = 0; iteration < cmdPtr->i; iteration++)
        {
            /** This represents the cache an application uses during a fill operation */
            clnk_nms_cache_t           cache_collection;
            clink_nodemask_t  nw_nodemask;

            if (cmdPtr->verbose) printf("Starting loop iteration %d of %d on 2 cache model...\n", 
                                        iteration, cmdPtr->i);

            if (cmdPtr->verbose) printf("  Initialize a collection cache...\n");
            clnk_nms_init_cache(&cache_collection);

            mara_nms_show_stats(cmdPtr->ds);

            if (cmdPtr->verbose) printf("  Prepopulate the collection cache with known "
                                        "good elements...\n");
            clnk_nms_copy_cache(&cache_master, &cache_collection);

            mara_nms_show_stats(cmdPtr->ds);

            if (cmdPtr->verbose) printf("  Fill the collection cache with new elements...\n");
            clnk_nms_fill_cache(clnk_ctx, &cache_collection,
                                (iteration&1) ? &eset_1 : &eset_0, 
                                cmdPtr->fillms, 
                                cmdPtr->a ? mara_sleep_func : NULL,
                                &nw_nodemask);

            mara_nms_show_stats(cmdPtr->ds);

            if (cmdPtr->verbose) printf("  When Application is ready, copy collection "
                                        "cache over master cache...\n");
            clnk_nms_copy_cache(&cache_collection, &cache_master);

            mara_nms_show_stats(cmdPtr->ds);

            if (cmdPtr->verbose) printf("  ... and finally release the collection cache...\n");
            clnk_nms_release_cache(&cache_collection);

            mara_nms_show_stats(cmdPtr->ds);

            if (cmdPtr->verbose) printf("  Showing Cache Contents:\n");
            mara_nms_show_cache_contents(&cache_master, cmdPtr->a);

            if (iteration + 1 < cmdPtr->i) mara_tmr_spin(cmdPtr->iterms);
        }
    }

    if (cmdPtr->verbose) printf("Iterations complete.\n");
    mara_nms_show_stats(cmdPtr->ds);

    if (cmdPtr->verbose) printf("Release master cache...\n");
    clnk_nms_release_cache(&cache_master);

    mara_nms_show_stats(cmdPtr->ds);
    if (cmdPtr->verbose) printf("Sequence complete.\n");

    /** Last check for memory pool correctness */
    {
        SYS_UINT32 poolname;
        for (poolname = 0; 
             poolname < CLNK_NMS_ELEMENT_POOL_NAME_MAX; 
             poolname++)
        {

            if (mara_nms_internal_allocation_counts[poolname])
            {
                mara_nms_context_unexpected(SYS_FALSE, __FILE__, __LINE__, 
                                            "memory allocation pool %d not all released.  Leak probable. %d %d",
                                            poolname,
                                            mara_nms_internal_allocation_counts[poolname]);
            }
        }
    }

    printf("Operation completing after %d ms\n", 
           massive_timeout - mara_tmr_msRemaining(&timer));

    if (mara_nms_unexpected_count)
    {
        printf("ERRORS SEEN! Saw %d\n", mara_nms_unexpected_count);
        clnk_destroy(clnk_ctx);
        return -1;
    }
    else
    {
        clnk_destroy(clnk_ctx);
        return 0;
    }
}


mara_unix_exit_status_t
mara_handler_nmspdef(mara_cmd_nmspdef_t* cmdPtr)
{
    clnk_nms_param_t  param;
    clnk_nms_named_eset_t eset_name = 
         (cmdPtr->ne < 0) ? CLNK_NMS_NAMED_ESET_ALL
                          : (clnk_nms_named_eset_t)cmdPtr->ne;
    clnk_nms_eset_t eset;

    clnk_nms_init_eset(&eset,eset_name);

    printf(" %-5s %-30s %-12s %-10s %-10s\n",
           "ID", "Name", "Cardinality", "Type", "AdmCnst" );
    for (param = 0; param < CLNK_NMS_PARAM__MAX; param++)
    {
        const clnk_nms_pdef_t* pdef_ptr = clnk_nms_get_pdef(param);

        if (!clnk_nms_eset_test_any(&eset, pdef_ptr, 
                                    CLINK_NODEMASK_ALL, 
                                    CLINK_NODEMASK_ALL)) continue;

        printf(" /%3.3d  %-30s %-12s %-10s %c\n", 
               pdef_ptr->param, 
               pdef_ptr->name_ptr,
               mara_nms_str_cardinality(pdef_ptr->cardinality),
               mara_nms_str_type(pdef_ptr->type),
               pdef_ptr->admission_constant ? 'Y' : 'N');
    }

    return 0;
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_nmsqreset(mara_cmd_nmsqreset_t* cmdPtr)
{
   /* Note that this function doesn't actually use the C++ api. */
    clnk_ctx_t*                clnk_ctx;
    clnk_nms_reset_types_t     forced_reset;

    printf("Opening driver...\n");
    if(clnk_init(NULL, &clnk_ctx))
    {
        printf("Could not open driver.\n");
        return -1;
    }

    printf("Initializing NMS service...\n");
    clnk_nms_init(&mara_nms_context);

    SYS_BOOLEAN do_reset = clnk_nms_get_hard_reset(clnk_ctx, &forced_reset);

    if (!do_reset && (forced_reset == ((SYS_UINT32)CLNK_NMS_HARD_RESET)))
    {
        printf(" YES!\n export NMSQRESET=%d\n",forced_reset);
        printf(" Rebooting...");
        system("reboot");
    }
    else
    {
        printf(" no\n export NMSQRESET=0\n");
    }

    clnk_destroy(clnk_ctx);
    return 0;
}


mara_unix_exit_status_t
mara_handler_nmsRESET(mara_cmd_nmsRESET_t* cmdPtr)
{
    SYS_UINT32      retval;
    clink_node_id_t node_id;
    SYS_BOOLEAN     is_first = SYS_TRUE;
    SYS_UINT32      index;
    clink_nodemask_t nodes_ack;
    clink_nodemask_t nodes_ack_overall;

    /* Note that this function doesn't actually use the C++ api. */
    clnk_ctx_t*                clnk_ctx;

    printf("Opening driver...\n");
    if(clnk_init(NULL, &clnk_ctx))
    {
        printf("Could not open driver.\n");
        return -1;
    }

    printf("Initializing NMS service...\n");
    clnk_nms_init(&mara_nms_context);

    printf(" Sending reset command to node ids: ");

    for (node_id = 0; node_id < MAX_NUM_NODES; node_id++)
    {
        if (!INCTYPES_NODE_IN_NODEMASK(node_id,cmdPtr->nm))
            continue;

        if (is_first)
        {
            is_first = SYS_FALSE;
        }
        else
        {
            printf(", ");
        }
        printf("%d", node_id);
    }

    if (is_first)
    {
        printf("NONE!\n");
    }
    printf("...\n");

    retval = clnk_nms_push_hard_reset(clnk_ctx, cmdPtr->nm);

    if (!retval)
    {
        printf(" clnk_nms_push_hard_reset Command issue fail!\n");
        clnk_destroy(clnk_ctx);
        return -1;
    }

    /*
     * Wait until all nodes ack or retry limit, whichever is first
     */
    nodes_ack_overall = 0;
    for (index = 0; index < NUM_RETRIES; index++)
    {
        /* sleep 1 sec */
        retval = usleep(1000000);     /* units of usec, returns unsigned */

        retval = clnk_nms_get_push_acknowledged(clnk_ctx, cmdPtr->nm, &nodes_ack);
        if (!retval)
        {
            printf(" clnk_nms_get_push_acknowledged command issue fail!\n");
        }
        else
        {
            nodes_ack_overall |= nodes_ack;
        }
        if ( nodes_ack_overall == cmdPtr->nm )
            break;
    }

    printf("Nodes acknowledging push: 0x%08X  : ", nodes_ack_overall);
    if( nodes_ack_overall == cmdPtr->nm )
    {
        printf("ALL NODES ACKed\n");
    }
    else
    {
        printf("THESE NODES DID NOT ACK 0x%08X\n", ~(cmdPtr->nm) ^ ~(nodes_ack_overall) );
    }

    clnk_destroy(clnk_ctx);
    /* Return 0 for success or 1 for fail */
    return (retval ? 0 : 1);
}


mara_unix_exit_status_t
mara_handler_nmsGETLOCAL(mara_cmd_nmsGETLOCAL_t* cmdPtr)
{
    SYS_UINT32                  retval;
    clnkdefs_nms_local_ccpu_t   nms_local_ccpu;
    clnkdefs_nms_local_daemon_t nms_local_daemon;
    clnk_nms_eview_t            eview;
    const clnk_nms_pdef_t*      pdef_ptr;
    ClnkDef_MyNodeInfo_t        my_node_info;
    clnk_nms_disposition_t      disposition = CLNK_NMS_DISPOSITION_FRESH;
    clink_node_id_t             remote_node_id;
    SYS_BOOLEAN                 show_all = cmdPtr->verbose;

    /* Note that this function doesn't actually use the C++ api. */
    clnk_ctx_t*                clnk_ctx;

    if (cmdPtr->verbose) printf("Opening driver...\n");
    if(clnk_init(NULL, &clnk_ctx))
    {
        printf("Could not open driver.\n");
        return -1;
    }

    if (cmdPtr->verbose) printf("Initializing NMS service...\n");
    clnk_nms_init(&mara_nms_context);

    retval = clnk_get_nms_local_ccpu(clnk_ctx, &nms_local_ccpu);
    if (retval)
    {
        printf(" Failure calling clnk_get_nms_local_ccpu\n");
        clnk_destroy(clnk_ctx);
        return -1;
    }

    retval = clnk_get_nms_local_daemon(clnk_ctx, &nms_local_daemon);
    if (retval)
    {
        printf(" Failure calling clnk_get_nms_local_daemon: %d\n", retval);
        clnk_destroy(clnk_ctx);
        return -1;
    }

    retval = clnk_get_my_node_info(clnk_ctx,&my_node_info,0);
    if (retval)
    {
        printf(" Failure calling clnk_get_my_node_info\n");
        clnk_destroy(clnk_ctx);
        return -1;
    }

    pdef_ptr = clnk_nms_get_pdef(CLNK_NMS_PARAM_NODE_ADM_REQ_RX);
    eview.s.uint32 = nms_local_daemon.node_adm_req_rx_ctr;
    mara_nms_show_param_header(pdef_ptr);
    mara_nms_show_element_contents(pdef_ptr, disposition, &eview, "(local)", 
                                   my_node_info.NodeId, 0, SYS_TRUE);

    pdef_ptr = clnk_nms_get_pdef(CLNK_NMS_PARAM_NODE_LINK_TIME);
    eview.s.uint32 = nms_local_ccpu.node_link_time;
    mara_nms_show_param_header(pdef_ptr);
    mara_nms_show_element_contents(pdef_ptr, disposition, &eview, "(local)", 
                                   my_node_info.NodeId, 0, SYS_TRUE);

    pdef_ptr = clnk_nms_get_pdef(CLNK_NMS_PARAM_NODE_LINK_TIME_BAD);
    eview.s.uint32 = nms_local_daemon.node_link_time_bad_deciseconds / 10;
    mara_nms_show_param_header(pdef_ptr);
    mara_nms_show_element_contents(pdef_ptr, disposition, &eview, "(local)", 
                                   my_node_info.NodeId, 0, SYS_TRUE);

    pdef_ptr = clnk_nms_get_pdef(CLNK_NMS_PARAM_NODE_LINK_UP_EVENT);
    eview.s.uint32 = nms_local_daemon.node_link_up_event_ctr;
    mara_nms_show_param_header(pdef_ptr);
    mara_nms_show_element_contents(pdef_ptr, disposition, &eview, "(local)", 
                                   my_node_info.NodeId, 0, SYS_TRUE);

    if (!clnk_get_lmo_advanced_ctr(clnk_ctx,&eview.s.uint32)) 
    {
        pdef_ptr = clnk_nms_get_pdef(CLNK_NMS_PARAM_LMO_ADVANCED_CTR);
        mara_nms_show_param_header(pdef_ptr);
        mara_nms_show_element_contents(pdef_ptr, disposition, &eview, "(local)", 
                                   my_node_info.NodeId, 0, SYS_TRUE);
    }

    pdef_ptr = clnk_nms_get_pdef(CLNK_NMS_PARAM_PHY_MTRANS_P2P_RX);

    mara_nms_show_param_header(pdef_ptr);
    for (remote_node_id = 0;
         remote_node_id < MAX_NUM_NODES;
         remote_node_id++)
    {
        /** Diagonals illegal! */
        if (remote_node_id == my_node_info.NodeId) continue;

        if (show_all || 
            INCTYPES_NODE_IN_NODEMASK(remote_node_id,my_node_info.NetworkNodeBitMask))
        {
            eview.s.uint32 = nms_local_daemon.
                               all_node_stats.
                                nodes[remote_node_id].
                                 mtrans_p2p_rx_count_good;
            mara_nms_show_element_contents(pdef_ptr, disposition, &eview, "(local)", 
                                           my_node_info.NodeId, remote_node_id, SYS_TRUE);
        }
    }

    pdef_ptr = clnk_nms_get_pdef(CLNK_NMS_PARAM_PHY_MTRANS_P2P_RX_ERRORS);
    mara_nms_show_param_header(pdef_ptr);
    for (remote_node_id = 0;
         remote_node_id < MAX_NUM_NODES;
         remote_node_id++)
    {
        /** Diagonals illegal! */
        if (remote_node_id == my_node_info.NodeId) continue;

        if (show_all || 
            INCTYPES_NODE_IN_NODEMASK(remote_node_id,my_node_info.NetworkNodeBitMask))
        {
            eview.s.uint32 = nms_local_daemon.
                               all_node_stats.
                                nodes[remote_node_id].
                                 mtrans_p2p_rx_count_error;
            mara_nms_show_element_contents(pdef_ptr, disposition, &eview, "(local)", 
                                           my_node_info.NodeId, remote_node_id, SYS_TRUE);
        }
    }

    pdef_ptr = clnk_nms_get_pdef(CLNK_NMS_PARAM_PHY_MTRANS_TX_PKT_CTR);
    eview.s.uint32 = nms_local_daemon.node_tx_pkt_ctr;
    mara_nms_show_param_header(pdef_ptr);
    mara_nms_show_element_contents(pdef_ptr, disposition, &eview, "(local)", my_node_info.NodeId, 0, SYS_TRUE);

    clnk_destroy(clnk_ctx);
    return 0;
}

/* PQoS config via L2ME messaging */
mara_unix_exit_status_t
mara_handler_pqosmode(mara_cmd_pqosmode_t* cmdPtr)
{
    SYS_UINT32      retval;
    clink_node_id_t node_id;
    SYS_BOOLEAN     is_first = SYS_TRUE;

    /* Note that this function doesn't actually use the C++ api. */
    clnk_ctx_t*     clnk_ctx;

    printf("Opening driver...\n");
    if(clnk_init(NULL, &clnk_ctx))
    {
        printf("Could not open driver.\n");
        return -1;
    }

    printf("Initializing NMS service...\n");
    clnk_nms_init(&mara_nms_context);

    printf("Sending pqos mode=%u to node ids: ", cmdPtr->mode);
    for (node_id = 0; node_id < MAX_NUM_NODES; node_id++)
    {
        if (!INCTYPES_NODE_IN_NODEMASK(node_id,cmdPtr->nm)) 
            continue;

        if (is_first)
        {
            is_first = SYS_FALSE;
        }
        else
        {
            printf(", ");
        }
        printf("%d", node_id);
    }

    if (is_first)
    {
        printf("NONE!\n");
    }
    printf("...\n");

    retval = clnk_nms_push_pqos_mode(clnk_ctx, 
                                     cmdPtr->nm, 
                                     (clnk_pqos_mode_t)cmdPtr->mode);

    if (!retval)
    {
        printf(" Command issue fail!\n");
        clnk_destroy(clnk_ctx);
        return -1;
    }

    clnk_destroy(clnk_ctx);
    return 0;
}

/* 
 * 
 * NMS push messages via L2ME messaging 
 *
 */

mara_unix_exit_status_t
mara_handler_ngetm(mara_cmd_ngetm_t* cmdPtr)
{
    SYS_UINT32 i;
    SYS_UINT32      retval = SYS_FALSE;
    clnk_nms_query_msg_local_t query_msg;

    /* Note that this function doesn't actually use the C++ api. */
    clnk_ctx_t*     clnk_ctx;

    printf("Opening driver...\n");
    if(clnk_init(NULL, &clnk_ctx))
    {
        printf("Could not open driver.\n");
        return -1;
    }

    printf("Vendor specific message: \n\n");
    retval = clnk_nms_query_message_local(clnk_ctx, &query_msg);

    printf("\t Sent by vendor:    0x%08X\n", query_msg.sent_by_vendor_id);
    printf("\t Sent by node:      0x%08X\n", query_msg.sent_by_node_id);
    printf("\t Sent by request #: 0x%08X\n", query_msg.sent_by_request_number);
    printf("\t Sent message:      ");
    for( i = 0; i < 6; i++ )
    {
        printf("%08X ", query_msg.message[i]);
    }
    printf("\n");

    if (!retval)
    {
        printf(" Command issue fail! %d\n", retval);
        clnk_destroy(clnk_ctx);
        return -1;
    }

    /* Return 0 for success or 1 for fail */
    clnk_destroy(clnk_ctx);
    return (retval ? 0 : 1);
}

mara_unix_exit_status_t
mara_handler_npushack(mara_cmd_npushack_t* cmdPtr)
{
    SYS_UINT32       retval = SYS_FALSE;    
    SYS_BOOLEAN      is_first = SYS_TRUE;
    clink_node_id_t  node_id;
    clink_nodemask_t nodes_ack;

    /* Note that this function doesn't actually use the C++ api. */
    clnk_ctx_t*     clnk_ctx;

    printf("Opening driver...\n");
    if(clnk_init(NULL, &clnk_ctx))
    {
        printf("Could not open driver.\n");
        return -1;
    }

    printf("Initializing NMS service...\n");
    clnk_nms_init(&mara_nms_context);

    printf("Sending ack request to node ids: ");
    for (node_id = 0; node_id < MAX_NUM_NODES; node_id++)
    {
        if (!INCTYPES_NODE_IN_NODEMASK(node_id,cmdPtr->nm)) 
            continue;

        if (is_first)
        {
            is_first = SYS_FALSE;
        }
        else
        {
            printf(", ");
        }
        printf("%d", node_id);
    }

    if (is_first)
    {
        printf("NONE!\n");
    }
    printf("...\n");

    retval = clnk_nms_get_push_acknowledged(clnk_ctx, cmdPtr->nm, &nodes_ack);

    if (!retval)
    {
        printf(" Command issue fail!\n");
        clnk_destroy(clnk_ctx);
        return -1;
    }

    printf("Nodes acknowledging last push: 0x%08X\n", nodes_ack);

    clnk_destroy(clnk_ctx);
    /* Return 0 for success or 1 for fail */
    return (retval ? 0 : 1);
}

/* NMS push message */
mara_unix_exit_status_t
mara_handler_npushm(mara_cmd_npushm_t* cmdPtr)
{
    SYS_UINT32      retval;
    clink_node_id_t node_id;
    SYS_BOOLEAN     is_first = SYS_TRUE;
    SYS_UINT32      dataList[6];
    SYS_UINT32      index;
    clink_nodemask_t nodes_ack;
    clink_nodemask_t nodes_ack_overall;

    /* Note that this function doesn't actually use the C++ api. */
    clnk_ctx_t*                clnk_ctx;

    printf("Opening driver...\n");
    if(clnk_init(NULL, &clnk_ctx))
    {
        printf("Could not open driver.\n");
        return -1;
    }

    printf("Initializing NMS service...\n");
    clnk_nms_init(&mara_nms_context);

    printf(" Sending vendor id=%u, datawords=%u %u %u %u %u %u to node ids: ",
           cmdPtr->vi, cmdPtr->d1, cmdPtr->d2, cmdPtr->d3, cmdPtr->d4, cmdPtr->d5, cmdPtr->d6);

    for (node_id = 0; node_id < MAX_NUM_NODES; node_id++)
    {
        if (!INCTYPES_NODE_IN_NODEMASK(node_id,cmdPtr->nm))
            continue;

        if (is_first)
        {
            is_first = SYS_FALSE;
        }
        else
        {
            printf(", ");
        }
        printf("%d", node_id);
    }

    if (is_first)
    {
        printf("NONE!\n");
    }
    printf("...\n");

    dataList[0] = cmdPtr->d1;
    dataList[1] = cmdPtr->d2;
    dataList[2] = cmdPtr->d3;
    dataList[3] = cmdPtr->d4;
    dataList[4] = cmdPtr->d5;
    dataList[5] = cmdPtr->d6;

    retval = clnk_nms_push_message(clnk_ctx,
                                   cmdPtr->vi,      // vendor id
                                   dataList,        // array of 6 data words
                                   cmdPtr->nm);     // nodemask
    if (!retval)
    {
        printf(" clnk_nms_push_message command issue fail!\n");
        clnk_destroy(clnk_ctx);
        return -1;
    }

    if (!cmdPtr->ms)
    {
        printf("  Push acknowledge checks skipped.\n");
        clnk_destroy(clnk_ctx);
        /* Return 0 for success or 1 for fail */
        return (retval ? 0 : 1);
    }

/*
 * Wait until all nodes ack or retry limit, whichever is first
 */
    nodes_ack_overall = 0;
    for (index = 0; index < NUM_RETRIES; index++)
    {
        /* sleep 1 sec */
        retval = usleep(1000 * cmdPtr->ms);  /* units of usec, returns unsigned */

        retval = clnk_nms_get_push_acknowledged(clnk_ctx, cmdPtr->nm, &nodes_ack);
        if (!retval)
        {
            printf(" clnk_nms_get_push_acknowledged command issue fail!\n");
        }
        else
        {
            nodes_ack_overall |= nodes_ack;
        }
        if ( nodes_ack_overall == cmdPtr->nm )
            break;
    }

    printf("Nodes acknowledging push: 0x%08X  : ", nodes_ack_overall);
    if( nodes_ack_overall == cmdPtr->nm )
    {
        printf("ALL NODES ACKed\n");
    }
    else
    {
        printf("THESE NODES DID NOT ACK 0x%08X\n", ~(cmdPtr->nm) ^ ~(nodes_ack_overall) );
    }

    clnk_destroy(clnk_ctx);
    /* Return 0 for success or 1 for fail */
    return (retval ? 0 : 1);
}


/* NMS push frequency settings */
mara_unix_exit_status_t
mara_handler_npushf(mara_cmd_npushf_t* cmdPtr)
{
    SYS_UINT32      retval;
    clink_node_id_t node_id;
    SYS_BOOLEAN     is_first = SYS_TRUE;
    SYS_UINT32      index;
    clink_nodemask_t nodes_ack;
    clink_nodemask_t nodes_ack_overall;

    /* Note that this function doesn't actually use the C++ api. */
    clnk_ctx_t*                clnk_ctx;

    printf("Opening driver...\n");
    if(clnk_init(NULL, &clnk_ctx))
    {
        printf("Could not open driver.\n");
        return -1;
    }

    printf("Initializing NMS service...\n");
    clnk_nms_init(&mara_nms_context);

    printf(" Sending last op freq=%u, prodmask=%u, chnmask=%u to node ids: ",
           cmdPtr->lof, cmdPtr->pm, cmdPtr->cm);

    for (node_id = 0; node_id < MAX_NUM_NODES; node_id++)
    {
        if (!INCTYPES_NODE_IN_NODEMASK(node_id,cmdPtr->nm))
            continue;

        if (is_first)
        {
            is_first = SYS_FALSE;
        }
        else
        {
            printf(", ");
        }
        printf("%d", node_id);
    }

    if (is_first)
    {
        printf("NONE!\n");
    }
    printf("...\n");

    retval = clnk_nms_push_frequency_settings(clnk_ctx,
                                              cmdPtr->lof, // last oper freq
                                              cmdPtr->pm,  // product mask
                                              cmdPtr->cm,  // channel mask
                                              cmdPtr->nm); // nodemask
    if (!retval)
    {
        printf(" clnk_nms_push_frequency_settings command issue fail!\n");
        clnk_destroy(clnk_ctx);
        return -1;
    }

    /*
     * Wait until all nodes ack or retry limit, whichever is first
     */
    nodes_ack_overall = 0;
    for (index = 0; index < NUM_RETRIES; index++)
    {
        /* sleep 1 sec */
        retval = usleep(1000000);     /* units of usec, returns unsigned */

        retval = clnk_nms_get_push_acknowledged(clnk_ctx, cmdPtr->nm, &nodes_ack);
        if (!retval)
        {
            printf(" clnk_nms_get_push_acknowledged command issue fail!\n");
        }
        else
        {
            nodes_ack_overall |= nodes_ack;
        }
        if ( nodes_ack_overall == cmdPtr->nm )
            break;
    }

    printf("Nodes acknowledging push: 0x%08X  : ", nodes_ack_overall);
    if( nodes_ack_overall == cmdPtr->nm )
    {
        printf("ALL NODES ACKed\n");
    }
    else
    {
        printf("THESE NODES DID NOT ACK 0x%08X\n", ~(cmdPtr->nm) ^ ~(nodes_ack_overall) );
    }

    clnk_destroy(clnk_ctx);
    /* Return 0 for success or 1 for fail */
    return (retval ? 0 : 1);
}


/* NMS push privacy password */
mara_unix_exit_status_t
mara_handler_npushp(mara_cmd_npushp_t* cmdPtr)
{
    SYS_UINT32      retval;
    clink_node_id_t node_id;
    SYS_BOOLEAN     is_first = SYS_TRUE;
    SYS_UINT32      index;
    clink_nodemask_t nodes_ack;
    clink_nodemask_t nodes_ack_overall;

    /* Note that this function doesn't actually use the C++ api. */
    clnk_ctx_t*                clnk_ctx;

    printf("Opening driver...\n");
    if(clnk_init(NULL, &clnk_ctx))
    {
        printf("Could not open driver.\n");
        return -1;
    }

    printf("Initializing NMS service...\n");
    clnk_nms_init(&mara_nms_context);

    printf(" Sending MOCA password=%s to node ids: ",
           cmdPtr->p);

    for (node_id = 0; node_id < MAX_NUM_NODES; node_id++)
    {
        if (!INCTYPES_NODE_IN_NODEMASK(node_id,cmdPtr->nm))
            continue;

        if (is_first)
        {
            is_first = SYS_FALSE;
        }
        else
        {
            printf(", ");
        }
        printf("%d", node_id);
    }

    if (is_first)
    {
        printf("NONE!\n");
    }
    printf("...\n");

    retval = clnk_nms_push_privacy_password(clnk_ctx,
                                            (SYS_UCHAR *)cmdPtr->p,     // MOCA password
                                            cmdPtr->nm);                // nodemask
    if (!retval)
    {
        printf(" clnk_nms_push_privacy_password command issue fail!\n");
        clnk_destroy(clnk_ctx);
        return -1;
    }

/*
 * Wait until all nodes ack or retry limit, whichever is first
 */
    nodes_ack_overall = 0;
    for (index = 0; index < NUM_RETRIES; index++)
    {
        /* sleep 1 sec */
        retval = usleep(1000000);     /* units of usec, returns unsigned */

        retval = clnk_nms_get_push_acknowledged(clnk_ctx, cmdPtr->nm, &nodes_ack);
        if (!retval)
        {
            printf(" clnk_nms_get_push_acknowledged command issue fail!\n");
        }
        else
        {
            nodes_ack_overall |= nodes_ack;
        }
        if ( nodes_ack_overall == cmdPtr->nm )
            break;
    }

    printf("Nodes acknowledging push: 0x%08X  : ", nodes_ack_overall);
    if( nodes_ack_overall == cmdPtr->nm )
    {
        printf("ALL NODES ACKed\n");
    }
    else
    {
        printf("THESE NODES DID NOT ACK 0x%08X\n", ~(cmdPtr->nm) ^ ~(nodes_ack_overall) );
    }

    clnk_destroy(clnk_ctx);
    /* Return 0 for success or 1 for fail */
    return (retval ? 0 : 1);
}


/* NMS push target phy rate */
mara_unix_exit_status_t
mara_handler_npusht(mara_cmd_npusht_t* cmdPtr)
{
    SYS_UINT32      retval;
    clink_node_id_t node_id;
    SYS_BOOLEAN     is_first = SYS_TRUE;
    SYS_UINT32      index;
    clink_nodemask_t nodes_ack;
    clink_nodemask_t nodes_ack_overall;

    /* Note that this function doesn't actually use the C++ api. */
    clnk_ctx_t*                clnk_ctx;

    printf("Opening driver...\n");
    if(clnk_init(NULL, &clnk_ctx))
    {
        printf("Could not open driver.\n");
        return -1;
    }

    printf("Initializing NMS service...\n");
    clnk_nms_init(&mara_nms_context);

    printf(" Sending target phy rate=%d to node ids: ", cmdPtr->tpr);

    for (node_id = 0; node_id < MAX_NUM_NODES; node_id++)
    {
        if (!INCTYPES_NODE_IN_NODEMASK(node_id,cmdPtr->nm))
            continue;

        if (is_first)
        {
            is_first = SYS_FALSE;
        }
        else
        {
            printf(", ");
        }
        printf("%d", node_id);
    }

    if (is_first)
    {
        printf("NONE!\n");
    }
    printf("...\n");

    retval = clnk_nms_push_target_phy_rate(clnk_ctx,
                                           cmdPtr->tpr,      // target phy rate in Mbps
                                           cmdPtr->nm);      // nodemask
    if (!retval)
    {
        printf(" clnk_nms_push_target_phy_rate command issue fail!\n");
        clnk_destroy(clnk_ctx);
        return -1;
    }

/*
 * Wait until all nodes ack or retry limit, whichever is first
 */
    nodes_ack_overall = 0;
    for (index = 0; index < NUM_RETRIES; index++)
    {
        /* sleep 1 sec */
        retval = usleep(1000000);     /* units of usec, returns unsigned */

        retval = clnk_nms_get_push_acknowledged(clnk_ctx, cmdPtr->nm, &nodes_ack);
        if (!retval)
        {
            printf(" clnk_nms_get_push_acknowledged command issue fail!\n");
        }
        else
        {
            nodes_ack_overall |= nodes_ack;
        }
        if ( nodes_ack_overall == cmdPtr->nm )
            break;
    }

    printf("Nodes acknowledging push: 0x%08X  : ", nodes_ack_overall);
    if( nodes_ack_overall == cmdPtr->nm )
    {
        printf("ALL NODES ACKed\n");
    }
    else
    {
        printf("THESE NODES DID NOT ACK 0x%08X\n", ~(cmdPtr->nm) ^ ~(nodes_ack_overall) );
    }

    clnk_destroy(clnk_ctx);
    /* Return 0 for success or 1 for fail */
    return (retval ? 0 : 1);
}


mara_unix_exit_status_t
mara_handler_gpqosmode(mara_cmd_gpqosmode_t* cmdPtr)
{
    SYS_UINT32          retval;
    clink_node_id_t     node_id;
    SYS_BOOLEAN         is_first = SYS_TRUE;
    clnk_pqos_mode_t    pqosmode_array[MAX_NUM_NODES];

    /* Note that this function doesn't actually use the C++ api. */
    clnk_ctx_t*                clnk_ctx;

    printf("Opening driver...\n");
    if(clnk_init(NULL, &clnk_ctx))
    {
        printf("Could not open driver.\n");
        return -1;
    }

    printf(" Retrieving pqosmode from node ids: ");
    for (node_id = 0;
         node_id < MAX_NUM_NODES;
         node_id++)
    {
        if (!INCTYPES_NODE_IN_NODEMASK(node_id,cmdPtr->nm)) continue;

        if (is_first)
        {
            is_first = SYS_FALSE;
        }
        else
        {
            printf(", ");
        }
        printf("%d", node_id);
    }

    if (is_first)
    {
        printf("NONE!\n");
    }
    printf("...\n");

    retval = clnk_nms_get_pqos_mode(clnk_ctx,
                                    cmdPtr->nm,
                                    pqosmode_array);

    if (!retval)
    {
        printf(" Command issue fail! retval %d\n", retval);
        clnk_destroy(clnk_ctx);
        return -1;
    }


    for (node_id = 0;
         node_id < MAX_NUM_NODES;
         node_id++)
    {
        if (!INCTYPES_NODE_IN_NODEMASK(node_id,cmdPtr->nm)) continue;

        printf("  Node %02d has pqosmode %d\n",
               node_id, pqosmode_array[node_id]);
    }

    clnk_destroy(clnk_ctx);
    return 0;
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_help(mara_cmd_help_t* cmdPtr)
{
    const mara_cmd_type_t* cmdTypePtr = mara_findCommandType(cmdPtr->cmd);

    if (!cmdTypePtr)
    {
        printf(MARA_PRE " No such cmd=%s\n", cmdPtr->cmd);
        return -1;
    }

    mara_printCommandHelp(cmdTypePtr);

    return 0;
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_handler_helpall(mara_cmd_helpall_t* cmdPtr)
{
    for (int cmdIndex = 0; cmdIndex < MARA_COMMAND__MAX; cmdIndex++)
    {
        mara_printCommandHelp(&mara_cmdTypes[cmdIndex]);
        printf("\n\n\n");
    }

    return 0;
}


/** ***************************************************************************
 */
mara_unix_exit_status_t
mara_dispatchCommand(mara_cmd_inst_t* cmdInstPtr)
{
    if (!cmdInstPtr || !cmdInstPtr->cmdTypePtr)
    {
        return -1;
    }

    int index = 0;

    switch (cmdInstPtr->cmdTypePtr->commandEnum)
    {
#define COMMAND_START(_token, _name, _defined)                                      \
                    case MARA_COMMAND_##_token:                                     \
                        {                                                           \
                            mara_cmd_##_token##_t cmd; 

#define PARAM_LONG(_token, _name, _unit, _defined, _type, _printf, _isRequired, _default)  \
                            cmd._token = (_type)cmdInstPtr->paramNumericValue[index++]; 

#define PARAM_STRING(_token, _name, _defined, _strlen, _default)                 \
                            cmd._token = cmdInstPtr->paramStringValue[index++]; 

#define PARAM_MAC(_token, _name, _defined, _required, _defaultHi, _defaultLo)    \
                            cmd._token.hi = cmdInstPtr->paramMacValue[index].hi; \
                            cmd._token.lo = cmdInstPtr->paramMacValue[index].lo; \
                            index++;

#define COMMAND_END(_token)                                                   \
                            return mara_handler_##_token(&cmd);               \
                        }                                                     \
                        break;
#include "mara.pp"
#undef COMMAND_START
#undef PARAM_LONG
#undef PARAM_STRING
#undef PARAM_MAC
#undef COMMAND_END
    default:
        return -1;
    }

    return 0;
}




static int list_interfaces(void)
{
    ClnkDef_ZipList_t zl[16];
    int n_dev, i;

    n_dev = clnk_list_devices(zl, 16);
    if(n_dev < 0)
    {
        printf("Error listing clink devices\n");
        mara_freeStringMemory();
        exit(1);
    }
    if(n_dev == 0)
    {
        printf("No clink devices found.\n");
        return(0);
    }
    for(i = 0; i < n_dev; i++)
        printf("%s\n", (char *)zl[i].ifname);
    return(0);
}




/** ***************************************************************************
 * A useful command line for figuring out preprocessor issues:
 *     gcc mara.cpp -E | sed 's/^#.*$//g' | grep -v ^$ > ppout.cpp  &&  gcc ppout.cpp
 */
int
main(int argc, char** argv)
{
    int retval = 0;
    int argsRemaining = argc - 1;
    char** nextArgPtr = &argv[1];    
    mara_cmd_inst_t     cmdInstPtr = {0};
    memset(g_StringBuffer,0,(sizeof(char *)*MARA_PARAM_COUNT_MAX));
    mara_mesh.iface = 0;

    /* if no args, print help and exit */
    if (argsRemaining == 0)
    {
        mara_listCommands();
        exit(0);
    }


    // check for -L option -- show list of available devices
    if( argsRemaining == 1 ) {
        if( strcmp( *nextArgPtr, "-L") == 0 ) {
            list_interfaces();
            exit(0);
        }
    }

    // check for -i iface option -- use iface to select device
    if( argsRemaining >= 2 ) {
        if( strcmp( *nextArgPtr, "-i") == 0 ) {
            argsRemaining--;
            nextArgPtr++;
            mara_mesh.iface = *nextArgPtr ;
            argsRemaining--;
            nextArgPtr++;
        }
    }

    /* if no args, print help and exit */
    if (argsRemaining == 0)
    {
        mara_listCommands();
        exit(0);
    }

    char* commandPtr = *nextArgPtr;
    argsRemaining--;
    nextArgPtr++;

    mara_parseCommand(&cmdInstPtr, commandPtr, argsRemaining, nextArgPtr);

    if (!cmdInstPtr.cmdTypePtr)
    {
        /* Someday we might add an instance specifier hunt here... not now */
        printf(MARA_PRE "'%s' unknown command.\n", commandPtr);
        mara_listCommands();
        mara_freeStringMemory();
        exit(2);
    }

    mara_echoCommand(&cmdInstPtr);

    mara_openMesh();
    const mara_unix_exit_status_t status = mara_dispatchCommand(&cmdInstPtr);

    if (status)
    {
        printf(MARA_PRE "FAIL with status %i.\n", status);
    }

    mara_mesh.CloseDvr();
    mara_freeStringMemory();
    exit(status);

    return retval;
}


/* End of File */
