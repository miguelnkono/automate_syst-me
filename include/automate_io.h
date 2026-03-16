#ifndef AUTOMATE_IO_H
#define AUTOMATE_IO_H

#include <stdint.h>

// une transition est constitué d'un état de départ, d'un symbole, et d'un état d'arriver;
typedef struct _automate_transition_
{
    uint8_t etat_depart;
    short symbole;
    uint8_t etat_arrive;
} __automate_transition__;

/**
 * Créer une transition;
 * @param etat_depart est l'état de départ;
 * @param symbole est le symbole de la transition;
 * @param etat_arrive;
 */
__automate_transition__* automate_transition_create(uint8_t etat_depart, short symbole, uint8_t etat_arrive);

/**
 * Fonction pour supprimer une transition;
 * @param t est la transition à supprimer;
 */
void automate_transition_destroy(__automate_transition__ *t);

#endif // AUTOMATE_IO_H