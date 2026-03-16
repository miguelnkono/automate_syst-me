#ifndef DATA_STRUCTURE_H
#define DATA_STRUCTURE_H

#include "automate_io.h"

#include <stdlib.h>

/**
 * List dynamique pour stocké les différents données;
 * @param length est la taille de la liste;
 * @param capacity est la capacitée total de la liste;
 * @param item_size est la taille de l'élément que nous souhaitons stocké;
 * 
 * `da` pour `dynamic array`
 */
typedef struct _da_header_
{
    size_t length;
    size_t capacity;
    size_t item_size;
} __da_header__;

typedef struct _automate_node_info
{
    uint8_t _etats_;
    uint8_t *_etat_numéros_;
} __automate_node_info__;


typedef struct _automate_state_
{
    uint8_t _nombre_symboles_;
    uint8_t _nombre_etats_;
    __automate_node_info__ _etat_initiaux_;
    __automate_node_info__ _etat_finaux_;
    uint8_t _nombre_transaction_;
    __automate_transition__ *_transitions_;
} __automate_state__;

__automate_state__ *automate_state_create();
void automate_state_destroy(__automate_state__ *as);

#endif // DATA_STRUCTURE_H
