/*****************************************************************/
/******     C  _  P  R  I  N  T  _  R  E  S  U  L  T  S     ******/
/*****************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef _OPENMP
#include <omp.h>
#endif

void c_print_results( char   *name,
                      char   clss,
                      int    n1, 
                      int    n2,
                      int    n3,
                      int    niter,
                      double t,
                      double mops,
		      char   *optype,
                      int    verification,
		      long long int checksum,
		      unsigned int np,
                      char   *npbversion,
                      char   *compiletime,
                      char   *cc,
                      char   *clink,
                      char   *c_lib,
                      char   *c_inc,
                      char   *cflags,
                      char   *clinkflags ){
    int num_threads, max_threads=1;
    char *num_threads_set;

/*   figure out number of threads used */
#ifdef _OPENMP
    max_threads = omp_get_max_threads();
#pragma omp parallel shared(num_threads)
{
    #pragma omp master
    num_threads = omp_get_num_threads();
}
#else
    num_threads = 1;
#endif
    num_threads_set = getenv("OMP_NUM_THREADS");
    if (!num_threads_set) num_threads_set = "unset";

    fprintf(stdout,"\n\n %s Benchmark Completed\n", name ); 
    fprintf(stdout," Class           =                %c\n", clss );
    fprintf(stdout," Dimensions      =              %3d\n", n1);
    fprintf(stdout," Measures        =              %3d\n", n2);
    fprintf(stdout," Input Tuples    =     %12d\n", n3);
    fprintf(stdout," Tuples Generated=     %12.0f\n", mops);
    fprintf(stdout," Number of views =     %12d\n", niter );
    fprintf(stdout," Total threads   =     %12d\n", num_threads);
    fprintf(stdout," Request threads =     %12s\n", num_threads_set);
    if (num_threads != max_threads)
    fprintf(stdout," Warning: Threads used differs from threads available");
    fprintf(stdout," Time in seconds =     %12.2f\n", t );
    fprintf(stdout," Tuples/s        =     %12.2f\n", mops/t );
    fprintf(stdout," Operation type  = %s\n", optype);
    if( verification==0 )
      fprintf(stdout," Verification    =       SUCCESSFUL\n" );
    else if( verification==-1)
      fprintf(stdout," Verification    =    NOT PERFORMED\n" );
    else
      fprintf(stdout," Verification    =     UNSUCCESSFUL\n" );
    fprintf(stdout," Checksum        =%17lld\n", checksum);
    if (np>1) 
      fprintf(stdout," Processes       =     %12d\n", np);
    fprintf(stdout," Version         =     %12s\n", npbversion );
    fprintf(stdout," Execution date  =     %12s\n", compiletime );
    fprintf(stdout,"\n Compile options:\n" );
    fprintf(stdout,"    CC           = %s\n", cc );
    fprintf(stdout,"    CLINK        = %s\n", clink );
    fprintf(stdout,"    C_LIB        = %s\n", c_lib );
    fprintf(stdout,"    C_INC        = %s\n", c_inc );
    fprintf(stdout,"    CFLAGS       = %s\n", cflags );
    fprintf(stdout,"    CLINKFLAGS   = %s\n", clinkflags );
#ifdef SMP
    evalue = getenv("MP_SET_NUMTHREADS");
    fprintf(stdout,"   MULTICPUS = %s\n", evalue );
#endif
    fprintf(stdout,"\n Please send all errors/feedbacks to:\n\n" );
    fprintf(stdout," NPB Development Team\n" );
    fprintf(stdout," npb@nas.nasa.gov\n\n" );
}

#define DEFFILE "../config/make.def"
void check_line(char *line, char *label, char *val){
  char *original_line;
  original_line = line;
  /* compare beginning of line and label */
  while (*label != '\0' && *line == *label) {
    line++; label++; 
  }
  /* if *label is not EOS, we must have had a mismatch */
  if (*label != '\0') return;
  /* if *line is not a space, actual label is longer than test label */
  if (!isspace(*line) && *line != '=') return ; 
  /* skip over white space */
  while (isspace(*line)) line++;
  /* next char should be '=' */
  if (*line != '=') return;
  /* skip over white space */
  while (isspace(*++line));
  /* if EOS, nothing was specified */
  if (*line == '\0') return;
  /* finally we've come to the value */
  strcpy(val, line);
  /* chop off the newline at the end */
  val[strlen(val)-1] = '\0';
  if (val[strlen(val) - 1] == '\\') {
    fprintf(stderr,"\n\
      check_line: Error in file %s. Because by historical reasons\n\
      you can't have any continued\n\
      lines in the file make.def, that is, lines ending\n\
      with the character \"\\\". Although it may be ugly, \n\
      you should be able to reformat without continuation\n\
      lines. The offending line is\n %s\n", DEFFILE, original_line);
    exit(1);
  }
}
