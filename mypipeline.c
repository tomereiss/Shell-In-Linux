#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
   

void print_debug (int pid, char* msg)
{
    if(pid != -1)
        fprintf(stderr, "(%s: %d)\n", msg, pid);
    else
        fprintf(stderr, "(%s)\n", msg);

        
}

int main(int argc, char** argv)
{
    int debug = 0;
    int fds[2];
    char* const ls[3] = {"ls", "-l",0};
    char* const tail[4] = {"tail","-n","2",0};
    pid_t pid1, pid2;


    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "-d") == 0) {// debug on
            debug = 1;
        }
    }

    if(pipe(fds) < 0)
    {
        perror("Oh no, pipe failed!");
        _exit(EXIT_FAILURE);
    }
    if(debug == 1)
        print_debug(-1,"parent_process>forking...");

    pid1 = fork();
    if(pid1 < 0)
    {
        perror("Oh no, fork failed!");
        _exit(EXIT_FAILURE);
    }
    if(debug == 1)
        print_debug(getpid(),"parent_process>created process with id: ");

    if(pid1 == 0) // Child1 proccess
    {
        if(debug == 1)
            print_debug(-1,"child1>redirecting stdout to the write end of the pipe...");   

        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);

        if(debug == 1)
            print_debug(-1,"child1>going to execute cmd..."); 

        execvp("ls", ls);
        perror(strerror(errno));
        _exit(EXIT_FAILURE);

    }
    if(debug == 1)
        print_debug(-1,"parent_process>forking...");

    pid2 = fork();
    if(pid2 < 0)
    {
        perror("Oh no, fork failed!");
        _exit(EXIT_FAILURE);
    }

    if(debug == 1)
        print_debug(getpid(),"parent_process>created process with id: ");


    if(pid2 == 0) // Child2 proccess
    {
        if(debug == 1)
            print_debug(-1,"child2>redirecting stdin to the read end of the pipe...");   
        close(fds[1]);
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);
        if(debug == 1)
            print_debug(-1,"child2>going to execute cmd..."); 

        execvp(tail[0], tail);
        perror(strerror(errno));
        _exit(EXIT_FAILURE);
    }
    
    if(debug == 1)
        print_debug(-1,"parent_process>closing the read end of the pipe...");
    close(fds[0]);
    
    if(debug == 1)
        print_debug(-1,"parent_process>closing the write end of the pipe...");
    close(fds[1]);

    if(debug == 1)
        print_debug(-1,"parent_process>waiting for child processes to terminate...");

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    if(debug == 1)
        print_debug(-1,"parent_process>exiting...");
    
    return 0;
}