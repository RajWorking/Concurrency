#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>

#define NRM "\033[1;0m"
#define RED "\033[1;31m"
#define GRN "\033[1;32m"
#define YEL "\033[1;33m"
#define BLU "\033[1;34m"
#define MAG "\033[1;35m"
#define WHT "\033[1;37m"
#define CYN "\033[1;36m"
#define BHBLK "\e[1;90m"

#define N 1000  //max entities
#define M 500   //max length of names

typedef struct Player {
    char name[M];
    char instrument[M];
    int eligible; // 0:none, 1:acoustic, 2:electric, 3:both, 4:singer
    int arrival_time;
    int perform_time;
    int st; // 0-7
    int stage_type;
    pthread_mutex_t mutex;
    sem_t chance;     // did the player perform?
    sem_t slave;      // singer as a partner
    pthread_t thread;   // primary thread
    pthread_t aco_th;   // child thread acoustic
    pthread_t ele_th;   // child thread electric
    pthread_t sing_th;  // child thread singer
} Player;

typedef struct Stage {
    int id;
    int available;
    int type;
} Stage;

Stage stages[2 * N];

Player players[N];

pthread_mutex_t stage_mutex;

char *statuses[8] = {"Not yet arrived", "Waiting to perform",
                     "Performing Solo", "Performing with Singer",
                     "Performing with Musician", "Waiting for t-shirt",
                     "T-shirt being collected", "Exited"};

int k, e, a, c, t1, t2, t;

pthread_mutex_t collab;

sem_t acoustic_stages, electric_stages, singer_stages, coordinators;

// singer_stages: not solo

void chk() {
    int aq, eq, sq;
    sem_getvalue(&acoustic_stages, &aq);
    sem_getvalue(&electric_stages, &eq);
    sem_getvalue(&singer_stages, &sq);
    printf("a: %d , e: %d , sing: %d\n", aq, eq, sq);
}

void *perform_aco(void *args) {
    Player *player = (Player *) args;

    if (a == 0)
        return NULL;
    sem_wait(&acoustic_stages);

    pthread_mutex_lock(&player->mutex);

    if (player->st == 1) {
        player->st = 2;
        player->stage_type = 1;
        sem_post(&player->chance);
        pthread_mutex_unlock(&player->mutex);
        return NULL;
    }
    pthread_mutex_unlock(&player->mutex);

    sem_post(&acoustic_stages);

    return NULL;
}

void *perform_ele(void *args) {
    Player *player = (Player *) args;

    if (e == 0)
        return NULL;
    sem_wait(&electric_stages);

    pthread_mutex_lock(&player->mutex);

    if (player->st == 1) {
        player->st = 2;
        player->stage_type = 2;
        sem_post(&player->chance);
        pthread_mutex_unlock(&player->mutex);
        return NULL;
    }
    pthread_mutex_unlock(&player->mutex);

    sem_post(&electric_stages);

    return NULL;
}

void *singing(void *args) {
    Player *player = (Player *) args;

    sem_wait(&singer_stages);
    pthread_mutex_lock(&collab);

    pthread_mutex_lock(&player->mutex);

    if (player->st == 1) {
        player->st = 4;
        player->stage_type = 4;
        sem_post(&player->chance);
        pthread_mutex_unlock(&player->mutex);
        return NULL;
    }

    sem_post(&singer_stages);
    pthread_mutex_unlock(&player->mutex);
    pthread_mutex_unlock(&collab);
    return NULL;
}

