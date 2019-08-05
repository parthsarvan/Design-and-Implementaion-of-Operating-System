#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include "function.h"
#include <sys/stat.h>
#include <signal.h>
#define MAXARGS 10

unsigned int childPIDs[20];
int indexPID = 0;
int ALARM_TIME = 5;
int ALARM_STAT = 1;
int masterPID = 0;
int ALARM_FLAG = 0;
int CONT_FLAG = 0;
int SEC_FLAG = 0;
// All commands have at least a type. Have looked at the type, the code
// typically casts the *cmd to some specific cmd type.
struct cmd {
	int type;          //  ' ' (exec), | (pipe), '<' or '>' for redirection
};

struct execcmd {
	int type;              // ' '
	char *argv[MAXARGS];   // arguments to the command to be exec-ed
};

//ordercmd is the cmd type for commands to run in order
struct ordercmd {
	int type;
	struct cmd *left;
	struct cmd *right;
};

//parrcmd is the cmd type for commands to run in parallel
struct parrcmd {
	int type;
	struct cmd *left;
	struct cmd *right;
};

int fork1(void);  // Fork but exits on failure.
struct cmd *parsecmd(char*);

// Execute cmd.  Never returns.
void alarmHandler(int sig) {
	signal(SIGALRM, SIG_IGN);
	kill(masterPID, SIGTERM);
	char c[16];
	printf("\n ALARM expired. Do you want to continue? [y/n]: ");
	scanf("%s", c);
	if (strcmp(c, "y") == 0)
		CONT_FLAG = 1;

	else {
		for (int i = indexPID - 1; i >= 0; i--) {
			kill(childPIDs[i], SIGTERM);
		}
		CONT_FLAG = 0;
		exit(0);
	}
	signal(SIGALRM, alarmHandler);
}

void runcmd(struct cmd *cmd) {

	int p[2], r, pidTemp;
	struct execcmd *ecmd;
	//added two cmd type struct
	struct ordercmd *ocmd;
	struct parrcmd *pacmd;

	char* patt;

	if (cmd == 0)
		exit(0);

	switch (cmd->type) {
	default:
		fprintf(stderr, "unknown runcmd\n");
		exit(-1);

	case ' ':
		ecmd = (struct execcmd*) cmd;
		if (ecmd->argv[0] == 0)
			exit(0);
		// fprintf(stderr, "exec not implemented\n");
		// Your code here ...
		//since we cannot determine the path, using execvp to run the commands
		signal(SIGALRM, alarmHandler);
		int ff;
		SEC_FLAG = 0;
		do {
			if (SEC_FLAG == 0)
				ff = fork1();
			else {
				ff = 1;

			}
			if (ff == 0) {
				if (execvp(ecmd->argv[0], ecmd->argv) == -1)
					printf("Command not Found. Try again...\n");
				exit(0);
			} else {
				masterPID = ff;

				alarm(5);
				wait(&ff);
				if (SEC_FLAG == 0)
					SEC_FLAG = 1;
				else
					SEC_FLAG = 0;
			}

		} while (CONT_FLAG == 1);
		alarm(0);
		break;

// if ";" in the command, the left part will be executed first
// after left command gave results, the right part will start to execute
	case ';':
		childPIDs[indexPID++] = getpid();
		ocmd = (struct ordercmd*) cmd;
		pidTemp = fork1();
		if (pidTemp == 0) {
			runcmd(ocmd->left);
		} else {

			wait(NULL); //wait for left part
			runcmd(ocmd->right);
		}
		break;

// if "&" in the command, both left and right part will be executed at the same time
	case '&':
		pacmd = (struct parrcmd*) cmd;
		// create two values to store return state of two fork1()
		pid_t process1, process2;

		// create the first child
		if ((process1 = fork1()) == 0) {
			// in the first child, run the left command
			runcmd(pacmd->left);
		} else {
			// create the second child
			childPIDs[indexPID++] = process1;
			process2 = fork1();
		}

		if (process2 == 0) {
			// in the second child, run the second command

			runcmd(pacmd->right);
		} else {
			childPIDs[indexPID++] = process2;
			// wait for both children
			// the reason I am doing this is because I want to wait for all these commands
			// to execute, then execute the commands after the ";", so I figured that I can
			// use many children, and let parent to wait for them to continue.

			wait(&process1);
			wait(&process2);
		}
		break;
	}
	exit(0);
}

int read_command(char *command_str) {
	if ((fgets(command_str, 100 * sizeof(char), stdin)) == NULL)
		return -1;

	command_str[strlen(command_str) - 1] = '\0';
	return 0;
}

int getcmd(char *buf, int nbuf) {
	char * prompt = getenv("PROMPT");
	if (isatty(fileno(stdin)))
		printf(" %s\t", prompt);
	signal(SIGINT, ctrl_c);
	setjmp(jbuffer);
	memset(buf, 0, nbuf);
	fgets(buf, nbuf, stdin);
	if (buf[0] == 0) // EOF
		return -1;

	return 0;
}

