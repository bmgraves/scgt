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

#define FILE_REV_USYS_C   "2011/09/08"

/******************************************************/
/********** INCLUDES **********************************/
/******************************************************/
#include "usys.h"

/******************************************************/
/********** TIMING FUNCTIONS ABSTRACTIONS *************/
/******************************************************/
uint32 usysMsTimeGetElapsed( usysMsTimeType *pTimer )       /************/
{
   int32 usecs;
   uint32 secs;

   if(pTimer->finishValid == 0)
   {
       usysMsTimeStop(pTimer);
       pTimer->finishValid = 0;
   }
   secs  = pTimer->finishTimeSecs - pTimer->startTimeSecs;
   usecs = (int32)pTimer->finishTimeUsecs - (int32)pTimer->startTimeUsecs;
   if (usecs <= 0)
   {
      usecs += 1000000L;
      secs--;
   }
   return ((secs * 1000) + ((usecs + 500) / 1000));
}

#ifdef PLATFORM_UNIX   /*=================== OS SWITCH ===================*/
void usysMsTimeStart (usysMsTimeType *pTimer)                 /************/
{
   struct timeval tc;

   pTimer->finishValid = 0;
   gettimeofday(&tc, NULL);
   pTimer->startTimeSecs  = tc.tv_sec;
   pTimer->startTimeUsecs = tc.tv_usec;
}

void usysMsTimeStop (usysMsTimeType *pTimer)                  /************/
{
   struct timeval tc;

   gettimeofday(&tc, NULL);
   pTimer->finishTimeSecs  = tc.tv_sec;
   pTimer->finishTimeUsecs = tc.tv_usec;
   pTimer->finishValid = 1;
}

void usysMsTimeDelay(uint32 milliSeconds)                     /************/
{
   struct timespec rqtp;

   if (milliSeconds == 0)
       sched_yield();
   else
   {
      rqtp.tv_sec  = milliSeconds / 1000;
      rqtp.tv_nsec = (milliSeconds % 1000) * 1000000;
      nanosleep(&rqtp, NULL);
   }
}

#elif defined PLATFORM_WIN || defined PLATFORM_VISA    /*=================== OS SWITCH ===================*/
void usysMsTimeStart( usysMsTimeType *pTimer )                /************/
{
   clock_t tc;

   pTimer->finishValid = 0;
   tc = clock();
   pTimer->startTimeSecs  = tc / CLOCKS_PER_SEC;
   pTimer->startTimeUsecs = ((tc % CLOCKS_PER_SEC)*1000000)/CLOCKS_PER_SEC;
}

void usysMsTimeStop( usysMsTimeType *pTimer )                 /************/
{
   clock_t tc;

   tc = clock();
   pTimer->finishTimeSecs  = tc / CLOCKS_PER_SEC;
   pTimer->finishTimeUsecs = ((tc % CLOCKS_PER_SEC)*1000000)/CLOCKS_PER_SEC;
   pTimer->finishValid = 1;
}

void usysMsTimeDelay( uint32 milliSeconds )                   /************/
{
#if 1
   Sleep(milliSeconds);
#else /* to get 1 ms resolution */
   timeBeginPeriod(1);
   Sleep(milliSeconds);
   timeEndPeriod(1);
#endif
}

#elif PLATFORM_RTX    /*=================== OS SWITCH ===================*/

#define RTX_CLOCKS_PER_SEC  10000000

void usysMsTimeStart( usysMsTimeType *pTimer )                /************/
{
   uint64 tc;
   LARGE_INTEGER li;

   pTimer->finishValid = 0;
   RtGetClockTime(CLOCK_FASTEST, &li);
   tc = li.QuadPart;
   
   /* RTX gives time in 100ns ticks */
   pTimer->startTimeSecs  = (uint32) (tc / RTX_CLOCKS_PER_SEC);
   
   /* get rid of seconds by modding.. then convert to us */
   pTimer->startTimeUsecs = (uint32) (tc % RTX_CLOCKS_PER_SEC) / (RTX_CLOCKS_PER_SEC / 1000000);
}

void usysMsTimeStop( usysMsTimeType *pTimer )                 /************/
{
   uint64 tc;
   LARGE_INTEGER li;
   
   RtGetClockTime(CLOCK_FASTEST, &li);
   tc = li.QuadPart;
   pTimer->finishTimeSecs  = (uint32) (tc / RTX_CLOCKS_PER_SEC);
   pTimer->finishTimeUsecs = (uint32) (tc % RTX_CLOCKS_PER_SEC) / (RTX_CLOCKS_PER_SEC / 1000000);
   pTimer->finishValid = 1;
}

void usysMsTimeDelay( uint32 milliSeconds )                   /************/
{
#if 1
   Sleep(milliSeconds);
#else /* to get 1 ms resolution */
   timeBeginPeriod(1);
   Sleep(milliSeconds);
   timeEndPeriod(1);
#endif
}



#elif PLATFORM_VXWORKS /*=================== OS SWITCH ===================*/

#include "sysLib.h"
#include "taskLib.h"

void usysMsTimeStart(usysMsTimeType *pTimer)                  /************/
{
   struct timespec  tc;

   pTimer->finishValid = 0;
   clock_gettime(CLOCK_REALTIME, &tc);
   pTimer->startTimeSecs = tc.tv_sec;
   pTimer->startTimeUsecs = tc.tv_nsec / 1000;
}

void usysMsTimeStop(usysMsTimeType *pTimer)                   /************/
{
   struct timespec  tc;

   clock_gettime(CLOCK_REALTIME, &tc);
   pTimer->finishTimeSecs = tc.tv_sec;
   pTimer->finishTimeUsecs = tc.tv_nsec / 1000;
   pTimer->finishValid  = 1;
}

