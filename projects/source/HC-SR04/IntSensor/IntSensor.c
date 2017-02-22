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

    gcc -o isr4pi isr4pi.c -lwiringPi

Run as follows:

    sudo ./isr4pi

 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <sys/time.h>
#include <semaphore.h>
#include <time.h>

// Use GPIO Pin 17, which is Pin 0 for wiringPi library
#define TRIGGER 0
// Use GPIO Pin 27, which is Pin 2 for wiringPi library
#define ECHO 2
sem_t calculation;

#define FILE_NAME "/mnt/nfs/home/share/sump/sump_log1.txt"

struct sample_data {
   double distance;
   double time;
   struct sample_data *next;
};
int sample_counter = 0;

// defines that should become variables
#define SAMPLE_PERIOD_ms 2000
#define TRIGGER_DURATION_MS 10
#define RUNNING_AVG_SAMPLES 5
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
// -------------------------------------------------------------------------
// main
int main(void) {
 
  struct sample_data *plastsamples=NULL,*pheadsamples,*pcurrsamples;
 struct sample_data *avgRunner; 
 int i; 
  sem_init(&calculation,1,0);
   double avg,sum,lastrunningavg=0,runningavgpct=0;
  // sets up the wiringPi library
  if (wiringPiSetup () < 0) {
      fprintf (stderr, "Unable to setup wiringPi: %s\n", strerror (errno));
      return 1;
  }

 /* configure trigger pin as output, init value to low */ 
 pinMode(TRIGGER,OUTPUT);
  digitalWrite(TRIGGER,0);

  // set Pin 17/0 generate an interrupt on both edges. handler will process each edge
  // and attach myInterrupt() to the interrupt
  if ( wiringPiISR (ECHO, INT_EDGE_FALLING|INT_EDGE_RISING, &myInterrupt) < 0 ) {
      fprintf (stderr, "Unable to setup ISR: %s\n", strerror (errno));
      return 1;
  }
 plastsamples = (struct sample_data*)malloc(sizeof(struct sample_data) *RUNNING_AVG_SAMPLES );
  pheadsamples = pcurrsamples = plastsamples;
  
  for (i=1;i<RUNNING_AVG_SAMPLES;i++)
  {
    pcurrsamples->next = pcurrsamples+1;
    pcurrsamples = pcurrsamples->next;
  }
  pcurrsamples->next=pheadsamples;
  pcurrsamples = pheadsamples; 
 
  // display counter value every second.
  while ( 1 ) {
   long timediff;
    double distance,percentChange;
 
    digitalWrite(TRIGGER,1);
    delayMicroseconds(TRIGGER_DURATION_MS);
    digitalWrite(TRIGGER,0);
    sem_wait(&calculation);
/* calculate distance, may move to function */
    timediff = timevaldiff(&t1,&t2);
   distance = timediff * 17./1000.;
   if (lastDistance) 
         percentChange = (distance-lastDistance)/lastDistance; 
   lastDistance=distance;
   printf("distance: %.2lf change: %.2lf\%\n",distance,percentChange*100);
/* end calc distance */
    pcurrsamples->distance = distance;
    pcurrsamples->time = time(NULL); 
    pcurrsamples = pcurrsamples->next;
  sum=0; 
  avgRunner = pcurrsamples;
for (i=0;i<RUNNING_AVG_SAMPLES;i++)
  {
    sum+=avgRunner->distance;
    avgRunner=avgRunner->next;
}
   avg=sum/RUNNING_AVG_SAMPLES;
   if (lastrunningavg)
   runningavgpct =100* (lastrunningavg-avg)/lastrunningavg;   

   printf("running average: %.2lf running avg percent: %.2lf\%\n",avg,runningavgpct);
   lastrunningavg=avg;

 //timestamp();

    delay(SAMPLE_PERIOD_ms);
  }

  if(plastsamples) free(plastsamples);

  return 0;
}
