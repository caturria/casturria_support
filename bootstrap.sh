#!/bin/bash
#CVC support layer
#Installation bootstrap script
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

set -eu

#Dependency version control.
LAME_VERSION=3.100
OGG_VERSION=1.3.6
OPUS_VERSION=1.6.1
VORBIS_VERSION=1.3.7
ZLIB_VERSION=1.3.2
FFMPEG_VERSION=8.0.1

sudo apt install autoconf make cmake pkg-config git wget lsb-release gnupg xz-utils -y

mkdir ./build
cd ./build
mkdir ./output
mkdir ./output/include

PREFIX="`pwd`/output"
export EM_PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
export LIBRARY_PATH=${PREFIX}/lib
export cflags=-flto
export CXXFLAGS="-flto"

#Setup emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source `pwd`/emsdk_env.sh

cd ..

#lame
wget https://sourceforge.net/projects/lame/files/lame/${LAME_VERSION}/lame-${LAME_VERSION}.tar.gz
tar -xpf lame-${LAME_VERSION}.tar.gz
rm ./lame-${LAME_VERSION}.tar.gz
cd lame-${LAME_VERSION}
emconfigure ./configure --prefix=${PREFIX} \
--disable-shared \
--enable-static \
--disable-gtktest \
--disable-frontend \
--disable-decoder
emmake make -j `nproc`
emmake make install
cd ..
rm -rf ./lame-${LAME_VERSION}

#ogg
wget https://downloads.xiph.org/releases/ogg/libogg-${OGG_VERSION}.tar.gz
tar -xpf libogg-${OGG_VERSION}.tar.gz
rm ./libogg-${OGG_VERSION}.tar.gz
cd libogg-${OGG_VERSION}
emconfigure ./configure \
--prefix=${PREFIX} \
--disable-shared \
--enable-static
emmake make -j `nproc`
emmake make install
cd ..
rm -rf ./libogg-${OGG_VERSION}

#Opus
wget https://downloads.xiph.org/releases/opus/opus-${OPUS_VERSION}.tar.gz
tar -xpf opus-${OPUS_VERSION}.tar.gz
rm ./opus-${OPUS_VERSION}.tar.gz
cd opus-${OPUS_VERSION}
emconfigure ./configure \
--prefix=${PREFIX} \
--disable-shared \
--enable-static \
--disable-intrinsics
emmake make -j `nproc`
emmake make install
cd ..
rm -rf ./opus-${OPUS_VERSION}

#Vorbis
wget https://downloads.xiph.org/releases/vorbis/libvorbis-${VORBIS_VERSION}.tar.gz
tar -xpf libvorbis-${VORBIS_VERSION}.tar.gz
rm -rf ./libvorbis-${VORBIS_VERSION}.tar.gz
cd libvorbis-${VORBIS_VERSION}
emconfigure ./configure \
--prefix=${PREFIX} \
--disable-shared \
--enable-static
emmake make -j `nproc`
emmake make install
cd ..
rm -rf ./libvorbis-${VORBIS_VERSION}

#Zlib
git clone https://github.com/madler/zlib
cd zlib
git fetch --tags
git checkout v${ZLIB_VERSION}
emconfigure ./configure \
--prefix=${PREFIX} \
--disable-shared \
--enable-static
emmake make -j `nproc`
emmake make install
cd ..
rm -rf ./zlib

#FFmpeg
wget https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz
tar -xpf ffmpeg-${FFMPEG_VERSION}.tar.xz
rm ./ffmpeg-${FFMPEG_VERSION}.tar.xz
cd ffmpeg-${FFMPEG_VERSION}
emconfigure ./configure \
--prefix=${PREFIX} \
--disable-shared \
--enable-static \
--extra-ldflags="-L${PREFIX}/lib" \
--extra-cflags="-I${PREFIX}/include -flto" \
--pkg-config-flags="--static" \
--cc=emcc \
--cxx=em++ \
--ar=emar \
--ranlib=emranlib \
--disable-devices \
--disable-programs \
--disable-asm \
--disable-swscale \
--disable-network \
--disable-pthreads \
--disable-sdl2 \
--enable-libmp3lame \
--enable-libopus \
--enable-libvorbis \
--disable-openssl \
--enable-version3
emmake make -j `nproc`
emmake make install
cd ..
rm -rf ./ffmpeg-${FFMPEG_VERSION}

#Now build the library itself:

cd ..

emmake cmake -DCMAKE_PREFIX_PATH=`pwd`/build/output -DCMAKE_LIBRARY_PATH=`pwd`/build/output/lib -DCMAKE_INSTALL_PREFIX=`pwd`/build/output .
emmake make
emmake make install
