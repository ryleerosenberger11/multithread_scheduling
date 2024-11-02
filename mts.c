#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>
#include <sched.h>

#define INITIAL_SIZE 2 
#define NANOSECOND_CONVERSION 1e9
#define MICRO_CONV 100000


//mutexes and conditions for queue access and track access and output file access
pthread_mutex_t lock_queue;
pthread_mutex_t lock_track;
pthread_mutex_t lock_file;


//mutex and conditions for broadcast to start loading
pthread_mutex_t start_loading = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t start_cond = PTHREAD_COND_INITIALIZER;
int start = 0;

struct timespec start_time = {0};
FILE *output;

//Train structure definition
typedef struct Train{
    int id;
    int load_time;
    int cross_time;
    char *direction;
    char *priority;
    pthread_cond_t status; 
    int off_track;
    int dispatch_signal;
    struct Train *next;
} Train;

Train* create_train_struct(char *direction, int load, int cross, int id){
    Train *new_train = (Train *)malloc(sizeof(Train));
    
    //test if malloc fails
    if(!new_train){
        exit(1);
    }
    printf("train %d created\n", id);
    new_train->id = id; //from count i in dispatcher_routine()
    new_train->load_time = load;
    new_train->cross_time = cross;
    
    if(isupper(direction[0])){
        new_train->priority = "h";
    } else {
        new_train->priority = "l";
    }
    
    if(tolower(direction[0])=='e'){
        new_train->direction = "East";
    } else {
        //printf("%d\n", direction[0]);
        new_train->direction = "West";
    }  

    pthread_cond_init(&(new_train->status), NULL);
    new_train->off_track = 0;
    new_train->dispatch_signal = 0;
    new_train->next = NULL;

    return new_train;
}

//Queue structure definition
typedef struct Queue{
    Train *head;
    Train *tail;
} Queue;

Queue* create_queue(){
    Queue* q = (Queue*)malloc(sizeof(Queue));
    q->head = NULL;
    q->tail = NULL;
    return q;
}

//global queues
Queue *lW, *hW, *lE, *hE;

//timespec conversion
double timespec_to_sec(struct timespec *ts){
    return ((double)ts->tv_sec) + (((double) ts->tv_nsec) / NANOSECOND_CONVERSION);
}

void load_trains(){
    pthread_mutex_lock(&start_loading);
    start = 1;
    pthread_cond_broadcast(&start_cond);
    pthread_mutex_unlock(&start_loading);
}

void enqueue(Train *train, Queue *q){
    if (q->tail == NULL){
        q->head = train;
        q->tail = train;
    } else {
        q->tail->next = train;
        q->tail = train;
    }
}

Train* dequeue(Queue *q){
    if (q->head == NULL){
        printf("queue is empty\n");
        return NULL;
    } 
    
    Train *removed = q->head;
    q->head = q->head->next;

    if(q->head == NULL){
        //then q is now empty
        q->tail = NULL;
    }
    return removed;
}

void print_simulation_time(struct timespec start_time, FILE *output){
    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time); //get cur timestamp
    
    time_t seconds_difference = cur_time.tv_sec - start_time.tv_sec;
    long nano_difference = cur_time.tv_nsec - start_time.tv_nsec;

    // case when nanoseconds of current is less than start's
    if (nano_difference < 0) {
        seconds_difference -= 1;
        nano_difference += NANOSECOND_CONVERSION;
    }

    int hours = seconds_difference / 3600;
    int min = (seconds_difference%3600) / 60;
    int sec = seconds_difference%60;
    int tenths = nano_difference / (NANOSECOND_CONVERSION/10);

    fprintf(output, "%02d:%02d:%02d.%d ", hours, min, sec, tenths);
}

void free_train(Train *train){
    pthread_cond_destroy(&(train->status));
    free(train);
}

