#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>

#include "helpers.h"

#define STUDENT_ID_LAST_DIGIT 5
#define SHM_KEY 0x1235
#define SHM_SIZE (1024 * 1024)
#define MAX_FILENAME_LEN 256
#define MAX_FILES 100

#define SEM_PARENT_WRITE "/sem_parent_write_p2b"
#define SEM_CHILD_READ "/sem_child_read_p2b"

typedef struct {
    char filename[MAX_FILENAME_LEN];
    long data_size;
    int is_last_chunk;
    int is_end_signal;
    char data[SHM_SIZE];
} SharedData;

typedef struct {
    char paths[MAX_FILES][MAX_FILENAME_LEN];
    int count;
} FileList;

void traverseDirRecursive(const char *dir_name, FileList *file_list, int current_depth);

void traverseDir(char *dir_name) {
    printf("Note: Directory traversal initiated before fork.\n");
}

int main(int argc, char **argv) {
    int process_id;
    int shm_fd = -1;
    SharedData *shm_ptr = (SharedData *)-1;
    sem_t *sem_parent = SEM_FAILED;
    sem_t *sem_child = SEM_FAILED;
    FileList file_list;

    char *dir_name = argv[1];

    if (argc < 2) {
        printf("Main process: Please enter a source directory name.\nUsage: ./problem2 <dir_name>\n");
        exit(-1);
    }

    file_list.count = 0;
    printf("Main process: Traversing directory '%s'...\n", dir_name);
    traverseDirRecursive(dir_name, &file_list, 0);
    printf("Main process: Found %d text files.\n", file_list.count);
    if (file_list.count == 0) {
        printf("Main process: No .txt files found. Exiting.\n");
        exit(0);
    }

    shm_fd = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_fd < 0) {
        perror("shmget failed");
        exit(-1);
    }
    shm_ptr = (SharedData *)shmat(shm_fd, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("shmat failed");
        shmctl(shm_fd, IPC_RMID, NULL);
        exit(-1);
    }
    printf("Main process: Shared memory created and attached.\n");

    sem_unlink(SEM_PARENT_WRITE);
    sem_unlink(SEM_CHILD_READ);

    sem_parent = sem_open(SEM_PARENT_WRITE, O_CREAT | O_EXCL, 0666, 1);
    if (sem_parent == SEM_FAILED) {
        perror("sem_open (parent) failed");
        shmdt(shm_ptr);
        shmctl(shm_fd, IPC_RMID, NULL);
        exit(-1);
    }
    sem_child = sem_open(SEM_CHILD_READ, O_CREAT | O_EXCL, 0666, 0);
    if (sem_child == SEM_FAILED) {
        perror("sem_open (child) failed");
        sem_close(sem_parent);
        sem_unlink(SEM_PARENT_WRITE);
        shmdt(shm_ptr);
        shmctl(shm_fd, IPC_RMID, NULL);
        exit(-1);
    }
    printf("Main process: Semaphores created.\n");

    process_id = fork();

    switch (process_id) {

    default:
        printf("Parent process: My ID is %jd\n", (intmax_t) getpid());

        for (int i = 0; i < file_list.count; ++i) {
            const char *filepath = file_list.paths[i];
            FILE *file = fopen(filepath, "rb");
            if (!file) {
                fprintf(stderr, "Parent process: Failed to open file %s, skipping.\n", filepath);
                continue;
            }

            long bytes_read;
            int is_last = 0;
            do {
                if (sem_wait(sem_parent) == -1) {
                    is_last = 1;
                    break;
                }

                bytes_read = fread(shm_ptr->data, 1, SHM_SIZE, file);
                if (ferror(file)) {
                    fprintf(stderr, "Parent process: Error reading file %s\n", filepath);
                    shm_ptr->data_size = 0;
                    shm_ptr->is_last_chunk = 1;
                    shm_ptr->is_end_signal = 0;
                    clearerr(file);
                    if (sem_post(sem_child) == -1) perror("Parent sem_post failed after read error");
                    is_last = 1;
                    break;
                }

                is_last = feof(file);

                strncpy(shm_ptr->filename, filepath, MAX_FILENAME_LEN - 1);
                shm_ptr->filename[MAX_FILENAME_LEN - 1] = '\0';
                shm_ptr->data_size = bytes_read;
                shm_ptr->is_last_chunk = is_last;
                shm_ptr->is_end_signal = 0;

                if (sem_post(sem_child) == -1) {
                    is_last = 1;
                    break;
                }

            } while (!is_last);

            fclose(file);
            int sem_val;
            if (errno == EINTR || (sem_getvalue(sem_child, &sem_val) == -1 && errno != 0)) {
                break;
            }
        }

        if (sem_wait(sem_parent) == -1) perror("Parent sem_wait (end signal) failed");
        shm_ptr->is_end_signal = 1;
        shm_ptr->data_size = 0;
        if (sem_post(sem_child) == -1) perror("Parent sem_post (end signal) failed");

        wait(NULL);

        sem_close(sem_parent);
        sem_close(sem_child);
        sem_unlink(SEM_PARENT_WRITE);
        sem_unlink(SEM_CHILD_READ);
        shmdt(shm_ptr);
        shmctl(shm_fd, IPC_RMID, NULL);

        printf("Parent process: Finished.\n");
        break;

    case 0:
        printf("Child process: My ID is %jd\n", (intmax_t) getpid());

        SharedData *child_shm_ptr = (SharedData *)shmat(shm_fd, NULL, 0);
        if (child_shm_ptr == (void *)-1) {
            perror("Child shmat failed");
            exit(-1);
        }
        sem_t *child_sem_parent = sem_open(SEM_PARENT_WRITE, 0);
        if (child_sem_parent == SEM_FAILED) {
            perror("Child sem_open (parent) failed");
            shmdt(child_shm_ptr);
            exit(-1);
        }
        sem_t *child_sem_child = sem_open(SEM_CHILD_READ, 0);
        if (child_sem_child == SEM_FAILED) {
            perror("Child sem_open (child) failed");
            sem_close(child_sem_parent);
            shmdt(child_shm_ptr);
            exit(-1);
        }

        long total_word_count = 0;
        long current_file_word_count = 0;
        char matching_filename[MAX_FILENAME_LEN] = "";
        char current_processing_filename[MAX_FILENAME_LEN] = "";

        while (1) {
            if (sem_wait(child_sem_child) == -1) {
                perror("Child sem_wait failed");
                break;
            }

            if (child_shm_ptr->is_end_signal) {
                if (sem_post(child_sem_parent) == -1) perror("Child sem_post (ack end) failed");
                break;
            }

            if (child_shm_ptr->data_size > 0) {
                if (strlen(current_processing_filename) == 0 ||
                    strcmp(current_processing_filename, child_shm_ptr->filename) != 0) {
                    strncpy(current_processing_filename, child_shm_ptr->filename, MAX_FILENAME_LEN -1);
                    current_processing_filename[MAX_FILENAME_LEN -1] = '\0';
                    current_file_word_count = 0;
                }
                current_file_word_count += wordCount(child_shm_ptr->data, child_shm_ptr->data_size);
            }

            if (child_shm_ptr->is_last_chunk) {
                total_word_count += current_file_word_count;

                if ((current_file_word_count % 10) == STUDENT_ID_LAST_DIGIT) {
                    if (strlen(matching_filename) == 0) {
                        strncpy(matching_filename, current_processing_filename, MAX_FILENAME_LEN - 1);
                        matching_filename[MAX_FILENAME_LEN - 1] = '\0';
                    }
                }
                current_processing_filename[0] = '\0';
                current_file_word_count = 0;
            }

            if (sem_post(child_sem_parent) == -1) {
                perror("Child sem_post failed");
                break;
            }
        }

        printf("Child process: Total word count: %ld\n", total_word_count);
        if (saveResult("p2_result.txt", total_word_count) != 0) {
            fprintf(stderr, "Child process: Failed to save result.\n");
        } else {
            printf("Child process: Result saved to p2_result.txt\n");
        }

        if (strlen(matching_filename) > 0) {
            printf("Child process: File matching criteria (last digit %d): %s\n", STUDENT_ID_LAST_DIGIT, matching_filename);
        } else {
            printf("Child process: No file found with word count ending in %d.\n", STUDENT_ID_LAST_DIGIT);
        }

        sem_close(child_sem_parent);
        sem_close(child_sem_child);
        shmdt(child_shm_ptr);

        printf("Child process: Finished.\n");
        exit(0);

    case -1:
        printf("Fork failed!\n");
        if (sem_parent != SEM_FAILED) sem_close(sem_parent);
        if (sem_child != SEM_FAILED) sem_close(sem_child);
        sem_unlink(SEM_PARENT_WRITE);
        sem_unlink(SEM_CHILD_READ);
        if (shm_ptr != (void*)-1) shmdt(shm_ptr);
        if (shm_fd != -1) shmctl(shm_fd, IPC_RMID, NULL);
        exit(-1);
    }

    if (process_id > 0) {
    }

    exit(0);
}

