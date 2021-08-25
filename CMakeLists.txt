#
# project: xjson
#
cmake_minimum_required(VERSION 2.8)
project(xjson)
# include the fips main cmake file
get_filename_component(FIPS_ROOT_DIR "../fips" ABSOLUTE)
include("${FIPS_ROOT_DIR}/cmake/fips.cmake")


fips_setup()

include_directories(.)

fips_begin_app(xjson_sample cmdline)
    fips_vs_warning_level(3)
    fips_files(sample.c)
fips_end_app()

fips_finish()