void usysMsTimeDelay(uint32 milliSeconds)                     /************/
{
   if(milliSeconds != 0)
   {
#if 1
       /* at least one tick */
       taskDelay(((milliSeconds * sysClkRateGet()) / 1000) + 1);
#else
       /*Common VxWorks tick intervals are 16.67ms (60Hz)
         and 10ms (100Hz). We choose 60Hz */
       taskDelay((milliSeconds+16) / 17);  /* at least one tick */
#endif
   }
   else
   {
       taskDelay(milliSeconds);
   }
}

#endif

/******************************************************/
/********** THREAD FUNCTIONS ABSTRACTIONS *************/
/******************************************************/

#ifdef PLATFORM_UNIX   /*=================== OS SWITCH ===================*/
int usysCreateThread(usysThreadParams *pt,                    /************/
                      void * (*func_ptr)(void *))
{
   int rval;
   pthread_attr_t attr;

   if (pthread_attr_init(&attr))
       return(-1);
   if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
   {
      pthread_attr_destroy(&attr);
      return(-1);
   }
   rval = pthread_create(&(pt->threadHandle), &attr, func_ptr, pt->parameter);
   pthread_attr_destroy(&attr);
   return rval;
}

int usysKillThread (usysThreadParams *pt)                     /************/
{
   return pthread_cancel(pt->threadHandle);
}

#elif defined PLATFORM_WIN  || defined PLATFORM_VISA  /*=================== OS SWITCH ===================*/


int usysCreateThread(usysThreadParams *pt,                    /************/
                       void * (*func_ptr)(void *))
{
   int nPriority;
   
   pt->threadHandle = CreateThread( NULL, 0, 
       (LPTHREAD_START_ROUTINE)func_ptr, pt->parameter,0, &(pt->threadID));

   if( pt->threadHandle )
   {
      switch( pt->priority )/* Map Priority Level to NT Specific */
      {
         case UPRIO_LOW: nPriority = THREAD_PRIORITY_LOWEST; break;
         case UPRIO_MED: nPriority = THREAD_PRIORITY_NORMAL; break;
         case UPRIO_HIGH: nPriority = THREAD_PRIORITY_HIGHEST; break;
         case UPRIO_REALTIME: nPriority = THREAD_PRIORITY_TIME_CRITICAL; break;
         default: nPriority = THREAD_PRIORITY_NORMAL; break;
      }
      if( !SetThreadPriority(pt->threadHandle, nPriority))
      {
         CloseHandle(pt->threadHandle);
         return(-1);
      }
      else
      {
         CloseHandle(pt->threadHandle);
         return(0);
      }
   }
   return(-1);
}

int usysKillThread( usysThreadParams *pt )                    /************/
{
   return 0;
}

#elif PLATFORM_RTX    /*=================== OS SWITCH ===================*/


int usysCreateThread(usysThreadParams *pt,                    /************/
                       void * (*func_ptr)(void *))
{
   int nPriority;
   
   pt->threadHandle = CreateThread( NULL, 0, 
       (LPTHREAD_START_ROUTINE)func_ptr, pt->parameter,0, &(pt->threadID));

   if( pt->threadHandle )
   {
      switch( pt->priority )/* Map Priority Level to NT Specific */
      {
         case UPRIO_LOW: nPriority = THREAD_PRIORITY_LOWEST; break;
         case UPRIO_MED: nPriority = THREAD_PRIORITY_NORMAL; break;
         case UPRIO_HIGH: nPriority = THREAD_PRIORITY_HIGHEST; break;
         case UPRIO_REALTIME: nPriority = THREAD_PRIORITY_TIME_CRITICAL; break;
         default: nPriority = THREAD_PRIORITY_NORMAL; break;
      }
      if( !SetThreadPriority(pt->threadHandle, nPriority))
      {
         CloseHandle(pt->threadHandle);
         return(-1);
      }
      else
      {
         CloseHandle(pt->threadHandle);
         return(0);
      }
   }
   return(-1);
}

int usysKillThread( usysThreadParams *pt )                    /************/
{
   /* TerminateThread(pt->threadHandle, 0); */
               
   /* the ExitProcess() is an unfortunate hack to get the input thread to quit.
      The TerminateThread() function doesn't kill it properly */
   
   //ExitProcess(0);
   return 0;
}


#elif PLATFORM_VXWORKS /*=================== OS SWITCH ===================*/
int usysCreateThread(usysThreadParams *pt,                    /************/
                      void * (*func_ptr)(void *))
{
   int priority;

   taskPriorityGet(taskIdSelf(), &priority);
   switch (pt->priority)
   {
      case UPRIO_LOW:      priority += 20; break;
      case UPRIO_MED:      break;
      case UPRIO_HIGH:     priority -= 20; break;
      case UPRIO_REALTIME: priority = 20; break;
      default: break;
   }
   if(priority <= 0)
       taskPriorityGet(taskIdSelf(), &priority);

   pt->threadID = taskSpawn(pt->threadName, priority, 0,32768,(FUNCPTR)func_ptr,
                            (int)pt->parameter, 0, 0, 0, 0, 0, 0, 0, 0, 0);
   return pt->threadID;
}

int usysKillThread(usysThreadParams *pt)                      /************/
{
   return taskDelete(pt->threadID);
}

#endif

/******************************************************/
/********** MEMORY ALLOCATION ABSTRACTIONS ************/
/******************************************************/

#ifdef PLATFORM_RTX  /*=================== OS SWITCH ===================*/
void *usysMemMalloc(uint32 nBytes)
{
    LARGE_INTEGER li;
    void *ptr;
    
    li.QuadPart = (uint64) 0xFFFFFFFF;
    ptr = RtAllocateContiguousMemory(nBytes, li);

    return ptr;
}

#endif
