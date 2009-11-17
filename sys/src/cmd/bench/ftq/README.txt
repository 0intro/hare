------------------------------------------
FTQ: The Fixed Time Quantum microbenchmark
------------------------------------------

This directory contains FTQ, a useful benchmark for probing the interference
characteristics of a particular computer (standalone, cluster node, etc...)
as experienced by user applications.  It is discussed extensively in two
places:

Sottile/Minnich : Cluster 2004 paper
Sottile : Univ. of New Mexico, PhD dissertation

Both of these should be available at some point in downloadable form from
the chamatools web page.  For the time being, the following is a brief
summary of the code, what it does, and how you should run it.

For further information, contact Matt Sottile at matt@cs.uoregon.edu.

0.  Important Files
-------------------

In this directory, the only files that matter are:

Makefile
ftq.c
ftq.h
README.txt

The rest are either experimental or related to the Plan9 port of the code.
The ftq_omp.c code is an experimental OpenMP-based FTQ contributed by
Brent Gorda from LLNL.

1.  What is FTQ?
----------------

FTQ is a simple benchmark that probes interference within a system.  It does
so as follows.  For a given sampling period (say, 1ms), we would like to know
how many units of work can be achieved by a user application.  The unit
of work is defined below.  We can iteratively sample the system, checking for
a series of 1ms sample intervals, how much work is actually achieved in each.
For a high interference system, one will see a great deal of variability in
they output data.  For a low interference system, one will see less
variability.  Of course, there is the distinct possibility that one may be
experiencing constant interference, which would be identical to the low
interference case, except with a lower than ideal mean value.  A method
for investigating whether or not this is the case is discussed below.

Traditionally, a simple benchmark to quantify this effect was what we call
the Fixed Work Quantum benchmark.  That method basically fixes a work quantum
(say, 10000 integer operations), and samples the amount of time it takes
to execute over a series of samples.  The problem with this method is that
although interference is revealed, the lack of a firm time axis (think about it
for a second -- instead of sampling intervals being defined in terms of time,
they're defined in terms of work!) makes sophisticated analysis in things like
the frequency domain difficult and at times, impossible.

FTQ fixes this with two subtle tweaks to FWQ.  First off, instead of performing
a single work quantum that requires on the order of the time we wish the
sampling period to be, FTQ uses many samples of work quanta that are
significantly smaller than the desired sampling period.  Second, due to the
fact that FTQ is a self sampling system, it is entirely possible (actually, 
it's almost guaranteed) that a sample may exceed the sampling interval by
some amount.  FTQ is constructed to compensate for that by shrinking
subsequent samples to get the time series back on track.

In the end, FTQ provides data that both reveals interference and is in a form
where sophisticated analysis is possible depending on the desires of the
user.

2. Building FTQ
---------------

FTQ is simple to build.  It has been tested on MacOSX, Linux, and even at one
time under Windows XP via cygwin (note: Windows hasn't been tested in a long
time).  There is no configuration step at the present time, although
it is likely that one may emerge if the FTQ configuration options get
sophisticated enough.  In the meantime, if one just says:

% make

You should observe 4 executables being created.  They are ftq, ftq15, ftq31,
and ftq63.  Each of these represents increasing granularity work quanta
used for sampling.  For example, ftq uses a single integer increment 
operation as the work quantum, while ftq31 uses 16 increments followed by
15 decrements (effectively equivalent to the single ftq count++).  The
reason for this is that one will observe, especially for tiny work
quanta, jitter in the data due to the fact that the majority (approx 70-80%)
of the instructions executed per work quantum are actually branch and
conditional operations.  This causes some level of fluctuation in how long
a work quantum takes to execute simply due to the structure of the instruction
stream.  As the work quantum increases in granularity, the proportion of
work quantum instructions to control flow and conditional structures becomes
more even, and the fluctuations within the data begin to be due to
interference external to FTQ itself (other than apparent cache related, high
frequency low level perturbations).  

If you would like a version of FTQ that runs in a multicore or SMP
environment where each thread runs the core of the benchmark (resulting
in multiple files when run, one data set per thread), you can say:

% make threads

This yields a similar set of executables, except with t_ as a prefix on the
executable name, indicating a pthreads version.  It is not recommended that
you use the threaded versions on a single core.  In that case, just use
the non-threaded FTQ.

3. Running FTQ
--------------

The simplest way to get FTQ going is to just try it out with a small
sample count and observe what the data looks like.  FTQ has just a few
parameters.  Note that it might sounds strange to have the work quantum
granularity a compile time option, but this is done so that the instruction
stream is not polluted with conditionals enforcing a runtime determined
work quantum.

The parameters are as follows:

  -s : Dump output to the STDOUT
  -o : Prefix for output data files
  -i : Bits in sampling interval limits
  -n : Number of samples to take
  -h : Usage

Let's consider a simple run like this:

% ./ftq63 -o testrun -i 20 -n 1000

What does this produce?  If we look at the current directory, we should
see two new files: testrun_counts.dat and testrun_times.dat.  These are the
outputs from FTQ.

The times file contains the end times of each sample as reported by the high
precision timers in cycle.h.  We should see 1000 lines for the above case,
corresponding to the end times for each of the 1000 samples executed.  Note
that if one subtracts time k from time k+1, one can observe the actual
sampling interval that the k+1'th sample required.  Note that the first
sample has no start time, and is generally thrown out without losing any
really valuable information.

The counts file contains the real data from FTQ.  Each of the 1000 entries
represents the number of work quanta (in this case, 32 increment operations
followed by 31 decrements per work quantum) executed in the time period ending
at the corresponding time in the testrun_times.dat file.  That's about it
for the output data without getting into analysis techniques and tips.

The final argument that is unexplained is the "-i 20" argument.  What does this
mean?  Easy.  The sampling intervals that FTQ uses are determined using a
bitmask and some simple tricks to deal with long samples that must be
compensated for.  The "20" is the number of bits that the desired sampling
period requires in terms of processor cycles (so even though it may seem large,
it isn't).  In other words, for the case above, "-i 20", a single sample
period will span at most (2^20)-1 samples.  On a 2.8GHz CPU, this corresponds
to approximately 0.5ms. **

(**: note - the exponent may be off by one here, and my time computation may
     be off too.  Writing this fast, so please forgive errors until I get a
     chance to proofread this.  -m)

The threaded versions of the code have one additional argument, "-t threads".
This is used to specify the number of threads to be executed.  In the case
of a multithreaded run, instead of just getting files like "ftq_times.dat"
and "ftq_counts.dat", you will get a pair of files for each thread.  The
thread IDs range from 0 to numthreads-1, so for a two threaded run you
will see:

ftq_0_counts.dat
ftq_0_times.dat
ftq_1_counts.dat
ftq_1_times.dat

4. Simple data analysis
-----------------------

So you've made data from FTQ, and want to actually do something with it.
This is pretty easy with tools you likely already have on your desktop.
Here is a brief overview of some things you can try.

You likely have GNU Octave on your machine, or available to install without
much effort.  Let's try looking at the data that was produced above
(testrun_counts.dat).  Here is the sequence of Octave commands to run:

octave:1> load testrun_counts.dat
octave:2> mean(testrun_counts)
ans = 1721.7
octave:3> var(testrun_counts)
ans =  1.0718e+04
octave:4> sqrt(var(testrun_counts))
ans = 103.53
octave:5> plot(testrun_counts)

In the above sequence of commands, we loaded the data into the Octave
array "testrun_counts", looked at it's mean, variance, and standard
deviation.  Finally, we plotted it.  Easy, eh?

