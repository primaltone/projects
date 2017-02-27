/*
   - add timestamp to sample
   - add variable for polling rate, change to 1 second default
   - add running average, make #samples variable, start with 5
   - define theshold for when to save a sample; ideally no more than 1ish sample per minute
   - add mechanism, such that even if value doesn't change per threshold, a sample is still saved at a minimum interval, such as 1 second
   - not code, really: is there a way to detect when sump runs, without adding risk to functionality?
   - make sure storing off values does not interfere with timing
 */
/*
   Compile as follows:

   gcc -o IntSensor IntSensor.c -lwiringPi -pthread -Wall

   Run as follows:

   sudo ./IntSensor

 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <sys/time.h>
#include <semaphore.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#define VERSION 1.1
#define DEBUG 1
// Use GPIO Pin 17, which is Pin 0 for wiringPi library
#define TRIGGER 0
// Use GPIO Pin 27, which is Pin 2 for wiringPi library
#define ECHO 2
sem_t calculation;
#define min_sample_store_period 30 // jwd make me a variable
#define REMOTE_FILE_NAME "/mnt/nfs/home/share/sump/sump_log1.txt"
#define DISTANCE_UNITS INCHES
#define SENSOR_TO_BOTTOM_OF_WELL_INCHES 34 // make me a variable
#define TEMP_FILE "/var/tmp/sump_info.txt"
int verbose=0;
int sensorErrorCount = 0;
struct sample_data {
	double distance;
	time_t time;
	struct sample_data *next;
};

typedef enum 
{
	INCHES,
	CM
} UNITS;

// defines that should become variables
#define SAMPLE_PERIOD_ms 2000
#define TRIGGER_DURATION_MS 10
#define RUNNING_AVG_SAMPLES 5
#define CHANGE_THRESHOLD_PERCENT 4

int sendmail(const char *to, const char *from, const char *subject, const char *message)
{
	int retval = -1;
	
	FILE *mailpipe;
	
	if (!to)
	{
		printf("Error: must specify email \"to\" address\n");
	}
	else if (!from)
	{
		printf("Error: must specify email \"from\" address\n");
	}
	else if ((mailpipe = popen("/usr/lib/sendmail -t", "w")) != NULL)
	{
		fprintf(mailpipe, "To: %s\n", to);
		fprintf(mailpipe, "From: %s\n", from);
		fprintf(mailpipe, "Subject: %s\n\n", subject);
		fwrite(message, 1, strlen(message), mailpipe);
		fwrite(".\n", 1, 2, mailpipe);
		pclose(mailpipe);
		retval = 0;
	}
	if (verbose) printf("email: %s %s\n",subject,message);

	return retval;
}

/* return delay in micro seconds */
long timevaldiff(struct timeval *starttime, struct timeval *finishtime)
{
	long usec;
	usec=(finishtime->tv_sec-starttime->tv_sec)*1000000;
	usec+=(finishtime->tv_usec-starttime->tv_usec);
	return usec;
}

double lastDistance=0;
struct timeval t1, t2 ;
void myInterrupt(void) {
	int echoVal;

	echoVal = digitalRead(ECHO);

	if (echoVal == 1)
	{
		// signal went high, start timing
		gettimeofday(&t1,NULL); 
	}
	else // echoVal must = 0
	{
		gettimeofday(&t2,NULL);
		sem_post(&calculation); 
	}
}

int SetupIO(void)
{
	// sets up the wiringPi library
	if (wiringPiSetup () < 0) {
		fprintf (stderr, "Unable to setup wiringPi: %s\n", strerror (errno));
		return -1;
	}

	/* configure trigger pin as output, init value to low */
	pinMode(TRIGGER,OUTPUT);
	digitalWrite(TRIGGER,0);

	// set Pin 17/0 generate an interrupt on both edges. handler will process each edge
	// and attach myInterrupt() to the interrupt
	if ( wiringPiISR (ECHO, INT_EDGE_FALLING|INT_EDGE_RISING, &myInterrupt) < 0 ) {
		fprintf (stderr, "Unable to setup ISR: %s\n", strerror (errno));
		return -1;
	}
	return 0;
}

struct sample_data * SetupRingBuffer(const unsigned int NumElements)
{
	struct sample_data *plastsamples=NULL,*pcurrsamples;
	int i;

	plastsamples = (struct sample_data*)malloc(sizeof(struct sample_data) * NumElements );

	pcurrsamples = plastsamples;

	for (i=1;i<NumElements;i++)
	{
		pcurrsamples->next = pcurrsamples+1;
		pcurrsamples = pcurrsamples->next;
	}
	pcurrsamples->next=plastsamples;

	return plastsamples;
}

