#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define NRM "\033[1;0m"
#define RED "\033[1;31m"
#define GRN "\033[1;32m"
#define YEL "\033[1;33m"
#define BLU "\033[1;34m"
#define UMAG "\e[4;35m"
#define BCYN "\e[1;36m"
#define BBLU "\033[0;34m"
#define MAG "\033[1;35m"
#define CYN "\033[1;36m"
#define WHT "\033[1;37m"
#define BHGRN "\e[1;92m"
#define HBLK "\e[0;90m"
#define HBLU "\e[0;94m"

#define N 1000
// max of n,m,o

int n, accept=0, m, o;

/*
n : number of pharmaceutical companies
m : vaccination zones
o : students
*/
int min(int a, int b) {
    return a < b ? a : b;
}

typedef struct Student {
    int id;
    int res;
    pthread_mutex_t mutex;
    pthread_t thread;
} Student;

typedef struct Pharma {
    int id;
    int batches;    // r
    int capacity;   // p
    int vaccines;   // r*p
    double prob;    // x
    pthread_mutex_t mutex;
    pthread_t thread;
} Pharma;

typedef struct Zone {
    int id;
    int slots;
    int filled;
    Student *group[8];
    pthread_mutex_t mutex;
    pthread_t thread;
    Pharma *pharma;
} Zone;

Pharma pharmas[N];

Zone zones[N];

Student students[N];

void *pharma_thread(void *args) {
    Pharma *pharma = (Pharma *) args;

    while (1) {
        int w = rand() % 4 + 2; // time taken to prepare batches

        pthread_mutex_lock(&pharma->mutex);

        pharma->capacity = rand() % 10 + 10; // capacity of each batch
        pharma->batches = rand() % 5 + 1; // number of batches

        printf("%sPharmaceutical Company %d is preparing %d batches "
               "which have success probability %.2lf\n%s", MAG, pharma->id,
               pharma->batches, pharma->prob, NRM);

        sleep(w); // preparation time
        pharma->vaccines = pharma->batches * pharma->capacity;

        printf("%sPharmaceutical Company %d has prepared %d batches, each with %d vaccines, "
               "which have success probability %.2lf.\nWaiting for all the "
               "vaccines to be used to resume production\n%s", BLU, pharma->id, pharma->batches,
               pharma->capacity, pharma->prob, NRM);

        pthread_mutex_unlock(&pharma->mutex);

        while (pharma->vaccines > 0);
        // busy waiting

        printf("%sAll the vaccines prepared by Pharmaceutical Company %d are emptied. "
               "Resuming production now.%s\n",MAG,pharma->id,NRM);
    }

    return NULL;
};

void vac_phase(Zone *zone) {

    Pharma *pharma = zone->pharma;

    pthread_mutex_lock(&pharma->mutex);

    for (int i = 0; i < zone->filled; i++) {
        Student *student = zone->group[i];

        pthread_mutex_lock(&student->mutex);
        double test = (float) rand() / (float) RAND_MAX;
        pharma->vaccines--;

        if (test < pharma->prob)
            student->res = 1;
        else
            student->res = 0;

        pthread_mutex_unlock(&student->mutex);

        printf("%sStudent %d on Vaccination Zone %d has been vaccinated "
               "which has success probability %.2lf\n%s", CYN, student->id,
               zone->id, pharma->prob, NRM);

        sleep(1); // time for vaccine to take effect

        if (student->res == 1)
            printf("%sStudent %d has tested POSITIVE for antibodies.%s\n", BLU, student->id, NRM);
        else if (student->res == 0)
            printf("%sStudent %d has tested NEGATIVE for antibodies.%s\n", MAG, student->id, NRM);
    }
    pthread_mutex_unlock(&pharma->mutex);

    zone->filled = 0;
    zone->slots = 0;
}

