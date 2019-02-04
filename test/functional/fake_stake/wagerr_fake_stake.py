#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os
import sys
sys.path.append(os.path.abspath(__file__))
sys.path.append(os.path.abspath(".."))

from test01 import Test_01
from test02 import Test_02
from test03 import Test_03

if __name__ == '__main__':
    #Test_01().main()
    #Test_02().main()
    Test_03().main()
