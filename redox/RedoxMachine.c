/*
htop - RedoxMachine.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "RedoxMachine.h"

#include <stdlib.h>
#include <string.h>

#include "Machine.h"


static void RedoxMachine_updateCPUcount(RedoxMachine* this) {
   Machine* super = &this->super;
   long int s;
   bool change = false;

   s = sysconf(_SC_NPROCESSORS_CONF);
   if (s < 1)
      CRT_fatalError("Cannot get existing CPU count by sysconf(_SC_NPROCESSORS_CONF)");

   if (s != super->existingCPUs) {
      if (s == 1) {
         this->cpus = xRealloc(this->cpus, sizeof(CPUData));
         this->cpus[0].online = true;
      } else {
         this->cpus = xReallocArray(this->cpus, s + 1, sizeof(CPUData));
         this->cpus[0].online = true; /* average is always "online" */
         for (int i = 1; i < s + 1; i++) {
            this->cpus[i].online = false;
         }
      }

      change = true;
      super->existingCPUs = s;
   }

   s = sysconf(_SC_NPROCESSORS_ONLN);
   if (s < 1)
      CRT_fatalError("Cannot get active CPU count by sysconf(_SC_NPROCESSORS_ONLN)");

   if (s != super->activeCPUs) {
      change = true;
      super->activeCPUs = s;
   }

   if (change) {
      // TODO
   }
}

Machine* Machine_new(UsersTable* usersTable, uid_t userId) {
   RedoxMachine* this = xCalloc(1, sizeof(RedoxMachine));
   Machine* super = &this->super;

   Machine_init(super, usersTable, userId);

   this->pageSize = sysconf(_SC_PAGESIZE);
   if (this->pageSize == -1)
      CRT_fatalError("Cannot get pagesize by sysconf(_SC_PAGESIZE)");
   this->pageSizeKb = this->pageSize / 1024;

   RedoxMachine_updateCPUcount(this);

   return super;
}

void Machine_delete(Machine* super) {
   RedoxMachine* this = (RedoxMachine*) super;
   Machine_done(super);
   free(this);
}

bool Machine_isCPUonline(const Machine* host, unsigned int id) {
   assert(id < host->existingCPUs);

   (void) host; (void) id;

   return true;
}

void Machine_scan(Machine* super) {
   RedoxMachine* this = (RedoxMachine*) super;

   super->totalMem = 0;
   super->usedMem = 0;
   super->buffersMem = 0;
   super->cachedMem = 0;
   super->sharedMem = 0;
   super->availableMem = 0;

   super->totalSwap = 0;
   super->usedSwap = 0;
   super->cachedSwap = 0;

   RedoxMachine_updateCPUcount(this);
}
