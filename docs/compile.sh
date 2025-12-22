#!/usr/bin/env bash
set -e

#look for all .txt and .md files in /home/randy/._____RANDY_____/REPOS/aria/docs
#ignore tests and status updates but include everything else in the directory and subdirectories
#for any files that only have a .md version, make a .txt version as gemini cannot read .md I upload
#compile all files in a rational order and store a file name aria_full.txt
#make sure this script can self discover any future additions whenver i run it. 
