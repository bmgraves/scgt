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

#define APP_VER_STRING "GT Latency (gtlat) rev. 1.03 (2011/09/07)"
#define APP_VxW_NAME gtlat

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
#include "systypes.h"                      /* this will get a path */   /**/
#include "usys.h"                          /* this will get a path */   /**/
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
            int argc; char argv0[]="gtlat"; char *argv[VXL]={argv0};    /**/
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
/******************* HARDWARE SPECIFIC INCLUDES ***************************/
/**************************************************************************/

#include "scgtapi.h"
#define HAND scgtHandle

/**************************************************************************/
/************** APPLICATION-SPECIFIC CODE STARTS HERE *********************/
/**************************************************************************/

/**************************************************************************/
/**************************  D A T A  T Y P E S  **************************/
/**************************************************************************/

typedef struct {
    uint32  unit, helpLevel, displayMode, samplesPerSec, screenUpdatePeriod;
    uint32  timeToExit, resetStats, secondsToRun, scaleType, doGrid, doZoom;
    uint32  forceLinkSpeed;
    double  usOscPeriod;
    HAND  hDriver;
} iOptionsType;

typedef struct {
    uint32             running;
    iOptionsType      *iO;
    usysThreadParams   pt;
} exitThreadParms, *pexitThreadParms;

/**************************************************************************/
/******************  F U N C T I O N  P R O T O T Y P E S  ****************/
/**************************************************************************/
/**************************************************************************/
/******************  F U N C T I O N  P R O T O T Y P E S  ****************/
/**************************************************************************/
static int parseCommand(int argc, char **argv, iOptionsType *iO, char *optionsString);
static void printHelpLevel1(iOptionsType *iO, char *optionsString);
static void printHelpLevel2();
static void printHelpLevel3();
static void *getRuntimeChar(void *voidp);
static int  gtlatBoss(iOptionsType *iO);
static int getOscillatorPeriod(iOptionsType *iO);

#define NUMERIC     1
#define ALLGRAPH    2

#define FORCE_1G    1
#define FORCE_2G    2
#define FORCE_25G   3

#define OSC_MHZ_1G        53.125
#define OSC_MHZ_2G       106.250
#define OSC_MHZ_25G      125.000
#define CLKUS    (iO->usOscPeriod) /* visable clock period in useconds */
#define SLB          (CLKUS/2.0)       /* byte time/period */
#define ITMAX_DEFINED  ((unsigned long)(900.0/CLKUS)) /*900 us is a lot */
#define APP_VARIANT ""
#define printResourceHeader(x)
#define DRIVER_SUCCESS SCGT_SUCCESS
#define DRIVER_OPEN(x,y)  { scgtDeviceInfo devInfo;                  \
        if (scgtOpen(x,y))                                           \
        {                                                            \
            printf("ERROR: could not open driver for unit %d\n", x); \
            return 0;                                                \
        }                                                            \
        scgtGetDeviceInfo(y, &devInfo);}
#define DRIVER_CLOSE(x) scgtClose(&(x));
/*get this node's id */
#define GET_HW_ID     (scgtGetState(&iO->hDriver, SCGT_NODE_ID))
#define GET_NET_LEN   (scgtGetState(&iO->hDriver, SCGT_RING_SIZE))
#define GET_NET_LATENCY     (scgtGetState(&iO->hDriver, SCGT_LATENCY_TIMER_VAL))


MAIN_IN                                                     /* gtlat main */
    iOptionsType   iO;
    int            j;
    char           optionsString[160];
    uint8*         tp;
    exitThreadParms exitParms;               /* for keyboard input thread */

    PARSER;    /* preparing argc and agrv if not provided by OS (VxWorks) */

    tp=(uint8 *)&iO;
    for(j=0;j < sizeof(iOptionsType);j++)        /* clear all the options */
        tp[j]=0;

    j = parseCommand(argc, argv, &iO, optionsString);  /* parsing options */

    if( j || iO.helpLevel!=0) /*1, 2, 3, ...*/
    {
        printHelpLevel1(&iO, optionsString);
        return j;
    }

    DRIVER_OPEN(iO.unit, (HAND *) &iO.hDriver);
    printf("Running: %s\n",optionsString);

    if( getOscillatorPeriod(&iO) )
    {
        DRIVER_CLOSE(iO.hDriver);                                     /* bye */
        return -1;
    }

    iO.timeToExit = iO.resetStats = 0;

    exitParms.pt.parameter = &exitParms;    /* spawn exit thread */
    exitParms.pt.priority = UPRIO_MED;
    sprintf(exitParms.pt.threadName, "gtlatExit");
    exitParms.running = 0;
    exitParms.iO = &iO;
    usysCreateThread(&(exitParms.pt), getRuntimeChar);
    /* Give thread a moment to start up. Needed on VxWorks, Solaris 8 */
    usysMsTimeDelay(50);

    j=gtlatBoss (&iO);                                  /* call the test */
    DRIVER_CLOSE(iO.hDriver);                                     /* bye */

    if( exitParms.running )
        usysKillThread(&(exitParms.pt));

    return 0;
}                                          /* end of MAIN_IN that is main */

