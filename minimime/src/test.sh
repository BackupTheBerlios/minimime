#!/bin/sh
# MiniMIME test cases

[ ! -x ./minimime ] && {
	echo "You need to compile MiniMIME first to accomplish tests"
	exit 1
}

LD_LIBRARY_PATH=.
export LD_LIBRARY_PATH

for f in test/test*.txt; do
	echo -n "Running test for $f (file)... "
	output=`./minimime $f 2>&1`
	[ $? != 0 ] && {
		echo "FAILED ($output)"
	} || {
		echo "PASSED"
	}
	echo -n "Running test for $f (memory)... "
	output=`./minimime -m $f 2>&1`
	[ $? != 0 ] && {
		echo "FAILED ($output)"
	} || {
		echo "PASSED"
	}	
done

unset LD_LIBRARY_PATH
