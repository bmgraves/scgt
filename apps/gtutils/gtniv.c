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

#define APP_VER_STRING "GT Network Interrupt Verifier (gtniv) rev. 1.08 (2005/04/01)"
#define APP_VxW_NAME gtniv

/**************************************************************************/
/****** TAKING CARE OF ALL POSSIBLE OS TYPES - COPY-PASTE *****************/
/**************************************************************************/
/* control by PLATFORM_WIN, PLATFORM_UNIX, PLATFORM_VXWORKS */          /**/
#include <math.h>                                                       /**/
#include <stdio.h>                                                      /**/
#include <stdlib.h>                                                     /**/
#include <time.h>                                                       /**/
#include <string.h>                                                     /**/
#include <ctype.h>                                                      /**/
                                                                        /**/
#define PARSER                                                          /**/
#define MAIN_IN      int main(int argc, char **argv) {                  /**/
                                                                        /**/
#ifdef PLATFORM_WIN                                                     /**/
    #include <windows.h>                                                /**/
                                                                        /**/
#elif PLATFORM_VXWORKS                                                  /**/
    #undef PARSER                                                       /**/
    #undef MAIN_IN                                                      /**/
    #define VXL     40                                                  /**/
    #define MAIN_IN int APP_VxW_NAME(char *cmdLine) { \
            int argc; char argv0[]="gtniv"; char *argv[VXL]={argv0};    /**/
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

#include "scgtapi.h"
#include "usys.h"

#define INPUT_OPTIONS \
    uint32  msDelay, unit, helpLevel, displayMode, doSend; \
    uint32  seconds, mId, bcMask, ucMask, intrPerSec, flags; \
    int burstSize;

typedef struct {
    uint32            timeToExit;
    uint32            resetStats;
    uint32            showHelp;
    uint32            lockMask;
    uint32            flag;
    uint32            running;
    uint32            displayMode;
    usysThreadParams  pt;
} exitThreadParms;

typedef struct
{
    INPUT_OPTIONS
    char optionsString[128];
    scgtHandle hDriver;
    exitThreadParms exitParms;
} iOptionsType;

/* behavior flags - positionally coded */
#define FLAG_UNEVEN_DIST 0x1 /* sender */
#define FLAG_RANDOM_MASK 0x2 /* receiver */

/* display modes - positionally coded */
#define DM_SUM   0x1     /* summary (TOTAL) line updated every second */
#define DM_CONF  0x2     /* network view updated on changes and errors */
#define DM_ERR   0x4     /* error details */
#define DM_DUMP  0x8     /* extensive interrupt details on error */

static void buildOptionsString( char *optionsString, iOptionsType *iO );
static int parseCommand(int argc, char **argv, iOptionsType *iO,
                         char *optionsString);
static void printHelpLevel1(iOptionsType *iO, char *optionsString);
static void printHelpLevel2();
static void printHelpLevel3();
static void *getRuntimeChar(void *voidp);
static void gtnivSendBoss(iOptionsType *iO);
static void gtnivRecvBoss(iOptionsType *iO);

/**************************************************************************/
/************************* G T N I V   C O D E ****************************/
/**************************************************************************/

MAIN_IN

    int ret;
    iOptionsType iO;

    PARSER;        /* prepare argc and agrv if not provided by OS (VxWorks) */

    memset(&iO, 0, sizeof(iOptionsType));               /* zero all options */
    ret = parseCommand(argc, argv, &iO, iO.optionsString);  /* parsing options */

    if(ret || (iO.helpLevel!=0)) /*1, 2, 3, ...*/
    {
        printHelpLevel1(&iO, iO.optionsString);
        return ret;
    }

    if (scgtOpen(iO.unit, &iO.hDriver) != SCGT_SUCCESS)
    {
        printf("gtniv: could not open unit %i\n", iO.unit);
        return -1;
    }

    if( iO.mId == -1 )
    {
        iO.mId = scgtGetState(&(iO.hDriver), SCGT_NODE_ID);
    }

    memset(&iO.exitParms, 0, sizeof(exitThreadParms));

//    if( (iO.seconds == -1) || (iO.seconds > 60) )
    {                                              /* spawn exit thread */
        iO.exitParms.pt.parameter = &iO.exitParms;
        iO.exitParms.pt.priority = UPRIO_HIGH; /* only use UPRIO_HIGH */
        sprintf(iO.exitParms.pt.threadName, "gtnivExit");
        usysCreateThread(&iO.exitParms.pt, getRuntimeChar);
        /* Give thread a moment to start up. Needed on VxWorks, Solaris 8 */
        usysMsTimeDelay(50);
    }

    printf("Running: %s\n", iO.optionsString);

    if( iO.doSend )
        gtnivSendBoss(&iO);
    else
        gtnivRecvBoss(&iO);

    if( iO.exitParms.running )
        usysKillThread(&iO.exitParms.pt);

    scgtClose(&iO.hDriver);
    return 0;
}

/****** Random Number Generator - Copyright (c) 1991 Leszek D. Wronski ********/
#define irdn6(a,x) {int _irdP=a[63]; _irdP=(_irdP+1)%63; (x)=(x)-a[_irdP];\
    if((x)<0) (x)=(x)+2147483646; a[_irdP]=(x);a[63]=_irdP;}
#define irdn6_vars(a) long int a[64] = {67,111,112,121,114,105,103,104,116,32,\
  40,99,41,32,49,57,57,49,44,32,76,101,115,122,101,107,32,68,46,32,87,114,111,\
  110,115,107,105,32,32,1775811865,893145566,995405759,2102736524,271520213,\
  456633834,2058258651,1583689592,878442065,1675280054,1627576247,2054802212,\
  60335757, 684462146, 1625503827, 521827728,1266032265, 93253262, 2053047983,\
  685333180, 127474245, 948921754, 1964990155, 1228891816,0}
/*Usage: instantiate irdn6_vars and some long int randvector. To randomize
 starting point you may put some value into randvector . Call irdn6(randvector);
 - new random value will be placed in randvector - use it but do not change it*/
/********************* Random Number Generator - END **************************/


/***************************************************************************/
/* Function:    gtnivDiffCounter()                                         */
/* Description: Calculates difference between current counter value and    */
/*    previous counter value, accounting for (at most 1) roll-over.        */
/***************************************************************************/
static uint32 gtnivDiffCounter(uint32 currCnt, uint32 lastCnt, uint32 maxCnt)
{
    if( currCnt >= lastCnt)
        return ( currCnt - lastCnt );
    else /* overflow */
        return ( currCnt + ( maxCnt - lastCnt ) + 1 );
}


