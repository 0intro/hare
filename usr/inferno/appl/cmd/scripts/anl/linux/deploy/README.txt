This README explains how to submit jobs to brasil and how to start
brasil server on Linux Systems.  

#############################################################

Submitting jobs to Brasil.
Jobs can be submitted to brasil server using "runJob.py" script.

Usage: ./runJob.py <server-ip> <port> <resources> <command> <inputFile>
Example: ./runJob.py localhost 5544 0 "ls -l"
Example: ./runJob.py localhost 5544 "3 Linux 386" "wc -l" ./runJob.py

<server-ip> <port> : Here, server-ip and port are the TCP address of 
the brasil server.

<resources> : This parameter tells how many instances of specified command
to execute. "0" signifies local execution.  Requested executions are
then distributed between available children.  One can also add constraints
for OS and platform in addition to number of resources.
Remember that resources parameter is single string, if you plan to
specify OS and platform constraints, then sround them in double quotes.
One can use "*" as wildchar for platform if needed.

<command> : Forth parameter is command to be executed.  If your command has
its own parameter, then include the entire command in double quotes.

<inputFile> : This last parameter is optional.  If your command expects
standard input then provide the filename here.  This file will be fed 
as input to the <command>.

The script is quite simple and relatively small. You can do many more things
by directly modifying the code.  Feel free to look into code.

#############################################################

Starting Brasil
Script "deployBrasil.sh" will start the brasil instance.  This instance
is started in forground. Following command should work in this location.

$ ./deployBrasil.sh
using local port 5544 for listening

This script runs brasil instance in forgroud, so you may not be
able to use shell afterwards.  You can avoid this by starting
this script to run in background.

$ ./deployBrasil.sh &

By default, Inferno root filesystem will be exported on port 5544.
User can give different port by passing command line parameter

$ ./deployBrasil.sh 5566
using local port 5566 for listening

#############################################################

***** Asumptions *****
Following are the assumptions for proper working of these scripts.
1. Presence of compiled brasil is assumed, The script 
	"deployBrasil.sh" only launches it.

2. Presence of Python is assumed.  Script runJob.py needs it.

3. All these scripts are to be executed from the directory where they
	are found.  They will not work if executed from other locations.

3. Presence of bash shell is desired.  These scripts are tested on
	bash shell only.  these should work on other shells but it is
	not asured.
