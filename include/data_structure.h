#ifndef DATA_STRUCTURE_H
#define DATA_STRUCTURE_H

#include "automate_io.h"

#include <stdlib.h>

/**
 * En-tête d'une liste dynamique (dynamic array).
 * Stockée juste AVANT la zone de données en mémoire.
 * @param length     nombre d'éléments actuellement stockés.
 * @param capacity   nombre total d'éléments pouvant être stockés sans realloc.
 * @param item_size  taille en octets d'un élément stocké.
 */
typedef struct _da_header_
{
    size_t length;
    size_t capacity;
    size_t item_size;
} __da_header__;

/**
 * Informations sur un groupe d'états (initiaux ou finaux).
 * @param _etats_          nombre d'états dans ce groupe.
 * @param _etat_numéros_   tableau dynamique contenant les numéros des états (uint8_t[]).
 */
typedef struct _automate_node_info
{
    uint8_t _etats_;
    uint8_t *_etat_numéros_;
} __automate_node_info__;

/**
 * Représentation d'un automate fini en mémoire.
 * @param _nombre_symboles_     nombre de symboles dans l'alphabet.
 * @param _nombre_etats_        nombre d'états.
 * @param _etat_initiaux_       groupe des états initiaux.
 * @param _etat_finaux_         groupe des états finaux (terminaux).
 * @param _nombre_transaction_  nombre de transitions.
 * @param _transitions_         tableau dynamique des transitions (__automate_transition__[]).
 */
typedef struct _automate_state_
{
    uint8_t _nombre_symboles_;
    uint8_t _nombre_etats_;
    __automate_node_info__ _etat_initiaux_;
    __automate_node_info__ _etat_finaux_;
    uint8_t _nombre_transaction_;
    __automate_transition__ *_transitions_;
} __automate_state__;

/**
 * Longueur d'une liste dynamique.
 * Usage : _DA_LENGTH(mon_tableau)
 */
#define _DA_LENGTH(da) \
    (((const __da_header__ *)(da) - 1)->length)

/**
 * Accès à l'élément d'index `i` d'une liste dynamique typée.
 * Usage : _DA_GET(__automate_transition__, transitions, 2)
 */
#define _DA_GET(type, da, i) \
    (((type *)(da))[i])

/**
 * Ajouter un élément à la fin d'une liste dynamique typée.
 * Met à jour le pointeur si un realloc a eu lieu.
 * Usage : _DA_PUSH(uint8_t, &mon_tableau, valeur)
 *         _DA_PUSH(__automate_transition__, &transitions, t)
 */
#define _DA_PUSH(type, pda, valeur)       \
    do                                    \
    {                                     \
        type _tmp_ = (valeur);            \
        _da_push((void **)(pda), &_tmp_); \
    } while (0)

/** Créer un automate vide. */
__automate_state__ *automate_state_create();

/** Libérer toute la mémoire d'un automate. */
void automate_state_destroy(__automate_state__ *as);

/**
 * Fonction interne exposée pour le macro _DA_PUSH.
 * Ne pas appeler directement — utiliser le macro _DA_PUSH.
 */
void _da_push(void **da, const void *item);

#endif // DATA_STRUCTURE_H
