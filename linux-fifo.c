#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include "linux-fifo.h"

pthread_mutex_t globalMutex;
pthread_cond_t consumerCondition, producerCondition, print;

circular_buffer *cb;

// Keep the current number of items in the buffer as a global variable
int bufferItems = 0;

// This variable will be used to determine print priority
int printedThreads = 1;

// Fill a placeholder array with 0s to hold actual values
short setupVar[5] = { 0 };

void *producer(void *inputArg)
{
	int i;

	// Cast void pointer to integer pointer & then retrieve value stored inside, thus co-ercing the input argument
	int sequentialID = *((int *)inputArg);

	free(inputArg);

	// Initialize array
	unsigned int * producerArray = (unsigned int *)malloc(setupVar[NUMBER_OF_PROD_NUMS] * sizeof(unsigned int));

	if (producerArray == NULL)
	{
		printf(RED BRT "ERROR:" RST " Could not allocate memory for internal array for Thread %d (Producer), aborting\n", sequentialID);
		pthread_exit(0);
	}

	// Retrieve the real thread ID and multiply it by the seed
	int seed = pthread_self() * setupVar[RAND_SEED];

	for (i = 0; i < setupVar[NUMBER_OF_PROD_NUMS]; i++)
	{
		pthread_mutex_lock(&globalMutex);

		// Global variable bufferItems used to check if buffer is full
		while (bufferItems == setupVar[BUFFER_CAPACITY]) pthread_cond_wait(&producerCondition, &globalMutex);

		// Critical Area

		// Increment global counter
		bufferItems++;

		// Keep randomly generated number within range of unsigned integer (System dependant UINT_MAX macro)
		unsigned int input = rand_r(&seed) % UINT_MAX;

		// Push the number into the stack
		cb_push_back(cb, (void*)&input);

		// Save generater number to file
		FILE *filePointer;

		// (a)ppend and (+)create
		filePointer = fopen("prod_in.txt", "a+");
		fprintf(filePointer, "Producer %d: %d\n", sequentialID, input);
		fclose(filePointer);

		// Add it to in-thread buffer
		producerArray[i] = input;

		// Critical Area End

		// Signal other threads
		pthread_mutex_unlock(&globalMutex);
		pthread_cond_signal(&consumerCondition);
	}

	pthread_mutex_lock(&globalMutex);

	// Critical Area

	// Global variable printedThreads is incremented whenever a thread prints its output
	while (printedThreads < sequentialID) pthread_cond_wait(&print, &globalMutex);

	printedThreads++;

	// Print every produced number in sequence
	for (i = 0; i < setupVar[NUMBER_OF_PROD_NUMS]; i++)
	{
		printf(BLU BRT "Producer %d: %d\n" RST, sequentialID, producerArray[i]);
	}

	free(producerArray);

	// Critical Area End

	pthread_mutex_unlock(&globalMutex);

	// Wake up all threads listening on the print conditional
	pthread_cond_broadcast(&print);

	pthread_exit(0);
}

void *consumer(void *inputArg)
{
	int i;

	// Cast void pointer to integer pointer & then retrieve value stored inside, thus co-ercing the input argument
	int sequentialID = *((int *)inputArg);

	free(inputArg);

	// Initialize array
	unsigned int * consumerArray = (unsigned int *)malloc(setupVar[NUMBER_OF_PROD_NUMS] * sizeof(unsigned int));

	if (consumerArray == NULL)
	{
		printf(RED BRT "ERROR:" RST " Could not allocate memory for internal array for Thread %d (Consumer), aborting\n", sequentialID);
		pthread_exit(0);
	}

	for (i = 0; i < setupVar[NUMBER_OF_PROD_NUMS]; i++)
	{
		pthread_mutex_lock(&globalMutex);
		// Global variable bufferItems used to check if buffer is empty
		while (bufferItems == 0) pthread_cond_wait(&consumerCondition, &globalMutex);

		// Critical Area

		// Decrease global counter
		bufferItems--;

		// Create a pointer capable of holding an unsigned int
		unsigned int * tmp_read = (unsigned int *)malloc(sizeof(unsigned int));

		if (tmp_read == NULL)
		{
			printf(RED BRT "ERROR:" RST " Could not allocate memory for internal temporary variable for Thread %d (Consumer), aborting\n", sequentialID);
			pthread_exit(0);
		}

		// Pop the last member of the stack
		cb_pop_front(cb, tmp_read);

		// Save retrieved number to file
		FILE *filePointer;

		// (a)ppend and (+)create
		filePointer = fopen("cons_out.txt", "a+");
		fprintf(filePointer, "Consumer %d: %d\n", sequentialID, *(tmp_read));
		fclose(filePointer);

		// Add it to in-thread buffer
		consumerArray[i] = (*tmp_read);

		// Free memory
		free(tmp_read);

		// Critical Area End

		pthread_mutex_unlock(&globalMutex);
		pthread_cond_signal(&producerCondition);
	}

	pthread_mutex_lock(&globalMutex);

	// Critical Area

	// Global variable printedThreads is incremented whenever a thread prints its output, offset consumers by number of producers to ensure order
	while (printedThreads < sequentialID + setupVar[PRODUCER_COUNT]) pthread_cond_wait(&print, &globalMutex);

	printedThreads++;

	// Print every produced number in sequence
	for (i = 0; i < setupVar[NUMBER_OF_PROD_NUMS]; i++)
	{
		printf(GRN BRT "Consumer %d: %d\n" RST, sequentialID, consumerArray[i]);
	}

	free(consumerArray);

	// Critical Area End

	pthread_mutex_unlock(&globalMutex);

	// Wake up all threads listening on the print conditional
	pthread_cond_broadcast(&print);

	pthread_exit(0);
}