/***************************************************************************/
/* Function:    gtnivSendInterrupts                                        */
/* Description: Sends requested interrupts                                 */
/***************************************************************************/
static void gtnivSendBoss(iOptionsType *iO)
{
    #define NUM_INTS       33  /* 32 bcast + 1 ucast */
    #define UCAST_INT_NUM  32
    #define MIN_MS_DELAY   20   /* typical system delay resolution */
    #define MIN_MS_YIELD   2  /* guess reschedule resolution of "sleep(0)" */

    scgtInterrupt intr;
    usysMsTimeType curTime;
    uint32 i, j, curSecond, lastSecond;
    uint32 sendValBase[NUM_INTS] = {0};
    uint32 sendArray[NUM_INTS*2]; /* use two, so may send same int back-to-back */
    uint32 val, cnt = 0, oldCnt = 0, printCnt = 0, cntReal = 0, oldCntReal = 0;
    long int randvector, temp;
    int offset = 0, burst;
    int intId, intPerSec = 0, realIntPerSec = 0;
    double numBursts = 0.0;
    int tolerance = ( iO->intrPerSec / 20 ); /* 5% of */

    irdn6_vars(gen1);
    randvector = (time(NULL) & 0xeFFFeFFF);

    if ( tolerance == 0 )
        tolerance = 2;

    /* burst iO->burstSize interrupts, then delay for MIN_MS_DELAY */
    iO->burstSize = iO->intrPerSec/(1000/MIN_MS_DELAY);

    if ( iO->burstSize < 1 )
        iO->burstSize = 1;

    iO->msDelay = (1000/(iO->intrPerSec/iO->burstSize)) - 10;

    if( (int)iO->msDelay < 1 )
        iO->msDelay = 1;

    if ( (iO->bcMask == 0) && (iO->ucMask == 0) )
    {
        printf ("Invalid interrupt masks. No interrupts selected.\n");
        return;
    }

    for(i=0, intId=0; i < (NUM_INTS*2); intId = ((intId+1) % NUM_INTS) )
    {
        /* only send those which are not masked away */
        if ( (intId == UCAST_INT_NUM) )
        {
            if ( iO->ucMask )
                sendArray[i++] = intId;
        }
        else if ( (iO->bcMask & (1 << intId)) )
        {
            sendArray[i++] = intId;
        }
    }

    for( i = 0; i < NUM_INTS; i++)
    {
        /* don't start all with same send val, to flip more bits */
        irdn6(gen1,randvector);
        sendValBase[i] = randvector;
    }

    /* send one of each interrupt with val 0x(mid)ffffff to signal startup */
    for( i = 0; i < NUM_INTS; i++)
    {
        if ( (i == UCAST_INT_NUM) ) 
        {
            if (!iO->ucMask) /* apply "use" mask */
                continue;

            for( j=0; j < 256; j++ )
            {
                intr.type = SCGT_UNICAST_INTR;
                intr.id = j;
                intr.val = (iO->mId << 24) | 0xffffff;
                scgtWrite(&iO->hDriver, 0, NULL, 0, 0, NULL, &intr);
            }
        }
        else if ( (iO->bcMask & (1 << i)) ) /* apply "use" mask */
        {
            intr.type = SCGT_BROADCAST_INTR;
            intr.id = i;
            intr.val  = (iO->mId << 24) | 0xffffff;
            scgtWrite(&iO->hDriver, 0, NULL, 0, 0, NULL, &intr);
        }
    }

    usysMsTimeStart(&curTime);
    lastSecond = curSecond = 0;

    burst=0;

    do
    {
        /* randomize send array */
        for( i = 0; i < (NUM_INTS*2); i++)
        {
            irdn6(gen1,randvector);
            offset = randvector%(NUM_INTS*2);
            temp = sendArray[offset];
            sendArray[offset] = sendArray[i];
            sendArray[i] = temp;
        }

        /* now send the interrupts, at the desired rate */

        for( i = 0; i < (NUM_INTS*2) && !iO->exitParms.timeToExit; i++)
        {
            if ( iO->flags & FLAG_UNEVEN_DIST )
            {
                irdn6(gen1,randvector);
                intId = sendArray[randvector % (NUM_INTS*2)];
            }
            else
            {
                intId = sendArray[i]; /* send intr described by sendArray[i] */
            }

            /* construct value to be sent with interrupt, excluding intID */
            val = 0 | (iO->mId << 24) | (sendValBase[intId] & 0xffff);

            if( intId == UCAST_INT_NUM )
            {
                /* send all unicast interrupts at once, since each receiving
                   node will only get one */
                for(j=0; j < 256; j++)
                {
                    intr.type = SCGT_UNICAST_INTR;
                    intr.id   = j;
                    intr.val  = val | (j <<16);
                    scgtWrite(&iO->hDriver, 0, NULL, 0, 0, NULL, &intr);
                    cntReal++;
                }
            }
            else
            {
                intr.type = SCGT_BROADCAST_INTR;
                intr.id   = intId;
                intr.val  = val | (intId << 16);
                scgtWrite(&iO->hDriver, 0, NULL, 0, 0, NULL, &intr);
                cntReal++;
            }

            /* increment counts */
            sendValBase[intId]++;
            cnt++;
            burst++;

            if( burst >= iO->burstSize )
            {
                burst = 0;

                if( !iO->exitParms.timeToExit ) /* delay */
                    usysMsTimeDelay( iO->msDelay );
            }

            curSecond = usysMsTimeGetElapsed(&curTime) / 1000;

            if ( lastSecond < curSecond )
            {
                /* Calculate the int/second rate and adjust iO->burstSize */
                intPerSec = gtnivDiffCounter(cnt, oldCnt, 0xffffffff) /
                                (curSecond - lastSecond); /* rough */

                realIntPerSec = gtnivDiffCounter(cntReal, oldCntReal, 0xffffffff) /
                                (curSecond - lastSecond); /* rough */

                offset = intPerSec - iO->intrPerSec;

                /* how many bursts occurred last period */
                numBursts = (double)intPerSec / (double)iO->burstSize;

                if( ! (printCnt++ % 10) )
                    printf("intr/sec  dTGT    #bursts   size       total delay rIntr/sec\n");

                printf("%8d %+5d %10.2f %6d %11d %5d %9d\n", intPerSec,
                    offset, numBursts, iO->burstSize, cnt, iO->msDelay, realIntPerSec );

                if( !numBursts )
                    numBursts = 1;

                /* adjust burstSize to meet desired rate */
                iO->burstSize = (int)(((double)iO->intrPerSec / numBursts)+0.5);

                if( iO->burstSize <= 1 )
                {
                    iO->burstSize = 1;
                    if( offset > tolerance )
                        iO->msDelay += 10;
                }
                else if( iO->burstSize >= (int)iO->intrPerSec )
                {
                    iO->burstSize = iO->intrPerSec;
                    if( offset < -tolerance )
                        iO->msDelay -= 10;
                }
                else if ( iO->burstSize > 256 )
                {
                    if(offset < -tolerance)
                        iO->msDelay -= 10;
                    if(offset > tolerance)
                        iO->msDelay += 10;
                }

                if( (int)iO->msDelay <= 0 )
                {
                    iO->msDelay = 1;
                }

                oldCnt = cnt;
                oldCntReal = cntReal;
                lastSecond = curSecond;
                fflush(stdout);
            }

            /* if timed and not "run forever" and time met or exceeded */
            if( (iO->seconds != -1) && (curSecond >= iO->seconds) )
            {
                iO->exitParms.timeToExit = 1;
            }
        }

    }while( !iO->exitParms.timeToExit );

    printf("%8d %+5d %10.2f %6d %11d %5d\n", intPerSec,
        offset, numBursts, iO->burstSize, cnt, iO->msDelay );

    usysMsTimeStop(&curTime);
}

