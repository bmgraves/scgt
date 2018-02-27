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

#define APP_VER_STRING "GT Interrupt Test (gtint) rev. 1.03 (2011/08/30)"
#define APP_VxW_NAME gtint

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
            int argc; char argv0[]="gtint"; char *argv[VXL]={argv0};    /**/
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
    uint32  msPeriod, unit, helpLevel, displayMode, doSend, doSendAll; \
    uint32  destID, numTimes, value, seconds, doTimed;

typedef struct {
    volatile uint32 timeToExit;
    volatile uint32 resetStats;
    volatile uint32 toggleUcast;
    volatile uint32 toggleBcast;
    volatile uint32 toggleErr;
    volatile uint32 flag;
    volatile uint32 running;
    usysThreadParams  pt;
} exitThreadParms;

typedef struct
{
    INPUT_OPTIONS
    scgtHandle hDriver;
    exitThreadParms exitParms;
} iOptionsType;

#define INTS_BCAST     1
#define INTS_UCAST     2
#define INTS_ERR       4
#define INTS_UNKNOWN   8

static void buildOptionsString( char *optionsString, iOptionsType *iO );
static void parseCommand(int argc, char **argv, iOptionsType *iO,
                         char *optionsString);
static void printHelpLevel1(iOptionsType *iO, char *optionsString);
static void printHelpLevel2(void);
static void printHelpLevel3(void);
static void *getRuntimeChar(void *voidp);
static void gtintSendAllInterrupts(iOptionsType *iO);
static void gtintSendInterrupts(iOptionsType *iO);
static void gtintPrintInterrupts(iOptionsType *iO);

/**************************************************************************/
/************************* G T I N T   C O D E ****************************/
/**************************************************************************/

MAIN_IN

    iOptionsType iO;
    char optionsString[120];
    int i, iteration = 0;
    void (*func)(iOptionsType *iO);
    usysMsTimeType curTime;
    uint32 curSecond, lastSecond;

    PARSER;        /* prepare argc and agrv if not provided by OS (VxWorks) */

    parseCommand(argc, argv, &iO, optionsString);        /* parsing options */

    if(iO.helpLevel!=0) /*1, 2, 3, ...*/
    {
        printHelpLevel1(&iO, optionsString);
        return 0;
    }

    if (scgtOpen(iO.unit, &iO.hDriver) != SCGT_SUCCESS)
    {
        printf("gtint: could not open unit %i\n", iO.unit);
        return -1;
    }

    memset( &iO.exitParms, 0, sizeof(exitThreadParms) );

    if( iO.seconds )
    {                                              /* spawn exit thread */
        iO.exitParms.pt.parameter = &iO.exitParms;
        iO.exitParms.pt.priority = UPRIO_HIGH; /* only use UPRIO_HIGH */
        sprintf(iO.exitParms.pt.threadName, "gtintExit");
        usysCreateThread(&iO.exitParms.pt, getRuntimeChar);
        /* Give thread a moment to start up. Needed on VxWorks, Solaris 8 */
        usysMsTimeDelay(50);
    }

    if( !iO.doSendAll && !iO.doSend )
    {
        gtintPrintInterrupts(&iO);
    }
    else
    {
        if( iO.doSendAll )
        {
            func = gtintSendAllInterrupts;
        }
        else /* iO.doSend */
        {
            func = gtintSendInterrupts;
        }

        usysMsTimeStart(&curTime);

        lastSecond = curSecond = 0;

        do
        {
            if ( iO.seconds && (lastSecond < curSecond))
            {
                printf("\rRunning(%d): %s", iteration, optionsString);
                fflush(stdout);
                lastSecond = curSecond;
            }
            
            func(&iO);

            curSecond = (usysMsTimeGetElapsed(&curTime) / 1000);

            /* if timed and not "run forever" and time met or exceeded */
            if( iO.seconds && (iO.seconds != -1) && (curSecond >= iO.seconds) )
            {
                iO.exitParms.timeToExit = 1;
            }
    
            if( iO.msPeriod && iO.seconds)
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

            iteration++;
    
        }while( iO.seconds && !iO.exitParms.timeToExit );

        usysMsTimeStop(&curTime);

        if ( iO.seconds )
            printf("\rRunning(%d): %s\n", iteration, optionsString);
    }

    if( iO.exitParms.running )
    {   
        usysKillThread(&iO.exitParms.pt);
    }

    scgtClose(&iO.hDriver);
    return 0;
}