double MeasureDistance( UNITS UnitSetting )
{
	long timediff;
	double distance;

	digitalWrite(TRIGGER,1);
	delayMicroseconds(TRIGGER_DURATION_MS);
	digitalWrite(TRIGGER,0);
	sem_wait(&calculation);

	timediff = timevaldiff(&t1,&t2);
	distance = timediff * 17./1000.;
	if (UnitSetting == INCHES)
	{
		distance /= 2.54;	
	}
	return distance;
}

void FormatTime(const time_t time, char *timeStr,const int bufsize)
{
	struct tm *info;
	
	if (timeStr)
	{
		info = localtime( &time );
		strftime(timeStr, bufsize, "%Y-%m-%d %H:%M:%S %Z", info);
	}
}


int SaveSample(const time_t time, const double distance, char *remoteFileSave, char *localFileSave)
{
	int Output_fd, rc=-1; char buffer [128]; char timeBuf[128];

	if(remoteFileSave && (Output_fd = open(remoteFileSave, O_WRONLY | O_CREAT| O_APPEND, 0644)) >= 0 )
	{
		/* remote availale */
	}
	else if(localFileSave && (Output_fd = open(localFileSave, O_WRONLY | O_CREAT| O_APPEND, 0644)) >= 0 )
	{
		/* local availale */
	}

	if(Output_fd >= 0)
	{
		FormatTime(time,timeBuf,sizeof(timeBuf));
		snprintf(buffer, sizeof(buffer),"%s %.2lf%s\n", timeBuf,distance,(DISTANCE_UNITS==INCHES? "\"":"cm"));
		rc = write (Output_fd, &buffer,strlen(buffer));
		close (Output_fd);
	}
	else
		printf("Warning: unable to open local or remote file for save\n");

	return rc;
}
void PrintSyntax( char *executableName )
{
        printf("\n  %s Version %.2f\n",executableName,VERSION);
        printf("\n  Options:");
        printf("\n  -t : set email notification \"to\" address");
        printf("\n  -f : set email notification \"from\" address");
        printf("\n  -r : remote date file");
        printf("\n  -l : local data file - to be used if remote not available");
        printf("\n  -v : set verbose mode");
        printf("\n  -? : program description\n");
}

double MeasureAndValidate(UNITS units, double sensorHeight)
{
	double distance;
	while((distance = MeasureDistance(units)) > sensorHeight)
	{
		sensorErrorCount++;
		printf("Error: invalid reading. Numerrs: sensorErrorCount %d --  sensor distance = %.2lf sensorToBottomOfWell = %.2lf\n",sensorErrorCount,distance, sensorHeight);
		delay(1000);
	}
	return distance;
}

