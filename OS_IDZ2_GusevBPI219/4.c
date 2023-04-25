#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>

#define SHM_KEY 6666
#define SEMR_KEY 7777
#define SEMW_KEY 8888

struct Island {
    int **map, x, y;
    bool is_found;
};

int sem_read, sem_write;
struct Island *data;
int Shmid;

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

void initialize_child_process(int group, int sem_read, int sem_write, struct Island *data) {
    struct sembuf sem_buf;
    int pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (!pid) {
        while (true) {
            sleep(rand() % 3 + 1);
            sem_buf.sem_num = 0;
            sem_buf.sem_flg = 0;
            sem_buf.sem_op = -1;
            semop(sem_read, &sem_buf, 1);
            if (data->is_found == true) {
                sem_buf.sem_op = 1;
                semop(sem_write, &sem_buf, 1);
                exit(EXIT_SUCCESS);
            }
            if (data->y < 0) {
                data->y = 0;
            }
            if (!data->map[data->x][data->y]) {
                printf("Group %d, region {%d|%d}, did not find the treasure :(\n", group + 1, data->x, data->y);
                fflush(stdout);
                sem_buf.sem_op = 1;
                semop(sem_write, &sem_buf, 1);
            } else {
                data->is_found = true;
                printf("Group %d, region {%d|%d}, found the treasure! Was found %d gold!\n", group + 1, data->x, data->y, data->map[data->x][data->y]);
                fflush(stdout);
                sem_buf.sem_op = 1;
                semop(sem_write, &sem_buf, 1);
                exit(EXIT_SUCCESS);
            }
        }
    }
}

void parent_process(int sem_read, int sem_write, int width, struct Island *data) {
    struct sembuf sem_buf;
    for (; data->is_found == false; ++data->x) {
        for (data->y = -1; data->is_found == false && data->y < width; ++data->y) {
            sem_buf.sem_num = 0;
            sem_buf.sem_flg = 0;
            sem_buf.sem_op = -1;
            semop(sem_write, &sem_buf, 1);
            sem_buf.sem_op = 1;
            semop(sem_read, &sem_buf, 1);
        }
    }
}

void handle_sigint(int sig) {
    printf("Exiting [%d]\n", sig);
    semctl(sem_read, 0, IPC_RMID, 0);
    semctl(sem_write, 0, IPC_RMID, 0);
    shmdt(data);
    shmctl(Shmid, IPC_RMID, NULL);
    killpg(getpgid(getpid()), 0);
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

    struct Island island;
    initialize_island_struct(&island, length, width);
    Shmid = shmget(SHM_KEY, sizeof(island), IPC_CREAT | 0666);
    if (Shmid == -1) {
        perror("shmget");
        exit(1);
    }
    data = (struct Island *) shmat(Shmid, NULL, 0);
    initialize_island_struct(data, length, width);
    if (data == (struct Island *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    initialize_data(data, length, width);
    sem_read = semget(SEMR_KEY, 1, IPC_CREAT | 0666);
    if (sem_read == -1) {
        perror("semget() failed");
        exit(EXIT_FAILURE);
    }
    semctl(sem_read, 0, SETVAL, 0);
    sem_write = semget(SEMW_KEY, 1, IPC_CREAT | 0666);
    if (sem_write == -1) {
        perror("semget() failed");
        exit(EXIT_FAILURE);
    }
    semctl(sem_write, 0, SETVAL, 1);
    for (int i = 0; i < k; ++i) {
        initialize_child_process(i, sem_read, sem_write, data);
    }
    parent_process(sem_read, sem_write, width, data);
    semctl(sem_read, 0, IPC_RMID, 0);
    semctl(sem_write, 0, IPC_RMID, 0);
    shmdt(data);
    shmctl(Shmid, IPC_RMID, NULL);
    killpg(getpgid(getpid()), 0);
    return 0;
}
