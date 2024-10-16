#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include "LineParser.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <errno.h>


#define BUFSIZE (2048)
#define TERMINATED  -1
#define RUNNING 1
#define SUSPENDED 0
#define HISTLEN 20

int debug = 0;
char *historyArray[HISTLEN];
int oldest = 0;
int newest = 0;
int sizeHistory = 0;

typedef struct process{
    cmdLine* cmd;                         /* the parsed command line*/
    pid_t pid; 		                  /* the process id that is running the command*/
    int status;                           /* status of the process: RUNNING/SUSPENDED/TERMINATED */
    struct process *next;	                  /* next process in chain */
} process;

void redirect(cmdLine* cmd_line){
    char const* input_file = cmd_line->inputRedirect;
    char const* output_file = cmd_line->outputRedirect;
    if(input_file != NULL){ //redirect to input file
        int input = open(input_file, O_RDONLY);
        if(input == -1)
        {
            perror("error in open input file");
            exit(EXIT_FAILURE);
        }
        dup2(input, STDIN_FILENO);
        close(input);
    }
    if(output_file != NULL){ //redirect to output file
        int output = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if(output == -1)
        {
            perror("error in open output file");
            exit(EXIT_FAILURE);
        }
        dup2(output, STDOUT_FILENO);
        close(output);
    }
}

void execute(process** process_list, cmdLine *pCmdLine) {
    pid_t pid = fork();
    if(pid == -1) {
        perror("fork() error\n");
        freeCmdLines(pCmdLine);
        freeMemExit(process_list);
    }
    else if (pid == 0) { // child 
        redirect(pCmdLine);
        if(execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1) {
            perror("execvp() error\n");
            freeCmdLines(pCmdLine);
            freeMemExit(process_list);
        }
    }
    else { // parent
        addProcess(process_list,pCmdLine,pid);
        if(debug == 1){
            fprintf(stderr, "PID: %d running command: ", pid);
            for(int i=0; i<(pCmdLine->argCount); i++) 
                fprintf(stderr, "%s ", pCmdLine->arguments[i]);
            fprintf(stderr, "\n");
        }
        if(pCmdLine->blocking) { // blocking case
            int status;
            if(waitpid(pid, &status, 0) == -1) {
                perror("waitpid() error\n");
            }
        }
    }
}

void cdAct(cmdLine *pCmdLine)
{
    if(pCmdLine->argCount == 1)
    { 
        chdir(getenv("HOME"));
    }
    else if(pCmdLine->argCount > 2) 
    {
        printf("Oh no suspend failed! ): \nNumber of Arguments: %d\n", pCmdLine->argCount-1);
    }
    else if(chdir(pCmdLine->arguments[1]) == -1) 
    { 
        printf("Oh no cd failed ):");
    }
}

int suspendAct(int pId)
{
    if(pId == 0) // error
    {
        return -1;
    }
    if(kill(pId, SIGTSTP) == 0)
    {
        printf("Process Suspended! ID: %d\n", pId);
        return 0;
    }
    else
    {
        perror("Oh no suspend failed ):");
        return -1;
    }
}

int wakeAct(int pId)
{
    if(pId == 0) // error
    {
        return -1;
    }
    if(kill(pId, SIGCONT) == 0)
    {
        printf("Process Resumed! ID: %d\n", pId);
        return 0;
    }
    else
    {
        perror("Oh no wake failed ):");
        return -1;
    }
}

int killAct(int pId)
{
    if(pId == 0) // error
    {
        return -1;
    }
    if(kill(pId, SIGINT) == 0)
    {
        printf("kill success with ID: %d\n", pId);
        return 0;
    }
    else
    {
        perror("Oh no kill failed ):");
        return -1;
    }
}

