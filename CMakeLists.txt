cmake_minimum_required(VERSION 3.26)
project(cpmFilesystem C)

set(CMAKE_C_STANDARD 11)

add_executable(cpmFilesystem fsysdriver.c
            diskSimulator.c
            diskSimulator.h
            cpmfsys.c
            cpmfsys.h
)
