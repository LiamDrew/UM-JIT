#ifndef SEQUENCE_H__
#define SEQUENCE_H__

#include <stdlib.h>

// typedef struct Entry_T {
//     void *data;
//     struct Entry_T *next;
//     struct Entry_T *prev;
// } Entry_T;

// typedef struct {
//     unsigned size;
//     Entry_T *front;
//     Entry_T *back;
// } Sequence_T;


typedef struct {
    uint32_t size;
    uint32_t capacity;
    void **data;
} Sequence_T;

Sequence_T *Seq_new();

void Seq_free(Sequence_T **seq);

void Seq_addlo(Sequence_T *seq, void *ptr);

void Seq_addhi(Sequence_T *seq, void *ptr);

void *Seq_remlo(Sequence_T *seq);

void *Seq_remhi(Sequence_T *seq);

void *Seq_get(Sequence_T *seq, uint32_t idx);

void Seq_put(Sequence_T *seq, uint32_t idx, void *ptr);

uint32_t Seq_length(Sequence_T *seq);

#endif