void traverseDirRecursive(const char *dir_name, FileList *file_list, int current_depth) {
    if (current_depth > 10) {
        fprintf(stderr, "Warning: Max directory depth reached at %s\n", dir_name);
        return;
    }

    DIR *d = opendir(dir_name);
    if (!d) {
        perror("opendir failed");
        fprintf(stderr, "Failed path: %s\n", dir_name);
        return;
    }

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
            continue;
        }

        char full_path[MAX_FILENAME_LEN * 2];
        int len_needed = snprintf(full_path, sizeof(full_path), "%s/%s", dir_name, dir->d_name);
        if (len_needed >= sizeof(full_path)) {
            fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n", dir_name, dir->d_name);
            continue;
        }

        struct stat path_stat;
        if (stat(full_path, &path_stat) != 0) {
            continue;
        }

        if (S_ISDIR(path_stat.st_mode)) {
            traverseDirRecursive(full_path, file_list, current_depth + 1);
        } else if (S_ISREG(path_stat.st_mode)) {
            size_t name_len = strlen(dir->d_name);
            if (name_len > 4 && strcmp(dir->d_name + name_len - 4, ".txt") == 0) {
                if (file_list->count < MAX_FILES) {
                    strncpy(file_list->paths[file_list->count], full_path, MAX_FILENAME_LEN - 1);
                    file_list->paths[file_list->count][MAX_FILENAME_LEN - 1] = '\0';
                    file_list->count++;
                } else {
                    fprintf(stderr, "Warning: Maximum file limit (%d) reached. Skipping %s\n", MAX_FILES, full_path);
                }
            }
        }
    }
    closedir(d);
}