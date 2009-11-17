Chamatools Benchmarks
---------------------

This module of the chamatools sourceforge project contains benchmark related
programs.  The current directories are:

ftq/ : the fixed time quantum microbenchmark.  For a reference on this, see
       the Sottile/Minnich paper at Cluster 2004, or ask Matt for a copy of
       his dissertation.  The versions included here are the standard 
       sequential version, a threaded version with Pthreads, and an OpenMP 
       version.

common/ : files common to any benchmark code in here.  this currently is
          restricted to platform independent, high precision timers.  the
          cycle.h file is the redistributable timer header from the FFTW
          package.

Other relevant, small benchmarks are forthcoming as soon as their code gets
cleaned up and they get added to the repository.

Send Matt (matt@cs.uoregon.edu) e-mail if you need help with anything in here.

