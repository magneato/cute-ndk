#!/bin/sh

g++ $1 -O2 -g -o ${1%.cpp} -I../ -L../bin -lnetdkit -lpthread -lrt -DNDK_RTLOG
#g++ $1 -O2 -o ${1%.cpp} -I../ -L../bin -lnetdkit -lpthread -lrt
