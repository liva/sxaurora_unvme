FROM veos:develop AS boost
RUN yum install -y wget
RUN wget https://dl.bintray.com/boostorg/release/1.70.0/source/boost_1_70_0.tar.bz2
RUN tar xf boost_1_70_0.tar.bz2
WORKDIR boost_1_70_0
#RUN echo "using clang : ve : /opt/nec/nosupport/llvm-ve/bin/clang++ ; " >> tools/build/src/user-config.jam
RUN yum install -y which
RUN ./bootstrap.sh --prefix=/opt/nec/ve/ --with-libraries=headers
RUN ./b2 install
WORKDIR ..

FROM veos:develop
RUN yum install -y epel-release
RUN yum install -y which cmake3 make
COPY --from=boost /opt/nec/ve/include/boost /opt/nec/ve/include/boost
