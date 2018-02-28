/******************************************************************************/
/*                              SCRAMNet GT                                   */
/******************************************************************************/
/*                                                                            */
/* Copyright (c) 2002-2005 Curtiss-Wright Controls.                           */
/*               support@systran.com 800-252-5601 (U.S. only) 937-252-5601    */
/*                                                                            */
/* This program is free software; you can redistribute it and/or              */
/* modify it under the terms of the GNU General Public License                */
/* as published by the Free Software Foundation; either version 2             */
/* of the License, or (at your option) any later version.                     */
/*                                                                            */
/* See the GNU General Public License for more details.                       */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/* This is a project fork for maintaining the driver for working with the      */
/* 3.10 kernel in Redhat/Centos 7.                                             */
/*                                                                             */
/* This fork is created/maintained by Brandon M. Graves, https://metashell.net */
/* up to date copies can be pulled from https://github.com/bmgraves/scgt       */
/*                                                                             */
/* It is important to note all credit to the original Developer, this is merely*/
/* an ongoing support project to test for functionality in future maintained   */
/* version of linux for legacy cards.                                          */
/*                                                                             */
/*******************************************************************************/

#define APP_VER_STRING "GT Monitor (gtmon) rev. 1.09 (2011/08/30)"
#define APP_VxW_NAME gtmon

/**************************************************************************/
/****** TAKING CARE OF ALL POSSIBLE OS TYPES - COPY-PASTE *****************/
/**************************************************************************/
/* control by PLATFORM_WIN, PLATFORM_UNIX, PLATFORM_VXWORKS */          /**/
#include <math.h>                                                       /**/
#include <stdio.h>                                                      /**/
#include <time.h>                                                       /**/
#include <string.h>                                                     /**/
#include <ctype.h>                                                      /**/
                                                                        /**/
#define PARSER                                                          /**/
#define MAIN_IN      int main(int argc, char **argv) {                  /**/
                                                                        /**/
#if defined PLATFORM_WIN || defined PLATFORM_RTX                        /**/
    #include <windows.h>                                                /**/
                                                                        /**/
#elif PLATFORM_VXWORKS                                                  /**/
    #undef PARSER                                                       /**/
    #undef MAIN_IN                                                      /**/
    #define VXL     40                                                  /**/
    #define MAIN_IN int APP_VxW_NAME(char *cmdLine) { \
            int argc; char argv0[]="gtmon"; char *argv[VXL]={argv0};    /**/
    #define PARSER     argc = parseCls(cmdLine, argv);                  /**/
                                                                        /**/
static int parseCls(char *cLine, char *argv[VXL])                       /**/
{                                                                       /**/
    char *token, *ptr = NULL;                                           /**/
    int numTokens = 1;                                                  /**/
    char seps[] = " ,\t\n";                                             /**/
    if(cLine == NULL) return numTokens; /* no command line given */     /**/
    printf(" \b"); token=(char*)strtok_r(cLine,seps,&ptr);              /**/
    while(token != NULL)                                                /**/
    { argv[numTokens] = token;                                          /**/
      numTokens++;                                                      /**/
      token = (char*)strtok_r(NULL, seps, &ptr);                        /**/
    }                                                                   /**/
    return numTokens;                                                   /**/
}                                                                       /**/
#endif                                                                  /**/
/************ END OF TAKING CARE OF ALL POSSIBLE OS TYPES *****************/

/**************************************************************************/
/************** APPLICATION-SPECIFIC CODE STARTS HERE *********************/
/**************************************************************************/

#include <stdlib.h>
#include "scgtapi.h"
#include "usys.h"

/**************************************************************************/
/**************************  D A T A  T Y P E S  **************************/
/**************************************************************************/

#define INPUT_OPTIONS \
    uint32  doDispAll, msPeriod, unit, helpLevel, doVerbose, doNetwork; \
    uint32  doStats, doSeconds, optionsFlags; \
    option  wlast, ewrap, rx, tx, px, laser, laser0, laser1, uint, bint; \
    option  sint, lint, linkup, iface, nodeid, reset, ringsize; \
    option  signal, signal0, signal1, spyid, bswap, wswap, d64, readbypass;

    /*
     * note: couldn't use "interface" in INPUT_OPTIONS in MSVC for unknown
     *       reasons, so used "iface" instead
     */

typedef struct
{
    uint32 value;
    uint32 flags;
} option;

typedef struct
{
    int32 initialized;
    int32 numStats;
    int32 maxNameLen[2];
    uint32 *valueArray;
    uint32 *prevValueArray;
    char   **nameArray;
    char   *buf;
} statsType;

typedef struct {
    uint32            timeToExit;
    uint32            running;
    usysThreadParams  pt;
} exitThreadParms;

typedef struct
{
    INPUT_OPTIONS
    uint32 lastLatencyTmr, lastNetTmr,   lastTrafficCnt;
    uint32 lastIntrTrfcCnt, lastQIntrTrfcCnt, lastErrorCnt, lastLinkErrs;
    uint32 lastSpyTrafficCnt, iteration;
    uint32 boardInfoReg;
    double  lastNetTput, lastSpyTput, ticksPerSec, nanoPerTick;
    uint32 linkSpeed, pciSpeed;
    int32 lastIntrTrfcPerSec, lastQIntrTrfcPerSec;
    statsType stats;
    scgtDeviceInfo devInfo;
    scgtHandle hDriver;
    scgtHandle* phDriver;
    exitThreadParms exitParms;
} iOptionsType;

typedef struct
{
    uint32 nodeId,      upstreamId,   ringSize,      spyId,     d64;
    uint32 linkUp,      activeLink,   ewrap,         writeLast;
    uint32 laser0En,    laser0SigDet, laser1En,      laser1SigDet;
    uint32 rxEn,        txEn,         rtEn,          selfIntEn;
    uint32 linkErrs,    netTimer,     latTimer,      brdcastIntMask;
    uint32 intrTrfcCnt, qIntrTrfcCnt, unicastIntEn,  spyTrafficCnt;
    uint32 trafficCnt,  byteSwap,     wordSwap,      readbypass;
    uint32 linkErrInt;
} gtStatus;

/**************************************************************************/
/******************  F U N C T I O N  P R O T O T Y P E S  ****************/
/**************************************************************************/

static void gtmonConfigMode(iOptionsType *iO);
static int gtmonDefaultMode(iOptionsType *iO);

static uint32 gtmonDiffCntr(uint32 currCnt, uint32 lastCnt, uint32 maxCnt);
static uint32 gtmonGetSetOp(iOptionsType *iO, option *pOption,
                            uint32 stateCode, char *desc, char *printFormatStr);

static int parseCommand(int argc, char **argv, iOptionsType *iO);
static void printHelpLevel1(iOptionsType *iO);
static void printHelpLevel2(void);
static void printHelpLevel3(void);
static void gtmonPrintVerboseInfo(iOptionsType *iO);
static void gtmonPrintInfo(iOptionsType *iO);
static void gtmonPrintShortInfo(iOptionsType *iO);
static void gtmonPrintAllBoards(iOptionsType *iO);
static void gtmonPrintNetwork(iOptionsType *iO);
static void gtmonPrintStats(iOptionsType *iO);
static void gtmonGetStateInfo(iOptionsType *iO, gtStatus *st);
static int verifyMyLUT(uint32 nm, gtStatus *st );
static void *getRuntimeChar(void *voidp);

/**************************************************************************/
/***********************  D E F I N E S & M A C R O S  ********************/
/**************************************************************************/

#define NET_TICK_PER_SEC_1G    53125000.0  /*  53.125 MHz oscillator */
#define NET_TICK_PER_SEC_2G   106250000.0  /* 106.250 MHz oscillator */
#define NET_TICK_PER_SEC_25G  125000000.0  /* 125.000 MHz oscillator */

#define SCGT_MAX_DEVICES  16

#define OPT_FLAG           0x1
#define OPT_VALUE          0x2
#define OPT_ONOFF          0x4

/* macros for decoding the NS_LUT network management entries */
#define MAX_NODES             256
#define NM_VALID              0x80000000
#define NM_NODEID(x)         ((x) & 0xFF)
#define NM_UPSTREAMID(x)     (((x) & 0xFF00)>>8)
#define NM_LASER_0_EN(x)     (((x) & 0x10000)>>16)
#define NM_LASER_1_EN(x)     (((x) & 0x20000)>>17)
#define NM_LINK_SEL(x)       (((x) & 0x40000)>>18)
#define NM_RLPOP(x)          (((x) & 0x80000)>>19)
#define NM_TXEN(x)           (((x) & 0x100000)>>20)
#define NM_RXEN(x)           (((x) & 0x200000)>>21)
#define NM_RTEN(x)           (((x) & 0x400000)>>22)
#define NM_WML(x)            (((x) & 0x800000)>>23)
#define NM_LNK_UP(x)         (((x) & 0x1000000)>>24)
#define NM_LAS_0_SD(x)       (((x) & 0x2000000)>>25)
#define NM_LAS_1_SD(x)       (((x) & 0x4000000)>>26)
#define NM_DATA_VALID(x)     (((x) & NM_VALID)>>31)

