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
/*    Module      : gtcorexfer.c                                              */
/*    Description : core transfer functions                                   */
/*                                                                            */
/******************************************************************************/


/************************************************************************/
/****************************  INCLUDES  ********************************/
/************************************************************************/


#include "gtcore.h"
#include "gtcoreTypes.h"
#include "gtucore.h"
#include "ksys.h"
#include "scgtdrv.h"


char * FILE_REV_GTCOREXFER_C = "7";  /* 05/15/14 */


/*************************************************************/
/************************ PROTOTYPES *************************/
/*************************************************************/

static uint32 gtcoreHandleInterrupt2(scgtDevice *dev, uint32 intCSR);
static void gtcoreEnqueueNetIntrs(scgtDevice *dev);

void gtcoreDmaEnable(scgtDevice *dev, uint8 direction);
void gtcoreDmaDisable(scgtDevice *dev, uint8 direction);
void gtcoreDmaAbort(scgtDevice *dev, uint8 direction);
void gtcoreDmaDestroy(scgtDevice *dev);

/*
 * gtcoreGetExchange()
 */
 
gtcoreExch * gtcoreGetExchange(scgtDevice *dev, uint8 direction)
{
    gtcoreExchMgrData *exchMgrData;
    gtcoreExch *exch;
    
    exchMgrData = (direction == GTCORE_WRITE)? &dev->wexch : &dev->rexch;
    
    exch = &(exchMgrData->exchQ[exchMgrData->headIndex]);
    exch->state = GTCORE_EXCH_NOT_DONE;
    return exch;
}

/*
 * gtcoreTransfer()
 */

void gtcoreTransfer(scgtDevice *dev, gtcoreExch *exch, uint8 direction)
{
    gtcoreExchMgrData *exchMgrData;
    uint32 transQCtrlReg;
    uint32 newIndex;
    
    if (direction == GTCORE_WRITE)
    {
        exchMgrData = &dev->wexch;
        transQCtrlReg = GTCORE_R_TQ_CTL_TC0;
        
        if (exch->intr)
        {
            exch->tqe[GTCORE_TQE_NI_CTL] = (exch->intr->id << 8) | GTCORE_TQE_NET_INT;

            if (exch->intr->type == SCGT_UNICAST_INTR)  /* set the int type bit */
                exch->tqe[GTCORE_TQE_NI_CTL] |= GTCORE_TQE_NET_INT_TYPE;
            
            exch->tqe[GTCORE_TQE_NI_VECT] = exch->intr->val;
        }
        else
        {
            exch->tqe[GTCORE_TQE_NI_CTL] = 0;
        }
    }
    else
    {
        exchMgrData = &dev->rexch;
        transQCtrlReg = GTCORE_R_TQ_CTL_TC1;
    }
    
    exch->tqe[GTCORE_TQE_TNS_CSR] = (exch->bytesToTransfer >> 2);
    
    if ((exch->flags & SCGT_RW_DMA_BYTE_SWAP))
        exch->tqe[GTCORE_TQE_TNS_CSR] |= GTCORE_TQE_IDB_SWAP_OR;
    
    if ((exch->flags & SCGT_RW_DMA_WORD_SWAP))
        exch->tqe[GTCORE_TQE_TNS_CSR] |= GTCORE_TQE_IDW_SWAP_OR;
    
    exch->tqe[GTCORE_TQE_SMO] = exch->gtMemoryOffset;
    
    ksysCacheFlush(exchMgrData->trDmaHandle, (void *) exch->tqe, GTCORE_TQE_SIZE);
    
    /* update the head pointer for the queue index */
    newIndex = exchMgrData->headIndex + 1;
    exchMgrData->headIndex = newIndex % GTCORE_EXCH_CNT;
    
    /* tell hardware to go */
    scgtWriteCReg(dev, transQCtrlReg, GTCORE_R_TQ_PRSRV | newIndex);
}

/*
 * gtcoreCancelTransfer()
 *     Cancels a currently pending or queued transfer.
 */
