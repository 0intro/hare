/*************************************************************************
 *                                                                       *
 *  NOTE: THIS CODE IS IDENTICAL TO THAT OF NPB VERSION 3.0. IT IS NOT   *
 *        AVAILABLE IN CLASS D                                           *
 *                                                                       *
 *************************************************************************/


/*************************************************************************
 *                                                                       * 
 *       N  A  S     P A R A L L E L     B E N C H M A R K S  3.2        *
 *                                                                       *
 *                      O p e n M P     V E R S I O N                    *
 *                                                                       * 
 *                                  I S                                  * 
 *                                                                       * 
 ************************************************************************* 
 *                                                                       * 
 *   This benchmark is an OpenMP version of the NPB IS code.             *
 *   It is described in NAS Technical Report 99-011.                     *
 *                                                                       *
 *   Permission to use, copy, distribute and modify this software        *
 *   for any purpose with or without fee is hereby granted.  We          *
 *   request, however, that all derived work reference the NAS           *
 *   Parallel Benchmarks 3.2. This software is provided "as is"          *
 *   without express or implied warranty.                                *
 *                                                                       *
 *   Information on NPB 3.2, including the technical report, the         *
 *   original specifications, source code, results and information       *
 *   on how to submit new results, is available at:                      *
 *                                                                       *
 *          http://www.nas.nasa.gov/Software/NPB/                        *
 *                                                                       *
 *   Send comments or suggestions to  npb@nas.nasa.gov                   *
 *                                                                       *
 *         NAS Parallel Benchmarks Group                                 *
 *         NASA Ames Research Center                                     *
 *         Mail Stop: T27A-1                                             *
 *         Moffett Field, CA   94035-1000                                *
 *                                                                       *
 *         E-mail:  npb@nas.nasa.gov                                     *
 *         Fax:     (650) 604-3957                                       *
 *                                                                       *
 ************************************************************************* 
 *                                                                       * 
 *   Author: M. Yarrow                                                   * 
 *   OpenMP version:                                                     * 
 *           H. Jin                                                      * 
 *                                                                       * 
 *************************************************************************/

#include "npbparams.h"
#include <stdlib.h>
#include <stdio.h>
#ifdef _OPENMP
#include <omp.h>
#endif


/*****************************************************************/
/* For serial IS, buckets are not really req'd to solve NPB1 IS  */
/* spec, but their use on some machines improves performance, on */
/* other machines the use of buckets compromises performance,    */
/* probably because it is extra computation which is not req'd.  */
/* (Note: Mechanism not understood, probably cache related)      */
/* Example:  SP2-66MhzWN:  50% speedup with buckets              */
/* Example:  SGI Indy5000: 50% slowdown with buckets             */
/* Example:  SGI O2000:   400% slowdown with buckets (Wow!)      */
/*****************************************************************/
/* Bucket sort is not used in OpenMP */
/* #define USE_BUCKETS  */


/******************/
/* default values */
/******************/
#ifndef CLASS
#define CLASS 'S'
#endif


/*************/
/*  CLASS S  */
/*************/
#if CLASS == 'S'
#define  TOTAL_KEYS_LOG_2    16
#define  MAX_KEY_LOG_2       11
#define  NUM_BUCKETS_LOG_2   9
#endif


/*************/
/*  CLASS W  */
/*************/
#if CLASS == 'W'
#define  TOTAL_KEYS_LOG_2    20
#define  MAX_KEY_LOG_2       16
#define  NUM_BUCKETS_LOG_2   10
#endif

/*************/
/*  CLASS A  */
/*************/
#if CLASS == 'A'
#define  TOTAL_KEYS_LOG_2    23
#define  MAX_KEY_LOG_2       19
#define  NUM_BUCKETS_LOG_2   10
#endif


