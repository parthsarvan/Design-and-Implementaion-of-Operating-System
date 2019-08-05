#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include <ctype.h>



#define TRUE 1
#define STD_INPUT 0
#define STD_OUTPUT 1


jmp_buf jbuffer;

//void read_command(char * commandLine);
int read_command (char *command_str);
void get_parameters(char * commandLine, char ** parameters);
void get_commands_pipe(char * commandLine, char ** commands);
void execute_command(char * commandLine, char ** parameters);
void get_profile();
void get_variables();
void set_variables();
void set_one_variable(char * commandLine, char ** parameters);
void print_prompt();
void change_directory(char * commandLine, char ** parameters);
void ctrl_c(int sig);
float calculator(char * commandLine, char * parameters);
int get_filename(char * commandLine, char * file_name);
void execute_pipe(char * commandLine);
void exec_one_command(char * command, char ** parameters, int fdin, int fdout);
void check_dollar(char * commandLine);
int transform_commandLine(char * commandLine, char * variable, int ref, int endref);
void exec_seq_command(char * command, char ** parameters);
void signal_handler( int sig );
void get_commands_seq(char * commandLine, char ** commands);
void execute_seq(char * commandLine);

struct variables{
	char name[256];
	int value;
};

struct variables myVariables[512];