int main(int argc, char *argv[])
{
	printf(CLR);

	// Read & Parse input arguments
	if (argc != 6)
	{
		printf(RED BRT "ERROR:" RST " Expected 5 arguments but got %d\n", argc - 1);
		return 1;
	}
	else
	{
		int i;
		for (i = 1; i < argc; i++)
		{
			int j;
			int numLength = strlen(argv[i]);
			// If occurances of the numeric characters does not equal the string length itself or the first character is 0, it is not a number
			if (strspn(argv[i], "0123456789") != numLength || *(argv[i]) == '0')
			{
				printf(RED BRT "ERROR:" RST " Non-numeric argument %s detected at argument %d\n", argv[i], i);
				return 1;
			}

			// atoi has undefined behaviour on error and as such, it is better to manually parse
			for (j = 0; j < numLength; j++) {
				// Normalize the ASCII value of the numbers and multiple them in reverse to the power of 10, thus parsing the numbers
				setupVar[i - 1] += (argv[i][j] - 48) * (int)(pow(10, numLength - j - 1) + 0.5);
			}
		}
	}

	cb = (circular_buffer*)malloc(sizeof(struct circular_buffer));

	if (cb == NULL)
	{
		printf(RED BRT "ERROR:" RST " Could not allocate memory for circular buffer, aborting\n");
		exit(1);
	}

	// Size of unsigned int changes depending on CPU architecture so it is wiser to put the on-compile determined size
	cb_init(cb, setupVar[BUFFER_CAPACITY], sizeof(unsigned int));

	// Allocate enough memory for the two arrays
	pthread_t * producerArray = malloc(setupVar[PRODUCER_COUNT] * sizeof(pthread_t));

	if (producerArray == NULL)
	{
		printf(RED BRT "ERROR:" RST " Could not allocate memory for producer thread array, aborting\n");
		exit(1);
	}

	pthread_t * consumerArray = malloc(setupVar[CONSUMER_COUNT] * sizeof(pthread_t));

	if (consumerArray == NULL)
	{
		printf(RED BRT "ERROR:" RST " Could not allocate memory for consumer thread array, aborting\n");
		exit(1);
	}

	pthread_mutex_init(&globalMutex, 0);
	pthread_cond_init(&consumerCondition, 0);
	pthread_cond_init(&producerCondition, 0);
	pthread_cond_init(&print, 0);

	int i;

	// Generate all producer threads
	for (i = 0; i < setupVar[PRODUCER_COUNT]; i++)
	{
		int * threadID = malloc(sizeof(*threadID));

		if (threadID == NULL)
		{
			printf(RED BRT "ERROR:" RST " Could not allocate memory for thread ID, aborting\n");
			exit(1);
		}

		*threadID = i + 1;
		pthread_create(&consumerArray[i], 0, consumer, threadID);
	}

	// Generate all consumer threads
	for (i = 0; i < setupVar[CONSUMER_COUNT]; i++)
	{
		int * threadID = malloc(sizeof(*threadID));

		if (threadID == NULL)
		{
			printf(RED BRT "ERROR:" RST " Could not allocate memory for thread ID, aborting\n");
			exit(1);
		}

		*threadID = i + 1;
		pthread_create(&producerArray[i], 0, producer, threadID);
	}

	// Wait for producers to finish to prevent cutting them off due to main thread closing
	for (i = 0; i < setupVar[PRODUCER_COUNT]; i++)
	{
		pthread_join(producerArray[i], 0);
	}

	// Wait for consumers to finish to prevent cutting them off due to main thread closing
	for (i = 0; i < setupVar[CONSUMER_COUNT]; i++)
	{
		pthread_join(consumerArray[i], 0);
	}

	// Clean up program before exiting
	pthread_cond_destroy(&consumerCondition);
	pthread_cond_destroy(&producerCondition);
	pthread_cond_destroy(&print);
	pthread_mutex_destroy(&globalMutex);
	cb_free(cb);

	return 0;
}

// Below this point is the implementation of the circular_buffer

//initialize circular buffer
//capacity: maximum number of elements in the buffer
//sz: size of each element
void cb_init(circular_buffer *cb, size_t capacity, size_t sz)
{
	cb->buffer = malloc(capacity * sz);
	if (cb->buffer == NULL) {
		printf("Could not allocate memory..Exiting! \n");
		exit(1);
	}
	// handle error
	cb->buffer_end = (char *)cb->buffer + capacity * sz;
	cb->capacity = capacity;
	cb->count = 0;
	cb->sz = sz;
	cb->head = cb->buffer;
	cb->tail = cb->buffer;
}

//destroy circular buffer
void cb_free(circular_buffer *cb)
{
	free(cb->buffer);
	// clear out other fields too, just to be safe
}

//add item to circular buffer
void cb_push_back(circular_buffer *cb, const void *item)
{
	if (cb->count == cb->capacity)
	{
		printf("Access violation. Buffer is full\n");
		exit(1);
	}
	memcpy(cb->head, item, cb->sz);
	cb->head = (char*)cb->head + cb->sz;
	if (cb->head == cb->buffer_end)
		cb->head = cb->buffer;
	cb->count++;
}

//remove first item from circular item
void cb_pop_front(circular_buffer *cb, void *item)
{
	if (cb->count == 0)
	{
		printf("Access violation. Buffer is empy\n");
		exit(1);
	}
	memcpy(item, cb->tail, cb->sz);
	cb->tail = (char*)cb->tail + cb->sz;
	if (cb->tail == cb->buffer_end)
		cb->tail = cb->buffer;
	cb->count--;
}
