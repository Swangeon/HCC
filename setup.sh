#!/bin/bash

git clone https://github.com/swangeon/Haiku-openvpn.git
pkgman install pip_python39 -y
pip install pypacker
pkgman install libtool -y
pkgman install openpam_devel -y
pkgman install lzo_devel -y
pkgman install lz4_devel -y
pkgman install pkcs11_helper_devel -y
pkgman install docutils_python39 -y

cd Haiku-openvpn
autoreconf -i -v -f
LDFLAGS=-lnetwork ./configure --prefix=/boot/home/config/non-packaged
make install
