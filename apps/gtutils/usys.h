/******************************************************************************/
/*                              SCRAMNet GT                                   */
/******************************************************************************/
/*                                                                            */
/* Copyright (c) 2002-2005 Curtiss-Wright Controls.                           */
/*               support@systran.com 800-252-5601 (U.S. only) 937-252-5601    */
/*                                                                            */
/* This library is free software; you can redistribute it and/or              */
/* modify it under the terms of the GNU Lesser General Public                 */
/* License as published by the Free Software Foundation; either               */
/* version 2.1 of the License, or (at your option) any later version.         */
/*                                                                            */
/* See the GNU Lesser General Public License for more details.                */
/*                                                                            */
/******************************************************************************/

#ifndef _USYS_H_
#define _USYS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_REV_USYS_H   "2011/09/08"

/************************************************************************/
/*    Module      :  usys.h                                             */
/*    Description:  Code used to abstract OS specifics from the         */
/*                  application programs.                               */
/************************************************************************/
#if !defined PLATFORM_UNIX && !defined PLATFORM_WIN && !defined PLATFORM_VXWORKS && !defined PLATFORM_RTX && !defined PLATFORM_VISA
#error "Must define PLATFORM_UNIX, PLATFORM_WIN, PLATFORM_VXWORKS, PLATFORM_RTX or PLATFORM_VISA"
#endif

/******************************************************/
/********** INCLUDES **********************************/
/******************************************************/
#ifdef PLATFORM_UNIX   /*=================== OS SWITCH ===================*/
    #include <sys/types.h>    /* UNIX types             */
    #include <semaphore.h>    /* UNIX SVR4 semaphores   */
    #include <pthread.h>      /* POSIX threads support  */
    #include <sys/time.h>
    #include <signal.h>
    #include <stdlib.h>
#elif PLATFORM_WIN    /*=================== OS SWITCH ===================*/
    #include <windows.h>
    #include <time.h>         /* for clock_t */
#elif defined PLATFORM_VISA   /*=================== OS SWITCH ===================*/
    #include <windows.h>
    #include <time.h>         /* for clock_t */
#ifndef strtoull
	#define strtoull   strtoul64 
#endif
#elif PLATFORM_RTX    /*=================== OS SWITCH ===================*/
    #include <windows.h>
    #include <time.h>         /* for clock_t */
    #include <rtapi.h>
#elif PLATFORM_VXWORKS /*=================== OS SWITCH ===================*/
    #include "semaphore.h"
    #include "time.h"
    #include "stdlib.h"
#endif

#include "systypes.h"

/******************************************************/
/********** MEMORY FUNCTIONS ABSTRACTIONS *************/
/******************************************************/
#ifdef PLATFORM_UNIX   /*=================== OS SWITCH ===================*/
    #define usysMemMalloc(nBytes)  (void *)malloc(nBytes)
    #define usysMemFree(pBuf)      free(pBuf)
#elif PLATFORM_WIN     /*=================== OS SWITCH ===================*/
#if 0
    #define usysMemMalloc(nBytes)  (void*)GlobalAlloc(GMEM_FIXED, nBytes)
    #define usysMemFree(pBuf)      GlobalFree((HGLOBAL)pBuf)
#endif
    #define usysMemMalloc(nBytes)  (void *)malloc(nBytes)
    #define usysMemFree(pBuf)      free(pBuf)
#elif defined PLATFORM_VISA     /*=================== OS SWITCH ===================*/
    #define usysMemMalloc(nBytes)  (void *)malloc(nBytes)
    #define usysMemFree(pBuf)      free(pBuf)
#elif defined PLATFORM_RTX     /*=================== OS SWITCH ===================*/
    void *usysMemMalloc(uint32 nBytes);
    #define usysMemFree(pBuf)      RtFreeContiguousMemory(pBuf)
#elif PLATFORM_VXWORKS /*=================== OS SWITCH ===================*/
    #define usysMemMalloc(nBytes)  (void *)memalign(32, nBytes)
    #define usysMemFree(pBuf)      free((void*)pBuf)
#endif