/***************************************************************************/
/* Function:    gtlatBoss                                                  */
/* Description: Performs latency calculation and updates display           */
/***************************************************************************/
static int gtlatBoss (iOptionsType *iO)
{
    #define D_LIN 0
    #define D_LOG 1
    #define D_LOGLOG 2
    #define D_SQRT 3

    uint32 i,j;
    uint32 timeStart, scaleType;
    usysMsTimeType msTimer;

    #define HIS_SIZE 0x1fff
    uint32 iTmin, iTmax, iTnow, iScale, iTpeak;
    uint32 iLostCnt, iTmaxLost;
    uint32 *iHtmp = NULL;
    uint32 iHtmpSum;

    double sumT, aveT, stdT, medT, p99T, p01T, p33T, p66T;
    double ds;
    double *dH = NULL;
    double dHSum;
    double dHPick;

    #define PDSIZE (80)
    #define PDSIZE_V (20)
    char tmpStr[2*PDSIZE];
    char tmpStr2[2*PDSIZE];
    char spaceStr[PDSIZE];
    char wMark[8],eMark[8];

    double showH[PDSIZE];
    double showHmax;
    double minThing;
    uint32 iM, iMmax;
    uint32 iTw, iTe;

    /* must allocate large arrays so as not to blow vxworks stack */
    if( ((iHtmp = usysMemMalloc(sizeof(uint32)*HIS_SIZE)) == NULL) ||
        ((dH = usysMemMalloc(sizeof(double)*HIS_SIZE)) == NULL) )
    {
        printf("Histogram memory allocation failure\n");

        if(iHtmp)
            usysMemFree(iHtmp);
        if(dH)
            usysMemFree(dH);

        return -1;
    }

    for(i=0;i<PDSIZE;i++)
        spaceStr[i]=' ';
    sprintf(&spaceStr[PDSIZE-2]," ");

   /* the following need to be initialized for test start and restart */
    #define RESTART                \
    {for (i=0;i< HIS_SIZE;i++)     \
    {                              \
        iHtmp[i] = 0;              \
        dH[i] = 0.0;               \
    }                              \
    iTmin=~0;                      \
    iTpeak = iMmax = iTmax = iLostCnt = iTmaxLost = iO->resetStats = 0; \
    dHSum = 0.0;}

    RESTART;

    timeStart = time(NULL);

    while (!iO->timeToExit)                              /* Repeat forever. */
    {
        if(iO->resetStats)
            RESTART;
        iHtmpSum = 0;
        usysMsTimeStart(&msTimer);
                                                          /* data gathering */
        while( usysMsTimeGetElapsed(&msTimer) < iO->screenUpdatePeriod)
        {
            if((iO->resetStats) || (iO->timeToExit))
                break;
            usysMsTimeDelay(1000/iO->samplesPerSec);
            iTnow=GET_NET_LATENCY;
            if((iTnow >= HIS_SIZE) || (iTnow >= ITMAX_DEFINED))
            {
                iLostCnt++;
                if(iTnow > iTmaxLost)
                    iTmaxLost=iTnow;
                iTnow = iTmax;                       /* use the current max */
            }
            iHtmp[iTnow]++;
            iHtmpSum++;            /* good for few seconds before overflows */
            if(iTnow > iTmax)
                iTmax = iTnow;
            if(iTnow < iTmin)
                iTmin = iTnow;
        }

        usysMsTimeStop(&msTimer);

        dHSum = dHSum + (double)iHtmpSum;             /* total sample count */
        for (i=iTmin, sumT=0.0, dHPick=0.0; i<= iTmax; i++)
        {
            dH[i] = dH[i] + (double)iHtmp[i]; /* change type and add to old */
            iHtmp[i] = 0;                   /* clear tmp for the next round */
            sumT = sumT + dH[i] * (double)i;          /* total aquired time */
            if( dH[i]>dHPick)
            {
                dHPick=dH[i];
                iTpeak=i;
            }
        }
        aveT = sumT / dHSum;

        for (i=iTmin, ds=medT=p99T=p01T=p33T=p66T=.0; i<= iTmax; i++) /* stats */
        {
            if(p99T != 0.0)
                break;
            ds= ds + dH[i];
            if((p01T == 0) && (ds >= dHSum * 0.01))
                p01T = (double)i;
            else if((p33T == 0) && (ds >= dHSum*0.33))
                p33T = (double)i;
            else if((medT == 0) && (ds >= dHSum/2.0))
                medT = (double)i;
            else if((p66T == 0) && (ds >= dHSum*0.66))
                p66T = (double)i;
            if(ds >= dHSum * 0.99)  /* must get p99T for zooming, printing*/
                p99T = (double)i;
        }

        /* calculate standard deviation */
        for (i=iTmin, stdT = 0.0; i<= iTmax; i++)    /* second order stats */
        {
            stdT = stdT + dH[i] * ((double)i - aveT) * ((double)i - aveT);
        }
        stdT = sqrt(stdT / (dHSum-1.0));

        /*************** screen presentation part **************/
        for (i=0;i< PDSIZE;i++)             /* clean histogram array image */
            showH[i] = 0.0;

        /* choose horizontal bounds */
        if(iO->doZoom != 0)
        {
            iTw=(uint32)p01T;
            iTe=(uint32)p99T;
            sprintf(wMark,"1%%");
            sprintf(eMark,"99%%");
        }
        else
        {
            iTw=iTmin;
            iTe=iTmax;
            sprintf(wMark,"min");
            sprintf(eMark,"max");
        }

        /* squeeze or stretch values horizontally to fit window */
        iScale=iTe-iTw+1;
        for (i=iTw; i<= iTe; i++)
        {
            showH[((i-iTw)*PDSIZE)/iScale] += dH[i];
        }

        scaleType = iO->scaleType;

        /* scale values vertically based on selected scaling function */
        for (i=0, showHmax = 0; i<PDSIZE; i++)
        {
            if(scaleType == D_LIN)
                showH[i]=(double)PDSIZE_V*(double)PDSIZE_V*1000.0*showH[i];
            else if(scaleType == D_LOG)
                showH[i]=(double)PDSIZE_V*(double)PDSIZE_V*1000.0*log10(showH[i] + 1.);
            else if(scaleType == D_LOGLOG)
                showH[i]=(double)PDSIZE_V*(double)PDSIZE_V*1000.0*log10(log10(showH[i] + 1.)+1.);
            else if(scaleType == D_SQRT)
                showH[i]=(double)PDSIZE_V*(double)PDSIZE_V*1000.0*sqrt(showH[i]);
            else
                showH[i]=(double)PDSIZE_V*(double)PDSIZE_V*1000.0*sqrt(showH[i]);
            if(showH[i] > showHmax)
                showHmax = showH[i];
        }
        if(((time(NULL)-timeStart) >= iO->secondsToRun) || (iO->timeToExit !=0))
        {
            iO->displayMode |= ALLGRAPH | NUMERIC;
            iO->timeToExit |=1;
        }

        /* squeeze or stretch values vertically to fit window, and print */
        if((iO->displayMode & ALLGRAPH) != 0)
        {
            for (i=0;i<=PDSIZE_V;i++)
            {
                printf("\n");
                for (j=0;j<PDSIZE;j++)
                {
                    if(showH[j] >= (((double)(PDSIZE_V-i)/(double)PDSIZE_V) * showHmax) )
                    {
                        if(j== (uint32)((double)PDSIZE * (p01T-iTw)/(double)iScale))
                            printf("1");
                        else if(j== (uint32)((double)PDSIZE * (p33T-iTw)/(double)iScale))
                            printf("3");
                        else if(j== (uint32)((double)PDSIZE * (aveT-iTw)/(double)iScale))
                            printf("A");
                        else if(j== (uint32)((double)PDSIZE * (medT-iTw)/(double)iScale))
                            printf("M");
                        else if(j== (uint32)((double)PDSIZE * (p66T-iTw)/(double)iScale))
                            printf("6");
                        else if(j== (uint32)((double)PDSIZE * (p99T-iTw)/(double)iScale))
                            printf("9");
                        else
                        {
                            if(i==PDSIZE_V)
                            {
                                if(showH[j] > 0)
                                    printf("o");
                                else
                                    printf("-");
                            }
                            else
                                printf("^");
                        }
                    }
                    else
                    {
                        if( (((j% 4) == 0) || ((i% 4) == 0)) && (iO->doGrid != 0))
                            printf(".");
                        else
                            printf(" ");
                    }
                }
            }
            fflush(stdout);
        }

        if((iO->displayMode & (NUMERIC | ALLGRAPH)) != 0)
        {
            printf("\n");
            minThing=(double)iTw*CLKUS/SLB;
            sprintf(tmpStr,"%.2f [us] =%s= %.0f [B]  <--- %s ",
                (double)iTw*CLKUS, wMark,(double)iTw*CLKUS/SLB,spaceStr);
            sprintf(&tmpStr[PDSIZE/2 - 2],"%.0f [B] %s",
            (double)iTe*CLKUS/SLB-minThing,spaceStr);
            sprintf(tmpStr2,"---> %.0f [B] =%s= %.2f [us]",
                (double)iTe*CLKUS/SLB, eMark, (double)iTe*CLKUS);
            sprintf(&tmpStr[PDSIZE - strlen(tmpStr2)],"%s",tmpStr2);
            printf("%s\n",tmpStr);
            if((iM=GET_NET_LEN) > iMmax)
                iMmax=iM;
            if(scaleType == D_LIN)
                printf("LI");
            else if(scaleType == D_LOG)
                printf("LG");
            else if(scaleType == D_LOGLOG)
                printf("LGLG");
            else if(scaleType == D_SQRT)
                printf("SQRT");
            else
                printf("???");
#if 0
            printf(" R=%d/%d nId=%d", iM, iMmax , GET_HW_ID);
            printf(" Ave=+%.0f", aveT*CLKUS/SLB-minThing);
            printf(" Med=+%.0f", medT*CLKUS/SLB-minThing);
            printf(" std=%.0f", stdT*CLKUS/SLB);
            printf(" 99%%=+%.0f", p99T*CLKUS/SLB-minThing);
            printf(" Peak@+%.0f", (double)iTpeak*CLKUS/SLB-minThing);
            printf(" popCnt=+%.1e", dHSum);
#else /* let printf select leading + or - as appropriate */
            printf(" R=%d/%d nId=%d", iM, iMmax , GET_HW_ID);
            printf(" Ave=%+.0f", aveT*CLKUS/SLB-minThing);
            printf(" Med=%+.0f", medT*CLKUS/SLB-minThing);
            printf(" std=%.0f", stdT*CLKUS/SLB);
            printf(" 99%%=%+.0f", p99T*CLKUS/SLB-minThing);
            printf(" Peak@%+.0f", (double)iTpeak*CLKUS/SLB-minThing);
            printf(" popCnt=%+.1e", dHSum);
#endif
            if(iLostCnt > 0)
                printf("\nLate messages Count=%d, the latest at %.0f [us]",
                        (unsigned int)iLostCnt,(double)iTmaxLost * CLKUS);
            fflush(stdout);
        }
    }
    printf("\n");

    if(iHtmp)
        usysMemFree(iHtmp);
    if(dH)
        usysMemFree(dH);

    return 1;
}

