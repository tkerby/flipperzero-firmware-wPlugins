#!/bin/bash

t=$(mktemp -d);
mkdir $t/1 $t/2 $t/3
echo "Working in $t";

for i in $(ls *.png | sort); do 
  convert $i -alpha remove +repage -crop 512x256+1120+626 +repage $t/1/$i;
  convert $t/1/$i -resize 256x128 $t/2/$i;
  convert $t/2/$i  $t/3/$i;
done;

convert -delay 100 -loop 0 $t/3/*.png result.gif