/******************************************************/
/********** TIMING FUNCTIONS ABSTRACTIONS *************/
/******************************************************/
typedef struct {
        int       finishValid;
        uint32    startTimeSecs;
        uint32    startTimeUsecs;
        uint32    finishTimeSecs;
        uint32    finishTimeUsecs;
} usysMsTimeType;

void    usysMsTimeStart(usysMsTimeType *pTimer);
void    usysMsTimeStop(usysMsTimeType *pTimer);
uint32  usysMsTimeGetElapsed(usysMsTimeType *pTimer);
void    usysMsTimeDelay(uint32 milliSeconds);

/******************************************************/
/********** THREAD FUNCTIONS ABSTRACTIONS *************/
/******************************************************/
#define UPRIO_LOW                1
#define UPRIO_MED                2
#define UPRIO_HIGH               3
#define UPRIO_REALTIME           4

typedef struct {
    char        threadName[16];    /* used VxWorks and apps  */
  #ifdef PLATFORM_UNIX
    pthread_t   threadHandle;
  #elif PLATFORM_WIN || PLATFORM_RTX || PLATFORM_VISA
    void       *threadHandle;
  #endif                           /* VXWORKS need no Handle */
    void       *parameter;
    uint32      threadID;            /* unused with pthreads */
    int         priority;            /* unused with pthreads */
} usysThreadParams;

int usysKillThread(usysThreadParams *pt);
int usysCreateThread(usysThreadParams *pt, void * (*func_ptr)(void *));

/******************************************************/
/********** SEMAPHORE FUNCTIONS ABSTRACTIONS **********/
/******************************************************/
#ifdef PLATFORM_UNIX   /*=================== OS SWITCH ===================*/
    typedef pthread_mutex_t  usysMutex;
    typedef sem_t            usysSemb, usysSemc;
    /* MUTEX */
    #define usysMutexCreate(mtp)    pthread_mutex_init(mtp, NULL)
    #define usysMutexDestroy(mtp)   pthread_mutex_destroy(mtp)
    #define usysMutexTake(mt)       pthread_mutex_lock(&mt)
    #define usysMutexGive(mt)       pthread_mutex_unlock(&mt)
    /* COUNTING SEMAPHORE */
    #define usysSemCCreate(smcp, init_cnt) sem_init(smcp,0,init_cnt)
    #define usysSemCDestroy(smcp)   sem_destroy(smcp)
    #define usysSemCTake(smc)       sem_wait(&smc)
    #define usysSemCGive(smc)       sem_post(&smc)
    /* BINARY SEMAPHORE */
    #define usysSemBCreate(semsfxp) usysSemCCreate(semsfxp, 0)
    #define usysSemBDestroy         usysSemCDestroy
    #define usysSemBGive            usysSemCGive
    #define usysSemBTake            usysSemCTake

#elif PLATFORM_WIN /* || PLATFORM_RTX */   /*=================== OS SWITCH ===================*/
    typedef HANDLE    usysMutex, usysSemb, usysSemc;
    /* MUTEX */ /* default security, unowned by caller and no name */
    #define usysMutexCreate(mtp) {*mtp = CreateMutex( NULL, FALSE, NULL );}
    #define usysMutexDestroy(mtp)   CloseHandle(*mtp)
    #define usysMutexGive(mt)       ReleaseMutex(mt)
    #define usysMutexTake(mt)       WaitForSingleObject( mt, INFINITE );
    /* COUNTING SEMAPHORE */ /* def. security, init cnt 0, max cnt init_cnt, no name */
    #define usysSemCCreate(smcp,init_cnt) {*smcp=CreateSemaphore(NULL,0,init_cnt,NULL);}
    #define usysSemCDestroy(smcp)   CloseHandle(*smcp)
    #define usysSemCGive(smc)       ReleaseSemaphore(smc,1,NULL)
    #define usysSemCTake(smc)       WaitForSingleObject(smc, INFINITE );
    /* BINARY SEMAPHORE */
    #define usysSemBCreate(semsfxp) usysSemCCreate(semsfxp,1)
    #define usysSemBDestroy         usysSemCDestroy
    #define usysSemBGive            usysSemCGive
    #define usysSemBTake            usysSemCTake
