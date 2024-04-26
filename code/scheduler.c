/* ====================================================================
 * scheduler.c
 *
 * scheduler is responsible for creating and managing processes
 * according to a specific algorithm
 * ====================================================================
 */

#include "scheduler.h"
#include "headers.h"
#include <unistd.h>

int msgQID;

/**
 * main - The main function of the scheduler.
 *
 * @argc: the number of arguments
 * @argv: the arguments
 * return: 0 on success, -1 on failure
 */
int main(int argc, char *argv[]) {
  int key, gen_msgQID, response, quantem;
  scheduler_type schedulerType;

  signal(SIGINT, clearSchResources);
  signal(SIGTERM, clearSchResources);
  signal(SIGUSR1, sigUsr1Handler);
  if (atexit(cleanUpScheduler) != 0) {
    perror("atexit");
    exit(1);
  }

  initClk();

  schedulerType = getScParams(argv, &quantem);
  if (DEBUG)
    printf(ANSI_BLUE "==>SCH: My Scheduler Type is %i\n" ANSI_RESET,
           (int)schedulerType);

  msgQID = gen_msgQID = initSchGenCom();
  schedule(schedulerType, quantem, gen_msgQID);

  // TODO Initialize Scheduler
  //  Create Wait queue ??
  //  Create log file
  //
  // TODO Create process when generator tells you it is time
  //  Setup COM between process and Scheduler (init msgs queue)
  //
  // TODO Clear After process termination
  //  Calculate all needed values (till now)
  //  Remove process from processes Table (Delete its PCB)
  //
  // TODO Clean everything when Scheduler finishes or if it is killed
  //  Calculate all needed stats
  //  create pref file
  //  kill all living processes and destroy its PCB (is this right?)
  //  destroy process table
  //  destroy clk
  //  Any other clean up
  return (0);
}

/**
 * Schedule - Main loop of scheduler
 *
 * @schedulerType: scheduler type
 * @quantem: RR quantem
 * @gen_msgQID: msg queue ID between generator & scheduler
 * @processTable: pointer to process table
 */
void schedule(scheduler_type schType, int quantem, int gen_msgQID) {
  void *readyQ;
  process_t process, *newProcess, **currentProcess = NULL;
  int processesFlag = 1; // to know when to stop getting processes from gen
  int rQuantem = quantem;
  int (*algorithm)(void *readyQ, process_t **currentProcess,
                   process_t *newProcess, int *rQuantem);

  switch (schType) {
  case HPF:
    readyQ = createMinHeap(compareHPF);
    algorithm = HPFScheduling;
    break;
  case SRTN:
    readyQ = createMinHeap(compareSRTN);
    algorithm = SRTNScheduling;
    break;
  case RR:
    readyQ = createQueue(free);
    algorithm = RRScheduling;
    break;
  default:
    exit(-1);
  }

  currentProcess = malloc(sizeof(process_t *));
  if (!currentProcess) {
    perror("malloc");
    exit(-1);
  }

  *currentProcess = NULL;
  while (1) {
    newProcess = NULL;

    if (processesFlag) {
      if (getProcess(&processesFlag, gen_msgQID, &process))
        newProcess = createProcess(&process);
    }

    if (!algorithm(readyQ, currentProcess, newProcess, &rQuantem) &&
        !processesFlag)
      break;
    if (rQuantem <= 0)
      rQuantem = quantem;
  }
  printf(ANSI_BLUE "==>SCH: All processes are done\n" ANSI_RESET);
}

/**
 * compareHPF - compare function for HPF ready queue
 *
 * @e1: pointer to first element
 * @e2: pointer to first element
 * Return: 1 if e1 priority is higher, -1 if e2 is higher, 0 if they are equal
 */
int compareHPF(void *e1, void *e2) {
  if (((process_t *)e1)->priority < ((process_t *)e2)->priority)
    return -1;
  else if (((process_t *)e1)->priority > ((process_t *)e2)->priority)
    return 1;
  return 0;
}

