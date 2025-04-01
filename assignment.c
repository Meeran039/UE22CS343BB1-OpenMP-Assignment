#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <omp.h>

#define NUM_PROCS 4
#define CACHE_SIZE 4
#define MEM_SIZE 16
#define MAX_INSTR_NUM 32
#define MSG_BUFFER_SIZE 256

typedef unsigned char byte;

typedef enum { MODIFIED, EXCLUSIVE, SHARED, INVALID } cacheLineState;

typedef enum { EM, S, U } directoryEntryState;

typedef struct instruction {
    byte type;  // 'R' or 'W'
    byte address;
    byte value;
} instruction;

typedef struct cacheLine {
    byte address;
    byte value;
    cacheLineState state;
} cacheLine;

typedef struct directoryEntry {
    byte bitVector;
    directoryEntryState state;
} directoryEntry;

typedef struct processorNode {
    cacheLine cache[CACHE_SIZE];
    byte memory[MEM_SIZE];
    directoryEntry directory[MEM_SIZE];
    instruction instructions[MAX_INSTR_NUM];
    int instructionCount;
} processorNode;

void initializeProcessor(int threadId, processorNode *node, char *dirName);
void executeInstruction(int processorId, processorNode *node);
void printProcessorState(int processorId, processorNode node);
void handleCacheWrite(int processorId, processorNode *node, byte address, byte value);
int findCacheLineIndex(processorNode *node, byte address);

void initializeProcessor(int threadId, processorNode *node, char *dirName) {
    for (int i = 0; i < MEM_SIZE; i++) {
        node->memory[i] = 20 * threadId + i;
        node->directory[i].bitVector = 0;
        node->directory[i].state = U;
    }

    for (int i = 0; i < CACHE_SIZE; i++) {
        node->cache[i].address = 0xFF;  // Invalid address to start
        node->cache[i].value = 0;
        node->cache[i].state = INVALID;  // Invalid state initially
    }

    char filename[128];
    snprintf(filename, sizeof(filename), "tests/%s/core_%d.txt", dirName, threadId);
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: could not open file %s\n", filename);
        exit(EXIT_FAILURE);
    }

    char line[20];
    node->instructionCount = 0;
    while (fgets(line, sizeof(line), file) && node->instructionCount < MAX_INSTR_NUM) {
        if (line[0] == 'R' || line[0] == 'W') {
            sscanf(line, "%c %hhx %hhu", &node->instructions[node->instructionCount].type,
                   &node->instructions[node->instructionCount].address,
                   &node->instructions[node->instructionCount].value);
            node->instructionCount++;
        }
    }
    fclose(file);
}

int findCacheLineIndex(processorNode *node, byte address) {
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (node->cache[i].address == address) {
            return i;  // Found in cache
        }
    }
    return -1;  // Not found in cache
}

void handleCacheWrite(int processorId, processorNode *node, byte address, byte value) {
    int cacheIndex = findCacheLineIndex(node, address);

    if (cacheIndex != -1) {
        // Cache hit: Update the value and set state to MODIFIED
        node->cache[cacheIndex].value = value;
        node->cache[cacheIndex].state = MODIFIED;
        printf("Processor %d: Cache hit at index %d, updated value to %d, state set to MODIFIED\n", processorId, cacheIndex, value);
    } else {
        // Cache miss: Replace an invalid cache line (if exists) and set state to MODIFIED
        int replaceIndex = -1;

        // Find the first INVALID cache line to replace
        for (int i = 0; i < CACHE_SIZE; i++) {
            if (node->cache[i].state == INVALID) {
                replaceIndex = i;
                break;
            }
        }

        // If no INVALID line found, replace the first cache line (FIFO)
        if (replaceIndex == -1) {
            replaceIndex = 0;  // Replace the first cache line for simplicity
        }

        node->cache[replaceIndex].address = address;
        node->cache[replaceIndex].value = value;
        node->cache[replaceIndex].state = MODIFIED;

        printf("Processor %d: Cache miss, replaced cache line at index %d with address 0x%02X, value %d, state set to MODIFIED\n", processorId, replaceIndex, address, value);
    }
}

void executeInstruction(int processorId, processorNode *node) {
    for (int i = 0; i < node->instructionCount; i++) {
        instruction instr = node->instructions[i];
        byte address = instr.address;
        byte value = instr.value;

        if (instr.type == 'W') {  // Write Operation
            printf("Processor %d: Writing value %d to address 0x%02X\n", processorId, value, address);
            handleCacheWrite(processorId, node, address, value);
        }
    }
}

void printProcessorState(int processorId, processorNode node) {
    printf("Processor %d:\n", processorId);
    printf("Cache State:\n");
    for (int i = 0; i < CACHE_SIZE; i++) {
        printf("  Address: 0x%02X, Value: %d, State: %d\n", node.cache[i].address,
               node.cache[i].value, node.cache[i].state);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: Missing directory name argument\n");
        exit(EXIT_FAILURE);
    }

    char *testDir = argv[1];
    processorNode processors[NUM_PROCS];

    // Initialize processors
    for (int i = 0; i < NUM_PROCS; i++) {
        initializeProcessor(i, &processors[i], testDir);
    }

    // Execute instructions for each processor
    for (int i = 0; i < NUM_PROCS; i++) {
        executeInstruction(i, &processors[i]);
    }

    // Print final state of processors
    for (int i = 0; i < NUM_PROCS; i++) {
        printProcessorState(i, processors[i]);
    }

    return 0;
}
