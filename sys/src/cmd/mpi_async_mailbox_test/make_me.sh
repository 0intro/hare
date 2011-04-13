#!/bin/bash
MPICXX=mpig++

# Only Boost header files are needed, tested on Boost 1.33 and 1.45
BOOST_INCL=/usr/include
APGAF_INCL=$PWD/include

$MPICXX -O3 test_mpi_mailbox_unbuffered.cpp -o test_mpi_mailbox_unbuffered -I$BOOST_INCL -I$APGAF_INCL

