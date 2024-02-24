#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>

typedef struct {
    unsigned int seconds;
    unsigned int nanoseconds;
} SystemClock;

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <stay_seconds> <stay_nanoseconds> <shm_id>\n", argv[0]);
        exit(1);
    }
    int stay_seconds = atoi(argv[1]);
    int stay_nanoseconds = atoi(argv[2]);
    int shm_id = atoi(argv[3]);

    // Attach to the shared system clock
    SystemClock *sys_clock = (SystemClock *)shmat(shm_id, NULL, 0);
    if (sys_clock == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    unsigned int target_seconds = sys_clock->seconds + stay_seconds;
    unsigned int target_nanoseconds = sys_clock->nanoseconds + stay_nanoseconds;
    if (target_nanoseconds >= 1000000000) {
        target_seconds += 1;
        target_nanoseconds -= 1000000000;
    }

    // Loop until the target time is reached
    while (sys_clock->seconds < target_seconds ||
           (sys_clock->seconds == target_seconds && sys_clock->nanoseconds < target_nanoseconds)) {
    }

    // Detach from shared memory
    shmdt((void *)sys_clock);

    return 0;
}