/* some register offsets */
#define REG_NET_TMR        0x40  /* network timer (network tick counter) */
#define REG_HOST_TMR       0x44  /* host timer (PCI tick counter) */
#define REG_INTR_TRAFFIC   0x50  /* network interrupts, foreign, native */
#define REG_INTR_Q_TRAFFIC 0x58  /* queued network interrupts */

/**************************************************************************/
/************************* G T M O N   C O D E ****************************/
/**************************************************************************/

MAIN_IN

    iOptionsType iO;
    int i;
    usysMsTimeType curTime;
    uint32 curSecond;

    PARSER;        /* prepare argc and agrv if not provided by OS (VxWorks) */

    memset(&iO, 0, sizeof(iOptionsType));              /* zero all options */
    i = parseCommand(argc, argv, &iO);                  /* parsing options */

    if(iO.helpLevel!=0) /*1, 2, 3, ...*/
    {
        printHelpLevel1(&iO);
        return i;
    }

    iO.phDriver = &iO.hDriver; /* must be before "doDispAll", or any open */

    if (iO.doDispAll)
    {
        gtmonPrintAllBoards(&iO);
        return 0;
    }

    if (scgtOpen(iO.unit, &iO.hDriver) != SCGT_SUCCESS)
    {
        printf("gtmon: could not open unit %i\n", iO.unit);
        return -1;
    }

    iO.exitParms.timeToExit = iO.exitParms.running = 0;

    if( iO.msPeriod || iO.doSeconds )
    {                                              /* spawn exit thread */
        iO.exitParms.pt.parameter = &iO.exitParms;
        iO.exitParms.pt.priority = UPRIO_HIGH; /* only use UPRIO_HIGH */
        sprintf(iO.exitParms.pt.threadName, "gtmonExit");
        usysCreateThread(&iO.exitParms.pt, getRuntimeChar);
    }

    if( iO.msPeriod && !iO.doSeconds )
    {
        iO.doSeconds = -1;
    }

    if( iO.doSeconds && !iO.msPeriod )
    {
        iO.msPeriod = 500;
    }

    usysMsTimeStart(&curTime);
    iO.iteration = 0;

    do{
        if( iO.optionsFlags )
            gtmonConfigMode(&iO);
        else
            gtmonDefaultMode(&iO);

        curSecond = (usysMsTimeGetElapsed(&curTime) / 1000);

        if( iO.doSeconds && (iO.doSeconds != -1) &&
            ((curSecond + (iO.msPeriod/1000)) >= iO.doSeconds) )
        {
            iO.exitParms.timeToExit = 1;
        }

        if( iO.msPeriod )
        {   /* avoid long delays for quicker 'q' response */
            for(i=0; 
                (i<(int)(iO.msPeriod/2000)) && !iO.exitParms.timeToExit;
                i++)
            {
                usysMsTimeDelay( 2000 );
            }

            if( (iO.msPeriod % 2000) && !iO.exitParms.timeToExit)
            {
                usysMsTimeDelay( iO.msPeriod % 2000 );
            }
        }

        iO.iteration++;

    }while((iO.msPeriod || iO.doSeconds) && !iO.exitParms.timeToExit);

    usysMsTimeStop(&curTime);

    if( iO.stats.initialized )
        usysMemFree(iO.stats.buf);

    if( iO.exitParms.running ) /* For timed exit, a VxWorks "must-have" */
        usysKillThread(&iO.exitParms.pt);

    scgtClose(iO.phDriver);
    return 0;
}

/***************************************************************************/
/* Function:    gtmonGetSetOp()                                            */
/* Description: Get the value of, or set the value of a driver state       */
/***************************************************************************/
static uint32 gtmonGetSetOp(iOptionsType *iO, option *pOption,
                            uint32 stateCode, char *desc, char *printFormatStr)
{
    uint32 value = 0;

    if( iO->doVerbose )      /* only print description if verboseness is on */
        printf("%s = ", desc);

    if( pOption->flags & OPT_VALUE )              /* value given, so set it */
        scgtSetState(iO->phDriver, stateCode, pOption->value);

    if( (pOption->flags & OPT_FLAG) || iO->doVerbose) /* inquiry or verbose "set" */
    {
        value = scgtGetState(iO->phDriver, stateCode);

        if( printFormatStr )
            printf(printFormatStr, value);
        else
            printf("%i\n", value);
    }

    return 0;
}

/***************************************************************************/
/* Function:    gtmonConfigMode()                                          */
/* Description: Performs ops that may occur only once (!periodic mode)     */
/***************************************************************************/

static void gtmonConfigMode(iOptionsType *iO)
{
    if ( iO->wlast.flags )
        gtmonGetSetOp(iO, &iO->wlast, SCGT_WRITE_ME_LAST, "wlast", NULL);
    if ( iO->ewrap.flags )
        gtmonGetSetOp(iO, &iO->ewrap, SCGT_EWRAP, "ewrap", NULL);
    if ( iO->tx.flags )
        gtmonGetSetOp(iO, &iO->tx, SCGT_TRANSMIT_ENABLE, "tx", NULL);
    if ( iO->rx.flags )
        gtmonGetSetOp(iO, &iO->rx, SCGT_RECEIVE_ENABLE, "rx", NULL);
    if ( iO->px.flags )
        gtmonGetSetOp(iO, &iO->px, SCGT_RETRANSMIT_ENABLE, "px", NULL);
    if ( iO->laser.flags )
    {
        option *pOption = (scgtGetState(iO->phDriver, SCGT_ACTIVE_LINK) ?
                            &iO->laser1 : &iO->laser0);
        pOption->flags |= iO->laser.flags;

        if ( pOption->flags & OPT_VALUE )
            pOption->value |= iO->laser.value;
    }
    if ( iO->laser0.flags )
        gtmonGetSetOp(iO, &iO->laser0, SCGT_LASER_0_ENABLE, "laser0", NULL);
    if ( iO->laser1.flags )
        gtmonGetSetOp(iO, &iO->laser1, SCGT_LASER_1_ENABLE, "laser1", NULL);
    if ( iO->signal.flags ) /* yes signal, not signalF */
    {
       if( scgtGetState(iO->phDriver, SCGT_ACTIVE_LINK) )
           iO->signal1.flags |= iO->signal.flags;
       else
           iO->signal0.flags |= iO->signal.flags;
    }
    if ( iO->signal0.flags )
        gtmonGetSetOp(iO, &iO->signal0, SCGT_LASER_0_SIGNAL_DET, "signal0", NULL);
    if ( iO->signal1.flags )
        gtmonGetSetOp(iO, &iO->signal1, SCGT_LASER_1_SIGNAL_DET, "signal1", NULL);
    if ( iO->nodeid.flags )
        gtmonGetSetOp(iO, &iO->nodeid, SCGT_NODE_ID, "nodeid", NULL);
    if ( iO->iface.flags )
        gtmonGetSetOp(iO, &iO->iface, SCGT_ACTIVE_LINK, "interface", NULL);
    if ( iO->linkup.flags )
        gtmonGetSetOp(iO, &iO->linkup, SCGT_LINK_UP, "linkup", NULL);
    if ( iO->ringsize.flags )
        gtmonGetSetOp(iO, &iO->ringsize, SCGT_RING_SIZE, "ringsize", NULL);
    if ( iO->d64.flags )
        gtmonGetSetOp(iO, &iO->d64, SCGT_D64_ENABLE, "d64", NULL);
    if ( iO->bint.flags )
        gtmonGetSetOp(iO, &iO->bint, SCGT_BROADCAST_INT_MASK, "bint", "%#x\n");
    if ( iO->uint.flags )
        gtmonGetSetOp(iO, &iO->uint, SCGT_UNICAST_INT_MASK, "uint", NULL);
    if ( iO->sint.flags )
        gtmonGetSetOp(iO, &iO->sint, SCGT_INT_SELF_ENABLE, "sint", NULL);
    if ( iO->spyid.flags )
        gtmonGetSetOp(iO, &iO->spyid, SCGT_SPY_NODE_ID, "spyid", NULL);
    if ( iO->bswap.flags )
        gtmonGetSetOp(iO, &iO->bswap, SCGT_BYTE_SWAP_ENABLE, "bswap", NULL);
    if ( iO->wswap.flags )
        gtmonGetSetOp(iO, &iO->wswap, SCGT_WORD_SWAP_ENABLE, "wswap", NULL);
    if ( iO->lint.flags )
        gtmonGetSetOp(iO, &iO->lint, SCGT_LINK_ERR_INT_ENABLE, "lint", NULL);
    if (iO->readbypass.flags)
        gtmonGetSetOp(iO, &iO->readbypass, SCGT_READ_BYPASS_ENABLE, "readbypass", NULL);
    if (iO->reset.flags)
        scgtWriteCR(iO->phDriver, 0x4, (scgtReadCR(iO->phDriver, 0x4) | 0x1000));
}

/***************************************************************************/
/* Function:    gtmonDefaultMode()                                         */
/* Description: Call functions applicable to periodic (-p) mode operation  */
/***************************************************************************/
static int gtmonDefaultMode(iOptionsType *iO)
{
    if( iO->doNetwork )
        gtmonPrintNetwork(iO);
    else if( iO->doStats )
        gtmonPrintStats(iO);
    else if( iO->doVerbose )
        gtmonPrintVerboseInfo(iO);
    else
        gtmonPrintInfo(iO);

    return 0;
}

