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
/*    Module      : gtcore.c                                                  */
/*    Description : SCRAMNet GT Core Routines                                 */
/*                                                                            */
/******************************************************************************/

/************************************************************************/
/**************************  I N C L U D E S  ***************************/
/************************************************************************/

#include "gtcore.h"
#include "scgtdrv.h"
#include "ksys.h"


char * FILE_REV_GTCORE_C = "B";   /* 05/16/14 */

/**************************************************************************/
/***********************  D E F I N E S & M A C R O S  ********************/
/**************************************************************************/

#define GTCORE_SEQ_NUM_MASK   0xFFFFFFF    /* seqNum cannot be -1 */



/*
   The following enables customization of transaction queue and DMA
   chain allocation which will be needed under some OSes. 
   If custom versions are needed, use ksys.* files to provide for appropriate 
   equivalents for allocation and deallocation.
   To override the core exchange and queue allocation/mapping functions, 
   define scgtExchChain() and scgtTrQueue() in your scgt.c file.  
   Give a prototype for it in scgtdrv.h.  In almost all cases, you can use the default
   routines if you override the allocation/mapping routines.
*/



#ifndef scgtExchChain      /* means no customized version exists */
#define scgtExchChain(a,b,c,d,e) gtcoreExchChain(a,b,c,d,e)
#endif

#ifndef scgtTrQueue        /* means no customized version exists */
#define scgtTrQueue(a,b,c,d)     gtcoreTrQueue(a,b,c,d)
#endif

/* default action for DMA Mallocs is to copy mapDataVoidPtr to the location
   pointed to by dmaHandleVoidPtrPtr and then call ksysMalloc().  
   The is all that's needed for some OSs and won't hurt others that don't 
   use the mapDataVoidPtr at all */

#ifndef ksysDma1Malloc      /* means no special memory is needed for queues */ 
#define ksysDma1Malloc(mapDataVoidPtr, dmaHandleVoidPtrPtr, size)    (*(dmaHandleVoidPtrPtr) = mapDataVoidPtr, \
                                                                      ksysMalloc(size))
#endif

#ifndef ksysDma1Free        /* means no special memory free is needed for queues */
#define ksysDma1Free(dmaHandleVoidPtrPtr, pBuf, size)    ksysFree(pBuf, size)
#endif

#ifndef ksysDma2Malloc      /* means no special memory is needed for chains */
#define ksysDma2Malloc(mapDataVoidPtr, dmaHandleVoidPtrPtr, size)    (*(dmaHandleVoidPtrPtr) = mapDataVoidPtr, \
                                                                      ksysMalloc(size))
#endif

#ifndef ksysDma2Free        /* means no special memory free is needed for chains */
#define ksysDma2Free(dmaHandleVoidPtrPtr, pBuf, size)    ksysFree(pBuf, size)
#endif

#ifdef KSYS_DEFAULT_WDT_VAL
#define GTCORE_DEFAULT_WDT_VAL KSYS_DEFAULT_WDT_VAL
#else
#define GTCORE_DEFAULT_WDT_VAL 0xFFFFF
#endif


#define GTCORE_CHAIN_BYTES   (SCGT_DMA_CHAIN_LEN * GTCORE_CE_SIZE)   /* chain length in bytes */

/*
 * define SCGT_EXCH_MAP_PER_CHAIN in scgtdrv.h in order to have
 * gtcoreExchChain() allocate and map 1 chain per exchange.  If this is
 * not defined, there will be an allocation and map per exchange direction 
 * which is shared across chains.  1 allocation and map per direction can
 * be beneficial on some systems by limiting the number of maps required 
 * (some machines have a limited number of maps).  The memory must be 
 * physically contiguous for this to work.
 */
 
#ifndef SCGT_EXCH_CHAIN_MAP_PER_EXCH
#define SCGT_EXCH_CHAIN_ONE_MAP
#endif

/*************************************************************/
/************************ PROTOTYPES *************************/
/*************************************************************/

uint32 gtcoreInitExchMgr(scgtDevice *dev);
uint32 gtcoreInitExchEntries(scgtDevice *dev, gtcoreExchMgrData *exMgrData, uint8 direction);

void gtcoreDestroyExchMgr(scgtDevice *dev);

uint32 gtcoreExchChain(scgtDevice *dev, gtcoreExchMgrData *exchMgrData, 
                       uint32 exchNum, uint8 direction, uint8 doAlloc);
uint32 gtcoreTrQueue(scgtDevice *dev, gtcoreExchMgrData *exchMgrData,
                     uint8 direction, uint8 action);

uint32 gtcoreInitIntrQ(scgtDevice *dev);
void gtcoreDestroyIntrQ(scgtDevice *dev);

void gtcoreInitDevice(scgtDevice *dev);

/*************************************************************/
/************************ GLOBALS ****************************/
/*************************************************************/

/* statistics names string */
char gtcoreStatNames[] = "nStats,namesLen,ISR,DPC,DPC_DMA,intr,tc0Intr,tc1Intr," \
                         "w_entrySem_1,w_entrySem_2,r_entrySem_1,r_entrySem_2," \
                         "w_exch_sem_0,w_exch_sem_1,w_exch_sem_2,w_exch_sem_3," \
                         "r_exch_sem_0,r_exch_sem_1,r_exch_sem_2,r_exch_sem_3," \
                         "netIntr," \
                         "linkErrIntr,getIntrTmr,getIntrWaitCount,netIntrRoll," \
                         "netIntrRollFix,netIntCntFix0,netIntCntFix1," \
                         "netIntrCntFixFailed,swNetIntCnt,hwNetIntCnt";

uint32 gtcoreStatNameIndex[SCGT_STATS_NUM_STATS + 1];