int fork1(void) {
	int pid;

	pid = fork();
	if (pid == -1)
		perror("fork");
	return pid;
}

struct cmd*
execcmd(void) {
	struct execcmd *cmd;

	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = ' ';
	return (struct cmd*) cmd;
}

// function to construct parallel cmds
struct cmd*
parrcmd(struct cmd *left, struct cmd *right) {
	struct parrcmd *cmd;

	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = '&';
	cmd->left = left;
	cmd->right = right;
	return (struct cmd*) cmd;
}

// function to construct order cmds
struct cmd*
ordercmd(struct cmd *left, struct cmd *right) {
	struct ordercmd *cmd;

	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = ';';
	cmd->left = left;
	cmd->right = right;
	return (struct cmd*) cmd;
}
// Parsing

char whitespace[] = " \t\r\n\v";
// added ; and &
char symbols[] = "<|>;&";

int gettoken(char **ps, char *es, char **q, char **eq) {
	char *s;
	int ret;

	s = *ps;
	while (s < es && strchr(whitespace, *s))
		s++;
	if (q)
		*q = s;
	ret = *s;
	switch (*s) {
	case 0:
		break;
		//added case for ; and &
	case ';':
	case '&':
		s++;
		break;
	default:
		ret = 'a';
		while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
			s++;
		break;
	}
	if (eq)
		*eq = s;

	while (s < es && strchr(whitespace, *s))
		s++;
	*ps = s;
	return ret;
}