/***************************************************************************/
/* Function:    gtmonSelectChar()                                          */
/* Description: Retreive char encodings for link, sig detect, laser states */
/***************************************************************************/
#define CH_LINKUP    'U'
#define CH_LINKDOWN  'D'
#define CH_SIGDET    '+'
#define CH_NOSIGDET  '-'
#define CH_LASERON   'A'
#define CH_LASEROFF  'O'

static void gtmonSelectChar(gtStatus *st, uint32 iface,
                            char *sigDet, char *link, char *laser)
{
    uint32 sig = ( iface ? st->laser1SigDet : st->laser0SigDet );
    uint32 las = ( iface ? st->laser1En : st->laser0En );

    *link = (st->linkUp ? CH_LINKUP : CH_LINKDOWN);
    *sigDet = (sig ? CH_SIGDET : CH_NOSIGDET);
    *laser = (las ? CH_LASERON : CH_LASEROFF);

    if( st->activeLink != iface )
    {
        *laser = tolower(*laser);
        *sigDet = tolower(*sigDet);
        *link = '.';
    }
}

/***************************************************************************/
/* Function:    gtmonNMSelectChar()                                        */
/* Description: Retreive char encodings for link, sig detect, laser states */
/***************************************************************************/
static void gtmonNMSelectChar(uint32 nm, uint32 iface,
                              char *sigDet, char *link, char *laser)
{
    uint32 sig = ( iface ? NM_LAS_1_SD(nm) : NM_LAS_0_SD(nm) );
    uint32 las = ( iface ? NM_LASER_1_EN(nm) : NM_LASER_0_EN(nm) );

    *link = (NM_LNK_UP(nm) ? CH_LINKUP : CH_LINKDOWN);
    *sigDet = (sig ? CH_SIGDET : CH_NOSIGDET);
    *laser = (las ? CH_LASERON : CH_LASEROFF);

    if( NM_LINK_SEL(nm) != iface )
    {
        *laser = tolower(*laser);
        *sigDet = tolower(*sigDet);
        *link = '.';
    }
}

/***************************************************************************/
/* Function:    getSpeedInfo()                                             */
/* Description: Calculate info related to linkSpeed and PCI frequency      */
/***************************************************************************/
static void getSpeedInfo(iOptionsType *iO)
{
    usysMsTimeType curTime;
    uint32 oscTicks[2], pciTicks[2];
    uint32 startMS1, startMS2, stopMS1, stopMS2, elapsedMS;
    uint32 val;

    usysMsTimeStart(&curTime);
    usysMsTimeDelay(1);

    /* read counters */
    startMS1 = usysMsTimeGetElapsed(&curTime);
    oscTicks[0] = scgtReadCR(iO->phDriver, REG_NET_TMR);
    pciTicks[0] = scgtReadCR(iO->phDriver, REG_HOST_TMR);
    startMS2 = usysMsTimeGetElapsed(&curTime);

    usysMsTimeDelay(80); /* may need to raise if too inacurrate */
    /* Don't go lower than 80. MS Windows tested inacurrate at 50. */

    /* read counters */
    stopMS1 = usysMsTimeGetElapsed(&curTime);
    oscTicks[1] = scgtReadCR(iO->phDriver, REG_NET_TMR);
    pciTicks[1] = scgtReadCR(iO->phDriver, REG_HOST_TMR);
    stopMS2 = usysMsTimeGetElapsed(&curTime);

    /* try for accurate elapsed time */
    elapsedMS = (stopMS2 + stopMS1)/2 - (startMS2 + startMS1)/2;

    if( elapsedMS == 0 ) /* prevent divide-by-zero */
        elapsedMS = 1;

    usysMsTimeStop(&curTime);

    oscTicks[0] = gtmonDiffCntr(oscTicks[1], oscTicks[0], 0xFFFFFFFF);
    pciTicks[0] = gtmonDiffCntr(pciTicks[1], pciTicks[0], 0xFFFFFFFF);

    /* calculate link speed related info */
    oscTicks[0] *= 20;
    val = (oscTicks[0]/elapsedMS)/1000; /* Mbps */

    /* ~+-200 Mbps from actual, short delay used for calculation atop system
      scheduling yields inaccurate results*/

    if( (val >= 860) && (val < 1260) )
    {
        iO->linkSpeed = 1000; /* 1062.5 Mbps actual */
        iO->ticksPerSec = NET_TICK_PER_SEC_1G;
    }
    else if( (val > 1925) && (val < 2300) )
    {
        iO->linkSpeed = 2000; /* 2125 Mbps actual */
        iO->ticksPerSec = NET_TICK_PER_SEC_2G;
    }
    else if( (val >= 2300) && (val < 2700) )
    {
        iO->linkSpeed = 2500; /* 2500 Mbps actual */
        iO->ticksPerSec = NET_TICK_PER_SEC_25G;
    }
    else
    {
        iO->linkSpeed = 0; /* Error */
        iO->ticksPerSec = 1; /* would mean 1 Hz clock, aka. error */
        /* cannot use 0, or other calcs would cause divide-by-zero errors */
    }

    iO->nanoPerTick = (1000000000.0 / iO->ticksPerSec );

    /* calculate PCI bus frequency */
    val = (pciTicks[0]/elapsedMS)/1000; /* MHz */

    if( (val > 23) && (val < 43) )
        iO->pciSpeed = 33; /* MHz */
    else if( (val > 56 ) && (val < 76) )
        iO->pciSpeed = 66; /* MHz */
    else
        iO->pciSpeed = 0; /* MHz, error */
}

