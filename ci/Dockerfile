FROM debian:latest

RUN apt-get update

RUN apt-get install -y git
RUN apt-get install -y ruby-full

# install cmake
RUN apt-get install -y wget gpg
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
RUN echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main" | tee /etc/apt/sources.list.d/kitware.list
RUN apt-get update
RUN apt-get install -y cmake

# add idxd-config build dependencies
RUN apt-get install -y build-essential
RUN apt-get install -y autoconf automake autotools-dev libtool pkgconf asciidoc xmlto
RUN apt-get install -y uuid-dev libjson-c-dev libkeyutils-dev libz-dev libssl-dev
RUN apt-get install -y debhelper devscripts debmake quilt fakeroot lintian asciidoctor
RUN apt-get install -y file gnupg patch patchutils

# download idxd-config
RUN git clone https://github.com/intel/idxd-config.git

# build idxd-config
WORKDIR /idxd-config
RUN ./autogen.sh
RUN ./configure CFLAGS='-g -O2' --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib64
RUN make
RUN make check
RUN make install
WORKDIR /

# build and install syscall_intercept
RUN apt-get install -y clang pkg-config libcapstone-dev
RUN git clone https://github.com/pmem/syscall_intercept.git
WORKDIR /syscall_intercept/build
RUN cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang
RUN make
RUN make install
WORKDIR /
