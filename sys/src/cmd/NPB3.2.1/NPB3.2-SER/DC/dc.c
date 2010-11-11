/*
!-------------------------------------------------------------------------!
!                                                                         !
!        N  A  S     P A R A L L E L     B E N C H M A R K S  3.2         !
!                                                                         !
!                      S E R I A L     V E R S I O N                      !
!                                                                         !
!                                   D C									  !
!                                                                         !
!-------------------------------------------------------------------------!
!                                                                         !
!    DC creates all specifided data-cube views.							  !
!    Refer to NAS Technical Report 03-005 for details.                    !
!    It calculates all groupbys in a top down manner using well known     !
!    heuristics and optimizations.                                        !
!                                                                         !
!    Permission to use, copy, distribute and modify this software         !
!    for any purpose with or without fee is hereby granted.  We           !
!    request, however, that all derived work reference the NAS            !
!    Parallel Benchmarks 3.2. This software is provided "as is"           !
!    without express or implied warranty.                                 !
!                                                                         !
!    Information on NPB 3.2, including the technical report, the          !
!    original specifications, source code, results and information        !
!    on how to submit new results, is available at:                       !
!                                                                         !
!           http://www.nas.nasa.gov/Software/NPB/                         !
!                                                                         !
!    Send comments or suggestions to  npb@nas.nasa.gov                    !
!                                                                         !
!          NAS Parallel Benchmarks Group                                  !
!          NASA Ames Research Center                                      !
!          Mail Stop: T27A-1                                              !
!          Moffett Field, CA   94035-1000                                 !
!                                                                         !
!          E-mail:  npb@nas.nasa.gov                                      !
!          Fax:     (650) 604-3957                                        !
!                                                                         !
!-------------------------------------------------------------------------!
! NPB3.2  DC serial version												  !
! Michael Frumkin, Leonid Shabanov                                        !
!-------------------------------------------------------------------------!
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "adc.h"
#include "macrodef.h"
#include "npbparams.h"

#ifdef UNIX
#include <sys/types.h>
#include <unistd.h>

#define MAX_TIMERS 64  /* NPB maximum timers */
  time_t time(time_t *);
  struct tm *localtime(const time_t *clock);
  size_t strftime(char *s, size_t maxsize, const char *format,
                  const struct tm *timeptr);
  void    timer_clear(int);
  void    timer_start(int);
  void    timer_stop(int); 
  double  timer_read(int);
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
                      int    passed_verification,
		      long long int checksum,
		      unsigned int    np,
                      char   *npbversion,
                      char   *compiletime,
                      char   *cc,
                      char   *clink,
                      char   *c_lib,
                      char   *c_inc,
                      char   *cflags,
                      char   *clinkflags );

void initADCpar(ADC_PAR *par);
int ParseParFile(char* parfname, ADC_PAR *par); 
int GenerateADC(ADC_PAR *par);
void ShowADCPar(ADC_PAR *par);
int32 DC(ADC_VIEW_PARS *adcpp);
int Verify(long long int checksum,ADC_VIEW_PARS *adcpp);

#define BlockSize 1024

int main ( int argc, char * argv[] ) 
{
  ADC_PAR *parp;
  ADC_VIEW_PARS *adcpp;
  int32 retCode;

  fprintf(stderr,"\n\n NAS Parallel Benchmarks (NPB3.2-SER) - DC Benchmark\n\n" );
  if(argc!=3){
    fprintf(stderr,"No Paramter file. Using compiled defaults\n");
  }
  if(argc<2){
    fprintf(stderr,"Usage: <program name> <amount of memory>\n");
    fprintf(stderr,"       <file of parameters>\n");
    fprintf(stderr,"Example: bin/dc.S 1000000 DC/ADC.par\n");
    fprintf(stderr,"The last argument, (a parameter file) can be skipped\n");
    exit(1);
  }

  if(  !(parp = (ADC_PAR*) malloc(sizeof(ADC_PAR)))
     ||!(adcpp = (ADC_VIEW_PARS*) malloc(sizeof(ADC_VIEW_PARS)))){
     PutErrMsg("main: malloc failed")
     exit(1);
  }
  initADCpar(parp);
  parp->clss=CLASS;
  if(argc<=2){
    parp->dim=attrnum;
    parp->tuplenum=input_tuples;    
  }else if( (argc==3)&&(!ParseParFile(argv[2], parp))) {
    PutErrMsg("main.ParseParFile failed")
    exit(1);
  }
  ShowADCPar(parp); 
  if(!GenerateADC(parp)) {
     PutErrMsg("main.GenerateAdc failed")
     exit(1);
  }

  adcpp->ndid = parp->ndid;  
  adcpp->clss = parp->clss;
  adcpp->nd = parp->dim;
  adcpp->nm = parp->mnum;
  adcpp->nTasks = 1;
  if(argc==2)
    adcpp->memoryLimit = atoi(argv[1]);
  else{
    /* size of rb-tree with tuplenum nodes */
    adcpp->memoryLimit = parp->tuplenum*(40+4*parp->dim); 
    fprintf(stderr,"Estimated rb-tree size=%ld \n", adcpp->memoryLimit);
  }
  adcpp->nInputRecs = parp->tuplenum;
  strcpy(adcpp->adcName, parp->filename);
  strcpy(adcpp->adcInpFileName, parp->filename);

  if((retCode=DC(adcpp))) {
     PutErrMsg("main.DC failed")
     fprintf(stderr, "main.ParRun failed: retcode = %d\n", retCode);
     exit(1);
  }

  if(parp)  { free(parp);   parp = 0; }
  if(adcpp) { free(adcpp); adcpp = 0; }
  return 0;
}