/***************************************************************************/
/* Function:    gtmonPrintVerboseInfo()                                    */
/* Description: Displays verbose info for a board in the system            */
/***************************************************************************/
static void gtmonPrintVerboseInfo(iOptionsType *iO)
{
    gtStatus st;
    char linkChar, sigChar, laserChar;
    uint32 diffCnt = 0, diffTmr = 0, bir, val;
    int32 ival;
    double dval;
    char* str;

    /* first pass, store constant info */
    if( (iO->iteration == 0) || (iO->devInfo.popMemSize == 0) ||
        (iO->linkSpeed == 0))
    {
        iO->devInfo.popMemSize = 0;

        if (scgtGetDeviceInfo(iO->phDriver, &iO->devInfo))
        {
            printf("scgtGetDeviceInfo failed, unit %d\n", iO->unit);
            return;
        }

        iO->boardInfoReg = scgtReadCR(iO->phDriver, 0);

        /* get PCI and link speed */
        getSpeedInfo(iO);
    }

    bir = iO->boardInfoReg;
    gtmonGetStateInfo(iO, &st);

    /* all status calls are made before printing to limit display jerk 
       during periodic updates */
       
    printf("\n");
    printf("%s\n", APP_VER_STRING);

    val = (bir & 0x6000) >> 13;
    printf("%s ", ((val==0x0) ? "GT100" : ((val==0x1) ? "GT200" : "GT???")));

    val = (bir & 0x1C00) >> 10;

    if( val == 0x0 )
        str = "33MHz 64bit 5V";
    else if( val == 0x1 )
        str = "33MHz 64bit 3.3V";
    else if( val == 0x2 )
        str = "66MHz 64bit 3.3V";
    else
        str = "unknown";

    printf("unit %i %s - %s PCI compatible\n", iO->devInfo.unitNum,
            iO->devInfo.boardLocationStr, str );

    val = (bir & 0xC0) >> 6;

    if( val == 0x0 )
        str = "ZBT SRAM";
    else if( val == 0x1 )
        str = "DDR SDRAM";
    else
        str = "unknown";

    printf("Memory %iMB(PIO)/%iMB(POP) %s - Link %d.%1d Gbps - PCI %d MHz\n",
            iO->devInfo.mappedMemSize/1048576,
            iO->devInfo.popMemSize /1048576, str,
            iO->linkSpeed/1000, (iO->linkSpeed/100)%10, iO->pciSpeed);

    printf("Driver %s - API %s - Firmware %X.%.2X\n",
            iO->devInfo.driverRevisionStr, scgtGetApiRevStr(),
            iO->devInfo.revisionID >> 8, iO->devInfo.revisionID & 0xFF);

    printf(
"===========================================================================");

    /* Interface 0 printout */
    gtmonSelectChar(&st, 0, &sigChar, &linkChar, &laserChar);

    printf("\nInterface 0 %s: ",(!st.activeLink?"(selected)" : "          "));

    printf("Signal %s (%c) - ", (st.laser0SigDet ?
           "Detected" : "Missing "), sigChar);

    printf("Laser %s(%c)", (st.laser0En ? "On  " : "Off "), laserChar);

    if(st.activeLink == 0)
        printf(" - Link %s (%c)", (st.linkUp ? "Up" : "DOWN!!"), linkChar);

    /* Interface 1 printout */
    printf("\nInterface 1 %s: ",(st.activeLink ? "(selected)" : "          "));

    gtmonSelectChar(&st, 1, &sigChar, &linkChar, &laserChar);
    printf("Signal %s (%c) - ", (st.laser1SigDet ?
           "Detected" : "Missing "), sigChar);

    printf("Laser %s(%c)", (st.laser1En ? "On  " : "Off "), laserChar);;

    if(st.activeLink == 1)
        printf(" - Link %s (%c)", (st.linkUp ? "Up" : "DOWN!!"), linkChar);

    printf("\nNode ID      (nID): %-3i         Receive     (R): %i %s",
            st.nodeId, st.rxEn, (st.rxEn ? "(on) " : "(off)") );
    printf("    --------------\n");
    printf("Upstream nID (uID): %-3i         Retransmit  (P): %i %s",
            st.upstreamId, st.rtEn, (st.rtEn ? "(on) " : "(off)") );
    printf("    | %3i of %-3i |\n", st.nodeId, st.ringSize);
    printf("Ring Size     (RS): %-3i         Transmit    (T): %i %s",
            st.ringSize, st.txEn, (st.txEn ? "(on) " : "(off)") );
    printf("    --------------\n");
    printf("Unicast Interrupts: %i %s     Write Last  (W): %i %s\n",
            st.unicastIntEn, (st.unicastIntEn?"(on) ":"(off)"),
            st.writeLast, (st.writeLast ? "(on) " : "(off)"));

    printf("Self Interrupts   : %i %s     ",
            st.selfIntEn, (st.selfIntEn ? "(on) " : "(off)"));

    printf("E-Wrap      (E): %d %s",st.ewrap, (st.ewrap ? "(on) " : "(off)"));
    printf("  Byte Swap: %d %s\n", st.byteSwap, (st.byteSwap ? "(on) " : "(off)"));

    printf("Broadcast Int Mask: 0x%.8x  ", st.brdcastIntMask);

    printf("PCI 64bit data : %d %s",st.d64, (st.d64 ? "(on) " : "(off)"));
    printf("  Word Swap: %d %s\n", st.wordSwap, (st.wordSwap ? "(on) " : "(off)"));


    #if 0
    printf("LinkErr Interrupts: %i %s     ",
            st.linkErrInt, (st.linkErrInt ? "(on) " : "(off)"));
    printf("Read Bypass    : %d %s\n",st.readbypass, (st.readbypass ? "(on) " : "(off)"));
    #endif


    printf("\nData Count (Bytes): %-11.0f", (double)st.trafficCnt * 4.0);

    if (iO->iteration)
    {
        diffCnt = gtmonDiffCntr(st.trafficCnt,iO->lastTrafficCnt,0xFFFFFFFF);
        diffTmr = gtmonDiffCntr(st.netTimer, iO->lastNetTmr, 0xFFFFFFFF);
        printf(" (%+11.0f)", (double)diffCnt * 4.0);

        if ( diffTmr )                             /* avoids divide-by-zero */
        {                                                 /* compute MB/sec */
            dval = (diffCnt/(double)250000) / (diffTmr/(double)iO->ticksPerSec);
            printf("     MB/s: %-6.2f", dval);
            if(iO->iteration > 1)
                printf(" (%+6.2f)", (dval - iO->lastNetTput));
            iO->lastNetTput = dval;
        }
    }

    printf("\nSpy Count   (%c%3d): %-11.0f",
            (st.spyId == st.nodeId ? ' ' : '*'), st.spyId,
            (double)st.spyTrafficCnt * 4.0);

    if (iO->iteration)
    {
        diffCnt = gtmonDiffCntr(st.spyTrafficCnt,iO->lastSpyTrafficCnt,
                                   0xFFFFFFFF);
        /* diffTmr calculated above */
        printf(" (%+11.0f)", (double)diffCnt * 4.0);

        if ( diffTmr )                             /* avoids divide-by-zero */
        {                                                 /* compute MB/sec */
            dval = (diffCnt/(double)250000) / (diffTmr/(double)iO->ticksPerSec);
            printf("     MB/s: %-6.2f", dval);
            if(iO->iteration > 1)
                printf(" (%+6.2f)", (dval - iO->lastSpyTput));
            iO->lastSpyTput = dval;
        }
    }

    printf("\nLatency     (nsec): %-11.0f",(double)st.latTimer*iO->nanoPerTick);

    if (iO->iteration)
    {
        ival = (int32)(st.latTimer - iO->lastLatencyTmr );
        dval = ( ((double)ival * iO->nanoPerTick) /
                 ((double)iO->msPeriod * 1000000.0)) * 1000000.0;
        printf(" (%+11.0f)      ppm: %+.0f", (double)ival*iO->nanoPerTick,dval);
    }

    printf("\nNetwork Int (nInt): %-11u", st.intrTrfcCnt);

    if(iO->iteration)
    {
        diffCnt = gtmonDiffCntr(st.intrTrfcCnt,iO->lastIntrTrfcCnt,0xFFFFFFFF);
        printf(" (%+11i)", diffCnt);

        if ( diffTmr )                             /* avoids divide-by-zero */
        {                                                 /* compute MB/sec */
            ival = (int32)( ( (double)diffCnt /
                              (diffTmr/(double)iO->ticksPerSec) ) + 0.5);
            printf("   nInt/s: %-6d", ival);
            if(iO->iteration > 1)
                printf(" (%+6d)", (ival - iO->lastIntrTrfcPerSec));
            iO->lastIntrTrfcPerSec = ival;
        }
    }

    printf("\nQueued nInt (qInt): %-11u", st.qIntrTrfcCnt);

    if(iO->iteration)
    {
        diffCnt = gtmonDiffCntr(st.qIntrTrfcCnt,iO->lastQIntrTrfcCnt,0xFFFFFFFF);
        printf(" (%+11i)", diffCnt);

        if ( diffTmr )                             /* avoids divide-by-zero */
        {                                                 /* compute MB/sec */
            ival = (int32)( ( (double)diffCnt /
                              (diffTmr/(double)iO->ticksPerSec) ) + 0.5);
            printf("   qInt/s: %-6d", ival);
            if(iO->iteration > 1)
                printf(" (%+6d)", (ival - iO->lastQIntrTrfcPerSec));
            iO->lastQIntrTrfcPerSec = ival;
        }
    }

    printf("\nLink Errors (Errs): %-11u", st.linkErrs);
    if(iO->iteration)
    {
        printf(" (%+11i)",
               gtmonDiffCntr(st.linkErrs, iO->lastLinkErrs, 0xFFFFFFFF));
    }

    printf("\n");
    fflush(stdout);

    iO->lastLinkErrs = st.linkErrs;
    iO->lastLatencyTmr = st.latTimer;
    iO->lastTrafficCnt = st.trafficCnt;
    iO->lastNetTmr = st.netTimer;
    iO->lastIntrTrfcCnt = st.intrTrfcCnt;
    iO->lastQIntrTrfcCnt = st.qIntrTrfcCnt;
    iO->lastSpyTrafficCnt = st.spyTrafficCnt;
}

/***************************************************************************/
/* Function:    printCommonLocalInfo()                                     */
/* Description: Prints status info in common format                        */
/***************************************************************************/
static void printCommonLocalInfo(gtStatus *st)
{
    /* prints the following info:     "nID<uID  I0  I1 RPT W" */
    char linkChar, sigChar, laserChar;

    gtmonSelectChar(st, 0, &sigChar, &linkChar, &laserChar);
                                    /* print IDs and interface 0 information */
    printf("%3i<%-3i %c%c%c ", st->nodeId, st->upstreamId,
                               sigChar, linkChar, laserChar);

    gtmonSelectChar(st, 1, &sigChar, &linkChar, &laserChar);
    printf("%c%c%c ", sigChar, linkChar, laserChar);
                                                      /* routing switch info */
    printf("%i%i%i %i", st->rxEn, st->rtEn, st->txEn, st->writeLast );
}

/***************************************************************************/
/* Function:    gtmonPrintInfo()                                           */
/* Description: Displays verbose encoded info for a board in the system    */
/***************************************************************************/
static void gtmonPrintInfo(iOptionsType *iO)
{
    int32 netTmr, errCnt, nintCnt;
    uint32 trafficCnt;
    double throughPut;
    gtStatus st;

    gtmonGetStateInfo(iO, &st);

    if ((iO->iteration % 10) == 0)
    {
        printf("gtmon: unit: %i\n", iO->unit);
        printf("  MB/s    nsec       Bytes       nInt"
               "       Errs  RS nID<uID  I0  I1 RPT W E\n");
    }

    trafficCnt =gtmonDiffCntr(st.trafficCnt, iO->lastTrafficCnt, 0xFFFFFFFF);
    netTmr = gtmonDiffCntr(st.netTimer, iO->lastNetTmr, 0xFFFFFFFF);
    errCnt = gtmonDiffCntr(st.linkErrs, iO->lastErrorCnt, 0xFFFFFFFF);
    nintCnt = gtmonDiffCntr(st.intrTrfcCnt, iO->lastIntrTrfcCnt, 0xFFFFFFFF);

    iO->lastTrafficCnt = st.trafficCnt;
    iO->lastNetTmr = st.netTimer;
    iO->lastErrorCnt = st.linkErrs;
    iO->lastIntrTrfcCnt = st.intrTrfcCnt;

    if ( !iO->iteration || !netTmr || !iO->linkSpeed) /*avoid divide-by-zero*/
    {
        printf("     - ");
        getSpeedInfo(iO);
        /* getting speed info takes time, so must re-get some values */
        iO->lastNetTmr = scgtGetState(iO->phDriver, SCGT_NET_TIMER_VAL);
        iO->lastTrafficCnt = scgtGetState(iO->phDriver, SCGT_SM_TRAFFIC_CNT);
    }
    else
    {
        throughPut = (trafficCnt * 4) / (netTmr / iO->ticksPerSec);
        printf("%6.2f ", throughPut/1000000.0);
    }

    printf("%7.0f %11.0f %10u %10u %3i ",(double)(st.latTimer*iO->nanoPerTick),
          ((double)trafficCnt * 4.0), nintCnt, errCnt, st.ringSize);

    printCommonLocalInfo(&st);

    printf(" %c\n", (st.ewrap ? '1' : ' '));                /* EWRAP info */

    fflush(stdout);
}

