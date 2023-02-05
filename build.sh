# this script is used to build cgen directly from all sources.
# since cgen use cgen to build it self, there must be a way to create
# the alpha.

g++ -o cgen src/*.cpp src/xo/*.cpp src/mas/*.cpp src/pkgconf/*.cpp `pkg-config --cflags --libs libpkgconf`
