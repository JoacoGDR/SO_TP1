// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <dirent.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>


#define FILE_LENGTH 100 
#define SPECIAL_SYMBOL '$'
#define SPECIAL_SYMBOL_STRING "$"
#define INITIAL_FILES_TO_GIVE_OUT 2
#define MAX_LENGTH_TO_READ 1000
#define BUFFER_SIZE 100000
#define MAX_FILES_ALLOWED 200

static void run_and_check_error(int error, char message[], int retval);
static void listen_to_slaves(int buffer_pipes[][2], int slavesAmmount, int argc, int * file_index, char * argv[], int file_pipes[][2], char* buffer_ptr,sem_t * sem, sem_t * file_counter);   
static void send_files(int fd, int filesAmmount,int * file_index, char * argv[]);
static void close_pipes(int file_pipes[][2], int buffer_pipes[][2], int slavesAmmount);

int main(int argc, char * argv[]) {
    
    //checks that at least 1 file is sent as parameter
    if(argc-1 <= 0){
        printf("No files registered\n");
        return -1;
    }
    if(argc-1 > MAX_FILES_ALLOWED){
        printf("You can only process up to %d files!\n", MAX_FILES_ALLOWED);
        return -1;
    }

    setvbuf(stdout, NULL, _IONBF, 0);  

    int file_index = 1;
    int files = argc-1;

    //calculating number of slaves to create
    int slavesAmmount = (files)/20 + 1;

    //array to save the pids of the slaves
    int * slaves;
    if((slaves = malloc(sizeof(int) * slavesAmmount)) == NULL){
        dprintf(STDERR_FILENO, "No more space available for memory allocation\n");
        exit(EXIT_FAILURE);
    }

    //creates and initialices shared memory
    int buffer_fd; 
    run_and_check_error(buffer_fd = shm_open("/shared_memory", O_CREAT | O_RDWR  , 0600), "Error when creating shm", -1);  
    run_and_check_error(ftruncate(buffer_fd, BUFFER_SIZE * sizeof(char)), "Error when setting size of shm", -1);  
                                  
    char * buffer;
    if((buffer = mmap(NULL, BUFFER_SIZE * sizeof(char), PROT_WRITE, MAP_SHARED, buffer_fd, 0)) == (void *) -1 ){
        dprintf(STDERR_FILENO, "Error when doing mmap on shared memory\n");
        exit(EXIT_FAILURE);
    }

    //creates the two semaphores:
    //sem used to write and read from shared memory
    sem_t * access_shm;
    if((access_shm = sem_open("access_shm", O_CREAT, 0666, 1)) == SEM_FAILED){
        dprintf(STDERR_FILENO, "Error when creating access_shm sem\n");
        exit(EXIT_FAILURE);
    }
    //sem used to tell view.c when it has a new file to print
    sem_t * file_counter;
    if((file_counter = sem_open("file_counter", O_CREAT | O_RDWR, 0666, 0)) == SEM_FAILED) {
        dprintf(STDERR_FILENO, "Error when creating file_counter sem\n");
        exit(EXIT_FAILURE);
    }

    //output file with all the output read from the slaves
    FILE * result;
    if((result = fopen("result", "w")) == NULL){
        dprintf(STDERR_FILENO, "Unable to open file: result\n");
        exit(EXIT_FAILURE);  
    }
    
    //waits two seconds for view
    sleep(2);
    printf("%d\n", argc-1);
    
    //creates two arrays of which will contain the pipes: 
    int file_pipes[slavesAmmount][2]; //pipes that go from solve to slaves
    int buffer_pipes[slavesAmmount][2]; //pipes that go from slaves to solve

    //creates pipes
    for(int i = 0; i < slavesAmmount; i++) {
        run_and_check_error(pipe(file_pipes[i]), "Error creating file pipe!\n", -1);
        run_and_check_error(pipe(buffer_pipes[i]), "Error creating buffer pipe!\n", -1);
    }

    int pid;
    // Slave generator(creates slavesAmmount slaves)
    for(int i = 0; i < slavesAmmount; i++) {
        run_and_check_error(pid = fork(), "Error forking\n", -1);
        if(pid == 0){
            //Linking STDIN and STDOUT to the ends of our pipes so when the slave tries to output something
            //it is sent to the parent process instead. 
            run_and_check_error(close(fileno(stdin)), "Error closing STDIN\n", -1);
            run_and_check_error(dup(file_pipes[i][0]), "Error dupping file pipe\n", -1); 
            run_and_check_error(close(fileno(stdout)), "Error closing STDOUT\n", -1);       
            run_and_check_error(dup(buffer_pipes[i][1]), "Error dupping buffer pipe\n", -1); 

            //Closing all pipes because we no longer use them. 
            close_pipes(file_pipes, buffer_pipes, slavesAmmount);
            
            //Replacing the entire code with the slave code.
            execv("slave",argv);
        }
        else {   
            //Stores the PID of each slave and sends them the initial files for them to process.
            slaves[i] = pid; 
            if(file_index > argc - INITIAL_FILES_TO_GIVE_OUT){
                send_files(file_pipes[i][1], 1, &file_index, argv);            
            }else{
               send_files(file_pipes[i][1], INITIAL_FILES_TO_GIVE_OUT, &file_index, argv);
            }
        }
    }
    
    //All of this will only be executed by the parent process, since the slaves have replaced their entire code.
    //Reading from slaves and sending files to those who finished. Updating the shared memory as I go.
    listen_to_slaves(buffer_pipes, slavesAmmount, argc, &file_index, argv, file_pipes,buffer, access_shm, file_counter);
    
    //Storing the shared memory on the result file.
    fprintf(result, "%s", buffer);


    close_pipes(file_pipes, buffer_pipes, slavesAmmount);

    //Freeing everything and deleting all the shared memory that was created (such as the semaphores) and closing all pipes.     
    run_and_check_error(shm_unlink("/shared_memory"), "Error when unlinking shared memory\n", -1);
    run_and_check_error(munmap(buffer,BUFFER_SIZE*sizeof(char)), "Error when doing munmap\n", -1);
    run_and_check_error(sem_unlink("access_shm"), "Error when unlinking semaphore\n", -1);
    run_and_check_error(sem_unlink("file_counter"), "Error when unlinking semaphore\n", -1);
    run_and_check_error(sem_close(file_counter), "Error closing semaphore\n", -1);
    run_and_check_error(sem_close(access_shm), "Error closing semaphore\n", -1);
    run_and_check_error(fclose(result), "Could not close result file\n", -1);

    //Killing all the slave processes that might still be running...
    for(int i = 0; i < slavesAmmount; i++) {
        run_and_check_error(kill(slaves[i], SIGKILL), "Could not kill slave!\n", -1);
    }
    free(slaves);
    
    return 0;
}

