#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "llist.h"
#include "prioll.h"
#include "kernel.h"

/*

	STUDENT 1 NUMBER: A0180009N
	STUDENT 1 NAME: Lucas Gaylord

	STUDENT 2 NUMBER:
	STUDENT 2 NAME:

	STUDENT 3 NUMBER:
	STUDENT 3 NAME:

	*/

/* OS variables */

// Current number of processes
int procCount;

// Current timer tick
int timerTick=0;
int currProcess, currPrio;

#if SCHEDULER_TYPE == 0

/* Process Control Block for LINUX scheduler*/
typedef struct tcb
{
	int procNum;
	int prio;
	int quantum;
	int timeLeft;
} TTCB;

#elif SCHEDULER_TYPE == 1

/* Process Control Block for RMS scheduler*/

typedef struct tcb
{
	int procNum;
	int timeLeft;
	int deadline;
	int c;
	int p;
} TTCB;

#endif


TTCB processes[NUM_PROCESSES]; /* PROCESSES table defined here ********* -------- TTCB -------- */

#if SCHEDULER_TYPE == 0

// Lists of processes according to priority levels.
TNode *queueList1[PRIO_LEVELS];
TNode *queueList2[PRIO_LEVELS];

// Active list and expired list pointers
TNode **activeList = queueList1;
TNode **expiredList = queueList2;

#elif SCHEDULER_TYPE == 1

// Ready queue and blocked queue
TPrioNode *readyQueue, *blockedQueue;

// This stores the data for pre-empted processes.
TPrioNode *suspended; // Suspended process due to pre-emption

// Currently running process
TPrioNode *currProcessNode; // Current process
#endif


#if SCHEDULER_TYPE == 0

// Searches the active list for the next priority level with processes
int findNextPrio(int currPrio)
{
	int i;

	for(i=0; i<PRIO_LEVELS; i++)
		if(activeList[i] != NULL)
			return i;

	return -1; 

}

int linuxScheduler()
{
	// No RT-FIFO and RT Round Robin 👍👍 here we go 😬 😭
	int processLevel = currPrio; 
	/*** Scheduler picks the highest process in active set to run */
	if(currPrio == 0) processLevel = findNextPrio(currPrio); //this is for initialization 
	
	TNode *processToRun = &activeList[processLevel][0]; //get the head process in the priority level to run
	TTCB* TTCBprocess = &processes[processToRun -> procNum]; 	//procNum is index into processes. Get TTCB in order to decrement timeLeft

	if (TTCBprocess -> timeLeft < 1) { //if time is up, put the process in expired list
	/*** When time quantum is expired, it is moved to the expired set. 
	 * Next highest priority process is picked */

	//printf("\n****** PRE-EMTPTED **** Process level: %d \n", processLevel);
	int pid = remove(&activeList[processLevel]);
	insert(&expiredList[TTCBprocess -> prio], pid, processToRun -> quantum);

	processLevel = findNextPrio(currPrio);	//get next process priority level
	processToRun = &activeList[processLevel][0]; //get the head process in the priority level to run
	TTCBprocess = &processes[processToRun -> procNum]; 	//procNum is index into processes. Get TTCB in order to decrement timeLeft

	/*** When active set is empty, 
	 * active and expired pointers are swapped */
		if(processLevel == -1 || pid == -1) { //if list is empty swap active and expired pointers
		TNode **tmp = activeList;
		activeList = expiredList;
		expiredList = tmp;
	
		printf("\n****** SWAPPED LISTS ****\n");
		}
	}

	TTCBprocess -> timeLeft -= 1; //subtract 1 from time avaliable

	int returnMe = processToRun -> procNum;	//get the process number that will be returned
	currPrio = processLevel;	//set the current priority level
	return returnMe;
}
#elif SCHEDULER_TYPE == 1

int RMSScheduler()
{
	TPrioNode *node = checkReady(blockedQueue, timerTick);
	while (node != NULL) {
		prioRemoveNode(&blockedQueue, node);
		prioInsertNode(&readyQueue, node);
		node = checkReady(blockedQueue, timerTick);
	}

	//Check current process
	if (currProcessNode == NULL && peek(readyQueue) != NULL) {
		currProcessNode = prioRemove(&readyQueue);
	}
	
	if (currProcessNode != NULL && processes[currProcessNode->procNum].timeLeft == 0) {
		processes[currProcessNode->procNum].timeLeft = processes[currProcessNode->procNum].c;
		
		//check if currProcess missed deadline
		if (processes[currProcessNode->procNum].deadline < timerTick) {
			//Update currProcess deadline
			processes[currProcessNode->procNum].deadline
				= processes[currProcessNode->procNum].deadline
				+ processes[currProcessNode->procNum].p;	

			prioInsertNode(&readyQueue, currProcessNode);
		}
		else {
			//Update currProcess deadline
			processes[currProcessNode->procNum].deadline
				= processes[currProcessNode->procNum].deadline
				+ processes[currProcessNode->procNum].p;
		
			prioInsertNode(&blockedQueue, currProcessNode);
		}

		if (suspended == NULL && peek(readyQueue) == NULL) {
			currProcessNode = NULL;
		}
		else if (suspended != NULL && peek(readyQueue) != NULL) {
			if (peek(readyQueue)->p > suspended->p) {
				currProcessNode = suspended;
				suspended = NULL;
			}
			else {
				currProcessNode = prioRemove(&readyQueue);
			}
		}
		else if (suspended == NULL) {
			currProcessNode = prioRemove(&readyQueue);
		}
		else {
			currProcessNode = suspended;
			suspended = NULL;
		}

	}

	//Check if there is pre-empt
	TPrioNode *preEmpt = peek(readyQueue);
	if (preEmpt != NULL && currProcessNode != NULL && preEmpt->p < currProcessNode->p) {
		//Idle
		suspended = currProcessNode;
		currProcessNode = prioRemove(&readyQueue);
		printf("\n--- PRE-EMPTION ---\n\n");
	}

	//Check suspended process
	if (suspended != NULL && currProcessNode != NULL && suspended->p < currProcessNode->p) {
		TPrioNode *swap = currProcessNode;
		currProcessNode = suspended;
		suspended = swap;
	}

	//Execute process
	if (currProcessNode != NULL) {
		processes[currProcessNode->procNum].timeLeft--;
		return currProcessNode->procNum;
	}
	//Idle
	else {
		return -1;
	}
}

