/**
  * ftq_omp.c : Fixed Time Quantum microbenchmark
  *
  * Written by Matthew Sottile (matt@cs.uoregon.edu)
  * OpenMP added by Brent Gorda (bgorda@llnl.gov)
  *
  * Licensed under the terms of the GNU Public Licence.  See LICENCE_GPL
  * for details.
  */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#ifdef _WITH_OMP
#include <omp.h>
#endif /* _WITH_OMP */

/* use cycle timers from FFTW3 (http://www.fftw.org/).  this
  * defines a "ticks" typedef, usually unsigned long long, that is
  * used to store timings.  currently this code will NOT work if the
  * ticks typedef is something other than an unsigned long long.
  */
#include "cycle.h"

/**
  * macros and defines
  */
#define MAX_SAMPLES    2000000
#define DEFAULT_COUNT  10000
#define DEFAULT_BITS   20
#define MAX_BITS       30
#define MIN_BITS       3

#ifdef CORE15
#define MULTIITER
#define ITERCOUNT      8
#endif
#ifdef CORE31
#define MULTIITER
#define ITERCOUNT      16
#endif
#ifdef CORE63
#define MULTIITER
#define ITERCOUNT      32
#endif

/**
  * global variables
  */

/* samples: each sample has a timestamp and a work count. */
/* static unsigned long long samples[MAX_SAMPLES*2];*/

/**
  * usage()
  */
void usage(char *av0) {
   fprintf(stderr,"usage: %s [-n samples] [-i bits] [-h] [-o outname] [-s]\n",
	  av0);
   exit(EXIT_FAILURE);
}

/**
  * main()
  */
int main(int argc, char **argv) {
   /* local variables */
   unsigned long numsamples = DEFAULT_COUNT;
   unsigned long long interval_length;
   int interval_bits = DEFAULT_BITS;
   unsigned long long count;
   char fname_prefix[1024];
   char fname_times[1024], fname_counts[1024];
   int i;
   FILE *fp;
   int use_stdout = 0;
   int c, option_index;
   /*  unsigned long done;*/
   int done;
   ticks now, last, endinterval;
   unsigned long long *samples; /* [MAX_SAMPLES*2]; */
#ifdef MULTIITER
   register int k;  /* redefined below to be local in scope and 
therefore omp local */
#endif

   sprintf(fname_prefix,"ftq");

   samples = malloc(sizeof(unsigned long long)*MAX_SAMPLES*2);

   /*
    * getopt_long to parse command line options.
    * basically the code from the getopt man page.
    */
   while (1) {
     option_index = 0;
     static struct option long_options[] = {
       {"help",0,0,'h'},
       {"numsamples",0,0,'n'},
       {"interval",0,0,'i'},
       {"outname",0,0,'o'},
       {"stdout",0,0,'s'},
       {0,0,0,0}
     };

     c = getopt_long(argc, argv, "n:hsi:o:",
		    long_options, &option_index);
     if (c == -1)
       break;

     switch (c) {
     case 's':
       use_stdout = 1;
       break;
     case 'o':
       /* default filenames */
       strcpy(fname_prefix,optarg);

       break;
     case 'i':
       interval_bits = atoi(optarg);
       break;
     case 'n':
       numsamples = atoi(optarg);
       break;
     case 'h':
     default:
       usage(argv[0]);
       break;
     }
   }

   sprintf(fname_times,"%s_times.dat",fname_prefix);
   sprintf(fname_counts,"%s_counts.dat",fname_prefix); 

   /* sanity check */
   if (numsamples > MAX_SAMPLES) {
     fprintf(stderr,"WARNING: sample count exceeds maximum.\n");
     fprintf(stderr,"         setting count to maximum.\n");
     numsamples = MAX_SAMPLES;
   }

   if (interval_bits > MAX_BITS || interval_bits < MIN_BITS) {
     fprintf(stderr,"WARNING: interval bits invalid.  set to %d.\n",
	    MAX_BITS);
     interval_bits = MAX_BITS;
   }

   /* set up sampling.  first, take a few bogus samples to warm up the
      cache and pipeline */
   interval_length = 1 << interval_bits; 
   done = 0;
   samples[done*2] = 0;

#pragma omp parallel private(last, endinterval, now, count) shared(samples,interval_length)
    {
      int tid = omp_get_thread_num();
      samples[tid]=tid;
      last = getticks();
      endinterval = (last + interval_length) & (~(interval_length - 1));
      
      //fprintf(stderr, "hi from tid=%d numsamples = %d\n", tid, numsamples);
      last = getticks();

#pragma omp for schedule(static) nowait
      for (done = 0; done < numsamples; done++) {
        count = 0;
        //fprintf(stderr, "hi from tid=%d numsamples = %d\n", tid, numsamples);

        for (now = last; now < endinterval; ) {
#ifdef MULTIITER
	        register int k;
        	for (k=0;k<ITERCOUNT;k++)
	          count++;
	        for (k=0;k<(ITERCOUNT-1);k++)
	          count--;
#else
      	  count++;
#endif
       	  now = getticks();
        } /* for now=last ... */

        //fprintf(stderr, "hi from tid=%d sample[%d] = %d\n", tid, done, samples[done]);
        samples[done*2] = last;
        samples[done*2 + 1] = count;
        last = getticks();
       
        endinterval = (last + interval_length) & (~(interval_length - 1));
      } /* OMP parallel for */
#ifdef _WITH_OMP

    } /* openmp parallel region */
#endif /* _WITH_OMP */

    if (use_stdout == 1) {
      for (i=0;i<numsamples;i++) {
        fprintf(stdout,"%qu %qu\n",samples[i*2],samples[i*2 + 1]);
      }
    } else {
      fp = fopen(fname_times,"w");
      for (i=0;i<numsamples;i++) {
        fprintf(fp,"%qu\n",samples[i*2]);
      }
      fclose(fp);
     
      fp = fopen(fname_counts,"w");
      for (i=0;i<numsamples;i++) {
        fprintf(fp,"%qu\n",samples[i*2 + 1]);
      }
      fclose(fp);
    }
    
    free(samples);
    
    exit(EXIT_SUCCESS);
}