/*
 * gtcoreGetDeviceInfo()
 *     fill in device info structure
 *     gtcoreGetDeviceInfo doesn't:
 *           1) fill in driverRevisionStr()
 *           2) fill in boardLocationStr()
 *     these are up to the driver to do.
 */

uint32 gtcoreGetDeviceInfo(scgtDevice *dev, scgtDeviceInfo *devInfo)
{
    uint32 tmp;
    
    devInfo->unitNum = dev->unitNum;
    devInfo->mappedMemSize = dev->memSize;
    devInfo->popMemSize = dev->popMemSize;
    
    tmp = scgtReadCReg(dev, GTCORE_R_BRD_INFO);
    devInfo->revisionID = tmp >> 16;

    devInfo->numLinks = (scgtReadCReg(dev, GTCORE_R_LINK_STAT) & GTCORE_R_RLC)? 2 : 1;
    
    return SCGT_SUCCESS;
}

/*
 * gtcoreGetPopMemSize()
 *     returns the size (in bytes) of GT memory populated the board.
 */
 
uint32 gtcoreGetPopMemSize(scgtDevice *dev)
{
    uint32 tmp;
    tmp = scgtReadCReg(dev, GTCORE_R_BRD_INFO);

    if (tmp & GTCORE_R_MEM_TYPE)
    {
        /* DDR memory */
        return (0x80 << (tmp & GTCORE_R_MEM_PMS)) * 0x100000;
    }
    else
    {
        /* ZBT memory */
        return (0x100 << (tmp & GTCORE_R_MEM_PMS)) * 0x400;
    }
}

/*
 * gtcoreGetState()
 *     fills in val with state denoted by stateID
 */
uint32 gtcoreGetState(scgtDevice *dev, uint32 stateID, uint32 *val)
{
    switch (stateID)
    {
        case SCGT_NET_TIMER_VAL:
            *val = scgtReadCReg(dev, GTCORE_R_NET_TMR);
            break;
            
        case SCGT_LATENCY_TIMER_VAL:
            *val = scgtReadCReg(dev, GTCORE_R_LAT_TMR);
            break;
        
        case SCGT_SM_TRAFFIC_CNT:
            *val = scgtReadCReg(dev, GTCORE_R_SM_TRFC_CNTR);
            break;
            
        case SCGT_SPY_SM_TRAFFIC_CNT:
            *val = scgtReadCReg(dev, GTCORE_R_HNT_TRFC_CNTR);
            break;
            
        case SCGT_SPY_NODE_ID:
            *val = scgtReadCReg(dev, GTCORE_R_LINK_CTL) >> 24;
            break;
            
        case SCGT_NODE_ID:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_CTL) & GTCORE_R_NODE_ID) >> 8;
            break;
            
        case SCGT_ACTIVE_LINK:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_CTL) & GTCORE_R_LNK_SEL)? 1 : 0;
            break;

        case SCGT_WRITE_ME_LAST:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_CTL) & GTCORE_R_WML)? 1 : 0;
            break;
        
        case SCGT_UNICAST_INT_MASK:
            *val = scgtReadCReg(dev, GTCORE_R_RX_HUI_MSK) & 0x1;
            break;
            
        case SCGT_BROADCAST_INT_MASK:
            *val = scgtReadCReg(dev, GTCORE_R_RX_HBI_MSK);
            break;

        case SCGT_INT_SELF_ENABLE:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_CTL) & GTCORE_R_INT_SELF)? 1 : 0;
            break;

        case SCGT_RING_SIZE:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_STAT) >> 24) + 1;
            break;
            
        case SCGT_UPSTREAM_NODE_ID:
            *val = scgtReadCReg(dev, GTCORE_R_MISC_FNCTN) >> 24;
            break;
            
        case SCGT_EWRAP:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_CTL) & GTCORE_R_WRAP)? 1 : 0;
            break;
            
        case SCGT_NUM_LINK_ERRS:
            *val = scgtReadCReg(dev, GTCORE_R_LNK_ERR_CNTR);
            break;
        
        case SCGT_TRANSMIT_ENABLE:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_CTL) & GTCORE_R_TX_EN)? 1 : 0;
            break;
            
        case SCGT_RECEIVE_ENABLE:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_CTL) & GTCORE_R_RX_EN)? 1 : 0;
            break;
            
        case SCGT_RETRANSMIT_ENABLE:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_CTL) & GTCORE_R_RT_EN)? 1 : 0;
            break;
            
        case SCGT_LASER_0_ENABLE:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_CTL) & GTCORE_R_LAS_0_EN)? 1 : 0;
            break;
            
        case SCGT_LASER_1_ENABLE:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_CTL) & GTCORE_R_LAS_1_EN)? 1 : 0;
            break;
            
        case SCGT_LINK_UP:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_STAT) & GTCORE_R_LNK_UP)? 1 : 0;
            break;
            
        case SCGT_LASER_0_SIGNAL_DET:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_STAT) & GTCORE_R_LAS_0_SD)? 1 : 0;
            break;
            
        case SCGT_LASER_1_SIGNAL_DET:
            *val = (scgtReadCReg(dev, GTCORE_R_LINK_STAT) & GTCORE_R_LAS_1_SD)? 1 : 0;
            break;
        
        case SCGT_D64_ENABLE:
            *val = (scgtReadCReg(dev, GTCORE_R_USR_BRD_CSR) & GTCORE_R_INIT_D64_DIS)? 0 : 1;
            break;
            
        case SCGT_BYTE_SWAP_ENABLE:
            *val = (scgtReadCReg(dev, GTCORE_R_USR_BRD_CSR) & GTCORE_R_DB_SWAP)? 1 : 0;
            break;
            
        case SCGT_WORD_SWAP_ENABLE:
            *val = (scgtReadCReg(dev, GTCORE_R_USR_BRD_CSR) & GTCORE_R_DW_SWAP)? 1 : 0;
            break;
        
        case SCGT_LINK_ERR_INT_ENABLE:
            *val = (scgtReadCReg(dev, GTCORE_R_INT_CSR) & GTCORE_R_LNK_ERR_INT_EN)? 1 : 0;
            break;

        case SCGT_READ_BYPASS_ENABLE:
            *val = (scgtReadCReg(dev, GTCORE_R_MISC_FNCTN) & GTCORE_R_DIS_RD_BYP)? 0 : 1;
            break;
                
        default:
            return SCGT_BAD_PARAMETER;
    }
    return SCGT_SUCCESS;
}


