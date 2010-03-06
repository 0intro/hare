#!/bin/sh

# this file is used only to bootstrap mk onto a platform
# that currently lacks a binary for mk.  after that, mk can
# look after itself.

#	support@vitanuova.com

# change these defines as appropriate here or in mkconfig
# ROOT should be the root of the Inferno tree
ROOT=`pwd`

if [ `uname` = Darwin ]; then
	SYSHOST=MacOSX
else
	SYSHOST=`uname`
fi

if [ `uname -m` = x86_64 ]; then
	OBJTYPE=386
fi

if [ `uname -m` = i686 ]; then
	OBjTYPE=386
fi

if [ `uname -m` = ppc ]; then
	OBJTYPE=power
fi

if [ `uname -m` = ppc64 ]; then
	OBJTYPE=power 
fi

SYSTARG=$SYSHOST export SYSTARG

export SYSTARG
export SYSHOST
export ROOT
export OBJTYPE

rm -f mkconfig
echo ROOT=$ROOT > mkconfig
echo SYSHOST=$SYSHOST >> mkconfig
echo SYSTARG=$SYSTARG >> mkconfig
echo OBJTYPE=$OBJTYPE >> mkconfig
cat mkconfig.template >> mkconfig
echo 
echo Set PATH=$PATH:$ROOT/$SYSHOST/$OBJTYPE/bin export PATH
echo