void gtcoreCancelTransfer(scgtDevice *dev, gtcoreExch *exch, uint8 direction)
{
    uint32 hwConsumerIndex;

    /* Pause the transaction queues */
    gtcoreDmaDisable(dev, direction);

    hwConsumerIndex = (direction == GTCORE_WRITE)? scgtReadCReg(dev, GTCORE_R_TQ_CTL_TC0) :
                                                   scgtReadCReg(dev, GTCORE_R_TQ_CTL_TC1);
                                                 
    hwConsumerIndex = (hwConsumerIndex & GTCORE_R_TQ_CON_IDX) >> 8;
    

    if (hwConsumerIndex == exch->exchQIndex)
    {
        /* We are supposed to cancel the transaction in progress */
        exch->state = GTCORE_EXCH_DONE; /* Say it's done */

        /* Calculate the partial data transferred */
        if (direction == GTCORE_WRITE)
        {
            exch->bytesTransferred = exch->bytesToTransfer - 
                                     (scgtReadCReg(dev, GTCORE_R_TNS_LEN_TC0) << 2);
        }
        else
        {
            exch->bytesTransferred = exch->bytesToTransfer - 
                                     (scgtReadCReg(dev, GTCORE_R_TNS_LEN_TC1) << 2);
        }

        gtcoreDmaAbort(dev, direction);
    }
    else
    {
        /* Due to our semaphoring scheme, only 1 transaction is
           posted to the hardware at a time... This means that this
           transaction completed before we could cancel it.  To facilitate
           future development.. ie. non-blocking things, we will assume we
           can post more than one and tell the hardware to skip this entry
           assuming it hasn't gotten to it yet.  This won't harm anything
           if it is not true. */

        exch->state = GTCORE_EXCH_DONE; /* Say it's done */
        exch->tqe[GTCORE_TQE_TNS_CSR] |= GTCORE_TQE_SKP_ENTRY;

        ksysCacheFlush(((direction == GTCORE_WRITE)? &dev->wexch : &dev->rexch)->trDmaHandle, 
                       (void *) &(exch->tqe[GTCORE_TQE_TNS_CSR]), 
                       sizeof(exch->tqe[GTCORE_TQE_TNS_CSR]));

    }

    /* re-enable the transaction controller */
    gtcoreDmaEnable(dev, direction);
}

/****************************************************************/
/********************** INTERRUPT HANDLING **********************/
/****************************************************************/

/*
 * gtcoreHandleInterrupt()
 *     returns 1 if handled
 *             2 if handled and you should call gtcoreCompleteDMA()
 *             3 if interrupt has been added to the queue.
 *             0 if not our board
 */
 
uint32 gtcoreHandleInterrupt(scgtDevice *dev)
{
    uint32 interruptType;
    uint32 intCSR;
    uint32 hwConsumer;
    uint32 i;
    uint32 compHeadIndex;
    uint32 exchState;
    
    interruptType = 0;
    intCSR = scgtReadCReg(dev, GTCORE_R_INT_CSR);

    while (intCSR & 0xFF)
    {   
        /* clear interrupt */
        scgtWriteCReg(dev, GTCORE_R_INT_CSR, intCSR);
        
        /* write interrupt */
        if (intCSR & GTCORE_R_TC0_INT)
        {
            hwConsumer = (scgtReadCReg(dev, GTCORE_R_TQ_CTL_TC0) & 0x1F00) >> 8;
            
            /* transactions completed are from SW's tail index 
               up to HW's consumer index */
               
            for (i = dev->wexch.tailIndex; i != hwConsumer; 
                 i = (i + 1) % GTCORE_EXCH_CNT)
            {
                /* Do ONLY exchanges that aren't marked as DONE */
                /* An exchange could already be set to EXCH_DONE
                   due to timeout of semaphore. */
            
                exchState = dev->wexch.exchQ[i].state;
                compHeadIndex = dev->completedHeadIndex;
            
                if (exchState != GTCORE_EXCH_DONE)
                {
                    /* add the ith exchange to the completion queue 
                       for gtcoreCompleteDMA */
                    dev->completedExchQ[compHeadIndex] = &dev->wexch.exchQ[i];
                    dev->completedHeadIndex = (compHeadIndex + 1) % (2 * GTCORE_EXCH_CNT); 
                }
                dev->wexch.tailIndex = (i + 1) % GTCORE_EXCH_CNT;
            }
            
            interruptType |= GTCORE_ISR_DMA;
            dev->stats[SCGT_STATS_TC0_INTRS]++;
        }
        
        /* read interrupt (same was write but for TC1) */
        if (intCSR & GTCORE_R_TC1_INT)
        {
            hwConsumer = (scgtReadCReg(dev, GTCORE_R_TQ_CTL_TC1) & 0x1F00) >> 8;
               
            for (i = dev->rexch.tailIndex; i != hwConsumer; 
                 i = (i + 1) % GTCORE_EXCH_CNT)
            {            
                exchState = dev->rexch.exchQ[i].state;
                compHeadIndex = dev->completedHeadIndex;
            
                if (exchState != GTCORE_EXCH_DONE)
                {
                    /* add the ith exchange to the completion queue 
                       for gtcoreCompleteDMA */
                    dev->completedExchQ[compHeadIndex] = &dev->rexch.exchQ[i];
                    dev->completedHeadIndex = (compHeadIndex + 1) % (2 * GTCORE_EXCH_CNT); 
                }
                dev->rexch.tailIndex = (i + 1) % GTCORE_EXCH_CNT;
            }
            
            interruptType |= GTCORE_ISR_DMA;
            dev->stats[SCGT_STATS_TC1_INTRS]++;
        }

        /* received network interrupt */
        if (intCSR & GTCORE_R_RX_NET_INT)
        {
            gtcoreEnqueueNetIntrs(dev);
            interruptType |= GTCORE_ISR_QUEUED_INTR;
        }
        
        /* handle other less likely interrupts */
        if (intCSR & 0xFF & ~(GTCORE_R_TC0_INT | GTCORE_R_TC1_INT | GTCORE_R_RX_NET_INT))
        {
            interruptType |= gtcoreHandleInterrupt2(dev, intCSR);
        }
        
        dev->stats[SCGT_STATS_INTRS]++;
        
        intCSR = scgtReadCReg(dev, GTCORE_R_INT_CSR);  /* next interrupt */
    }
    
    if (interruptType)
        dev->stats[SCGT_STATS_ISR]++;
    
    return interruptType;
}