/***************************************************************************/
/* Function:    gtmonPrintStats()                                          */
/* Description: Displays driver statistics information                     */
/***************************************************************************/
static void gtmonPrintStats(iOptionsType *iO)
{
    char *statNames, *begin, *end = NULL;
    char *buf;
    uint32 *temp, *newVals, *oldVals;
    uint32 sizes[2];
    int i, len, num, firstPass = 0;
    char modChar = '*', flagChar;

    /* Only retrieve stat names, etc on first iteration. They won't change
       over time.  This lightens the load on the driver during periodic
       mode operation.  Stat values must be retrieved every time. */

    if( !iO->stats.initialized ) /* retrieve names etc on first iteration */
    {
        firstPass = 1;

        /* get the number of stats and size of names string */
        scgtGetStats(iO->phDriver, sizes, NULL, 0, 2);

        num = sizes[0];
        i = (sizeof(uint32) * num * 2) + (sizeof(char*) * num) + sizes[1];

        /* allocate space for the statistics value-array and name-string */
        if ((buf = usysMemMalloc(i)) == NULL)
        {
            printf("Memory allocation error\n");
            return;
        }

        iO->stats.initialized    = 1;
        iO->stats.numStats       = num;
        iO->stats.buf            = buf;
        iO->stats.valueArray     = (uint32*)buf;
        iO->stats.prevValueArray = (uint32*)&(buf[sizeof(uint32) * num]);
        iO->stats.nameArray      = (char**)&(buf[sizeof(uint32) * num * 2]);
        statNames = &(buf[(sizeof(uint32) * num * 2) + (sizeof(char*) * num)]);

        /* retrieve stat names as one string of comma-delimited tokens */
        scgtGetStats(iO->phDriver, NULL, statNames, 0, num);

        iO->stats.maxNameLen[0] = iO->stats.maxNameLen[1] = sizeof("Statistic");
        len = 0;

        /* tokenize stat-name-string and store token pointers for later */
        for (i = 0, begin = statNames; i < iO->stats.numStats; i++, begin = end)
        {
            int col = i % 2;

            if( begin && (end = strchr(begin, ',')) )  /* find end of token */
            {
                *end = '\0';
                end++;
            }

            if ( begin )
                len = (int) strlen(begin);

            if( len > iO->stats.maxNameLen[col] )
                iO->stats.maxNameLen[col] = len;

            iO->stats.nameArray[i] = begin;
        }
    }

    temp = iO->stats.valueArray;
    newVals = iO->stats.valueArray = iO->stats.prevValueArray;
    oldVals = iO->stats.prevValueArray = temp;

    /* retrieve statistics values */
    scgtGetStats(iO->phDriver, newVals, NULL, 0, iO->stats.numStats);

    if( firstPass )
    {
        modChar = ' ';              /* prevent flagging value as "changed" */
        oldVals[0] = ~(newVals[0]); /* force compare failure, so we print */
    }

    if( iO->msPeriod )
    {
        printf("Running(%i): gtmon -S -u %d  \r", iO->iteration, iO->unit);
        fflush(stdout);
    }

    if( memcmp(newVals, oldVals, (iO->stats.numStats * sizeof(uint32))) )
    {
        int col;

         /* print stats, marking changes with a "*" */
        if( iO->msPeriod )
        {
            printf("\n====================< gtmon -S -u %d >"
                   "======================\n", iO->unit);
        }

        for (i = 0, col = i%2; i < iO->stats.numStats; i++, col = i%2)
        {
            flagChar = (newVals[i] != oldVals[i]) ? modChar : ' ';
 
            printf("%-*s : %c%-14u%s", iO->stats.maxNameLen[col], 
                   (iO->stats.nameArray[i] ? iO->stats.nameArray[i] : "NULL"),
                   flagChar, newVals[i], (col ? "\n" : ""));
        }

        if( col )
        {
            printf("\n");
        }

        fflush(stdout);
    }

    return;
}

/***************************************************************************/
/* Function:    verifyMyLUT()                                              */
/* Description: Verifies that an NS_LUT entry with "my node ID" matches    */
/*              this device's status. Used to detect node ID collisions.   */
/***************************************************************************/
static int verifyMyLUT(uint32 nm, gtStatus *st )
{
    return ( (NM_UPSTREAMID(nm) != st->upstreamId) ||
             (NM_LASER_0_EN(nm) != st->laser0En) ||
             (NM_LASER_1_EN(nm) != st->laser1En) ||
             (NM_LINK_SEL(nm) != st->activeLink) ||
//             (NM_RLPOP(nm) != (st->numLinks - 1)) ||
             (NM_TXEN(nm) != st->txEn) ||
             (NM_RXEN(nm) != st->rxEn) ||
             (NM_RTEN(nm) != st->rtEn) ||
             (NM_WML(nm) != st->writeLast) ||
             (NM_LNK_UP(nm) != st->linkUp) ||
             (NM_LAS_0_SD(nm) != st->laser0SigDet) ||
             (NM_LAS_1_SD(nm) != st->laser1SigDet) );
}

/***************************************************************************/
/* Function:    gtmonPrintNetwork()                                        */
/* Description: Displays the network layout and each node's status info    */
/***************************************************************************/
static void gtmonPrintNetwork(iOptionsType *iO)
{
    int i, count = 0, duplicates = 0, foundMe = 0, skipped = 0;
    char linkChar, sigChar, laserChar;
    uint32 linkUp = 0, nmi = 0;
    uint32 nm[MAX_NODES];
    int32 nID[MAX_NODES + 2];
    int32 collision = 0, myNidCollision = 0;
    gtStatus st;

    gtmonGetStateInfo(iO, &st);

    if( st.linkUp )
    {
        /*
         * We only use this info if the link is up.  Leave all the register
         * read/write code before any printf calls.  This code takes "alot" of
         * time, and cause display to flash during periodic mode updates.
         */
        for(i = 0, count = 0; i < MAX_NODES; i++) /* clear the NS_LUT */
        {
            scgtWriteNMR(iO->phDriver, (i * 4), 0);
        }
    
        usysMsTimeDelay(20);        /* allow time for NS_LUT to be refilled */
    
        /*
         * Must read all registers, in case multiple nodes have the same id.
         * Consider the following setup:
         *
         *     thisNode <- nodeid 3 <- nodeid 3 <- nodeid 4 <- thisNode
         *
         * NS_LUT registers 0 and 2 would be valid, but 1 would not due to
         * the conflicting node IDs
         */
    
        for(i=0; i < MAX_NODES+2; nID[i]=-1, i++); /* set nodeIDs to invalid */

        for(i = 0, count = 0; i < MAX_NODES; i++)        /* read the NS_LUT */
        {
            nm[i] = scgtReadNMR(iO->phDriver, (i * 4));

            if(!NM_DATA_VALID(nm[i]))
            {
                skipped++;
            }
            else
            {
                count += (1 + skipped);
                skipped = 0;
                nID[i] = NM_NODEID(nm[i]);
                /* fill in upstream nodeID in case next reg is invalid */
                if( NM_LNK_UP(nm[i]) )
                {
                    nID[i+1] = NM_UPSTREAMID(nm[i]);
                }
            }
        }

        /*
         * At this point there are at least "count" nodes in the ring, possibly
         * more if there are nodes with "my node ID" immediately downstream.
         */
    }

    if( iO->msPeriod )
    {
        printf("====================================="
               "======================================\n");
    }
    printf("gtmon: unit: %i\n", iO->unit);
    printf("HOPS nID<uID  I0  I1 RPT W  NOTES\n");
    printf("  -0 ");
    printCommonLocalInfo(&st);
    printf("  Monitor node, Driver info");

    if( !st.linkUp )
    {
        printf(", LINK DOWN!!!\n");
        goto summary;
    }
    printf("\n");

    for(i = 0, skipped = 0; i < count; i++)
    {
        collision = 0;
        nmi = nm[i];

        if(!NM_DATA_VALID(nmi))         /* do collision detection/resolution */
        {              /* nID[i] may have been taken from downstream at init */
            if( ( nID[i] != -1 ) || ( nID[i+1] != -1 ) || (i==(count-1)) )
            {
                printf("%4i ", -(i+1));
                if( nID[i] != -1 )
                    printf("%3i", nID[i]);
                else
                    printf("???");

                if( nID[i+1] != -1 )
                    printf("<%-3i ", nID[i+1] );
                else
                    printf("<??? ");

                printf("??? ??? ??? ?  **ID collision");

                if( skipped )
                   printf("s for HOPS %d to %d", -(i+1-skipped),-(i+1));

                if( i != (count-1) )
                    printf(", Resolved ID");

                printf("\n");
            }

            skipped++;
        }
        else
        {                                  /* detect collision on my nodeId */
            linkUp = NM_LNK_UP(nmi);

            if ( st.nodeId == NM_NODEID(nmi) )
            {
                if( (i < (count-1)) || !linkUp || verifyMyLUT(nmi, &st) )
                    collision = myNidCollision = 1;
                else
                    foundMe = 1;
            }

            printf("%4i %3i<", -(i+1), nID[i]);               /* print info */

            if( linkUp )
                printf("%-3i ", NM_UPSTREAMID(nmi));
            else
                printf("??? ");
                                           /* print interface 0 information */
            gtmonNMSelectChar(nmi, 0, &sigChar, &linkChar, &laserChar);
            printf("%c%c%c ", sigChar, linkChar, laserChar);

            gtmonNMSelectChar(nmi, 1, &sigChar, &linkChar, &laserChar);
            printf("%c%c%c ", sigChar, linkChar, laserChar);
                                               /* print status information */
            printf("%i%i%i %i  ", NM_RXEN(nmi), NM_RTEN(nmi),
                                  NM_TXEN(nmi), NM_WML(nmi));

            if(collision)
                printf("**ID collision%s", (!linkUp ? ", " : ""));
            if(foundMe)
                printf("Monitor node, Network info, Ring closed ");
            if(!linkUp)
                printf("Link down, Chain terminated");
            else if( !(NM_RTEN(nmi)) )
                printf("! PX off");

            printf("\n");
            duplicates += skipped + collision;
            skipped = 0;

            /*
             * There may be at least one more node above "count". However, if
             * my info was found, then this would re-accounts for my upstream
             * node due to wrapping past the end of a completed chain. If
             * retransmit enable is off, the data flow chain is terminated.
             */
            if( (i==(count-1)) && !foundMe && (nID[i+1] != -1) && NM_RTEN(nmi))
            {
                count++; /* extra resolved node at end of chain */
            }
        }
    }

    duplicates += skipped;
                                              /* print whole topology list */
summary:
    printf("SUMMARY:\n");
    printf("_ %d nodes connected\n", count + 1 - foundMe);
    printf("_ %d nodes reporting\n", (count - duplicates + myNidCollision));
    printf("_ %d node ID duplications\n", duplicates);
    printf("_ Connections: %3i", st.nodeId);

    for( i = 0, skipped = 0; i < count; i++)
    {
        if( nID[i] != -1 )
        {
            if(skipped)
                printf("<?x%d?", skipped);
            printf("<%i", nID[i]);
            skipped = 0;
        }
        else
        {
            skipped++;
        }
    }

    nmi = nm[count-1];

    if(skipped)
        printf("<?x%d?  Chain/Ring undetermined", skipped);
    else if(foundMe)
        printf("  Ring closed");
    else if( (count==0) || (NM_DATA_VALID(nmi) && !NM_LNK_UP(nmi)) )
        printf("<END  Chain terminated");
    else if( NM_DATA_VALID(nmi) && !NM_RTEN(nmi) )
        printf("  Chain terminated, PX path off");
    else
        printf("<???  Chain/Ring undetermined");

    printf("\n");
    fflush(stdout);
}

