#!/bin/sh

g++ $1 -g -o ${1%.cpp} -I../ -L../bin -lnetdkit -lpthread -lrt -DNDK_RTLOG -DMULTI_THREAD
#g++ $1 -O2 -o ${1%.cpp} -I../ -L../bin -lnetdkit -lpthread -lrt
