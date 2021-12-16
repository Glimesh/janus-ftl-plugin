FROM ubuntu:20.04

RUN \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get -y install curl g++-10 git libmicrohttpd-dev libjansson-dev libssl-dev libsofia-sip-ua-dev libglib2.0-dev libopus-dev libogg-dev libcurl4-openssl-dev liblua5.3-dev libconfig-dev pkg-config gengetopt libtool automake python3 python3-pip python3-setuptools python3-dev python3-wheel ninja-build libavcodec-dev && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100 --slave /usr/bin/g++ g++ /usr/bin/g++-10 --slave /usr/bin/gcov gcov /usr/bin/gcov-10

RUN pip3 install meson

WORKDIR /tmp

ENV LIBNICE_VERSION=0.1.18
ENV LIBSRTP_VERSION=v2.4.2
ENV JANUSGATEWAY_VERSION=v0.11.6

RUN \
    DIR=/tmp/libnice && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/libnice/libnice/archive/${LIBNICE_VERSION}.tar.gz | tar -zx --strip-components=1 && \
    meson --prefix=/usr build/ && \
    ninja -C build && \
    ninja -C build install && \
    rm -rf ${DIR}

RUN \
    DIR=/tmp/libsrtp && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/cisco/libsrtp/archive/${LIBSRTP_VERSION}.tar.gz | tar -zx --strip-components=1 && \
    ./configure --prefix=/usr --enable-openssl && \
    make shared_library && \
    make install && \
    rm -rf ${DIR}

RUN \
    DIR=/tmp/janus-gateway && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/meetecho/janus-gateway/archive/${JANUSGATEWAY_VERSION}.tar.gz | tar -zx --strip-components=1 && \
    sh autogen.sh && \
    ./configure --prefix=/opt/janus \
                --disable-rabbitmq \
                --disable-mqtt \
                --disable-unix-sockets \
                --disable-websockets \
                --disable-all-handlers \
                --disable-all-plugins && \
    make && \
    make configs && \
    make install && \
    rm -rf ${DIR}

WORKDIR /app

COPY . /app

RUN \
    DIR=/app && \
    CC=gcc-10 CXX=g++-10 meson --buildtype=debugoptimized build/ && \
    cd build/ && \
    ninja && \
    ninja install

# Janus API HTTP
EXPOSE 8088/tcp
# Janus API HTTPS
EXPOSE 8089/tcp
# FTL Ingest Handshake
EXPOSE 8084/tcp
# FTL Media
EXPOSE 9000-9100/udp
# RTP Media
EXPOSE 20000-20100/udp
# NOTE: Usually we'd want a way larger Media/RTP port range
# but Docker is extremely slow at opening huge port ranges
# (see moby/moby#14288)

CMD exec /opt/janus/bin/janus --rtp-port-range=20000-20100 --nat-1-1=${DOCKER_IP}
