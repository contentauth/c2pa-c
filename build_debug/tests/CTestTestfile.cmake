# CMake generated Testfile for 
# Source directory: /Users/mathern/Desktop/git/@CAI/c2pa-c/tests
# Build directory: /Users/mathern/Desktop/git/@CAI/c2pa-c/build_debug/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(cpp_tests "/Users/mathern/Desktop/git/@CAI/c2pa-c/build_debug/tests/c2pa_c_tests")
set_tests_properties(cpp_tests PROPERTIES  ENVIRONMENT "LD_LIBRARY_PATH=/Users/mathern/Desktop/git/@CAI/c2pa-c/build_debug/tests:" WORKING_DIRECTORY "/Users/mathern/Desktop/git/@CAI/c2pa-c/build_debug/tests" _BACKTRACE_TRIPLES "/Users/mathern/Desktop/git/@CAI/c2pa-c/tests/CMakeLists.txt;94;add_test;/Users/mathern/Desktop/git/@CAI/c2pa-c/tests/CMakeLists.txt;0;")
add_test(c_test "/Users/mathern/Desktop/git/@CAI/c2pa-c/build_debug/tests/ctest")
set_tests_properties(c_test PROPERTIES  ENVIRONMENT "LD_LIBRARY_PATH=/Users/mathern/Desktop/git/@CAI/c2pa-c/build_debug/tests:" WORKING_DIRECTORY "/Users/mathern/Desktop/git/@CAI/c2pa-c" _BACKTRACE_TRIPLES "/Users/mathern/Desktop/git/@CAI/c2pa-c/tests/CMakeLists.txt;137;add_test;/Users/mathern/Desktop/git/@CAI/c2pa-c/tests/CMakeLists.txt;0;")
subdirs("../_deps/googletest-build")