/*
 * gtcoreSetState()
 *     sets state stateID based on val.
 */

uint32 gtcoreSetState(scgtDevice *dev, uint32 stateID, uint32 val)
{
    switch (stateID)
    {
        case SCGT_NODE_ID:
            scgtWriteCRegMasked(dev, GTCORE_R_LINK_CTL, 
                                ((val & 0xFF) << 8), GTCORE_R_NODE_ID);
            break;
           
        case SCGT_ACTIVE_LINK:
            scgtWriteCRegMasked(dev, GTCORE_R_LINK_CTL, 
                                ((val & 0x1) << 4), GTCORE_R_LNK_SEL);
            break;
            
        case SCGT_WRITE_ME_LAST:
            scgtWriteCRegMasked(dev, GTCORE_R_LINK_CTL, 
                                (val)? GTCORE_R_WML : 0, GTCORE_R_WML);
            break;
       
        case SCGT_NET_TIMER_VAL:
            scgtWriteCReg(dev, GTCORE_R_NET_TMR, val);
            break;
            
        case SCGT_LATENCY_TIMER_VAL:
            scgtWriteCReg(dev, GTCORE_R_LAT_TMR, val);
            break;
        
        case SCGT_SM_TRAFFIC_CNT:    
            scgtWriteCReg(dev, GTCORE_R_SM_TRFC_CNTR, val);
            break;
            
        case SCGT_SPY_SM_TRAFFIC_CNT:
            scgtWriteCReg(dev, GTCORE_R_HNT_TRFC_CNTR, val);
            break;
            
        case SCGT_SPY_NODE_ID:
            scgtWriteCRegMasked(dev, GTCORE_R_LINK_CTL, (val << 24), GTCORE_R_HNT_ID);
            break;
                
        case SCGT_UNICAST_INT_MASK:
            scgtWriteCRegMasked(dev, GTCORE_R_RX_HUI_MSK, val, 0x1);
            break;
            
        case SCGT_BROADCAST_INT_MASK:
            scgtWriteCReg(dev, GTCORE_R_RX_HBI_MSK, val);
            break;

        case SCGT_INT_SELF_ENABLE:
            scgtWriteCRegMasked(dev, GTCORE_R_LINK_CTL, 
                                (val)? GTCORE_R_INT_SELF : 0, GTCORE_R_INT_SELF);
            break;
                     
        case SCGT_EWRAP:
            scgtWriteCRegMasked(dev, GTCORE_R_LINK_CTL, 
                                (val)? GTCORE_R_WRAP : 0, GTCORE_R_WRAP);
            break;
            
        case SCGT_NUM_LINK_ERRS:
            break;
            
        case SCGT_TRANSMIT_ENABLE:
            scgtWriteCRegMasked(dev, GTCORE_R_LINK_CTL, 
                                (val)? GTCORE_R_TX_EN : 0, GTCORE_R_TX_EN);            
            break;
            
        case SCGT_RECEIVE_ENABLE:
            scgtWriteCRegMasked(dev, GTCORE_R_LINK_CTL, 
                                (val)? GTCORE_R_RX_EN : 0, GTCORE_R_RX_EN); 
            break;
            
        case SCGT_RETRANSMIT_ENABLE:
            scgtWriteCRegMasked(dev, GTCORE_R_LINK_CTL, 
                                (val)? GTCORE_R_RT_EN : 0, GTCORE_R_RT_EN); 
            break;
            
        case SCGT_LASER_0_ENABLE:
            scgtWriteCRegMasked(dev, GTCORE_R_LINK_CTL, 
                                (val)? GTCORE_R_LAS_0_EN : 0, GTCORE_R_LAS_0_EN); 
            break;
            
        case SCGT_LASER_1_ENABLE:
            scgtWriteCRegMasked(dev, GTCORE_R_LINK_CTL, 
                                (val)? GTCORE_R_LAS_1_EN : 0, GTCORE_R_LAS_1_EN); 
            break;

        case SCGT_D64_ENABLE:
            scgtWriteCRegMasked(dev, GTCORE_R_USR_BRD_CSR, 
                                (val)? 0 : GTCORE_R_INIT_D64_DIS, GTCORE_R_INIT_D64_DIS); 
            
            break;

        case SCGT_BYTE_SWAP_ENABLE:
            scgtWriteCRegMasked(dev, GTCORE_R_USR_BRD_CSR, 
                                (val)? GTCORE_R_DB_SWAP : 0, GTCORE_R_DB_SWAP);
            break;
            
        case SCGT_WORD_SWAP_ENABLE:
            scgtWriteCRegMasked(dev, GTCORE_R_USR_BRD_CSR,
                                (val)? GTCORE_R_DW_SWAP : 0, GTCORE_R_DW_SWAP);
            break;

        case SCGT_LINK_ERR_INT_ENABLE:
            scgtWriteCRegMasked(dev, GTCORE_R_INT_CSR,
                                (val)? GTCORE_R_LNK_ERR_INT_EN : 0, GTCORE_R_LNK_ERR_INT_EN);
            break;

        case SCGT_READ_BYPASS_ENABLE:
            scgtWriteCRegMasked(dev, GTCORE_R_MISC_FNCTN,
                                (val)? 0 : GTCORE_R_DIS_RD_BYP, GTCORE_R_DIS_RD_BYP);
            break;
    
        default:
            return SCGT_BAD_PARAMETER;
    }
    return SCGT_SUCCESS;
}

