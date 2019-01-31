#ddp

[![Build Status](https://travis-ci.com/jamesmtuck/ddp.svg?branch=master)](https://travis-ci.com/jamesmtuck/ddp)

Simple setup with Docker
--

1. git clone https://github.com/jamesmtuck/ddp.git
2. cd ddp
3. docker-compose build
4. docker-compose up
5. docker-compose run ddp

Now you are inside the docker container with the git repo in a mounted volume.

1. mkdir build && cd build
2. cmake -DLLVM_DIR=/usr/local/lib/cmake/llvm -DCMAKE_INSTALL_PREFIX=`pwd`/install ..
3. cmake --build . 
4. cmake --build . --target install
