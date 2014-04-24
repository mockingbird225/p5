import sys
import os

for x in range(1,11):
	cmd = "python testcases/test%s.py"%(x);#(sys.argv[1]);
	os.system(cmd);
