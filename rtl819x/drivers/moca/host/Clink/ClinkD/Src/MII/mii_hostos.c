/**
 * \file mii_hostos.c
 * \brief HostOS_* functions for userland Linux
 */
/*******************************************************************************
* This file is licensed under the terms of your license agreement(s) with      *
* Entropic covering this file. Redistribution, except as permitted by the      *
* terms of your license agreement(s) with Entropic, is strictly prohibited.    *
*******************************************************************************/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <sys/time.h>
#include <HostOS_Common.h>
#include "mii_common.h"

void HostOS_Memset(void *pMem, int val, int size)
{
    memset(pMem, val, size);
}
void HostOS_Memcpy(void *pTo, void *pFrom, int size)
{
    memcpy(pTo, pFrom, size); 
}

void* HostOS_AllocDmaMem(void* pHostOSContext, int size, void** ppMemPa)
{
    *ppMemPa = (void *)0x1;     /* non-NULL means success */
    return(*ppMemPa);
}

void* HostOS_Alloc(int size)
{
    return(malloc(size));
}

void HostOS_Free(void* pMem, int size)
{
    free(pMem);
    return;
}

void HostOS_Sleep(int timeInUs)
{
    usleep(timeInUs);
    return;
}

void HostOS_GetTime(void *tv)
{
#ifdef AEI_WECB
    aei_gettimeofday((struct timeval *) tv, NULL);
#else
    gettimeofday((struct timeval *) tv, NULL);
#endif
}

void HostOS_PrintLog(SYS_UINT32 lev, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vlog_msg(lev, fmt, ap);
    va_end(ap);
}

void HostOS_WaitForDebugger(void *context)
{
    struct mii_iface *iface = context;
    char line[2];

    if(iface->wait_for_debugger)
    {
        printf("Attach SoC debugger and hit enter: ");
        fflush(stdin);

        /* temporarily re-enable default SIGINT/SIGTERM handling */
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        fgets(line, 2, stdin);
        sig_setup();

        printf("Now booting SoC...\n");
    }
}
