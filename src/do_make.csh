#! /bin/csh

if($1 == "dibas") then
make clean ; make all_gbt S6_LOCATION="-D SOURCE_DIBAS"
else if($1 == "s6") then
make clean ; make all_gbt S6_LOCATION="-D SOURCE_S6"
else if($1 == "fast") then
make clean ; make all_fast S6_LOCATION="-D SOURCE_FAST"
else if($1 == "mro") then
make clean ; make all_mro S6_LOCATION="-D SOURCE_MRO"
else if($1 == "lab") then
make clean ; make all_mro S6_LOCATION="-D SOURCE_MRO -D LAB_TEST"
endif
