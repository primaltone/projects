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


#define DEBUG 1
// Use GPIO Pin 17, which is Pin 0 for wiringPi library
#define TRIGGER 0
// Use GPIO Pin 27, which is Pin 2 for wiringPi library
#define ECHO 2
sem_t calculation;
#define min_sample_store_period 30 // jwd make me a variable
#define FILE_NAME "/mnt/nfs/home/share/sump/sump_log1.txt"

struct sample_data {
	double distance;
	time_t time;
	struct sample_data *next;
};
int sample_counter = 0;

// defines that should become variables
#define SAMPLE_PERIOD_ms 2000
#define TRIGGER_DURATION_MS 10
#define RUNNING_AVG_SAMPLES 5
#define CHANGE_THRESHOLD_PERCENT 4

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

void timestamp(void)
{
	time_t ltime;

	ltime=time(NULL);
	printf("%s",asctime(localtime(&ltime)));
}

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
double MeasureDistance(void)
{
	long timediff;
	double distance;

	digitalWrite(TRIGGER,1);
	delayMicroseconds(TRIGGER_DURATION_MS);
	digitalWrite(TRIGGER,0);
	sem_wait(&calculation);

	timediff = timevaldiff(&t1,&t2);
	distance = timediff * 17./1000.;

	return distance;
}

int SaveSample(const time_t time, const double distance)
{   
    int Output_fd,rc;
    char buffer [128];
    char timeBuf[128];
    struct tm *info;

    Output_fd = open(FILE_NAME, O_WRONLY | O_CREAT| O_APPEND, 0644);

    if(Output_fd == -1){
        perror("open");
        return 3;
    }

    info = localtime( &time );
    
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S %Z", info);
    snprintf(buffer, sizeof(buffer),"%s %.2lf\n", timeBuf,distance); 
    
    rc = write (Output_fd, &buffer,strlen(buffer));
    close (Output_fd);

    return rc;
}

// -------------------------------------------------------------------------
// main
int main(void) {
	//  struct sample_data *plastsamples=NULL,*pheadsamples,*pcurrsamples;
	struct sample_data *plastsamples,*pcurrsamples,*avgRunner; 
	sem_init(&calculation,1,0);
	double avg,sum,lastrunningavg=0;
	time_t starttime,finishtime;;

	starttime=time(NULL);

	/* set up PIO's and configure interrupt */
	SetupIO();

	/* set up ring buffer for running avg calc */
	pcurrsamples= plastsamples=SetupRingBuffer(RUNNING_AVG_SAMPLES);

	SaveSample(time(NULL), MeasureDistance() );

	// display counter value every second.
	while ( 1 ) {
		int i;
		double percentChange;
		pcurrsamples->distance=MeasureDistance();
#if DEBUG
		printf("Current Distance: %.2lf\n",pcurrsamples->distance);
#endif
		pcurrsamples->time = time(NULL);
#if 1 // DEBUG

		if (lastDistance) 
			percentChange = 100*(pcurrsamples->distance-lastDistance)/lastDistance; 
#endif		
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
#if DEBUG
		{
			double runningavgpct;
			if (lastrunningavg)
				runningavgpct =100* (lastrunningavg-avg)/lastrunningavg;   

			printf("running average: %.2lf running avg percent: %.2lf%%\n",avg,runningavgpct);
		}
#endif
		lastrunningavg=avg;

		finishtime=time(NULL);

		if ((int)difftime(finishtime,starttime) > min_sample_store_period )
		{
			starttime=time(NULL);
            SaveSample(pcurrsamples->time, pcurrsamples->distance);
			//make sure we block until sample saved	
		}
		else if (abs(percentChange) > CHANGE_THRESHOLD_PERCENT)
		{
            SaveSample(pcurrsamples->time, pcurrsamples->distance);
		}
		/* Update sample data */
		pcurrsamples = pcurrsamples->next;
		delay(SAMPLE_PERIOD_ms);
	}
	// jwd - this will never execute; proper way to free when Ctrl-C?
	if(plastsamples) free(plastsamples);

	return 0;
}
