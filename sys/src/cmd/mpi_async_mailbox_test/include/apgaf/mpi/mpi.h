#ifndef _APGAF_MPI_H_
#define _APGAF_MPI_H_


#include<mpi.h>
#define CHK_MPI(a) { if (a != MPI_SUCCESS) {\
                      char* error_string = NULL; \
                      int len = 0; \
                      MPI_Error_string(a, error_string, &len); \
                      std::cerr << __FILE__ << ", line " << __LINE__  \
                           <<" MPI ERROR = " << error_string << std::endl; \
                           exit(-1); \
                     } }

#endif //_APGAF_MPI_H

