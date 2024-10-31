#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#define INITIAL_SIZE 2 
#define NANOSECOND_CONVERSION 1e9

//initialize global queue pointers
//4 queues - low priority west, high priority west, low priority east, high priority east
/*
struct Train *lW_head = NULL;
struct Train *hW_head = NULL;
struct Train *lE_head = NULL;
struct Train *hE_head = NULL;*/

//mutexes and conditions for queue access and track access
pthread_mutex_t lock_queue;
pthread_mutex_t lock_track;
//pthread_cond_t train_ready_cond = PTHREAD_COND_INITIALIZER;
//pthread_cond_t track_ready_cond = PTHREAD_COND_INITIALIZER;
//int off_track = 0; track_in


//mutex and conditions for broadcast to start loading
pthread_mutex_t start_loading = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t start_cond = PTHREAD_COND_INITIALIZER;
int start = 0;

struct timespec start_time = {0};


//Train structure definition
typedef struct Train{
    int id;
    int load_time;
    int cross_time;
    char *direction;//cond
    char *priority;
    pthread_cond_t status; //is this status?
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

    new_train->id = id; //from count i in dispatcher_routine()
    new_train->load_time = load;
    new_train->cross_time = cross;
    
    if(isupper(direction[0])){
        new_train->priority = "h";
    } else {
        new_train->priority = "l";
    }
    
    if(tolower(direction[0]=='e')){
        new_train->direction = "East";
    } else {
        new_train->direction = "West";
    }  

    new_train->status =  pthread_cond_init(&status, NULL);
    new_train->off_track = 0;
    new_train->dispatch_signal = 0;
    new_train->next = NULL;
    return new_train;
}

//Queue structure definition
typedef struct Queue{
    Train *head;
    Train *tail
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

void enqueue(Queue *q, Train *train){
    if (q->tail == NULL){
        q->head = train;
        q->tail = train;
    } else {
        q->tail->next = train;
        q->tail = train;
    }
}

Train* dequeue(Queue *q, Train *train){
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

void *train_routine(void *arg){
    
    pthread_mutex_lock(&start_loading);

    //wait until dispatcher signals to start loading
    while (start == 0){
        pthread_cond_wait(&start_cond, &start_loading);
    }
    
    pthread_mutex_unlock(&start_loading);
    //thread work
    Train *cur_train = (Train *)arg;
    int load_sec = cur_train->load_time / NANOSECOND_CONVERSION;
    
    //sleep(load_sec);
    

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
            enqueue(cur_train, lE);
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
    

    //attempt to lock track
    pthread_mutex_lock(&lock_track);
    
    //wait for signal TO SPECIFIC TRAIN from dispatcher 
    while(!cur_train->dispatch_signal){
        pthread_cond_wait(&cur_train->status, &lock_track); //lock_track?
    }
    //critical section
    
    //usleep(crossing_time)
    cur_train->off_track = 1;

    //signal to dispatcher that train has crossed
    pthread_cond_signal(&cur_train->status);
    
    pthread_mutex_unlock(&lock_track);


    //destroy thread


}

void select_train(Queue *west, Queue *east, Train **next_train, int *trains_crossed, char *last_crossed){
    //if both arent NULL:   
    if (west->head != NULL && east->head != NULL){
        if(strcmp(last_crossed, "e")==0){
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
        
    } else {
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
        return 1;
    }
    
    //read from input file - while file line matches format %s %d %d
    while (fscanf(file, "%s %d %d", direction, &load, &cross) == 3){


        //check array_size for array of train ids
        if (num_trains >= array_size){
            array_size *= 2;
            trains = realloc(trains, array_size * sizeof(pthread_t));
        }

        if (threads == NULL) {
            // error in realloc
            return 1;
        }
        //create train
        train *new_train = create_train_struct(direction, load, cross, num_trains);
        
        //create thread
        if (pthread_create(&trains[num_trains], NULL, train_routine, new_train)){
            //error creating thread
            return 1;
        }
        num_trains++;
    }
    //get start time
    clock_gettime(CLOCK_MONOTONIC, &start_time); 
    /* this will be used to calculate differences
    eg:
    simulation time =  timespec_to_sec(&load_time) - timespec_to_sec(&start_time)
    */
    //broadcast threads to start loading
    load_trains();

    //wait for a train to be ready - have a while loop and check all queues

    //put this in a void function? might need to pass time as an arg
    int trains_crossed = 0;
    char last_crossed = 'e' //since no trains have crossed yet
    Train *next_train = NULL;
    

    while(trains_crossed != num_trains){
        //first check high priority queues.
        pthread_mutex_lock(&lock_queue);

        if (hW != NULL || hE != NULL){ 
            select_train(hW, hE, &next_train, &trains_crossed, &last_crossed);
       
        } else if (lW != NULL || lE != NULL){
            select_train(lW, lE, &next_train, &trains_crossed, &last_crossed);

        }         
        pthread_mutex_unlock(&lock_queue);
       
        if (next_train!=NULL) { //if we entered one of the if statements above and dequeued a train
            
            //signal next_train
            next_train->dispatch_signal = 1;
            pthread_cond_signal(&next_train->status);
            
            //wait for train to finish crossing
            while(!next_train->off_track){
                pthread_cond_wait(&next_train->status, &lock_track); //if we need a mutex for writing to output file, pass that mutex here?
            }
            //critical section
            pthread_mutex_unlock(&lock_track);
            //critical section end
        }
        
    }

    

    //wait for all trains to complete - should work after loadtrains be in here?
    for(int i = 0; i<num_trains; i++){
        pthread_join(trains[i], NULL);
    }

}

/* moved to dispatcher thread
int read_file(char *filename){
    //this function reads from input file and creates threads

    //pthread_t train; //declare thread ids - NEEDS to be array of the size of number of threads we want to create
    //how to get this number without reading thru file twice? assume large number? malloc?
    //use malloc and realloc.     
}
*/


int main(int argc, char *argv[]){
    int num_trains;
    //takes one argument from command line: input_file_name.txt
    if (argc != 2){
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
    pthread_mutex_init(&lock_queue);
    pthread_mutex_init(&lock_track);
    
    //create dispatcher
    pthread_create(&dispatcher, NULL, dispatch_routine, filename);

    //destroy mutexes
    pthread_mutex_destroy(&lock_queue);
    pthread_mutex_destroy(&lock_track);
    pthread_mutex_destroy(&start_loading);

    //destroy condvars
    return 0;
}

//dont forget to free and close file