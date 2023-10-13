#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/msg.h>

typedef struct mssg_buffer {
        long mtype;
        int status;
} mssg_buffer;

int main(int argc, char *argv[]){
    //Here we are setting our variables to arg1 and arg2 which are holding our seconds and nanoseconds
    int time_limit_s = atoi(argv[1]);
    int time_limit_n = atoi(argv[2]);

    key_t mssg_key = 9876550
    if ((int msg_id = msgget(mssg_key, 0666)) == -1) {
         perror("msgget failed");
         exit(1);
    }


    /*This is going to be pretty much the same as what we did in oss.c, but isntead we will not be
    creating shared memory spaces with IPC_CREAT we only want to access them here in worker.c*/
    const int seconds_key = 5396211;
    const int nano_key = 2798414;

    int shmid_seconds = shmget(seconds_key, sizeof(int), 0666);
    if (shmid_seconds <= 0){
        printf("Shared memory failed for seconds");
    }
    int shmid_nano = shmget(nano_key, sizeof(int), 0666);
    if (shmid_nano <= 0){
        printf("Shared memory failed for nano");
    }
    int *seconds_ptr_worker = shmat(shmid_seconds, 0, 0);
    if (seconds_ptr_worker <= 0){
        printf("Failed to attach to seconds shared memory");
    }
    int *nano_ptr_worker = shmat(shmid_nano, 0, 0);
    if (nano_ptr_worker <= 0){
        printf("Failed to attach to nano shared memory");
    }
    /*Here I set start_time and prev_second to our seconds_ptr_worker which hold our seconds from
    shared memory. I will use start_time to help me calcuate the elapsed time for a process which
    you will see below. prev_second will be used as a conditioon in our loop down below to keep track
    of when a second has passed.*/
     int start_time_s = *seconds_ptr_worker;
     int prev_second = *seconds_ptr_worker;
     /*Here we are calculating the actual termination time for seconds and nanoseconds. We are
       adding our shared memory values with our arguments that were passed on to the worker.
       This gives us termination time*/
     int term_time_s = ((*seconds_ptr_worker) + time_limit_s);
     int term_time_n = ((*nano_ptr_worker) + time_limit_n);
      /*I was having trouble with in my print statement because the termination times for both seconds and
        nanoseconds kept looking off and not making any sense and I believe it was becasue they were overflowing
        sometimes. In oss.c we use a similar logic to actually increment our clock but this logic below is
        helping to keep the termination times from overflowing or whatever was going on wrong in my print
        statements. These are local variables to worker.c and will not interfere with our shared variable
        values*/
      while (term_time_n >= 1000000000) {
        term_time_n -= 1000000000;
        term_time_s++;
    }

   //Our intial print statement
    printf("WORKER PID: %d PPID: %d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n --Just Starting\n",
     getpid(), getppid(), *seconds_ptr_worker, *nano_ptr_worker, term_time_s, term_time_n);

    /*Here is the logic for our worker process. We start in a while loop that checks if our shared
      memory value holding seconds is less than our termination time for seconds. Or if our system
      clock seconds equals our termination time seconds and our system clock nanoseconds is less than
      our termination time for nanoseconds we will continue the loop. At first I was just doing
      while(*seconds_ptr == term_time_s && *nano_ptr == term_time_n) This led to some pretty bad
      output becasue it is too precise of a condition to meet and again let to some weird output*/
     while ((*seconds_ptr_worker < term_time_s) || (*seconds_ptr_worker) == term_time_s && (*nano_ptr_worker) < term_time_n){
        if (prev_second < *seconds_ptr_worker){
            int time_elapsed = *seconds_ptr_worker - start_time_s;//calculating the elapsed time
            //This is the message that will print out every second
            printf("WORKER PID: %d PPID: %d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n --%d seconds have passed since starting\n", getpid(), getppid(), *seconds_ptr_worker, *nano_ptr_worker, term_time_s, term_time_n, time_elapsed);
            prev_second = *seconds_ptr_worker;//This is setting the prev_second to the current second to get ready for next iteration
        }

    }
    //terminating message from worker
    printf("WORKER PID: %d PPID: %d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n --Terminating\n", getpid(), getppid(), *seconds_ptr_worker, *nano_ptr_worker, term_time_s, term_time_n);
    //We only need to detach our pointers from shared memory becasue we did not create any in worker.c
    shmdt(seconds_ptr_worker);
    shmdt(nano_ptr_worker);

    return 0;
}

