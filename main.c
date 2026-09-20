#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/ptrace.h>

int main()
{
    pid_t pid = fork();

    if (pid == -1)
    {
        perror("fork");
        return 1;
    }

    if (pid == 0)
    {
        printf("Child: I am the tracee\n");
        printf("Child PID: %d\n", getpid());

        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1)
        {
            perror("ptrace");
            return 1;
        }

        execlp("ls", "ls", NULL);

        perror("exec");
        return 1;
    }

    else
    {
        printf("Parent: I am the tracer\n");
        printf("Parent PID: %d\n", getpid());

        waitpid(pid, NULL, 0);

        printf("Child finished\n");
    }

    return 0;
}