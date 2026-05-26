#!/bin/bash

# this script runs tests on commodity servers (e.g., github CI).
# we exclude certain tests that do not run on commodity servers.
test_exclusion_pattern="dsa|cxl_dev"

cd build
make test ARGS="-E '$test_exclusion_pattern' --output-on-failure"
