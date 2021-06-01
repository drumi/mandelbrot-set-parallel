echo "Available build macros _MEASURE_, _DYNAMIC_, _GRANULARITY_VISUAL_, _GRANULARITY_VISUAL_EXTENDED_"

s=""
while [ ! -z $1 ]
do
    s+=" -D$1"
    shift
done

g++ -std=c++11 -O3 -ffast-math -pthread -march=native *.cpp $s
