#!/usr/bin/python
"""

"""

import sys
import os



#DCMPTCP
tm="ONE_MAP_ONE"


for fn in [2]:
  for sfs in [10000]:
    work_dir="results/"+str(fn)+'-'+str(sfs)
    if not os.path.exists(work_dir):
      os.makedirs(work_dir)
    cmd="./waf --run \"scratch/letflow-debug --tm="+tm+" --fn="+str(fn)+" --hn="+str(fn)+" --sfs="+str(sfs)\
          +"\" --cwd "+str(work_dir)
    os.system(cmd)



  





