#ifndef HEADER_RedoxMachine
#define HEADER_RedoxMachine
/*
htop - RedoxMachine.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Machine.h"

typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double idlePercent;
   double systemAllPercent;
   double frequency;
   uint64_t lnice;
   uint64_t luser;
   uint64_t lkrnl;
   uint64_t lintr;
   uint64_t lidle;
   bool online;
} CPUData;

typedef struct RedoxMachine_ {
   Machine super;

   CPUData* cpus;

   int pageSize;
   int pageSizeKb;
} RedoxMachine;

#endif