/***************************************************************************/
/* Function:    gtmonGetStateInfo()                                        */
/* Description: Retrieves driver state info                                */
/***************************************************************************/
static void gtmonGetStateInfo(iOptionsType *iO, gtStatus *st)
{
    st->nodeId       = scgtGetState(iO->phDriver, SCGT_NODE_ID);
    st->upstreamId   = scgtGetState(iO->phDriver, SCGT_UPSTREAM_NODE_ID);
    st->ringSize     = scgtGetState(iO->phDriver, SCGT_RING_SIZE);
    st->spyId        = scgtGetState(iO->phDriver, SCGT_SPY_NODE_ID);
    st->d64          = scgtGetState(iO->phDriver, SCGT_D64_ENABLE);
    st->linkUp       = scgtGetState(iO->phDriver, SCGT_LINK_UP);
    st->activeLink   = scgtGetState(iO->phDriver, SCGT_ACTIVE_LINK);
    st->ewrap        = scgtGetState(iO->phDriver, SCGT_EWRAP);
    st->writeLast    = scgtGetState(iO->phDriver, SCGT_WRITE_ME_LAST);
    st->laser0En     = scgtGetState(iO->phDriver, SCGT_LASER_0_ENABLE);
    st->laser0SigDet = scgtGetState(iO->phDriver, SCGT_LASER_0_SIGNAL_DET);
    st->laser1En     = scgtGetState(iO->phDriver, SCGT_LASER_1_ENABLE);
    st->laser1SigDet = scgtGetState(iO->phDriver, SCGT_LASER_1_SIGNAL_DET);
    st->rxEn         = scgtGetState(iO->phDriver, SCGT_RECEIVE_ENABLE);
    st->txEn         = scgtGetState(iO->phDriver, SCGT_TRANSMIT_ENABLE);
    st->rtEn         = scgtGetState(iO->phDriver, SCGT_RETRANSMIT_ENABLE);
    st->selfIntEn    = scgtGetState(iO->phDriver, SCGT_INT_SELF_ENABLE);
    st->linkErrs     = scgtGetState(iO->phDriver, SCGT_NUM_LINK_ERRS);
    st->netTimer     = scgtGetState(iO->phDriver, SCGT_NET_TIMER_VAL);
    st->latTimer     = scgtGetState(iO->phDriver, SCGT_LATENCY_TIMER_VAL);
    st->brdcastIntMask = scgtGetState(iO->phDriver, SCGT_BROADCAST_INT_MASK);
    st->intrTrfcCnt  = scgtReadCR(iO->phDriver, REG_INTR_TRAFFIC);
    st->qIntrTrfcCnt = scgtReadCR(iO->phDriver, REG_INTR_Q_TRAFFIC);
    st->unicastIntEn = scgtGetState(iO->phDriver, SCGT_UNICAST_INT_MASK);
    st->spyTrafficCnt = scgtGetState(iO->phDriver, SCGT_SPY_SM_TRAFFIC_CNT);
    st->trafficCnt   = scgtGetState(iO->phDriver, SCGT_SM_TRAFFIC_CNT);
    st->byteSwap     = scgtGetState(iO->phDriver, SCGT_BYTE_SWAP_ENABLE);
    st->wordSwap     = scgtGetState(iO->phDriver, SCGT_WORD_SWAP_ENABLE);
    st->linkErrInt   = scgtGetState(iO->phDriver, SCGT_LINK_ERR_INT_ENABLE);
    st->readbypass   = scgtGetState(iO->phDriver, SCGT_READ_BYPASS_ENABLE);
}

/***************************************************************************/
/* Function:    gtmonDiffCntr()                                            */
/* Description: Calculates difference between current counter value and    */
/*    previous counter value, accounting for (at most 1) roll-over.        */
/***************************************************************************/
static uint32 gtmonDiffCntr(uint32 currCnt, uint32 lastCnt, uint32 maxCnt)
{
    if( currCnt >= lastCnt)
        return ( currCnt - lastCnt );
    else /* overflow */
        return ( currCnt + ( maxCnt - lastCnt ) + 1 );
}

/***************************************************************************/
/* Function:    gtmonPrintShortInfo()                                      */
/* Description: Displays condensed info for a board in the system          */
/***************************************************************************/
static void gtmonPrintShortInfo(iOptionsType *iO)
{
    scgtDeviceInfo devInfo;

    if (scgtGetDeviceInfo(iO->phDriver, &devInfo))
    {
        printf("scgtGetDeviceInfo failed, unit %d\n", iO->unit);
        return;
    }
    printf("Unit: %i (%s)   ", devInfo.unitNum, devInfo.boardLocationStr);
    printf("FW: %X.%.2X   PIO: %iMB   NodeID: %i   Link %s\n",
        devInfo.revisionID >> 8, devInfo.revisionID & 0xFF,
        devInfo.mappedMemSize/1048576,
        scgtGetState(iO->phDriver, SCGT_NODE_ID),
        (scgtGetState(iO->phDriver, SCGT_LINK_UP) ? "Up" : "DOWN"));
}

/***************************************************************************/
/* Function:    gtmonPrintAllBoard()                                       */
/* Description: Displays info for all boards in the system                 */
/***************************************************************************/
static void gtmonPrintAllBoards(iOptionsType *iO)
{
    int i;

    for (i = 0; i < SCGT_MAX_DEVICES; i++)
    {
        if (scgtOpen(i, iO->phDriver) != SCGT_SUCCESS)
            continue;

        iO->unit = i;

        if(iO->doNetwork)
            gtmonPrintNetwork(iO);
        else if (iO->doStats)
            gtmonPrintStats(iO);
        else if(iO->doVerbose > 1)
            gtmonPrintVerboseInfo(iO);
        else if(iO->doVerbose == 1)
            gtmonPrintInfo(iO);
        else
            gtmonPrintShortInfo(iO);

        scgtClose(iO->phDriver);

        if( iO->stats.initialized && iO->stats.buf)
        {
            usysMemFree(iO->stats.buf);
            iO->stats.initialized = 0;
            iO->stats.buf = NULL;
        }
    }
}

