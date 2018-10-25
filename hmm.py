import sys
from distutils.core import setup, Extension
for p in sys.path:
    print(p)
setup(name='hmm', version='1.0', \
      ext_modules=[Extension('hmming', ['DTest.c'])])
print(sys.exec_prefix)
