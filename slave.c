// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

#define SPECIAL_SYMBOL '$'
#define SPECIAL_SYMBOL_STRING "$"

#define PID_LENGTH 10
#define FILE_LENGTH 100
#define COMMAND_LENGTH 200
#define MAX_OUTPUT_LENGTH 1000

#define MINISAT_CMD "minisat "
#define GREP_CMD " | grep -o -e 'Number of.*[0-9]\\+' -e 'CPU time.*' -e '.*SATISFIABLE' | xargs"

static void run_and_check_error(int error, char message[], int retval);
static void process_file(char file[]);

int main(int argc, char * argv[]){

    setvbuf(stdout, NULL, _IONBF,0);

    char file_to_process[FILE_LENGTH] = {'\0'};
    while(1){
        char files[MAX_OUTPUT_LENGTH] = {'\0'};
        run_and_check_error(read(STDIN_FILENO, files, MAX_OUTPUT_LENGTH), "Could not read files!\n", -1);
        int j=0;
        //separates each single file from the result of read, and sends each of them to process
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


static void process_file(char file[]) {
    char * cmd;
    if((cmd = calloc(COMMAND_LENGTH, sizeof(char))) == NULL){
        dprintf(STDERR_FILENO, "No more space available for memory allocation\n");
        exit(EXIT_FAILURE);
    }

    //builds the complete command to run: "minisat file_name | grep ..."
    strcat(cmd, MINISAT_CMD);
    strcat(cmd, file);
    strcat(cmd, GREP_CMD);
    
    //runs the comand and saves the result in a new file
    FILE * result = popen(cmd, "r");
    if( result == NULL){
        exit(EXIT_FAILURE);
    }

    char * buf;
    if((buf= calloc(MAX_OUTPUT_LENGTH, sizeof(char))) == NULL) {
        dprintf(STDERR_FILENO,"No more space available for memory allocation\n");
        exit(EXIT_FAILURE);
    }

    //saves everything from the file result to buf, and creates the complete string to send to solve
    fgets(buf, MAX_OUTPUT_LENGTH, result);
    strcat(buf, file);
    
    for(int i = 0; buf[i] != 0; i++){
        if(buf[i] == '\n'){
            buf[i] = ' ';
        }
    }

    char aux_pid[PID_LENGTH];
    sprintf(aux_pid, "  %d\n", getpid());
    strcat(buf, aux_pid);
    printf("%s", buf);

    free(cmd);
    free(buf);
}

static void run_and_check_error(int error, char message[], int retval){
    if(error == retval){
        dprintf(STDERR_FILENO, "%s", message);
        exit(EXIT_FAILURE);
    }
}