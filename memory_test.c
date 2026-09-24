#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <string.h>

int main()
{
    int pipefd[2];

    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();

    if (pid == -1)
    {
        perror("fork");
        return 1;
    }

    if (pid == 0)
    {
        close(pipefd[0]);

        char message[] = "Hello from tracee!";

        printf("Child: message address = %p\n",
               (void *)message);

        unsigned long address =
            (unsigned long)message;

        write(
            pipefd[1],
            &address,
            sizeof(address)
        );

        close(pipefd[1]);

        sleep(5);

        return 0;
    }

    else
    {
        close(pipefd[1]);

        unsigned long address;

        read(
            pipefd[0],
            &address,
            sizeof(address)
        );

        printf("Parent: received address = %p\n",
               (void *)address);

        char buffer[100];

        memset(buffer, 0, sizeof(buffer));

        struct iovec local;
        struct iovec remote;

        local.iov_base = buffer;
        local.iov_len = sizeof(buffer);

        remote.iov_base = (void *)address;
        remote.iov_len = sizeof(buffer);

        ssize_t bytes_read = process_vm_readv(
            pid,
            &local,
            1,
            &remote,
            1,
            0
        );

        if (bytes_read == -1)
        {
            perror("process_vm_readv");
            return 1;
        }

        printf(
            "Parent: read %zd bytes\n",
            bytes_read
        );

        printf(
            "Parent: message = %s\n",
            buffer
        );

        close(pipefd[0]);

        waitpid(pid, NULL, 0);
    }

    return 0;
}