#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#define INITIAL_SIZE 2 

typedef struct train{
    int id;
    int load_time;
    int cross_time;
    char *direction;
    char priority;
    pthread_cond_t cond;
} train;

train* create_train_struct(char *direction, int load, int cross, int id){
    train *new_train = (train *)malloc(sizeof(train));
    
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

void read_file(char *filename){
    //reads from input file and creates threads
    //pthread_t train; //declare thread ids - NEEDS to be array of the size of number of threads we want to create
    //how to get this number without reading thru file twice? assume large number? malloc?
    //use malloc and realloc.
    
    
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
        if (i >= array_size){
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
}
int main(int argc, char *argv[]){
    //takes one argument from command line: input_file_name.txt
    if (argc != 2){
        return 1;
    }
    read_file(argv[1]);
    return 0;
}

//dont forget to free and close file