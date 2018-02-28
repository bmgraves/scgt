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
/*                                                                            */
/*    Module      : scgt.h                                                    */
/*    Description : supporting structures and defines for scgt.c              */
/*    Platform    : Linux                                                     */
/*                                                                            */
/******************************************************************************/

#ifndef __SCGT_H__
#define __SCGT_H__

#include "systypes.h"
#include "ksys.h"
#include "gtcoreTypes.h"
#include "gtucore.h"
#include <linux/timer.h>


#define FILE_REV_SCGT_H    "3"   /* 03/23/04 */


typedef struct _scgtDMATools
{
    struct page **pages_1;
    struct page **pages_2;
    struct scatterlist *scatterList_1;
    struct scatterlist *scatterList_2;
    ksysSemS entrySem_1;
    ksysSemS entrySem_2;
    uint32 pending;
    
} scgtDMATools;
 

/* Device structure */

typedef struct _scgtDevice
{
    SCGT_DEVICE_CORE;
    
    uint32 *cRegPtr;        /* config/status register pointer */
    uint32 *nmRegPtr;       /* net management register pointer */
    uint32 memPhysAddr;

    struct pci_dev *pciDev;
    
    scgtDMATools writeTools;
    scgtDMATools readTools;
    
    struct timer_list *getIntrTimer;  /* used for timeout-ing of getInterrupt call */
    

    ksysSemS getIntrSem;    
    volatile uint32 getIntrWaitCount;
    ksysSpinLock getIntrWaitCountSpinLock;  /* protects wait count */
    ksysSpinLock getIntrTimerSpinLock;      /* protects timer use count
                                               and timer started */
                                              
    volatile uint32 getIntrTmrUseCnt;   /* # of threads using the timer */
    volatile uint8 getIntrTimerStarted;
} scgtDevice;



#endif /* __SCGT_H__ */
