/* ====================================================================
 * scheduler.c
 *
 * scheduler is responsible for creating and managing processes
 * according to a specific algorithm
 * ====================================================================
 */

#include "scheduler.h"
#include "headers.h"

int msgQID;
d_list *p_table = NULL;

/**
 * main - The main function of the scheduler.
 *
 * @argc: the number of arguments
 * @argv: the arguments
 * return: 0 on success, -1 on failure
 */
int main(int argc, char *argv[]) {
  int key, gen_msgQID, response;
  scheduler_type schedulerType;
  d_list *processTable = NULL;

  signal(SIGINT, clearSchResources);
  signal(SIGTERM, clearSchResources);
  if (atexit(cleanUpScheduler) != 0) {
    perror("atexit");
    exit(1);
  }

  initClk();

  schedulerType = getScType(atoi(argv[1]));
  printf(ANSI_YELLOW "==>SCH: My Scheduler Type is %i\n" ANSI_RESET,
         (int)schedulerType);

  p_table = processTable = createList(freeProcessEntry);
  if (!processTable) {
    perror("Error while creating process table");
    exit(-1);
  }

  msgQID = gen_msgQID = initSchGenCom();

  getProcesses(gen_msgQID, processTable);

  // All files handling (log - perf)- George
  // Communication with process - Samy
  // Preemting - Amir
  // Cleaning
  // maing loop of all algos - Ahmed
  //
  // TODO Initialize Scheduler
  //  Create Ready queue & (Wait queue ??)
  //  Create log file *
  //
  //
  // TODO Create process when generator tells you it is time
  //  Setup COM between process and Scheduler (init msgs queue)
  //
  // TODO Context Switch (print to log file in start and finish)
  //  When SRT process comes to ready queue (by myself)
  //  When quantem ends (communicate with clk)
  //  When a process finishes (process get SIGTRM)
  //  When a process gets a signal (SIGKILL, SIGINT, SIGSTP, ...etc)
  //  The Switch
  //    Move old process to ready or wait or (clear after, if it has terminated)
  //    Save PCB if it still exist (set attributes)
  //    Schedule new Process (We need the algo here)
  //    Load PCB (set attributes)
  //    Tell the process to start
  //
  // TODO Clear After process termination
  //  Calculate all needed values (till now)
  //  Remove process from processes Table (Delete its PCB)
  //
  // TODO Clean everything when Scheduler finishes or if it is killed
  //  Calculate all needed stats
  //  create perf file
  //  kill all living processes and destroy its PCB (is this right?)
  //  destroy process table
  //  destroy clk
  //  Any other clean up
}

/**
 * getScType - gets scheduler type
 *
 * @schedulerType: scheduler type
 * return: scheduler type enum
 */
scheduler_type getScType(int schedulerType) {
  switch (schedulerType) {
  case 0:
    return HPF;
  case 1:
    return SRTN;
  case 2:
    return RR;
  default:
    exit(-1);
  }
}

/**
 * getProcesses - gets processes from generator
 *
 * @gen_msgQID: msg queue with process generator
 * @processTable: process table
 */
void getProcesses(int gen_msgQID, d_list *processTable) {
  int response;
  process_t process;

  while (1) {
    response = msgrcv(gen_msgQID, &process, sizeof(process_t), 0, !IPC_NOWAIT);

    if (response == -1) {
      perror("Error in receiving process from process generator");
      exit(-1);
    }

    if (process.id == -1) {
      printf(ANSI_YELLOW "==>SCH: Received All Processes\n" ANSI_RESET);
      break;
    }
    printf(ANSI_BLUE "==>SCH: Received process with id = %i\n" ANSI_RESET,
           process.id);
    createProcess(processTable, &process);
  }
}

/**
 * freeProcessEntry - process entry free function
 *
 * @processEntry: process entry
 */
void freeProcessEntry(void *processEntry) {
  if (processEntry)
    free(((process_entry_t *)processEntry)->PCB);
  free(processEntry);
}

/**
 * createProcess - create a new process and add it to process table
 *
 * @processTable: pointer to process table
 * @process: pointer to new process info
 */
void createProcess(d_list *processTable, process_t *process) {
  pid_t pid;
  process_entry_t *processEntry;
  PCB_t *pcb;

  pid = fork();

  if (pid == -1) {
    perror("fork");
    exit(-1);
  }

  if (pid == 0) {
    char *args[] = {"./process.out", NULL};
    execvp(args[0], args);
    exit(0);
  }

  processEntry = malloc(sizeof(*processEntry));
  if (!processEntry) {
    perror("malloc");
    exit(-1);
  }

  pcb = malloc(sizeof(*pcb));
  if (!pcb) {
    perror("malloc");
    exit(-1);
  }

  pcb->state = READY;
  pcb->process = *process;
  processEntry->p_id = pid;
  processEntry->PCB = pcb;
  if (!insertNodeEnd(processTable, processEntry)) {
    perror("insertNodeEnd");
    exit(-1);
  }

  printf(ANSI_GREEN "==>SCH: Added process to processes table\n" ANSI_RESET);
}

/**
 * cleanUpScheduler - Make necessary cleaning
 */
void cleanUpScheduler() {
  msgctl(msgQID, IPC_RMID, (struct msqid_ds *)0);
  destroyClk(true);
  if (p_table)
    destroyList(&p_table);
  killpg(getpgrp(), SIGINT);
}

/**
 * clearSchResources - Clears all resources in case of interruption.
 *
 * @signum: the signal number
 */
void clearSchResources(int signum) {
  cleanUpScheduler();
  exit(0);
}
