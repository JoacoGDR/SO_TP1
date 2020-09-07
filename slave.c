#include <stdlib.h>
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
#include <string.h>

#define SPECIAL_SYMBOL '$'

#define SPECIAL_SYMBOL_STRING "$"

#define FILE_LENGTH 100

#define COMMAND_LENGTH 200

#define MINISAT_CMD "minisat "
#define GREP_CMD " | grep -o -e 'Number of.*[0-9]\\+' -e 'CPU time.*' -e '.*SATISFIABLE' | xargs"
#define MAX_OUTPUT_LENGTH 1000

static void run_and_check_error(int error, char message[], int retval);
void process_file(char file[]);

int main(int argc, char * argv[]){
    setvbuf(stdout, NULL, _IONBF,0);


    char file_to_process[FILE_LENGTH] = {'\0'};
    while(1){
        char files[MAX_OUTPUT_LENGTH] = {'\0'};
        run_and_check_error(read(STDIN_FILENO, files, MAX_OUTPUT_LENGTH), "Could not read files!\n", -1);
        int j=0;
        for(int i = 0; files[i] != 0; i++){
            if(files[i] != SPECIAL_SYMBOL){
                file_to_process[j] = files[i];
                j++;
                
            }
            else{
                file_to_process[j] = '\0';
                process_file(file_to_process);
                j = 0;
            }
        }
    }
    
}


void process_file(char file[]) {
    char * cmd;
    if((cmd = calloc(COMMAND_LENGTH, sizeof(char))) == NULL){
        dprintf(STDERR_FILENO, "No more space available for memory allocation\n");
        exit(EXIT_FAILURE);
    }
    strcat(cmd, MINISAT_CMD);
    strcat(cmd, file);
    strcat(cmd, GREP_CMD);
    
    FILE * result = popen(cmd, "r");   // open minisat command
    if( result == NULL){
        exit(EXIT_FAILURE);
    }

    free(cmd);

    char * buf = calloc(MAX_OUTPUT_LENGTH, sizeof(char));
    if(buf == NULL){
        perror("Error with memory allocation!!!");
        exit(EXIT_FAILURE);
    }
    fgets(buf, MAX_OUTPUT_LENGTH, result);
    strcat(buf, file);
    
    char aux_pid[10];
    for(int i = 0; buf[i] != 0; i++){
        if(buf[i] == '\n'){
            buf[i] = ' ';
        }
    }
    sprintf(aux_pid, "  %d\n", getpid());
    strcat(buf, aux_pid);
    printf("%s", buf);

    free(buf);
}


static void run_and_check_error(int error, char message[], int retval){
    if(error == retval){
        dprintf(STDERR_FILENO, "%s", message);
        exit(EXIT_FAILURE);
    }
}