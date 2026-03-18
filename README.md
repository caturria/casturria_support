# Casturria_support

## Purpose

This is a supporting library for use by the Casturria playout system. At the time of this writing, Casturria is a project still in its infancy which aims to become a powerful internet radio automation suite.

This component provides a high-level interface to various facilities, including those provided by FFmpeg's Libav* libraries, to be used within Casturria via Deno's foreign function interface (FFI).

# Note

Please be advised that this project is primarily being maintained by a totally blind individual. Though I do my best to avoid code formatting inconsistencies using automated tools, such tools are not perfect, and formatting errors are not at all obvious to someone using a screen reader without substantial effort. Please expect to see some degree of less-than-ideal formatting at least for the time being.


# Building

It is expected that this library will be built most often using Github CI. Casturria will simply fetch it as a prebuilt artifact.

If you need to build Casturria_support for development, it is recommended to use a dedicated VM as the build process currently makes some system-wide changes. Specifically, it will change the system's default compiler to a recent stable release of Clang.

On a fresh Debian or Ubuntu system, do the following:
```Bash
git clone https://github.com/caturria/casturria_support
cd casturria_support
#Build dependencies:
./bootstrap.sh
#Point CMake at the location of the built dependencies:
export CMAKE_PREFIX_PATH=`pwd`/build/output
cmake .
make
```

# License

    Copyright (C) 2026  Jordan Verner and contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