/***************************************************************************/
/* Function:    gtintSendAllInterrupts                                     */
/* Description: Sends one of each interrupt iO->numTimes times             */
/***************************************************************************/

static void gtintSendAllInterrupts(iOptionsType *iO)
{
    scgtInterrupt uIntr, bIntr;
    uint32 i, j;
    int sendBcast = 0, sendUcast = 0;

    if( !iO->doSend || (iO->doSend == INTS_UCAST))
    {
        sendUcast = 1;
        uIntr.type = SCGT_UNICAST_INTR;
        uIntr.val = iO->value;
    }

    if( !iO->doSend || (iO->doSend == INTS_BCAST))
    {
        sendBcast = 1;
        bIntr.type = SCGT_BROADCAST_INTR;
        bIntr.val = iO->value;
    }

    for( i = 0; (i < iO->numTimes) && !iO->exitParms.timeToExit; i++ )
    {
        if( sendBcast ) /* send broadcast interrupts */
        {
            for( j=0; j < 32; j++ )
            {
                bIntr.id = j;
                scgtWrite(&iO->hDriver, 0, NULL, 0, 0, NULL, &bIntr);
            }
        }

        if( sendUcast ) /* send Unicast interrupts */
        {
            for( j=0; j < 256; j++ )
            {
                uIntr.id = j;
                scgtWrite(&iO->hDriver, 0, NULL, 0, 0, NULL, &uIntr);
            }
        }
    }
}

/***************************************************************************/
/* Function:    gtintSendInterrupts                                        */
/* Description: Sends requested interrupts                                 */
/***************************************************************************/

static void gtintSendInterrupts(iOptionsType *iO)
{
    scgtInterrupt intr;
    uint32 i;

    if( iO->doSend == INTS_UCAST )
    {
        if( iO->destID > 256 )
        {
            printf("Invalid unicast destination id (%i).\n", iO->destID);
            return;
        }

        intr.type = SCGT_UNICAST_INTR;
        intr.id   = iO->destID % 256;
        intr.val  = iO->value;
    }
    else /* INTS_BCAST */
    {
        if( iO->destID > 31 )
        {
            printf("Invalid broadcast interrupt id (%i).\n", iO->destID);
            return;
        }

        intr.type = SCGT_BROADCAST_INTR;
        intr.id   = iO->destID % 32;
        intr.val  = iO->value;
    }

    for( i = 0; (i < iO->numTimes) && !iO->exitParms.timeToExit; i++ )
    {
        scgtWrite(&iO->hDriver, 0, NULL, 0, 0, NULL, &intr);
    }
}

/***************************************************************************/
/* Function:    gtintPrintHeader                                           */
/* Description: Displays current interrupt mask info                       */
/***************************************************************************/
static void gtintPrintHeader(iOptionsType *iO)
{
    uint32 bcastMask, unicastOn, selfintOn;

    bcastMask = scgtGetState(&iO->hDriver, SCGT_BROADCAST_INT_MASK);
    unicastOn = scgtGetState(&iO->hDriver, SCGT_UNICAST_INT_MASK);
    selfintOn = scgtGetState(&iO->hDriver, SCGT_INT_SELF_ENABLE);

    printf("TYPE  SID IID VALUE      COUNT     bint=%#8x  uint=%i  sint=%i\n",
            bcastMask, unicastOn, selfintOn);
}


/***************************************************************************/
/* Function:    gtintPrintInterrupts                                       */
/* Description: Displays received interrupts                               */
/***************************************************************************/

