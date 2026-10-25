#!/bin/sh
# Host checks. No Kinect, no Visual Studio, no AKDEPS.
set -eu
cd "$(dirname "$0")/.."
g++ -std=c++17 -Wall -Wextra -Werror -I src/feed -o /tmp/ak_frame_orient_test tests/frame_orient_test.cpp
g++ -std=c++17 -Wall -Wextra -Werror -I src/feed -o /tmp/ak_shutdown_order_test tests/shutdown_order_test.cpp
/tmp/ak_frame_orient_test
/tmp/ak_shutdown_order_test
python3 tests/check_release.py
