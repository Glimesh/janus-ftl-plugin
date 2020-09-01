FROM ubuntu:18.04

RUN apt-get update && apt-get -y install curl libmicrohttpd-dev libjansson-dev libssl-dev libsofia-sip-ua-dev libglib2.0-dev libopus-dev libogg-dev libcurl4-openssl-dev liblua5.3-dev libconfig-dev pkg-config gengetopt libtool automake python3 python3-pip python3-setuptools python3-dev python3-wheel ninja-build

RUN pip3 install meson

WORKDIR /tmp

ENV LIBNICE_VERSION=0.1.17
ENV LIBSRTP_VERSION=v2.3.0
ENV JANUSGATEWAY_VERSION=v0.10.4

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
    ./configure --prefix=/opt/janus && \
    make && \
    make configs && \
    make install && \
    rm -rf ${DIR}

WORKDIR /app

COPY . /app

RUN \ 
    DIR=/app && \
    meson build/ && \
    cd build/ && \
    ninja && \
    ninja install

EXPOSE 8088/tcp
EXPOSE 8089/tcp
EXPOSE 8084/tcp
EXPOSE 9000-10000/udp

CMD ["/opt/janus/bin/janus"]