/* statistic offsets */
#define MID_INT_COUNT         0
#define MID_INT_COUNT_GB      1
#define MID_INT_COUNT_LAST    2
#define MID_INT_COUNT_GB_LAST 3
#define MID_ERR               4
#define MID_ERR_LAST          5
#define MID_INT_PER_SEC       6
#define MID_SRC_NID           7
#define MID_MISSED_INT        8
#define MID_REREAT_INT        9
#define MID_LAST_TIME        10

#define NUM_STATS            11

/* array bounds, etc */
#define NUM_MIDS             256
#define NUM_INTS             33  /* 32 bcast + 1 ucast */
#define UCAST_INT_NUM        32

int gtnivReport(uint32 displayMode, uint32 *midStats, uint32 *totals)
{
    uint32 *mStats;
    uint32 k;

    if( !(displayMode & DM_CONF) )
        return 0;

    printf("\nmId nId     count     error    missed  repeated  intr/sec   time\n");

    for( k = 0; k < NUM_MIDS; k++ )
    {
        mStats = &(midStats[k*NUM_STATS + 0]);

        if( mStats[MID_LAST_TIME] ) /* has it ever been updated */
        {
            printf("%3d %3d %.0u%9.*u %9d %9d %9d %9d %6d\n",
                k, mStats[MID_SRC_NID], mStats[MID_INT_COUNT_GB],
                (mStats[MID_INT_COUNT_GB]? 9:1), mStats[MID_INT_COUNT],
                mStats[MID_ERR], mStats[MID_MISSED_INT],
                mStats[MID_REREAT_INT], mStats[MID_INT_PER_SEC],
                mStats[MID_LAST_TIME] );
        }
    }

    printf("TOTALS: %.0u%9.*u %9d %9d %9d %9d %6d",
        totals[MID_INT_COUNT_GB],
        (totals[MID_INT_COUNT_GB]? 9:1), totals[MID_INT_COUNT],
        totals[MID_ERR], totals[MID_MISSED_INT],
        totals[MID_REREAT_INT], totals[MID_INT_PER_SEC],
        totals[MID_LAST_TIME] );
    return 1;
}

void dumpInterrupt(scgtInterrupt* intr, int seconds)
{
    printf("\n_DUMP dT=%i: t=%s n=%d i=%d v=0x%x s=%d", seconds,
           (intr->type == SCGT_UNICAST_INTR) ? "U" :
             ((intr->type == SCGT_BROADCAST_INTR) ? "B" :
               ((intr->type == SCGT_BROADCAST_INTR) ? "E" : "?")),
           intr->sourceNodeID, intr->id, intr->val, intr->seqNum);
}