/**
 * compareSRTN - compare function for SRTN ready queue
 *
 * @e1: pointer to first element
 * @e2: pointer to first element
 * Return: 1 if e2 Remaining Time is less, -1 if e1 is less, 0 if they are equal
 */
int compareSRTN(void *e1, void *e2) {
  if (*((process_t *)e1)->RT < *((process_t *)e2)->RT)
    return -1;
  else if (*((process_t *)e1)->RT > *((process_t *)e2)->RT)
    return 1;
  return 0;
}

/**
 * HPFScheduling - HPF scheduling algorithm
 *
 * @readyQueue: scheduler ready queue
 * @process: pointer to process
 * @rQuantem: remaining quantem time
 *
 * Return: 0 if no process is no the system, 1 otherwise
 */
int HPFScheduling(void *readyQueue, process_t **currentProcess,
                  process_t *process, int *rQuantem) {
  min_heap *readyQ = (min_heap *)readyQueue;
  (void)rQuantem;

  if (process)
    insertMinHeap(&readyQ, process);

  if (!(*currentProcess) && !getMin(readyQ))
    return 0;

  if (!(*currentProcess))
    contextSwitch(currentProcess, extractMin(readyQ));
  return 1;
}

/**
 * SRTNScheduling - HPF scheduling algorithm
 *
 * @readyQueue: scheduler ready queue
 * @process: pointer to process
 * @rQuantem: remaining quantem time
 *
 * Return: 0 if no process is no the system, 1 otherwise
 */
int SRTNScheduling(void *readyQueue, process_t **currentProcess,
                   process_t *process, int *rQuantem) {
  min_heap *readyQ = (min_heap *)readyQueue;
  process_t *newScheduledProcess = NULL;
  (void)rQuantem;

  if (process)
    insertMinHeap(&readyQ, process);

  if (!(*currentProcess) && !getMin(readyQ))
    return 0;

  if (!(*currentProcess) || *currentProcess != (process_t *)getMin(readyQ))
    contextSwitch(currentProcess, extractMin(readyQ));

  return 1;
}

/**
 * RRScheduling - RR scheduling algorithm
 *
 * @readyQueue: scheduler ready queue
 * @process: pointer to process
 * @rQuantem: remaining quantem time
 *
 * Return: 0 if no process is no the system, 1 otherwise
 */
int RRScheduling(void *readyQueue, process_t **currentProcess,
                 process_t *process, int *rQuantem) {
  queue *readyQ = (queue *)readyQueue;

  if (process)
    push(readyQ, process);

  if (!(*currentProcess) && empty(readyQ))
    return 0;

  (*rQuantem)--;
  if (*rQuantem <= 0)
    contextSwitch(currentProcess, pop(readyQ));

  return 1;
}

/**
 * getProcess - gets ap process from generator
 *
 * @gen_msgQID: msg queue with process generator
 * @processTable: process table
 *
 * Return: 0 if no process, 1 if got the process
 */
int getProcess(int *processesFlag, int gen_msgQID, process_t *process) {
  int response;

  response = msgrcv(gen_msgQID, process, sizeof(process_t), 0, IPC_NOWAIT);

  if (response == -1) {
    if (errno == ENOMSG) {
      return 0;
    }
    perror("Error in receiving process from process generator");
    exit(-1);
  }

  if (process->id == -1) {
    printf(ANSI_BLUE "==>SCH: Received All Processes\n" ANSI_RESET);
    *processesFlag = 0;
    return 0;
  }
  printf(ANSI_BLUE "==>SCH: Received process with id = %i\n" ANSI_RESET,
         process->id);
  return 1;
}

/**
 * getScType - gets scheduler type
 *
 * @schedulerType: scheduler type
 * return: scheduler type enum
 */