int peek(char **ps, char *es, char *toks) {
	char *s;

	s = *ps;
	while (s < es && strchr(whitespace, *s))
		s++;
	*ps = s;
	return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parseexec(char**, char*);
//added two types' structs
struct cmd *parseparr(char**, char*);
struct cmd *parseorder(char**, char*);

// make a copy of the characters in the input buffer, starting from s through es.
// null-terminate the copy to make it a string.
char *mkcopy(char *s, char *es) {
	int n = es - s;
	char *c = malloc(n + 1);
	assert(c);
	strncpy(c, s, n);
	c[n] = 0;
	return c;
}

struct cmd*
parsecmd(char *s) {
	char *es;
	struct cmd *cmd;

	es = s + strlen(s);
	cmd = parseline(&s, es);
	peek(&s, es, "");
	if (s != es) {
		fprintf(stderr, "leftovers: %s\n", s);
		exit(-1);
	}
	return cmd;
}

struct cmd*
parseline(char **ps, char *es) {
	struct cmd *cmd;
	//cmd = parsepipe(ps, es);
	// first check the & in the command
	cmd = parseparr(ps, es);
	//cmd = parseorder(ps, es);
	//since after we check the & in the command, the pointer will not point to its
	//original position, so here I am using if to use the initial value to check ";"
	if (peek(ps, es, ";")) {
		gettoken(ps, es, 0, 0);
		cmd = ordercmd(cmd, parseline(ps, es));
	}
	return cmd;
}

//check if & exists in the command
struct cmd*
parseparr(char **ps, char *es) {
	struct cmd *cmd;

	cmd = parseexec(ps, es);
	if (peek(ps, es, "&")) {
		gettoken(ps, es, 0, 0);
		cmd = parrcmd(cmd, parseparr(ps, es));
	}
	return cmd;
}

//this function is not used since I needed to use if statement in parseline
//but resevered for later use
struct cmd*
parseorder(char **ps, char *es) {
	struct cmd *cmd;

	cmd = parseexec(ps, es);
	if (peek(ps, es, ";")) {
		gettoken(ps, es, 0, 0);
		cmd = ordercmd(cmd, parseorder(ps, es));
	}
	return cmd;
}

struct cmd*
parseexec(char **ps, char *es) {
	char *q, *eq;
	int tok, argc;
	struct execcmd *cmd;
	struct cmd *ret;

	ret = execcmd();
	cmd = (struct execcmd*) ret;

	argc = 0;
	//ret = parseredirs(ret, ps, es);
	// deleted pipe and added ; and & to rule out desired symbols
	//=================================
	//====================================
	while (!peek(ps, es, ";") && !peek(ps, es, "&")) {
		if ((tok = gettoken(ps, es, &q, &eq)) == 0)
			break;

		if (tok != 'a') {
			fprintf(stderr, "syntax error\n");
			exit(-1);
		}
		cmd->argv[argc] = mkcopy(q, eq);
		argc++;
		if (argc >= MAXARGS) {
			fprintf(stderr, "too many args\n");
			exit(-1);
		}
		//ret = parseredirs(ret, ps, es);
	}
	cmd->argv[argc] = 0;
	return ret;
}
void get_profile() {
	FILE * file = fopen("PROFILE", "r");
	if (file == NULL) {
		printf("PROFILE file not found, default home directory\n");
		setenv("HOME", "/home", 1);
		chdir("/home/");
		return;
	}
	chdir("/home");
	char tmp1[256];
	char tmp2[256];

	while (fgets(tmp1, sizeof(tmp1), file)) {
			int j = 0;
		int i = 0;

		while (tmp1[i] != '=') {
			tmp2[i] = tmp1[i];
			i++;
		}
		tmp2[i] = '\0';



		if (tmp2[0]=='P' && tmp2[1]=='A' && tmp2[2]=='T' &&
									tmp2[3]=='H' ) {
			//printf("%s\n", tmp2);
			i++;
			while (tmp1[i] != '\0' && tmp1[i] != '\n') {
				tmp2[j] = tmp1[i];
				j++;
				i++;
			}
			tmp2[j] = '\0';
			setenv("PATH", tmp2, 1);
		}

		if (tmp2[0]=='P' && tmp2[1]=='R' && tmp2[2]=='O' &&
				tmp2[3]=='M' && tmp2[4]=='P' && tmp2[5]=='T')
		{
			//printf("%s\n", tmp2);
			i++;
			while (tmp1[i] != '\0' && tmp1[i] != '\n') {
				tmp2[j] = tmp1[i];
				j++;
				i++;
			}
			tmp2[j] = '\0';
			if (j == 0) {
				fprintf(stderr, "Error: Prompt not defined...\n");
				exit(-1);
			}
			setenv("PROMPT", tmp2, 1);
		}

		if (tmp2[0]=='A' && tmp2[1]=='L' && tmp2[2]=='A' &&
						tmp2[3]=='R' && tmp2[4]=='M') {
					i++;
			while (tmp1[i] != '\0' && tmp1[i] != '\n') {
				tmp2[j] = tmp1[i];
				j++;
				i++;
			}
			tmp2[j] = '\0';
			setenv("ALARM", tmp2, 1);
			if (j < 2) {
				fprintf(stderr, "ALARM not set properly. Use ON or OFF\n");
				exit(-1);
			}

			if (tmp2[0] == 'O' && tmp2[1] == 'N') {
				ALARM_STAT = 1;

			} else if ((tmp2[0] == 'O' && tmp2[1] == 'F' && tmp2[2] == 'F')) {
				ALARM_STAT = 0;
			} else {
				fprintf(stderr, "ALARM not set properly. Use ON or OFF\n");
				exit(-1);
			}

		}

		if (tmp2[0]=='H' && tmp2[1]=='O' && tmp2[2]=='M' &&
						tmp2[3]=='E' ) {
			printf("HO<E");
			i++;
			while (tmp1[i] != '\0' && tmp1[i] != '\n') {
				tmp2[j] = tmp1[i];
				j++;
				i++;
			}
			tmp2[j] = '\0';
			if (j == 0) {
				fprintf(stderr, "Error: HOME not defined...\n");
				exit(-1);
			}
	printf("%s", tmp2);
			chdir(tmp2);
			setenv("HOME", tmp2, 1);

		}
		if (tmp2[0]=='T' && tmp2[1]=='I' && tmp2[2]=='M' &&
							tmp2[3]=='E' ) {
			i++;
			while (tmp1[i] != '\0' && tmp1[i] != '\n') {
				tmp2[j] = tmp1[i];
				j++;
				i++;
			}
			tmp2[j] = '\0';
			if (j == 0) {
				printf("Alarm time not set. Using default value...\n");
				return;
			}
			ALARM_TIME = atoi(tmp2);
			//	printf("%d",ALARM_TIME);
		}
	}
	fclose(file);
}
void ctrl_c(int sig) {
	char c[16];
	printf("\n You are about to EXIT. Are you sure? [y/n]: ");
	scanf("%s", c);
	if (strcmp(c, "y") == 0)
		exit(0);
	else
		longjmp(jbuffer, 0);

}

void change_directory(char * commandLine, char ** parameters) {
	if (parameters[1] == NULL) {
		chdir(getenv("HOME"));
	} else {
		chdir(parameters[1]);
	}
}

int main(void) {
	//masterPID = getpid();

	char c[16];
	get_profile();
	if (ALARM_STAT == 1) {
		printf("The ALARM signal set to %s\n", getenv("ALARM"));

	} else
		printf("ALARM signal set to %s\n", getenv("ALARM"));

	printf("use exit or Ctrl+c to stop the shell\n");
	static char buf[100];
	int fd, r;

	// Read and run input commands.
	while (getcmd(buf, sizeof(buf)) >= 0) {
		if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
			// Clumsy but will have to do for now.
			// Chdir has no effect on the parent if run in the child.
			buf[strlen(buf) - 1] = 0;  // chop \n
			if (chdir(buf + 3) < 0)
				fprintf(stderr, "cannot cd %s\n", buf + 3);
			continue;
		} else if (buf[strlen(buf) - 2] == '&') {
			// check if the command ended with "&", if so, exit
			fprintf(stderr, "cannot end with &\n");
			continue;
		} else if (buf[0] == 'e' && buf[1] == 'x' && buf[2] == 'i'
				&& buf[3] == 't') {
			//exit the shell
			printf("See you Later\n");
			exit(0);
		}

		int pidTemp = fork1();
		if (pidTemp == 0) {

			runcmd(parsecmd(buf));

		} else {

			childPIDs[indexPID++] = pidTemp;
			wait(&r);

		}

	}
	exit(0);
}

