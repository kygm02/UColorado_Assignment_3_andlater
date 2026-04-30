#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    openlog(argv[0], LOG_PID, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Invalid arguments. Expected 2, got %d", argc - 1);
        fprintf(stderr, "Usage: %s <file> <string>\n", argv[0]);
        closelog();
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr  = argv[2];
    size_t      len       = strlen(writestr);  // store once

    int fd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "Error opening file %s: %s", writefile, strerror(errno));
        perror("Error opening file");
        closelog();
        return 1;
    }

    syslog(LOG_DEBUG, "Writing \"%s\" to %s", writestr, writefile);

    ssize_t nr = write(fd, writestr, len);
    if (nr == -1) {
        syslog(LOG_ERR, "Error writing to file %s: %s", writefile, strerror(errno));
        perror("Error writing to file");
        close(fd);
        closelog();
        return 1;
    } else if (nr != (ssize_t)len) {
        syslog(LOG_ERR, "Partial write to %s (%zd of %zu bytes)", writefile, nr, len);
        close(fd);
        closelog();
        return 1;  // treat as error
    }

    // Unix convention - end file with newline
    write(fd, "\n", 1);

    close(fd);
    closelog();
    return 0;
}
