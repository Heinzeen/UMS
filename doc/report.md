# AOSV Final Project Report
_A.Y. 2020/2021_

Author: Matteo Marini 1739106

# Introduction
In this project we were asked to create a system that allowed a user-space application to schedule its own threads; after that, we had to expose some information about the scheduling process in the /proc filesystem. To do that I splitted my work in two big parts:
- A *user library* that will expose the interface of the project to a user-space application.
- A *kernel module* that will manage the core functionalities of the project.
In the next sections I will briefly explains how the project works, for a full descriptions of each function, and to get a deep understanding on how to use the library, please refere to the documentation.


# The library
The library that I created is meant to be compiled into a shared object that a user-space application will then need to link in order to use UMS. Moreover, to use my implementation of the user-mode scheduling the user has to link the library pthread. Indeed, in this project I manage the thread with the pthread library, but I wanted the implementation of the threads to be as transparent as possible to the user; for this reason the only moment in which the user is exposed to it it's when there is the need to compile the application (as already said, the pthread library needs to be linked during this process). In any other moment the real implementation of the threads is hidden behind APIs and new type definitions. This is done to decouple as much as possible my project from the implementation of the threads, so that if anything will need to change, the users will not have to change anything in their code.

This library exposes to the user various functions and structs that will be used to create and manage the threads; there are two types of thread defined in this project: _worker threads_ and _scheduler threads_. Each of them has a specific function that is used to create a thread of one type. To create a thread (with the right function) a function pointer has to be passed, with a pointer to some arguments; the functions that create the thread will, in the end, call the given functions passing the given arguments. Scheduler threads also want completion lists to be created; after creating the workers, they can be added to completion lists with the proper functions, and each scheduler thread will execute threads from its completion list. In the end, a user can wait for the execution of all the threads (in the main thread, as one would do using pthreads_join) and should delete all the lists that were created in order to avoid leaks.

The initialization and the exiting functions are autoatically called since they were added to init and fini array, (thus, the user don't even notice them).


# The kernel module
The kernel module manages the core functionalities of UMS. The communication between the module and the library is performed using the IOCTL syscall on a specific file created when the module is loaded. I decided to use IOCTL because I wanted the speed and ease of use of a syscall, but I did not want to implement a new one to avoid modifying the kernel itself; I think that the solution of creating a kernel module and a library is the best since they can be easily shipped and do not require changes in the system to be run.

The kernel manages the schedule of the threads by changing the state of the threads and by calling the schedule() function; for example, if a scheduler needs to execute a worker thread (or vice versa, a worker thread is yielding and has to give the control back to the scheduler) its state will be set to TASK_INTERRUPTIBLE, the worker thread will be called with the function wake_up_process and then the scheduler will call the function schedule(). Since the scheduler has the state TASK_INTERRUPTIBLE, it will not be executed again untill someone (a worker thread scheduled by it, that is yielding) will wake it up.

Some information about the scheduling process are exposed in /proc filesystem; by performing some specific read in those files, information about the workers or the schedulers are printed.


# Results
I developed the whole project in a VM running ubuntu 20.04 lts, with 2 cores and kernel 5.8. During my tests I had an average of a few micro seconds per switch. This value used change from 1 micro seconds to more than 10 micro seconds, but after some tests it seemed that the average is around 5 micro seconds.

The project can run more processes at one, without having problems, thanks to the fact that very few things are in common among all the processes that use UMS; indeed only one list is shared among all processes, but accesses to it are protected by a lock and in rare occasions the processes need to access it, so the slow-down is not high.

# Conclusions
The project was really intense, but very important to develop new skills that we, as students, did not have; working with the kernel module is a new, fascinating experience that can be very important for our future.

In the end, this implementation of user-mode-scheduling is capable of realizing everything required by the project, I think that I found good ways to do what was asked, trying to keep the project as elegant as possible, but without decreasing efficiency. In the future one might think of expanding the projects by adding new functionalities, like preemption.
