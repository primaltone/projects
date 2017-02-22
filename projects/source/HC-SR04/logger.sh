filename=/mnt/nfs/home/share/sump/sump_log.txt
while true
do
   printf "%s %s\n" $(date +"%m-%d-%Y-%T") $(./usonic.py) >> $filename
   sleep 2
done