/*
 * gtcoreEnqueueNetIntrs()
 *     gtcoreHandleInterrupt helper function for network interrupt
 *     handling.
 */
 
static void gtcoreEnqueueNetIntrs(scgtDevice *dev)
{
    scgtInterrupt intr;
    uint32 hwNHIQIntCntrNewVal;
    uint32 iqID;
    uint32 offset;
    uint32 i;
    uint32 rollOver = 0;

    hwNHIQIntCntrNewVal = scgtReadCReg(dev, GTCORE_R_NHIQ_INT_CNTR);
    
    if (dev->hwNHIQIntCntrVal == hwNHIQIntCntrNewVal)
        return;

//!!!!!!!!!! begin fix for incorrect interrupt cntr read.
    i = scgtReadCReg(dev, GTCORE_R_NHIQ_INT_CNTR);
    
    if (i < hwNHIQIntCntrNewVal)  //! try to fix problem with register read!
    {
        scgtReadCReg(dev, 0xf0);
        dev->stats[SCGT_STATS_NET_INT_CNT_FIX]++;
        dev->stats[SCGT_STATS_NET_INT_CNT_0] = hwNHIQIntCntrNewVal;
        dev->stats[SCGT_STATS_NET_INT_CNT_1] = i;
        hwNHIQIntCntrNewVal = i;
    }
//!!!!!!!!!! end fix

//!!!!!! detect and log problem in stats    
    if (hwNHIQIntCntrNewVal < dev->hwNHIQIntCntrVal  && 
        ((dev->hwNHIQIntCntrVal - hwNHIQIntCntrNewVal) < 256))
    {
        scgtReadCReg(dev, 0xf1);
        dev->stats[SCGT_STATS_SW_NET_INT_CNT_VAL] = dev->hwNHIQIntCntrVal;
        dev->stats[SCGT_STATS_HW_NET_INT_CNT_VAL] = hwNHIQIntCntrNewVal;
        dev->stats[SCGT_STATS_NET_INT_CNT_FIX_FAILED]++;
        dev->hwNHIQIntCntrVal = hwNHIQIntCntrNewVal;
        return;
    }
//!!!!!! end problem detection.     
    
    if ((hwNHIQIntCntrNewVal - dev->hwNHIQIntCntrVal) > 256)
    {
        if (hwNHIQIntCntrNewVal > dev->hwNHIQIntCntrVal)
        {
            /* we missed a queues worth - 
               add DRIVER_MISSED_INTERRUPTS error interrupt 
               and catch up */
            intr.type = SCGT_ERROR_INTR;
            intr.val = SCGT_DRIVER_MISSED_INTERRUPTS;
            intr.id = 0;
            intr.sourceNodeID = 0;
            intr.seqNum = hwNHIQIntCntrNewVal - dev->hwNHIQIntCntrVal;  /* for fun */
            gtcorePutIntr(dev, &intr);
            dev->hwNHIQIntCntrVal = hwNHIQIntCntrNewVal - 1;
        }
        else if ((dev->hwNHIQIntCntrVal - hwNHIQIntCntrNewVal) > 0xFFFF)
        {
           /* hw count rolled over */
            rollOver = 1;
            hwNHIQIntCntrNewVal = (uint32) -1;
            
            if ((hwNHIQIntCntrNewVal - dev->hwNHIQIntCntrVal) > 256)
                dev->hwNHIQIntCntrVal = hwNHIQIntCntrNewVal - 1;
        }
        else
        {
            /* register read problem */
            dev->stats[SCGT_STATS_NET_INT_CNT_FIX_FAILED]++;
            dev->hwNHIQIntCntrVal = hwNHIQIntCntrNewVal;
        }
    }
#ifdef _MSC_VER
#pragma warning(push)  
#pragma warning(disable:4127) 
#endif
    while (1)
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    {

        for (i = dev->hwNHIQIntCntrVal; i < hwNHIQIntCntrNewVal; i++)
        {
            offset = GTCORE_NM_NHI_QID + (i & 0xFF) * 8;

            iqID = scgtReadNMReg(dev, offset + 4);

            if ((iqID & GTCORE_NM_HI_TYPE))
            {
                /* Broadcast interrupt */
                intr.type = SCGT_BROADCAST_INTR;
                intr.id = (iqID & 0x1F00) >> 8;
            }
            else
            {
                /* Unicast interrupt */
                intr.type = SCGT_UNICAST_INTR;
                intr.id = 0;
            }

            intr.sourceNodeID = iqID & 0xFF;
            intr.val = scgtReadNMReg(dev, offset);
            intr.seqNum = ((iqID & 0xFFFE0000) >> 9) | (i & 0xFF);

            gtcorePutIntr(dev, &intr);

            dev->stats[SCGT_STATS_NET_INTRS]++;
        }

        if (rollOver)
        {
            dev->hwNHIQIntCntrVal = 0;
            rollOver = 0;
            hwNHIQIntCntrNewVal = scgtReadCReg(dev, GTCORE_R_NHIQ_INT_CNTR);
            dev->stats[SCGT_STATS_NET_INT_CNT_ROLL]++;
            continue;
        }
        else
        {        
            dev->hwNHIQIntCntrVal = hwNHIQIntCntrNewVal;
            break;
        }
    }

}



