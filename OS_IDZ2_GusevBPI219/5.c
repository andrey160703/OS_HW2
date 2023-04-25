#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>

#define SHM_NAME "/island_shm"
#define SEM_WRITE_NAME "sem_write"
#define SEM_READ_NAME "sem_read"

struct Island {
    int **map, x, y;
    bool is_found;
};

sem_t *sem_write, *sem_read;
struct Island *data;
struct Island island;

void initialize_island_struct(struct Island* s, int length, int width) {
    s->map = malloc(length * sizeof(int *));
    for (int i = 0; i < length; ++i) {
        s->map[i] = malloc(width * sizeof(int));
    }
    s->x = 0;
    s->y = 0;
    s->is_found = false;
}

void initialize_data(struct Island* data, int length, int width) {
    int search_length = rand() % length;
    int search_width = rand() % width;
    int treasure = rand() % 12363;
    for (int i = 0; i < length; ++i) {
        for (int j = 0; j < width; ++j) {
            data->map[i][j] = 0;
        }
    }
    data->map[search_length][search_width] = treasure;
}


void initialize_child_process(int group, sem_t sem_read, sem_t sem_write, struct Island *data) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        while (true) {
            sleep(rand() % 3 + 1);
            sem_wait(sem_read);
            if (data->is_found == true) {
                sem_post(sem_write);
                exit(EXIT_SUCCESS);
            }
            if (data->y < 0) {
                data->y = 0;
            }
            if (!data->map[data->x][data->y]) {
                printf("Group %d, region {%d|%d}, did not find the treasure :(\n", group + 1, data->x, data->y);
                fflush(stdout);
                sem_post(sem_write);
            } else {
                data->is_found = true;
                printf("Group %d, region {%d|%d}, found the treasure! Was found %d gold!\n", group + 1, data->x, data->y, data->map[data->x][data->y]);
                fflush(stdout);
                sem_post(sem_write);
                exit(EXIT_SUCCESS);
            }
        }
    }
}

void parent_process(int sem_read, int sem_write, int width, struct Island *data) {
    for (; data->is_found == false; ++data->x) {
        for (data->y = -1; data->is_found == false && data->y < width; ++data->y) {
            sem_wait(sem_write);
            sem_post(sem_read);
        }
    }
}

void handle_sigint(int sig) {
    printf("Exiting [%d]\n", sig);
    sem_close(sem_write);
    sem_close(sem_read);
    sem_unlink(SEM_WRITE_NAME);
    sem_unlink(SEM_READ_NAME);
    munmap(data, sizeof(island));
    shm_unlink(SHM_NAME);
    killpg(getpgid(getpid()), sig);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    srand((int) time(0));
    signal(SIGINT, handle_sigint);
    int length, width, k;

    printf("Enter the length of the map(2-12): ");
    scanf("%d", &length);

    while (length < 2 || length > 12) {
        printf("Invalid length. Please enter a number between 2 and 12: ");
        scanf("%d", &length);
    }

    printf("Enter the width of the map(2-12): ");
    scanf("%d", &width);

    while (width < 2 || width > 12) {
        printf("Invalid width. Please enter a number between 2 and 12: ");
        scanf("%d", &width);
    }

    printf("Enter the number of groups (1-12): ");
    scanf("%d", &k);

    while (k < 1 || k > 12) {
        printf("Invalid number of groups. Please enter a number between 1 and 12: ");
        scanf("%d", &k);
    }

    int fd;
    shm_unlink(SHM_NAME);
    fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    initialize_island_struct(&island, length, width);
    ftruncate(fd, sizeof(island));
    data = mmap(NULL, sizeof(island), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    initialize_island_struct(data, length, width);
    if (data == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    initialize_data(data, length, width);
    sem_unlink(SEM_WRITE_NAME);
    sem_unlink(SEM_READ_NAME);
    sem_write = sem_open(SEM_WRITE_NAME, O_CREAT | O_RDWR, 0666, 1);
    if (sem_write == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    sem_read = sem_open(SEM_READ_NAME, O_CREAT | O_RDWR, 0666, 0);
    if (sem_read == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < k; ++i) {
        initialize_child_process(i, sem_read, sem_write, data);
    }
    parent_process(sem_read, sem_write, width, data);
    sem_close(sem_write);
    sem_close(sem_read);
    sem_unlink(SEM_WRITE_NAME);
    sem_unlink(SEM_READ_NAME);
    munmap(data, sizeof(island));
    shm_unlink(SHM_NAME);
    killpg(getpgid(getpid()), 0);
    return 0;
}