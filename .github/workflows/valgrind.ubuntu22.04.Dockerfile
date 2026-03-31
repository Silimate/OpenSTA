# further run memory leak checks with valgrind
FROM sta-ubuntu-22.04

RUN apt-get install -y valgrind

RUN cd /OpenSTA &&\
    rm -rf build &&\
    mkdir build &&\
    cd build &&\
    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-O0' -DCUDD_DIR=../cudd-3.0.0 .. && \
    make -j`nproc` VERBOSE=1

RUN cd /OpenSTA/test &&\
    ./regression -valgrind -j`nproc`&&\
    ./regression -valgrind -collections -j`nproc`
