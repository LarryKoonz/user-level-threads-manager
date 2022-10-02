#ifndef OS_EX02_THREAD_H
#define OS_EX02_THREAD_H

#include <csetjmp>
#include "uthreads.h"

class Thread
{
private:
	__jmp_buf_tag* _buffer;
	int _tid;
	int _priority;
	int _state;
	char _stack[STACK_SIZE];
	int _quantum_elapsed = 0;
public:
	// Constructor
	Thread(__jmp_buf_tag* buffer, int tid, int priority, int state);
	char* getStack();
	void setBuffer(__jmp_buf_tag* buffer);
	int getState();
	void setState(int state);
	int getPriority();
	void setPriority(int priority);
	int getQuantumElapsed();
	__jmp_buf_tag* getBuffer();
	void increaseQuantumElapsed();
};


#endif //OS_EX02_THREAD_H
