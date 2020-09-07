#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <math.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/select.h>


#define FILE_LENGTH 100 
#define SPECIAL_SYMBOL '$'
#define SPECIAL_SYMBOL_STRING "$"
#define INITIAL_FILES_TO_GIVE_OUT 2
#define MAX_LENGTH_TO_READ 1000
#define BUFFER_SIZE 100000


static void run_and_check_error(int error, char message[], int retval);

static void read_from_slave(int buffer_pipes[][2], int slavesAmmount, int argc, int * file_index, char * argv[], int file_pipes[][2], char* buffer_ptr,sem_t * sem, sem_t * file_counter);   
static void send_files(int fd, int filesAmmount,int * file_index, char * argv[]);

int main(int argc, char * argv[]) {
    
  

    
    if(argc-1 <= 0){
        printf("No files registered\n");
        return -1;
    }

    setvbuf(stdout, NULL, _IONBF, 0);  

    int file_index = 1;
    int files = argc-1;

    int slavesAmmount = (files)/20 + 1;
    int * slaves = malloc(sizeof(char) * slavesAmmount);

    int buffer_fd; 
    run_and_check_error(buffer_fd = shm_open("/shared_memory", O_CREAT | O_RDWR  , 0600), "Error when creating shm", -1); /* create s.m object*/   
    run_and_check_error(ftruncate(buffer_fd, BUFFER_SIZE*sizeof(char)), "Error when setting size of shm", -1);                                 /* resize memory object */
    char * buffer = mmap(NULL, BUFFER_SIZE*sizeof(char), PROT_WRITE, MAP_SHARED, buffer_fd, 0);

    sem_t * access_shm = sem_open("access_shm", O_CREAT, 0666, 1);
    sem_t * file_counter = sem_open("files_ready_to_print", O_CREAT | O_RDWR, 0666,0);


    sleep(3);
    printf("%d\n",argc-1);
    
    int file_pipes[slavesAmmount][2];
    int buffer_pipes[slavesAmmount][2];

    for(int i = 0; i < slavesAmmount; i++) {
        run_and_check_error(pipe(file_pipes[i]),"Error creating file pipe!\n",-1);
        run_and_check_error(pipe(buffer_pipes[i]),"Error creating buffer pipe!\n",-1);
    }

    int pid;
    // Creo mi cantidad de esclavos.
    for(int i = 0; i < slavesAmmount; i++) {
        run_and_check_error(pid = fork(), "Error forking\n", -1);
        if(pid == 0){
            run_and_check_error(close(fileno(stdin)),"Error closing STDIN\n", -1);
            run_and_check_error(dup(file_pipes[i][0]),"Error dupping file pipe\n", -1); // new STDIN: read-end of master-to-slave pipe N°i
            run_and_check_error(close(fileno(stdout)),"Error closing STDOUT\n", -1);       
            run_and_check_error(dup(buffer_pipes[i][1]),"Error dupping buffer pipe\n", -1); // new STDOUT: write-end of slave-to-master pipe N°i

            for(int i = 0; i<slavesAmmount;i++){
                close(file_pipes[i][0]);
                close(file_pipes[i][1]);
                close(buffer_pipes[i][0]);
                close(buffer_pipes[i][1]);
            }

            execv("slave",argv);
        }
        else {   
            slaves[i] = pid; 
            if(file_index > argc - INITIAL_FILES_TO_GIVE_OUT){
                send_files(file_pipes[i][1], 1, &file_index, argv);            
            }else{
               send_files(file_pipes[i][1], INITIAL_FILES_TO_GIVE_OUT, &file_index, argv);
            }
        }
    }
    read_from_slave(buffer_pipes, slavesAmmount, argc, &file_index, argv, file_pipes,buffer, access_shm, file_counter);
    
    sem_unlink("access_shm");
    sem_unlink("files_ready_to_print");
    sem_close(file_counter);
    sem_close(access_shm);

    for(int i = 0; i < slavesAmmount; i++) {
        printf("Matando el proceso %d\n", slaves[i]);
        kill(slaves[i], SIGKILL);
    }
    return 0;
}

static void read_from_slave(int buffer_pipes[][2], int slavesAmmount, int argc, int * file_index, char * argv[], int file_pipes[][2], char * buffer_ptr, sem_t * sem,sem_t*file_counter) {

    int files_received = 0;

    fd_set all_fd, aux_fd;
    FD_ZERO(&all_fd);
    int retval;
    int max_fd = 0;
    FILE * resultado = fopen("resultado", "w");

    while(files_received < argc-1) {

        for(int i = 0; i < slavesAmmount; i++) {
            if(max_fd < buffer_pipes[i][0]){
                max_fd = buffer_pipes[i][0];
            }
            FD_SET(buffer_pipes[i][0], &all_fd);   // always prepare the readable-fds-set for reading
        }
        aux_fd = all_fd;
        retval = select( max_fd + 1, &aux_fd, NULL, NULL, NULL ); // blocks until a child has something to write to parent

        if(retval > 0){
            for(int i = 0; i < slavesAmmount; i++) {
                if(FD_ISSET(buffer_pipes[i][0], &aux_fd)) {
                    char * answer = calloc(MAX_LENGTH_TO_READ, sizeof(char));
                    run_and_check_error(read(buffer_pipes[i][0], answer, MAX_LENGTH_TO_READ), "Could not read files!\n", -1);
               
                    //SEMAPHORE 
                    sem_wait(sem);
                    strcat(buffer_ptr, answer);
                    sem_post(sem);
                    sem_post(file_counter);

                
                    printf("Slave %d sends: \n", i);
                    
                    files_received++;


                    if(*file_index < argc) { // BANCA!, puede que haya un slave con la file que me falta, No puedo mandar mas quiza                                                                      ////        este if        ////////
                        send_files(file_pipes[i][1], 1, file_index, argv);     
                    }
                }
            }
        }
    }
    
    
    fprintf(resultado, "%s", buffer_ptr);
    shm_unlink("/shared_memory");
    
    munmap(buffer_ptr,BUFFER_SIZE*sizeof(char));

    
}


static void send_files(int fd, int filesAmmount,int * file_index, char * argv[]) {
    char * file;

    if((file=calloc(FILE_LENGTH, sizeof(char))) == NULL) {
        dprintf(STDERR_FILENO,"No more space available for memory allocation\n");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < filesAmmount; i++) {
        strcat(file, argv[*file_index]);
        (*file_index)++;     
        strcat(file, SPECIAL_SYMBOL_STRING);  
    }

    run_and_check_error(write(fd, file, strlen(file)),"Could not write to file\n",-1);       
}

static void run_and_check_error(int error, char message[], int retval) {

    if(error == retval) {
        dprintf(STDERR_FILENO, "%s\n", message);
        printf("%d\n", errno);
        exit(EXIT_FAILURE);
    }
}