scheduler_type getScParams(char *argv[], int *quantem) {
  int schedulerType = atoi(argv[1]);

  switch (schedulerType) {
  case 0:
    return HPF;
  case 1:
    return SRTN;
  case 2:
    *quantem = atoi(argv[2]);
    return RR;
  default:
    exit(-1);
  }
}

/**
 * createProcess - create a new process and add it to process table
 *
 * @processTable: pointer to process table
 * @process: pointer to new process info
 */
process_t *createProcess(process_t *process) {
  pid_t pid;
  process_t *newProcess;

  newProcess = malloc(sizeof(*newProcess));
  if (!newProcess) {
    perror("malloc");
    exit(-1);
  }

  *newProcess = *process;
  newProcess->state = READY;

  pid = fork();
  if (pid == -1) {
    perror("fork");
    exit(-1);
  }

  if (pid == 0) {
    char *args[] = {"./process.out", NULL};
    // NOTE: If you want to autostart the process, uncomment the next line
    kill(getpid(), SIGSTOP);
    execvp(args[0], args);
    exit(0);
  }

  newProcess->pid = pid;

  int shmid = initSchProShm(pid);
  int *shmAdd = (int *)shmat(shmid, (void *)0, 0);

  newProcess->RT = shmAdd;
  *newProcess->RT = process->BT;

  return newProcess;
}

void contextSwitch(process_t **currentProcess, process_t **newProcess) {
  printf(ANSI_BLUE "==>SCH: Context Switching\n" ANSI_RESET);
  // TODO Context Switch (print to log file in start and finish)
  //  When a process finishes (process get SIGTRM)
  //  When a process gets a signal (SIGKILL, SIGINT, SIGSTP, ...etc)
  //  The Switch
  //    Move old process to ready or wait or (clear after, if it has terminated)
  //    Save PCB if it still exist (set attributes)
  //    Schedule new Process (We need the algo here)
  //    Load PCB (set attributes)
  //    Tell the process to start
}

/**
 * preemptProcessByIndex - Preempt a process by its index in the process table
 * @process: pointer to process
 */
void preemptProcess(process_t *process) {
  printf(ANSI_GREY "==>SCH: Preempting process with id = %i\n" ANSI_RESET,
         process->pid);

  if (process->state == RUNNING) {
    kill(process->pid, SIGSTOP);
    process->state = STOPPED;
    process->LST = getClk();
  }
}

/**
 * resumeProcessByIndex - Resume a process by its index in the process table
 * @process: pointer to process
 */
void resumeProcessByIndex(process_t *process) {
  printf(ANSI_BLUE "==>SCH: Resuming process with id = %i\n" ANSI_RESET,
         process->pid);

  if (process->state == READY) {
    kill(process->pid, SIGCONT);
    process->state = RUNNING;
    process->WT += getClk() - process->LST;
  }
}

/**
 * cleanUpScheduler - Make necessary cleaning
 */
void cleanUpScheduler() {
  msgctl(msgQID, IPC_RMID, (struct msqid_ds *)0);
  destroyClk(true);
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

int initSchProQ() {
  int id = ftok("keyfiles/PRO_SCH_Q", SCH_PRO_COM);
  int q_id = msgget(id, IPC_CREAT | 0666);

  if (q_id == -1) {
    perror("error in creating msg queue between process & scheduler");
    exit(-1);
  } else if (DEBUG) {
    printf("Message queue created successfully with pid = %d\n", q_id);
  }

  return q_id;
}

void sigUsr1Handler(int signum) {
  // TODO: write an appropriate implementation for this function
  // detach the scheduler from the sharedmemory of the rem. time
  // of this process (the running one)
  // FIXME: Just tell me why you kill the sch here I spent 1 hour debugging for
  // this
  // raise(SIGKILL);

  pid_t killedProcess;
  int status;
  killedProcess = wait(&status);
  printf(ANSI_GREY "==>SCH: Process %d terminated\n" ANSI_RESET, killedProcess);
}
