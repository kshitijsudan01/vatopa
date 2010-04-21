#!/usr/bin/env python


import os, array, sys, struct, re

trace_filename = int(sys.argv[1])
print "Opening trace_file = %s" % trace_filename

trace_fp = open(trace_filename,'r+')
trace_fp.seek(0)

def main():
    for line in trace_fp : 

        if(line == '#eof') : # we're done here
            trace_fp.close()
            print "Done processing this file."
            exit

        m = re.match(r"(\w+) (\w+) (\d) (\d)",line)
        ip, ea, size, rw = m.groups()
        
        if(size == 0) :
            






if __name__ == "__main__":
    main()

