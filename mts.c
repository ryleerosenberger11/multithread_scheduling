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

pthread_mutex_t lock_queue;
pthread_mutex_t lock_track;

pthread_mutex_t start_loading = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t start_cond = PTHREAD_COND_INITIALIZER;
int start = 0;

struct timespec start_time = {0};

//timespec conversion
double timespec_to_sec(struct timespec *ts){
    return ((double)ts->tv_sec) + (((double) ts->tv_nsec) / NANOSECOND_CONVERSION);
}

//Train structure definition
typedef struct Train{
    int id;
    int load_time;
    int cross_time;
    char *direction;
    char priority;
    pthread_cond_t cond;
} Train;

Train* create_train_struct(char *direction, int load, int cross, int id){
    Train *new_train = (Train *)malloc(sizeof(Train));
    
    //test if malloc fails
    if(!new_train){
        exit(1);
    }

    new_train->id = id; //from count i in read_file()
    new_train->load_time = load;
    new_train->cross_time = cross;
    
    if(isupper(direction[0])){
        new_train->priority = 'h';
    } else {
        new_train->priority = 'l';
    }
    
    if(strcmp(tolower(direction[0]), 'e'){
        new_train->direction = "East";
    } else {
        new_train->direction = "West"
    }   
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

}
/* moved to main
int read_file(char *filename){
    //this function reads from input file and creates threads

    //pthread_t train; //declare thread ids - NEEDS to be array of the size of number of threads we want to create
    //how to get this number without reading thru file twice? assume large number? malloc?
    //use malloc and realloc.     
}
*/
void load_trains(){
    pthread_mutex_lock(&start_loading);
    start = 1;
    pthread_cond_broadcast(&start_cond);
    pthread_mutex_unlock(&start_loading);
}

int main(int argc, char *argv[]){
    int num_trains;
    //takes one argument from command line: input_file_name.txt
    if (argc != 2){
        return 1;
    }
    //initialize mutexes
    pthread_mutex_init(&lock_queue);
    pthread_mutex_init(&lock_track);

    //read from input and create threads - do this in main so that we have access to pthread_t* trains
    char *filename = argv[1];
    
    int array_size = INITIAL_SIZE;
    pthread_t* trains = malloc(array_size * sizeof(pthread_t)); // Dynamically allocated array for thread IDs
    
    FILE *file;

    char direction[10];
    int load;
    int cross;
    int num_trains = 0;

    file = fopen(filename, r);

    //check if fopen successful
    if (file == NULL){
        return 1;
    }
    
    //read from input file - while file line matches format %s %d %d
    while (fscanf(file, "%s %d %d", direction, &load, &cross) == 3){


        //check array_size for array of train ids
        if (num_trains >= array_size){
            array_size *= 2;
            trains = realloc(threads, array_size * sizeof(pthread_t));
        }

        if (threads == NULL) {
            // error in realloc
            return 1;
        }
        //create train
        train *new_train = create_train_struct(direction, load, cross, i);
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

    //wait for all trains to complete
    for(int i = 0; i<num_trains; i++){
        pthread_join(trains[i], NULL);
    }

    //destroy mutexes
    pthread_mutex_destroy(&lock_queue);
    pthread_mutex_destroy(&lock_track);
    return 0;
}

//dont forget to free and close file