/**********************************************************************/
/****************** Network Interrupt Managment ***********************/
/**********************************************************************/

/*
 * gtcorePutIntr()
 *     append interrupt to end of interrupt queue
 */
 
void gtcorePutIntr(scgtDevice *dev, scgtInterrupt *intr)
{
    gtcoreIntrQData *iqd = &dev->intrQData;
    scgtInterrupt *curI;
    
    curI = &iqd->intrQ[iqd->head];

    curI->type = intr->type;
    curI->sourceNodeID = intr->sourceNodeID;
    curI->id = intr->id;
    curI->val = intr->val;
    curI->seqNum = intr->seqNum;
    
    iqd->seqNum = (iqd->seqNum + 1) & GTCORE_SEQ_NUM_MASK;
    iqd->head = iqd->seqNum % GTCORE_INTR_Q_SIZE;
}

/*
 * gtcoreGetIntr()
 *     get a buffer of interrupts from interrupt queue
 *     returns SCGT_SUCCESS if there were interrupt waiting (buffer has stuff in it)
 *     returns SCGT_MISSED_INTERRUPTS if there were interrupt missed (buffer has stuff in it)
 *     returns SCGT_TIMEOUT if there were no interrupts waiting
 *         note: This call does not include timeout functionality (You have to perform a wait
 *               after calling this function if it returns SCGT_TIMEOUT.  Then call this function 
 *               again if something comes in or on a certain interval if using a timer etc).
 */

uint32 gtcoreGetIntr(scgtDevice *dev, scgtGetIntrBuf *gibuf)
{
    scgtInterrupt *iBufPtr;
    uint32 intrQSeqNum;
    uint32 intrQHead;
    uint32 seqNum;
    uint32 n2Copy;
    uint32 startIndex;
    uint32 ret = SCGT_TIMEOUT;
    uint32 missed = 0;

    iBufPtr = UINT64_TO_PTR(scgtInterrupt, gibuf->intrBuf);
    seqNum = gibuf->seqNum;
    gibuf->numInterruptsRet = 0;

#ifdef _MSC_VER
#pragma warning(push)  
#pragma warning(disable:4127) 
#endif
    while (1)
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    {
        /* save local copy of queue head */
        intrQSeqNum = dev->intrQData.seqNum;  /* critical operation (race cond.) */
        
        if (intrQSeqNum > seqNum)
        {
            n2Copy = intrQSeqNum - seqNum;

gtcoreGetIntr_docopy:                
            /* check for missed interrupts (-1 is so we don't touch the one being updated) */
            if (n2Copy > (GTCORE_INTR_Q_SIZE - 1))
            {
                /* missed interrupts */
                /* set sequence number to oldest and call gtcoreGetIntr() again to get them */

                /* & GTCORE_SEQ_NUM_MASK is used here for rollover case */                
                seqNum = (intrQSeqNum - (GTCORE_INTR_Q_SIZE - 1)) & GTCORE_SEQ_NUM_MASK;
                missed++;
                
                if (missed >= 5)   /* avoid looping forever */
                    break; 
            }
            else
            {
                /* everything is kewl... do the copy */               
                if (n2Copy > gibuf->bufSize)
                    n2Copy = gibuf->bufSize;
                gibuf->numInterruptsRet = n2Copy;
    
                startIndex = seqNum % GTCORE_INTR_Q_SIZE;
                intrQHead = intrQSeqNum % GTCORE_INTR_Q_SIZE;

                seqNum += n2Copy;
    
                if (startIndex < intrQHead)
                {
                    ksysCopyToUser(iBufPtr, 
                                   &dev->intrQData.intrQ[startIndex], 
                                   sizeof(scgtInterrupt) * n2Copy);
                }
                else
                {
                    uint32 nFirstCopy = GTCORE_INTR_Q_SIZE - startIndex;
                    if (nFirstCopy > n2Copy)
                        nFirstCopy = n2Copy;
    
                    ksysCopyToUser(iBufPtr, 
                                   &dev->intrQData.intrQ[startIndex], 
                                   sizeof(scgtInterrupt) * nFirstCopy);
    
                    n2Copy -= nFirstCopy;
                    if (n2Copy)
                    {
                        ksysCopyToUser(iBufPtr + nFirstCopy,
                                       &dev->intrQData.intrQ[0], 
                                       sizeof(scgtInterrupt) * n2Copy);                
                    }
                }
    
                ret = SCGT_SUCCESS;
                break;
            }
        }
        else if (intrQSeqNum < seqNum)
        {
            if (seqNum == (uint32) -1)
            {
                /* this is the apps first time in... init seqNum
                   to current and check to see if anything came in
                   by calling gtcoreGetIntr() again. */
                seqNum = intrQSeqNum;
            }
            else if (seqNum > GTCORE_SEQ_NUM_MASK)
            {
                return SCGT_BAD_PARAMETER;
            }
            else
            {
                /* assume intrQSeqNum overflowed 
                   (rather than invalid seqNum passed in) */
                
                n2Copy = (GTCORE_SEQ_NUM_MASK + 1) - seqNum + intrQSeqNum;
                goto gtcoreGetIntr_docopy;
            }
        }
        else
        {
            ret = SCGT_TIMEOUT;
            break; /* timeout */
        }
    }

    gibuf->seqNum = seqNum & GTCORE_SEQ_NUM_MASK;
    return (missed ? SCGT_MISSED_INTERRUPTS : ret);
}


