#define HEAP_START 0x01000000
#define HEAP_MAX   0x03000000
#define BLOCK_MAGIC 0xDEADC0DE

struct heap_block {
    unsigned int magic;
    unsigned int size;
    int is_free;
    struct heap_block *next;
};

static struct heap_block *first_block = (struct heap_block *)HEAP_START;

void init_heap(void) {
    first_block->magic = BLOCK_MAGIC;
    first_block->size = (HEAP_MAX - HEAP_START) - sizeof(struct heap_block);
    first_block->is_free = 1;
    first_block->next = 0;
}

void *malloc(unsigned int size) {
    struct heap_block *curr = first_block;
    while (curr) {
        if (curr->magic == BLOCK_MAGIC && curr->is_free && curr->size >= size) {
            if (curr->size > size + sizeof(struct heap_block) + 4) {
                struct heap_block *new_block = (struct heap_block *)((unsigned char *)curr + sizeof(struct heap_block) + size);
                new_block->magic = BLOCK_MAGIC;
                new_block->size = curr->size - size - sizeof(struct heap_block);
                new_block->is_free = 1;
                new_block->next = curr->next;
                curr->size = size;
                curr->is_free = 0;
                curr->next = new_block;
            } else {
                curr->is_free = 0;
            }
            return (void *)((unsigned char *)curr + sizeof(struct heap_block));
        }
        curr = curr->next;
    }
    return 0;
}

void free(void *ptr) {
    if (!ptr) return;
    struct heap_block *block = (struct heap_block *)((unsigned char *)ptr - sizeof(struct heap_block));
    if (block->magic == BLOCK_MAGIC) {
        block->is_free = 1;
    }
    struct heap_block *curr = first_block;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += sizeof(struct heap_block) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}
