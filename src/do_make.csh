#! /bin/csh

if($1 == "dibas") then
make clean ; make all_gbt S6_LOCATION="-D SOURCE_DIBAS"
else if($1 == "s6") then
make clean ; make all_gbt S6_LOCATION="-D SOURCE_S6"
else if($1 == "fast") then
make clean ; make all_fast S6_LOCATION="-D SOURCE_FAST"
endif