/***************************************************************************/
/* Function:    gtnivRecvBoss                                              */
/* Description:                                                            */
/***************************************************************************/
static void gtnivRecvBoss(iOptionsType *iO)
{
    #define PERIOD_FORCED_REFRESH  599
    #define PERIOD_MASK_CHANGE     19
    /* consider an mid gone if no ints in this many seconds */
    //#define PERIOD_KILL            (PERIOD_MASK_CHANGE + 10)
    #define PERIOD_KILL            2

    #define PRINTIT(DM)  if(displayMode&(DM)) printits += printf

    #define RESET_STATS \
            usysMsTimeStop(&curTime); \
            usysMsTimeStart(&curTime); \
            curSecond = (usysMsTimeGetElapsed(&curTime) / 1000); \
            lastSecond = lastUpdate = lastMaskUpdate = curSecond; \
            errorCnt = bcastCnt = ucastCnt = appMissedCnt = errorCntLast = 0; \
            timeoutCnt = drvMissedCnt = drvLinkErrCnt = unknownErrCnt = 0; \
            ignoredCnt = 0; \
            memset(errArray, 0, (NUM_ERRORS*sizeof(uint32)) ); \
            memset(midStats, 0, (NUM_MIDS*NUM_STATS*sizeof(uint32)) ); \
            memset(expect, 0xff, (NUM_MIDS*NUM_INTS*sizeof(uint32)) );

    #define SUMMARY   PRINTIT(DM_SUM)("%sdT=%i I/s=%i B=%i U=%i EM=%i EL=%i M=%i X=%i T=%i Ex=%d    \r",\
                    printits ? "\n" : "", curSecond, intrPerSec, bcastCnt, ucastCnt, drvMissedCnt,\
                    drvLinkErrCnt, appMissedCnt, unknownErrCnt, timeoutCnt, errArray[0]); printits=0;

    #define INTR_PER_CALL  512
    scgtIntrHandle hIntr = -1;
    scgtInterrupt intrs[INTR_PER_CALL];
    usysMsTimeType curTime;
    uint32 curSecond, lastSecond, lastUpdate, lastMaskUpdate;
    uint32 ucastCnt, bcastCnt, errorCnt, errorCntLast, timeoutCnt, ignoredCnt;
    uint32 drvMissedCnt, drvLinkErrCnt, unknownErrCnt, appMissedCnt;
    uint32 numIntrs = 0, ret = 0, update = 0, error, intrPerSec=0;
    uint16 missed = 0;
    uint32 *mStats, *midStats, *expect;
    uint32 myNid = scgtGetState(&(iO->hDriver), SCGT_NODE_ID);
    uint32 i, j, k, mid, vVal, intId, exp, changeDelay = 0, printits=0;
    uint32 displayMode = iO->displayMode, lockMask = 0;
    uint32 bcastmask, expectMask, expectMaskOld, rxBmask, expectMaskChange = 0;
    uint32 canUseMask = iO->bcMask, totalRxBmask = 0, totalRxUmask = 0;
    uint32 expectSeqNum = -1;
    #define NUM_ERRORS  32
    uint32 errArray[NUM_ERRORS] = {0};
    uint32 totals[NUM_STATS] = {0};

    long int randvector;
    irdn6_vars(gen1);
    randvector = (time(NULL) & 0xeFFFeFFF);

    if ( (iO->bcMask == 0) && (iO->ucMask == 0) )
    {
        printf ("Invalid interrupt masks. No interrupts selected.\n");
        return;
    }

    midStats = usysMemMalloc(sizeof(uint32)*NUM_MIDS*NUM_STATS);
    /* use 256 for safety, we pull the ID from 8-bits of the value in gtniv */
    expect = usysMemMalloc(sizeof(uint32)*NUM_MIDS*256);
    /*expect   = usysMemMalloc(sizeof(uint32)*NUM_MIDS*NUM_INTS);*/

    /*
     * NOTE: midStats and expect function as if defined as:
     *  uint32 midStats[NUM_MIDS][NUM_STATS];
     *  uint32 expect[NUM_MIDS][NUM_INTS];
     */

    if( (midStats == NULL) || (expect == NULL) )
    {
        goto end;
    }

    usysMsTimeStart(&curTime);
    RESET_STATS;

    bcastmask = scgtGetState(&iO->hDriver, SCGT_BROADCAST_INT_MASK);
    expectMask = canUseMask & bcastmask;
    expectMaskOld = expectMask;

    do
    {
        if( iO->exitParms.flag )
        {
            iO->exitParms.flag = 0;

            if( iO->exitParms.showHelp )
            {
                time_t timer = time(NULL);
                /* dump stats */
                iO->exitParms.showHelp = 0;

                printf("\n%s\n", APP_VER_STRING);
                printf("ERROR STATISTICS - reported on %s", ctime(&timer));
                for(i=0; i < NUM_ERRORS; i++)
                {
                    printf("ERR%.2d=%8d  %s",
                           i, errArray[i], ((i%4)==3)?"\n":"");
                }
                printf("Running: %s\n", iO->optionsString);
                printf("_CONF - Masks: b=0x%.8x e=0x%.8x oe=0x%.8x v=0x%.8x\n",
                            bcastmask, expectMask, expectMaskOld, iO->bcMask);
                printf("Display mode: 0x%x   Mask lock: %s\n",
                        displayMode, lockMask ? "on" : "off");
                printits+=gtnivReport(-1 /* all */, midStats, totals);
                SUMMARY;
                printf("\n");
                fflush(stdout);
            }
            if( iO->exitParms.lockMask )
            {
                lockMask = (lockMask+1)%2;
                iO->exitParms.lockMask = 0;
            }
            if( iO->exitParms.resetStats )
            {
                printf("RESET\n");
                RESET_STATS;
                iO->exitParms.resetStats = 0;
                continue;
            }
            if( iO->exitParms.displayMode )
            {
                displayMode = (displayMode + iO->exitParms.displayMode) % 8;
                iO->exitParms.displayMode = 0;
            }
        }

        ret = scgtGetInterrupt(&iO->hDriver, &hIntr, intrs,
                INTR_PER_CALL, 250, &numIntrs);

        if ( ret == SCGT_TIMEOUT )
        {
            timeoutCnt++;
            numIntrs = 0;
        }
        else if ( ret == SCGT_MISSED_INTERRUPTS )
        {
            PRINTIT(DM_ERR)("\n_ERR17 dT=%i: Missed interrupts!!!!", curSecond);
            errArray[17]++;
            appMissedCnt++;
        }
        else if( ret )
        {
            PRINTIT(DM_ERR)("\n_ERR3 dT=%i: %s", curSecond, scgtGetErrStr(ret));
            errArray[3]++;
            numIntrs = 0;
        }

        for( i = 0; i < numIntrs; i++ )
        {
            error = 0;

            /* mid, intId, and vVal invalid for error ints */
            mid = (intrs[i].val >> 24) & 0xff;
            intId = (intrs[i].val >> 16) & 0xff;
            vVal = intrs[i].val & 0xffff;

            mStats = &(midStats[mid*NUM_STATS + 0]);

            /* must test seqnum for all interrupts, even those our mask
               tells us to ignore */
            #define SEQ_NUM_MASK  0x7fffff

            if( (intrs[i].seqNum != expectSeqNum ) &&
                (expectSeqNum != -1) )
            {
                PRINTIT(DM_ERR)("\n_ERR18 dT=%i: seqNum error s=0x%x e=0x%x missed=%i",
                                curSecond, intrs[i].seqNum, expectSeqNum,
                                (int)gtnivDiffCounter(intrs[i].seqNum,
                                                 expectSeqNum,
                                                 SEQ_NUM_MASK+1));
                errArray[18]++;
            }

            expectSeqNum = ((intrs[i].seqNum+1) & SEQ_NUM_MASK);

            /* Will assume no error interrupt or unknown type
               interrupt with such a value */
            if( (intrs[i].val & 0xffffff) == 0xffffff )
            {
                /* reset info for this node ID */

                /* set all expected values to -1, and clear all stats */
                memset(&expect[mid*NUM_INTS], 0xff, (NUM_INTS*sizeof(uint32)) );
                memset(mStats, 0, (NUM_STATS*sizeof(uint32)) );
                continue; /* process next interrupt in queue */
            }

            switch( intrs[i].type )
            {
                case SCGT_UNICAST_INTR:

                    totalRxUmask = 1;

                    if ( !iO->ucMask )
                    {
                        ignoredCnt++;
                        continue;
                    }

                    ucastCnt++;

                    if( intrs[i].sourceNodeID > 255 )
                    {
                        PRINTIT(DM_ERR)("\n_ERR4 dT=%i: Invalid source ID (%d)",
                                        curSecond, intrs[i].sourceNodeID);
                        errArray[4]++;
                        error++;
                    }

                    if( intId != myNid )
                    {
                        /* try to account for possible change in node ID */
                        myNid=scgtGetState(&(iO->hDriver), SCGT_NODE_ID);

                        if( intId != myNid )
                        {
                            PRINTIT(DM_ERR)("\n_ERR5 dT=%i: UC ID mismatch, txID=%d, rxID=%d",
                                            curSecond, intId, myNid);
                            errArray[5]++;
                            error++;
                        }
                    }

                    exp = expect[mid*NUM_INTS+UCAST_INT_NUM];

                    if( (vVal != exp) && (exp != -1) )
                    {
                        missed = (uint16)(gtnivDiffCounter(vVal, exp, 0xffff) &
                                         0xffff);
                        /*
                         * HW queue is only 256 interrupts, so an actual repeat
                         * of anything older than 256 should be impossible.
                         * Assume older ones are from misses and not repeats.
                         */
                        if( ((int16)missed < 0) && ((int16)missed > -256))
                            mStats[MID_REREAT_INT] += -((int16)missed);
                        else /* vVal < exp */
                            mStats[MID_MISSED_INT] += missed;
                        error++;
                        PRINTIT(DM_ERR)("\n_ERR6 dT=%i: UC nid=%d mid=%d, d=0x%.8x, e=0x%.8x, x=0x%.8x, m=%d",
                            curSecond, intrs[i].sourceNodeID, mid, vVal, exp, exp ^ vVal, (int16)missed);
                        errArray[6]++;
                    }

                    expect[mid*NUM_INTS+UCAST_INT_NUM]= (vVal + 1) % 0x10000;
                    mStats[MID_SRC_NID] = intrs[i].sourceNodeID;
                    mStats[MID_INT_COUNT]++;

                    if ( error )
                    {
                        if(displayMode & DM_DUMP){printits++; dumpInterrupt(&intrs[i], curSecond);}
                        errorCnt++;
                        mStats[MID_ERR]++;
                    }

                    break;

                case SCGT_BROADCAST_INTR:

                    rxBmask = 1 << intrs[i].id;
                    totalRxBmask |= rxBmask;

                    if ( !(rxBmask & canUseMask) )
                    {
                        ignoredCnt++;
                        continue;
                    }

                    bcastCnt++;

                    if ( !(expectMask & rxBmask) )
                    {
                        PRINTIT(DM_ERR)("\n_ERR7 dT=%i: Received masked bcast interrupt. m=%x id=%d",
                                        curSecond, expectMask, intrs[i].id);
                        errArray[7]++;
                        error++;
                    }

                    if( intrs[i].sourceNodeID > 255 )
                    {
                        PRINTIT(DM_ERR)("\n_ERR8 dT=%i: Invalid source ID %d", curSecond, intrs[i].sourceNodeID);
                        errArray[8]++;
                        error++;
                    }
                    if( intrs[i].id > 31 )
                    {
                        PRINTIT(DM_ERR)("\n_ERR9 dT=%i: Invalid rxID %d", curSecond, intrs[i].id);
                        errArray[9]++;
                        error++;
                    }
                    if( intId > 31 )
                    {
                        PRINTIT(DM_ERR)("\n_ERR10 dT=%i: Invalid txID %d", curSecond, intId);
                        errArray[10]++;
                        error++;
                    }
                    if( intId != intrs[i].id )
                    {
                        PRINTIT(DM_ERR)("\n_ERR11 dT=%i: BC ID mismatch, txID=%d rxID=%d",
                            curSecond, intId, intrs[i].id);
                        errArray[11]++;
                        error++;
                    }

                    exp = expect[mid*NUM_INTS+intId];

                    if( (vVal != exp) && (exp != -1) )
                    {
                        missed = (uint16)(gtnivDiffCounter(vVal, exp, 0xffff) &
                                         0xffff);
                        /*
                         * HW queue is only 256 interrupts, so an actual repeat
                         * of anything older than 256 should be impossible.
                         * Assume older ones are from misses and not repeats.
                         */
                        if( ((int16)missed < 0) && ((int16)missed > -256))
                            mStats[MID_REREAT_INT] += -((int16)missed);
                        else /* vVal < exp */
                            mStats[MID_MISSED_INT] += missed;

                        error++;
                        PRINTIT(DM_ERR)("\n_ERR12 dT=%i: BC nid=%d mid=%d d=0x%.8x e=0x%.8x x=0x%.8x m=%d",
                            curSecond, intId, mid, vVal, exp, exp ^ vVal, (int16)missed);
                        errArray[12]++;
                    }

                    expect[mid*NUM_INTS+intId] = (vVal + 1) % 0x10000;
                    mStats[MID_SRC_NID] = intrs[i].sourceNodeID;
                    mStats[MID_INT_COUNT]++;

                    if ( error )
                    {
                        if(displayMode & DM_DUMP){printits++; dumpInterrupt(&intrs[i], curSecond);}
                        errorCnt++;
                        mStats[MID_ERR]++;
                    }

                    break;

                case SCGT_ERROR_INTR:
                    errorCnt++;

                    if( intrs[i].val == SCGT_LINK_ERROR )
                    {
                        drvLinkErrCnt++;
                        PRINTIT(DM_ERR)("\n_ERR13 dT=%i: Link error interrupt", curSecond);
                        errArray[13]++;
                    }
                    else if( intrs[i].val == SCGT_DRIVER_MISSED_INTERRUPTS )
                    {
                        drvMissedCnt++;
                        PRINTIT(DM_ERR)("\n_ERR14 dT=%i: Driver missed interrupts", curSecond);
                        errArray[14]++;
                    }
                    else
                    {
                        unknownErrCnt++;
                        PRINTIT(DM_ERR)("\n_ERR15 dT=%i: Unknown error interrupt (%s)",
                                        curSecond, scgtGetErrStr(intrs[i].val));
                        errArray[15]++;
                    }

                    if(displayMode & DM_DUMP){printits++; dumpInterrupt(&intrs[i], curSecond);}

                    break;

                default:
                    errorCnt++;
                    unknownErrCnt++;
                    PRINTIT(DM_ERR)("\n_ERR16 dT=%i: Invalid interrupt type %d", curSecond, intrs[i].type);
                    if(displayMode & DM_DUMP){printits++; dumpInterrupt(&intrs[i], curSecond);}
                    errArray[16]++;
                    break;
            }
        }

        if (changeDelay)
        {
            /* update changeDelay after each call to scgtGetInterrupt, but
               not until after interrupts are processed */
            changeDelay--;

            if( numIntrs < INTR_PER_CALL )
                changeDelay = 0;

            if ( changeDelay == 0 )
            {
                /* officially remove the ones that were turned off at
                   last mask update */
                expectMask &= ~(expectMaskOld & expectMaskChange);
            }
        }

        curSecond = (usysMsTimeGetElapsed(&curTime) / 1000);

        if( lastSecond != curSecond )
        {
            /* if timed and not "run forever" and time not met or exceeded */
            if( (iO->seconds != -1) && (curSecond >= iO->seconds) )
            {
                iO->exitParms.timeToExit = 1;
            }

            if(iO->exitParms.timeToExit)
                update = 1;

            if( (lastMaskUpdate + PERIOD_MASK_CHANGE) < curSecond )
            {
                if( (totalRxBmask & expectMask) != expectMask )
                {
                    errorCnt++;
                    PRINTIT(DM_ERR)("\n_ERR1 dT=%i: Missing BC interrupts. IDs mask 0x%x e=0x%x",
                            curSecond, ((~totalRxBmask) & expectMask), expectMask );
                    errArray[1]++;
                }
    
                if( (totalRxUmask & iO->ucMask) != iO->ucMask )
                {
                    errorCnt++;
                    PRINTIT(DM_ERR)("\n_ERR2 dT=%i: Missing UC interrupts.\n", curSecond);
                    errArray[2]++;
                }
    
                if( (iO->flags & FLAG_RANDOM_MASK) && !lockMask )
                {
                    irdn6(gen1,randvector);
                    randvector |= (randvector << 1); /* so more are enabled */

                    expectMaskOld = expectMask;
                    expectMask = (randvector & canUseMask);
    
                    if (expectMask==0)
                        expectMask = 0xffffffff & canUseMask;

                    bcastmask = scgtGetState(&iO->hDriver, SCGT_BROADCAST_INT_MASK);
                    bcastmask &= ~canUseMask; /* don't modify ones we can't use */
                    bcastmask |= expectMask;
                    scgtSetState(&iO->hDriver, SCGT_BROADCAST_INT_MASK, bcastmask);
    
                    PRINTIT(DM_CONF)("\n_CONF dT=%i: Masks b=0x%.8x e=0x%.8x oe=0x%.8x v=0x%.8x",
                            curSecond, bcastmask, expectMask, expectMaskOld, iO->bcMask);
    
                    expectMaskChange = expectMaskOld ^ expectMask;
                    changeDelay = 4;
            
                    for( i = 0; i < NUM_INTS; i++ )
                    {
                        if( i == UCAST_INT_NUM )
                            continue;
            
                        if ( (expectMask & expectMaskChange & (1 << i)) )
                        {
                            /* This bcast intr turned on, so reset expected values.
                               Don't reset when turned off, because some may have
                               previously made it into the queue. */
                            for(j=0; j<NUM_MIDS; j++)
                                expect[j*NUM_INTS+i] = -1;
                        }
                    }
    
                    /* Until changeDelay is 0, we will still accept interrupts
                       that were on but are now off, because some may have
                       previously made it into the queue. */
                    expectMask |= (expectMaskOld & expectMaskChange);
                    /* At least one, but enough to empty driver interrupt queue.
                       Will assume queue is not larger than 2048. */
                    changeDelay = (2048+INTR_PER_CALL-1)/INTR_PER_CALL;
                }
    
                lastMaskUpdate = curSecond;
                totalRxBmask = totalRxUmask = 0;
            }

            intrPerSec = 0;
            memset(totals, 0, (NUM_STATS * sizeof(uint32)) );

            for( k = 0; k < NUM_MIDS; k++ )
            {
                int newInts, oldRate, rateCh;

                mStats = &(midStats[k*NUM_STATS + 0]);
                newInts = mStats[MID_INT_COUNT] - mStats[MID_INT_COUNT_LAST];

                if( newInts )
                {
                    if(mStats[MID_LAST_TIME] == 0)
                        PRINTIT(DM_CONF)("\n_CONF dT=%i: Server mId %d added.", curSecond, k);

                    mStats[MID_LAST_TIME]=curSecond;

                    if (mStats[MID_INT_COUNT] >= 1000000000)
                    {
                        mStats[MID_INT_COUNT_GB]+=mStats[MID_INT_COUNT]/1000000000;
                        mStats[MID_INT_COUNT] %= 1000000000;
                    }

                    mStats[MID_INT_COUNT_GB_LAST] = mStats[MID_INT_COUNT_GB];
                    mStats[MID_INT_COUNT_LAST] = mStats[MID_INT_COUNT];
                }

                oldRate = mStats[MID_INT_PER_SEC];
                mStats[MID_INT_PER_SEC]=(uint32)((double)newInts/
                                         ((double)(curSecond-lastSecond)));
                intrPerSec += mStats[MID_INT_PER_SEC];
                rateCh = mStats[MID_INT_PER_SEC] - oldRate;
                rateCh = rateCh < 0 ? -rateCh : rateCh;

                /* if new errors, or 5 percent rate change */
                if( ((rateCh > 10) && (rateCh > (oldRate/20) )) ||
                    ((mStats[MID_ERR] - mStats[MID_ERR_LAST])) )
                {
                    update = 1;
                }

                mStats[MID_ERR_LAST] = mStats[MID_ERR];

                if( mStats[MID_LAST_TIME] && ((mStats[MID_LAST_TIME] + PERIOD_KILL) < curSecond) )
                {
                    /* reset all info for member, so gone from printout */
                    PRINTIT(DM_CONF)("\n_CONF dT=%i: Server mId %d gone.", curSecond, k);
                    memset(mStats, 0, (NUM_STATS*sizeof(uint32)) );
                    update = 1;
                }

                /* update totals */
                for(j=0; j < NUM_STATS; j++)
                {
                    totals[j] += mStats[j];
                }

                if (totals[MID_INT_COUNT] >= 1000000000)
                {
                    totals[MID_INT_COUNT_GB]+=totals[MID_INT_COUNT]/1000000000;
                    totals[MID_INT_COUNT] %= 1000000000;
                }
            }

            if ( update || (errorCntLast != errorCnt) ||
                 ((lastUpdate + PERIOD_FORCED_REFRESH) < curSecond) )
            {
                printits+=gtnivReport(displayMode, midStats, totals);
                update = 0;
                lastUpdate = curSecond;
                errorCntLast = errorCnt;
            }

            for( i = 1, errArray[0]=0; i < NUM_ERRORS; i++)
                errArray[0] += errArray[i];

            SUMMARY;
            
            lastSecond = curSecond;

            if(iO->exitParms.timeToExit)
                goto end;
        }

        fflush(stdout);

    }while( 1 );