void *zone_thread(void *args) {

    Zone *zone = (Zone *) (args);

    while (1) {
        pthread_mutex_lock(&zone->mutex);
        zone->slots = 0;
        zone->filled = 0;
        zone->pharma = NULL;
        pthread_mutex_unlock(&zone->mutex);

        for (int i = 0; i < n; i++) {
            pthread_mutex_lock(&pharmas[i].mutex);

            if (pharmas[i].batches > 0) {
                pharmas[i].batches--;

                zone->pharma = &pharmas[i];
                pthread_mutex_unlock(&pharmas[i].mutex);
                break;
            }

            pthread_mutex_unlock(&pharmas[i].mutex);
        }

        if (zone->pharma == NULL)
            continue;

        printf("%sPharmaceutical Company %d is delivering a vaccine batch to Vaccination "
               "Zone %d which has success probability %.2lf%s\n", WHT, zone->pharma->id, zone->id, zone->pharma->prob   ,NRM);

        sleep(2); //delivery time

        printf("%sPharmaceutical Company %d has delivered vaccines to Vaccination Zone %d, "
               "resuming vaccinations now%s\n", WHT, zone->pharma->id, zone->id, NRM);

        int qty = (zone->pharma)->capacity;
        while (qty) {
            pthread_mutex_lock(&zone->mutex);
            zone->slots = min(8, qty);
            pthread_mutex_unlock(&zone->mutex);

            printf("%sVaccination Zone %d is ready to vaccinate with %d slots%s\n",
                   CYN, zone->id, zone->slots, NRM);

            while (zone->filled <= 0) {
                sleep(5); // waiting for slots to be filled
            }

            printf("%sVaccination Zone %d entering Vaccination Phase "
                   "with %d students\n%s", CYN, zone->id, zone->filled, NRM);
            pthread_mutex_lock(&zone->mutex);
            qty -= zone->filled;
            printf("%d vaccines left in Zone %d\n", qty, zone->id);
            vac_phase(zone);
            pthread_mutex_unlock(&zone->mutex);
        }
        printf("%sVaccination Zone %d has run out of vaccines%s\n", BBLU, zone->id, NRM);
    }

    return NULL;
};

void *student_thread(void *args) {
    Student *student = (Student *) args;
    int arrival_time = rand() % 10;
    int trial = 1;
    sleep(arrival_time);

    while (trial <= 3) {
        printf("%sStudent %d has arrived for round %d of Vaccination%s\n", YEL, student->id, trial, NRM);
        trial++;
        pthread_mutex_lock(&student->mutex);
        student->res = -1;
        pthread_mutex_unlock(&student->mutex);
        sleep(2); //chilling: rest period
        printf("%sStudent %d is waiting to be allocated a slot on a Vaccination Zone%s\n", RED, student->id, NRM);

        for (int i = 0; i < m; i = (i + 1) % m) {
            pthread_mutex_lock(&zones[i].mutex);

            if (zones[i].filled < zones[i].slots) {
                zones[i].group[zones[i].filled] = student;
                zones[i].filled++;
                printf("%sStudent %d assigned a slot on the Vaccination Zone %d "
                       "and waiting to be vaccinated%s\n", GRN, student->id, zones[i].id, NRM);
                pthread_mutex_unlock(&zones[i].mutex);

                while (student->res == -1);
                //busy waiting for result

                if (student->res == 1) {
                    accept++;
                    return NULL;
                }
                else if (student->res == 0) {
                    break;
                }
            }
            else
                pthread_mutex_unlock(&zones[i].mutex);
        }
    }

    printf("%sStudent %d was sent HOME\n%s", RED, student->id, NRM);

    return NULL;
};

int main() {
    srand(time(0));
    printf("%sEnter the number of Pharmaceutical Companies, "
           "Vaccination zones and Students: \n%s", BLU, NRM);
    scanf("%d %d %d", &n, &m, &o);

    if (n > 0)
        printf("%sEnter the probabilities of success rate "
               "for each of the Pharmaceutical Companies: %s\n", BLU, NRM);

    for (int i = 0; i < n; i++) {
        pharmas[i].id = i + 1;
        scanf("%lf", &pharmas[i].prob);
        pthread_mutex_init(&pharmas[i].mutex, NULL);
    }
    for (int i = 0; i < m; i++) {
        zones[i].id = i + 1;
        pthread_mutex_init(&zones[i].mutex, NULL);
    }
    for (int i = 0; i < o; i++) {
        students[i].id = i + 1;
        pthread_mutex_init(&pharmas[i].mutex, NULL);
    }

    if (n == 0 || m == 0)
        printf("No Vaccines Available for Students--> %sNO SIMULATION%s\n", RED, NRM);
    else if (o == 0)
        printf("No Students Available--> %sNO SIMULATION%s\n", RED, NRM);
    else {
        printf("\n%sBeginning Simulation!\n\n%s", BHGRN, NRM);

        for (int i = 0; i < n; i++)
            pthread_create(&(pharmas[i].thread), NULL, pharma_thread, &pharmas[i]);
        for (int i = 0; i < m; i++)
            pthread_create(&(zones[i].thread), NULL, zone_thread, &zones[i]);
        for (int i = 0; i < o; i++)
            pthread_create(&(students[i].thread), NULL, student_thread, &students[i]);

        for (int i = 0; i < o; i++)
            pthread_join(students[i].thread, 0);

        sleep(1);

        printf("%d students attend college!\n",accept);
        printf("\n%sSimulation Over!\n%s", BHGRN, NRM);
    }

    for (int i = 0; i < n; i++)
        pthread_mutex_destroy(&(pharmas[i].mutex));
    for (int i = 0; i < m; i++)
        pthread_mutex_destroy(&(zones[i].mutex));

    return 0;
}