/***************************************************************************/
/* Function:    getRuntimeChar                                             */
/* Description: Thread that reads keyboard                                 */
/***************************************************************************/
static void *getRuntimeChar(void *voidp)
{
    exitThreadParms *eP = (exitThreadParms*) voidp;
    iOptionsType *iO = eP->iO;
    char ch;
    int ich;
    eP->running = 1;
    while (!(iO->timeToExit))
    {
        if ((ich = getchar()) == EOF)
            break;
            
        ch = toupper(ich); /* toupper may be a macro */

        if (ch == 'Q')
        {
            eP->running = 0;
            iO->timeToExit = 1;
        }
        if (ch == 'R')
            iO->resetStats = 1;
        else if (ch == 'S')
            iO->scaleType = (iO->scaleType +1) % 4;
        else if (ch == 'G')
            iO->doGrid = (iO->doGrid + 1) % 2;
        else if (ch == 'Z')
            iO->doZoom = (iO->doZoom + 1) % 2;
    }
    eP->running = 0;
    return voidp;
}

/***************************************************************************/
/* Function:    diffCounter()                                              */
/* Description: Calculates difference between current counter value and    */
/*    previous counter value, accounting for (at most 1) roll-over.        */
/***************************************************************************/
static uint32 diffCounter(uint32 currCnt, uint32 lastCnt, uint32 maxCnt)
{
    if( currCnt >= lastCnt)
        return ( currCnt - lastCnt );
    else /* overflow */
        return ( currCnt + ( maxCnt - lastCnt ) + 1 );
}

