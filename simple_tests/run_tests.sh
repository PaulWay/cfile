#!/bin/bash

tests=(*.test)
num_tests=${#tests[@]}

#echo "tests = ${tests[@]}."
#echo "num_tests = $num_tests"

echo "1..$num_tests"
test_num=0
for test_prog in ${tests[@]}; do
	let test_num++
	if sh $test_prog "$@"; then
		echo "$test_num ok $test_prog $@"
	else
		echo "$test_num not ok $test_prog $@"
	fi
done