void pipe_cmdLine(process **process_list, cmdLine* pCmdLine)
{
    int fds[2];
    pid_t pid1, pid2;
    cmdLine* pCmdLine2 = pCmdLine->next;
    char const* input_file = pCmdLine->inputRedirect;
    char const* output_file = pCmdLine->outputRedirect;

    if(pipe(fds) == -1)
    {
        perror("Oh no, pipe failed!");
        freeCmdLines(pCmdLine);
        freeMemExit(process_list);
    }
    pid1 = fork();

    if(pid1 == -1)
    {
        perror("Oh no, fork of child1 failed!");
        freeCmdLines(pCmdLine);
        freeMemExit(process_list);
    }
    else if(pid1 == 0) // Child1 proccess
    {
        if(input_file !=NULL){
            int input = open(input_file, O_RDONLY);
            if(input == -1)
            {
                perror("error in open input file");
                exit(EXIT_FAILURE);
            }
            dup2(input, STDIN_FILENO);
            //close(input);
        }
        close(STDOUT_FILENO);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        if(execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1){
            perror("execvp() error\n");
            freeCmdLines(pCmdLine);
            freeMemExit(process_list);
        }
    }
    else{
        addProcess(process_list, pCmdLine, pid1);
        close(fds[1]);
        pid2 = fork();
        if(pid2 == -1){
            perror("Oh no, fork of child2 failed!");
            freeCmdLines(pCmdLine);
            freeMemExit(process_list);
        }
        if(pid2 == 0){
            if(output_file != NULL){
                int output = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
                if(output == -1)
                {
                    perror("error in open output file");
                    exit(EXIT_FAILURE);
                }
                dup2(output, STDOUT_FILENO);
                //close(output); 
            }
            close(STDIN_FILENO);
            dup2(fds[0], STDIN_FILENO);
            close(fds[0]);
            close(fds[1]);
            if(execvp(pCmdLine2->arguments[0], pCmdLine2->arguments) == -1){
                perror("execvp() error\n");
                freeCmdLines(pCmdLine2);
                freeMemExit(process_list);
            }
        }
        else{
            addProcess(process_list, pCmdLine2, pid2);
            close(fds[0]);
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
        }
    }
}


void addProcess(process** process_list, cmdLine* cmd, pid_t pid)
{
    process* new_process = (process*)malloc(sizeof(process));
    new_process->cmd = cmd;
    new_process->cmd->next = NULL;
    new_process->pid = pid;
    new_process->status = RUNNING;

    if((*process_list) == NULL)
        new_process->next = NULL;
    else
        new_process->next = (*process_list);

    *process_list = new_process;

}

void printProcessList(process** process_list)
{
    updateProcessList(process_list);
    printf("PID\t\tCommand\t\tStatus\n");
    process *cur_process = (*process_list);
            // char command[100] = "";
    process *prev = NULL;

    while(cur_process != NULL)
    {
        printf("%d\t\t", cur_process->pid);
        for(int i=0; i< cur_process->cmd->argCount; i++)
            printf("%s", cur_process->cmd->arguments[i]);
        printf("\t\t");
        if (cur_process->status == TERMINATED)
        {
            printf("\tTerminated\n");
            if(prev == NULL)
                *process_list = cur_process->next;
            else
                prev->next = cur_process->next;

            process *remove = cur_process;
            freeProcess(remove);
            
        }
        else{
            if (cur_process->status == RUNNING)
                printf("\tRunning\n");
            else if (cur_process->status == SUSPENDED)
                printf("\tSuspended\n");
            prev = cur_process;
        }
        cur_process = cur_process->next;
    }
}

void freeProcess(process* proc)
{
    freeCmdLines(proc->cmd);
    free(proc);
}

void printProcess(process *pro)
{
    if(pro != NULL)
    {
        char command[100] = "";
        if(pro->cmd->argCount > 0)
        {
            for (int i = 0; i < pro->cmd->argCount; i++)
            {
                strcat(command, pro->cmd->arguments[i]);
                strcat(command, "");
            } 
        }
        printf("%d\t\t%s\t\t", pro->pid, pro->cmd->arguments[0]);
        if (pro->status == TERMINATED)
        {
            printf("\tTerminated\n");
        }
        else if (pro->status == RUNNING)
        {
            printf("\tRunning\n");
        }
        else if (pro->status == SUSPENDED)
        {
            printf("\tSuspended\n");
        }   
    }
}