void *player_thread(void *args) {

    Player *player = (Player *) args;

    sleep(player->arrival_time);

    struct timespec ts;

    printf("%s%s has arrived (%s)%s\n", GRN, player->name, player->instrument, NRM);
    player->st = 1;

    if (player->eligible == 4)
        pthread_create(&player->sing_th, 0, singing, player);
    if (player->eligible == 1 || player->eligible == 3 || player->eligible == 4)
        pthread_create(&player->aco_th, 0, perform_aco, player);
    if (player->eligible == 2 || player->eligible == 3 || player->eligible == 4)
        pthread_create(&player->ele_th, 0, perform_ele, player);

    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    ts.tv_sec += t;

    if (sem_timedwait(&player->chance, &ts) == -1) {
        if (errno == ETIMEDOUT) {
            printf("%s%s left because of impatience%s\n", BLU, player->name, NRM);
            player->st = 7;
        }
        else {
            perror("Error in timed wait");
            player->st = -1;
        }
        return NULL;
    }

    if (player->stage_type == 4) {
        for (int i = 0; i < k; i++) {
            pthread_mutex_lock(&players[i].mutex);
            if (players[i].st == 2 && players[i].eligible != 4) {
                players[i].st = 3;
                pthread_mutex_unlock(&players[i].mutex);
                pthread_mutex_unlock(&collab);

                printf("%s%s joined %s's performance, extended by 2 secs %s\n",
                       CYN, player->name, players[i].name, NRM);
                sem_wait(&players[i].slave);
                break;
            }
            pthread_mutex_unlock(&players[i].mutex);
        }
    }
    else {
        int stage_perform = 0;
        pthread_mutex_lock(&stage_mutex);
        for (int i = 0; i < (a + e); i++) {
            if (stages[i].available == 1 && stages[i].type == player->stage_type) {
                stage_perform = stages[i].id;
                stages[i].available = 0;
                break;
            }
        }
        pthread_mutex_unlock(&stage_mutex);

        player->perform_time = (rand() % (t2 - t1 + 1)) + t1;

        if (player->stage_type == 1)
            printf("%s%s performing %s at acoustic stage (%d) for %d sec%s\n",
                   YEL, player->name, player->instrument, stage_perform, player->perform_time, NRM);
        else if (player->stage_type == 2)
            printf("%s%s performing %s at electric stage (%d) for %d sec%s\n",
                   YEL, player->name, player->instrument, stage_perform, player->perform_time, NRM);

        if (player->eligible != 4) {
            sem_post(&singer_stages);
        }

        sleep(player->perform_time);

        if (player->eligible != 4) {
            pthread_mutex_lock(&collab);
            if (player->st == 3) {
                pthread_mutex_unlock(&collab);
                sleep(2);
                sem_post(&player->slave);
            }
            else if (player->st == 2) {
                sem_wait(&singer_stages);
                player->st = 5;
                pthread_mutex_unlock(&collab);
            }
            else pthread_mutex_unlock(&collab);
        }

        if (player->stage_type == 1) {
            printf("%s%s performance at acoustic stage (%d) ended%s\n",
                   BHBLK, player->name, stage_perform, NRM);
            sem_post(&acoustic_stages);
        }
        else if (player->stage_type == 2) {
            printf("%s%s performance at electric stage (%d) ended%s\n",
                   BHBLK, player->name, stage_perform, NRM);
            sem_post(&electric_stages);
        }
        pthread_mutex_lock(&stage_mutex);
        stages[stage_perform - 1].available = 1;
        pthread_mutex_unlock(&stage_mutex);
    }

    pthread_mutex_lock(&player->mutex);
    printf("%s%s waiting for t-shirt%s\n", WHT, player->name, NRM);
    player->st = 5;
    sem_wait(&coordinators);
    player->st = 6;
    sleep(2);   //time taken by coordinator
    sem_post(&coordinators);
    printf("%s%s collected t-shirt%s\n", MAG, player->name, NRM);
    pthread_mutex_unlock(&player->mutex);

    player->st = 7;
    return NULL;
}

int main() {
    srand(time(0));
    scanf("%d %d %d %d %d %d %d", &k, &a, &e, &c, &t1, &t2, &t);

    sem_init(&acoustic_stages, 0, a);
    sem_init(&electric_stages, 0, e);
    sem_init(&coordinators, 0, c);
    sem_init(&singer_stages, 0, 0);

    for (int i = 0; i < a + e; i++) {
        stages[i].id = i + 1;
        stages[i].available = 1;
        if (i < a)
            stages[i].type = 1;
        else
            stages[i].type = 2;
    }

    char ins;
    for (int i = 0; i < k; i++) {
        scanf("%s %c %d", players[i].name, &ins, &players[i].arrival_time);
        pthread_mutex_init(&players[i].mutex, NULL);
        players[i].st = 0;
        players[i].stage_type = 0;
        sem_init(&players[i].chance, 0, 0);
        sem_init(&players[i].slave, 0, 0);

        switch (ins) {
            case 'p':
                strcpy(players[i].instrument, "piano");
                players[i].eligible = 3;
                break;
            case 'g':
                strcpy(players[i].instrument, "guitar");
                players[i].eligible = 3;
                break;
            case 'v':
                strcpy(players[i].instrument, "violin");
                players[i].eligible = 1;
                break;
            case 'b':
                strcpy(players[i].instrument, "bass");
                players[i].eligible = 2;
                break;
            case 's':
                strcpy(players[i].instrument, "singing");
                players[i].eligible = 4;
                break;
            default:
                sscanf(players[i].instrument, "undefined");
                players[i].eligible = 0;
        }
    }

    for (int i = 0; i < k; i++)
        pthread_create(&players[i].thread, 0, player_thread, &players[i]);

    for (int i = 0; i < k; i++)
        pthread_join(players[i].thread, 0);

    printf("\n%sFinished%s\n", RED, NRM);

    return 0;
}