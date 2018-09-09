cmake_minimum_required(VERSION 3.12)

aux_source_directory(. DIR_TEST_SUB_SRCS)
add_library(test_sub ${DIR_TEST_SUB_SRCS})