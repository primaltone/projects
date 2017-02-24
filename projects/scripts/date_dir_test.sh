#!/bin/bash

UpdateSaveDir()
{
   MONTH=$(date +%m)
   DAY=$(date +%d)
   YEAR=$(date +%Y)

   BASE_PATH=$1

   if [ -d  $BASE_PATH ]; then
      echo "path exists"
      if [ -d $BASE_PATH/$MONTH$DAY$YEAR ]; then
         echo "$BASE_PATH/$MONTH$DAY$YEAR don't create"
      else
         mkdir $BASE_PATH/$MONTH$DAY$YEAR
         if [ -d $BASE_PATH/$MONTH$DAY$YEAR ]; then
            echo "successfully created $BASE_PATH/$MONTH$DAY$YEAR"
         else
            echo "failed to creat $BASE_PATH/$MONTH$DAY$YEAR"
         fi
      fi
   else
      echo "$BASE_PATH does not exist"
      return
   fi
   echo $BASE_PATH/$MONTH$DAY$YEAR   
}

NewDir=$(UpdateSaveDir $1)
echo $NewDir