end:
    printf("\n");


    if( midStats )
        usysMemFree(midStats);
    if( expect )
        usysMemFree(expect);

    return;
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
        else if (ch == 'R')
            pparms->resetStats = 1;
        else if(ch == 'D')
            pparms->displayMode++;
        else if (ch == 'H')
            pparms->showHelp = 1;
        else if (ch == 'L')
            pparms->lockMask = 1;

        pparms->flag = 1;
    }
    return voidp;
}

/**************************************************************************/
/*  function:     buildOptionsString                                      */
/*  description:  Updates the options string                              */
/**************************************************************************/
static void buildOptionsString( char *optionsString, iOptionsType *iO )
{
    char *str = optionsString;
    int off = 0;

    off += sprintf(&str[off],
              "gtniv%s -u %u -d 0x%x -s %d -r %d -f 0x%x -b 0x%x -c %d",
              iO->doSend ? " -W":"", iO->unit, iO->displayMode,
              iO->seconds, iO->intrPerSec, iO->flags, iO->bcMask, iO->ucMask);

    if( iO->mId != -1 )
        off += sprintf(&optionsString[off]," -m %d", iO->mId);
}

/***************************************************************************/
/* Function:    parseCommand()                                             */
/* Description: Parses command line options                                */
/***************************************************************************/
static int parseCommand(int argc,char **argv,iOptionsType *iO,char *optionsString)
{
    char *endptr, nullchar = 0;
    int  i, j, len, tookParam=0;
                                     /* setup non-zero default options */
    iO->helpLevel = 1;      iO->intrPerSec = 1000;    iO->seconds = -1;
    iO->mId = -1;           iO->bcMask = 0xffffffff;  iO->ucMask = 1;
    iO->displayMode = 0xf;

    buildOptionsString(optionsString, iO);

    if(argc == 1)                                       /* start processing */
        return 0;

    for(i = 1; i < argc; i++, tookParam=0)
    {
        len = strlen(argv[i]);
        if((argv[i][0] != '-') || (len < 2))
        {
            printf("\nERROR: Unexpected option: \"%s\"\n\n", argv[i]);
            return -1;
        }

        if ( (argv[i][1] == '-') )
        {
            if( !strcmp( &argv[i][2], "version" ) )
            {
                printf("%s\n", APP_VER_STRING);
                printf(" - built with API revision %s\n", scgtapiRevisionStr);
                exit (0);
            }
            else if( !strcmp( &argv[i][2], "help" ) )
            {
                iO->helpLevel=2;
                return 0;
            }
        }
                                                       /* parse options */
        for(j=1; j<len; j++)          /* do options with arguments first*/
        {
            if( (j == (len-1)) && ((i+1) < argc))
            {
                tookParam = 1;  endptr = &nullchar;
                /* test for options which take parameters */
                /* these options only valid as last char of -string */
                if(     argv[i][j]=='u')
                    iO->unit=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='d')
                    iO->displayMode=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='b')
                    iO->bcMask=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='c')
                    iO->ucMask=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='m')
                    iO->mId=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='r')
                    iO->intrPerSec=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='s')
                    iO->seconds=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='f')
                    iO->flags=strtoul(argv[i+1],&endptr,0);
                else tookParam = 0;

                if( tookParam && (*endptr) )
                {
                    printf("\nInvalid parameter \"%s\" for option \"-%c\"\n\n",
                           argv[i+1], argv[i][j]);
                    return -1;
                }
            }
                                       /* options without arguments now */
            if(              argv[i][j]=='W') iO->doSend = 1;
            else if(tolower(argv[i][j])=='h')
            {
                iO->helpLevel=2;
                if(argc > 2)
                    iO->helpLevel=3;
                return 0;
            }
            else if(!tookParam)
            {
                printf("\nERROR: Unexpected option: \"-%s\"\n\n",&(argv[i][j]));
                return -1;
            }
        }

        if (tookParam) i++;
    }

    if(!iO->intrPerSec)
        iO->intrPerSec = 1;

    if( iO->ucMask )
        iO->ucMask = 1;

    buildOptionsString(optionsString, iO);
    iO->helpLevel=0;
    return 0;
}

