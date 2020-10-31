# **COVID-19 : The Vaccination Process**

We have 3 basic enities:  
* The companies which manufacture these vaccines (n)
* The zones which recieve batches from the companies (m)
* The students who get vaccinated in the zones (o)

Command to run the program:
```
$ gcc -pthread q2.c -o vaccines
$ ./vaccines
```
_Input_ >> **n,m,o**

## Main
Basic initialisation of each of the above entities.
```c
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
```

## Pharmaceutical companies
Each company manufactures a random number of batches `r`, each having some 
random number of vaccines `p`. It busy waits on the total count of vaccines `r*p`
before resuming to prepare another drill of batches.

```c
    while (pharma->vaccines > 0);
        // busy waiting
```

## Zone
Each zone loops over the pharma companies to check if they have a batch that can
be delivered to it. When it finds such a batch, it has a certain capacity which is
the total vaccines that it has. Now in each iteration it waits for a certain while
till it recieves atleast one student. Now with the required vaccine count, it begins
its vaccination phase.

The zone has the ids of each student in its vaccination phase. It loops over them,
and gives them a vaccine each. The total count of the vaccines for the pharma company
is decremented by 1. The Zone also maintains which pharma the vaccines have come from
and hence their probability of working `prob`.

```c
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
```

## Student
Each student has his own thread. They have a random arrival time.
After some while they start waiting to be alloted a seat in the zones.
We loop over the zones continuously till we find some zone where the number of
`filled` seats are less than the `slots` in the zone. 

After being allocated a seat, he gets vaccined and then tested for antibodies.
If he fails, he again starts looping over the zones to get a slot.
If he fails thrice, he is sent home.

```c
        zones[i].group[zones[i].filled] = student;
        zones[i].filled++;
        printf("%sStudent %d assigned a slot on the Vaccination Zone %d "
               "and waiting to be vaccinated%s\n", GRN, student->id, zones[i].id, NRM);
        pthread_mutex_unlock(&zones[i].mutex);

        while (student->res == -1);
        //busy waiting for result
```
