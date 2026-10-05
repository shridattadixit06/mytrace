#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <sys/user.h>
#include <string.h>
#include <sys/uio.h>
#include <fcntl.h>

size_t min(size_t a, size_t b)
{
    return (a < b) ? a : b;
}
enum arg_type
{
    ARG_INT,
    ARG_POINTER,
    ARG_STRING,
    ARG_FLAGS
};
struct syscall_info
{
    long number;
    const char *name;
    int arg_count;
    enum arg_type arg_types[6];
};

struct syscall_info syscalls[] =
{
    {
        0,
        "read",
        3,
        {
            ARG_INT,
            ARG_POINTER,
            ARG_INT
        }
    },

    {
        1,
        "write",
        3,
        {
            ARG_INT,
            ARG_POINTER,
            ARG_INT
        }
    },

    {
        257,
        "openat",
        4,
        {
            ARG_INT,
            ARG_STRING,
            ARG_FLAGS,
            ARG_INT
        }
    }
};
const char *get_signal_name(int signal)
{
    switch (signal)
    {
        case SIGINT:  return "SIGINT";
        case SIGTERM: return "SIGTERM";
        case SIGQUIT: return "SIGQUIT";
        case SIGSEGV: return "SIGSEGV";
        case SIGSTOP: return "SIGSTOP";
        case SIGCONT: return "SIGCONT";
        case SIGTSTP: return "SIGTSTP";
        case SIGTRAP: return "SIGTRAP";
        default:      return "UNKNOWN";
    }
}
ssize_t read_tracee_memory(
    pid_t pid,
    unsigned long address,
    void *buffer,
    size_t size
    )
{
    memset(buffer, 0, size);
    struct iovec local;
    struct iovec remote;
    local.iov_base = buffer;
    local.iov_len = size;
    remote.iov_base = (void *)address;
    remote.iov_len = size;
    return process_vm_readv(
            pid,
            &local,
            1,
            &remote,
            1,
            0
    );
}
void print_open_flags(unsigned long long flags)
{
    long access_mode = flags & O_ACCMODE;

    if (access_mode == O_RDONLY)
    {
        printf("O_RDONLY");
    }
    else if (access_mode == O_WRONLY)
    {
        printf("O_WRONLY");
    }
    else if (access_mode == O_RDWR)
    {
        printf("O_RDWR");
    }
    if(flags & O_CREAT)
    {
        printf("| O_CREAT");
    }
    if(flags & O_TRUNC)
    {
        printf("| O_TRUNC");
    }
    if(flags & O_APPEND)
    {
        printf("| O_APPEND");
    }    
} 
void print_syscall_args(pid_t pid,struct user_regs_struct *regs, struct syscall_info *info)
{
    unsigned long long args[] =
    {
        regs->rdi,
        regs->rsi,
        regs->rdx,
        regs->r10,
        regs->r8,
        regs->r9
    };
    for(int i = 0; i < info->arg_count; i++)
    {
        switch(info->arg_types[i])
        {
            case ARG_INT:
                printf("arg%d = %lld\n",i + 1,(long long)args[i]);
                break;

            case ARG_POINTER:
                printf("arg%d = 0x%llx\n", i + 1, args[i]);
                break;
        
            case ARG_STRING:
            {
                char buffer[4096];

                ssize_t bytes = read_tracee_memory(
                    pid,
                    args[i],
                    buffer,
                    sizeof(buffer) - 1
                );

                if(bytes < 0)
                {
                    printf("arg%d = <invalid string>\n", i + 1);
                }
                else
                {
                    buffer[bytes] = '\0';

                    printf("arg%d = \"%s\"\n",
                        i + 1,
                        buffer);
                }

                break;
            }
            case ARG_FLAGS:
                printf("arg%d=",i+1);
                print_open_flags(args[i]);
                printf("\n");
                break;
        }
    }
}
struct syscall_info *get_syscall_info(long number)
{
    int count = sizeof(syscalls) / sizeof(syscalls[0]);

    for(int i = 0; i < count; i++)
    {
        if(syscalls[i].number == number)
        {
            return &syscalls[i];
        }
    }

    return NULL;
}
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
        unsigned long read_buffer = 0;
        int read_fd = 0;

        waitpid(pid, &status, 0);
        struct user_regs_struct regs;
        if(ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACESYSGOOD)==-1)
        {
            perror("PTRACE_SETOPTIONS");
            exit(EXIT_FAILURE);
        }
        int deliver_signal;
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
                if(sig == (SIGTRAP | 0x80))
                {
                    deliver_signal = 0;
                    if(ptrace(PTRACE_GETREGS, pid, NULL, &regs)==-1)
                    {
                        perror("PTRACE_GETREGS");
                        break;
                    }
                    if (in_syscall == 0)
                    {
                        printf("[SYSCALL ENTRY] ");
                        struct syscall_info *info;
                        info = get_syscall_info(regs.orig_rax);

                        if(info != NULL)
                        {
                            printf("%s\narg count = %d\n",info->name,info->arg_count);
                            print_syscall_args(pid,&regs, info);
                        }
                        else
                        {
                            printf("[SYSCALL ENTRY] unknown\n");
                        }
                        in_syscall = 1;
                    }
                    else
                    {
                        struct syscall_info *info = get_syscall_info(regs.orig_rax);

                        if(info != NULL)
                        {
                            printf(
                                "[SYSCALL EXIT] %s -> %lld\n",
                                info->name,
                                regs.rax
                            );
                        }
                        else
                        {
                            printf(
                                "[SYSCALL EXIT] unknown -> %lld\n",
                                regs.rax
                            );
                        }
                        long bytes_read = regs.rax;
                        if (regs.orig_rax == 0 && bytes_read > 0)
                        {
                            char buffer[100];

                            size_t amount = min(
                                sizeof(buffer) - 1,
                                bytes_read
                            );

                            ssize_t copied = read_tracee_memory(
                                pid,
                                read_buffer,
                                buffer,
                                amount
                            );

                            if (copied > 0)
                            {
                                buffer[copied] = '\0';

                                printf(
                                    "Read data: %.*s\n",
                                    (int)copied,
                                    buffer
                                );
                            }
                        }

                        in_syscall = 0;
                    }
                }
                else
                {
                    printf("[SIGNAL STOP] %s (%d)\n", get_signal_name(sig),sig);
                    if (sig == SIGTRAP)
                        deliver_signal = 0;
                    else
                        deliver_signal = sig;
                }
                if (ptrace(PTRACE_SYSCALL, pid, NULL, deliver_signal) == -1)
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