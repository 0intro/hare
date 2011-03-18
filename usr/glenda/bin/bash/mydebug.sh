#!/usr/bin/perl

#
# mydebug:
#
# Given a Cobalt job ID, provide a limited interactive interface
# to mmcs_db_server that allows running JTAG debug commands
# against that job's block.  This allows a subset of the functionality
# provided by mmcs_db_console
#
# Usage: jtagdebug <cobalt_job_id>
#
# Author: Andrew Cherry - 3/21/2007
# Based on sample script from IBM's BG/L System Administration Redbook
#

use strict;
use English;
use IO::Socket;
use Cwd;
use File::Basename;
use POSIX;
use Term::ReadLine;

my $debug = 0;

# Hash of permitted (JTAG) commands with usage summary
#
my %commands = (
		"boot" => "[<target>] boot <sram-program>  [no_broadcast] [ V | C ]",
		"blcinitchip" => "[<target>] blcinitchip [options]",
		"load_microloader" => "[<target>] load_microloader <sram-program>  [no_broadcast]",
		"load" => "[<target>] load <dram-program> [no_broadcast] [verify]",
		"start" => "[<target>] start",
		"step_core" => "[<target>] step_core",
		"start_cores" => "[<target>] start_cores",
		"halt_cores" => "[<target>] halt_cores",
		"read_dcr" => "[<target>] read_dcr <dcr-name-or-hex-number>",
		"write_dcr" => "[<target>] write_dcr <num> <val>",
		"tree_status" => "[<target>] tree_status",
		"stop_clocks" => "[<target>] stop_clocks",
		"read_idcode" => "[<target>] read_idcode",
		"read_ecid" => "[<target>] read_ecid",
		"dump_personalities" => "[<target>] dump_personalities",
		"dump_proctable" => "[<target>] dump_proctable",
		"dump_serialnumbers" => "[<target>] dump_serialnumbers",
		"read_ctrl" => "[<target>] read_ctrl",
		"write_ctrl" => "[<target>] write_ctrl <hex_value> <hex_mask>",
		"clear_ctrl" => "[<target>] clear_ctrl <bit_num>",
		"set_ctrl" => "[<target>] set_ctrl <bit_num>",
		"sysrq" => "[<target>] sysrq [options]",
		"write_con" => "[<target>] write_con <console-command>",
		"locate" => "[<target>] locate [neighbors] [ras_format] [get_portinfo]",
		"dump_memory" => "[<target>] dump_memory <addr> <size>",
		"dump_global_int" => "[<target>] dump_global_int",
		"dump_gprs" => "[<target>] dump_gprs",
		"dump_iar" => "[<target>] dump_iar",
		"dump_dcache" => "[<target>] dump_dcache",
		"dump_stacks" => "[<target>] dump_stacks",
		"dump_sprs" => "[<target>] dump_sprs",
		"read_kernel_status" => "[<target>] read_kernel_status",
		"status" => "[<target>] status",
		"read_scanring" => "[<target>] read_scanring  <scanring-name-or-hex-number>",
		"write_scanring" => "[<target>] write_scanring <scanring-name-or-hex-number> <scanring_data>",
		"flush_ring" => "[<target>] flush_ring <scanring-name-or-hex-number> <value>",
		"pulse_ring" => "[<target>] pulse_ring <scanring-name-or-hex-number>",
		"read_scom" => "[<target>] read_scom <scom-name-or-hex-number>",
		"redirect" => "[<target>] redirect <block-id> [on | off]",
		"write_scom" => "[<target>] write_scom <hex-addr> [<hex-byte>]...",
		"scom_reset" => "[<target>] scom_reset",
		"read_mem" => "[<target>] read_mem <hex-addr> <count>",
		"write_mem" => "[<target>] write_mem <hex-addr> [<hex-byte>]...",
	       );

# Most service node accounts are different from the ID used to run the job.
# This provides a mapping from service node usernames to job usernames
my %sn1_idmap = ();

# Connection info for mmcs_db_server socket interface
my $host = "localhost";
my $servicenode = "sn1";
my $port = "32031";
my $username= getpwuid($UID);
$username = $sn1_idmap{$username} if($sn1_idmap{$username});
my $replyformat = 0;

#
# Start of mainline
#
my $prog = basename($0);

