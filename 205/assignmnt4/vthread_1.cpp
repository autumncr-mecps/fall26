#include <iostream>
#include <cstdlib>
#include <pthread.h>
using namespace std;

#define NUM_THREADS 5

void *vMyThread(void *threadid) {
  long tid;
  tid = (long)threadid;
  printf("Hello World! Thread id, %d\n", tid);
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
  }
  pthread_exit(NULL);
  return 0;
}
