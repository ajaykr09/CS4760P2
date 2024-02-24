#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#define MAX_PROCESSES 10

typedef struct {
    unsigned int seconds;
    unsigned int nanoseconds;
} SystemClock;

typedef struct {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
} PCB;

int shm_id;
SystemClock *sys_clock;
PCB processTable[MAX_PROCESSES] = {0};

void incrementClock(SystemClock *sys_clock) {
    sys_clock->nanoseconds += 100000;
    if (sys_clock->nanoseconds >= 1000000000) {
        sys_clock->seconds += 1;
        sys_clock->nanoseconds -= 1000000000;
    }
}

void timeout_handler(int sig) {
    // Terminate all child processes
    for (int j = 0; j < MAX_PROCESSES; j++) {
        if (processTable[j].occupied) {
            kill(processTable[j].pid, SIGTERM);
        }
    }
    // Detach and remove shared memory
    shmdt((void *)sys_clock);
    shmctl(shm_id, IPC_RMID, NULL);
    exit(0); // Exit the program
}

int main(int argc, char *argv[]) {
    int opt, n = 5, s = 3, t = 7, i = 100; // Default values

    // Parse command-line options
    while ((opt = getopt(argc, argv, "hn:s:t:i:")) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: %s [-n proc] [-s simul] [-t timelimitForChildren] [-i intervalInMsToLaunchChildren]\n", argv[0]);
                exit(EXIT_SUCCESS);
            case 'n':
                n = atoi(optarg);
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 't':
                t = atoi(optarg);
                break;
            case 'i':
                i = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-n proc] [-s simul] [-t timelimitForChildren] [-i intervalInMsToLaunchChildren]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Create shared memory for the system clock
    shm_id = shmget(IPC_PRIVATE, sizeof(SystemClock), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    // Attach the system clock to shared memory
    sys_clock = (SystemClock *)shmat(shm_id, NULL, 0);
    if (sys_clock == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    // Initialize system clock
    sys_clock->seconds = 0;
    sys_clock->nanoseconds = 0;

    // Set up the timeout handler
    struct itimerval timer;
    signal(SIGALRM, timeout_handler);
    timer.it_value.tv_sec = 60;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);

    // Main loop to manage child processes
    int launchedChildren = 0;
    int activeChildren = 0;
    while (launchedChildren < n) {
        incrementClock(sys_clock);

        // Check if any child has terminated
        int pid, status;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            // Update process table and decrement active children count
            for (int j = 0; j < MAX_PROCESSES; j++) {
                if (processTable[j].occupied && processTable[j].pid == pid) {
                    processTable[j].occupied = 0;
                    activeChildren--;
                    break;
                }
            }
        }

        // Launch new child if under the limit and time interval has passed
        if (activeChildren < s && (launchedChildren == 0 || sys_clock->nanoseconds % (i * 1000000) == 0)) {
            pid_t child_pid = fork();
            if (child_pid == 0) { // Child process
                // Generate random time within bounds for the child
                int childTime = (rand() % (t - 1)) + 1; // 1 to t seconds
                int childNano = rand() % 1000000000; // 0 to 999999999 nanoseconds
                char timeArg[20], nanoArg[20], shmIdArg[20];
                snprintf(timeArg, 20, "%d", childTime);
                snprintf(nanoArg, 20, "%d", childNano);
                snprintf(shmIdArg, 20, "%d", shm_id);
                execl("./worker", "worker", timeArg, nanoArg, shmIdArg, (char *)NULL);
                perror("execl");
                exit(EXIT_FAILURE);
            } else if (child_pid > 0) { // Parent process
                // Update process table with new child info
                for (int j = 0; j < MAX_PROCESSES; j++) {
                    if (!processTable[j].occupied) {
                        processTable[j].occupied = 1;
                        processTable[j].pid = child_pid;
                        processTable[j].startSeconds = sys_clock->seconds;
                        processTable[j].startNano = sys_clock->nanoseconds;
                        break;
                    }
                }
                launchedChildren++;
                activeChildren++;
            } else {
                perror("fork");
                exit(EXIT_FAILURE);
            }
        }

        // Periodic output of the process table
        if (sys_clock->nanoseconds % 500000000 == 0) {
            printf("OSS PID:%d SysClockS: %u SysclockNano: %u\n", getpid(), sys_clock->seconds, sys_clock->nanoseconds);
            printf("Process Table:\n");
            printf("Entry Occupied PID StartS StartN\n");
            for (int j = 0; j < MAX_PROCESSES; j++) {
                printf("%d %d %d %d %d\n", j, processTable[j].occupied, processTable[j].pid, processTable[j].startSeconds, processTable[j].startNano);
            }
        }
    }

    // Wait for all child processes to complete
    while (wait(NULL) > 0);

    // Detach and remove shared memory
    shmdt((void *)sys_clock);
    shmctl(shm_id, IPC_RMID, NULL);

    return 0;
}