/*************/
/*  CLASS B  */
/*************/
#if CLASS == 'B'
#define  TOTAL_KEYS_LOG_2    25
#define  MAX_KEY_LOG_2       21
#define  NUM_BUCKETS_LOG_2   10
#endif


/*************/
/*  CLASS C  */
/*************/
#if CLASS == 'C'
#define  TOTAL_KEYS_LOG_2    27
#define  MAX_KEY_LOG_2       23
#define  NUM_BUCKETS_LOG_2   10
#define  USE_HEAP
#endif


#define  TOTAL_KEYS          (1 << TOTAL_KEYS_LOG_2)
#define  MAX_KEY             (1 << MAX_KEY_LOG_2)
#define  NUM_BUCKETS         (1 << NUM_BUCKETS_LOG_2)
#define  NUM_KEYS            TOTAL_KEYS
#define  SIZE_OF_BUFFERS     NUM_KEYS  
                                           

#define  MAX_ITERATIONS      10
#define  TEST_ARRAY_SIZE     5


/*************************************/
/* Typedef: if necessary, change the */
/* size of int here by changing the  */
/* int type to, say, long            */
/*************************************/
typedef  int  INT_TYPE;


/********************/
/* Some global info */
/********************/
INT_TYPE *key_buff_ptr_global;         /* used by full_verify to get */
                                       /* copies of rank info        */

int      passed_verification;
                                 

/************************************/
/* These are the three main arrays. */
/* See SIZE_OF_BUFFERS def above    */
/************************************/
INT_TYPE key_array[SIZE_OF_BUFFERS],    
         **key_buff1 = NULL,    
         key_buff2[SIZE_OF_BUFFERS],
         partial_verify_vals[TEST_ARRAY_SIZE];


/**********************/
/* Partial verif info */
/**********************/
INT_TYPE test_index_array[TEST_ARRAY_SIZE],
         test_rank_array[TEST_ARRAY_SIZE],

         S_test_index_array[TEST_ARRAY_SIZE] = 
                             {48427,17148,23627,62548,4431},
         S_test_rank_array[TEST_ARRAY_SIZE] = 
                             {0,18,346,64917,65463},

         W_test_index_array[TEST_ARRAY_SIZE] = 
                             {357773,934767,875723,898999,404505},
         W_test_rank_array[TEST_ARRAY_SIZE] = 
                             {1249,11698,1039987,1043896,1048018},

         A_test_index_array[TEST_ARRAY_SIZE] = 
                             {2112377,662041,5336171,3642833,4250760},
         A_test_rank_array[TEST_ARRAY_SIZE] = 
                             {104,17523,123928,8288932,8388264},

         B_test_index_array[TEST_ARRAY_SIZE] = 
                             {41869,812306,5102857,18232239,26860214},
         B_test_rank_array[TEST_ARRAY_SIZE] = 
                             {33422937,10244,59149,33135281,99}, 

         C_test_index_array[TEST_ARRAY_SIZE] = 
                             {44172927,72999161,74326391,129606274,21736814},
         C_test_rank_array[TEST_ARRAY_SIZE] = 
                             {61147,882988,266290,133997595,133525895};



/***********************/
/* function prototypes */
/***********************/
double	randlc( double *X, double *A );

void full_verify( void );

void c_print_results( char   *name,
                      char   class,
                      int    n1, 
                      int    n2,
                      int    n3,
                      int    niter,
                      double t,
                      double mops,
		      char   *optype,
                      int    passed_verification,
                      char   *npbversion,
                      char   *compiletime,
                      char   *cc,
                      char   *clink,
                      char   *c_lib,
                      char   *c_inc,
                      char   *cflags,
                      char   *clinkflags );


void    timer_clear( int n );
void    timer_start( int n );
void    timer_stop( int n );
double  timer_read( int n );


