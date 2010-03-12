#!/bin/bash

# This script should be executed from the directory where this file is found.
BrasilPath="../../../../../../../brasil/"

cd $BrasilPath
./Linux/386/bin/brasil server -d 'tcp!*!5544'