/***************************************************************************/
/* Function:    printHelpLevel1()                                          */
/* Description: Display help and usage info                                */
/***************************************************************************/
static void printHelpLevel1(iOptionsType *iO, char *optionsString)
{
    printf("%s\n\n", APP_VER_STRING);
    printf("Usage: gtniv [-u unit] [-s seconds] [-d displayMode] [-f flags]\n"
           "             [-b bcastMask] [-c uintEnable]\n");
    printf("       gtniv -W [-u unit] [-s seconds] [-r intPerSec]\n"
           "             [-f flags] [-m memberId]\n\n");

    printf("Defaults: %s\n", optionsString);
#ifdef PLATFORM_VXWORKS
    printf("VxWorks users: Enclose options list in a set of quotes \" \".\n");
#endif
    printf("\n");
    if(iO->helpLevel>1)
        printHelpLevel2();
    if(iO->helpLevel>2)
        printHelpLevel3();
}

static void printHelpLevel2()
{
    printf(
"Common options:\n"
"  -h      - display this help menu\n"
"  -u #    - board/unit number\n"
"  -s #    - number of seconds to run test (default is forever).\n"
"  -b #    - broadcast mask. Selects the broadcast interrupts that the server\n"
"            will send, or the client will not ignore.\n"
"  -c #    - unicast mask. When 0, the server will not send unicast interrupts,\n"
"            or the client will ignore them.\n"
"Client options:\n"
"  -f #    - client flags (default is 0):\n"
"            0x2 - Randomly modify broadcast mask (only those in -b #)\n"
"  -d #    - display mode flags (default is 'all on'):\n"
"            0x1 - Summary line turned on and updated every second.\n"
"            0x2 - Display active servers. Updated on errors, rate changes.\n"
"            0x4 - Display error details.\n"
"            0x8 - Display extensive interrupt details on error.\n"
"Server options:\n"
"  -m #    - member ID to use for sending (default is current node ID)\n"
"  -W      - write/serve interrupts (default is to receive)\n"
"  -r #    - number of interrupts to send per second.\n"
"  -f #    - server flags (default is 0): 1 - Uneven interrupt distribution.\n"
);
    printf(
"Runtime options:\n"
"   q - quit    r - reset statistics   d - increment display mode\n"
"   h - dump statistics, etc   l - lock/unlock bcast mask (see client -f)\n"
);
    printf(
"Notes: Run `gtniv -h 1' for more information.\n");
}