#endif

void timerISR()
{

#if SCHEDULER_TYPE == 0
	currProcess = linuxScheduler();
#elif SCHEDULER_TYPE == 1
	currProcess = RMSScheduler();
#endif
	
#if SCHEDULER_TYPE == 0
	static int prevProcess=-1;

	// To avoid repetitiveness for hundreds of cycles, we will only print when there's
	// a change of processes
	if(currProcess != prevProcess)
	{

		// Print process details for LINUX scheduler
		printf("Time: %d Process: %d Prio Level: %d Quantum : %d\n", timerTick, processes[currProcess].procNum+1,
			processes[currProcess].prio, processes[currProcess].quantum);
		prevProcess=currProcess;
	}
#elif SCHEDULER_TYPE == 1

	// Print process details for RMS scheduler

	printf("Time: %d ", timerTick);
	if(currProcess == -1)
		printf("---\n");
	else
	{
		// If we have busted a processe's deadline, print !! first
		int bustedDeadline = (timerTick >= processes[currProcess].deadline);

		if(bustedDeadline)
			printf("!! ");

		printf("P%d Deadline: %d", currProcess+1, processes[currProcess].deadline);

		if(bustedDeadline)
			printf(" !!\n");
		else
			printf("\n");
	}

#endif

	// Increment timerTick. You will use this for scheduling decisions.
	timerTick++;
}

void startTimer()
{
	// In an actual OS this would make hardware calls to set up a timer
	// ISR, start an actual physical timer, etc. Here we will simulate a timer
	// by calling timerISR every millisecond

	int i;

#if SCHEDULER_TYPE==0
	int total = processes[currProcess].quantum;

	for(i=0; i<PRIO_LEVELS; i++)
	{
		total += totalQuantum(activeList[i]);
	}

	for(i=0; i<NUM_RUNS * total; i++)
	{
		timerISR();
		usleep(1000);
	}
#elif SCHEDULER_TYPE==1

	// Find LCM of all periods
	int lcm = prioLCM(readyQueue);

	for(i=0; i<NUM_RUNS*lcm; i++)
	{
		timerISR();
		usleep(1000);
	}
#endif
}

void startOS()
{
#if SCHEDULER_TYPE == 0
	// There must be at least one process in the activeList
	currPrio = findNextPrio(0);

	if(currPrio < 0)
	{
		printf("ERROR: There are no processes to run!\n");
		return;
	}

	// set the first process
	currProcess = remove(&activeList[currPrio]);

#elif SCHEDULER_TYPE == 1
	currProcessNode = prioRemove(&readyQueue);
	currProcess = currProcessNode->procNum;
#endif

	// Start the timer
	startTimer();

#if SCHEDULER_TYPE == 0
	int i;

	for(i=0; i<PRIO_LEVELS; i++)
		destroy(&activeList[i]);

#elif SCHEDULER_TYPE == 1
	prioDestroy(&readyQueue);
	prioDestroy(&blockedQueue);

	if(suspended != NULL)
		free(suspended);
#endif
}

void initOS()
{
	// Initialize all variables
	procCount=0;
	timerTick=0;
	currProcess = 0;
	currPrio = 0;
#if SCHEDULER_TYPE == 0
	int i;

	// Set both queue lists to NULL
	for(i=0; i<PRIO_LEVELS; i++)
	{
		queueList1[i]=NULL;
		queueList2[i]=NULL;
	}
#elif SCHEDULER_TYPE == 1

	// Set readyQueue and blockedQueue to NULL
	readyQueue=NULL;
	blockedQueue=NULL;

	// The suspended variable is used to store
	// which process was pre-empted.
	suspended = NULL;
#endif

}

#if SCHEDULER_TYPE == 0

// Returns the quantum in ms for a particular priority level.
int findQuantum(int priority)
{
	return ((PRIO_LEVELS - 1) - priority) * QUANTUM_STEP + QUANTUM_MIN;
}

// Adds a process to the process table
int addProcess(int priority)
{
	if(procCount >= NUM_PROCESSES)
		return -1;

	// Insert process data into the process table
	processes[procCount].procNum = procCount;
	processes[procCount].prio = priority;
	processes[procCount].quantum = findQuantum(priority);
	processes[procCount].timeLeft = processes[procCount].quantum;

	// Add to the active list
	insert(&activeList[priority], processes[procCount].procNum, processes[procCount].quantum);
	procCount++;
	return 0;
}
#elif SCHEDULER_TYPE == 1

// Adds a process to the process table
int addProcess(int p, int c)
{
	if(procCount >= NUM_PROCESSES)
		return -1;

		// Insert process data into the process table
		processes[procCount].p = p;
		processes[procCount].c = c;
		processes[procCount].timeLeft=c;
		processes[procCount].deadline = p;

		// And add to the ready queue.
		prioInsert(&readyQueue, procCount, p, p);
	procCount++;
	return 0;
}


#endif


