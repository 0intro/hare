#!/usr/bin/env perl

## FTQ experiment generation

print "=================================\n";
print "Fixed Time Quantum Microbenchmark\n";
print "=================================\n\n";

print "Experiment name (eg: Argonne BGL): ";
chomp($exper = <>);

print "Hostname (empty for hostname -s output): ";
chomp($hostname = <>);
if (length($hostname) == 0) {
    chomp($hostname = `hostname -s`);
}

print "Clock speed in MHz (eg: 2.800): ";
chomp($clockrate = <>);

print "Core count: ";
chomp($ncores = <>);

print "Number of FTQ samples: ";
chomp($nsamp = <>);

print "\n";
print " EXPERIMENT NAME=$exper\n";
print " HOSTNAME=$hostname\n";
print " SPEED=$clockrate MHz\n";
print " CORES=$ncores\n";
print " NUM SAMPLES=$nsamp\n\n";
print "CORRECT? (Y/N) : ";
chomp($response = <>);

if ($response ne "Y" && $response ne "y") {
  print "Please re-run then.\n";
  exit(1);
}

@timeData = localtime(time);

$year = $timeData[5]+1900;

$dirname = $hostname."_".$timeData[3]."_".$timeData[4]."_".$year;

if (-e $dirname.".tar.gz") {
  print "\n\nERROR:\nExperiment already exists.  Please remove or move file called\n";
  print "$dirname.tar.gz out of the way and re-run.\n\n";
  exit(1);
}

print "Performing experiment in directory \"$dirname\".\n";

mkdir($dirname) || die "Unable to create experiment directory."; 

chdir($dirname) || die "Unable to change to experiment directory.";

system("uname -a > uname.out");
system("hostname > hostname.out");
system("date > date.out");
system("ps -ax > allprocesses.out");
open(OUTFILE,"> params.out");
print OUTFILE "EXPERIMENT: $exper\n";
print OUTFILE "CLOCKRATE: $clockrate\n";
print OUTFILE "CORES: $ncores\n";
print OUTFILE "SAMPLES: $nsamp\n";
close(OUTFILE);

print "Running single threaded FTQ...";

system("../ftq63 -n $nsamp -o singlethread");

print "done.\n";

print "Running multithreaded FTQ...";

system("../t_ftq63 -t $ncores -n $nsamp -o multithreaded");

print "done.\n";

chdir("..");

system("tar cf $dirname.tar $dirname");
system("gzip $dirname.tar");
system("rm -Rf $dirname");

print "\nAll done.\n";
print "Experiment data is in $dirname.tar.gz\n\n";
