#
# Copyright (c) 2020 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
#
# Distributed under the MIT License, Version 1.0. (See accompanying
# file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/guoh27/jay
#

add_executable(simple_example main.cpp)
target_link_libraries(simple_example jay::jay)