static void printHelpLevel3()
{
    printf(
"\n"
"   Gtniv is a client/server type application for verifying the generation\n"
"and reception of GT network interrupts. Multiple instances of the client and\n"
"server can be executed at once on a network/node. Each server must have a\n"
"unique member ID (see -m). The member ID is passed as part of the value sent\n"
"with an interrupt. If two servers have the same member ID, many errors will be\n"
"detected by the client because the \"gtniv\" protocol will be broken.\n"
"\n");
    printf(
"THE GTNIV SERVER\n"
"   To run a gtniv server, specify option -W. Optionally specify an interrupts\n"
"per second rate with -r and a member ID with -m. Option -b can be used to give\n"
"a mask of the broadcast interrupts to actually send, and -c can be used to\n"
"specify whether or not to send the unicast interrupts. By default, each\n"
"enabled interrupt is sent an equal number of times (when all are enabled).\n"
"Use -f 1 to enable uneven (random) distribution of the sent interrupts.\n"
"In both cases, the ordering of the sent interrupts is random.\n"
"\n"
"The server output looks like the following:\n"
"\n"
"intr/sec  dTGT    #bursts   size       total delay rIntr/sec \n"
"    2000    +0      50.00     40   311403535    10     17555 \n"
"\n"
"The column headers are:\n"
"  intr/sec  - The number of interrupts directed to each node per second.\n"
"              A client will only receive those allowed by its HW and gtniv\n"
"              broadcast and unicast interrupt masks. All 256 unicast\n"
"              interrupts are sent at once and are treated as 1 interrupt.\n"
"  dTGT      - Delta target. The variance from the requested -r rate.\n"
"  #bursts   - The number of interrupt bursts directed to each node. A burst\n"
"              is a series of interrupts sent without intermediate delay.\n"
"  size      - The number of interrupts per burst\n"
"  total     - Running total of the number of interrupts sent.\n"
"  delay     - Millisecond delay inserted between bursts.\n"
"  rIntr/sec - Real intr/sec. The real number of interrupts sent per second by\n"
"              the server. Here the 256 unicast interrupts are treated as 256.\n"
"\n"
"Gtniv actively varies #bursts, size, and delay based on the intr/sec rate\n"
"from the previous reporting period in an attempt to keep intr/sec at close to\n"
"the specified -r rate as possible. Note that under some network conditions\n"
"it may be impossible to reach the desired rate.\n"
"\n");
    printf(
"   The gtniv server sends specially formatted values with the interrupts.\n"
"The values have the form 0xMMIIVVVV, where MM is the member ID, II is the\n"
"gtniv interrupt ID, and VVVV is a value associated with the interrupt ID.\n"
"There is one VVVV for each interrupt ID. It is incremented each time the\n"
"interrupt ID is sent. II is equal to the broadcast ID for broadcast interrupts\n"
"and is the destination node ID for unicast interrupts. The client uses all\n"
"this information for error detection.\n"
"\n");
    printf(
"THE GTNIV CLIENT\n"
"   By default, gtniv operates in client mode. Option -b can be used to give\n"
"a mask of the broadcast interrupts to verify and -c can be used to specify\n"
"whether or not to verify unicast interrupts. All are on by default. If\n"
"option -f 2 is specified, the HW broadcast mask will be varied during\n"
"operation (only those enabled by -b). The -d option can be used to alter the\n"
"output of the client.\n"
"\n"
"   Several runtime options are present. Use 'q' to stop the test, 'r' to\n"
"reset all statistics, 'h' to display test parameters & counts for all errors,\n"
"'d' to increment the display mode, and 'l' to lock/unlock broadcast mask\n"
"changes (when -f 2 is specified).\n"
"\n");
    printf(
"   The gtniv client checks for various errors, reported as ERR#, where # is\n"
"a decimal error code (view totals with runtime option 'h'). The errors are:\n"
"CODE   TYPE   DESCRIPTION\n"
"   0 -        total of all other errors\n"
"   1 - bcast  interrupts selected for verification not being received.\n"
"   2 - ucast  interrupts selected for verification not being received.\n"
"   3 -        unknown return code from scgtGetInterrupt\n"
"   4 - ucast  scgtInterrupt structure: sourceNodeID member invalid\n"
"   5 - ucast  client nodeID not equal to gtniv interrupt destination ID\n"
"   6 - ucast  unexpected VVVV value received\n"
"   7 - bcast  Received interrupt that was disabled in hardware mask.\n"
"   8 - bcast  scgtInterrupt structure: sourceNodeID member invalid\n"
"   9 - bcast  scgtInterrupt structure: id member invalid (rxID)\n"
"  10 - bcast  gtniv interrupt ID (txID) invalid\n"
"  11 - bcast  rxID doesn't match txID\n"
"  12 - bcast  unexpected VVVV value received\n"
"  13 - error  link-error interrupt received\n"
"  14 - error  driver-missed-interrupts interrupt received\n"
"  15 - error  unknown error interrupt id\n"
"  16 -        scgtInterrupt structure: type member invalid\n"
"  17 -        too slow, scgtGetInterrupt returned SCGT_MISSED_INTERRUPTS\n"
"  18 -        out of order sequence number in the scgtInterrupt structure.\n"
"Other codes are reserved.\n"
"\n");
    printf(
"When display mode flag -d 0x2 is enabled the client displays a list of active\n"
"servers. The list is updated when the rates change significantly, and at\n"
"10 minute intervals. The server list looks like the following:\n"
"\n"
"mId nId     count     error    missed  repeated  intr/sec   time\n"
" 67  67      2577         0         0         0       727      3\n"
" 68  67      3540         0         0         0      1000      3\n"
"TOTALS:      5182         0         0         0      1727      6\n"
"\n"
"where:\n"
"   mId      - member ID (-m) of the server\n"
"   nId      - node ID of the server\n"
"   count    - count of interrupts received from the server\n"
"   error    - number of errored interrupt received from the server\n"
"   missed   - number of interrupts from the server that have been missed\n"
"   repeated - number of interrupts from the server that were repeated\n"
"   intr/sec - interrupts per second receive rate\n"
"   time     - length of time the server has been active\n"
"The TOTALS line contains the totals for all servers.\n"
"\n");
    printf(
"The client's summary output looks like the following:\n"
"\n"
"dT=148461 I/s=1034 B=143954803 U=8997175 EM=0 EL=0 M=0 X=0 T=30 Ex=0\n"
"\n"
"where:\n"
"   dT  - elapsed time since test started (or was reset)\n"
"   I/s - average interrupts received per second since last update\n"
"   B   - broadcast interrupts received and processed\n"
"   U   - unicast interrupts received and processed\n"
"   EM  - driver-missed-interrupts type error interrupts\n"
"   EL  - link-error type error interrupts\n"
"   M   - interrupts missed by gtniv (can't keep up with driver and HW queue)\n"
"   X   - interrupts of unknown type received from the driver\n"
"   T   - number of calls to scgtGetInterrupt which have timed out\n"
"   Ex  - total errors detected by the application\n"
"\n");
    printf(
"   SCRAMNet GT devices have network interrupt filtering capability. In order\n"
"to receive network interrupts, interrupt filtering settings must be configured\n"
"appropriately. The relavent settings are the broadcast interrupt mask,\n"
"unicast interrupt enable, and self-interrupt enable.  Each of the settings\n"
"is configurable through the software API library, and through the gtmon\n"
"utility application. See your hardware and software documentation for\n"
"information regarding these settings, and the functionality they provide.\n"
"The gtniv application does not modify these settings unless -f 2 is specified\n"
"for the client, in which case the broadcast interrupt mask is varied.\n"
"\n");

return;

}

