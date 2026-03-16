#include "data_structure.h"

#include <stdlib.h>
#include <stdio.h>

__automate_state__ *automate_state_create()
{
    __automate_state__ *as = (__automate_state__ *) malloc( sizeof(__automate_state__) );
    if (as == NULL)
    {
        fprintf(stderr, "Impossible de créer l'automate\n");
        return NULL;
    }
    
    as->_transitions_ = _da_init( sizeof(__automate_transition__), 4);
}

void automate_state_destroy(__automate_state__ *as)
{
    if (as != NULL)
    {
        if (as->_etat_initiaux_._etat_numéros_ != NULL)
        {
            _da_free((__automate_node_info__ *)as->_etat_initiaux_._etat_numéros_);
        }
        
        if (as->_etat_finaux_._etat_numéros_ != NULL)
        {
            _da_free((__automate_node_info__ *)as->_etat_finaux_._etat_numéros_);
        }

        if (as->_transitions_ != NULL)
        {
            _da_free((__automate_transition__ *)as->_transitions_);
        }
        
        free(as);
    }
}

/////////////////////////////////////////////////////////
//// méthodes privées 
/////////////////////////////////////////////////////////
/**
 * Initialiser une liste dynamique
 * @param item_size est la taille d'un élément qui va être stocké dans la liste;
 * @param capacity est la capacité totale de la liste;
 *  */ 
static inline void *_da_init(const size_t item_size, const size_t capacity)
{
    void *ptr = 0;
    __da_header__ *h = (__da_header__ *) malloc(item_size * capacity + sizeof(__da_header__));

    if (h)
    {
        // initialise les attributs (length, capacity, et item_size);
        h->length = 0;
        h->capacity = capacity;
        h->item_size = item_size;

        // faire pointé `ptr` juste après les méta-données de l'en-tête de la liste;
        ptr = h + 1;
    }
    else
    {
        fprintf(stderr, "Impossible de créer la liste dynamique");
        exit(1);
    }

    return ptr;
}

/**
 * s'assuré que la liste a accés d'espace pour stocker la nouvelle donnée de taille `capacity_increase`
 * @param da est la liste dynamique;
 * @param capacity_increase est la taille de la nouvelle donnée;
 *  */ 
static inline void _da_ensure_capacity(void **da, const size_t capacity_increase)
{
    __da_header__ *h = ((__da_header__ *)(*da) - 1);    // revenir à l'en-tête de la liste;
    if (h->length + capacity_increase > h->capacity)
    {

        // redimensionné la liste;
        size_t new_capacity = h->capacity * 2;
        while (h->length + capacity_increase > new_capacity)
        {
            new_capacity *= 2;
        }

        h = (__da_header__ *) realloc(h, h->item_size * new_capacity + sizeof(__da_header__));

        if (!h)
        {
            fprintf(stderr, "Impossible de redimensionné la liste");
            exit(1);
        }

        h->capacity = new_capacity;
        *da = h + 1;
    }
}

// static inline void *_da_priority_insert(void **da, const size_t priority, int (*compare)(const void *, const size_t))
// { }

static inline size_t _da_length(const void *da)
{
    return da ? ((const __da_header__ *)da - 1)->length : 0;
}

static inline void _da_increment_length(void *da)
{
    if (da)
    {
        ((__da_header__ *)(da)-1)->length++;
    }
}

static inline void _da_free(void *da)
{
    if (da)
    {
        free((__da_header__ *)(da) - 1);
    }
}