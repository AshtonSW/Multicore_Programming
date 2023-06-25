/* $begin shellmain */  
#include "csapp.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

/* user define */
#define MAXARGS   128
#define MAXHISTORY 1000
#define MAX_COMMANDS 4

/* struct for history */
typedef struct {
    char cmdline[MAXLINE];
} History;

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv, char *cmdline);

/* User Function prototypes */
void save_history(char* cmdline);
void unix_error(char* msg); // = perror
void execute_piped_commands(char** argv);

char bin_path[MAXLINE]; /* user: Path of the binary */
History history[MAXHISTORY];
int history_count = 0;


int main() 
{
    // Load history from file
    FILE* history_file = fopen("history.txt", "r");
    if (history_file) {
        while (fgets(history[history_count].cmdline, MAXLINE, history_file) != NULL) {
            history_count++;
        }
        fclose(history_file);
    }
    char cmdline[MAXLINE]; /* Command line */
    

    while (1) {
        /* Read */
	    printf("CSE4100-MP-P1> ");                   
        char* result = fgets(cmdline, MAXLINE, stdin);
        if (result == NULL || feof(stdin)) {
       
            exit(0);
        }

	    /* Evaluate */
	    eval(cmdline);
        } 
}

/* eval - Evaluate a command line */
void eval(char* cmdline)
{
    char* argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL) {
        return;   /* Ignore empty lines */
    }

    /* user: ls, rmdir, mkdir command*/
    if (!strcmp(argv[0], "ls") || !strcmp(argv[0], "rmdir") || !strcmp(argv[0], "mkdir")) {
        strcpy(bin_path, "/bin/");
        strcat(bin_path, argv[0]);
        argv[0] = bin_path; // update the binary path
    }
    /* user: touch, echo, cat command */
    if (!strcmp(argv[0], "touch") || !strcmp(argv[0], "echo") || !strcmp(argv[0], "cat")) {
        strcpy(bin_path, "/bin/");
        strcat(bin_path, argv[0]);
        argv[0] = bin_path; // update the binary path
    }
    
    if (!builtin_command(argv, cmdline)) { //quit -> exit(0), & -> ignore, other -> run
        char** pipe_pos = argv;
        int found_pipe = 0;

        if (argv[0][0] != '!') {
            save_history(cmdline);
            if (argv[0][1] == '!') {
                eval(cmdline);
            }
        }

        while (*pipe_pos) {
            if (strcmp(*pipe_pos, "|") == 0) {
                found_pipe = 1;
                break;
            }
            pipe_pos++;
        }

        if (found_pipe) {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }

            if (pid == 0) {
                execute_piped_commands(argv);
                exit(0);
            }
            else {
                int status;
                if (waitpid(pid, &status, 0) < 0)
                    unix_error("waitfg: waitpid error");
            }
        }
        else {
            pid = fork();
            if (pid < 0) {
                printf("fork error\n");
                return;
            }
            else if (pid == 0) {
                /* Child runs user job */
                if (execve(argv[0], argv, environ) < 0) {
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
            }
            else {
                /* Parent waits for foreground job to terminate */
                if (!bg) {
                    int status;
                    if (waitpid(pid, &status, 0) < 0)
                        unix_error("waitfg: waitpid error");
                }
                else { // when there is background process!
                    printf("%d %s", pid, cmdline);
                }
                return;
            }
        }
    }
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv, char *cmdline) 
{
    if (!strcmp(argv[0], "quit")) { /* quit command */
        exit(0);
    }

    /* user: exit command */
    if (strcmp(argv[0], "exit") == 0) {
        exit(0);
    }

    if (!strcmp(argv[0], "&")) {    /* Ignore singleton & */
        return 1;
    }
    /* user: history command */
    if (!strcmp(argv[0], "history")) {
        save_history(cmdline);
        for (int i = 0; i < history_count; i++) {
            printf("%d %s", i + 1, history[i].cmdline);
        }
        return 1;
    }
    if (argv[0][0] == '!') {
        if (argv[0][1] == '!') {
            if (history_count > 0) {
                strcpy(cmdline, history[history_count - 1].cmdline);
                printf("%s", cmdline);
                eval(cmdline);
                return 1;
            }
            else {
                printf("No commands in history.\n");
            }
            return 1;
        }
        else {
            int index = atoi(argv[0] + 1) - 1;
            if (index >= 0 && index < history_count) {
                strcpy(cmdline, history[index].cmdline);
                printf("%s", cmdline);
                eval(cmdline);
                return 1;
            }
            else {
                printf("No such command in history.\n");
            }
            return 1;
        }
    }
    /* user: cd command */
    if (!strcmp(argv[0], "cd")) 
    {
        if (chdir(argv[1]) != 0)
            perror("cd");
        save_history(cmdline); // Save the cd command 
        return 1;
    }
        
    return 0;                     /* Not a builtin command */
}

/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	argv[--argc] = NULL;

    return bg;
}

/* Execute piped command Function */
void execute_piped_commands(char** argv) {
    int pipefds[MAX_COMMANDS - 1][2];
    char* commands[MAX_COMMANDS][MAXARGS];
    int command_index = 0;
    int num_commands = 0;

    // Split the commands at the pipe ('|')
    for (int i = 0; i < MAX_COMMANDS; i++) {
        command_index = 0;
        while (*argv) {
            if (strcmp(*argv, "|") == 0) {
                commands[i][command_index] = NULL;
                argv++; // Skip the pipe symbol
                break;
            }
            commands[i][command_index++] = *argv++;
        }
        commands[i][command_index] = NULL;
        num_commands++;

        if (!*argv) {
            break;
        }
    }

    // Create the pipes
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipefds[i]) < 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Fork and execute the commands
    for (int i = 0; i < num_commands; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) {
            if (i > 0) {
                // If not first command, set stdin to read end of previous pipe
                dup2(pipefds[i - 1][0], STDIN_FILENO);
            }
            if (i < num_commands - 1) {
                // If not last command, set stdout to write end of current pipe
                dup2(pipefds[i][1], STDOUT_FILENO);
            }
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }

            if (execvp(commands[i][0], commands[i]) < 0) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Close - all remaining pipe file descriptors (in the parent process)
    for (int j = 0; j < num_commands - 1; j++) {
        close(pipefds[j][0]);
        close(pipefds[j][1]);
    }

    // Wait - all child processes to finish
    for (int i = 0; i < num_commands; i++) {
        wait(NULL);
    }
}


/* Save history function */
void save_history(char* cmdline)
{
    if (history_count == 0 || strcmp(cmdline, history[history_count - 1].cmdline) != 0) {
        if (history_count < MAXHISTORY) {
            strcpy(history[history_count].cmdline, cmdline);
            history_count++;
        }
        else {
            for (int i = 1; i < MAXHISTORY; i++) {
                strcpy(history[i - 1].cmdline, history[i].cmdline);
            }
            strcpy(history[MAXHISTORY - 1].cmdline, cmdline);
        }
        FILE* history_file = fopen("history.txt", "w");
        if (history_file) {
            for (int i = 0; i < history_count; i++) {
                fputs(history[i].cmdline, history_file);
            }
            fclose(history_file);
        }
    }
}

/* Error message print (= perror)*/
void unix_error(char* msg) {
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}


