#include "sequence.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

// Entry_T *new_entry(void *ptr)
// {
//     Entry_T *entry = malloc(sizeof(Entry_T));
//     assert(entry != NULL);
    
//     // make sure I don't make a dummy entry by acccident
//     assert(ptr != NULL);

//     entry->data = ptr;
//     entry->next = NULL;
//     entry->prev = NULL;

//     return entry;
// }

// void free_entry(Entry_T **entry)
// {
//     assert(entry != NULL && *entry != NULL);
//     (*entry)->next = NULL;
//     (*entry)->prev = NULL;
//     (*entry)->data = NULL; // NOTE: not freeing data, just wiping it
//     free(*entry);
//     *entry = NULL;

// }

// Sequence_T *Seq_new()
// {
//     Sequence_T *seq = malloc(sizeof(Sequence_T));
//     assert(seq != NULL);
//     seq->size = 0;
//     seq->front = NULL;
//     seq->back = NULL;

//     return seq;
// }

// void Seq_free(Sequence_T **seq)
// {
//     // TODO: handle this properly

//     Entry_T *curr = (*seq)->front;

//     while (curr != NULL) {
//         Entry_T *to_delete = curr;
//         curr = curr->next;

//         free_entry(&to_delete);
//         // TODO Intentionally not freeing the array right now.
//     }

//     free(*seq);
//     *seq = NULL;
// }

// void Seq_addlo(Sequence_T *seq, void *ptr)
// {
//     printf("Addlo messing with segment 0\n");
//     // insert at front of linked list
//     assert(seq != NULL);
//     Entry_T *e = new_entry(ptr);

//     if (seq->size == 0) { // if list is empty
//         seq->front = e;
//         seq->back = e;
//     } else {
//         e->prev = NULL;
//         e->next = seq->front;
//         seq->front = e;
//     }

//     seq->size++;
// }

// void Seq_addhi(Sequence_T *seq, void *ptr)
// {
//     // insert at back of linked list
//     assert(seq != NULL);
//     Entry_T *e = new_entry(ptr);

//     if (seq->size == 0) {
//         seq->front = e;
//         seq->back = e;
//     } else {
//         e->prev = seq->back;
//         e->next = NULL;
//         seq->back = e;
//     }

//     seq->size++;
// }

// void *Seq_remlo(Sequence_T *seq)
// {
//     printf("Remlo messing with segment 0\n");
//     // remove from the front of the linked list
//     assert(seq != NULL);
//     assert(seq->size > 0);
//     Entry_T *to_remove = seq->front;

//     seq->front = seq->front->next;
//     seq->front->prev = NULL;

//     // // Not certain about this, have to revisit (probably want a testing main)
//     if (seq->size == 1) {
//         seq->back = NULL;
//     }

//     seq->size--;
//     void *temp = to_remove->data;
//     free_entry(&to_remove);
//     return temp;
// }

// void *Seq_remhi(Sequence_T *seq)
// {
//     // removed from the back of the linked list
//     assert(seq != NULL);
//     assert(seq->size > 0);

//     Entry_T *to_remove = seq->back;

//     seq->back = seq->back->prev;
//     seq->back->next = NULL;

//     if (seq->size == 1) {
//         seq->front = NULL;
//     }

//     seq->size--;
//     void *temp = to_remove->data;
//     free_entry(&to_remove);
//     return temp;

// }

// void *Seq_get(Sequence_T *seq, uint32_t idx)
// {
//     // iterate to index in the linked list
//     assert(seq != NULL);
//     assert(0 <= idx && idx <= seq->size);


//     uint32_t i = 0;
//     Entry_T *e = seq->front;

//     // while (i < idx && (e->next != NULL)) {
//     //     i++;
//     //     e = e->next;
//     // }

//     while (i < idx)
//     {
//         if (e->next == NULL) {
//             printf("Index is %d\n", i);
//         }
//         i++;
//         e = e->next;
//     }