/*
 * gtcoreSendIntr()
 *     Send network interrupt using register writes (not DMA engine)
 */
 
uint32 gtcoreSendIntr(scgtDevice *dev, scgtInterrupt *intr)
{
    if (!intr)
        return SCGT_BAD_PARAMETER;

    if (intr->type == SCGT_BROADCAST_INTR)
    {
        scgtWriteNMReg(dev, GTCORE_NM_TX_HBI + (intr->id * 4), intr->val);
    }
    else if (intr->type == SCGT_UNICAST_INTR)
    {
        scgtWriteNMReg(dev, GTCORE_NM_TX_HUI + (intr->id * 4), intr->val);
    }
    else
    {
        return SCGT_BAD_PARAMETER;
    }
    
    return SCGT_SUCCESS;
}

/****************************************************************/
/************************ Statistics ****************************/
/****************************************************************/

/*
 * gtcoreGetStats()
 *     Copy statistics to user.
 *     returns SCGT_BAD_PARAMETER on boundary condition failed (ie. want too many stats)
 *     returns SCGT_SUCCESS otherwise.
 */
 
uint32 gtcoreGetStats(scgtDevice *dev, scgtStats *stats)
{
    uint32 *statsArray;
    char *names;
        
    /* check boundary conditions */
    if (stats->firstStatIndex + stats->num > SCGT_STATS_NUM_STATS)
        return SCGT_BAD_PARAMETER;
    
    if (stats->stats)
    {
        statsArray = UINT64_TO_PTR(uint32, stats->stats);
        ksysCopyToUser(statsArray, &dev->stats[stats->firstStatIndex], sizeof(uint32) * stats->num);
    }

    if (stats->names)
    {
        uint32 startIndex = gtcoreStatNameIndex[stats->firstStatIndex];
        uint32 lastIndex = gtcoreStatNameIndex[stats->firstStatIndex + stats->num];

        names = UINT64_TO_PTR(char, stats->names);

        /* last index is the beginning of the next name */
        lastIndex -= 1;
        ksysCopyToUser(names,
                       &gtcoreStatNames[startIndex],
                       lastIndex - startIndex);
        ksysCopyToUser(&names[lastIndex], "\0", 1);                     
    }
    
    return SCGT_SUCCESS;
}



/****************************************************************/
/****************** gtcore Initialization ***********************/
/****************************************************************/

/*
 * gtcoreFixRegSwapping()
 *     Turns on byte swapping for registers if needed.
 *     Also sets memory byte swapping based on the register setting.
 *     The memory byte swapping will not be set right if the register
 *     read function does swapping (as it does on some platforms).  But
 *     at least it will be in a consistant state after every call to
 *     this function.
 *     Call this as soon as registers are mapped.
 */
 
void gtcoreFixRegSwapping(scgtDevice *dev)
{
    uint32 val = scgtReadCReg(dev, GTCORE_R_DRV_BRD_CSR);
    
    if ((val & GTCORE_R_STC_A) == 0x5)
    {
        /* turn off reg swapping if on, otherwise turn on reg swapping */
        if (val & 0x10000000)
            scgtWriteCReg(dev, GTCORE_R_DRV_BRD_CSR, val & (~0x10000000));
        else
            scgtWriteCReg(dev, GTCORE_R_DRV_BRD_CSR, val | 0x10000000);
    }
    
    if (scgtReadCReg(dev, GTCORE_R_DRV_BRD_CSR) & GTCORE_R_TCB_SWAP)
    {
        /* byte swapping is enabled for registers */
        /* enable byte swapping for memory also */
        scgtWriteCRegMasked(dev, GTCORE_R_USR_BRD_CSR, 
                            GTCORE_R_DB_SWAP, GTCORE_R_DB_SWAP);
        
        /* enable byte swapping when fetching transaction queue 
           entries and chains */                    
        scgtWriteCRegMasked(dev, GTCORE_R_DRV_BRD_CSR, 
                            GTCORE_R_ICB_SWAP, GTCORE_R_ICB_SWAP);
        
    }
    else
    {
        /* byte swapping is disabled for registers */
        /* disable byte swapping for memory also */
        scgtWriteCRegMasked(dev, GTCORE_R_USR_BRD_CSR, 0, GTCORE_R_DB_SWAP);

        /* disable byte swapping when fetching transaction queue 
           entries and chains */                    
        scgtWriteCRegMasked(dev, GTCORE_R_DRV_BRD_CSR, 0, GTCORE_R_ICB_SWAP);

    }
    
}

/*
 * gtcoreInit()
 *     Initialize gtcore for this device.
 *     Returns 0 on success, non-zero otherwise.
 */
 
uint32 gtcoreInit(scgtDevice *dev)
{
    uint32 ret;
    
    if ((ret = gtcoreInitExchMgr(dev)) != SCGT_SUCCESS)
        return ret;

    if ((ret = gtcoreInitIntrQ(dev)) != SCGT_SUCCESS)
        return ret;
                
    gtcoreInitDevice(dev);
    
    return SCGT_SUCCESS;
}

/*
 * gtcoreDestroy()
 *     Free core data
 */

void gtcoreDestroy(scgtDevice *dev)
{
    /* make sure interrupts are disabled */
    scgtWriteCReg(dev, GTCORE_R_INT_CSR, 0);

    gtcoreDestroyExchMgr(dev);
    gtcoreDestroyIntrQ(dev);
}