/***************************************************************************/
/* Function:    getRuntimeChar                                             */
/* Description: Thread that reads keyboard                                 */
/***************************************************************************/
static void *getRuntimeChar(void *voidp)
{
    exitThreadParms *pparms = (exitThreadParms*) voidp;
    char ch;
    int ich;
    pparms->running = 1;
    while (!pparms->timeToExit)
    {
        if ((ich = getchar()) == EOF)
            break;
            
        ch = toupper(ich); /* toupper may be a macro */
        if (ch == 'Q')
        {   pparms->running = 0;
            pparms->timeToExit = 1;
        }
    }
    return voidp;
}

/***************************************************************************/
/* Function:    parseCommand()                                             */
/* Description: Parses command line options                                */
/***************************************************************************/
static int parseCommand(int argc,char **argv,iOptionsType *iO)
{
    char *endptr, nullchar = 0;
    int  i, j, len, tookParam=0;                         /* setting defaults */
                                                    /* setup default options */
    iO->helpLevel = 1;       

    if(argc == 1)                                       /* start processing */
        return 0;

    for(i = 1; i < argc; i++)
    {
        if( (argv[i][0] != '-') || (argv[i][1] == '\0') )
        {
            printf("\nInvalid option: \"%s\"\n\n", argv[i]);
            return -1;
        }

        if( argv[i][1] == '-' )                        /* parse a -- option */
        {
            option *pOption = NULL;
            char *opt = &(argv[i][2]);
            unsigned int equalVal=0, intType=0;
            unsigned int type = (OPT_VALUE|OPT_ONOFF|OPT_FLAG); /* default to all types */
            #define COMPARE_N(s)  strncmp(opt, s, (len=(sizeof(s))-1))

            if( COMPARE_N("linkup") == 0 )
            {   pOption = &iO->linkup; type = OPT_FLAG; }
            else if( COMPARE_N("nodeid") == 0 )
            {   pOption = &iO->nodeid;  type = (OPT_VALUE|OPT_FLAG); }
            else if( COMPARE_N("rx") == 0 )
                pOption = &iO->rx;
            else if( COMPARE_N("px") == 0 )
                pOption = &iO->px;
            else if( COMPARE_N("tx") == 0 )
                pOption = &iO->tx;
            else if( COMPARE_N("wlast") == 0 )
                pOption = &iO->wlast;
            else if( COMPARE_N("ewrap") == 0 )
                pOption = &iO->ewrap;
            else if( COMPARE_N("laser0") == 0 )
                pOption = &iO->laser0;
            else if( COMPARE_N("laser1") == 0 )
                pOption = &iO->laser1;
            /* "laser" must follow "laser0" and "laser1" */
            else if( COMPARE_N("laser") == 0 )
                pOption = &iO->laser;
            else if( COMPARE_N("sint") == 0 )
                pOption = &iO->sint;
            else if( COMPARE_N("uint") == 0 )
                pOption = &iO->uint;
            else if( COMPARE_N("bint") == 0 )
            {   pOption = &iO->bint; type = (OPT_VALUE|OPT_FLAG); }
            else if( COMPARE_N("d64") == 0 )
                pOption = &iO->d64;
            else if( COMPARE_N("bswap") == 0 )
                pOption = &iO->bswap;
            else if( COMPARE_N("wswap") == 0 )
                pOption = &iO->wswap;
            else if( COMPARE_N("interface") == 0 )
            {   pOption = &iO->iface; type = (OPT_VALUE|OPT_FLAG); }
            else if( COMPARE_N("ringsize") == 0 )
            {   pOption = &iO->ringsize; type = OPT_FLAG; }
            else if( COMPARE_N("spyid") == 0 )
            {   pOption = &iO->spyid; type = (OPT_VALUE|OPT_FLAG); }
            else if( COMPARE_N("signal0") == 0 )
            {   pOption = &iO->signal0; type = OPT_FLAG; }
            else if( COMPARE_N("signal1") == 0 )
            {   pOption = &iO->signal1; type = OPT_FLAG; }
            /* "signal" must follow "signal0" and "signal1" */
            else if( COMPARE_N("signal") == 0 )
            {   pOption = &iO->signal; type = OPT_FLAG; }
            else if( COMPARE_N("lint") == 0 )
                pOption = &iO->lint;
            else if( COMPARE_N("readbypass") == 0 )
                pOption = &iO->readbypass;
            else if( COMPARE_N("reset") == 0 )
            {   pOption = &iO->reset; type = OPT_FLAG; }
            else if( COMPARE_N("version") == 0 )
            {
                printf("%s\n", APP_VER_STRING);
                printf(" - built with API revision %s\n", scgtGetApiRevStr());
                exit (0);
            }
            else if( COMPARE_N("help") == 0 )
            {
                iO->helpLevel = 2;
                return 0;
            }
            else
                goto parse_fail;

            if( (opt[len] == '=') && (opt[len+1] != '\0') )
            {
                equalVal = strtoul(&(opt[len+1]), &endptr, 0);
                intType = !((int)(*endptr));
            }

            if ( (intType && (type & OPT_VALUE)) ||
                 ((type & OPT_ONOFF) &&
                     ((equalVal = 0, !strcmp(&(opt[len]), "off")) ||
                      (equalVal = 1,  !strcmp(&(opt[len]), "on")))    )
               )
            {
                pOption->flags |= OPT_VALUE;
                pOption->value = equalVal;
            }
            else if( opt[len] == '\0' && (type & OPT_FLAG) )
            {
                pOption->flags |= OPT_FLAG; /* flag that option is present */
            }
            else
            {
parse_fail:
                printf("\nInvalid option: \"%s\"\n\n", argv[i]);
                return -1;
            }

            iO->optionsFlags |= pOption->flags;
            continue;
        }

        for(j=1; argv[i][j] != '\0'; j++)               /* parse a - option */
        {                                 /* do options with arguments first*/
            if( (argv[i][j+1] == '\0') && (i < (argc - 1)))
            {
                uint32 flags = 0;
                tookParam = 1;  endptr = &nullchar;
                /* test for options which take parameters */
                /* these options only valid as last char of -string */
                if(     argv[i][j]=='u')
                    iO->unit=strtoul(argv[++i],&endptr,0);
                else if(argv[i][j]=='p')
                    iO->msPeriod=strtoul(argv[++i],&endptr,0);
                else if(argv[i][j]=='s')
                    iO->doSeconds=strtoul(argv[++i],&endptr,0);
                else if(argv[i][j]=='n')
                {   iO->nodeid.value=strtoul(argv[++i],&endptr,0);
                    iO->nodeid.flags |= OPT_VALUE; flags = OPT_VALUE;
                }
                else if(argv[i][j]=='b')
                {   iO->bint.value=strtoul(argv[++i],&endptr,0);
                    iO->bint.flags |= OPT_VALUE; flags = OPT_VALUE;
                }
                else if(argv[i][j]=='i')
                {   iO->iface.value=strtoul(argv[++i],&endptr,0);
                    iO->iface.flags |= OPT_VALUE; flags = OPT_VALUE;
                }
                else if(argv[i][j]=='y')
                {   iO->spyid.value=strtoul(argv[++i],&endptr,0);
                    iO->spyid.flags |= OPT_VALUE; flags = OPT_VALUE;
                }
                else tookParam = 0;

                iO->optionsFlags |= flags;

                if( *endptr )
                {
                    printf("\nInvalid parameter \"%s\" for option \"-%c\"\n\n",
                           argv[i], argv[i-1][j]);
                    return -1;
                }

                if( tookParam )
                    break;
            }
                                           /* options without arguments now */
            if(     toupper(argv[i][j])=='V') iO->doVerbose++;
            else if(         argv[i][j]=='N') iO->doNetwork = 1;
            else if(         argv[i][j]=='S') iO->doStats   = 1;
            else if(toupper(argv[i][j])=='A') iO->doDispAll = 1;
            else if(tolower(argv[i][j])=='h')
            {
                iO->helpLevel = (argc > 2) ? 3 : 2;
                return 0;
            }
            else
            {
                printf("\nUnexpected option: \"-%s\"\n\n",&(argv[i][j]));
                return -1;
            }
        }
    }
    iO->helpLevel=0;
    iO->iface.value = (iO->iface.value && 1); /* if non-zero, then 1 */
    iO->spyid.value &= 0xff;
    iO->nodeid.value &= 0xff;
    return 0;
}

/***************************************************************************/
/* Function:    printHelpLevel1()                                          */
/* Description: Display help and usage info                                */
/***************************************************************************/
static void printHelpLevel1(iOptionsType *iO)
{
    printf("%s\n\n", APP_VER_STRING);
    printf(
"Monitoring:\n"
"    gtmon [-u unit] [-p msperiod] [-s seconds] [-N |-V |-S] [-h]\n"
"    gtmon -A [-V | -VV | -N | -S]\n"
"\n");
    printf(
"Inquiry:\n"
"    gtmon [-u unit] [--nodeid] [--interface] [--ringsize] [--d64] [-V]\n"
"          [--wlast] [--linkup] [--rx] [--uint] [--laser ] [--signal ]\n"
"          [--ewrap] [--bswap ] [--tx] [--sint] [--laser0] [--signal0]\n"
"          [--spyid] [--wswap ] [--px] [--bint] [--laser1] [--signal1] [--lint]\n"
"\n");
    printf(
"Configuration:\n"
"    gtmon [-u unit] [-n nodeid] [-i interface] [-y spyid] [-b bintMask]\n"
"          [--wlaston|--wlastoff] [--rxon  |--rxoff  ] [--ewrapon |--ewrapoff ]\n"
"          [--sinton |--sintoff ] [--txon  |--txoff  ] [--laseron |--laseroff ]\n"
"          [--uinton |--uintoff ] [--pxon  |--pxoff  ] [--laser0on|--laser0off]\n"
"          [--bswapon|--bswapoff] [--d64on |--d64off ] [--laser1on|--laser1off]\n"
"          [--wswapon|--wswapoff] [--linton|--lintoff] [-V]\n"
"\n");
    printf("Defaults: gtmon -u 0\n");
#ifdef PLATFORM_VXWORKS
    printf("VxWorks users: Enclose options list in a set of quotes \" \".\n");
#endif
printf("\n");
    if(iO->helpLevel>1)
        printHelpLevel2();
    if(iO->helpLevel>2)
        printHelpLevel3();
}

