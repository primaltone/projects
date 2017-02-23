#!/bin/bash

usage()
{
   echo "$0 <month> <day> <sunphase = rise or set> <file table>"
}

if [[ $1 != "" ]] && [[ $1 -ge 1 ]] && [[ $1 -le 12 ]]; then
  MONTH=$1
else
  echo "No month or invalid month given"
  usage
  exit
fi

if [[ $2 != "" ]] && [[ $2 -ge 1 ]] && [[ $2 -le 31 ]]; then
  DAY=$2 
else
  echo "no day or invalid day given"
  usage
  exit
fi

if [[ $3 == "rise" ]]; then
    SUNPHASE=0
elif [[ $3 == "set" ]]; then
    SUNPHASE=1
else
  echo "no sunphase or invalid value given"
  usage
  exit
fi

if [[ $4 != "" ]] && [ -f $4 ]; then
  FILE=$4 
else
  echo "no file or invalid file given"
  usage
  exit
fi

awk -v month=$MONTH -v sun=$SUNPHASE -v day=$DAY '{if ($1 == day) print $((month*2)+sun)}' $FILE