void freeProcessList(process** process_list)
{  
    process *nextP = *process_list;
    while(nextP != NULL)
    {
        process *temp = *process_list;
        nextP = nextP->next;
        freeProcess(temp);
    }
    free(process_list);

}



void updateProcessList(process **process_list)
{
    process *curr_process = (*process_list);
    while (curr_process != NULL)
    {
        int status = 0;
        int changed = waitpid(curr_process->pid, &status, WNOHANG | WUNTRACED | WCONTINUED); // instead 8 "WCONTINUED"
        if(changed == -1)
        {
            updateProcessStatus(curr_process, curr_process->pid, TERMINATED);
        }
        else if(changed == 0)
        {
            updateProcessStatus(curr_process, curr_process->pid, RUNNING);
        }
        else
        {
            if( WIFEXITED(status) | WIFSIGNALED(status))
                updateProcessStatus(curr_process, curr_process->pid, TERMINATED);
            else if( WIFSTOPPED(status))
                    updateProcessStatus(curr_process, curr_process->pid, SUSPENDED);
            else if(WIFCONTINUED(status))
                updateProcessStatus(curr_process, curr_process->pid, RUNNING);

        }
        
        curr_process = curr_process->next;
    }
}

void updateProcessStatus(process* process_list, int pid, int status)
{
    process *curr_process = process_list;
    while (curr_process != NULL)
    {
        if(curr_process->pid == pid)
        {
            curr_process->status = status;
            break;
        }
        curr_process = curr_process->next;
    }
}

void print_history()
{
    for (int i = 0; i < sizeHistory; i++)
        printf("%d: %s\n", i + 1, historyArray[i]);
}

void addHistory(char* input){
    int flag = 0;
    // if(((newest + 1) % HISTLEN) == (oldest)) // array is full
    if(sizeHistory == HISTLEN) // array is full
    {
        free(historyArray[oldest]);
        flag = 1;
    }
    historyArray[newest] = malloc(sizeof(char)*strlen(input) + 1);
    strcpy(historyArray[newest], input);
    newest = (newest + 1) % HISTLEN;
    if (flag == 1)
        oldest = (oldest + 1) % HISTLEN;
    else
        sizeHistory++;
}

int isLegalIndex(curr_index)
{
    if( HISTLEN <= curr_index || curr_index < 0 || historyArray[curr_index] == NULL ){
        fprintf(stderr, "error- invalid index\n");
        return -1;
    }
    return 1;
}

void historyAct(process** process_list,char* input, int index) {
    if (oldest == newest && historyArray[0] == NULL)
    {
        printf("history is empty\n");
    }
    else
    {
       
        int curr_index = (newest - index)% HISTLEN;
        if(curr_index<0)
            curr_index = curr_index + HISTLEN;
        if(isLegalIndex(curr_index) == -1) // invalid index
            return;            
        char *command_line = strdup(historyArray[(curr_index) % HISTLEN]);
        
        if(command_line != NULL){
            if(strncmp(command_line, "!",1)!=0)
                addHistory(command_line);
            cmdLine* command = parseCmdLines(command_line);
            if(command->next != NULL){ // pipe execution
                pipe_cmdLine(process_list, command); 
            }
            else{
                int spec = executeSpec(command, historyArray[index], process_list);
                if(spec != 1){ 
                    if(spec == -1){ // quit
                        exit(0);
                    }
                    else{ // spec = 0 - no spec execute
                        execute(process_list, command);
                        addHistory(command_line);
                    }
                } 

            }
        }
            
    }
}

void freeHistory()
{
    for(int i = 0; i < HISTLEN; i ++)
    {
        if(historyArray[i] != NULL)
        {
            free(historyArray[i]);
        }
    }
}