if($#ARGV < 0)
  {
    print "Usage: $prog <cobalt_jobid>\n";
    exit 1;
  }

my $jobid = shift(@ARGV);

# Check to make sure we're on the service node (our mmcs_db_server listens
# on the loopback interface only)
my ($sysname, $nodename, $release, $version, $machine) = uname();
if($nodename ne $servicenode) {
  #print "This script must be run on $servicenode\n";
  #print "Exiting...\n";
  #exit 1;
  print "Running jtagdebug on $servicenode via ssh\n" if($debug);
  my $cmd = "/usr/bin/ssh -t $servicenode /home/ericvh/bin/mydebug $jobid";
  print "$cmd" if($debug);
  exec $cmd;
}

print "Looking up job id $jobid\n";
my $block = get_job_block($jobid,$username);
if($block == -1) {
  print "Could not find any running job for $username with job ID $jobid.  Exiting.\n";
  exit 1;
}

print "Connecting to $host:$port\n";
my $cmd;
my @results;
my $x;
my $remote = IO::Socket::INET->new( Proto => 'tcp',
				    PeerAddr => $host,
				    PeerPort => $port,
				  );
unless ($remote) { die "Could not connect to mmcs_db_server: $!" };
print "Connected to mmcs_db_server\n";
@results = mmcs_cmd($remote, "set_username $username\n");
check_mmcs_result($results[0]) or die "Error running set_username: $results[1]\n";
if($debug) { foreach $x(@results) { print $x,"\n" } }
@results = mmcs_cmd($remote, "replyformat 1\n");
check_mmcs_result($results[0]) or die "Error setting replyformat: $results[1]\n";
if($debug) { foreach $x(@results) { print $x,"\n" } }
$replyformat = 1;
@results = mmcs_cmd($remote, "select_block $block\n");
check_mmcs_result($results[0]) or die "Error running select_block $block: $results[1]\n";
if($debug) { foreach $x(@results) { print $x,"\n" } }

my $term = new Term::ReadLine 'mydebug';
my $OUT = $term->OUT || \*STDOUT;
while ( defined ($_ = $term->readline("mydebug[$block]>")) ) {
  my $cmd = $_;
  my $target;
  my $cmdname;
  my @cmdargs;
  
  # Remove leading whitespace
  chomp($cmd);
  $cmd =~ s/^\s+//;
  if($cmd =~ /^{[^}]*}(\.[0-9])?\s/) {
    ($target,$cmdname,@cmdargs) = split(/\s+/,$cmd);
  }
  else {
    ($cmdname,@cmdargs) = split(/\s+/,$cmd);
  }
  if($cmdname eq 'exit' || $cmdname eq 'quit') {
    last;
  }
  elsif($cmdname eq 'help') {
    print $OUT "\nValid JTAG commmands:\n\n";
    foreach my $i (sort keys %commands) {
      print $OUT "$commands{$i}\n";
    }
  }
  elsif($commands{$cmdname}) {
    @results = mmcs_cmd($remote, "$cmd\n");
    foreach $x(@results) { print $OUT $x,"\n" };
  }
  elsif($cmdname eq 'wait') {
    @results = mmcs_read($remote);
    foreach $x(@results) { print $OUT $x,"\n" };
  }
  else {
    print $OUT "Sorry, you are not allowed to run $cmdname\n";
    print $OUT "Type \"help\" to get a list of supported JTAG commands\n";
  }
  $term->addhistory($_) if /\S/;
}

print "Closing connection\n";
$remote->shutdown(2);
close($remote);

print "\n**Please remember to cqdel your job in order to free the block!**\n\n";
exit 0; # made it this far, must be successful.

# Subroutines

################################################################
# get_job_block(cobalt_jobid)
#
# Returns the block a given Cobalt job is running on, or -1 if
# it is not found
#

sub get_job_block ($$)
  {
    my $jobid = shift(@_);
    my $username = shift(@_);
    my $return = -1;
    open(CQSTAT, "/usr/bin/cqstat |") or die "Failed to run cqstat: $!\n";
    while(<CQSTAT>) {
      print if($debug);
      chomp;
      if(/running/) {
	my ($id,$user,$walltime,$nodes,$state,$block) = split(/\s+/);
	print "Check $id for $jobid\n" if($debug);
	if($id eq $jobid) {
	  print "Found job $id, owner $user, block $block\n";
	  if($user ne $username) {
	    print "Sorry $username, you do not own job $id.  Permission denied.\n";
	    $return = -1;
	  }
	  else {
	    $return = $block;
	  }
	  last;
	}
      }
    }
    close(CQSTAT);
    return($return);
  }

################################################################
# check_mmcs_result(result)
#
# check_mmcs_result -- check for success of an MMCS DB command
# inputs:
# param0 -- first line of command output
# outputs:
# return 1 if success, 0 if fail
 sub check_mmcs_result($)
    {
      my $result = shift(@_);
      if($result =~ /^OK/) {
	return 1;
      }
      else {
	return 0;
      }
    }


################################################################
# mmcs_cmd(szSocket, szCmd)
#
# mmcs_cmd -- send an mmcs command and gather and check the response.
# inputs:
# param0 -- remote tcp port to send command to.
# param1 -- command string
# outputs:
# return values in a list.
#
sub mmcs_cmd ($$)
  {
  my $szSocket = @_[0];
  my $szCmd = @_[1]; # pick off the command parameter.
  my $szReturn;
  my @listReturn;
  print $szCmd if($debug); # echo the command
  print $szSocket $szCmd; # execute the command.
  if ($replyformat == 0) # reply format 0
    {
	$szReturn = <$szSocket>; # read the result...
	chomp($szReturn); # get rid of lf at end.
	@listReturn = split(/;/,$szReturn);
      }
    else # reply format 1
      {
	while (1)
	  {
	    $szReturn = <$szSocket>; # read the result...
	    last if ($szReturn eq "\0\n");
	    chomp $szReturn;
	    $listReturn[++$#listReturn] = $szReturn;
	  }
      }
    return @listReturn;
  }

# evh
# mmcs_read(szSocket)
# read subsequent responses
sub mmcs_read ($$)
  {
  my $szSocket = @_[0];
  my $szReturn;
  my @listReturn;
  if ($replyformat == 0) # reply format 0
    {
	$szReturn = <$szSocket>; # read the result...
	chomp($szReturn); # get rid of lf at end.
	@listReturn = split(/;/,$szReturn);
      }
    else # reply format 1
      {
	while (1)
	  {
	    $szReturn = <$szSocket>; # read the result...
	    last if ($szReturn eq "\0\n");
	    chomp $szReturn;
	    $listReturn[++$#listReturn] = $szReturn;
	  }
      }
    return @listReturn;
  }