/***************************************************************************/
/* Function:    getOscillatorPeriod()                                      */
/* Description: Calculate info related to linkSpeed and PCI frequency      */
/***************************************************************************/
static int getOscillatorPeriod(iOptionsType *iO)
{
    usysMsTimeType curTime;
    uint32 oscTicks[2];
    uint32 startMS1, startMS2, stopMS1, stopMS2, elapsedMS;
    uint32 val;

    if( iO->forceLinkSpeed )
    {
        if( iO->forceLinkSpeed == FORCE_1G )
            iO->usOscPeriod = (1.0/OSC_MHZ_1G);
        else if( iO->forceLinkSpeed == FORCE_2G )
            iO->usOscPeriod = (1.0/OSC_MHZ_2G);
        else if( iO->forceLinkSpeed == FORCE_25G )
            iO->usOscPeriod = (1.0/OSC_MHZ_25G);
        else
        {
            printf("Invalid force-link-speed value\n");
            return -1;
        }

        return 0;
    }

    usysMsTimeStart(&curTime);
    usysMsTimeDelay(1);

    /* read counters */
    startMS1 = usysMsTimeGetElapsed(&curTime);
    oscTicks[0] = scgtReadCR(&iO->hDriver, 0x40);
    startMS2 = usysMsTimeGetElapsed(&curTime);

    /* Large delay because we want accurate results in gtlat,
       time not an issue. Don't go too high, as can't handle
       multiple rollovers */
    usysMsTimeDelay(200); 

    /* read counters */
    stopMS1 = usysMsTimeGetElapsed(&curTime);
    oscTicks[1] = scgtReadCR(&iO->hDriver, 0x40);
    stopMS2 = usysMsTimeGetElapsed(&curTime);

    /* try for accurate elapsed time */
    elapsedMS = (stopMS2 + stopMS1)/2 - (startMS2 + startMS1)/2;

    if( elapsedMS == 0 ) /* prevent divide-by-zero */
        elapsedMS = 1;

    usysMsTimeStop(&curTime);

    oscTicks[0] = diffCounter(oscTicks[1], oscTicks[0], 0xFFFFFFFF);

    /* calculate oscillator frequency */
    val = (oscTicks[0]/elapsedMS)/1000; /* MHz */

    /* ~+-20 Mbps from actual, short delay used for calculation atop system
      scheduling yields inaccurate results*/
    if( (val >= 33) && (val < 73) ) /* 53.125 MHz actual frequency */
    {
        printf("!!!! 1.0 Gbps Link Speed Detected !!!!\n");
        iO->usOscPeriod = (1.0/OSC_MHZ_1G); /* .01882 us*/
    }
    else if( (val > 86) && (val < 116) ) /* 106.250 MHz actual frequency */
    {
        printf("!!!! 2.0 Gbps Link Speed Detected !!!!\n");
        iO->usOscPeriod = (1.0/OSC_MHZ_2G); /* .00941 us*/
    }
    else if( (val >= 116) && (val < 145) ) /* 125.000 MHz actual frequency */
    {
        printf("!!!! 2.5 Gbps Link Speed Detected !!!!\n");
        iO->usOscPeriod = (1.0/OSC_MHZ_25G); /* .008 us*/
    }
    else
    {
        printf("!!!! Unable to determine oscillator frequency (see -z) !!!!\n");
        return -1;
    }
    return 0;
}