int main( int argc, char * argv[] ) {
	struct sample_data *plastsamples,*pcurrsamples,*avgRunner; 
	sem_init(&calculation,1,0);
	double avg,sum,sensorToBottomOfWell,sensorDistance,AlertThreshold=0;
	time_t starttime,finishtime;;
	int tmpFileFD = -1;
	int alertCount = 0, index=0; 
	int numOptions;
	char *emailAddressTo=NULL; 
	char *emailAddressFrom=NULL;
	char *remoteFile;
	char *localFile;

///////////////
    /* first check to see if a help screen is desired */

    if ( (argc > 1) &&  ( ((argv[1][0] == '/') || (argv[1][0] == '-')) && (argv[1][1] == '?')) )
    {
        PrintSyntax(argv[0]);
        exit(-1);
    }

    /* now check for options */
    /* add 1 to index to account for program name*/

    if (argc > 1) // don't attempt to read if no argument exists
    {
        numOptions = 1;
        while (argc > numOptions)
        {
            switch (argv[numOptions][1])
            {
            /* check to see if user wants associated file names at the top of */
            /* each column */
            
            case 'a':
			if ( argc >= numOptions)
			{
				numOptions++;
               	AlertThreshold = atof(argv[numOptions]);
				printf("Alert Threshold set to: %.2lf\n",AlertThreshold);
	     	    numOptions++;
			}
			else
			{
                	PrintSyntax(argv[0]);
	                exit(-1);
			}
            break;
            case 't':
			if ( argc >= numOptions)
			{
				numOptions++;
               	emailAddressTo = argv[numOptions];
				printf("Sending email notifications to: %s\n",emailAddressTo);
	     	    numOptions++;
			}
			else
			{
                	PrintSyntax(argv[0]);
	                exit(-1);
			}
            break;
           
			case 'f':
			if ( argc >= numOptions)
			{
				numOptions++;
           		emailAddressFrom = argv[numOptions];
				printf("Sending email notifications from: %s\n",emailAddressFrom);
	        	numOptions++;
			}
			else
			{
                	PrintSyntax(argv[0]);
	                exit(-1);
			}
            break;
   
			case 'r':
			if ( argc >= numOptions)
			{
				numOptions++;
           		remoteFile = argv[numOptions];
				printf("Using %s for remote file save\n",remoteFile);
	        	numOptions++;
			}
			else
			{
                	PrintSyntax(argv[0]);
	                exit(-1);
			}
            break;
			case 'l':
			if ( argc >= numOptions)
			{
				numOptions++;
           		localFile = argv[numOptions];
				printf("Using %s for local file save if remote not available\n",localFile);
	        	numOptions++;
			}
			else
			{
                	PrintSyntax(argv[0]);
	                exit(-1);
			}
            break;
			case 'v':
			if ( argc >= numOptions)
			{
	        	verbose=1;
				numOptions++;
			}
			else
			{
                	PrintSyntax(argv[0]);
	                exit(-1);
			}
            break;
            default:
                printf("\n%c%c is not a valid option",argv[numOptions][0],argv[numOptions][1]);
                PrintSyntax(argv[0]);
                exit(-1);
                break;
            }
        }
    }
    else
    {
        PrintSyntax(argv[0]);
        exit(-1);
    }

//////////////

#if 0
jwd next steps:
 parameter to save to sd if no server
#endif

	if (DISTANCE_UNITS == CM)
		sensorToBottomOfWell = SENSOR_TO_BOTTOM_OF_WELL_INCHES*2.54;  
	else
		sensorToBottomOfWell = SENSOR_TO_BOTTOM_OF_WELL_INCHES;  
	
	starttime=time(NULL);
	/* test 1, sump test started */

	sendmail(emailAddressTo,emailAddressFrom,"sump monitor info: started","monitoring started");

	/* set up PIO's and configure interrupt */
	SetupIO();

	/* set up ring buffer for running avg calc */
	pcurrsamples= plastsamples=SetupRingBuffer(RUNNING_AVG_SAMPLES);

	sensorDistance = MeasureAndValidate(DISTANCE_UNITS,sensorToBottomOfWell);
	SaveSample(time(NULL),sensorToBottomOfWell - sensorDistance, remoteFile, localFile );

	// display counter value every second.
	while ( 1 ) {
		int i;
		double percentChange;
		char  waterLevel[256];
		char timeBuf[128];
		
		pcurrsamples->distance = MeasureAndValidate(DISTANCE_UNITS,sensorToBottomOfWell);
		pcurrsamples->time = time(NULL);
#if DEBUG
		printf("Index: %d Current Sensor Distance: %.2lf%s\t",index,pcurrsamples->distance,(DISTANCE_UNITS==INCHES? "\"":"cm"));
		printf("Water height = %.2lf%s\n",(sensorToBottomOfWell -pcurrsamples->distance),(DISTANCE_UNITS==INCHES? "\"":"cm"));
#endif
		FormatTime(pcurrsamples->time,timeBuf,sizeof(timeBuf));
		snprintf(waterLevel,sizeof(waterLevel),"%s water height running avg: %.2lf%s",timeBuf,
		  (sensorToBottomOfWell - avg),(DISTANCE_UNITS==INCHES)? "\"":"cm");
	
		tmpFileFD = open(TEMP_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (tmpFileFD < 0)
		{
			perror("failed to open tmp file");
		}
		else
		{
			write (tmpFileFD, &waterLevel,strlen(waterLevel));
			close (tmpFileFD);
		}

		if ((AlertThreshold > 0) &&
			((sensorToBottomOfWell - avg) > AlertThreshold) &&
            (index > RUNNING_AVG_SAMPLES))
		{
			if (alertCount < 3)
			{
				/* don't want to flood email */
				sendmail(emailAddressTo,emailAddressFrom,"Sump Monitor WARNING!!\n%s ",waterLevel);
				alertCount++;
			}
		}

		if (lastDistance) 
			percentChange = 100*(pcurrsamples->distance-lastDistance)/lastDistance; 
		
		lastDistance=pcurrsamples->distance;

		/* running avg calculations */	
		sum=0; 
		avgRunner = pcurrsamples;
		for (i=0;i<RUNNING_AVG_SAMPLES;i++)
		{
			sum+=avgRunner->distance;
			avgRunner=avgRunner->next;
		}
		avg=sum/RUNNING_AVG_SAMPLES;

		finishtime=time(NULL);

		if ((int)difftime(finishtime,starttime) > min_sample_store_period ) // jwd wrong? only grabbing start time once?
		{
			starttime=time(NULL);
			SaveSample(pcurrsamples->time,sensorToBottomOfWell - pcurrsamples->distance, remoteFile, localFile );
			//make sure we block until sample saved	
		}
		else if (abs(percentChange) > CHANGE_THRESHOLD_PERCENT)
		{
			SaveSample(pcurrsamples->time,sensorToBottomOfWell - pcurrsamples->distance, remoteFile, localFile );
		}
		/* Update sample data */
		pcurrsamples = pcurrsamples->next;
		delay(SAMPLE_PERIOD_ms);
		index++;
	}
	// jwd - this will never execute; proper way to free when Ctrl-C?
	if(plastsamples) free(plastsamples);
	close(tmpFileFD);

	return 0;
}
