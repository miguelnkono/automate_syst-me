#include "data_structure.h"

#include <stdlib.h>
#include <stdio.h>

/**
 * Initialiser une liste dynamique.
 * @param item_size est la taille d'un élément qui va être stocké dans la liste.
 * @param capacity est la capacité initiale de la liste.
 * @return un pointeur vers la zone de données (juste après l'en-tête).
 */
static inline void *_da_init(const size_t item_size, const size_t capacity)
{
    __da_header__ *h = (__da_header__ *)malloc(item_size * capacity + sizeof(__da_header__));

    if (h == NULL)
    {
        fprintf(stderr, "Impossible de créer la liste dynamique\n");
        exit(1);
    }

    h->length = 0;
    h->capacity = capacity;
    h->item_size = item_size;

    // retourner le pointeur juste après les méta-données de l'en-tête
    return h + 1;
}

/**
 * S'assurer que la liste a assez d'espace pour stocker `capacity_increase` éléments supplémentaires.
 * @param da est un double pointeur vers la liste dynamique (pour permettre la mise à jour
 *           de l'adresse en cas de realloc).
 * @param capacity_increase est le nombre d'éléments supplémentaires à accueillir.
 */
static inline void _da_ensure_capacity(void **da, const size_t capacity_increase)
{
    __da_header__ *h = ((__da_header__ *)(*da)) - 1;

    if (h->length + capacity_increase > h->capacity)
    {
        size_t new_capacity = h->capacity * 2;
        while (h->length + capacity_increase > new_capacity)
        {
            new_capacity *= 2;
        }

        h = (__da_header__ *)realloc(h, h->item_size * new_capacity + sizeof(__da_header__));

        if (h == NULL)
        {
            fprintf(stderr, "Impossible de redimensionner la liste\n");
            exit(1);
        }

        h->capacity = new_capacity;
        *da = h + 1;
    }
}

/**
 * Retourner la longueur (nombre d'éléments) de la liste dynamique.
 * @param da est la liste dynamique.
 */
static inline size_t _da_length(const void *da)
{
    return da ? ((const __da_header__ *)da - 1)->length : 0;
}

/**
 * Incrémenter la longueur de la liste dynamique de 1.
 * @param da est la liste dynamique.
 */
static inline void _da_increment_length(void *da)
{
    if (da)
    {
        ((__da_header__ *)(da)-1)->length++;
    }
}

/**
 * Ajouter un élément à la fin de la liste dynamique.
 * Le pointeur `*da` peut être mis à jour si un realloc a eu lieu.
 * @param da    est un double pointeur vers la liste dynamique.
 * @param item  est un pointeur vers l'élément à copier dans la liste.
 */
void _da_push(void **da, const void *item)
{
    _da_ensure_capacity(da, 1);

    __da_header__ *h = ((__da_header__ *)(*da)) - 1;
    char *dest = (char *)(*da) + h->length * h->item_size;

    // copier l'élément octet par octet dans la liste
    for (size_t i = 0; i < h->item_size; i++)
    {
        dest[i] = ((const char *)item)[i];
    }

    _da_increment_length(*da);
}

/**
 * Libérer la mémoire d'une liste dynamique (en-tête + données).
 * @param da est la liste dynamique.
 */
static inline void _da_free(void *da)
{
    if (da)
    {
        free((__da_header__ *)(da)-1);
    }
}

/**
 * Créer et initialiser un automate vide.
 * @return un pointeur vers l'automate créé, ou NULL en cas d'échec.
 */
__automate_state__ *automate_state_create()
{
    __automate_state__ *as = (__automate_state__ *)malloc(sizeof(__automate_state__));
    if (as == NULL)
    {
        fprintf(stderr, "Impossible de créer l'automate\n");
        return NULL;
    }

    as->_nombre_symboles_ = 0;
    as->_nombre_etats_ = 0;
    as->_nombre_transaction_ = 0;

    // initialiser les listes dynamiques pour les états initiaux et finaux
    as->_etat_initiaux_._etats_ = 0;
    as->_etat_initiaux_._etat_numéros_ = (uint8_t *)_da_init(sizeof(uint8_t), 4);

    as->_etat_finaux_._etats_ = 0;
    as->_etat_finaux_._etat_numéros_ = (uint8_t *)_da_init(sizeof(uint8_t), 4);

    // initialiser la liste dynamique des transitions
    as->_transitions_ = (__automate_transition__ *)_da_init(sizeof(__automate_transition__), 4);

    return as;
}

/**
 * Libérer toute la mémoire associée à un automate.
 * @param as est l'automate à détruire.
 */
void automate_state_destroy(__automate_state__ *as)
{
    if (as != NULL)
    {
        if (as->_etat_initiaux_._etat_numéros_ != NULL)
        {
            _da_free(as->_etat_initiaux_._etat_numéros_);
        }

        if (as->_etat_finaux_._etat_numéros_ != NULL)
        {
            _da_free(as->_etat_finaux_._etat_numéros_);
        }

        if (as->_transitions_ != NULL)
        {
            _da_free(as->_transitions_);
        }

        free(as);
    }
}