/*
 * gtcoreInitExchMgr()
 *     Initialize the exchange manager data
 */
 
uint32 gtcoreInitExchMgr(scgtDevice *dev)
{
    int i;
    uint32 ret;
        
    /* 
       Allocate transaction queues first. We will call scgtTrQueue which
       in most cases will be defined as gtcoreTrQueue(). If you need
       a special method for allocating appropriate space, make a function
       called scgtTrQueue() in your scgt.c to override the gtcoreTrQueue()
       definition.  Give a prototype of it in scgtdrv.h
    */

    if ((ret = scgtTrQueue(dev, &dev->wexch, GTCORE_WRITE, 1)) != SCGT_SUCCESS)
        return ret;
        
    if ((ret = scgtTrQueue(dev, &dev->rexch, GTCORE_READ, 1)) != SCGT_SUCCESS)
        return ret;
        
    for (i = 0; i < GTCORE_EXCH_CNT; i++)
    {
        dev->wexch.exchQ[i].tqe = &dev->wexch.trQueue[i * (GTCORE_TQE_SIZE / 4)];
        dev->rexch.exchQ[i].tqe = &dev->rexch.trQueue[i * (GTCORE_TQE_SIZE / 4)];
    }
    
    /* now that we have transaction queues allocated
       let's initialize the exchange entries */

    if ((ret = gtcoreInitExchEntries(dev, &dev->wexch, GTCORE_WRITE)) != SCGT_SUCCESS)
        return ret;

    if ((ret = gtcoreInitExchEntries(dev, &dev->rexch, GTCORE_READ)) != SCGT_SUCCESS)
        return ret;
        
    /* Initialize Transaction Queue Registers */
    scgtWriteCReg(dev, GTCORE_R_TQ_ADD32_TC0, (uint32) dev->wexch.trPhysical);
    scgtWriteCReg(dev, GTCORE_R_TQ_ADD64_TC0, (uint32) (dev->wexch.trPhysical >> 32));
    scgtWriteCReg(dev, GTCORE_R_TQ_ADD32_TC1, (uint32) dev->rexch.trPhysical);
    scgtWriteCReg(dev, GTCORE_R_TQ_ADD64_TC1, (uint32) (dev->rexch.trPhysical >> 32));
    
    scgtWriteCReg(dev, GTCORE_R_TQ_CTL_TC0, ((GTCORE_EXCH_CNT-1) << 16) | GTCORE_R_TQ_EN | GTCORE_R_TQ_RST);
    scgtWriteCReg(dev, GTCORE_R_TQ_CTL_TC1, ((GTCORE_EXCH_CNT-1) << 16) | GTCORE_R_TQ_EN | GTCORE_R_TQ_RST);
    
    dev->completedHeadIndex = dev->completedTailIndex = 0;
    
    return SCGT_SUCCESS;
}

/*
 * gtcoreDestroyExchMgr()
 *     Frees the exchange manager data
 */
 
void gtcoreDestroyExchMgr(scgtDevice *dev)
{
    int i;
    gtcoreExchMgrData *exMgrData;

    /* deallocate read exchange chains and sems */    
    exMgrData = &dev->rexch;
    for (i = 0; i < GTCORE_EXCH_CNT; i++)
    {
        scgtExchChain(dev, exMgrData, i, GTCORE_READ, 0);
        ksysSemBDestroy(&(exMgrData->exchQ[i].compSem));
    }

    /* deallocate write exchange chains and sems */    
    exMgrData = &dev->wexch;
    for (i = 0; i < GTCORE_EXCH_CNT; i++)
    {
        scgtExchChain(dev, exMgrData, i, GTCORE_WRITE, 0);
        ksysSemBDestroy(&(exMgrData->exchQ[i].compSem));
    }
    
    /* free transaction queues. */
    scgtTrQueue(dev, &dev->rexch, GTCORE_READ, 0);
    scgtTrQueue(dev, &dev->wexch, GTCORE_WRITE, 0);
}


/*
 * gtcoreInitExchEntries()
 *     Initialize an exchange
 */
 
uint32 gtcoreInitExchEntries(scgtDevice *dev, gtcoreExchMgrData *exMgrData, uint8 direction)
{
    int i;
    uint32 ret;
    gtcoreExch *exch;
    
    exMgrData->headIndex = exMgrData->tailIndex = 0;
    
    for (i = 0; i < GTCORE_EXCH_CNT; i++)
    {
        exch = &exMgrData->exchQ[i];
        exch->exchQIndex = i;
        exch->state = GTCORE_EXCH_NOT_DONE;
        exch->gtMemoryOffset = 0;
        exch->bytesToTransfer = 0;
        exch->bytesTransferred = 0;
        exch->intr = NULL;
        exch->flags = 0;
        exch->direction = direction;
        
        if ((ret = scgtExchChain(dev, exMgrData, i, direction, 1)) != SCGT_SUCCESS)
            return ret;
        
        /* point the tranaction queue entry to the chain */
        exch->tqe[GTCORE_TQE_CE_ADD32] = (uint32) exch->sgListPhysAddr[0];
        exch->tqe[GTCORE_TQE_CE_ADD64] = (uint32)(exch->sgListPhysAddr[0] >> 32);
        
        ksysSemBCreate(&(exch->compSem));
    }
    
    return SCGT_SUCCESS;
}

/*
 * gtcoreAlignAddr()
 *     Align address to alignSize (rounding up).
 *     alignSize must be a power of 2.
 */