static void printHelpLevel2(void)
{
    printf(
"Options:\n"
"  -u #  - board/unit number         -h   - show help, use -h 1 for more\n"
"  -n #  - set node ID               -b # - set broadcast interrupt mask\n"
"  -N    - show network info         -i # - set receive interface, 0 or 1\n"
"  -S    - show driver statistics    -A   - show info for all local devices\n"
"  -V    - verbose                   -VV  - very verbose, with -A\n"
"  -p #  - period, refresh every # milliseconds, 'q' to stop\n"
"  -s #  - seconds to run gtmon, defaults -p to 500, 'q' to stop\n"
"  -y #  - set the spy ID. HW will count (spy) traffic from this node ID.\n"
);
    printf(
"Inquiry options (value of 1 means \"on\", 0 means \"off\"):\n"
"  --linkup - link status               --signal  - active interface sig.det.\n"
"  --nodeid - node ID                   --signal0 - interface 0 signal detect\n"
"  --bint   - broadcast interrupt mask  --signal1 - interface 1 signal detect\n"
"  --spyid  - spy node ID               --interface - active interface\n"
);
    printf(
"Inquiry/assignment options (append \"on\" or \"off\" to assign value):\n"
"  --rx     - receive path    (R)       --laser   - active interface laser\n"
"  --px     - retransmit path (P)       --laser0  - interface 0 laser status\n"
"  --tx     - transmit path   (T)       --laser1  - interface 1 laser status\n"
"  --wlast  - write-last path (W)       --ewrap   - electronic wrap path (E)\n"
"  --uint   - unicast interrupts        --bswap   - byte swap GT memory\n"
"  --sint   - self-interrupts           --wswap   - word swap GT memory\n"
"  --d64    - 64bit PCI data transfer   --lint    - link error interrupts\n"
);

    printf("Note:  run `gtmon -h 1' for more information.\n");
}

static void printHelpLevel3()
{
    printf(
"\n"
"   Gtmon is a device monitoring and configuration utility for GT hardware.\n"
"Gtmon has several modes of operation.  The default mode displays general\n"
"information about the device (such as link status and laser status) in a\n"
"condensed format.  In verbose mode (-V), the same information is displayed\n"
"in human-readable form.  Additional information (including driver, firmware,\n"
"and API revisions) is also provided in verbose mode.  In statistics (-S)\n"
"mode, gtmon retrieves and displays statistical information that is kept by\n"
"the GT driver.  In network (-N) mode, gtmon constructs a network loop table.\n"
"The all-units (-A) option displays information for all local devices. The\n"
"output of the -A option may be modified with the -S, -N, -V and -VV options.\n"
"\n");
    printf(
"   Gtmon has many inquiry-type options for retreiving individual pieces of\n"
"device status information.  When these options are specified, a numeric\n"
"result will be displayed.  In most cases, the result is boolean where a\n"
"value of 1 means active (on) and 0 means inactive (off).  Some exceptions\n"
"are: --nodeid which reports the current node ID, --interface which reports\n"
"the interface number of the active receiver, and --bint which reports the\n"
"broadcast interrupt mask. Though it is valid to perform multiple inquiries\n"
"in one execution of gtmon, the inquiries will NOT be performed according\n"
"to their relative ordering on the command line.  In such situations,\n"
"the -V option may be used for interpretable results.\n"
"\n");
    printf(
"   Many of the inquiry-type options also have an assignment form which\n"
"allows the user to specify a device setting.  The assignment form of an\n"
"inquiry option is made by appending 'on' or 'off' to the base option.\n"
"For example, the --rx option is used to determine if the device receive\n"
"path is on or off.  The option --rxon will turn the receive path on, and\n"
"the option --rxoff will turn the receive path off.\n"
"\n");
    printf(
"============================================================================\n"
"ABBREVIATIONS - This section lists some abbreviations used by gtmon.\n"
"\n"
" MB/s  - megabytes/second network traffic     I0 - interface 0\n"
" nsec  - latency in nanoseconds               I1 - interface 1\n"
" Bytes - bytes of network traffic             R  - receive path\n"
" nInt  - number of network interrupts         P  - retransmit (pass-thru) path\n"
" qInt  - nInts placed in receive queue        T  - transmit path\n"
" Errs  - number of errors                     W  - write-last path\n"
" nID   - node ID                              E  - electronic wrap path\n"
" uID   - upstream node ID                     RS - ring size\n"
" HOPS  - relative position of upstream node\n"
"\n");
    printf(
"============================================================================\n"
"DEFAULT DISPLAY - This section discusses the default (condensed) display\n"
"mode. Example output is shown for command 'gtmon -u 0'.\n"
"\n"
"gtmon: unit: 0\n"
"  MB/s    nsec       Bytes       nInt       Errs  RS nID<uID  I0  I1 RPT W E\n"
"     -    2146  3430146768  148565778     680520   2 138<1   +UA -.a 111 1  \n"
"  6.31    2108     6376036          0          0   2 138<1   +UA -.a 111 1  \n"
"\n");
    printf(
"   The MB/s, nsec, Bytes, nInt, and Errs columns report information as\n"
"described above in the ABBREVIATIONS section. In periodic mode (-p), each\n"
"display update after the initial display reports Bytes, nInt, and Errs as\n"
"the difference in value since the previous update.  MB/s is only valid in\n"
"periodic mode.\n"
"\n");
    printf(
"   For RS, values from 1 to 255 are valid.  A value of 256 indicates the\n"
"absence of a ring (data path between the device's transmitter and receiver).\n"
"\n"
"   The nID field gives the node ID of the selected device/unit.  The uID\n"
"field gives the node ID of the upstream device, where the upstream device\n"
"sends data to the selected device. The '<' arrow indicates data flow.\n"
"\n");
    printf(
"    For I0 and I1, three characters are displayed.  The first character\n"
"represents signal-detect status, where '+' means signal-detected and '-' means\n"
"signal-not-detected.  The second represents link status, where 'U' means\n"
"link-up and 'D' means link-down.  The third represents laser status, where\n"
"'A' means active and 'O' means off.  The respective letters will be\n"
"uppercase for the active interface, and lowercase for the inactive interface.\n"
"A dot '.' means not-applicable.\n"
"\n");
    printf(
"    For any of R, P, T, W, and E, a value of 1 means the path is active (on)\n"
"and a value of 0 means the path is inactive (off).\n"
"\n");
    printf(
"============================================================================\n"
"VERBOSE DISPLAY - This section discusses the verbose display mode (gtmon -V).\n"
"\n"
"   The verbose display provides the information from the default display in\n"
"human-readable form, plus additional information. It also makes use of the\n"
"same abbreviations and encodings as the default display. In periodic mode,\n"
"change counts and throughput rates are provided for some statistics,\n"
"on each update after the initial display.\n"
"\n");
    printf(
"============================================================================\n"
"NETWORK DISPLAY - This section discusses the network display mode (gtmon -N).\n"
"Example output is shown for command 'gtmon -u 0 -N' for a ring topology.\n"
"\n"
"gtmon: unit: 0\n"
"HOPS nID<uID  I0  I1 RPT W  NOTES\n"
"  -0  17<1   +UA -.a 111 1  Monitor node, Driver info\n"
"  -1   1<2   +UA +.a 110 1  \n"
"  -2   2<14  -.a +UA 011 1  \n"
"  -3  14<3   +UA -.o 111 1  \n"
"  -4   3<17  +UA -.a 111 0  \n"
"  -5  17<1   +UA -.a 111 1  Monitor node, Network info, Ring closed \n"
"SUMMARY:\n"
"_ 5 nodes connected\n"
"_ 5 nodes reporting\n"
"_ 0 node ID duplications\n"
"_ Connections: 17<1<2<14<3<17  Ring closed\n"
"\n"
"   The network display shows many of the fields from the default display\n"
"(see the DEFAULT DISPLAY section), however the information is shown for all\n"
"nodes on the network for which the information is available. Gtmon attempts\n"
"to detect duplicate node IDs, and reports such conditions. The '?' symbol is\n"
"used to indicate unknown information. Notes may appear in the NOTES column.\n"
"\n"
"   The HOPS field gives the relative upstream positioning of a node. HOPS\n"
"values are negative to indicate the node's positioning is upstream and\n"
"against the flow of data.\n"
"\n");
}
