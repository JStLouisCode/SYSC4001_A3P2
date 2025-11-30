#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>

#define MAX_RUBRIC_LINES 5
#define MAX_QUESTIONS    5

typedef struct {
    char rubric[MAX_RUBRIC_LINES][20];   // rubric lines: "1,A" etc.
    int  questions_marked[MAX_QUESTIONS]; // 0 = not marked, 1 = marked
    int  current_exam_index;             // index into exam_files[]
    int  student_id;                     // current student number
    int  finished;                       // 1 when TAs should stop
} SharedData;

// random delay in microseconds between min_ms and max_ms
int random_delay(int min_ms, int max_ms) {
    return (rand() % (max_ms - min_ms + 1) + min_ms) * 1000;
}

/* -------------------- exam file list -------------------- */
/* matches the files in exams/ */
const char *exam_files[] = {
    "exams/0001", "exams/0002","exams/0003","exams/0004","exams/0005","exams/0010","exams/0015",
    "exams/0020","exams/0025","exams/0030","exams/0100","exams/0200","exams/0300","exams/0500",
    "exams/1000","exams/1500","exams/2000","exams/3000","exams/5000","exams/7000","exams/8000",
    "exams/8500","exams/9000","exams/9500",
    "exams/9999"      // stop exam
};
const int NUM_EXAMS = sizeof(exam_files) / sizeof(exam_files[0]);

/* -------------------- rubric helpers -------------------- */

// Reads rubric.txt into shared memory
void load_rubric(SharedData *data, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        // fallback if file is not there
        perror("fopen rubric");
        strcpy(data->rubric[0], "1,A");
        strcpy(data->rubric[1], "2,B");
        strcpy(data->rubric[2], "3,C");
        strcpy(data->rubric[3], "4,D");
        strcpy(data->rubric[4], "5,E");
        return;
    }

    char line[64];
    for (int i = 0; i < MAX_RUBRIC_LINES; i++) {
        if (!fgets(line, sizeof(line), f)) break;

        // strips newline
        line[strcspn(line, "\r\n")] = '\0';

        strncpy(data->rubric[i], line, sizeof(data->rubric[i]) - 1);
        data->rubric[i][sizeof(data->rubric[i]) - 1] = '\0';
    }

    fclose(f);
}

// Writes current rubric from shared memory back to rubric.txt
// In part 2.a we don't use semaphores, so races are expected.
void save_rubric(SharedData *data, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("fopen rubric for write");
        return;
    }

    for (int i = 0; i < MAX_RUBRIC_LINES; i++) {
        fprintf(f, "%s\n", data->rubric[i]);
    }

    fclose(f);
}

/* -------------------- exam helpers -------------------- */

// Load one exam file, set student_id, and reset questions_marked[]
void load_exam(SharedData *data, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen exam");
        data->student_id = -1;
    } else {
        char line[64];
        if (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            data->student_id = atoi(line);
        } else {
            data->student_id = -1;
        }
        fclose(f);
    }

    // reset question state for this exam
    for (int i = 0; i < MAX_QUESTIONS; i++) {
        data->questions_marked[i] = 0;
    }
}

/* -------------------- TA process code -------------------- */

