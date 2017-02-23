#!/bin/bash
MONTH=$(date +%m)
DAY=$(date +%d)
HOUR=$(date +%H)
MINUTE=$(date +%M)

#echo "$MONTH/$DAY $HOUR:$MINUTE"
CURRTIME=$HOUR$MINUTE
#echo "CURRTIME: $CURRTIME"
FILE=sun_table.txt

SUNRISE=`./getsunriseset.sh $MONTH $DAY rise $FILE`
SUNRISE=$((10#$SUNRISE+0))

SUNSET=`./getsunriseset.sh $MONTH $DAY set  $FILE`
SUNSET=$((10#$SUNSET+0))

CURRTIME=$((10#$CURRTIME+0))
#echo "Sunrise: $SUNRISE Sunset: $SUNSET"

if (( $CURRTIME > $SUNRISE )) && (( $CURRTIME < $SUNSET )); then
   echo "auto"
else
   echo "night"
fi