/**************************************************************************/
/*  function:     buildOptionsString                                      */
/*  description:  Getting argv details into internal usable form          */
/**************************************************************************/
static void buildOptionsString(iOptionsType *iO, char *optionsString)
{
    sprintf(optionsString, "gtlat -u %d -r %d -p %d -d %d -s %d",
            iO->unit, iO->samplesPerSec, iO->screenUpdatePeriod,
            iO->displayMode, iO->secondsToRun);
}

/**************************************************************************/
/*  function:     parseCommand                                            */
/*  description:  Getting argv details into internal usable form          */
/**************************************************************************/
static int parseCommand(int argc,char **argv,iOptionsType *iO,char *optionsString)
{
    char *endptr, nullchar = 0;
    int  len, tookParam=0;
    int i,j;                /* setting defaults - 0's are set in the main */
    iO->samplesPerSec = 1000;
    iO->screenUpdatePeriod = 1000;
    iO->displayMode = NUMERIC | ALLGRAPH;
    iO->secondsToRun = ~0;
    iO->helpLevel=1;

    buildOptionsString(iO, optionsString); /* defaults */

    if(argc == 1)                          /* start processing */
        return 0;
    for(i = 1; i < argc; i++, tookParam=0)
    {
        len = (int) strlen(argv[i]);
        if((argv[i][0] != '-') || (len < 2))
        {
            printf("\nERROR: Unexpected option: \"%s\"\n\n", argv[i]);
            iO->helpLevel=1;
            return (iO->helpLevel=1, -1);
        }

        if ( (argv[i][1] == '-') )
        {
            if( !strcmp( &argv[i][2], "version" ) )
            {
                printf("%s\n", APP_VER_STRING);
                printf(" - built with API revision %s\n", scgtGetApiRevStr());
                exit (0);
            }
            else if( !strcmp( &argv[i][2], "help" ) )
            {
                iO->helpLevel=2;
                return 0;
            }
        }

        for(j=1; j<len; j++) /* do options with arguments first*/
        {
            if( (j == (len-1)) && ((i+1) < argc))
            {
                tookParam = 1;  endptr = &nullchar;
                /* test for options which take parameters */
                /* these options only valid as last char of -string */
                if(     argv[i][j]=='u') iO->unit=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='d') iO->displayMode = strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='r') iO->samplesPerSec = strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='p') iO->screenUpdatePeriod = strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='s') iO->secondsToRun = strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='z') iO->forceLinkSpeed = strtoul(argv[i+1],&endptr,0);
                else tookParam = 0;

                if( tookParam && (*endptr))
                {
                    printf("\nInvalid parameter \"%s\" for option \"%c\"\n\n", argv[i+1], argv[i][j]);
                    return (iO->helpLevel=1, -1);
                }
            }
            /* options without arguments now */
            if(tolower(argv[i][j])=='h')
            {
                iO->helpLevel=2;
                if(argc > 2) iO->helpLevel=3;
                return 0;
            }
            else if(!tookParam)
            {
                printf("\nERROR: Unexpected option: \"%c\"\n\n",argv[i][j]);
                return (iO->helpLevel=1, -1);
            }
        }
        if (tookParam) i++;
    }
    buildOptionsString(iO, optionsString);
    return (iO->helpLevel=0, 0);
}

