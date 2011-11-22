#!/bin/sh

EXE_LOCATION="../bin/Debug"
TEST_BASE_DIR="."

VALIDATE_TEST_CASES=0

VALGRIND_MODE=0
TRACK_ORIGINS=1
LEAK_CHECK=1

PREPEND=''
APPEND=''
if [ $VALIDATE_TEST_CASES -ne 0 ] 
then
    TESTCASE_FILE="revalidate.txt"
else
    TESTCASE_FILE='testcases.txt'
fi


setVars()
{
    if [ $VALIDATE_TEST_CASES -ne 0 ] 
    then
        APPEND=" > $TEST_BASE_DIR/$TEST/expected 2>&1"
    else
        if [ $VALGRIND_MODE -ne 0 ]
        then
            PREPEND='valgrind -q'
            if [ $TRACK_ORIGINS -ne 0 ]
            then
                PREPEND="$PREPEND --track-origins=yes"
            fi
            if [ $LEAK_CHECK -ne 0 ]
            then
                PREPEND="$PREPEND --leak-check=yes"
            fi
            APPEND='2>&1 | grep "=="'
        else
            APPEND="2>&1 | diff -u $TEST_BASE_DIR/$TEST/expected -"
        fi
    fi
}


while read TEST
do
    echo "========"
    echo "Testing: $TEST"
    echo "========"
    setVars
    COMMAND="$EXE_LOCATION/eplc $TEST_BASE_DIR/$TEST/test.epl"
    eval "$PREPEND $COMMAND $APPEND"
done < $TESTCASE_FILE


