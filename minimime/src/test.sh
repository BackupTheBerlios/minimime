#!/bin/sh
# MiniMIME test cases

[ ! -x ./minimime ] && {
	echo "You need to compile MiniMIME first to accomplish tests"
	exit 1
}

LD_LIBRARY_PATH=.
export LD_LIBRARY_PATH

DIRECTORY=${1:-test}
FILES=${2:-"*"}

TESTS=0
F_ERRORS=0
F_INVALID=""
M_ERRORS=0
M_INVALID=""
for f in ${DIRECTORY}/${FILES}; do
	if [ -f "${f}" ]; then
		TESTS=$((TESTS + 1))
		echo -n "Running test for $f (file)... "
		output=`./minimime $f 2>&1`
		[ $? != 0 ] && {
			echo "FAILED ($output)"
			F_ERRORS=$((F_ERRORS + 1))
			F_INVALID="${F_INVALID} ${f} "
		} || {
			echo "PASSED"
		}
		echo -n "Running test for $f (memory)... "
		output=`./minimime -m $f 2>&1`
		[ $? != 0 ] && {
			echo "FAILED ($output)"
			M_ERRORS=$((M_ERRORS + 1))
			M_INVALID="${M_INVALID} ${f} "
		} || {
			echo "PASSED"
		}
	fi
done

echo "Ran a total of ${TESTS} tests"

if [ ${F_ERRORS} -gt 0 ]; then
	echo "!! ${F_ERRORS} messages had errors in file based parsing"
	echo "-> ${F_INVALID}"
fi	
if [ ${M_ERRORS} -gt 0 ]; then
	echo "!! ${F_ERRORS} messages had errors in memory based parsing"
fi	

unset LD_LIBRARY_PATH