/*
 * gtcoreHandleInterrupt2()
 *     returns 1 if handled
 *             3 if interrupt has been added to the queue.
 *             0 if not our board
 */
 
static uint32 gtcoreHandleInterrupt2(scgtDevice *dev, uint32 intCSR)
{
    uint32 interruptType = 0;
    scgtInterrupt intr;
    
    /* link error interrupt */
    if (intCSR & GTCORE_R_LNK_ERR_INT)
    {
        dev->stats[SCGT_STATS_LINK_ERRORS]++;
        intr.type = SCGT_ERROR_INTR;
        intr.val = SCGT_LINK_ERROR;
        intr.id = 0;
        intr.sourceNodeID = 0;
        gtcorePutIntr(dev, &intr);

        interruptType |= GTCORE_ISR_QUEUED_INTR;
        
        /* clear the LINK_STAT bit(s) telling type of link error */
        scgtWriteCReg(dev, GTCORE_R_LINK_STAT, scgtReadCReg(dev, GTCORE_R_LINK_STAT));
    }

    return interruptType;
}

/**************************************************************************/
/**************** THIS IS INTERRUPT ROUTINE CONTINUATION ******************/
/************ executed as DPC if direct call can not be done **************/
/**************************************************************************/

/*
**   function:     gtcoreCompleteDMA
**   description:  Responsible for completing and cleaning up DMA transactions.
**                 For each exchange in the "completed" queue:
**                   Updates exchange's status, transferCount, and flags.
**                   Changes exchange's state to EXCH_DONE.
**                   Gives exchange's compSem.
*/
void gtcoreCompleteDMA(scgtDevice *dev)
{
    gtcoreExch *exch;
    uint32 compTailIndex;
    uint32 tqeTNS_CSR;
    uint32 status;

    /* Release the semaphores for the transactions completed in the ISR */
    while (dev->completedHeadIndex != (compTailIndex = dev->completedTailIndex))
    {
        exch = dev->completedExchQ[compTailIndex];
        
        /* Invalidate the cache line for this transaction queue entry */
        ksysCacheInvalidate((exch->direction == GTCORE_WRITE)? dev->wexch.trDmaHandle : dev->rexch.trDmaHandle,
                            (void *) exch->tqe, GTCORE_TQE_SIZE);

        tqeTNS_CSR = exch->tqe[GTCORE_TQE_TNS_CSR];


        if (tqeTNS_CSR & GTCORE_TQE_FIXED_1)
        {
            /* write-back occured */
            exch->bytesTransferred = exch->bytesToTransfer - ((tqeTNS_CSR & GTCORE_TQE_LEN) << 2);

            if (tqeTNS_CSR & GTCORE_TQE_ERR)
            {
                status = SCGT_HARDWARE_ERROR;
            }
            else if (tqeTNS_CSR & GTCORE_TQE_TIMEOUT)
            {
                status = SCGT_TIMEOUT;
            }
            else
            {
                status = SCGT_SUCCESS;
            }
        }
        else
        {
            /* no write-back -- it's either disabled or not available in firmware version */
            exch->bytesTransferred = exch->bytesToTransfer;
            status = SCGT_SUCCESS;
        }

        exch->status = status;
        exch->state = GTCORE_EXCH_DONE;
        dev->completedTailIndex = (compTailIndex + 1) % (2 * GTCORE_EXCH_CNT);

        ksysSemBGive(&exch->compSem);
    }
}