int32		 CloseAdcView(ADC_VIEW_CNTL *adccntl);  
int32		 PartitionCube(ADC_VIEW_CNTL *avp);				
ADC_VIEW_CNTL *NewAdcViewCntl(ADC_VIEW_PARS *adcpp, uint32 pnum);
int32		 ComputeGivenGroupbys(ADC_VIEW_CNTL *adccntl);

int32 DC(ADC_VIEW_PARS *adcpp) {
   int32 itsk=0;
   double t_total=0.0;
   time_t tm;
   struct tm *tmp ;
   const int LL=400;
   char execdate[LL];

   typedef struct { 
      uint32 verificationFailed;
      uint32 totalViewTuples;
      uint64 totalViewSizesInBytes;
      uint32 totalNumberOfMadeViews;
      uint64 checksum;
      double tm_max;
   } PAR_VIEW_ST;
   
   PAR_VIEW_ST *pvstp;
   ADC_VIEW_CNTL *adccntlp;

   pvstp = (PAR_VIEW_ST*) malloc(sizeof(PAR_VIEW_ST));
   pvstp->verificationFailed = 0;
   pvstp->totalViewTuples = 0;
   pvstp->totalViewSizesInBytes = 0;
   pvstp->totalNumberOfMadeViews = 0;
   pvstp->checksum = 0;
   
   adccntlp = NewAdcViewCntl(adcpp, itsk);
   if (!adccntlp) { 
      PutErrMsg("ParRun.NewAdcViewCntl: returned NULL")
      return ADC_INTERNAL_ERROR;
   }else{
     if (adccntlp->retCode!=0) {
   	fprintf(stderr, 
   		 "DC.NewAdcViewCntl: return code = %d\n",
   						adccntlp->retCode); 
     }
   }
   if( PartitionCube(adccntlp) ) {
      PutErrMsg("DC.PartitionCube failed");
   }
   timer_clear(itsk);
   timer_start(itsk);
   if( ComputeGivenGroupbys(adccntlp) ) {
      PutErrMsg("DC.ComputeGivenGroupbys failed");
   }
   timer_stop(itsk);
   pvstp->tm_max = timer_read(itsk);
   pvstp->verificationFailed += adccntlp->verificationFailed;
   if (!adccntlp->verificationFailed) {
     pvstp->totalNumberOfMadeViews += adccntlp->numberOfMadeViews;
     pvstp->totalViewSizesInBytes += adccntlp->totalViewFileSize;
     pvstp->totalViewTuples += adccntlp->totalOfViewRows;
     pvstp->checksum += adccntlp->totchs[0];
   }   
   t_total=pvstp->tm_max; 
   if(CloseAdcView(adccntlp)) {
     PutErrMsg("ParRun.CloseAdcView: is failed");
     adccntlp->verificationFailed = 1;
   }

   pvstp->verificationFailed=Verify(pvstp->checksum,adcpp);
   if (pvstp->verificationFailed)
      fprintf(stderr, "Verification failed\n");

   time(&tm);
   tmp = localtime(&tm);
   strftime(execdate, (size_t)LL, "%d %b %Y", tmp);

   c_print_results((char *)"DC",
  		   adcpp->clss,
  		   (int)adcpp->nd,
  		   (int)adcpp->nm,
  		   (int)adcpp->nInputRecs,
                   pvstp->totalNumberOfMadeViews,
  		   t_total,
  		   (double) pvstp->totalViewTuples, 
  		   (char *)"Tuples generated", 
  		   (int)(pvstp->verificationFailed),
		   (long long int) pvstp->checksum,
		   (unsigned int) adcpp->nTasks,
  		   (char *) NPBVERSION,
  		   (char *) execdate,
  		   (char *) CC,
  		   (char *) CLINK,
  		   (char *) C_LIB,
  		   (char *) C_INC,
  		   (char *) CFLAGS,
  		   (char *) CLINKFLAGS); 
   return ADC_OK;
}

long long checksumS=464620213;
long long checksumWlo=434318;
long long checksumWhi=1401796;
long long checksumAlo=178042;
long long checksumAhi=7141688;
long long checksumBlo=700453;
long long checksumBhi=9348365;

int Verify(long long int checksum,ADC_VIEW_PARS *adcpp){
  switch(adcpp->clss){
    case 'S':
      if(checksum==checksumS) return 0;
      break;
    case 'W':
      if(checksum==checksumWlo+1000000*checksumWhi) return 0;
      break;
    case 'A':
      if(checksum==checksumAlo+1000000*checksumAhi) return 0;
      break;
    case 'B':
      if(checksum==checksumBlo+1000000*checksumBhi) return 0;
      break;
    default:
      return -1; /* CLASS U */
  }
  return 1;
}

