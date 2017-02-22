VERSION=1.0
WIDTH=1280
HEIGHT=1024
QUALITY=40
ROTATION=180
DELAY=1
FILE_DIR=/mnt/750GBShare/Share750/WC/02052017

usage()
{
 echo "Version: $VERSION"
 echo "Uses camera to take still shots:"
 echo ""
 echo "$0 <options>"
 echo "-P --> path for primary file storage location"
 echo "-p --> path for secondary file storage location"
 echo "-h --> height dimension (defaults to $HEIGHT)"
 echo "-w --> height dimension (defaults to $WIDTH)"
 echo "-q --> jpeg quality (defaults to $QUALITY)"
 echo "-d --> delay between captures (defaults to $DELAY)"
 echo "-r --> rotation (defaults to $ROTATION)"
}

############################################################################################
if [[ $1 == "" ]]; then
    usage
    exit
fi

OPTIND=1         # Reset in case getopts has been used previously in the shell.

# Initialize our own variables:

while getopts "P:p:h:w:q:r:d:h" opt; do
    case "$opt" in
    P)
        PRIMARY_PATH=$OPTARG
        ;;
    p)  
        SECONDARY_PATH=$OPTARG
        ;;
    h)  
        HEIGHT=$OPTARG
        ;;
    w)  
        WIDTH=$OPTARG
        ;;
    q)  
        QUALITY=$OPTARG
        ;;
    r)  
        ROTATION=$OPTARG
        ;;
    d)  
        DELAY=$OPTARG
        ;;
    h) # print usage
        usage
        exit 0
    esac
done

shift $((OPTIND-1))

[ "$1" = "--" ] && shift

if [[ $PATH == "" ]]; then
    echo "must provide path for file storage"
    # make sure to check this path later
    usage
    exit
fi

echo "PRIMARY_PATH: $PRIMARY_PATH QUALITY: $QUALITY WIDTH: $WIDTH HEIGHT: $HEIGHT DELAY: $DELAY"


i=1
while true
do
  echo "image $i"
  i=$((i+1))
  DATE=`date +%Y-%m-%d:%H:%M:%S` 
# night  raspistill -ss 500000 -ex night -md 0 -awb auto -q $QUALITY -rot $ROTATION -t 500 -w $WIDTH -h $HEIGHT -o $PRIMARY_PATH/Pic$DATE.jpg
  raspistill -ex auto -md 0 -awb auto -q $QUALITY -rot $ROTATION -t 500 -w $WIDTH -h $HEIGHT -o $PRIMARY_PATH/Pic$DATE.jpg

 # echo "raspistill -md 0 -awb auto -q $QUALITY -rot $ROTATION -t 500 -w $WIDTH -h $HEIGHT -o $PRIMARY_PATH/Pic$DATE.jpg"
  #fswebcam -S 10 $FILE_DIR/Pic$DATE.jpg 
sleep $DELAY 
done