#elif defined PLATFORM_VISA  /*=================== OS SWITCH ===================*/
    typedef HANDLE    usysMutex, usysSemb, usysSemc;
    /* MUTEX */ /* default security, unowned by caller and no name */
    #define usysMutexCreate(mtp) {*mtp = CreateMutex( NULL, FALSE, NULL );}
    #define usysMutexDestroy(mtp)   CloseHandle(*mtp)
    #define usysMutexGive(mt)       ReleaseMutex(mt)
    #define usysMutexTake(mt)       WaitForSingleObject( mt, INFINITE );
    /* COUNTING SEMAPHORE */ /* def. security, init cnt 0, max cnt init_cnt, no name */
    #define usysSemCCreate(smcp,init_cnt) {*smcp=CreateSemaphore(NULL,0,init_cnt,NULL);}
    #define usysSemCDestroy(smcp)   CloseHandle(*smcp)
    #define usysSemCGive(smc)       ReleaseSemaphore(smc,1,NULL)
    #define usysSemCTake(smc)       WaitForSingleObject(smc, INFINITE );
    /* BINARY SEMAPHORE */
    #define usysSemBCreate(semsfxp) usysSemCCreate(semsfxp,1)
    #define usysSemBDestroy         usysSemCDestroy
    #define usysSemBGive            usysSemCGive
    #define usysSemBTake            usysSemCTake

#elif PLATFORM_RTX    /*=================== OS SWITCH ===================*/
    typedef HANDLE    usysMutex, usysSemb, usysSemc;
    /* MUTEX */ /* default security, unowned by caller and no name */
    #define usysMutexCreate(mtp) {*mtp = CreateMutex( NULL, FALSE, NULL );}
    #define usysMutexDestroy(mtp)   CloseHandle(*mtp)
    #define usysMutexGive(mt)       ReleaseMutex(mt)
    #define usysMutexTake(mt)       WaitForSingleObject( mt, INFINITE );
    /* COUNTING SEMAPHORE */ /* def. security, init cnt 0, max cnt init_cnt, no name */
    #define usysSemCCreate(smcp,init_cnt) {*smcp=RtCreateSemaphore(NULL,0,init_cnt,NULL);}
    #define usysSemCDestroy(smcp)   RtCloseHandle(*smcp)
    #define usysSemCGive(smc)       RtReleaseSemaphore(smc,1,NULL)
    #define usysSemCTake(smc)       RtWaitForSingleObject(smc, INFINITE );
    /* BINARY SEMAPHORE */
    #define usysSemBCreate(semsfxp) usysSemCCreate(semsfxp,1)
    #define usysSemBDestroy         usysSemCDestroy
    #define usysSemBGive            usysSemCGive
    #define usysSemBTake            usysSemCTake
#elif PLATFORM_VXWORKS /*=================== OS SWITCH ===================*/
    typedef SEM_ID    usysMutex, usysSemb, usysSemc;
    /* MUTEX */
    #define usysMutexCreate(mtp)  {(*(mtp))=(usysMutex)semMCreate(SEM_Q_FIFO);}
    #define usysMutexDestroy(mtp)   semDelete((SEM_ID)*(mtp))
    #define usysMutexTake(mt)       semTake((SEM_ID)(mt), WAIT_FOREVER)
    #define usysMutexGive(mt)       semGive((SEM_ID)(mt))
    /* COUNTING SEMAPHORE */
    #define usysSemCCreate(sp,ct) ((*(sp))=(usysSemc)semCCreate(SEM_Q_FIFO, (ct)))
    #define usysSemCDestroy(smcp)   semDelete((SEM_ID)*(smcp))
    #define usysSemCTake(smc)       semTake((SEM_ID)(smc), WAIT_FOREVER)
    #define usysSemCGive(smc)       semGive((SEM_ID)(smc))
    /* BINARY SEMAPHORE */
    #define usysSemBCreate(sp)    ((*(sp))=(usysSemb)semBCreate(SEM_Q_FIFO, SEM_EMPTY))
    #define usysSemBDestroy         usysSemCDestroy
    #define usysSemBGive            usysSemCGive
    #define usysSemBTake            usysSemCTake
#endif

#ifdef __cplusplus
}
#endif

#endif /* _USYS_H_ */

