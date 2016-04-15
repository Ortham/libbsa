/*  libbsa

    A library for reading and writing BSA files.

    Copyright (C) 2012-2013    WrinklyNinja

    This file is part of libbsa.

    libbsa is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    libbsa is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libbsa.  If not, see
    <http://www.gnu.org/licenses/>.
*/

#include "libbsa/libbsa.h"

#include <stdint.h>
#include <iostream>

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

TEST(bsa_open, shouldFailIfNullHandlePointerIsGiven) {
    FAIL();
}

TEST(bsa_open, shouldFailIfNullPathIsGiven) {
    FAIL();
}

TEST(bsa_open, shouldFailIfNonExistentPathIsGiven) {
    FAIL();
}

TEST(bsa_open, shouldSucceedIfValidPathIsGiven) {
    FAIL();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