void *train_routine(void *arg){
    Train *cur_train = (Train *)arg;

    //load the train
    pthread_mutex_lock(&start_loading);

    //wait until dispatcher signals to start loading
    while (start == 0){
        pthread_cond_wait(&start_cond, &start_loading);
        printf("waiting...\n");
    }
    
    pthread_mutex_unlock(&start_loading);

    //sleep for load time
    int load_tenth_sec = cur_train->load_time; 
    usleep(load_tenth_sec*MICRO_CONV);
    
    //loading done - output that train is ready
    pthread_mutex_lock(&lock_file);

        print_simulation_time(start_time, output);
        fprintf(output, "Train %2d is ready to go %4s\n", 
                cur_train->id, 
                cur_train->direction);

   pthread_mutex_unlock(&lock_file);

    //add the train to its respective queue
    
    //attempt to lock q mutex first
    pthread_mutex_lock(&lock_queue);
    
        //critical section
        if(strcmp(cur_train->direction, "East") == 0){
            if(strcmp(cur_train->priority, "h") == 0){
                //add to hE
                enqueue(cur_train, hE);
            }else{
                //add to lE
                printf("adding to lE\n");
                enqueue(cur_train, lE);
                printf("%s - lE head direction\n", lE->head->direction);
            }
        }else{
            if(strcmp(cur_train->priority, "h") == 0){
                //add to hW
                enqueue(cur_train, hW);
            }else{
                //add to lW
                enqueue(cur_train, lW);
            }
        }
    pthread_mutex_unlock(&lock_queue);

    //attempt to lock file - if we lock file here then that means we have track access

    pthread_mutex_lock(&lock_file);
    //wait for signal TO SPECIFIC TRAIN from dispatcher 
    while(!cur_train->dispatch_signal){
        printf("wait..\n");
        pthread_cond_wait(&(cur_train->status), &lock_file); //lock_track?
    }
    //critical section (lock_file acquired)
        printf("signal for train %d acquired\n", cur_train->id);

        print_simulation_time(start_time, output);
        fprintf(output, "Train %2d is ON the main track going %4s\n", 
                cur_train->id, 
                cur_train->direction); 

    pthread_mutex_unlock(&lock_file);
/*
    pthread_mutex_lock(&lock_track);
    //wait for signal TO SPECIFIC TRAIN from dispatcher 
    while(!cur_train->dispatch_signal){
        printf("wait..\n");
        pthread_cond_wait(&(cur_train->status), &lock_track); //lock_track?
    }
    //critical section (lock_track acquired)
        printf("signal for train %d acquired\n", cur_train->id);

        pthread_mutex_lock(&lock_file);

        print_simulation_time(start_time, output);
        fprintf(output, "Train %2d is ON the main track going %4s\n", 
                cur_train->id, 
                cur_train->direction); 

        pthread_mutex_unlock(&lock_file); */
              
    //cross the track
    pthread_mutex_lock(&lock_track);
    int cross_tenth_sec = cur_train->cross_time; 
    usleep(cross_tenth_sec*MICRO_CONV);
    pthread_mutex_unlock(&lock_track);
    //Print:
    //TIME Train cur_train->id is OFF the main track after going cur_train->direction
    cur_train->off_track = 1;
    cur_train->dispatch_signal = 0;

    //order of following two statements?
    //signal to dispatcher that train has crossed
    //pthread_mutex_unlock(&lock_track);
    pthread_cond_signal(&(cur_train->status));

    pthread_mutex_lock(&lock_file);

        print_simulation_time(start_time, output);
        fprintf(output, "Train %2d is OFF the main track after going %4s\n", 
                cur_train->id, 
                cur_train->direction);

    pthread_mutex_unlock(&lock_file);

    free_train(cur_train);

    pthread_exit(NULL);


}

void select_train(Queue *west, Queue *east, Train **next_train, int *trains_crossed, char *last_crossed){
    //if both arent NULL:   
    if (west->head != NULL && east->head != NULL){
        if(*last_crossed == 'e'){
            *next_train = dequeue(west);
            *last_crossed = 'w';
            
        } else {
            *next_train = dequeue(east);
            *last_crossed = 'e';
            
        }
    //else dequeue the one that isn't NULL

    }else if (west->head!=NULL){
        *next_train = dequeue(west);
        *last_crossed = 'w';
        
    } else if (east->head!=NULL){
        *next_train = dequeue(east);
        *last_crossed = 'e';
        
    }
    (*trains_crossed)++;
}

