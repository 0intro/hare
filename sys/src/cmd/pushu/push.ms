
 

Motivation

With multimedia, high throughput and other applications. we are moving towards something that involves streaming to deal with large data parallel applications. 

There have been things like map reduce. 
With the advent of multicore and distributed computing into the mainstream the unix stream based way of doing things is more attractive because the type of "embarassing parallel" that people need to solve map well to a graph of autonomous filters that process data in stages. However the principle problem of dealing with guys in the unix model in stages is that currently pipelines are linear. There are methods to create graphical shells using map reduce and systems like streamline which do what we want. 


comfortable and fast way of doing processes in a big distributed system. much of the original work on push was inspired by the difficulties and complications instantiating processes handling large amount of data on large supercomputers as part of the HARE project. 

typically batch jobs are decided, systems are booted, this means that potentially shorter jobs are rebooted. 

how do 

The Push solution is a bit different. Push keeps the traditional unix model of using individual processes. but it creates arbitrary process graphs, act in a complimentary manner to a system that provides a unified view of computation, where this computation can potentially be distributed. Push was designed concurrently with the UEM, which is summarized later and described in detail in [cite].

Push is not a simulated environment. It is an actual shell. Meaning that it is just a common way of process marshaling, argument selection and creating pipelines. 

Wants to create a general way of interacting with different systems. 

allows traditional executables to used, interacts with the system calls in order to make ordered graphs of processes. Push makes no effort to maximize anything. Like Unix shells traditionally are it is a facilitator for composing graphs of processes. 

Push's goal is to provide a method of instantiating composing and using processes in things like Xcpu and the UEM. 

The original push was implemented in the inferno distributed system. this version is implemented in unix, created by modifying Tom Duff's rc shell. 

The original Push

The original push was based on a shell called Mash, and took considerable advantage of Inferno's distributed infrastructure. 

The original push shell took advantage of Inferno's coroutines.
Filters were modules and processes shared a common address space allowing buffers to be passed around.

This considerab ley 

Moving to Unix

A portable version that runs on something with a traditional unix model, especially one where we can't be assured of dynamic libraries, composing filters is not as easy. 

Since we are aiming for a statically linked executable. we can't do the same thing. 

we need to make filters uinsg file descriptors and running processes. 

We do this using a following methods. 

the syntax is transformed into combination pipe structure, first by using a tree transformation to put together filter commands(referred to by shell variables). The actual execution is handled by RC's stack machine 

Implementation of the RC shell

rather than being a traditional interpreted shell, rc compiles its syntax trees into an intermediate stack machine representation with the machine code containing functions, pointers to strings and integers. 

The implementation goes along with this way of doing things. Which means that you need 

Syntax

Creating multiple stream flows is done with two new operators |<(fanout) and >|(fanin). Fanin creates a set of pipes(due to the LIFO nature of rc's pipe syntax in the parse tree, Fanin's are traversed first in the translation of the syntax tree into stack machine code) which are then attached to a filter which takes the incoming byte streams finds their record boundaries and ensures that the incoming data is coalesced into a single stream that maintains the coherency of the incoming data streams. Fanout takes the set of pipes created by Fanin(which are stored on a stack, to allow nesting of Fanins and Fanouts) and connects the input file descriptors to a ORF(output record filter), like IRF, a shell variable referring to a filter executable, which is responsible for taking a single input stream(leftmost child of the fanout tree), separating the input into records, and then distributing these records to a set of file descriptors provided using the command's argument vector. Figure x shows a graph of the mapping of syntax to syntax tree. 



Implementation of Push

push implements process graphs. 
implemented as a set of pipes(a "multipipe") that connect 

The multipipe aspects of push are implemented at three levels of the rc shell.

first change the tree structure. The hidden aspects 

we change the generation 

change the plan9 forking behavior. 




Fanin and Fanout operators

The fanin and fanout operators must appear as pairs. 

are implemented as a tree transformation. 

Unix filters

We implement unix filters as processes communicating through file descriptors instead. 

the shell creates 
Anatomy of a multipipe

Multipipes are contained using a stack inside the the shell. This is for nested fanins and fanouts. 

Writing a filter

filters are simple C programs connected to the multipipes using pipes. They take the number of file descriptors present in the multipipe and then handle them. 

because unix programs are programmed to handle things in different ways we have to do things a little bit differently. This is done by having special purpose filter programs that accept file descriptors as arguments and then use those descriptors to do things.

the internals of a filter are as follows, the shell provides the numbers of the file descriptors to the argument vector(argv in C) to the filter program. The program creates a reader that reads from these. Since the value is a separate process we can do things more easily, one of the main goals of the system is to use a nice concurrent program to handle the inputs rather than polling on a select. 


UEM integration

The key to taking advantage of this approach is coming up with a replacement for the traditional unix system calls, read, open, fork etc... with a new method that handles distribution of processes across cores and the transparent stitching of file descriptors as well as providing an interface to the state of the process.

One of the major advantage of the unix system calls. 

The UEM is described in [cite] and [cite]. Gives you the ability to create things on different systems. 



This means that push leaves the responsibility for distribution and io 

This is implemented by changing the system call functionality to be file system interface interaction instead of the traditional process creation and fd redirection system calls. adding another level of indirection to the system. 

Problems

One of the central problems with the implementation was changing the stack machine to handle operations that directly manipulate the values on argument stack. We had to add a new function Xsimple to hand.le 

Evaluation

