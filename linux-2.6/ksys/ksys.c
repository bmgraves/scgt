/******************************************************************************/
/*                                  KSYS                                      */
/******************************************************************************/
/*                                                                            */
/* Copyright (c) 2002-2007 Curtiss-Wright Controls.                           */
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

/*
 * ksys.c
 *     Linux ksys implementation
 */
 
#include "ksys.h"
#include "systypes.h"

#include <asm/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/version.h>

char * FILE_REV_KSYS_C = "5";    /* 12/22/2008 */

/********************* prototypes ***********************/

void ksysSemBTimerCallback(ksysSemB *p_sem);


/***********************************************************/
/************** cache flush/invalidate *********************/
/***********************************************************/


#ifdef KSYS_USE_DMA_ALLOC
void ksysCacheFlush(void *dmaHandle, void *ptr, uint32 size)
{
#if 0
    ksysDmaHandleData *dmaHandleData;
    
    dmaHandleData = (ksysDmaHandleData *) dmaHandle;    
    pci_dma_sync_single(dmaHandleData->pd, dmaHandleData->dmaHandle, size, PCI_DMA_TODEVICE);
#endif
}

void ksysCacheInvalidate(void *dmaHandle, void *ptr, uint32 size)
{
#if 0
    ksysDmaHandleData *dmaHandleData;
    
    dmaHandleData = (ksysDmaHandleData *) dmaHandle;    
    pci_dma_sync_single(dmaHandleData->pd, dmaHandleData->dmaHandle, size, PCI_DMA_FROMDEVICE);
#endif
}

#else

void ksysCacheFlush(void *dmaHandle, void *ptr, uint32 size)
{
    dma_cache_wback(ptr, size);
}

void ksysCacheInvalidate(void *dmaHandle, void *ptr, uint32 size)
{
    dma_cache_inv(ptr, size);
}

#endif

/***********************************************************/
/***************** copy from/to user ***********************/
/***********************************************************/

void ksysCopyToUser(void *usrPtrDest, void *srcPtr, uint32 numBytes)
{
    unsigned long ret;  /* avoid warnings */
    ret = copy_to_user(usrPtrDest, srcPtr, numBytes);
}

void ksysCopyFromUser(void *kernPtrDest, void *srcPtr, uint32 numBytes)
{
    unsigned long ret;  /* avoid warnings */
    ret = copy_from_user(kernPtrDest, srcPtr, numBytes);
}


/*************************************************************/
/****************** register write/read **********************/
/*************************************************************/

void ksysWriteReg(void *pRegs, uint32 offset, uint32 val)
{
    /* you would use the following if our cards didn't do register swapping */
    /* writel(cpu_to_le32(val), (void *)(((char *)pRegs) + offset)); */
    writel(val, (void *)(((char *)pRegs) + offset));
}


uint32 ksysReadReg(void *pRegs, uint32 offset)
{
    /* you would use the following if our cards didn't do register swapping */
    /* return le32_to_cpu(readl((void *)(((char *)pRegs) + offset))); */
    return readl((void *)(((char *)pRegs) + offset));
}

/********************************************************/
/****************** memory allocation *******************/
/********************************************************/

void *ksysMalloc(uint32 nbytes)
{
    return kmalloc(nbytes, GFP_KERNEL);
}
 
void ksysFree(void *p, uint32 nbytes)
{
    kfree(p);
}


#ifdef KSYS_USE_DMA_ALLOC
void *ksysDmaMalloc(void *mapData, void **dmaHandle, uint32 numBytes)
{
    ksysDmaHandleData *dmaHandleData;
    void *pBuf;
    
    dmaHandleData = kmalloc(sizeof(ksysDmaHandleData), GFP_KERNEL);
    *dmaHandle = dmaHandleData;

    dmaHandleData->pd = (struct pci_dev *) mapData;

    pBuf = pci_alloc_consistent(dmaHandleData->pd, numBytes, &dmaHandleData->dmaHandle);
    dmaHandleData->pBuf = pBuf;

    return pBuf;
}

void ksysDmaFree(void *dmaHandle, void *pBuf, uint32 numBytes)
{
    ksysDmaHandleData *dmaHandleData;
    
    dmaHandleData = (ksysDmaHandleData *) dmaHandle;
    pci_free_consistent(dmaHandleData->pd, numBytes, 
                        pBuf, dmaHandleData->dmaHandle);
                        
    kfree(dmaHandle);
}


void *ksysDmaMalloc_using_kmalloc(void *mapData, void **dmaHandle, uint32 numBytes)
{
    ksysDmaHandleData *dmaHandleData;
    void *pBuf;
    
    dmaHandleData = kmalloc(sizeof(ksysDmaHandleData), GFP_KERNEL);
    *dmaHandle = dmaHandleData;
    
    dmaHandleData->pd = (struct pci_dev *) mapData;
    
    pBuf = kmalloc(numBytes, GFP_KERNEL);
    dmaHandleData->pBuf = pBuf;
    dmaHandleData->dmaHandle = pci_map_single(dmaHandleData->pd, pBuf, numBytes, PCI_DMA_BIDIRECTIONAL);

    return pBuf;
}

void ksysDmaFree_using_kmalloc(void *dmaHandle, void *pBuf, uint32 numBytes)
{
    ksysDmaHandleData *dmaHandleData;
    
    dmaHandleData = (ksysDmaHandleData *) dmaHandle;
                        
    pci_unmap_single(dmaHandleData->pd, dmaHandleData->dmaHandle, numBytes, PCI_DMA_BIDIRECTIONAL);
    kfree(pBuf);
    kfree(dmaHandle);
}

