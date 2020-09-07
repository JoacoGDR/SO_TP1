// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <pthread.h>

#define BUFFER_SIZE 12000
#define SPECIAL_SYMBOL '$'

static void run_and_check_error(int error, char message[], int retval);

int main(int argc, char * argv[]){
    int files_to_print;
    if(argc == 2){
        //This means the process has been ran seperately since it has received an argument.
        //Which should obviously be the amount of files to process.
        files_to_print = atoi(argv[1]);
    }
    else if(argc == 1){
        //This means the process has been ran piped. 
        //The solve process has a printf with the ammount of files it received to process. 
        scanf("%d", &files_to_print);
    }
    else{
        //It wouldn't make sense to have received more than two arguments.
        perror("Error! Must receive only one argument!\n");
        exit(EXIT_FAILURE);
    }

    //I open the shared memory and the semaphores that should have been created by the solve process
    int buffer_fd;
    char * buffer;
    run_and_check_error(buffer_fd = shm_open("/shared_memory", O_RDWR  , 0666), "Error when opening shm", -1); 
    run_and_check_error(ftruncate(buffer_fd, BUFFER_SIZE * sizeof(char)), "Error when setting size of shm", -1); 
    if((buffer = mmap(NULL, BUFFER_SIZE * sizeof(char), PROT_WRITE, MAP_SHARED, buffer_fd, 0)) == (void *) -1 ){
        dprintf(STDERR_FILENO, "Error when doing mmap on shared memory\n");
        exit(EXIT_FAILURE);
    }
    sem_t * access_shm;
    if((access_shm = sem_open("access_shm", O_RDWR)) == SEM_FAILED){
        dprintf(STDERR_FILENO, "Error when opening access_shm sem\n");
        exit(EXIT_FAILURE);
    }
    sem_t * file_counter;
    if((file_counter = sem_open("file_counter", O_RDWR)) == SEM_FAILED){
        dprintf(STDERR_FILENO, "Error when opening access_shm sem\n");
        exit(EXIT_FAILURE);
    }

    int i = 0;
    //The process should be running for as long as I have files to print
    while(files_to_print){
        //I wait for the solve process to post the processed files on the shared memory.
        //The file counter symbolizes the UNREAD files that are on the shm.
        sem_wait(file_counter);

        //ONLY ONE PROCESS SHOULD BE READING/WRITING ON THE SHARED MEMORY AT A TIME. 
        sem_wait(access_shm);
        
        //I don't want to print the WHOLE buffer, I only want to print what has just recently been uploaded to it. 
        for (; buffer[i] != '\0'; i++){
            printf("%c", buffer[i]);
        }
        printf("\n");

        sem_post(access_shm);
        files_to_print--;
    }
    return 0;
}

static void run_and_check_error(int error, char message[], int retval) {
    if(error == retval) {
        dprintf(STDERR_FILENO, "%s\n", message);
        exit(EXIT_FAILURE);
    }
}

