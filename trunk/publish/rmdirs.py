
import os
import sys
def rmdirs(dir):
    if len(dir) is 0:
        return
    for d in dir:
        for r, ds, fs in os.walk(d):
            if len(fs) == 0 and len(ds) == 0:
                os.rmdir(r)
        else:
            rmdirs(ds)

rmdirs([sys.argv[1]])
