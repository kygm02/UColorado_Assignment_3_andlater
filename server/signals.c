#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   // Added for memset
#include <syslog.h>
#include <signal.h>   // Added for sigaction, SIGINT, SIGTERM, and pause
#include <stdbool.h>  // Added for bool, true, and false
#include <unistd.h>   // Added for pause

// Global flags must be volatile sig_atomic_t so the compiler 
// doesn't optimize away checks inside the main loop
volatile sig_atomic_t caught_sigint  = 0; // could be a bool, but sig_atomic_t is guaranteed to be atomic for signal handlers
volatile sig_atomic_t caught_sigterm = 0;

// Signal handler must be defined outside of the main function
void signal_handler(int signal_number)
{
    if (signal_number == SIGINT) {
        caught_sigint = 1;
    } else if (signal_number == SIGTERM) {
        caught_sigterm = 1;
    }
}

int main (int argc, char **argv)
{
    struct sigaction new_action;
    
    // Clear the structure and assign the handler
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = signal_handler;
    
    // Register the signals
    sigaction(SIGTERM, &new_action, NULL);  // handle kill command
    sigaction(SIGINT,  &new_action, NULL);  // handle Ctrl+C

    printf("Waiting forever for a signal\n");
    
    // Loop until one of our registered signals is caught
    while (!caught_sigint && !caught_sigterm) {
        pause();  // sleep until ANY signal arrives
    }
    
    if (caught_sigint) {
        printf("\nCaught SIGINT!\n");
    }
    if (caught_sigterm) {
        printf("\nCaught SIGTERM!\n");
    }
    
    return 0;
}