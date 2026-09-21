#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <signal.h>

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
        raise(SIGSTOP);
        execlp("ls", "ls", NULL);

        perror("exec");
        return 1;
    }

    else
    {
        printf("Parent: I am the tracer\n");
        printf("Parent PID: %d\n", getpid());

        int status;
        int in_syscall = 0;
        waitpid(pid, &status, 0);

        while (1)
        {
            if (WIFEXITED(status))
            {
                printf("Child exited with status %d\n", WEXITSTATUS(status));
                break;
            }
            if (WIFSIGNALED(status))
            {
                printf("Child killed by signal %d\n", WTERMSIG(status));
                break;
            }
            if (WIFSTOPPED(status))
            {
                int sig = WSTOPSIG(status);
                printf("Child stopped by signal %d\n", sig);
                
                if(ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACESYSGOOD)==-1)
                {
                    perror("PTRACE_SETOPTIONS");
                    break;
                }

                if(sig == (SIGTRAP | 0x80))
                {
                    if (in_syscall == 0)
                    {
                        printf("Syscall entry\n");

                        in_syscall = 1;
                    }
                    else
                    {
                        printf("Syscall exit\n");

                        in_syscall = 0;
                    }
                }
                if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) == -1)
                {
                    perror("ptrace");
                    break;
                }
            }
            waitpid(pid, &status, 0);
        }
    }

    return 0;
}