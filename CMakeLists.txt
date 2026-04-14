
#CVC support layer
#CMake build script
#Copyright (C) 2026  Jordan Verner and contributors

#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU Affero General Public License as published by
#the Free Software Foundation, either version 3 of the License, or
#(at your option) any later version.

#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU Affero General Public License for more details.

#You should have received a copy of the GNU Affero General Public License
#along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.5)
project(support)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(DEP_INCLUDE_DIR $ENV{CMAKE_PREFIX_PATH}/include)

find_library(OGG ogg REQUIRED)
find_library(VORBIS vorbis REQUIRED)
find_library(VORBISENC vorbisenc REQUIRED)
find_library(OPUS opus REQUIRED)
find_library(LAME mp3lame REQUIRED)
find_library(CRYPTO crypto REQUIRED)
find_library(SSL ssl required)

find_library(TLS tls required)
find_library(AVFORMAT avformat REQUIRED)
find_library(AVCODEC avcodec REQUIRED)
find_library(AVFILTER avfilter REQUIRED)
find_library(AVUTIL avutil REQUIRED)
find_library(SWRESAMPLE swresample REQUIRED)
find_library(SWSCALE swscale REQUIRED)
find_library(Z z REQUIRED)

add_library(casturria_support SHARED
src/decoder.cpp
src/encoder.cpp
            src/events.cpp
           )
target_link_options(casturria_support PRIVATE
                    -fuse-ld=lld
                             -Wl,--version-script=src/libcasturria_support.map
                             -Wl,--no-undefined
                                     -static-libstdc++
                                     )
                                     target_link_libraries(casturria_support PRIVATE
                                     ${SWSCALE}
${AVFORMAT}
${AVCODEC}
${SWRESAMPLE}
${AVUTIL}
${AVFILTER}
${VORBIS}
${VORBISENC}
${OPUS}
${OGG}
${LAME}
${CRYPTO}
${TLS}
${SSL}
${Z}

)
target_include_directories(casturria_support PRIVATE
                           ${DEP_INCLUDE_DIR})
install(TARGETS casturria_support)
