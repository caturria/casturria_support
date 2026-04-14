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
LLVM_VERSION=21
LAME_VERSION=3.100
OGG_VERSION=1.3.6
OPUS_VERSION=1.6.1
VORBIS_VERSION=1.3.7
ZLIB_VERSION=1.3.2
SSL_VERSION=4.2.1
FFMPEG_VERSION=8.0.1

sudo apt install autoconf make cmake pkg-config git wget nasm lsb-release gnupg xz-utils libssl-dev -y

#Install Clang from LLVM repo.
wget https://apt.llvm.org/llvm.sh
chmod 755 llvm.sh
sudo ./llvm.sh ${LLVM_VERSION}
rm ./llvm.sh

#Configure Clang as the default C/ C++ compiler.
sudo update-alternatives --install /usr/bin/cc cc /usr/bin/clang-${LLVM_VERSION} 100
sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang-${LLVM_VERSION} 100
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/clang-${LLVM_VERSION} 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/clang-${LLVM_VERSION} 100
sudo update-alternatives --install /usr/bin/ld ld /usr/bin/lld-${LLVM_VERSION} 100
sudo update-alternatives --install /usr/bin/ld.lld ld.lld /usr/bin/ld.lld-${LLVM_VERSION} 100

mkdir ./build
cd ./build
mkdir ./output
mkdir ./output/include

PREFIX="`pwd`/output"
export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
export LIBRARY_PATH=${PREFIX}/lib
#Work around Lame's broken shared build when linking with LLD.
#export LDFLAGS="-Wl,--undefined-version"
export CFLAGS="-fPIC"
export CXXFLAGS="-fPIC"
export ASFLAGS="-fPIC"

#lame
wget https://sourceforge.net/projects/lame/files/lame/${LAME_VERSION}/lame-${LAME_VERSION}.tar.gz
tar -xpf lame-${LAME_VERSION}.tar.gz
rm ./lame-${LAME_VERSION}.tar.gz
cd lame-${LAME_VERSION}
./configure --prefix=${PREFIX} \
--disable-shared \
--enable-static \
--enable-nasm \
--disable-gtktest \
--disable-frontend \
--disable-decoder
make -j `nproc`
make install
cd ..
rm -rf ./lame-${LAME_VERSION}

#ogg
wget https://downloads.xiph.org/releases/ogg/libogg-${OGG_VERSION}.tar.gz
tar -xpf libogg-${OGG_VERSION}.tar.gz
rm ./libogg-${OGG_VERSION}.tar.gz
cd libogg-${OGG_VERSION}
./configure \
--prefix=${PREFIX} \
--disable-shared \
--enable-static
make -j `nproc`
make install
cd ..
rm -rf ./libogg-${OGG_VERSION}

#Opus
wget https://downloads.xiph.org/releases/opus/opus-${OPUS_VERSION}.tar.gz
tar -xpf opus-${OPUS_VERSION}.tar.gz
rm ./opus-${OPUS_VERSION}.tar.gz
cd opus-${OPUS_VERSION}
./configure \
--prefix=${PREFIX} \
--disable-shared \
--enable-static
make -j `nproc`
make install
cd ..
rm -rf ./opus-${OPUS_VERSION}

#Vorbis
wget https://downloads.xiph.org/releases/vorbis/libvorbis-${VORBIS_VERSION}.tar.gz
tar -xpf libvorbis-${VORBIS_VERSION}.tar.gz
rm -rf ./libvorbis-${VORBIS_VERSION}.tar.gz
cd libvorbis-${VORBIS_VERSION}
./configure \
--prefix=${PREFIX} \
--disable-shared \
--enable-static
make -j `nproc`
make install
cd ..
rm -rf ./libvorbis-${VORBIS_VERSION}

#Zlib
git clone https://github.com/madler/zlib
cd zlib
git fetch --tags
git checkout v${ZLIB_VERSION}
./configure \
--prefix=${PREFIX} \
--disable-shared \
--enable-static
make -j `nproc`
make install
cd ..
rm -rf ./zlib

#Libressl
wget https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-${SSL_VERSION}.tar.gz
tar -xpf libressl-${SSL_VERSION}.tar.gz
rm ./libressl-${SSL_VERSION}.tar.gz
cd libressl-${SSL_VERSION}
./configure \
--prefix=${PREFIX} \
--disable-shared \
--enable-static
make -j `nproc`
make install
cd ..
rm -rf ./libressl-${SSL_VERSION}
#FFmpeg
wget https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz
tar -xpf ffmpeg-${FFMPEG_VERSION}.tar.xz
rm ./ffmpeg-${FFMPEG_VERSION}.tar.xz
cd ffmpeg-${FFMPEG_VERSION}
./configure \
--prefix=${PREFIX} \
--disable-shared \
--enable-static \
--extra-ldflags="-L${PREFIX}/lib" \
--extra-cflags="-I${PREFIX}/include -fuse-ld=lld" \
--pkg-config-flags="--static" \
--disable-devices \
--disable-programs \
--disable-asm \
--enable-libmp3lame \
--enable-libopus \
--enable-libvorbis \
--enable-libtls \
--disable-openssl \
--enable-pic \
--enable-version3
make -j `nproc`
make install
cd ..
rm -rf ./ffmpeg-${FFMPEG_VERSION}
