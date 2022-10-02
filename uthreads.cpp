#include "uthreads.h"
#include <csignal>
#include <vector>
#include <iostream>
#include <csetjmp>
#include <sys/time.h>
#include "Thread.h"
#include <list>


/********  Constants. ********/

#define FAILURE -1
#define SUCCESS 0
#define SYS_ERROR_MSG "system error: "
#define LIBRARY_ERROR_MSG "thread library error: "
#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define TERMINATED 3
#define MAIN_TID 0
#define EXECUTE_THREAD 1
#define SECOND_IN_MILLISECOND 1000000


/********  Data structures of the library. ********/

// Holds the priorities quantum time
static std::vector<int> quantumPriorityList;
// Signal set.
static sigset_t set;
// Signal handler.
static struct sigaction sa = {nullptr};
// Timer of scheduler.
static struct itimerval timer = {0,0,0,0};
// Holds all existed threads.
static std::vector<Thread*> existedThreadsList;
// Current running thread id
static int currentRunningTid = MAIN_TID;
// Number of threads.
static int quantityThreads = 0;
// Holds all the id of the threads that are in READY state, in order.
static std::list<int> readyTidList;
// Total number of quantum elapsed.
static int totalQuantumElapsed = 0;


/******** Helper functions ********/

// Checks if the given array has valid quantum time.
bool isValidQuantum(const int *quantum_usecs, const int size)
{
	if(size <= 0)
	{
		return false;
	}
	for (int i = 0; i < size; i++)
	{
		if (quantum_usecs[i] <= 0)
		{
			return false;
		}
	}
	return true;
}

// Adds quantum time to quantum time list, from given array.
void addQuantumTimes(const int *quantum_usecs, const int size)
{
	for (int i = 0; i < size; i++)
	{
		quantumPriorityList.push_back(quantum_usecs[i]);
	}
}

void maskingBlock()
{
	if(sigprocmask(SIG_BLOCK, &set, nullptr) < 0)
	{
		std::cerr << SYS_ERROR_MSG << "failed to initialize masking set.\n";
		exit(1);
	}
}

// Unblocks masking.
void maskingUnblock()
{
	if(sigprocmask(SIG_UNBLOCK, &set, nullptr) < 0)
	{
		std::cerr << SYS_ERROR_MSG << "failed to initialize masking set.\n";
		exit(1);
	}
}

// Initializes the masking set.
void initializeMaskingSet()
{
	sigemptyset(&set);
	if(sigaddset(&set,SIGVTALRM) < 0)
	{
		std::cerr << SYS_ERROR_MSG << "failed to initialize masking set.\n";
		exit(1);
	}
}

// Returns the number of seconds, in given milliseconds.
int getSeconds(int milliseconds)
{
	return milliseconds / SECOND_IN_MILLISECOND;
}

// Sets timer to given quantum value.
void setTimer(int quantum)
{
	timer.it_value.tv_sec = getSeconds(quantum); // timer seconds.
	timer.it_value.tv_usec = quantum - timer.it_value.tv_sec * SECOND_IN_MILLISECOND; // timer milliseconds.
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;

	if (setitimer (ITIMER_VIRTUAL, &timer, nullptr) < 0)
	{
		std::cerr << SYS_ERROR_MSG << "failed to set a timer.\n";
		exit(1);
	}
}

// Checks if we have terminated threads, that we need to dealloc from the memory.
void checkAndDeleteTerminated()
{
	maskingBlock();
	for(Thread* thread:existedThreadsList)
	{
		if(thread != nullptr)
		{
			if(thread->getState() == TERMINATED)
			{
				delete thread->getBuffer();
				delete thread;
				thread = nullptr;
			}
		}
	}
	maskingUnblock();
}

// Changes the state of the current running thread (there exist single running thread) to ready state.
void changeStateRunningToReady()
{
	readyTidList.push_back(currentRunningTid);
	existedThreadsList[currentRunningTid]->setState(READY);
}

// Changes the next thread on the 'ready thread list', to become the new current running thread.
void changeNextReadyToRunning()
{
	maskingBlock();
	existedThreadsList[currentRunningTid]->increaseQuantumElapsed();
	++totalQuantumElapsed;

	currentRunningTid = readyTidList.front();
	readyTidList.pop_front();

	existedThreadsList[currentRunningTid]->setState(RUNNING);
	setTimer(quantumPriorityList[existedThreadsList[currentRunningTid]->getPriority()]);

	maskingUnblock();
	siglongjmp(existedThreadsList[currentRunningTid]->getBuffer(), EXECUTE_THREAD);
}