static void gtintPrintInterrupts(iOptionsType *iO)
{
    #define INTR_PER_CALL  512
    scgtIntrHandle hIntr = -1;
    scgtInterrupt intrs[INTR_PER_CALL];
    char *str = "";
    usysMsTimeType curTime;
    uint32 curSecond = 0, lastSecond = -1, ret = 0;
    uint32 ucastCnt = 0,  bcastCnt = 0,   errorCnt = 0;
    uint32 drvMissedCnt = 0, drvLinkErrCnt = 0;
    uint32 appMissedCnt = 0, timeoutCnt = 0, i;
    uint32 numIntrs = 0,  printed = 0;
    uint32 error = 0,     cnt = 0,        type = 0, cr = 0;

    iO->displayMode |= INTS_UNKNOWN;
    usysMsTimeStart(&curTime);

    do
    {
        ret = scgtGetInterrupt(&iO->hDriver, &hIntr, intrs,
                INTR_PER_CALL, 1000, &numIntrs);

        if ( ret == SCGT_TIMEOUT )
        {
            timeoutCnt++;
        }
        else if ( ret == SCGT_MISSED_INTERRUPTS )
        {
            if( iO->displayMode & 0x7 )
                printf("\nMissed interrupts!!!!\n");

            appMissedCnt++;
        }
        else if( ret )
        {
            printf("%s\n", scgtGetErrStr(ret));
        }

        if( iO->exitParms.flag )
        {
            iO->exitParms.flag = 0;

            if( iO->exitParms.resetStats )
            {
                printf("%78s\r", "");
                usysMsTimeStop(&curTime);
                usysMsTimeStart(&curTime);
                lastSecond = -1;
                curSecond = 0;
                errorCnt = bcastCnt = ucastCnt = appMissedCnt = 0;
                timeoutCnt = drvMissedCnt = drvLinkErrCnt = 0;
                iO->exitParms.resetStats = 0;
                continue;
            }
    
            if( iO->exitParms.toggleUcast )
            {
                iO->displayMode ^= INTS_UCAST;
                iO->exitParms.toggleUcast = 0;
            }
    
            if( iO->exitParms.toggleBcast )
            {
                iO->displayMode ^= INTS_BCAST;
                iO->exitParms.toggleBcast = 0;
            }
    
            if( iO->exitParms.toggleErr )
            {
                iO->displayMode ^= INTS_ERR;
                iO->exitParms.toggleErr = 0;
            }
        }

        for( i = 0; i < numIntrs; i++ )
        {
            error = 0;

            switch( intrs[i].type )
            {
                case SCGT_UNICAST_INTR:
                    type = INTS_UCAST;
                    cnt = (++ucastCnt);
                    str = "Ucast";

                    if( intrs[i].sourceNodeID > 255 )
                    {
                        error = 1;
                    }

                    break;

                case SCGT_BROADCAST_INTR:
                    type = INTS_BCAST;
                    cnt = (++bcastCnt);
                    str = "Bcast";

                    if( (intrs[i].sourceNodeID > 255) || (intrs[i].id > 31) )
                    {
                        error = 1;
                    }

                    break;

                case SCGT_ERROR_INTR:
                    type = INTS_ERR;
                    str = "Err";
                    cnt = (++errorCnt);

                    if( intrs[i].val == SCGT_LINK_ERROR )
                        drvLinkErrCnt++;
                    else if( intrs[i].val == SCGT_DRIVER_MISSED_INTERRUPTS )
                        drvMissedCnt++;

                    break;

                default:
                    type = INTS_UNKNOWN;
                    str = "???";
                    error = 1;
                    break;
            }

            if( iO->displayMode & type )
            {
                if( cr )
                {
                    printf("\n");
                    cr = 0;
                }

                if( !(printed%15) )
                {
                    gtintPrintHeader(iO);
                }

                printed++;

                printf("%-5s %3d %3d 0x%.8x %d %s\n",
                        str, intrs[i].sourceNodeID,
                        intrs[i].id, intrs[i].val, cnt,
                        (type == INTS_ERR ? scgtGetErrStr(intrs[i].val) :
                             (error ? "ERROR" : "") ));
            }
        }

        curSecond = (usysMsTimeGetElapsed(&curTime) / 1000);

        if( lastSecond != curSecond )
        {
            cr = 1;
            printf("\rgtint(%i): B=%i U=%i EM=%i EL=%i M=%i T=%i d=0x%x  ",
                    curSecond, bcastCnt, ucastCnt, drvMissedCnt,
                    drvLinkErrCnt, appMissedCnt, timeoutCnt,
                    (iO->displayMode & 0x7));

            lastSecond = curSecond;
        }

        fflush(stdout);

        /* if timed and not "run forever" and time not met or exceeded */
        if( iO->seconds && (iO->seconds != -1) && (curSecond >= iO->seconds) )
        {
            iO->exitParms.timeToExit = 1;
        }

    } while( iO->seconds && !iO->exitParms.timeToExit );

    printf("\n");

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
        {
            pparms->resetStats = pparms->flag = 1;
        }
        else if (ch == 'U')
        {
            pparms->toggleUcast = pparms->flag = 1;
        }
        else if (ch == 'B')
        {
            pparms->toggleBcast = pparms->flag = 1;
        }
        else if (ch == 'E')
        {
            pparms->toggleErr = pparms->flag = 1;
        }
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
              "gtint%s%s%s%s -u %u -i %u -n %u -w 0x%x -d 0x%x ",
              (iO->doSendAll || iO->doSend) ? " -":"", iO->doSendAll ? "A":"",
              iO->doSend==INTS_UCAST ? "U" : "",
              iO->doSend==INTS_BCAST ? "B" : "",
              iO->unit, iO->destID, iO->numTimes, iO->value, iO->displayMode);

    if(iO->seconds)
    {
        off += sprintf(&str[off],"-s %i ", iO->seconds);
    }

    if(iO->msPeriod)
    {
        off += sprintf(&str[off],"-p %d ", iO->msPeriod);
    }
}