/*****************************************************************/
/******************* DMA engine management ***********************/
/*****************************************************************/

/*
 * gtcoreDmaEnable()
 *     enable DMA engine
 */
void gtcoreDmaEnable(scgtDevice *dev, uint8 direction)
{
    uint32 tqctl;
    
    if (direction == GTCORE_WRITE)
    {
        tqctl = scgtReadCReg(dev, GTCORE_R_TQ_CTL_TC0);
        if (!(tqctl & GTCORE_R_TQ_EN))
        {
            scgtWriteCReg(dev, GTCORE_R_TQ_CTL_TC0, 
                          (tqctl & ~GTCORE_R_TQ_PRSRV) | GTCORE_R_TQ_EN);
        }
    }
    else
    {
        tqctl = scgtReadCReg(dev, GTCORE_R_TQ_CTL_TC1);
        if (!(tqctl & GTCORE_R_TQ_EN))
        {
            scgtWriteCReg(dev, GTCORE_R_TQ_CTL_TC1,
                          (tqctl & ~GTCORE_R_TQ_PRSRV) | GTCORE_R_TQ_EN);
        }    
    }
}

/*
 * gtcoreDmaDisable()
 *     disable DMA engine
 */
void gtcoreDmaDisable(scgtDevice *dev, uint8 direction)
{
    uint32 tqctl;
    
    if (direction == GTCORE_WRITE)
    {
        tqctl = scgtReadCReg(dev, GTCORE_R_TQ_CTL_TC0);
        if (tqctl & GTCORE_R_TQ_EN)
        {
            scgtWriteCReg(dev, GTCORE_R_TQ_CTL_TC0, 
                          (tqctl & ~(GTCORE_R_TQ_EN | GTCORE_R_TQ_PRSRV)));
        }
    }
    else
    {
        tqctl = scgtReadCReg(dev, GTCORE_R_TQ_CTL_TC1);
        if (tqctl & GTCORE_R_TQ_EN)
        {
            scgtWriteCReg(dev, GTCORE_R_TQ_CTL_TC1, 
                          (tqctl & ~(GTCORE_R_TQ_EN | GTCORE_R_TQ_PRSRV)));
        }    
    }
}


/*
 * gtcoreDmaAbort()
 *     aborts the current transaction
 */
void gtcoreDmaAbort(scgtDevice *dev, uint8 direction)
{
    uint32 tqctl;
    
    if (direction == GTCORE_WRITE)
    {
        tqctl = scgtReadCReg(dev, GTCORE_R_TQ_CTL_TC0);

        /* Set the abort bit and head index for Producer index */
        scgtWriteCReg(dev, GTCORE_R_TQ_CTL_TC0, 
                      (tqctl & (~GTCORE_R_TQ_PRSRV)) | GTCORE_R_TQ_ABRT);
    }
    else
    {
        tqctl = scgtReadCReg(dev, GTCORE_R_TQ_CTL_TC1);

        scgtWriteCReg(dev, GTCORE_R_TQ_CTL_TC1, 
                      (tqctl & (~GTCORE_R_TQ_PRSRV)) | GTCORE_R_TQ_ABRT);
    }
}

/*
 * gtcoreDmaDestroy()
 *     disable transaction queues
 *     abort pending transactions
 */
void gtcoreDmaDestroy(scgtDevice *dev)
{
    gtcoreDmaDisable(dev, GTCORE_READ);
    gtcoreDmaAbort(dev, GTCORE_READ);
    
    gtcoreDmaDisable(dev, GTCORE_WRITE);
    gtcoreDmaAbort(dev, GTCORE_WRITE);
}