static void listen_to_slaves(int buffer_pipes[][2], int slavesAmmount, int argc, int * file_index, char * argv[], int file_pipes[][2], char * buffer_ptr, sem_t * sem,sem_t*file_counter) {
    int files_received = 0;

    fd_set all_fd, aux_fd;
    FD_ZERO(&all_fd);
    int retval;
    int max_fd = 0;
     
    //until all files are processed and read by solve
    while(files_received < argc-1) {

        //setting the fd_set to use in select
        for(int i = 0; i < slavesAmmount; i++) {
            if(max_fd < buffer_pipes[i][0]){
                max_fd = buffer_pipes[i][0];
            }
            FD_SET(buffer_pipes[i][0], &all_fd); 
        }

        //we use an auxiliar fd_set because select modifies the fd_set sent as parameter
        aux_fd = all_fd;
        //select used to know from which pipes there is something to read
        retval = select( max_fd + 1, &aux_fd, NULL, NULL, NULL );

        if(retval > 0){
            for(int i = 0; i < slavesAmmount; i++) {
                if(FD_ISSET(buffer_pipes[i][0], &aux_fd)) {
                    char * answer;
                    if((answer = calloc(MAX_LENGTH_TO_READ, sizeof(char))) == NULL){
                        dprintf(STDERR_FILENO, "No more space available for memory allocation\n");
                        exit(EXIT_FAILURE);
                    }
                
                    run_and_check_error(read(buffer_pipes[i][0], answer, MAX_LENGTH_TO_READ), "Could not read files!\n", -1);
               
                    //uses semaphore to access shared memory
                    sem_wait(sem);
                    strcat(buffer_ptr, answer);
                    sem_post(sem);
                    //increases the semaphore used as a file counter that can be printf by view
                    sem_post(file_counter);
                    free(answer);

                    //increases the number of files already processed. When all files are done, the while ends
                    files_received++;

                    //sends a new file to the slave that has no file to process if there are remaining files
                    if(*file_index < argc) {
                        send_files(file_pipes[i][1], 1, file_index, argv);     
                    }
                }
            }
        }
    }
}


static void send_files(int fd, int filesAmmount,int * file_index, char * argv[]) {
    char * file;
    if((file = calloc(FILE_LENGTH, sizeof(char))) == NULL) {
        dprintf(STDERR_FILENO,"No more space available for memory allocation\n");
        exit(EXIT_FAILURE);
    }

    //concats into a single string the names of "filesAmount" of files with a '$' in between of them
    for(int i = 0; i < filesAmmount; i++) {
        strcat(file, argv[*file_index]);
        (*file_index)++;    
        //the special symbol is used to separate the files 
        strcat(file, SPECIAL_SYMBOL_STRING);  
    }

    run_and_check_error(write(fd, file, strlen(file)), "Could not write to file\n", -1);
    free(file);       
}

static void run_and_check_error(int error, char message[], int retval) {
    if(error == retval) {
        dprintf(STDERR_FILENO, "%s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void close_pipes(int file_pipes[][2], int buffer_pipes[][2], int slavesAmmount){
    for(int i = 0; i < slavesAmmount;i++){
        run_and_check_error(close(file_pipes[i][0]), "Could not close pipe!\n", -1);
        run_and_check_error(close(file_pipes[i][1]), "Could not close pipe!\n", -1);
        run_and_check_error(close(buffer_pipes[i][0]), "Could not close pipe!\n", -1);
        run_and_check_error(close(buffer_pipes[i][1]), "Could not close pipe!\n", -1);
    }
}