#endif

/***********************************************************/
/************ Vitual to bus addr translation ***************/
/***********************************************************/

#ifdef KSYS_USE_DMA_ALLOC

uint32 ksysMapVirtToBus(void *dmaHandle, void *ptr, uint32 numBytes)
{
    ksysDmaHandleData *dmaHandleData;
    
    dmaHandleData = (ksysDmaHandleData *) dmaHandle;
    
    return dmaHandleData->dmaHandle + ((uintpsize) ptr - (uintpsize) dmaHandleData->pBuf);
}

void ksysUnmapVirtToBus(void *dmaHandle, void *ptr)
{
    /* do nothing.. DmaFree does unmap */
}

#else

uint32 ksysMapVirtToBus(void *dmaHandle, void *ptr, uint32 numBytes)
{
    return (uint32) virt_to_bus(ptr);
}

void ksysUnmapVirtToBus(void *dmaHandle, void *ptr)
{
}

#endif

/************************************************************/
/*********** timeouting semaphore implementation ************/
/************************************************************/

#if KSYS_HAVE_TO_SEM        /************ down_timeout available *************/
/*
 * Create a timed binary semaphore.
 * The initial state is "taken".
 */
 
void ksysSemBCreate(ksysSemB *p_sem)
{
    p_sem->sem = (struct semaphore *) kmalloc(sizeof(struct semaphore), GFP_KERNEL);
    sema_init(p_sem->sem, 0);
}

/*
 * Destroy a timed semaphore.
 */
 
void ksysSemBDestroy(ksysSemB *p_sem)
{
    kfree(p_sem->sem);
}


/*
 * Perform a timed wait on a semaphore
 * returns 0 on success, 1 on timeout.
 */
 
uint32 ksysSemBTakeWithTimeout(ksysSemB *p_sem, uint32 to)
{
    if (down_timeout(p_sem->sem, to))
        return 1;
    
    return 0;
}


/*
 * Give a semaphore. 
 * returns 0 on success, 1 if already given or give failed.
 */
 
uint32 ksysSemBGive(ksysSemB *p_sem)
{
    up(p_sem->sem);
    return 0;
}


#else  /******************* no down_timeout available ***********************/
/*
 * Create a timed binary semaphore.
 * The initial state is "taken".
 */
 
void ksysSemBCreate(ksysSemB *p_sem)
{
    p_sem->sem = (struct semaphore *) kmalloc(sizeof(struct semaphore), GFP_KERNEL);
    sema_init(p_sem->sem, 0);
    p_sem->given = 0;
}
 

/*
 * Destroy a timed semaphore.
 */
 
void ksysSemBDestroy(ksysSemB *p_sem)
{
    kfree(p_sem->sem);
}


/*
 * Perform a timed wait on a semaphore
 * returns 0 on success, 1 on timeout.
 */
 
uint32 ksysSemBTakeWithTimeout(ksysSemB *p_sem, uint32 to)
{
    struct timer_list stimer;
      
    if (!down_trylock(p_sem->sem))
    {
        /* we took it and passed through - means isr already happened */
        p_sem->given = 1;
        return 0; 
    }

    p_sem->timedOut = 0;
    init_timer(&stimer);
    stimer.data     = (unsigned long) p_sem;
    stimer.function = (void *) ksysSemBTimerCallback;
    stimer.expires  = get_jiffies_64() + to;
    add_timer(&stimer);
    down(p_sem->sem);   /* wait for isr or timeout */
    p_sem->given = 1;
    del_timer(&stimer);

    return p_sem->timedOut;
}


/*
 * Give a semaphore. 
 * returns 0 on success, 1 if already given or give failed.
 */
 
uint32 ksysSemBGive(ksysSemB *p_sem)
{
    if (p_sem->given == 0) /* give it back only if not given back yet */
    {
        p_sem->given = 1;
        up(p_sem->sem);  
        return 0;
    }

    return 1;
}

void ksysSemBTimerCallback(ksysSemB *p_sem)
{
    if (p_sem == NULL)
        return;
   
    p_sem->timedOut = 1;
    p_sem->given = 1;
    
    if (atomic_read(&(p_sem->sem->count)) < 0) /* somebody waiting */
        up(p_sem->sem);
}
#endif

/*****************************************************************/
/*************** simple semaphore implementation *****************/
/*****************************************************************/

/*
 * ksysSemSCreate()
 *     create simple semaphore
 *     initial state is "taken"
 */
 
void ksysSemSCreate(ksysSemS *pSemS)
{
    *pSemS = (struct semaphore *) kmalloc(sizeof(struct semaphore), GFP_KERNEL);
    sema_init(*pSemS, 0);
}

void ksysSemSDestroy(ksysSemS *pSemS)
{
    kfree(*pSemS);
}

void ksysSemSTake(ksysSemS *pSemS)
{
    down(*pSemS);
}

void ksysSemSGive(ksysSemS *pSemS)
{
    up(*pSemS);
}

/*****************************************************************/
/************************** sleep ********************************/
/*****************************************************************/

/*
 * Delay for specified number of microseconds.
 *
 * NOTE: Delays >40us will delay and then sleep for 1 extra jiffy.
 */
void ksysUSleep(uint32 usec)
{
    if (usec > 1000L)
    {
        mdelay(usec/1000L);
    }
    else
    {
        udelay(usec);
    }
    
    if (usec > 40)
    {
        schedule_timeout(1);
    }
}