/*
 *    FUNCTION RANDLC (X, A)
 *
 *  This routine returns a uniform pseudorandom double precision number in the
 *  range (0, 1) by using the linear congruential generator
 *
 *  x_{k+1} = a x_k  (mod 2^46)
 *
 *  where 0 < x_k < 2^46 and 0 < a < 2^46.  This scheme generates 2^44 numbers
 *  before repeating.  The argument A is the same as 'a' in the above formula,
 *  and X is the same as x_0.  A and X must be odd double precision integers
 *  in the range (1, 2^46).  The returned value RANDLC is normalized to be
 *  between 0 and 1, i.e. RANDLC = 2^(-46) * x_1.  X is updated to contain
 *  the new seed x_1, so that subsequent calls to RANDLC using the same
 *  arguments will generate a continuous sequence.
 *
 *  This routine should produce the same results on any computer with at least
 *  48 mantissa bits in double precision floating point data.  On Cray systems,
 *  double precision should be disabled.
 *
 *  David H. Bailey     October 26, 1990
 *
 *     IMPLICIT DOUBLE PRECISION (A-H, O-Z)
 *     SAVE KS, R23, R46, T23, T46
 *     DATA KS/0/
 *
 *  If this is the first call to RANDLC, compute R23 = 2 ^ -23, R46 = 2 ^ -46,
 *  T23 = 2 ^ 23, and T46 = 2 ^ 46.  These are computed in loops, rather than
 *  by merely using the ** operator, in order to insure that the results are
 *  exact on all systems.  This code assumes that 0.5D0 is represented exactly.
 */

/*****************************************************************/
/*************           R  A  N  D  L  C             ************/
/*************                                        ************/
/*************    portable random number generator    ************/
/*****************************************************************/

static int      KS=0;
static double	R23, R46, T23, T46;
#ifdef _OPENMP
#pragma omp threadprivate(KS, R23, R46, T23, T46)
#endif

double	randlc(X, A)
double *X;
double *A;
{
      double		T1, T2, T3, T4;
      double		A1;
      double		A2;
      double		X1;
      double		X2;
      double		Z;
      int     		i, j;

      if (KS == 0) 
      {
        R23 = 1.0;
        R46 = 1.0;
        T23 = 1.0;
        T46 = 1.0;
    
        for (i=1; i<=23; i++)
        {
          R23 = 0.50 * R23;
          T23 = 2.0 * T23;
        }
        for (i=1; i<=46; i++)
        {
          R46 = 0.50 * R46;
          T46 = 2.0 * T46;
        }
        KS = 1;
      }

/*  Break A into two parts such that A = 2^23 * A1 + A2 and set X = N.  */

      T1 = R23 * *A;
      j  = T1;
      A1 = j;
      A2 = *A - T23 * A1;

/*  Break X into two parts such that X = 2^23 * X1 + X2, compute
    Z = A1 * X2 + A2 * X1  (mod 2^23), and then
    X = 2^23 * Z + A2 * X2  (mod 2^46).                            */

      T1 = R23 * *X;
      j  = T1;
      X1 = j;
      X2 = *X - T23 * X1;
      T1 = A1 * X2 + A2 * X1;
      
      j  = R23 * T1;
      T2 = j;
      Z = T1 - T23 * T2;
      T3 = T23 * Z + A2 * X2;
      j  = R46 * T3;
      T4 = j;
      *X = T3 - T46 * T4;
      return(R46 * *X);
} 




/*****************************************************************/
/************   F  I  N  D  _  M  Y  _  S  E  E  D    ************/
/************                                         ************/
/************ returns parallel random number seq seed ************/
/*****************************************************************/

/*
 * Create a random number sequence of total length nn residing
 * on np number of processors.  Each processor will therefore have a
 * subsequence of length nn/np.  This routine returns that random
 * number which is the first random number for the subsequence belonging
 * to processor rank kn, and which is used as seed for proc kn ran # gen.
 */

