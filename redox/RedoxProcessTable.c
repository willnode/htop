/*
htop - RedoxProcessTable.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "RedoxProcessTable.h"

#include <stdlib.h>
#include <string.h>

#include "ProcessTable.h"
#include "RedoxProcess.h"


ProcessTable* ProcessTable_new(Machine* host, Hashtable* pidMatchList) {
   RedoxProcessTable* this = xCalloc(1, sizeof(RedoxProcessTable));
   Object_setClass(this, Class(ProcessTable));

   ProcessTable* super = &this->super;
   ProcessTable_init(super, Class(Process), host, pidMatchList);

   return super;
}

void ProcessTable_delete(Object* cast) {
   RedoxProcessTable* this = (RedoxProcessTable*) cast;
   ProcessTable_done(&this->super);
   free(this);
}

void ProcessTable_goThroughEntries(ProcessTable* super) {
   bool preExisting = true;
   Process* proc;

   proc = ProcessTable_getProcess(super, 1, &preExisting, RedoxProcess_new);

   /* Empty values */
   proc->time = proc->time + 10;
   Process_setPid(proc, 1);
   Process_setParent(proc, 1);
   Process_setThreadGroup(proc, 0);

   Process_updateComm(proc, "commof16char");
   Process_updateCmdline(proc, "<redox architecture>", 0, 0);
   Process_updateExe(proc, "/path/to/executable");

   const Settings* settings = super->super.host->settings;
   if (settings->ss->flags & PROCESS_FLAG_CWD) {
      free_and_xStrdup(&proc->procCwd, "/current/working/directory");
   }

   proc->super.updated = true;

   proc->state = RUNNING;
   proc->isKernelThread = false;
   proc->isUserlandThread = false;
   proc->super.show = true; /* Reflected in settings-> "hideXXX" really */
   proc->pgrp = 0;
   proc->session = 0;
   proc->tty_nr = 0;
   proc->tty_name = NULL;
   proc->tpgid = 0;
   proc->processor = 0;

   proc->percent_cpu = 2.5;
   proc->percent_mem = 2.5;
   Process_updateCPUFieldWidths(proc->percent_cpu);

   proc->st_uid = 0;
   proc->user = "nobody"; /* Update whenever proc->st_uid is changed */

   proc->priority = 0;
   proc->nice = 0;
   proc->nlwp = 1;
   proc->starttime_ctime = 1433116800; // Jun 01, 2015
   Process_fillStarttimeBuffer(proc);

   proc->m_virt = 100;
   proc->m_resident = 100;

   proc->minflt = 20;
   proc->majflt = 20;

   if (!preExisting)
      ProcessTable_add(super, proc);
}
