#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
using namespace std;

#define NUM_THREADS 5
int counter = 0;
pthread_mutex_t mut;

void *vMyThread(void *threadid) {
  long tid;
  tid = (long)threadid;
  for (int i = 0; i < 5; i++) {
    pthread_mutex_lock(&mut);
    counter++;
    sleep(1);
    printf("Hello World! Thread id is %d, counter is %d\n", tid, counter);
    pthread_mutex_unlock(&mut);
    usleep(1000);
  }
  pthread_exit(NULL);
}

int main() {
  pthread_t threads[NUM_THREADS];
  int rc;
  int i;
  for (i = 0; i < NUM_THREADS; i++) {
    cout << "main(): creating thread " << i << endl;
    rc = pthread_create(&threads[i], NULL, vMyThread, (void*)i);
    if (rc) {
      printf("Error: unable to create thread %d\n", i);
      exit(-1);
    }
    //usleep(1000);
  }
  pthread_exit(NULL);
  return 0;
}
