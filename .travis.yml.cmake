language: c
dist: bionic
osx_image: xcode10.3

compiler:
  - gcc
  - clang
os:
  - linux
  - osx
env:
  - VERSION=@VERSION@

addons:
  apt:
    sources:
    - sourceline: 'deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-9 main'
      key_url: 'https://apt.llvm.org/llvm-snapshot.gpg.key'
    - sourceline: 'ppa:ubuntu-toolchain-r/test'
    packages:
    - rpm
    - libsdl2-dev
    - libsdl2-image-dev
    - libsdl2-mixer-dev
    - clang-9
    - cmake
    - gcc-9
    - g++-9
    - ninja-build

install:
# /usr/bin/gcc points to an older compiler on both Linux and macOS.
- if [ "$CXX" = "g++" ]; then export CXX="g++-9" CC="gcc-9"; fi
# /usr/bin/clang points to an older compiler on both Linux and macOS.
#
# Homebrew's llvm package doesn't ship a versioned clang++ binary, so the values
# below don't work on macOS. Fortunately, the path change above makes the
# default values (clang and clang++) resolve to the correct compiler on macOS.
- if [ "$TRAVIS_OS_NAME" = "linux" ]; then
    if [ "$CXX" = "clang++" ]; then export CXX="clang++-9" CC="clang-9"; fi;
  fi
- echo ${CC}

before_script:
  - export CTEST_OUTPUT_ON_FAILURE=1

script:
  - if [[ "$TRAVIS_OS_NAME" == "osx" ]]; then sh build/macosx/install-sdl2.sh ; fi
  # Match install prefix with data dir so that package contains everything required
  - cmake -DCMAKE_INSTALL_PREFIX=. -DDATA_INSTALL_DIR=. -Wno-dev .
  - make -j2

  # Tests are broken on osx. Hope this will be fixed some day
  - if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then make test ; fi

after_success:
  - bash <(curl -s https://codecov.io/bash)

before_deploy:
  - make package

  #debug
  - ls $TRAVIS_BUILD_DIR
deploy:
  provider: releases
  edge: true
  api_key:
    secure: Rus8lTl0EnVqM6PXwleQ8cffjMTMY1gHGwVdbGsu8cWaDgAWQ86TFgGBbV+x12z9floDPzI7Z1K/entktkiSWQyRPIa9jQfJBIomNABhIykUvpRsL026Cs8TysI4L4hrTvFev10QI28RFyZvUDBT8yytowFsuU5Pfb4n7kDIisQ=
  file_glob: true
  skip_cleanup: true
  file:
    - "$TRAVIS_BUILD_DIR/C-Dogs*SDL-*-{Linux,OSX}.{tar.gz,dmg}"
  on:
    tags: true
    condition: $CC = gcc

after_deploy:
  - bash build/travis-ci/butler.sh
