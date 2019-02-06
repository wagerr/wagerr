#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os
import sys
sys.path.append(os.path.abspath(__file__))
sys.path.append(os.path.abspath(".."))

from test01 import Test_01
from test02 import Test_02
from test03 import Test_03
from test05 import Test_05

total_num_of_tests = 5

def syntax_error(err_msg=None):
    if err_msg is not None:
        print("\n ** %s" % str(err_msg))
    print("\nUsage:")
    print("python3 %s --n\t: runs test number n (1 to %d)" % (__file__, total_num_of_tests))
    print("\nExample:")
    print("python3 %s --3\t: runs test number 3\n" % __file__)


def get_test():
    if len(sys.argv) != 2 or len(sys.argv[1]) < 3 or sys.argv[1][:2] != "--":
        syntax_error("No \"--\" prefix")
        return 0
    try:
        choice = int(sys.argv[1][2:])
    except ValueError:
        syntax_error("%s is not a number" % str(sys.argv[1][2:]))
        return 0

    if choice < 1 or choice > total_num_of_tests:
        syntax_error("No test for number %d" % choice)
        return 0

    return choice

switcher = {
    1: Test_01(),
    2: Test_02(),
    3: Test_03(),
    5: Test_05()
}

def run_test(choice):
    # clear arguments for test_fw parser
    sys.argv = sys.argv[:1]
    # get test
    test = switcher.get(choice, None)
    # run test
    if test is not None:
        test.main()


if __name__ == '__main__':
    # select test number
    choice = get_test()
    # run corresponding test
    run_test(choice)

