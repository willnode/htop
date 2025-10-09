#ifndef HEADER_RedoxProcess
#define HEADER_RedoxProcess
/*
htop - RedoxProcess.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Machine.h"
#include "Process.h"

typedef struct RedoxProcess_
{
   Process super;
   unsigned long long int last_time;
} RedoxProcess;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process *RedoxProcess_new(const Machine *host);

void Process_delete(Object *cast);

extern const ProcessClass RedoxProcess_class;

#endif
