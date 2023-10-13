#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
//structure for process control block
struct PCB {
    int occupied; 
    pid_t pid; 
    int start_seconds;
    int start_nano; 
};
struct PCB process_table[20];
/* Here is where we update the process table with the relevant child data. At first I was only having 
   the function take in pid, but becasue *seconds_ptr and *nano_ptr are not intialized in this scope
   I had to make them parameters as well, but that did not cause any issues. This will loop through 
   the process_table and checking to see if a spot is available or un-occupied. If it is then we set 
   occupied to 1 indicating it is occupied, we set pid to the pid we get from our parameters. In our 
   case that will be the fork_pid and we will call this function in the parent process to get the 
   childs PID. Then we get starting seconds and starting nanoseconds from our parameters as well.
   Becasue we are using waitpid a non-blocking wait the parent process is free to do other things
   which is why our starting times should be pretty accurate as soon as a fork is called the parent
   increments two variables and then updatesPCB so it should be the starting time. Then there is a 
   break becasue we care about finding the first open slot and as soon as that happens we don't 
   need to iterate through the rest of the table.
   */
void update_PCB(pid_t pid, int *seconds_ptr, int *nano_ptr) {
    for (int i = 0; i < 20; i++) {
        if (!process_table[i].occupied) {
            process_table[i].occupied = 1;
            process_table[i].pid = pid;
            process_table[i].start_seconds = *seconds_ptr;
            process_table[i].start_nano = *nano_ptr;
            break;
        }
    }
}
/*When a child has terminated we want to set occupied flag to 0 to indicate it is avaible to be used
  and then we also set the rest to zero becsue there is no longer a process in there.*/
void updatePCBOnTermination(pid_t pid) {
    for (int i = 0; i < 20; i++) {
        if (process_table[i].pid == pid) {
            process_table[i].occupied = 0;
            process_table[i].pid = 0;
            process_table[i].start_seconds = 0;
            process_table[i].start_nano = 0;
        }
    }
}
/*This is printing the process table it also takes in the *seconds_ptr and *nano_ptr as parameters
   and then prints out the table in a nice format*/
void print_PCB(int *seconds_ptr, int *nano_ptr) {
    printf("OSS PID:%d SysClockS: %d SysclockNano: %d\n", getpid(), *seconds_ptr, *nano_ptr);
    printf("Process Table:\nEntry Occupied PID StartS StartN\n");
    for (int i = 0; i < 20; i++) {
        printf("%d %d %d %d %d\n", i, process_table[i].occupied, process_table[i].pid, process_table[i].start_seconds, process_table[i].start_nano);
    }
}

