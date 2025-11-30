#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <time.h>

#define MAX_RUBRIC_LINES 5
#define MAX_QUESTIONS 5

typedef struct {
    int ta1_ready;
    int ta2_ready;
    sem_t lock1;
    sem_t lock2;
    sem_t start_barrier;  // To synchronize start
    int started_count;
} SharedData;

void ta_process_livelock(int ta_id, SharedData *data) {
    printf("TA %d: Started\n", ta_id);
    fflush(stdout);

    // Wait for both TAs to be ready
    sem_wait(&data->start_barrier);
    data->started_count++;
    if (data->started_count == 2) {
        sem_post(&data->start_barrier);
        sem_post(&data->start_barrier);  // Wakes up the other TA
    } else {
        sem_post(&data->start_barrier);
        usleep(10000);  // Small delay to ensure both are ready
    }

    printf("TA %d: Both TAs ready, beginning work\n", ta_id);
    fflush(stdout);

    int attempts = 0;
    int max_attempts = 20;  // Limit attempts to prevent infinite loop

    if (ta_id == 1) {
        // TA 1 tries to be polite and yields if TA 2 is ready
        while (attempts < max_attempts) {
            printf("TA %d: Attempt %d - Acquiring lock1\n", ta_id, attempts + 1);
            fflush(stdout);
            sem_wait(&data->lock1);
            data->ta1_ready = 1;

            // Check if TA 2 is ready
            if (data->ta2_ready == 1) {
                printf("TA %d: Detected TA 2 is ready, being polite and releasing lock1\n", ta_id);
                fflush(stdout);
                data->ta1_ready = 0;
                sem_post(&data->lock1);
                usleep(50000);  // Small delay
                attempts++;
                continue;
            }

            // Try to acquire lock2
            printf("TA %d: Trying to acquire lock2\n", ta_id);
            fflush(stdout);
            sem_wait(&data->lock2);

            // Success - both locks acquired
            printf("TA %d: Successfully acquired both locks!\n", ta_id);
            fflush(stdout);

            usleep(100000);

            sem_post(&data->lock2);
            data->ta1_ready = 0;
            sem_post(&data->lock1);
            printf("TA %d: Released both locks\n", ta_id);
            fflush(stdout);
            break;
        }

        if (attempts >= max_attempts) {
            printf("TA %d: LIVELOCK - Could not acquire both locks after %d attempts\n", ta_id, max_attempts);
            fflush(stdout);
        }
    } else if (ta_id == 2) {
        // TA 2 tries to be polite and yields if TA 1 is ready
        while (attempts < max_attempts) {
            printf("TA %d: Attempt %d - Acquiring lock2\n", ta_id, attempts + 1);
            fflush(stdout);
            sem_wait(&data->lock2);
            data->ta2_ready = 1;

            // Check if TA 1 is ready
            if (data->ta1_ready == 1) {
                printf("TA %d: Detected TA 1 is ready, being polite and releasing lock2\n", ta_id);
                fflush(stdout);
                data->ta2_ready = 0;
                sem_post(&data->lock2);
                usleep(50000);  // Small delay
                attempts++;
                continue;
            }

            // Try to acquire lock1
            printf("TA %d: Trying to acquire lock1\n", ta_id);
            fflush(stdout);
            sem_wait(&data->lock1);

            // Success - both locks acquired
            printf("TA %d: Successfully acquired both locks!\n", ta_id);
            fflush(stdout);

            usleep(100000);

            sem_post(&data->lock1);
            data->ta2_ready = 0;
            sem_post(&data->lock2);
            printf("TA %d: Released both locks\n", ta_id);
            fflush(stdout);
            break;
        }

        if (attempts >= max_attempts) {
            printf("TA %d: LIVELOCK - Could not acquire both locks after %d attempts\n", ta_id, max_attempts);
            fflush(stdout);
        }
    }

    printf("TA %d: Stopped\n", ta_id);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_TAs>\n", argv[0]);
        fprintf(stderr, "Note: This demonstration uses exactly 2 TAs\n");
        return 1;
    }

    int num_tas = atoi(argv[1]);
    if (num_tas < 2) {
        fprintf(stderr, "Number of TAs must be at least 2\n");
        return 1;
    }

    printf("Starting Part 2.b LIVELOCK DEMONSTRATION with 2 TAs\n");
    printf("This version demonstrates a livelock scenario where TAs keep yielding to each other.\n");
    fflush(stdout);

    // Create shared memory
    SharedData *data = mmap(NULL, sizeof(SharedData),
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (data == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // Initialize
    memset(data, 0, sizeof(SharedData));
    data->ta1_ready = 0;
    data->ta2_ready = 0;
    data->started_count = 0;

    // Initialize semaphores
    sem_init(&data->lock1, 1, 1);
    sem_init(&data->lock2, 1, 1);
    sem_init(&data->start_barrier, 1, 1);

    // Fork exactly 2 TA processes for livelock demonstration
    pid_t pids[2];
    for (int i = 0; i < 2; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            return 1;
        } else if (pids[i] == 0) {
            ta_process_livelock(i + 1, data);
            exit(0);
        }
    }

    // Wait for TAs
    for (int i = 0; i < 2; i++) {
        waitpid(pids[i], NULL, 0);
    }

    printf("\nAll TAs finished\n");

    // Cleanup
    sem_destroy(&data->lock1);
    sem_destroy(&data->lock2);
    sem_destroy(&data->start_barrier);
    munmap(data, sizeof(SharedData));

    return 0;
}