void freeMemExit(process** process_list)
{
    freeHistory();
    freeProcessList(process_list);
    exit(EXIT_FAILURE);
}



int executeSpec(cmdLine* cmd_line, char* user_input, process** process_list)
{            
    
    if (strcmp(cmd_line->arguments[0], "quit") == 0) 
    {
        freeCmdLines(cmd_line);
        freeProcessList(process_list);
        freeHistory();
        return -1;
    } 
    else if (strcmp(cmd_line->arguments[0], "cd") == 0) 
    {
        cdAct(cmd_line);
        return 1;
    }
    else if (strcmp(cmd_line->arguments[0], "kill") == 0) 
    {
        int pid = atoi(cmd_line->arguments[1]);
        if(killAct(pid) == 0) //succeeded
            updateProcessStatus(*process_list, pid, TERMINATED);
        return 1;
    } 
    else if (strcmp(cmd_line->arguments[0], "suspend") == 0) {
        int pid = atoi(cmd_line->arguments[1]);
        if(suspendAct(pid) == 0) //succeeded
            updateProcessStatus(*process_list, pid, SUSPENDED);
        return 1;

    }  
    else if (strcmp(cmd_line->arguments[0], "wake") == 0) 
    {
        int pid = atoi(cmd_line->arguments[1]);
        if(wakeAct(pid) == 0) //succeeded
            updateProcessStatus(*process_list, pid, RUNNING);
        return 1;

    }
    else if (strcmp(cmd_line->arguments[0], "procs") == 0) 
    {
        printProcessList(process_list);
        return 1;

    }
    else if (strcmp(cmd_line->arguments[0], "history") == 0) 
    {
        print_history();
        return 1;

    }
    else if (strcmp(cmd_line->arguments[0], "!!") == 0) 
    {
                historyAct(process_list, user_input, 1);
                freeCmdLines(cmd_line);
                return 1;

    }
    else if (strstr(cmd_line->arguments[0],"!") != NULL) 
    {
                historyAct(process_list, user_input, atoi(cmd_line->arguments[0]+1));
                freeCmdLines(cmd_line);
                return 1;

    }

    return 0;
        
}        

int main(int argc, char *argv[])
{
    int pid;
    char user_input[BUFSIZE];
    char cwd[PATH_MAX];
    cmdLine* pCmdLine = NULL;
    process** process_list = (process**) malloc(sizeof(process*));
    for (int i = 0; i < HISTLEN; i++)
        historyArray[i] = NULL;
    
    for(int i = 1; i < argc; i++)
    {
        //---------------------------
        if(strcmp(argv[i], "-d") == 0) {// debug on
            debug = 1;
        //---------------------------
        }
    }
    while (1) {
        getcwd(cwd, sizeof(cwd));
        printf("%s $ ", cwd); // print path
        fgets(user_input, BUFSIZE, stdin);
        pCmdLine = parseCmdLines(user_input);

        if(pCmdLine == NULL){
            continue;
        }
        if(debug == 1){
            fprintf(stderr, "PID: %d running command: ", getpid());
            for(int i=0; i<(pCmdLine->argCount); i++) {
                fprintf(stderr, "%s ", pCmdLine->arguments[i]);
            }
            fprintf(stderr, "\n");
        }
        if(pCmdLine->next != NULL){ // pipe
        
            if(pCmdLine->outputRedirect !=NULL || pCmdLine->next->inputRedirect !=NULL){
                perror("errorr with pipe");
                //exit(EXIT_FAILURE);
            }
            else{
                pipe_cmdLine(process_list, pCmdLine);
                addHistory(user_input);
            }
        } 
        else{ // no pipe
            int spec = executeSpec(pCmdLine, user_input, process_list);
            if(spec != 1){
                if(spec == -1){ // quit
                    exit(0);
                }
                else{ // spec = 0 - no spec execute
                    execute(process_list, pCmdLine);
                    addHistory(user_input);
                }
            }   
        }
    }
    return 0;
}      