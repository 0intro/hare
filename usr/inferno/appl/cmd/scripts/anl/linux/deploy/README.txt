This README explains how to setup HARE binaries, submit jobs to brasil 
and how to start brasil server on Linux Systems.  

#############################################################

Setting up location ( HARE_LOCATION )

By default, HARE binaries will be deployed to the location
"/bgsys/argonne-utils/profiles/plan9/LATEST/bin/"
This path can be overridden by two ways.

1. Export HARE_PATH environment variable.  If this variable is present,
      it will override the default path.  This variable can be exported as follows
	  $ export HARE_LOCATION=/path/to/hare/
	  Above command should be added to startup script (ie. ~/.bashrc) so that this
	  variable will be always available.

2. use "-l /path/to/hare/" command line option with all commands bellow.
	  This option overrides both default location and environment variable value.

#############################################################

Setting up HARE 
Script "installHelp.sh" will copy the binaries/scripts in proper location,
so that executions of subsequent deployBrasil.sh and runJob.py are
location independent.  This script should be executed only once, and
can be considered as equivalent of "make install"

installHelp.sh : Script to help installation of  hare binaries to proper location.
   The proper location depends on "-l" cmd option, HARE_LOCATION environement variable
   or default path "/bgsys/argonne-utils/profiles/plan9/LATEST/bin/"
 
USAGE: 
./installHelp.sh [-l hare_location]

WARNING : 
	This script *MUST* be executed from the same directory where is is found,
	as it depends on "pwd" for locating other files.
 
ARGUMENTS :
   -l: path to the location where hare binaries will be installed.

ENVIRONMENT VARIABLES: (Can be used to override defaults in the place of cmdline)
  HARE_LOCATION: path to the location where hare binaries will be installed. 

#############################################################

Starting Brasil
Script "deployBrasil.sh" will start the brasil instance.  This instance
is started in foreground.

deployBrasil.sh : Script to launch the brasil instance

USAGE: 
deployBrasil.sh [-p brasil_port] [-l hare_location]

ARGUMENTS :
   -p: port running brasil daemon (should be unique per user)
   -l: path to the location of hare installation

ENVIRONMENT VARIABLES: (Can be used to override defaults in the place of cmdline)
  HARE_LOCATION: Location of hare installation
  BRASIL_PORT - port running brasil daemon (should be unique)
 
By default, Inferno root filesystem will be exported on port 5670.
User can give different port by passing command line option -p.

EXAMPLES
$ ./deployBrasil.sh 

WARNING: using default port (5670)
Please set using BRASIL_PORT environment variable or -p command line argument

$ deployBrasil.sh -p 4433

$ deployBrasil.sh -l /path/to/hare/installation/bin/ -p 4433

#############################################################

Submitting jobs to Brasil.
Jobs can be submitted to brasil server using "runJob.py" script.

USAGE: runJob.py [-l hare_location] [-9 py9p_location]  
                [-h brasil_host] [-p brasil_port] 
                [-n numnodes] [-o os_type] [-a architecture_type]
                "cmd arg1 arg2 ..." </path/to/inputfile>
 
ARGUMENTS:
    -l: hare_location default value ["/bgsys/argonne-utils/profiles/plan9/LATEST/bin/"]
    -9: py9p_loation (specify only if py9p is installed separately)
    -D: debug mode
    -h: host running brasil daemon (defaults to current host)
    -p: port running brasil daemon (default: 5670)
    -n: number of cpu nodes to provision (mandetory)
    -o: OS type (default value "any") (supported values: Linux, Plan9, MacOSX, Nt)
    -a: arch type (default value "any") (supported values: 386, arm, power, spim)
    "cmd arg1 arg2 ...": command to run on I/O nodes (eventually compute nodes maybe)
        (WARN: if commmand has arguments, make sure to include it in double quotes)
   <path/to/inputfile> If present, this file is used as STDIN

ENVIRONMENT VARIABLES: (can be used to override defaults in place of cmdline)
	HARE_LOCATION - Place where hare executables are installed
	BRASIL_HOST - host running brasil daemon
	BRASIL_PORT - port running brasil daemon

EXAMPLES
    runJob.py -n 4 date
    runJob.py -l /path/to/hare/binaries/ -n 8 -o Linux -a 386 "wc -l" /path/to/input/file
    runJob.py -9 /path/to/py9p/ -n 3 -o Linux date

The script is quite simple and relatively small. You can do many more things
by directly modifying the code.  Feel free to look into code.

#############################################################

***** Assumptions *****
Following are the assumptions for proper working of these scripts.
1. Presence of compiled brasil is assumed, The script 
	"deployBrasil.sh" only launches it.

2. User should make sure that either he is having write permission to 
	default location "/bgsys/argonne-utils/profiles/plan9/LATEST/bin/"
	OR should set "HARE_LOCATION" environement variable
	OR should provide location with "-l /path/to/hare/installation/bin/"

3. Only the script "installHelp.sh" must be executed from the directory where
	it is found.  It will not work if executed from other locations.

2. Presence of Python is assumed.  Script runJob.py needs it.

4. Presence of bash shell is desired.  These scripts are tested on
	bash shell only.  these should work on other shells but it is
	not assured.