double   find_my_seed( int kn,        /* my processor rank, 0<=kn<=num procs */
                       int np,        /* np = num procs                      */
                       long nn,       /* total num of ran numbers, all procs */
                       double s,      /* Ran num seed, for ex.: 314159265.00 */
                       double a )     /* Ran num gen mult, try 1220703125.00 */
{

      double t1,t2;
      long   mq,nq,kk,ik;

      if ( kn == 0 ) return s;

      mq = (nn/4 + np - 1) / np;
      nq = mq * 4 * kn;               /* number of rans to be skipped */

      t1 = s;
      t2 = a;
      kk = nq;
      while ( kk > 1 ) {
      	 ik = kk / 2;
         if( 2 * ik ==  kk ) {
            (void)randlc( &t2, &t2 );
	    kk = ik;
	 }
	 else {
            (void)randlc( &t1, &t2 );
	    kk = kk - 1;
	 }
      }
      (void)randlc( &t1, &t2 );

      return( t1 );

}



/*****************************************************************/
/*************      C  R  E  A  T  E  _  S  E  Q      ************/
/*****************************************************************/

void	create_seq( double seed, double a )
{
	double x, s;
	int    i, k;

#ifdef _OPENMP
#pragma omp parallel private(x,s,i,k)
#endif
    {
	int k1, k2;
#ifdef _OPENMP
	int myid, num_procs, mq;

	myid = omp_get_thread_num();
	num_procs = omp_get_num_threads();

	mq = (NUM_KEYS + num_procs - 1) / num_procs;
	k1 = mq * myid;
	k2 = k1 + mq;
	if ( k2 > NUM_KEYS ) k2 = NUM_KEYS;

	KS = 0;
	s = find_my_seed( myid, num_procs,
			  (long)4*NUM_KEYS, seed, a );
#else
	k1 = 0;
	k2 = NUM_KEYS;
	s = seed;
#endif

        k = MAX_KEY/4;

	for (i=k1; i<k2; i++)
	{
	    x = randlc(&s, &a);
	    x += randlc(&s, &a);
    	    x += randlc(&s, &a);
	    x += randlc(&s, &a);  

            key_array[i] = k*x;
	}
    } /*omp parallel*/
}



/*****************************************************************/
/*****************    Allocate Working Buffer     ****************/
/*****************************************************************/
void alloc_key_buff()
{
    int i, num_procs;

    if (key_buff1) return;

#ifdef _OPENMP
    num_procs = omp_get_max_threads();
#else
    num_procs = 1;
#endif
    key_buff1 = (INT_TYPE **)malloc(sizeof(INT_TYPE *) * num_procs);
    if (!key_buff1) {
        perror("malloc");
        exit(1);
    }
    for (i = 0; i < num_procs; i++) {
        key_buff1[i] = (INT_TYPE *)malloc(sizeof(INT_TYPE) * MAX_KEY);
        if (!key_buff1[i]) {
            perror("malloc");
            exit(1);
        }
    }
}



/*****************************************************************/
/*************    F  U  L  L  _  V  E  R  I  F  Y     ************/
/*****************************************************************/


void full_verify()
{
    INT_TYPE    i, j;
    INT_TYPE    k;


    
/*  Now, finally, sort the keys:  */

/*  Copy keys into work array; keys in key_array will be reassigned. */
#ifdef _OPENMP
#pragma omp parallel private(i,k)
#endif
  {
#ifdef _OPENMP
    #pragma omp for
#endif
    for( i=0; i<NUM_KEYS; i++ )
        key_buff2[i] = key_array[i];


    /* This is actual sorting */
#ifdef SERIAL_SORT
#ifdef _OPENMP
    #pragma omp master
#endif
    for( i=0; i<NUM_KEYS; i++ ) {
        k = --key_buff_ptr_global[key_buff2[i]];
        key_array[k] = key_buff2[i];
    }
#else /*SERIAL_SORT*/

    /* Memory sorting can be done directly */
#ifdef _OPENMP
    #pragma omp for
#endif
    for( k=0; k<MAX_KEY; k++ ) {
        i = (k==0)? 0 : key_buff_ptr_global[k-1];
        while ( i<key_buff_ptr_global[k] )
            key_array[i++] = k;
    }
#endif /*SERIAL_SORT*/


/*  Confirm keys correctly sorted: count incorrectly sorted keys, if any */

#ifdef _OPENMP
    #pragma omp master
#endif
    j = 0;
#ifdef _OPENMP
    #pragma omp barrier
    #pragma omp for reduction(+:j)
#endif
    for( i=1; i<NUM_KEYS; i++ )
        if( key_array[i-1] > key_array[i] )
            j++;
  } /*omp parallel*/

    if( j != 0 )
        printf( "Full_verify: number of keys out of sort: %d\n", j );
    else
        passed_verification++;

}




