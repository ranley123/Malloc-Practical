#include "myalloc.h"
#include <stdio.h>
#include <pthread.h>

#include <unistd.h> // for testing

void *func1() {
	printf("Inside thread 1\n");
    sleep(0.0001);
    void *ptrs[12];
    int index = 0;
	for(int i = 5; i < 65; i = i + 5){
        void *p = myalloc(i);
        ptrs[index++] = p;
    }

    for(int i = 0; i < index; i++){
        myfree(ptrs[i]);
    }

	printf("-----Thread 1 Done----\n");
	return NULL;
}

void *func2() {
	printf("Inside thread 2\n");
	void *ptrs[12];
    int index = 0;
	for(int i = 65; i < 125; i = i + 5){
        void *p = myalloc(i);
        ptrs[index++] = p;
    }

    for(int i = 0; i < index; i++){
        myfree(ptrs[i]);
    }

	printf("-----Thread 2 Done----\n");
	return NULL;
}
void main()
{
// Invalid Input: -1, 0
    // fprintf(stdout, "Allocating -1 byte... \n");
    // int *ptr = (int *)myalloc(-1);

    // fprintf(stdout, "Allocating 0 byte... \n");
    // int *ptr = (int *)myalloc(0);

// Edge Case: 4 pages: 16384 bytes
    // fprintf(stdout, "Allocating 4 pages bytes... \n");
    // void *ptr= (void *)myalloc(4 * sysconf(_SC_PAGESIZE));

// Double free:
    // void *ptr = myalloc(48);
    // myfree(ptr);
    // myfree(ptr);

// Formal case: 48 bytes
    // fprintf(stdout, "Allocating 48 bytes... \n");
    // void *ptr = myalloc(48);


// Multiple allocations and free
    // printf("Allocating 60 * sizeof(int)... \n");

    // int* ptr = myalloc(60*sizeof(int));
    // ptr[0] = 42;    


    // printf("\nAllocating 40 * sizeof(int)... \n");

    // int *ptr2 = myalloc(40 *sizeof(int));

    // printf("Freeing the allocated memory... \n");
    // myfree(ptr);
    // myfree(ptr2);


// test threads
    // pthread_t t1, t2;
	// pthread_create(&t1, NULL, func1,  NULL);
	// pthread_create(&t2, NULL, func2, NULL);
	// pthread_join(t1, NULL);
	// pthread_join(t2, NULL);

    printf("Yay!\n");
}
