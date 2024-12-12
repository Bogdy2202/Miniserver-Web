#include "threadpool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

struct tpool_work 
{
    thread_func_t      func;// functia de executat
    void              *arg;// argumentul functiei(fd)--pentru a intra in handle client
    struct tpool_work *next;// pointer catre urmatorul task din coada
};
typedef struct tpool_work tpool_work_t;

struct tpool 
{
    tpool_work_t    *work_first; // primul task din coada
    tpool_work_t    *work_last; // ultimul task din coada --pentru a fii mai usor sa adaugam task uri in coada
    pthread_mutex_t  work_mutex; // pentru accesa sincronizat accesul la threaduri
    pthread_cond_t   work_cond; // conditie de a anunta task uri noi
    pthread_cond_t   working_cond; // conditie pentru a anunta terminarea task urilor
    size_t           working_cnt;// task uri in lucru
    size_t           thread_cnt;// thread uri in lucru
    bool             stop;
};


static tpool_work_t *tpool_work_create(thread_func_t func, void *arg)
{
    tpool_work_t *work;

    if (func == NULL)
    {
        return NULL;
    }

    work       = malloc(sizeof(*work));
    work->func = func;
    work->arg  = arg;
    work->next = NULL;

    return work;
}

static void tpool_work_destroy(tpool_work_t *work)
{
    if (work == NULL)
    {
        return;
    }
    free(work);
}

static tpool_work_t *tpool_work_get(tpool_t *tm)
{
    tpool_work_t *work;

    if (tm == NULL)
    {
        return NULL;
    }

    work = tm->work_first;
    if (work == NULL)
    {
        return NULL;
    }

    if (work->next == NULL) 
    {
        tm->work_first = NULL;
        tm->work_last  = NULL;
    } 
    else 
    {
        tm->work_first = work->next;
    }

    return work;
}

// functie executata pentru fiecare thread din pool
static void *tpool_worker(void *arg)
{
    tpool_t      *tm = arg;
    tpool_work_t *work;

    while (1) 
    {
        pthread_mutex_lock(&(tm->work_mutex));

        // asteapta task uri sau oprirea pool ului
        while (tm->work_first == NULL && !tm->stop)
        {   
            pthread_cond_wait(&(tm->work_cond), &(tm->work_mutex));
        }

        if (tm->stop)
        {
            break;
        }

        // preia task
        work = tpool_work_get(tm);
        tm->working_cnt++;
        pthread_mutex_unlock(&(tm->work_mutex));

        // executa task ul
        if (work != NULL) 
        {
            work->func(work->arg);
            tpool_work_destroy(work);
        }

        pthread_mutex_lock(&(tm->work_mutex));
        tm->working_cnt--;

        // anunta daca toate task urile  s au terminat
        if (!tm->stop && tm->working_cnt == 0 && tm->work_first == NULL)
        {
            pthread_cond_signal(&(tm->working_cond));
        }
        pthread_mutex_unlock(&(tm->work_mutex));
    }

    tm->thread_cnt--;
    pthread_cond_signal(&(tm->working_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    return NULL;
}

tpool_t *tpool_create(size_t num)
{
    tpool_t   *tm;
    pthread_t  thread;
    size_t     i;

    // daca numarul de thread-uri specificat (num) este zero, functia seteaza un numar implicit de 2 thread-uri
    if (num == 0)
    {    
        num = 2;
    }

    tm = calloc(1, sizeof(*tm));
    tm->thread_cnt = num;

    pthread_mutex_init(&(tm->work_mutex), NULL);
    pthread_cond_init(&(tm->work_cond), NULL);
    pthread_cond_init(&(tm->working_cond), NULL);

    tm->work_first = NULL;
    tm->work_last  = NULL;

    //  fiecare thread executa functia tpool_worker, care preia si executa task-uri din coada pool-ului
    for (i=0; i<num; i++) 
    {
        pthread_create(&thread, NULL, tpool_worker, tm);
        pthread_detach(thread); // firele sunt detasate  pentru a evita necesitatea de a le alatura (join) manual mai tarziu
    }

    return tm; // pointerul poate fi folosit ulterior pentru a adauga task-uri, astepta finalizarea, sau distruge pool-ul 
}


void tpool_destroy(tpool_t *tm)
{
    tpool_work_t *work;
    tpool_work_t *work2;

    if (tm == NULL)
    {
        return;
    }

    pthread_mutex_lock(&(tm->work_mutex));
    work = tm->work_first;
    while (work != NULL) 
    {
        work2 = work->next;
        tpool_work_destroy(work);
        work = work2;
    }
    tm->work_first = NULL;
    tm->stop = true;
    pthread_cond_broadcast(&(tm->work_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    tpool_wait(tm);

    pthread_mutex_destroy(&(tm->work_mutex));
    pthread_cond_destroy(&(tm->work_cond));
    pthread_cond_destroy(&(tm->working_cond));

    free(tm);
}


//adauga task nou in coada
bool tpool_add_work(tpool_t *tm, thread_func_t func, void *arg)
{
    tpool_work_t *work;
    //printf("Am intrat in add_work.\n");
    if (tm == NULL)
    {
        return false;
    }

    work = tpool_work_create(func, arg);
    if (work == NULL)
    {
        return false;
    }

    pthread_mutex_lock(&(tm->work_mutex));
    if (tm->work_first == NULL) 
    {
        tm->work_first = work;
        tm->work_last  = tm->work_first;
    } 
    else 
    {
        tm->work_last->next = work;
        tm->work_last       = work;
    }

    pthread_cond_broadcast(&(tm->work_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    return true;
}

void tpool_wait(tpool_t *tm)
{
    if (tm == NULL)
    {
        return;
    }

    pthread_mutex_lock(&(tm->work_mutex));
    while (1) 
    {
        if (tm->work_first != NULL || (!tm->stop && tm->working_cnt != 0) || (tm->stop && tm->thread_cnt != 0)) {
            pthread_cond_wait(&(tm->working_cond), &(tm->work_mutex));
        } 
        else 
        {
            break;
        }
    }
    pthread_mutex_unlock(&(tm->work_mutex));
}