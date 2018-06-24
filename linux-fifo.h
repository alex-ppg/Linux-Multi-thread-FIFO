// Although it is obviously unnecessary for this project, we include a guard so that when multiple files that are linked under a single project import our header it doesn't get re-included
#ifndef CONC_QUEUE
# define CONC_QUEUE

// ANSI Escape Codes for colors (RED GREEN BLUE BRIGHT RESET)
# define RED "\x1B[31m"
# define GRN "\x1B[32m"
# define BLU "\x1B[34m"
# define BRT "\x1B[1m"
# define RST "\x1B[0m"

// ANSI sequence for clearing screen in POSIX systems
# define CLR "\e[1;1H\e[2J"

// Mapping the array values for better readability
# define PRODUCER_COUNT 0
# define CONSUMER_COUNT 1
# define BUFFER_CAPACITY 2
# define NUMBER_OF_PROD_NUMS 3
# define RAND_SEED 4

typedef struct circular_buffer
{
	void *buffer;     // data buffer
	void *buffer_end; // end of data buffer
	size_t capacity;  // maximum number of items in the buffer
	size_t count;     // number of items in the buffer
	size_t sz;        // size of each item in the buffer
	void *head;       // pointer to head
	void *tail;       // pointer to tail
} circular_buffer;

void cb_init(circular_buffer *, size_t, size_t);
void cb_free(circular_buffer *);
void cb_push_back(circular_buffer *, const void *);
void cb_pop_front(circular_buffer *, void *);

#endif