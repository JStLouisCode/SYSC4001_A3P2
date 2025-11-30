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
    char rubric[MAX_RUBRIC_LINES][20];
    int questions_marked[MAX_QUESTIONS];
    int current_exam_num;
    int finished;
    sem_t rubric_sem;      // Protects rubric access
    sem_t questions_sem;   // Protects questions_marked array
    sem_t exam_sem;        // Protects exam transitions
} SharedData;

int random_delay(int min_ms, int max_ms) {
    return (rand() % (max_ms - min_ms + 1) + min_ms) * 1000;
}

void ta_process_deadlock(int ta_id, SharedData *data) {
    srand(time(NULL) + ta_id * 1000);
    int exams_to_process[] = {1, 2, 3, 9999};

    printf("TA %d: Started\n", ta_id);
    fflush(stdout);

    // DEADLOCK SCENARIO: Different lock acquisition order
    if (ta_id % 2 == 1) {
        // Odd TAs: acquire rubric first, then questions
        printf("TA %d: Trying to acquire rubric lock first\n", ta_id);
        fflush(stdout);
        sem_wait(&data->rubric_sem);
        printf("TA %d: Acquired rubric lock\n", ta_id);
        fflush(stdout);

        usleep(100000);  // Delays to increase chance of deadlock

        printf("TA %d: Trying to acquire questions lock\n", ta_id);
        fflush(stdout);
        sem_wait(&data->questions_sem);
        printf("TA %d: Acquired questions lock\n", ta_id);
        fflush(stdout);

        // Do work
        printf("TA %d: Performing work with both locks\n", ta_id);
        fflush(stdout);
        usleep(random_delay(500, 1000));

        sem_post(&data->questions_sem);
        sem_post(&data->rubric_sem);
        printf("TA %d: Released both locks\n", ta_id);
        fflush(stdout);
    } else {
        // Even TAs: acquire questions first, then rubric
        printf("TA %d: Trying to acquire questions lock first\n", ta_id);
        fflush(stdout);
        sem_wait(&data->questions_sem);
        printf("TA %d: Acquired questions lock\n", ta_id);
        fflush(stdout);

        usleep(100000);  // Delays to increase chance of deadlock

        printf("TA %d: Trying to acquire rubric lock\n", ta_id);
        fflush(stdout);
        sem_wait(&data->rubric_sem);
        printf("TA %d: Acquired rubric lock\n", ta_id);
        fflush(stdout);

        // Do work
        printf("TA %d: Performing work with both locks\n", ta_id);
        fflush(stdout);
        usleep(random_delay(500, 1000));

        sem_post(&data->rubric_sem);
        sem_post(&data->questions_sem);
        printf("TA %d: Released both locks\n", ta_id);
        fflush(stdout);
    }

    printf("TA %d: Stopped\n", ta_id);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_TAs>\n", argv[0]);
        return 1;
    }

    int num_tas = atoi(argv[1]);
    if (num_tas < 2) {
        fprintf(stderr, "Number of TAs must be at least 2\n");
        return 1;
    }

    printf("Starting Part 2.b DEADLOCK DEMONSTRATION with %d TAs\n", num_tas);
    printf("This version demonstrates a deadlock scenario with different lock acquisition orders.\n");
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
    data->finished = 0;

    // Initialize semaphores (shared between processes)
    sem_init(&data->rubric_sem, 1, 1);      // Binary semaphore
    sem_init(&data->questions_sem, 1, 1);   // Binary semaphore
    sem_init(&data->exam_sem, 1, 1);        // Binary semaphore

    // Fork TA processes
    pid_t pids[num_tas];
    for (int i = 0; i < num_tas; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            return 1;
        } else if (pids[i] == 0) {
            ta_process_deadlock(i + 1, data);
            exit(0);
        }
    }

    // Wait for all TAs (will likely hang due to deadlock)
    printf("\nWaiting for TAs to finish... (may hang if deadlock occurs)\n");
    fflush(stdout);

    for (int i = 0; i < num_tas; i++) {
        waitpid(pids[i], NULL, 0);
    }

    printf("\nAll TAs finished (no deadlock occurred)\n");

    // Cleanup semaphores
    sem_destroy(&data->rubric_sem);
    sem_destroy(&data->questions_sem);
    sem_destroy(&data->exam_sem);

    munmap(data, sizeof(SharedData));
    return 0;
}