void * gtcoreAlignAddr(void *ptr, uint32 alignSize)
{
    uintpsize iPtr = (uintpsize) ptr + (alignSize - 1);

    iPtr &= ~((uintpsize)(alignSize - 1));
    return (void *) iPtr;
}



/*
 * gtcoreExchChain()
 *     Allocate and initialize or Deallocate a chain for the exchange.
 *     Note that this function will be overriden if you define scgtExchChain()
 *     Returns SCGT_SUCCESS if successful.
 */

uint32 gtcoreExchChain(scgtDevice *dev, gtcoreExchMgrData *exchMgrData, 
                       uint32 exchNum, uint8 direction, uint8 doAlloc)
{
    uint32 mySize;
    int i;
    volatile uint32 *chainEntry;
    gtcoreExch *exch;
    uint64 nextAddr;

#ifndef KSYS_CACHE_LINE_SIZE
#define GTCORE_CACHE_LINE_SIZE  256
#else
#define GTCORE_CACHE_LINE_SIZE  KSYS_CACHE_LINE_SIZE    /* cache line size must be >= 16 */
#endif

    exch = &exchMgrData->exchQ[exchNum];

    /* sgSpace of mySize is/will be larger than needed to cover aligment
       and cache safety */

#ifdef SCGT_EXCH_CHAIN_ONE_MAP       
    mySize = GTCORE_CHAIN_BYTES * GTCORE_EXCH_CNT + GTCORE_CACHE_LINE_SIZE * 2;
#else
    mySize = GTCORE_CHAIN_BYTES + GTCORE_CACHE_LINE_SIZE * 2;
#endif

    if (doAlloc == 0)
    {
#ifdef SCGT_EXCH_CHAIN_ONE_MAP
        if (exchNum != 0)
            return SCGT_SUCCESS;
#endif
        if (exch->sgSpace != NULL)
        {
            /* unmap sgList[0] */
            if (exch->sgListPhysAddr[0])
                ksysUnmapVirtToBus(exch->sgDmaHandle, (void *) exch->sgList[0]);

            /* free sgSpace */
            if (exch->sgSpace)
                ksysDma2Free(exch->sgDmaHandle, (void *)exch->sgSpace, mySize);
            exch->sgSpace = NULL;
        }
        
        return SCGT_SUCCESS;
    }
    
#ifdef SCGT_EXCH_CHAIN_ONE_MAP
    /**** allocate and map chain for only exchange 0 
          use offsets into it for other exchanges ******/
    
    if (exchNum == 0)
    {
        /* allocate the space, align it, and map */
        exch->sgSpace = (uint32 *) ksysDma2Malloc(dev->mapData, &exch->sgDmaHandle, mySize);
        if (exch->sgSpace == NULL)
            return SCGT_DRIVER_ERROR;
        
        exch->sgList[0] = (uint32 *) gtcoreAlignAddr((void *) exch->sgSpace, GTCORE_CACHE_LINE_SIZE);

        exch->sgListPhysAddr[0] = (uint64) ((uintpsize) ksysMapVirtToBus(exch->sgDmaHandle, 
                                                                         ((void *)exch->sgList[0]), 
                                                                         GTCORE_CHAIN_BYTES * GTCORE_EXCH_CNT));

        if (exch->sgListPhysAddr[0] == 0)
            return SCGT_DRIVER_ERROR;
    }
    else
    {
        /* here we use an offset into the already mapped address from exchange 0 */
        gtcoreExch *exch0 = &exchMgrData->exchQ[0];
        
        if (exch0->sgList[0] == NULL)
            return SCGT_DRIVER_ERROR;
        
        /* sgList[0] is type uint32 * so we add num of words to get new ptr */
        exch->sgList[0] = exch0->sgList[0] + ((exchNum * GTCORE_CHAIN_BYTES) / 4);
        exch->sgListPhysAddr[0] = exch0->sgListPhysAddr[0] + (exchNum * GTCORE_CHAIN_BYTES);
        exch->sgDmaHandle = exch0->sgDmaHandle;
    }
#else
    /**** allocate and map chain for every exchange separately ******/

    exch->sgSpace = (uint32 *) ksysDma2Malloc(dev->mapData, &exch->sgDmaHandle, mySize);
    if (exch->sgSpace == NULL)
        return SCGT_DRIVER_ERROR;
    
    exch->sgList[0] = (uint32 *) gtcoreAlignAddr((void *)exch->sgSpace, GTCORE_CACHE_LINE_SIZE);

    exch->sgListPhysAddr[0] = (uint64) ksysMapVirtToBus(exch->sgDmaHandle, 
                                                        ((void *)exch->sgList[0]), 
                                                        GTCORE_CHAIN_BYTES);
    if (exch->sgListPhysAddr[0] == 0)
        return SCGT_DRIVER_ERROR;
#endif

    /* initialize each chain entry */
    chainEntry = exch->sgList[0];
    for (i = 0; i < SCGT_DMA_CHAIN_LEN; i++)
    {
        chainEntry[GTCORE_CE_BUF_ADD32] = 0;
        chainEntry[GTCORE_CE_TNS_CSR] = 0;
        chainEntry[GTCORE_CE_BUF_ADD64] = 0;

        chainEntry[GTCORE_CE_RESERVED_0] = direction << 24 | exchNum << 16 | i;  /* debugging purposes */        
        chainEntry[GTCORE_CE_RESERVED_1] = 0;
        chainEntry[GTCORE_CE_RESERVED_2] = 0;
        
        /* assumes memory is physically contiguous */
        nextAddr = exch->sgListPhysAddr[0] + (i+1) * GTCORE_CE_SIZE;
        chainEntry[GTCORE_CE_NEXT_ADD32] = (uint32) nextAddr;
        chainEntry[GTCORE_CE_NEXT_ADD64] = (uint32) (nextAddr >> 32);

        chainEntry += (GTCORE_CE_SIZE / 4);  /* move to next chain queue entry */
    }

    return SCGT_SUCCESS;
}