/*****************************************************************/
/*************             R  A  N  K             ****************/
/*****************************************************************/


void rank( int iteration )
{

    INT_TYPE    i, k;
    INT_TYPE    *key_buff_ptr, *key_buff_ptr2;


    key_array[iteration] = iteration;
    key_array[iteration+MAX_ITERATIONS] = MAX_KEY - iteration;


/*  Determine where the partial verify test keys are, load into  */
/*  top of array bucket_size                                     */
    for( i=0; i<TEST_ARRAY_SIZE; i++ )
        partial_verify_vals[i] = key_array[test_index_array[i]];


    key_buff_ptr2 = key_array;

    key_buff_ptr = key_buff1[0];

#ifdef _OPENMP
#pragma omp parallel private(i)
#endif
  {
    INT_TYPE *work_buff;
    int myid = 0, num_procs = 1;

#ifdef _OPENMP
    myid = omp_get_thread_num();
    num_procs = omp_get_num_threads();
#endif
    work_buff = key_buff1[myid];


/*  Clear the work array */
    for( i=0; i<MAX_KEY; i++ )
        work_buff[i] = 0;


/*  Ranking of all keys occurs in this section:                 */

/*  In this section, the keys themselves are used as their 
    own indexes to determine how many of each there are: their
    individual population                                       */

#ifdef _OPENMP
    #pragma omp for nowait
#endif
    for( i=0; i<NUM_KEYS; i++ )
        work_buff[key_buff_ptr2[i]]++;  /* Now they have individual key   */
                                       /* population                     */

/*  To obtain ranks of each key, successively add the individual key
    population, not forgetting to add m, the total of lesser keys,
    to the first key population                                          */

    for( i=0; i<MAX_KEY-1; i++ )   
        work_buff[i+1] += work_buff[i];
#ifdef _OPENMP
    #pragma omp barrier
#endif

/*  Accumulate the global key population */
    for( myid=1; myid<num_procs; myid++ ) {
#ifdef _OPENMP
        #pragma omp for nowait
#endif
        for( i=0; i<MAX_KEY; i++ )
            key_buff_ptr[i] += key_buff1[myid][i];
    }

  } /*omp parallel*/

/* This is the partial verify test section */
/* Observe that test_rank_array vals are   */
/* shifted differently for different cases */
    for( i=0; i<TEST_ARRAY_SIZE; i++ )
    {                                             
        k = partial_verify_vals[i];          /* test vals were put here */
        if( 0 <= k  &&  k <= NUM_KEYS-1 )
            switch( CLASS )
            {
                case 'S':
                    if( i <= 2 )
                    {
                        if( key_buff_ptr[k-1] != test_rank_array[i]+iteration )
                        {
                            printf( "Failed partial verification: "
                                  "iteration %d, test key %d\n", 
                                   iteration, i );
                        }
                        else
                            passed_verification++;
                    }
                    else
                    {
                        if( key_buff_ptr[k-1] != test_rank_array[i]-iteration )
                        {
                            printf( "Failed partial verification: "
                                  "iteration %d, test key %d\n", 
                                   iteration, i );
                        }
                        else
                            passed_verification++;
                    }
                    break;
                case 'W':
                    if( i < 2 )
                    {
                        if( key_buff_ptr[k-1] != 
                                          test_rank_array[i]+(iteration-2) )
                        {
                            printf( "Failed partial verification: "
                                  "iteration %d, test key %d\n", 
                                   iteration, i );
                        }
                        else
                            passed_verification++;
                    }
                    else
                    {
                        if( key_buff_ptr[k-1] != test_rank_array[i]-iteration )
                        {
                            printf( "Failed partial verification: "
                                  "iteration %d, test key %d\n", 
                                   iteration, i );
                        }
                        else
                            passed_verification++;
                    }
                    break;
                case 'A':
                    if( i <= 2 )
        	    {
                        if( key_buff_ptr[k-1] != 
                                          test_rank_array[i]+(iteration-1) )
                        {
                            printf( "Failed partial verification: "
                                  "iteration %d, test key %d\n", 
                                   iteration, i );
                        }
                        else
                            passed_verification++;
        	    }
                    else
                    {
                        if( key_buff_ptr[k-1] != 
                                          test_rank_array[i]-(iteration-1) )
                        {
                            printf( "Failed partial verification: "
                                  "iteration %d, test key %d\n", 
                                   iteration, i );
                        }
                        else
                            passed_verification++;
                    }
                    break;
                case 'B':
                    if( i == 1 || i == 2 || i == 4 )
        	    {
                        if( key_buff_ptr[k-1] != test_rank_array[i]+iteration )
                        {
                            printf( "Failed partial verification: "
                                  "iteration %d, test key %d\n", 
                                   iteration, i );
                        }
                        else
                            passed_verification++;
        	    }
                    else
                    {
                        if( key_buff_ptr[k-1] != test_rank_array[i]-iteration )
                        {
                            printf( "Failed partial verification: "
                                  "iteration %d, test key %d\n", 
                                   iteration, i );
                        }
                        else
                            passed_verification++;
                    }
                    break;
                case 'C':
                    if( i <= 2 )
        	    {
                        if( key_buff_ptr[k-1] != test_rank_array[i]+iteration )
                        {
                            printf( "Failed partial verification: "
                                  "iteration %d, test key %d\n", 
                                   iteration, i );
                        }
                        else
                            passed_verification++;
        	    }
                    else
                    {
                        if( key_buff_ptr[k-1] != test_rank_array[i]-iteration )
                        {
                            printf( "Failed partial verification: "
                                  "iteration %d, test key %d\n", 
                                   iteration, i );
                        }
                        else
                            passed_verification++;
                    }
                    break;
            }        
    }




/*  Make copies of rank info for use by full_verify: these variables
    in rank are local; making them global slows down the code, probably
    since they cannot be made register by compiler                        */

    if( iteration == MAX_ITERATIONS ) 
        key_buff_ptr_global = key_buff_ptr;

}      


