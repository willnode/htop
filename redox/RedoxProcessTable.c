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
#include "RedoxProcess.h"

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

static long long parse_redox_time(const char *time_str)
{
    int hours = 0, minutes = 0, seconds = 0, hundredths = 0;
    sscanf(time_str, "%d:%d:%d.%d", &hours, &minutes, &seconds, &hundredths);
    return (long long)hundredths + (seconds * 100) + (minutes * 60 * 100) + (hours * 3600 * 100);
}

static unsigned long parse_redox_mem(const char *value_str, const char *unit_str)
{
    unsigned long value = atol(value_str);
    if (strcmp(unit_str, "MB") == 0)
    {
        return value * 1024;
    }
    if (strcmp(unit_str, "GB") == 0)
    {
        return value * 1024 * 1024;
    }
    // Assume KB if not specified or "KB"
    return value;
}

static char map_redox_state(const char *state_str)
{
    // What these are means?
    return RUNNING;
}

void ProcessTable_goThroughEntries(ProcessTable *super)
{
    FILE *file = fopen("/scheme/sys/context", "r");
    if (!file)
    {
        return;
    }

    char *line = NULL;
    size_t len = 0;
    long mem = sysconf(_SC_PHYS_PAGES) * 4;
    //  long long total_cpu_time_hsec = 0;

    getline(&line, &len, file);
    int last_pid = -1;

    while (getline(&line, &len, file) != -1)
    {
        int pid, euid, egid, ens, cpu_num;
        char stat[16], time_str[32], mem_val[16], mem_unit[8], name[256];

        int items = sscanf(line, "%d %d %d %d %15s #%d %31s %15s %7s %255s[^\n]",
                           &pid, &euid, &egid, &ens, stat, &cpu_num, time_str, mem_val, mem_unit, name);

        if (items < 10)
        {
            continue;
        }

        bool preExisting = true;
        Process *proc = ProcessTable_getProcess(super, pid, &preExisting, RedoxProcess_new);
        RedoxProcess *rproc = (RedoxProcess *)proc;

        long long parsed_time = parse_redox_time(time_str);

        // guaranteed to be sorted by pid
        if (pid != last_pid)
        {
            // count reset
            rproc->last_time = proc->time;
            proc->time = 0;
            proc->nlwp = 0;

            proc->m_resident = parse_redox_mem(mem_val, mem_unit);
            proc->m_virt = proc->m_resident;
            proc->percent_mem = ((float)proc->m_resident) / ((float)mem) * 100.f;
        }
        proc->time += parsed_time;
        long long delta_time = ((long long)proc->time) - ((long long)rproc->last_time);
        if (delta_time > 0 && rproc->last_time != 0)
            proc->percent_cpu = (float)delta_time; // already in hundredth

        //   total_cpu_time_hsec += parsed_time;

        proc->nlwp++;

        proc->processor = cpu_num;
        last_pid = pid;

        if (!preExisting)
        {
            Process_setPid(proc, pid);
            //    Process_setParent(proc, 1);
            //    Process_setThreadGroup(proc, 0);

            proc->st_uid = euid;
            struct passwd *pws;
            pws = getpwuid(euid);
            proc->user = strdup(pws->pw_name);

            proc->state = map_redox_state(stat);

            char *trimmed_name = name;
            while (isspace((unsigned char)*trimmed_name))
                trimmed_name++;

            if (pid == 0)
            {
                proc->isKernelThread = true;
            }

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

    //  if (super->processCount > 0) {
    //      long long average_cpu_time_hsec = total_cpu_time_hsec / super->processCount;
    //      long secs = average_cpu_time_hsec / 100;
    //      long hsec = average_cpu_time_hsec % 100;

    //      printf("Average CPU time per process: %ld.%02ld seconds\n", secs, hsec);
    //  }

    free(line);
    fclose(file);
}