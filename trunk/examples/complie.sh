#!/bin/sh

g++ $1 -O2 -o ${1%.cpp} -I../ -L../bin -lnetdkit -lpthread -lrt
#g++ $1 -O2 -o ${1%.cpp} -I../ -L../bin -lnetdkit -lpthread -lrt
