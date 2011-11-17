#!/bin/bash

# Testing with no parameters

echo "--------------------------";
echo "Call EPLC without parameters: ";
echo "--------------------------";

../bin/Debug/eplc 

echo "--------------------------";
echo "Call EPLC with nonexistent file: ";
echo "--------------------------";

../bin/Debug/eplc notexists.epl

echo "--------------------------";
echo "Call EPLC with correct file: ";
echo "--------------------------";

../bin/Debug/eplc test.epl






