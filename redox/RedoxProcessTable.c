/*
htop - RedoxProcessTable.c
(C) 2014 Hisham H. Muhammad
(C) 2025 Wildan Mubarok
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "RedoxProcessTable.h"

#include <ctype.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>

#include "ProcessTable.h"
#include "RedoxMachine.h"
#include "RedoxProcess.h"
#include "generic/gettime.h"

ProcessTable *ProcessTable_new(Machine *host, Hashtable *pidMatchList)
{
   RedoxProcessTable *this = xCalloc(1, sizeof(RedoxProcessTable));
   Object_setClass(this, Class(ProcessTable));

   ProcessTable *super = &this->super;
   ProcessTable_init(super, Class(Process), host, pidMatchList);

   return super;
}

void ProcessTable_delete(Object *cast)
{
   RedoxProcessTable *this = (RedoxProcessTable *)cast;
   ProcessTable_done(&this->super);
   free(this);
}

static unsigned long parse_redox_time(const char *time_str)
{
   int hours = 0, minutes = 0, seconds = 0, hundredths = 0;
   sscanf(time_str, "%d:%d:%d.%d", &hours, &minutes, &seconds, &hundredths);
   return (unsigned long long)hundredths + (seconds * 100) + (minutes * 60 * 100) + (hours * 3600 * 100);
}

static unsigned long parse_redox_mem(const char *value_str, const char *unit_str)
{
   double value = atof(value_str);
   if (strcmp(unit_str, "MB") == 0) {
      return (unsigned long)(value * 1024.0);
   }
   if (strcmp(unit_str, "GB") == 0) {
      // 1024 * 1024
      return (unsigned long)(value * 1048576.0);
   }
   if (strcmp(unit_str, "KB") == 0) {
      return (unsigned long)(value);
   }

   return 1;
}

static char map_redox_state(const char *state_str)
{
   if (!state_str) {
      return UNKNOWN;
   }

   if (strchr(state_str, '+') != NULL) {
      return RUNNING;
   }

   if (strchr(state_str, 'S') != NULL) {
      return BLOCKED;
   }

   if (strchr(state_str, 'B') != NULL) {
      return SLEEPING;
   }

   if (strchr(state_str, 'Z') != NULL) {
      return ZOMBIE;
   }

   return RUNNABLE;
}

void ProcessTable_goThroughEntries(ProcessTable *super)
{
   FILE *context_file = fopen("/scheme/sys/context", "r");
   if (!context_file) {
      return;
   }

   FILE *stat_file = fopen("/scheme/sys/stat", "r");
   if (!stat_file) {
      return;
   }

   char *line = NULL;
   size_t len = 0;
   float memFactor = 100.f / (float)(sysconf(_SC_PHYS_PAGES) * 4);

   getline(&line, &len, context_file);
   int last_pid = -1;
   uint64_t msec = 0;
   Generic_gettime_monotonic(&msec);
   RedoxMachine *m = (RedoxMachine *)super->super.host;

   while (getline(&line, &len, stat_file) != -1) {
      uint64_t user, nice, kernel, idle, irq;
      int cpu_id;

      if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9') {
         if (sscanf(line, "cpu%d %lu %lu %lu %lu %lu", &cpu_id, &user, &nice, &kernel, &idle, &irq) == 6) {
            if (cpu_id < (int)m->super.activeCPUs) {
               CPUData* cpu = &m->cpus[cpu_id];

               uint64_t du = user   - cpu->luser;
               uint64_t dn = nice   - cpu->lnice;
               uint64_t dk = kernel - cpu->lkrnl;
               // irq is in number of times instead of ms
               // uint64_t di = irq    - cpu->lintr;
               uint64_t dl = idle   - cpu->lidle;
               uint64_t total = du + dn + dk + dl; // + di
               if (total > 0) {
                  double invTotal = 100.0 / (double)total;
                  cpu->userPercent   = du * invTotal;
                  cpu->nicePercent   = dn * invTotal;
                  cpu->systemPercent = dk * invTotal;
                  cpu->irqPercent    = 0;// di * invTotal;
                  cpu->idlePercent   = dl * invTotal;
                  
                  cpu->systemAllPercent = cpu->systemPercent;
               } else {
                  cpu->userPercent = cpu->nicePercent = cpu->systemPercent = 
                  cpu->irqPercent = cpu->systemAllPercent = 0.0;
                  cpu->idlePercent = 100.0;
               }

               cpu->luser = user;
               cpu->lnice = nice;
               cpu->lkrnl = kernel;
               cpu->lintr = irq;
               cpu->lidle = idle;
               cpu->online = true;
            }
         }
      }
      
      if (strncmp(line, "IRQs", 4) == 0) break;
   }

   while (getline(&line, &len, context_file) != -1) {
      int pid, euid, egid, cpu_num;
      unsigned int affinity;
      char stat[16], time_str[32], priv_val[16], priv_unit[8], shrd_val[16], shrd_unit[8], name[256];

      int items = sscanf(line, "%d %d %d %15s #%d %x %31s %15s %7s %15s %7s %255s[^\n]",
                     &pid, &euid, &egid, stat, &cpu_num, &affinity, time_str, priv_val, priv_unit, shrd_val, shrd_unit, name);

      if (items < 10) {
         continue;
      }

      bool preExisting = true;
      Process *proc = ProcessTable_getProcess(super, pid, &preExisting, RedoxProcess_new);
      RedoxProcess *rproc = (RedoxProcess *)proc;

      unsigned long long parsed_time = parse_redox_time(time_str);

      // guaranteed to be sorted by pid
      if (pid != last_pid) {
         // count reset
         rproc->last_time = proc->time;
         rproc->last_nthread = proc->nlwp;
         if (rproc->last_update != 0) {
            rproc->last_update_duration = msec - rproc->last_update;
         }
         rproc->last_update = msec;
         proc->time = 0;
         proc->nlwp = 0;
         proc->state = UNKNOWN;

         proc->m_resident = parse_redox_mem(priv_val, priv_unit);
         proc->m_virt = proc->m_resident + parse_redox_mem(shrd_val, shrd_unit);
         proc->percent_mem = ((float)proc->m_resident) * memFactor;
      } else if (pid == 0) {
         // the memory is accumulative for kernel pid
         proc->m_resident += parse_redox_mem(priv_val, priv_unit);
         proc->m_virt += proc->m_resident + parse_redox_mem(shrd_val, shrd_unit);
         proc->percent_mem += ((float)proc->m_resident) * memFactor;
      }

      proc->time += parsed_time;
      proc->nlwp++;
      long long delta_time = ((long long)proc->time) - ((long long)rproc->last_time);
      if (delta_time >= 0 && rproc->last_time != 0 && proc->nlwp == rproc->last_nthread) {
         proc->percent_cpu = (float)delta_time / ((float)rproc->last_update_duration * 0.001f); // already in hundredth
         Process_updateCPUFieldWidths(proc->percent_cpu);
      } else {
         // This is accurate to count userland thread, but we don't know which is one is.
         super->userlandThreads++;
      }

      rproc->time_cpus[cpu_num] = parsed_time;
      // init kernel service should be a separate pid really
      if (pid != 0 || !strchr(stat, 'U')) {
         char state = map_redox_state(stat);
         proc->state = (state > proc->state) ? state : proc->state;
         // assume idling if kernel is active
         if (state == RUNNING && pid != 0) {
            super->runningTasks++;
         }
         if (pid == 0) {
            super->kernelThreads++;
         }
      } else {
         // the init kernel is running in userspace
         super->userlandThreads++;
      }
      super->totalTasks++;
      proc->processor = (cpu_num > proc->processor) ? cpu_num : proc->processor;

      last_pid = pid;

      if (!preExisting) {
         Process_setPid(proc, pid);
         //   Process_setParent(proc, 1);
         //   Process_setThreadGroup(proc, 0);

         proc->st_uid = euid;
         struct passwd *pws;
         pws = getpwuid(euid);
         proc->user = strdup(pws->pw_name);

         char *trimmed_name = name;
         while (isspace((unsigned char)*trimmed_name))
            trimmed_name++;

         if (pid == 0)
            // this is partially incorrect. 
            // some kernel services can be in userspace, but this wrongly on the same PID
            proc->isKernelThread = true; // !strchr(stat, 'U')

         Process_updateComm(proc, trimmed_name);
         Process_updateExe(proc, trimmed_name);
         Process_updateCmdline(proc, trimmed_name, 0, 0);

         proc->priority = 0;
         proc->nice = 0;
         proc->starttime_ctime = time(NULL);
         Process_fillStarttimeBuffer(proc);

         ProcessTable_add(super, proc);
      }

      proc->super.updated = true;
   }

   free(line);
   fclose(context_file);
   fclose(stat_file);
}