//     if (e->data == NULL) {
//         printf("Segment %d is NULL\n", idx);
//         // assert(false);
//     }

//     return e->data;
// }

// void Seq_put(Sequence_T *seq, uint32_t idx, void *ptr)
// {
//     // iterate to index and update
//     assert(seq != NULL);
//     assert(0 <= idx && idx <= seq->size);

//     if (idx == 0) {
//         printf("more segment 0 stuff\n");
//     }

//     // no dummy data
//     // assert(ptr != NULL);

//     uint32_t i = 0;
//     Entry_T *e = seq->front;

//     while (i < idx && (e->next != NULL))
//     {
//         i++;
//         e = e->next;
//     }

//     e->data = ptr;
// }

// unsigned Seq_length(Sequence_T *seq)
// {
//     assert(seq != NULL);
//     return seq->size;
// }

// starting capacity of 10
Sequence_T *Seq_new()
{
    Sequence_T *seq = malloc(sizeof(Sequence_T));
    assert(seq != NULL);

    seq->size = 0;
    seq->capacity = 10;

    seq->data = calloc(seq->capacity, sizeof(void*));
    assert(seq->data != NULL);

    return seq;
}

void Seq_free(Sequence_T **seq)
{
    assert(seq != NULL && *seq != NULL);

    // Leaving this out for now, since it's guaranteed to cause problems
    // for (uint32_t i = 0; i < (*seq)->size; i++) {
    //     // Free everything stored in the sequence
    //     free((*seq)->data[i]);
    //     (*seq)->data[i] = NULL;
    // }

    free((*seq)->data);
    (*seq)->data = NULL;

    free(*seq);
    *seq = NULL;
}

// 100% need a working expand function this time around
void expand(Sequence_T *seq)
{
    assert(seq != NULL);
    assert(seq->size == seq->capacity);

    seq->capacity *= 2;
    void **new_data = calloc(seq->capacity, sizeof(void*));

    for (u_int32_t i = 0; i < seq->size; i++) {
        new_data[i] = seq->data[i];
    }

    free(seq->data);
    seq->data = new_data;
}

void Seq_addlo(Sequence_T *seq, void *ptr)
{
    assert(seq != NULL);

    if (seq->size == seq->capacity) {
        expand(seq);
    }

    // copy over everything
    for (uint32_t i = seq->size; i > 0; i--) {
        seq->data[i] = seq->data[i - 1];
    }

    seq->data[0] = ptr;
    seq->size++;
}

void Seq_addhi(Sequence_T *seq, void *ptr)
{
    assert(seq != NULL);
    
    if (seq->size == seq->capacity) {
        expand(seq);
    }

    seq->data[seq->size] = ptr;
    seq->size++;
}

void *Seq_remlo(Sequence_T *seq)
{
    assert(seq != NULL);
    assert(seq->size > 0);

    void *temp = seq->data[0];

    // TODO: be careful copy over all the elements
    for (uint32_t i = 0; i < seq->size - 1; i++) {
        seq->data[i] = seq->data[i + 1];
    }

    // Manually resetting the last element to avoid out of bounds read
    seq->data[seq->size - 1] = NULL;

    seq->size--;
    return temp;
}

void *Seq_remhi(Sequence_T *seq)
{
    assert(seq != NULL);
    assert(seq->size > 0);
    void *temp = seq->data[seq->size - 1];
    seq->data[seq->size - 1] = NULL;

    seq->size--;
    return temp;
}

void *Seq_get(Sequence_T *seq, uint32_t idx)
{
    assert(seq != NULL);
    return seq->data[idx];
}

void Seq_put(Sequence_T *seq, uint32_t idx, void *ptr)
{
    assert(seq != NULL);
    assert(idx < seq->size);

    // This memory needs to get freed somewhere. Probably not in here tho 
    seq->data[idx] = ptr;
}

uint32_t Seq_length(Sequence_T *seq)
{
    assert(seq != NULL);
    return seq->size;
}
