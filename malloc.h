#ifndef MALLOC_H
#define MALLOC_H

void init_heap(void);
void *malloc(unsigned int size);
void free(void *ptr);

#endif
