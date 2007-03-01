#!/bin/sh
#feel free to clean this up ive made no attempt to write this overly portable ...

datadir="./data"

logfile="$datadir/seek.regression"
reffile="$1"

list=`ls data/a-* data/b-* | sort`
rm $logfile
for i in $list ; do
    echo ---------------- >>$logfile
    echo $i >>$logfile
    ./seek_test $i >> $logfile
done

if diff -u "$logfile" "$reffile" ; then
    echo
    echo Regression test succeeded.
    exit 0
else
    echo
    echo Regression test: Error.
    exit 1
fi