void ta_process(int ta_id, SharedData *data) {
    srand(time(NULL) + ta_id * 1000);

    printf("TA %d: Started\n", ta_id);
    fflush(stdout);

    while (!data->finished) {
        // review rubric
        printf("TA %d: Reviewing rubric\n", ta_id);
        fflush(stdout);

        for (int i = 0; i < MAX_RUBRIC_LINES; i++) {
            // 0.5–1s delay per rubric line
            usleep(random_delay(500, 1000));

            // 20% chance to "fix" this rubric line
            if (rand() % 100 < 20) {
                char *comma = strchr(data->rubric[i], ',');
                if (comma != NULL && comma[1] != '\0') {
                    char old_char = comma[1];
                    comma[1] = old_char + 1;

                    printf("TA %d: Modified rubric line %d: '%c' -> '%c'\n",
                           ta_id, i + 1, old_char, comma[1]);
                    fflush(stdout);

                    // write updated rubric back to file (race condition possible!)
                    save_rubric(data, "rubric.txt");
                }
            }
        }

        // pick a question to mark
        for (int attempt = 0; attempt < 10; attempt++) {
            int q = rand() % MAX_QUESTIONS;
            if (data->questions_marked[q] == 0) {
                printf("TA %d: Accessing exam for student %d (exam idx %d)\n",
                       ta_id, data->student_id, data->current_exam_index);
                fflush(stdout);

                data->questions_marked[q] = 1;
                printf("TA %d: Marking question %d for student %d\n",
                       ta_id, q + 1, data->student_id);
                fflush(stdout);

                // 1–2s to mark
                usleep(random_delay(1000, 2000));

                printf("TA %d: Finished marking question %d for student %d\n",
                       ta_id, q + 1, data->student_id);
                fflush(stdout);
                break;
            }
        }

        // check if all questions for this exam are done
        int all_marked = 1;
        for (int i = 0; i < MAX_QUESTIONS; i++) {
            if (data->questions_marked[i] == 0) {
                all_marked = 0;
                break;
            }
        }

        if (all_marked) {
            int current_student = data->student_id;

            printf("TA %d: All questions marked for student %d\n",
                   ta_id, current_student);
            fflush(stdout);

            // stop condition: exam with student 9999
            if (current_student == 9999) {
                printf("TA %d: Reached student 9999, finishing\n", ta_id);
                fflush(stdout);
                data->finished = 1;
                break;
            }

            // move to the next exam (race condition possible!)
            int next_idx = data->current_exam_index + 1;
            if (next_idx >= NUM_EXAMS) {
                data->finished = 1;
                break;
            }

            data->current_exam_index = next_idx;

            // Safety check before array access
            if (data->current_exam_index < NUM_EXAMS) {
                load_exam(data, exam_files[data->current_exam_index]);
                printf("TA %d: Moving to next exam (student %d)\n",
                       ta_id, data->student_id);
                fflush(stdout);
            }
        }

        // small delay so output is not insane
        usleep(50000);
    }

    printf("TA %d: Stopped\n", ta_id);
    fflush(stdout);
}

/* -------------------- main -------------------- */

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

    printf("Starting with %d TAs\n", num_tas);
    fflush(stdout);

    // shared memory for SharedData
    SharedData *data = mmap(NULL, sizeof(SharedData),
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    memset(data, 0, sizeof(SharedData));
    data->current_exam_index = 0;
    data->finished = 0;

    // load rubric from file into shared memory
    load_rubric(data, "rubric.txt");
    printf("Rubric loaded:\n");
    for (int i = 0; i < MAX_RUBRIC_LINES; i++) {
        printf("  %s\n", data->rubric[i]);
    }
    fflush(stdout);

    // load the first exam from file into shared memory
    load_exam(data, exam_files[data->current_exam_index]);
    printf("First exam loaded: student %d\n", data->student_id);
    fflush(stdout);

    // fork TAs
    pid_t *pids = malloc(sizeof(pid_t) * num_tas);
    if (!pids) {
        perror("malloc");
        return 1;
    }

    for (int i = 0; i < num_tas; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            return 1;
        } else if (pids[i] == 0) {
            ta_process(i + 1, data);
            exit(0);
        }
    }

    // parent waits for all children
    for (int i = 0; i < num_tas; i++) {
        waitpid(pids[i], NULL, 0);
    }

    printf("\nAll TAs finished\n");
    printf("Final rubric:\n");
    for (int i = 0; i < MAX_RUBRIC_LINES; i++) {
        printf("  %s\n", data->rubric[i]);
    }

    munmap(data, sizeof(SharedData));
    free(pids);
    return 0;
}