/***************************************************************************/
/* Function:    parseCommand()                                             */
/* Description: Parses command line options                                */
/***************************************************************************/
static void parseCommand(int argc,char **argv,iOptionsType *iO,char *optionsString)
{
    char *endptr, nullchar = 0;
    int  i, j, len, tookParam=0;                         /* setting defaults */

                                                    /* setup default options */
    iO->helpLevel = 0;      iO->unit = 0;        iO->msPeriod = 0;
    iO->doSend = 0;         iO->doSendAll = 0;   iO->destID = 0;
    iO->numTimes = 1;       iO->value = 0;       iO->seconds = 0;
    iO->displayMode = 0;

    buildOptionsString(optionsString, iO);

    if(argc == 1)                                       /* start processing */
        { iO->helpLevel=1; return; }

    for(i = 1; i < argc; i++, tookParam=0)
    {
        len = (int) strlen(argv[i]);
        if((argv[i][0] != '-') || (len < 2))
        {
            printf("\nERROR: Unexpected option: \"%s\"\n\n", argv[i]);
            iO->helpLevel=1;
            return;
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
                return;
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
                else if(argv[i][j]=='w')
                    iO->value=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='i')
                    iO->destID=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='n')
                    iO->numTimes=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='p')
                    iO->msPeriod=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='s')
                    iO->seconds=strtoul(argv[i+1],&endptr,0);
                else tookParam = 0;

                if( tookParam && (*endptr) )
                {
                    printf("\nInvalid parameter \"%s\" for option \"-%c\"\n\n",
                           argv[i+1], argv[i][j]);
                    iO->helpLevel=1;
                    return;
                }
            }
                                       /* options without arguments now */
            if(     toupper(argv[i][j])=='A') iO->doSendAll    = 1;
            else if(         argv[i][j]=='U') iO->doSend       = INTS_UCAST;
            else if(         argv[i][j]=='B') iO->doSend       = INTS_BCAST;
            else if(tolower(argv[i][j])=='h')
            {
                iO->helpLevel=2;
                if(argc > 2) iO->helpLevel=3;
                return;
            }
            else if(!tookParam)
            {
                printf("\nERROR: Unexpected option: \"-%s\"\n\n",&(argv[i][j]));
                iO->helpLevel=1;
                return;
            }
        }

        if (tookParam) i++;
    }

    buildOptionsString(optionsString, iO);
}