/*
 * gtcoreInitTrQueue()
 *     Allocate and align or free transaction queue
 */

uint32 gtcoreTrQueue(scgtDevice *dev, gtcoreExchMgrData *exchMgrData,
                     uint8 direction, uint8 doAlloc)
{
    uint32 qSize;
    uint32 i;
    uint32 *tqe;
#ifdef _MSC_VER
    UNREFERENCED_PARAMETER(direction);
#endif
    qSize = (GTCORE_EXCH_CNT * GTCORE_TQE_SIZE) + (GTCORE_CACHE_LINE_SIZE * 2);
    
    if (doAlloc == 0)
    {
        if (exchMgrData->trSpace != NULL)
        {
            /* unmap trQueue */
            if (exchMgrData->trPhysical)
                ksysUnmapVirtToBus(exchMgrData->trDmaHandle, exchMgrData->trQueue);

            /* free trSpace */
            if (exchMgrData->trSpace)
                ksysDma1Free(exchMgrData->trDmaHandle, exchMgrData->trSpace, qSize);
            exchMgrData->trSpace = NULL;
        }
        
        return SCGT_SUCCESS;
    }
    
    /* Allocate space for the Transaction Queues */
    /* Triple the size for alignment purposes */
    exchMgrData->trSpace = (uint32 *) ksysDma1Malloc(dev->mapData, 
                                                     &exchMgrData->trDmaHandle,
                                                     qSize);
    if (exchMgrData->trSpace == NULL)
        return SCGT_DRIVER_ERROR;
        
    exchMgrData->trQueue = gtcoreAlignAddr((void *) exchMgrData->trSpace, GTCORE_CACHE_LINE_SIZE);

    exchMgrData->trPhysical = (uint64) ((uintpsize) ksysMapVirtToBus(exchMgrData->trDmaHandle,
                                                                     exchMgrData->trQueue,
                                                                     GTCORE_EXCH_CNT * GTCORE_TQE_SIZE));
                                                        
    if (exchMgrData->trPhysical == 0)
    {
        ksysDma1Free(exchMgrData->trDmaHandle, exchMgrData->trSpace, qSize);
        return SCGT_DRIVER_ERROR;
    }
    
    /* initialize some members of each transaction queue entry 
       that won't change on a transfer by transfer basis */
    tqe = exchMgrData->trQueue;
    for (i = 0; i < GTCORE_EXCH_CNT; i++)
    {
        tqe[GTCORE_TQE_NI_CTL] = 0;
        tqe[GTCORE_TQE_RESERVED_0] = 0;
        tqe[GTCORE_TQE_RESERVED_1] = 0;
        tqe += (GTCORE_TQE_SIZE / 4);
    }

    return SCGT_SUCCESS;
}

/*
 * gtcoreInitIntrQ()
 *     initialize device interrupt Q
 */
 
uint32 gtcoreInitIntrQ(scgtDevice *dev)
{
    dev->intrQData.intrQ = ksysMalloc(sizeof(scgtInterrupt) * GTCORE_INTR_Q_SIZE);
    
    if (dev->intrQData.intrQ == NULL)
        return SCGT_INSUFFICIENT_RESOURCES;

    dev->intrQData.head = 0;
    dev->intrQData.seqNum = 0;
    
    dev->hwNHIQIntCntrVal = scgtReadCReg(dev, GTCORE_R_NHIQ_INT_CNTR);
    
    return SCGT_SUCCESS;
}


/*
 * gtcoreDestroyIntrQ()
 *     free device interrupt Q
 */
 
void gtcoreDestroyIntrQ(scgtDevice *dev)
{
    if (dev->intrQData.intrQ)
        ksysFree(dev->intrQData.intrQ, sizeof(scgtInterrupt) * GTCORE_INTR_Q_SIZE);
}

/*
 * gtcoreInitDevice()
 *     initialize gt device
 */
 
void gtcoreInitDevice(scgtDevice *dev)
{
    uint32 i, j;
    
    dev->popMemSize = gtcoreGetPopMemSize(dev);
    
    /* setup watchdog timer value */
    scgtWriteCReg(dev, GTCORE_R_TC_WDT_VAL, GTCORE_DEFAULT_WDT_VAL);
    
    /* initialize device statistics */
    dev->stats[0] = SCGT_STATS_NUM_STATS;
    dev->stats[1] = sizeof(gtcoreStatNames);
    for (i = 2; i < SCGT_STATS_NUM_STATS; i++)
        dev->stats[i] = 0;
    
    gtcoreStatNameIndex[0] = 0;    
    for (i = 0, j = 1; i < sizeof(gtcoreStatNames) && j < SCGT_STATS_NUM_STATS + 1; i++)
    {
        if (gtcoreStatNames[i] == ',')
        {
            gtcoreStatNameIndex[j++] = i + 1;
            i++;
        }
        else if (gtcoreStatNames[i] == '\0')  /* last one points to NULL character */
        {
            gtcoreStatNameIndex[j] = i + 1;
            break;
        }
    }
    
    /* enable interrupts - hope you are ready ;-) */ 
    scgtWriteCReg(dev, GTCORE_R_INT_CSR, GTCORE_R_ALL_INT_EN);
}


/*
 * gtcoreSizeOfSCGTDevice()
 *     Used for debugging precompiled core vs final linked driver.
 */
 
uint32 gtcoreSizeOfSCGTDevice()
{
    return sizeof(scgtDevice);
}