// Signal handler function, for virtual time expiration.
void switchThread(int signum)
{
	int returnValue = sigsetjmp(existedThreadsList[currentRunningTid]->getBuffer(),1);
	if(returnValue == EXECUTE_THREAD)
	{
		return;
	}
	changeStateRunningToReady();
	changeNextReadyToRunning();
}

// Initializes the signal handler.
void initializeSignalHandler()
{
	sa.sa_handler = &switchThread;
	sa.sa_mask = set;
	if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
	{
		std::cerr << SYS_ERROR_MSG << "failed to initialize signal handler.\n";
		exit(1);
	}
}

// Initializes the quantum priority from given quantum list. On success, return 0. On failure
// , return -1.
int initializeQuantumPriority(const int *quantum_usecs, int size)
{
	if (isValidQuantum(quantum_usecs, size))
	{
		addQuantumTimes(quantum_usecs, size);
		return SUCCESS;
	}
	return FAILURE;
}

// Creates the main thread.
void initializeMainThread()
{
	maskingBlock();
	__jmp_buf_tag* mainBuffer = new(__jmp_buf_tag);
	maskingUnblock();
	sigsetjmp(mainBuffer, 1);
	maskingBlock();
	Thread *mainThread = new Thread(mainBuffer, MAIN_TID, 0, RUNNING);
	maskingUnblock();
	existedThreadsList.push_back(mainThread);
	++quantityThreads;
	setTimer(quantumPriorityList[existedThreadsList[MAIN_TID]->getPriority()]);
}

// Returns the next available tid, returns -1 if it fails.
int getAvailableTid()
{
	checkAndDeleteTerminated(); // delete threads, that have been blocked while they were running.
	if (quantityThreads == MAX_THREAD_NUM)
	{
		return FAILURE;
	}

	int availableTid = 0;
	for(Thread* thread: existedThreadsList)
	{
		if (thread == nullptr)
		{
			return availableTid;
		}
		++availableTid;
	}

	return availableTid;
}

// Checks if given priority is valid.
bool isValidPriority(int priority)
{
	return (priority >= 0 && (size_t) priority <= quantumPriorityList.size() - 1);
}

// Checks if the given tid is valid.
bool isValidTid(int tid)
{
	if(tid >= 0 && ((size_t) tid < existedThreadsList.size()))
	{
		if(existedThreadsList[tid] == nullptr){return false;}
		return existedThreadsList[tid]->getState() != TERMINATED;
	}
	return false;
}

// Frees all the memory in the program.
void freeMemory()
{
	maskingBlock();
	for(Thread* thread : existedThreadsList)
	{
		if(thread != nullptr)
		{
			delete thread->getBuffer();
			delete thread;
			thread = nullptr;
		}
	}
	maskingUnblock();
}

// Terminates the main thread.
void terminateMainThread()
{
	freeMemory();
	exit(SUCCESS);
}

// Terminates the current running thread.
void terminateCurrentRunningThread()
{
	existedThreadsList[currentRunningTid]->setState(TERMINATED); // Will be deleted next time we spawn a new thread.
	// Nonetheless the thread will be eventually deleted, right before terminating the main thread.
	--quantityThreads;
	changeNextReadyToRunning();
}

// Terminates blocked\ready thread, with the given id.
void terminateBlockedOrReadyThread(int tid)
{
	maskingBlock();
	if(existedThreadsList[tid]->getState() == READY)
	{
		readyTidList.remove(tid);
	}
	delete existedThreadsList[tid]->getBuffer();
	delete existedThreadsList[tid];
	existedThreadsList[tid] = nullptr;
	--quantityThreads;
	maskingUnblock();
}


#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
	address_t ret;
	asm volatile("xor    %%fs:0x30,%0\n"
				 "rol    $0x11,%0\n"
	: "=g" (ret)
	: "0" (addr));
	return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
		"rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif

// Creates a new thread
void createThread(void (*f)(void), int priority, int tid)
{
	maskingBlock();
	address_t sp, pc;
	__jmp_buf_tag* buffer = new(__jmp_buf_tag);
	Thread *thread = new Thread(buffer, tid, priority, READY);
	maskingUnblock();

	sp = (address_t)thread->getStack() + STACK_SIZE - sizeof(address_t);
	pc = (address_t)f;
	sigsetjmp(buffer, 1);
	(buffer->__jmpbuf)[JB_SP] = translate_address(sp);
	(buffer->__jmpbuf)[JB_PC] = translate_address(pc);
	sigemptyset(&buffer->__saved_mask);

	if(existedThreadsList.size() - 1 < (size_t) tid) // in this case we should increment the length of our vector.
	{
		existedThreadsList.push_back(thread);
	} else // otherwise, there is an empty space (previously terminated thread) that the new thread can occupy.
		{
			existedThreadsList[tid] = thread;
		}

	readyTidList.push_back(tid);
	++quantityThreads;
}


/******** API functions ********/

/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * an array of the length of a quantum in micro-seconds for each priority.
 * It is an error to call this function with an array containing non-positive integer.
 * size - is the size of the array.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int *quantum_usecs, int size)
{
	if(initializeQuantumPriority(quantum_usecs, size) == FAILURE)
	{
		std::cerr << LIBRARY_ERROR_MSG << "invalid quantum priority.\n";
		return FAILURE;
	}

	initializeMaskingSet();

	initializeSignalHandler();

	maskingUnblock();

	initializeMainThread();

	return SUCCESS;
}

/*
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * priority - The priority of the new thread.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)(void), int priority)
{
	int tid = getAvailableTid();

	if(tid == FAILURE || !(isValidPriority(priority)))
	{
		std::cerr << LIBRARY_ERROR_MSG << "exceeded the limit of threads.\n";
		return FAILURE;
	}

	if(f == nullptr)
	{
		std::cerr << LIBRARY_ERROR_MSG << "invalid function was given.\n";
		return FAILURE;
	}

	createThread(f, priority, tid);
	return tid;
}

/*
 * Description: This function changes the priority of the thread with ID tid.
 * If this is the current running thread, the effect should take place only the
 * next time the thread gets scheduled.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_change_priority(int tid, int priority)
{
	if(isValidTid(tid) && isValidPriority(priority))
	{
		existedThreadsList[tid]->setPriority(priority);
	}else
		{
			std::cerr << LIBRARY_ERROR_MSG << "invalid value was entered.\n";
			return FAILURE;
		}
	return SUCCESS;
}

/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
int uthread_terminate(int tid)
{
	if(isValidTid(tid))
	{
		if(tid == MAIN_TID)
		{
			terminateMainThread();
		}
		else if(tid == currentRunningTid)
		{
			terminateCurrentRunningThread();
		}
		else
			{
				terminateBlockedOrReadyThread(tid);
			}
	} else
		{
			std::cerr << LIBRARY_ERROR_MSG << "invalid value was entered, while trying to terminate a thread.\n";
			return FAILURE;
		}

	return SUCCESS;
}

/*
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid)
{
	if(isValidTid(tid) && (tid != MAIN_TID) )
	{
		existedThreadsList[tid]->setState(BLOCKED);
		if(tid == currentRunningTid)
		{
			if(sigsetjmp(existedThreadsList[currentRunningTid]->getBuffer(),1) == EXECUTE_THREAD)
			{
				return SUCCESS;
			}
			changeNextReadyToRunning();
		}
		readyTidList.remove(tid);
	}
	else
		{
			std::cerr << LIBRARY_ERROR_MSG << "invalid value was entered.\n";
			return FAILURE;
		}
	return SUCCESS;
}

/*
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state if it's not synced. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid)
{
	if (isValidTid(tid))
	{
		if (existedThreadsList[tid]->getState() == BLOCKED)
		{
			existedThreadsList[tid]->setState(READY);
			readyTidList.push_back(tid);
		}
	}
	else
		{
			std::cerr << LIBRARY_ERROR_MSG << "invalid value was entered.\n";
			return FAILURE;
		}
	return SUCCESS;
}

/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid()
{
	return currentRunningTid;
}

/*
 * Description: This function returns the total number of quantums since
 * the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums()
{
	return totalQuantumElapsed + 1;
}

/*
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered an error.
 * Return value: On success, return the number of quantums of the thread with ID tid.
 * 			     On failure, return -1.
*/
int uthread_get_quantums(int tid)
{
	if(isValidTid(tid))
	{
		if (tid == currentRunningTid)
		{
			return existedThreadsList[tid]->getQuantumElapsed() + 1;
		}
		return existedThreadsList[tid]->getQuantumElapsed();
	}
	std::cerr << LIBRARY_ERROR_MSG << "invalid value was entered.\n";
	return FAILURE;
}
