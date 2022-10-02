# User-Level Threads Manager
A static library that allows to create and manage user-level threads.<br />
A potential user can include this library and use it according to the package's public interface: [uthreads.h](uthreads.h).

## How to Run
In order to generate a static library file named ***libuthreads.a***, simply run the super complicated command in your terminal:
```
make
```

## How It Works
Once the user has decided to create a new thread, the manager starts scheduling according to [Round-Robin scheduling algorithm](https://en.wikipedia.org/wiki/Round-robin_scheduling).<br />
Each thread, that the user has created, is in one of the following states:
1. Blocked - a blocked thread can be resumed later using ***thread_resume***.
2. Running - this state is available only to a single thread at a time, and it’s the thread that is currently running.
3. Terminated - a thread that has finished its purpose, or was manually terminated by the function ***uthread_terminate***.

## Usage
1. Initialize the thread library by running the function ***uthread_init***.<br />
:no_entry: This function should be executed only once!
2. Create new threads.
3. Terminate / block / resume threads by your choice.
4. Terminate all threads (if they weren’t terminated by themselves).
