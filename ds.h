#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>


typedef struct{
	void* data;
	size_t size;     	 // Size in elements
	size_t capacity; 	 // Capacity in elements
	size_t element_size; // Size of element in bytes
} list;

#ifndef LIST_GROW_FACTOR
	#define LIST_GROW_FACTOR 2
#endif


list* new_list(size_t element_size, size_t element_count){
	list* l = malloc(sizeof(list));
	if(l == NULL){
		return NULL;
	}
	l->data = malloc(element_size*element_count);
	if (l->data == NULL){
		free(l);
		return NULL;
	}
	l->size = 0;
	l->capacity = element_count;
	l->element_size = element_size;
	return l;
}

void free_list(list* l){
	// frees only the list not its elements
	free(l->data);
	free(l);
}

int realloc_list(list* l){
    if(l == NULL)
    	return -1;
    
    size_t new_size_elements = l->capacity * LIST_GROW_FACTOR;
    void* new_data = realloc(l->data, new_size_elements * l->element_size);
    if(new_data == NULL){
        fprintf(stderr, "Failed to reallocate list\n");
        return -1;
    }
    l->data = new_data;
    l->capacity = new_size_elements;
    return 0;
}

void push_str(list* l, char* str){
	if(l == NULL || str == NULL){
		fprintf(stderr, "Null list or string passed!\n");
		return;
	}
	if(l->size+1 > l->capacity){
		if(realloc_list(l) != 0){
			return;
		}
	}
	((char**)l->data)[l->size] = str;
	l->size++;
}

char* get_str(list* l, size_t index){
	if(index >= l->size){
		fprintf(stderr, "Out of bounds error!\n");
		return NULL;	
	}
	return ((char**)l->data)[index];
}

void push_char(list* l, char character){
	if(l == NULL){
		fprintf(stderr, "Null list passed!\n");
		return;
	}
	if(l->size+1 > l->capacity){
		if(realloc_list(l) != 0){
			return;
		}
	}
	((char*)l->data)[l->size] = character;
	l->size++;
}


char get_char(list* l, size_t index){
	if(index >= l->size){
		fprintf(stderr, "Out of bounds error!\n");
		return 0;	
	}
	return ((char*)l->data)[index];
}
