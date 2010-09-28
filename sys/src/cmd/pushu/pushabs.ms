
.SH
Introduction

As data processing has moved towards huge distributed and multicore systems there has been a move away from the unix model of doing things using things like piopelines. Instead systems are made as heavily customized middleware, for example 

However as these systems evolve they, and the tools they involve rely on one programming and processing model. Some projects like Microsoft's Dryad[cite] can run unmodified executables but even these require wrapper code. 

We are interested in something similar to streamline[cite] but one in which the shell works as a traditional shell making no assumptions regardin 

Make reusability very difficult and reinvent the weheel. 

in this abstract we present Push a shell for instantiating multiprocess, multichannel and which are potentially, but not necessarily, distributed over a variety of cores. 

the unix way of doing data processing is venerable, flexible and efficient. 

As computation moves to distributed and manycore computing, the unix model no longer works. 

people making processes to take advantage of this is becoming more and more cumbersome. 

In this abstract we present Push, a new unix shell. Push allows users to deploy. 

.SH
Challenges

how do you do this at the systems level?

so how are you going to put the stuff together? 

the unix model only provides uni directional pipes 

no concept of anything other than byte streams. 

how do you run executables unmodified?

How do you implement something that works across platforms? 



.SH
Push: a DISC Shell

Based on rc the successor to the bourne shell in research unix. 
.SH
Syntax

|< and >| 


.SH
Implementation

.SH
Project Status

The original version of Push exists as a fully functional implementation in the Inferno[cite] operating system. It is currently being reimplemented using unix. Both versions will be integrated with the Unified execution environment being developed by the Hare project[cite]. 