/**************************************************************************/
/*  function:     printHelpLevel1                                         */
/*  description:  Print hints and call more help if needed                */
/**************************************************************************/
static void printHelpLevel1( iOptionsType *iO, char *optionsString)
{
    printf( "%s %s\n\n", APP_VER_STRING, APP_VARIANT );
    printf("Usage: gtlat [-u unit] [-r samplesPerSec] [-p screenUpdatePeriod]\n"
           "             [-d displayMode] [-s secondsToRun] [-z forceLinkSpeed]\n"
           "             [-h]\n\n");
    printf("Defaults: %s\n",optionsString);
#ifdef PLATFORM_VXWORKS
    printf("VxWorks users: Enclose options list in a set of quotes \" \".\n");
#endif
    printf("\n");
    if(iO->helpLevel>1)
        printHelpLevel2();
    if(iO->helpLevel>2)
        printHelpLevel3();
}

/**************************************************************************/
/*  function:     printHelpLevel2                                         */
/*  description:  Print more hints                                        */
/**************************************************************************/
static void printHelpLevel2()
{
    printf(
"Options:\n"
"  -u # - board/unit number (default 0)                     \n"
"  -p # - screen update period in [ms]  \n"
"  -r # - sampling rate in [samples/s]  \n"
"  -s # - seconds to run  \n"
"  -d # - display mode positionally coded:  \n"
"            1 - numerical data only 2 - whole graph \n"
"  -z # - force calculations for link speed of: \n"
"            1 - 1.0 Gbps, 2 - 2.0 Gbps, 3 - 2.5 Gbps \n");
    printf(
"\nRuntime options (followed by <Enter>):\n"
"   q - quit,           g - grid toggle,\n"
"   s - scale toggle,   z - zoom toggle,   r - reset\n");
    printf(
"\nExample: gtlat -u 0    - use defaults on unit 0\n"
"Note: Run `gtlat -h 1' for more information.\n\n");
}


