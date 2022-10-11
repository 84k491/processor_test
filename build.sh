#!/bin/bash

build_dir="build"
mkdir $build_dir
cd $build_dir
cmake ..
cmake --build . -j3