/*****************************************************************/
/*************             M  A  I  N             ****************/
/*****************************************************************/

int main( argc, argv )
    int argc;
    char **argv;
{

    int             i, iteration, timer_on;

    double          timecounter;

    FILE            *fp;


/*  Initialize timers  */
    timer_on = 0;            
    if ((fp = fopen("timer.flag", "r")) != NULL) {
        fclose(fp);
        timer_on = 1;
    }
    timer_clear( 0 );
    if (timer_on) {
        timer_clear( 1 );
        timer_clear( 2 );
        timer_clear( 3 );
    }

    if (timer_on) timer_start( 3 );


/*  Initialize the verification arrays if a valid class */
    for( i=0; i<TEST_ARRAY_SIZE; i++ )
        switch( CLASS )
        {
            case 'S':
                test_index_array[i] = S_test_index_array[i];
                test_rank_array[i]  = S_test_rank_array[i];
                break;
            case 'A':
                test_index_array[i] = A_test_index_array[i];
                test_rank_array[i]  = A_test_rank_array[i];
                break;
            case 'W':
                test_index_array[i] = W_test_index_array[i];
                test_rank_array[i]  = W_test_rank_array[i];
                break;
            case 'B':
                test_index_array[i] = B_test_index_array[i];
                test_rank_array[i]  = B_test_rank_array[i];
                break;
            case 'C':
                test_index_array[i] = C_test_index_array[i];
                test_rank_array[i]  = C_test_rank_array[i];
                break;
        };

        

/*  Printout initial NPB info */
    printf
      ( "\n\n NAS Parallel Benchmarks (NPB3.2-OMP) - IS Benchmark\n\n" );
    printf( " Size:  %d  (class %c)\n", TOTAL_KEYS, CLASS );
    printf( " Iterations:  %d\n", MAX_ITERATIONS );
#ifdef _OPENMP
    printf( " Number of available threads:  %d\n", omp_get_max_threads() );
#endif
    printf( "\n" );

    if (timer_on) timer_start( 1 );

/*  Generate random number sequence and subsequent keys on all procs */
    create_seq( 314159265.00,                    /* Random number gen seed */
                1220703125.00 );                 /* Random number gen mult */

    alloc_key_buff();
    if (timer_on) timer_stop( 1 );


/*  Do one interation for free (i.e., untimed) to guarantee initialization of  
    all data and code pages and respective tables */
    rank( 1 );  

/*  Start verification counter */
    passed_verification = 0;

    if( CLASS != 'S' ) printf( "\n   iteration\n" );

/*  Start timer  */             
    timer_start( 0 );


/*  This is the main iteration */
    for( iteration=1; iteration<=MAX_ITERATIONS; iteration++ )
    {
        if( CLASS != 'S' ) printf( "        %d\n", iteration );
        rank( iteration );
    }


/*  End of timing, obtain maximum time of all processors */
    timer_stop( 0 );
    timecounter = timer_read( 0 );


/*  This tests that keys are in sequence: sorting of last ranked key seq
    occurs here, but is an untimed operation                             */
    if (timer_on) timer_start( 2 );
    full_verify();
    if (timer_on) timer_stop( 2 );

    if (timer_on) timer_stop( 3 );


/*  The final printout  */
    if( passed_verification != 5*MAX_ITERATIONS + 1 )
        passed_verification = 0;
    c_print_results( "IS",
                     CLASS,
                     TOTAL_KEYS,
                     0,
                     0,
                     MAX_ITERATIONS,
                     timecounter,
                     ((double) (MAX_ITERATIONS*TOTAL_KEYS))
                                                  /timecounter/1000000.,
                     "keys ranked", 
                     passed_verification,
                     NPBVERSION,
                     COMPILETIME,
                     CC,
                     CLINK,
                     C_LIB,
                     C_INC,
                     CFLAGS,
                     CLINKFLAGS );


/*  Print additional timers  */
    if (timer_on) {
       double t_total, t_percent;

       t_total = timer_read( 3 );
       printf("\nAdditional timers -\n");
       printf(" Total execution: %8.3f\n", t_total);
       if (t_total == 0.0) t_total = 1.0;
       timecounter = timer_read(1);
       t_percent = timecounter/t_total * 100.;
       printf(" Initialization : %8.3f (%5.2f%%)\n", timecounter, t_percent);
       timecounter = timer_read(0);
       t_percent = timecounter/t_total * 100.;
       printf(" Benchmarking   : %8.3f (%5.2f%%)\n", timecounter, t_percent);
       timecounter = timer_read(2);
       t_percent = timecounter/t_total * 100.;
       printf(" Sorting        : %8.3f (%5.2f%%)\n", timecounter, t_percent);
    }

    return 0;
         /**************************/
}        /*  E N D  P R O G R A M  */
         /**************************/