/**************************************************************************/
/*  function:     printHelpLevel3                                         */
/*  description:  Print detailed hints                                    */
/**************************************************************************/
static void printHelpLevel3()
{
    printf(
"   The gtlat utility collects network latency information and performs\n"
"statistical analysis of the data.  The data is presented both numerically\n"
"and in the form of a histogram.\n"
"\n");
    printf(
"Example output:"
"\n"
"       3\n"
"       3\n"
"       3\n"
"       3\n"
"       3\n"
"       3\n"
"       3\n"
"       3\n"
"       3\n"
"       3       M\n"
"       3       M\n"
"       3       M\n"
"       3       M\n"
"       3       M                                                    ^\n"
"       3       M                                                    ^\n"
"       3       M                                            ^       ^\n"
"1      3       M                                            ^       ^\n"
"1      3       M                                     ^      ^       ^\n"
"1      3       M      ^                              ^      ^       ^\n"
"1      3       M      ^       6       ^      ^       ^      ^       ^       9\n"
"1------3-------M------o-----A-6-------o------o-------o------o-------o-------9---\n"
"4.50 [us] =min= 478 [B]  <---         40 [B]        ---> 518 [B] =max= 4.88 [us]\n"
"LI R=3/4 nId=1 Ave=+15 Med=+8 std=13 99%%=+40 Peak@+4 popCnt=+7.2e+02\n"
"\n");
    printf(
"   The histogram output, y=f(x), provides a proportional representation of\n"
"the frequency of occurance for the sampled link latency values. The histogram\n"
"can also be displayed in non-linear scale (see below). In the example above,\n"
"the line beginning \"4.50 [us]\" says the minimum latency represented is\n"
"4.50 microseconds, which is equivalent to 478 bytes of data.  The maximum\n"
"latency represented is 4.88 microseconds, or 518 bytes, and the horizontal\n"
"range covers 40 bytes.\n");
    printf(
"The following characters are used as line markers in the histogram:\n"
"  1 =  1%% of all samples were at or below this latency.\n"
"  3 = 33%% of all samples were at or below this latency.\n"
"  6 = 66%% of all samples were at or below this latency.\n"
"  9 = 99%% of all samples were at or below this latency.\n"
"  M = the statistical median.\n"
"  A = the statistical average.\n"
"  o = samples having this latency value were seen.\n"
"  ^ = line marker at other latencies.\n"
"\n");
    printf(
"   In the example above, the line beginning \"LI R=4/4\" says that the\n"
"vertical lines are proportioned on a linear (LI) scale. The scale can be\n"
"changed during execution by typing 's <Enter>'.  The scales are\n"
"   LI   = linear scale,             y = f(x)\n"
"   LG   = logarithmic scale,        y = log10(f(x))\n"
"   LGLG = double logarithmic scale, y = log10(log10(f(x)))\n"
"   SQRT = square root scale,        y = sqrt(f(x))\n"
"\n");
    printf(
"   This line also provides information on network ring size and node ID.\n"
"R=3/4 means the current ring contains 3 nodes, and that the largest ring\n"
"seen since sampling began contained 4 nodes.  nId=1 indicates this test is\n"
"executing on a node having node ID of 1. The following info is also provided:\n"
"   Ave    = average latency\n"
"   Med    = median latency\n"
"   std    = standard deviation\n"
"   99%%    = 99 percent of samples were below this latency\n"
"   Peak@  = the most frequently occurring latency, peak\n"
"   popCnt = number of samples collected\n"
"Ave, Med, 99%%, and Peak@ are all relative to the minimum latency.  So above,\n"
"Med=+8 means that Med=478+8=486.\n"
"\n");
    printf(
"   Executing gtlat on a idle network will produce idle results. Latency\n"
"testing should be conducted under expected operating conditions as link\n"
"utilization will affect the latency. The gtlat utility does not modify device\n"
"settings, and does not generate link/network traffic.\n"
"\n");
}
