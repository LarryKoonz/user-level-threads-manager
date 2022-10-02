#include "Thread.h"

// Constructor
Thread::Thread(sigjmp_buf buffer, int tid, int priority, int state)
: _buffer(buffer), _tid(tid), _priority(priority), _state(state)
{

}

char *Thread::getStack()
{
	return _stack;
}

void Thread::setBuffer(__jmp_buf_tag *buffer)
{
	_buffer = buffer;
}

int Thread::getState()
{
	return _state;
}

int Thread::getPriority()
{
	return _priority;
}

void Thread::setPriority(int priority)
{
	_priority = priority;
}

void Thread::setState(int state)
{
	_state = state;
}

int Thread::getQuantumElapsed()
{
	return _quantum_elapsed;
}

__jmp_buf_tag *Thread::getBuffer()
{
	return _buffer;
}

void Thread::increaseQuantumElapsed()
{
	++_quantum_elapsed;
}