/***************************************************************************/
/* Function:    printHelpLevel1()                                          */
/* Description: Display help and usage info                                */
/***************************************************************************/
static void printHelpLevel1(iOptionsType *iO, char *optionsString)
{
    printf("%s\n\n", APP_VER_STRING);
    printf("Usage: gtint [-U | -B] [-Ah] [-u unit] [-s seconds] [-p msPause]\n"
           "             [-n iterations] [-i dest_or_bcast_ID] [-w value]\n"
           "             [-d displayMode]\n\n");

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

static void printHelpLevel2(void)
{
    printf(
"Options:\n"
"  -u #    - board/unit number\n"
"  -B      - send a broadcast interrupt\n"
"  -U      - send a unicast interrupt\n"
"  -A      - send all broadcast and/or all unicast interrupts\n"
"  -i #    - destination ID (unicast), interrupt ID (broadcast), default is 0\n"
"  -w #    - data value to send with interrupt\n"
"  -n #    - number of times to send the interrupt(s)\n"
"  -d #    - display mode flags (default is 0):\n"
"            0x1 - broadcast, 0x2 - unicast, 0x4 - error interrupts\n"
"  -s #    - number of seconds to run test, use -1 for infinity. Default is\n"
"            1 burst iteration for sends, 1 receive iteration for receives.\n"
"  -p #    - pause for # milliseconds b/w interrupt bursts (send modes)\n"
"  -h      - display this help menu\n"
);
    printf(
"Runtime options:\n"
"   q - quit\n"
"   r - reset statistics\n"
"   b - toggle broadcast display\n"
"   e - toggle error interrupt display\n"
"   u - toggle unicast display\n\n");
    printf(
"Notes: Run `gtint -h 1' for more information.\n\n");
}

static void printHelpLevel3()
{
    printf(
"When displaying received interrupts, the following abbreviations are used:\n"
"   B  - broadcast interrupts\n"
"   U  - unicast interrupts\n"
"   EM - error interrupts, driver missed interrupts\n"
"   EL - error interrupts, link error\n"
"   M  - application missed interrupts\n"
"   d  - current display mode (see -d option for encoding)\n"
"\n");
    printf(
"Examples:\n"
"   gtint -AB -w 4       --send all broadcast intrs with value 4 from unit 0\n"
"   gtint -U -i 7 -w 0x8 --send a unicast intr with value 0x8 to nodeID 7\n"
"   gtint -u 1 -d 0x1    --receive intrs on unit 1 and display broadcast intrs\n"
"   gtint -s -1          --receive intrs infinitely\n"
"   gtint -B -i 1 -p 1000 -n 10 -s 20  --send broadcast intr 1 in bursts of\n"
"           10 for 20 seconds, pausing for 1000 milliseconds between bursts\n"
"\n"
    );
    printf(
"   SCRAMNet GT devices have network interrupt filtering capability. In order\n"
"to receive network interrupts, interrupt filtering settings must be configured\n"
"appropriately. The relavent settings are the broadcast interrupt mask,\n"
"unicast interrupt enable, and self-interrupt enable.  Each of the settings\n"
"is configurable through the software API library, and through the gtmon\n"
"utility application. The gtint application does not modify these settings.\n"
"See your hardware and software documentation for information regarding\n"
"these settings, and the functionality they provide.\n"
"\n"
    );
}

