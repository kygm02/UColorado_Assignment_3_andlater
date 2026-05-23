/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#define _GNU_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>


volatile sig_atomic_t caught_sigint  = 0; // could be a bool, but sig_atomic_t is guaranteed to be atomic for signal handlers
volatile sig_atomic_t caught_sigterm = 0;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void signal_handler(int signal_number)
{
    if (signal_number == SIGINT) {
        caught_sigint = 1;
    } else if (signal_number == SIGTERM) {
        caught_sigterm = 1;
    }
}

int main(int argc, char *argv[])
{
    struct sigaction new_action;
    
    // Clear the structure and assign the handler
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = signal_handler;
    
    // Register the signals
    sigaction(SIGTERM, &new_action, NULL);  // handle kill command
    sigaction(SIGINT,  &new_action, NULL);  // handle Ctrl+C
    openlog(argv[0], LOG_PID, LOG_USER);
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (sockfd < 0) 
        error("ERROR opening socket");
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    portno = 9000;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR on binding");
    listen(sockfd,5);
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        pid_t pid = fork();
        if (pid < 0) error("ERROR on fork");
        if (pid > 0) exit(0);  // parent exits

    // child continues as daemon
        umask(0);
        if (setsid() < 0) error("ERROR on setsid");

    // redirect stdin/stdout/stderr to /dev/null
        int devnull = open("/dev/null", O_RDWR);
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        close(devnull);
    }
    while (!caught_sigint && !caught_sigterm) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0){
            if (errno == EINTR) break;
            error("ERROR on accept");
        }
        unsigned char *ip = (unsigned char *)&cli_addr.sin_addr.s_addr;
        printf("Accepted connection from %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
        syslog(LOG_DEBUG, "Accepted connection from %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

// Accumulate data until newline
        size_t buf_size = 256;
        size_t buf_used = 0;
        char *dynbuf = malloc(buf_size);
        if (!dynbuf) error("ERROR malloc");

        int got_newline = 0;
        while (!got_newline) {
            if (buf_used == buf_size) {
            buf_size *= 2;
            char *tmp = realloc(dynbuf, buf_size);
            if (!tmp) { free(dynbuf); error("ERROR realloc"); }
            dynbuf = tmp;
        }
            n = read(newsockfd, dynbuf + buf_used, buf_size - buf_used);
            if (n < 0) { free(dynbuf); error("ERROR reading from socket"); }
            if (n == 0) break;
            buf_used += n;
            if (memchr(dynbuf, '\n', buf_used)) got_newline = 1;
        }

// Append packet to file
        int wfd = open("/var/tmp/aesdsocketdata", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (wfd < 0) { free(dynbuf); error("ERROR opening file for write"); }
        write(wfd, dynbuf, buf_used);
        close(wfd);
        free(dynbuf);

// Send full file back to client
        int rfd = open("/var/tmp/aesdsocketdata", O_RDONLY);
        if (rfd < 0) error("ERROR opening file for read");
        char filebuf[256];
        ssize_t bytes;
        while ((bytes = read(rfd, filebuf, sizeof(filebuf))) > 0) {
            if (write(newsockfd, filebuf, bytes) < 0) {
                close(rfd);
                error("ERROR writing to socket");
            }
        }
        close(rfd);
        close(newsockfd);
        syslog(LOG_DEBUG, "Closed connection from %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        printf("Closed connection from %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    }
    if (caught_sigint) {
        printf("\nCaught SIGINT!\n");
        close(sockfd);
        syslog(LOG_DEBUG, "Caught signal, exiting");
        closelog();
        remove("/var/tmp/aesdsocketdata");
    }
    if (caught_sigterm) {
        printf("\nCaught SIGTERM!\n");
        close(sockfd);
        syslog(LOG_DEBUG, "Caught signal, exiting");
        closelog();
        remove("/var/tmp/aesdsocketdata");
    }
    return 0; 
}