void *dispatch_routine(void *arg){
    /*  this routine reads from file specified when program was run. 
        as it reads a train from the file, it creates an individual train thread
        it then broadcasts all trains to start loading
        as trains signal that they are ready, it signals which trains can cross the track.
        it continues to do this until all trains have crossed.
    */
    //get filename form *arg by casting void ptr to char ptr
    char* filename = (char*) arg;
    
    int array_size = INITIAL_SIZE;
    pthread_t* trains = malloc(array_size * sizeof(pthread_t)); // Dynamically allocated array for thread IDs
    
    FILE *file;

    char direction[10];
    int load;
    int cross;
    int num_trains = 0;

    file = fopen(filename, "r");

    //check if fopen successful
    if (file == NULL){
        perror("error opening file\n");
        exit(1);
    }
    
    //read from input file - while file line matches format %s %d %d
    while (fscanf(file, "%s %d %d", direction, &load, &cross) == 3){

        //check array_size for array of train ids
        if (num_trains >= array_size){
            array_size *= 2;
            trains = realloc(trains, array_size * sizeof(pthread_t));
        }

        if (trains == NULL) {
            // error in realloc
            exit(1);
        }
        //create train
        Train *new_train = create_train_struct(direction, load, cross, num_trains);
        
        //create thread
        if (pthread_create(&trains[num_trains], NULL, train_routine, new_train)){
            //error creating thread
            perror("error creating train thread\n");
            exit(1);
        }
        num_trains++;
    }
    //get start time
    clock_gettime(CLOCK_MONOTONIC, &start_time); //used to calculate time differences

    //broadcast threads to start loading
    load_trains();

    
    int trains_crossed = 0;
    char last_crossed = 'e'; //since no trains have crossed yet
    Train *next_train = NULL;

    //wait for a train to be ready - have a while loop and check all queues
    while(trains_crossed != num_trains){
        //first check high priority queues.

        usleep(5000); //to allow time for threads that finished loading at same time to be added to queues
        pthread_mutex_lock(&lock_queue);
        
            if (hW->head != NULL || hE->head != NULL){ 
                select_train(hW, hE, &next_train, &trains_crossed, &last_crossed);
        
            } else if (lW->head != NULL || lE->head != NULL){
                printf("found a l train\n");
                select_train(lW, lE, &next_train, &trains_crossed, &last_crossed);
            }         
        pthread_mutex_unlock(&lock_queue);
       
        if (next_train!=NULL) { //if we entered one of the if statements above and dequeued a train
            //signal next_train
            next_train->dispatch_signal = 1;
            pthread_cond_signal(&(next_train->status));
            
            //wait for train to finish crossing
            
            pthread_mutex_lock(&lock_track);
            while(!next_train->off_track){
                pthread_cond_wait(&(next_train->status), &lock_track); //if we need a mutex for writing to output file, pass that mutex here?
            }
            //critical section
            pthread_mutex_unlock(&lock_track);
            //critical section end

            next_train = NULL; // reset for next iteration
        }
        //printf("trains crossed: %d\n", trains_crossed);
        
    }
    //wait for all trains to complete
    for(int i = 0; i<num_trains; i++){
        pthread_join(trains[i], NULL);
    }

    free(trains);
    
    pthread_exit(NULL);

}

int main(int argc, char *argv[]){
    //takes one argument from command line: input_file_name.txt
    if (argc != 2){
        perror("not enough args\n");
        return 1;
    }

    char *filename = argv[1];
    pthread_t dispatcher;

    //initialize 4 queues - low priority west, high priority west, low priority east, high priority east
    lW = create_queue();
    hW = create_queue();
    lE = create_queue();
    hE = create_queue();

    //initialize mutexes
    pthread_mutex_init(&lock_queue, NULL);
    pthread_mutex_init(&lock_track, NULL);
    pthread_mutex_init(&lock_file, NULL);

    //clear output file
    output = fopen("output.txt", "w");
    if (output == NULL) {
        perror("Failed to open output file");
        exit(1);
    }
    fclose(output);
    
    //open for append mode
    output = fopen("output.txt", "a");
    if (output == NULL) {
        perror("Failed to open output file");
        exit(1);
    }
    
    printf("creating dispatcher\n");
    //create dispatcher
    int result = pthread_create(&dispatcher, NULL, dispatch_routine, filename);
    
    if(result!=0){
        perror("error creating dispatcher");
        return EXIT_FAILURE;
    }
    
    pthread_join(dispatcher, NULL);
    fclose(output);

    //destroy mutexes
    pthread_mutex_destroy(&lock_queue);
    pthread_mutex_destroy(&lock_track);
    pthread_mutex_destroy(&start_loading);
    pthread_mutex_destroy(&lock_file);

    //destroy condvars
    pthread_cond_destroy(&start_cond);

    //free queues
    free(lW);
    free(hW);
    free(lE);
    free(hE);

    return 0;
}

//dont forget to free and close file