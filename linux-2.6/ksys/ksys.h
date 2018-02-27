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
 * ksys.h
 *     Linux 2.6 ksys implementation
 */
 
#ifndef __K_SYS_H__
#define __K_SYS_H__

/*********************************/
/********* INCLUDE FILES *********/
/*********************************/

#include "systypes.h"
#include <asm/io.h>

#include <asm/uaccess.h>
#include <linux/pci.h>
#include <linux/version.h>

#define FILE_REV_KSYS_H    "6"     /* 05/12/2011 */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
  #define KSYS_HAVE_TO_SEM     0
#else
  #define KSYS_HAVE_TO_SEM     1
  #include <linux/semaphore.h>  /* this file doesn't exist on older kernels */
#endif

/****************************************/
/********** TYPES & STRUCTURES **********/
/****************************************/

/**************************/
/* semaphores w/ timeouts */
/**************************/

/*
 * ksysSemS and ksysSemB have a pointer to a semaphore rather than just the 
 * struct semaphore member because the pre-compiled core binary files must 
 * remain kernel-structure-size-neutral.
 * Since the semaphore struct is a kernel defined structure, we must
 * not allow changes in it's size based on kernel version/config 
 * (and it does change size) effect the code compiled in the core lib in the 
 * binary distribution.
 */

typedef struct _ksysSemB
{
   struct semaphore    *sem;        /* actually counting semaphore */
   volatile int        given;
   volatile int        timedOut;
} ksysSemB;

/*******************************************************/
/***** simple semaphore (no timeout functionality) *****/
/*******************************************************/

typedef struct semaphore * ksysSemS;

/******************************/
/****** spin lock type ********/
/******************************/

/*
 * NOTE: On Linux, as with semaphores, a spin lock is implemented as a
 *       pointer.  This is because a spin lock varies in size in SMP vs non-SMP kernels.
 *       To keep the device structure the same size for a core compiled on each we use
 *       a pointer
 */
typedef spinlock_t *  ksysSpinLock;
typedef unsigned long ksysSpinLockFlags;


#define ksysSpinLockCreate(pSpinLock)              *(pSpinLock) = kmalloc(sizeof(spinlock_t), GFP_KERNEL); \
                                                   spin_lock_init(*(pSpinLock))
#define ksysSpinLockDestroy(pSpinLock)                  kfree(*(pSpinLock))
#define ksysSpinLockLock(pSpinLock, pSpinLockFlags)     spin_lock_irqsave(*(pSpinLock), *(pSpinLockFlags))
#define ksysSpinLockUnlock(pSpinLock, pSpinLockFlags)   spin_unlock_irqrestore(*(pSpinLock), *(pSpinLockFlags))



/****************** prototypes ********************/

void ksysCacheFlush(void *dmaHandle, void *ptr, uint32 size);
void ksysCacheInvalidate(void *dmaHandle, void *ptr, uint32 size);
void ksysCopyToUser(void *usrPtrDest, void *srcPtr, uint32 numBytes);
void ksysCopyFromUser(void *kernPtrDest, void *srcPtr, uint32 numBytes);

/*
 * void *mapData is provided to allow different OSes the ability
 * to pass these functions whatever data they need to do virt-to-bus
 * mapping.  This is a product and OS independant way to get this
 * information to the function.  The core for the product then expects
 * a void *mapData in the device structure to pass to this function.
 */
/* for Linux, mapData should be the pci_dev, dmaHandle will contain
   a pointer to ksysDmaHandleData */

typedef struct
{
    struct pci_dev *pd;
    dma_addr_t dmaHandle;
    void *pBuf;
} ksysDmaHandleData;



#define KSYS_USE_DMA_ALLOC

#ifdef KSYS_USE_DMA_ALLOC
/* queue allocation */
#define ksysDma1Malloc  ksysDmaMalloc
#define ksysDma1Free    ksysDmaFree
/* chain allocation */
#define ksysDma2Malloc  ksysDmaMalloc
#define ksysDma2Free    ksysDmaFree

void *ksysDmaMalloc(void *mapData, void **dmaHandle, uint32 numBytes);
void ksysDmaFree(void *dmaHandle, void *pBuf, uint32 numBytes);
#endif


uint32 ksysMapVirtToBus(void *dmaHandle, void *ptr, uint32 numBytes);
void ksysUnmapVirtToBus(void *dmaHandle, void *ptr);

void ksysWriteReg(void *pRegs, uint32 offset, uint32 val);
uint32 ksysReadReg(void *pRegs, uint32 offset);

void *ksysMalloc(uint32 nbytes);
void ksysFree(void *p, uint32 nbytes);

/* timeouting binary semaphore */

void ksysSemBCreate(ksysSemB *p_sem);
void ksysSemBDestroy(ksysSemB *p_sem);
uint32 ksysSemBTakeWithTimeout(ksysSemB *p_sem, uint32 to);
uint32 ksysSemBGive(ksysSemB *p_sem);

/* simple semaphore */
void ksysSemSCreate(ksysSemS *pSemS);
void ksysSemSDestroy(ksysSemS *pSemS);
void ksysSemSTake(ksysSemS *pSemS);
void ksysSemSGive(ksysSemS *pSemS);
uint32 ksysSemSCount(ksysSemS *pSemS);

void ksysUSleep(uint32 usec);


#endif /* __K_SYS_H__ */
