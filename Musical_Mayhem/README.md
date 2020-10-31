# **Musical Mayhem**

The program takes input the values of:
* the number of performers (k)
* the number of acoustic stages (a)
* the number of electric stages (e)
* the number of coordinators (c)
* the time range of performance [t1, t2]
* the max waiting time of a performer (t)

We now simulate the following:  
Musicians getting assigned to stages. Singers can go solo or 
perform with them.  
After each performance the musician gets a t-shirt for participating.

Command to run the program:
```
$ gcc -pthread q3.c -o musical
$ ./musical
```

## Main
1. Most initialisations happen here. The semaphores control access to:
    1. acoustic_stages: number of acoustic stages.
    2. electric_stages: number of electric stages.
    3. coordinators: coordinators to get a t-shirt.
    4. singer_stages: stages where solo musicians are 
                      performing so singer may join.
2. Stages are assigned as either acoustic or electric. They are made available.
3. There exists a thread for each player and they all run concurrently. 

```c
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
        ----------------------
    pthread_mutex_init(&players[i].mutex, NULL);
    players[i].st = 0;
    players[i].stage_type = 0;
    sem_init(&players[i].chance, 0, 0);
    sem_init(&players[i].slave, 0, 0);
```

## Player thread

Each player has a main thread and 3 child threads: aco_th, ele_th, sing_th.
The player's status `st` takes values 0-7 upon various transitions. Each has his own
`mutex` lock to ensure atomic change in its properties. 
Depending upon the `eligibility` of the player, he can try to gain access of 
the acoustic and electric stages or perform with an already performing musician. 

In the parent thread, the player performs a timed wait on its semaphore `chance`
which tells if the player exceeded his waiting time (t) . This semaphore is signalled in
child threads.

Now, when the player performs, he increases the value of the semaphore `singer_stages`
to denote that an available singer can perform with him. Now he performs for some
time between t1 and t2.

After the performance, he checks if his status has been changed and if no singer
has collaborated with him, he decreases the semaphore `singer_stage`, else he extends
the performance time by 2 seconds.

The semaphore `collab` ensures mutual exclusion between the performer and his singer
partner.

### Singer child-thread for collaboration

When singer finds a solo musician, he locks `collab` and loops over the range of players
to find id of the musician who is available to join. He then waits for that musician
to release the semaphore `'slave` so that both can leave the stage together after
the extended performance time.
 
### T-shirt collection

Each player waits for some coordinator to become free and after collecting, makes
that coordinator available
```c
    pthread_mutex_lock(&player->mutex);
    printf("%s%s waiting for t-shirt%s\n", WHT, player->name, NRM);
    player->st = 5;
    sem_wait(&coordinators);
    player->st = 6;
    sleep(2);   //time taken by coordinator
    sem_post(&coordinators);
    printf("%s%s collected t-shirt%s\n", MAG, player->name, NRM);
    pthread_mutex_unlock(&player->mutex);
```

### Stage access

Once the musician knows that the acoustic/electric stage is free,
He loops over them to get the id of the free stage and later makes it available 
again.

```c
        pthread_mutex_lock(&stage_mutex);
        for (int i = 0; i < (a + e); i++) {
            if (stages[i].available == 1 && stages[i].type == player->stage_type) {
                stage_perform = stages[i].id;
                stages[i].available = 0;
                break;
            }
        }
        pthread_mutex_unlock(&stage_mutex);

    -----------

        pthread_mutex_lock(&stage_mutex);
          stages[stage_perform - 1].available = 1;
        pthread_mutex_unlock(&stage_mutex);
```