//This is the help message 
void help(){
        printf("Please use this template for running the program: oss [-h] [-n proc] [-s simul] [-t time limit]
        the program only takes in positive integer values. -n is how many child processes you want to create
        in total, -s is how many processes can be ran concurrently/simultaneously and -t is the amount of 
        seconds that will pass before a process is terminated");
}
/*I got some help from these two websites for the signal handeling https://gist.github.com/pbosetti/2c81425fc53b02b4b6ce
  and https://linuxhint.com/sigalarm_alarm_c_language/ anyway this is a timeout flag for my signals  */
int timeout = 0;
/*This is the handler function when a signal goes off this function will be called and it sets out timeout
  flag to 1 which we will use later in this code. Also, I did not know this but having the \n at the end 
  of my print statements pretty much guarentees that it will print before terminating without them I could
  not see them output to my screen. */
void handler(int sig){
    if (sig == SIGALRM){
        printf("Terminating becasue 1 minute has passed\n");
        timeout = 1;
    } else if (sig == SIGINT) {
        printf("Terminating becasue ctrl-c was pressed\n");
        timeout = 1;
    } 
}

int main(int argc, char *argv[]) {
    //Intializing varaibles that will hold out command line arguments 
    int children = 0, simulations = 0, time_limit = 0, opt;
    //seeding random number generator 
    srand(time(NULL));
    /*This is where we are parsing our command line using getopt. This is basically the exact same as 
      project 1 but -t is now time_limit instead of iterations user input is converted to integer 
      becasue the command line takes them in as strings*/
    while ((opt = getopt(argc, argv, "hn:s:t:")) != -1){
        switch(opt) {
            case 'h':
               help();
               exit(0);
            case 'n':
                children = atoi(optarg);
                break;
            case 's':
                simulations = atoi(optarg);
                break;
            case 't':
                time_limit = atoi(optarg);
                break;
            default:
                help();
                exit(1);
        }
    }



    //These are two keys with random numbers I chose these will be used to create our shared memory spaces
    const int seconds_key = 5396211;
    const int nano_key = 2798414;
    /* I basically used exactly what you gave us on canvas but I will still explain. Here we are setting 
       our shmid_seconds variable to shmget(...) this is actually creating the shared memory space for 
       our seconds variable. We use our key as the first parameter that we created above for a space 
       in memory then we initialize the size of that memory to be able to hold an integer. Then we 
       are actually creating the shared memory space with IPC_CREAT and 0666 is giving us reading 
       and writting permissions to this shared memory*/
    int shmid_seconds = shmget(seconds_key, sizeof(int), IPC_CREAT|0666);
    if (shmid_seconds <= 0){
        printf("Shared memory failed for seconds");
    }
    //The same as above but for nanoseconds
    int shmid_nano = shmget(nano_key, sizeof(int), IPC_CREAT|0666);
    if (shmid_nano <= 0){
        printf("Shared memory failed for nano");
    }
    /*Now we are attaching to our shared memory space for seconds with a pointer so that we can actaully 
      work with the data in that space. We use shmat to attach so the first parameter is the id for 
      the shared memory space that we want to attach to then the first 0 is just tellin out system to 
      pick a suitable address in memory and the second 0 specifies flags but it is zero so nothing is 
      set for that. Again I used your example for the shared memory part of this project*/
    int *seconds_ptr = shmat(shmid_seconds, 0, 0);
    if (seconds_ptr <= 0){
        printf("Failed to attach to seconds shared memory");
    }
    //same as about but for nanoseconds 
    int *nano_ptr = shmat(shmid_nano, 0, 0);
    if (nano_ptr <= 0){
        printf("Failed to attach to nano shared memory");
    }

    /* The first two lines here are going to be used to call our handler when one of these signals is recieved
       the third line is setting our alarm to 60 seconds. So after 60 seconds there will be a SIGALRM signal 
       which will invoke our handler function setting out timeout variable to 1 and we will see further down
       what that means.*/
    signal(SIGALRM, handler);
    signal(SIGINT, handler);
    alarm(60);
    //Here we are intializing our clock to 0 for both seconds and nanoseconds
    *seconds_ptr = 0;
    *nano_ptr = 0;
    //intializing more variable these are used to keep track of our workers
    int total_workers_launched = 0, active_workers = 0;
    /*This is where all the logic for running the processes happens. We start with a while loop that 
     will continue to execute as long as the arguments below are kept. This makes sense becasue 
     children is holding the input for our -n command which dictates how many children/workers
     we will be creating. So, if total_workers_launched is equal to children then we stop creating
     new workers or we will stop if the number of active_workers hits zero becasue there should always be 
     some process running while our criteria has not been met yet and we want to make sure that a process
     does not get cut off.*/
    while (total_workers_launched < children || active_workers > 0) {
        /*When one of our signals goes off it will set timeout = 1. 1 in C is equivalent to true
          so, if true then we will iterate through our process table looking for spots that are occupied 
          if it is occupied we will retireve the PID and kill the process using SIGTERM. I read that SIGTERM
          is better to use than SIGKILL so that is why I am using it. I also was looking up how to kill all
          child processes and one way a lot of people were saying to keep track of child PID's in an array
          well there is not point in creating a seperate array when our table is already holding the PID's 
          for our processes.*/
        if (timeout){
            for (int i = 0; i < 20; i++){
                if (process_table[i].occupied){
                    KILL(process_table[i].pid, SIGTERM);
                }
            }
            //Still within the if statement we will detach our pointers from shared memory and remove our shared memory
            shmdt(seconds_ptr);
            shmdt(nano_ptr);
            shmctl(shmid_seconds, IPC_RMID, NULL);
            shmctl(shmid_nano, IPC_RMID, NULL);
        }   /*Here is where I increment the clock. This website helped me with the logic of it https://www.codingninjas.com/studio/library/digital-clock-using-c
              First we increment our nanoseconds to 200,000. I started out incrementing by 50,000,000 but that
              was making the program run almost instantaneously. For me 200,000 was a somewhat realistic speed.
              Then if nanseconds reaches 1 billion we will add 1 to seconds and subtract a billion from nanoseconds
              because a billion nanoseconds equals 1 second.*/
            (*nano_ptr) += 200000;
            if((*nano_ptr) >= 1000000000){
                    (*seconds_ptr) += 1;
                    (*nano_ptr) -= 1000000000;
                }
            /*If the value of nano_ptr is divisible by 500,000,000(half a second) then we will print out our table*/
            if(*nano_ptr % 500000000 == 0){
                   print_PCB(seconds_ptr, nano_ptr);//taking in seconds_ptr and nano_ptr as our parameters
           }

           /*The if statement holds the logic for our -s command on whther we can launch
             more workers concurrently. If the current number of active_workers is less than simulations which
             holds our -s input than we can run more workers if it is equal then we cannot. And again we want 
             to make sure the total_workers_launched is less than our children holding the value of -n input.
             Then we create a variable rand_seconds which will hold a random value between 1 and our user input
             for -t command line argument. rand_nano is picking a random number between 0 and a billion.
             We need to convert the values created from rand_seconds and rand_nano to strings. We create two 
             variables to hold strings and then we convert the value held in rand_second and rand_nano to
             strings and storing them in our new string variables second_string and nano_string %d is 
             telling it to convert to a integer string. we do this because execlp only accepts them as strings*/
            if (active_workers < simulations && total_workers_launched < children){
                int rand_seconds = (rand() % time_limit) + 1;
                int rand_nano = rand() % 1000000000;
                char second_string[10];
                char nano_string[15];
                sprintf(second_string, "%d", rand_seconds);
                sprintf(nano_string, "%d", rand_nano);
                /
                pid_t fork_pid = fork();
                if (fork_pid == 0) { //Then we are working with child process
                        //This is sending our shared variables holding seconds and nanoseconds to worker.c
                        execlp("./worker", "worker", second_string, nano_string, NULL);
                        perror("execlp has failed");//if execlp fails for some reason perror will give us a descriptive error message
                        exit(1);
                } else if (fork_pid > 0) {//In parent process
                       //We increment active_workers and total_workers_launched to keep track of our processes
                       total_workers_launched++;
                       active_workers++;
                       //We also update our PCB table here taking in the child pid, seconds_ptr and nano_ptr
                       update_PCB(fork_pid, seconds_ptr, nano_ptr);
                } else {
                       perror("fork has failed");//If fork fails we will get a descriptive error message
                       exit(1);
                }
           }
           /*waitpid is system call that waits
             for a child process to end. -1 mean it is waiting for any child process to end. NULL would keep
             the termination status of child but it is NULL so it does nothing. WNOHANG is a flag that 
             ensures that the parent process will not be blocked while a child process is active. This 
             is neccessary becasue we are keeping track of our workers in the parent process. If wait_pid
             is greater than zero then a child process has ended and we will decrement our active_workers*/
           pid_t wait_pid = waitpid(-1, NULL, WNOHANG);
           if (wait_pid > 0) {
                   update_PCB_on_termination(wait_pid);//Here we are clearing out the data a process was holding after a child has terminated 
                   active_workers--;
           }


        }
    //again this is detaching our pointers from shared memory then removing the shared memory from the system 
    shmdt(seconds_ptr);
    shmdt(nano_ptr);
    shmctl(shmid_seconds, IPC_RMID, NULL);
    shmctl(shmid_nano, IPC_RMID, NULL);

    return 0;

}