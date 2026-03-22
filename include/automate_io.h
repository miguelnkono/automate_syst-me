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
__automate_transition__ *automate_transition_create(uint8_t etat_depart, short symbole, uint8_t etat_arrive);

/**
 * Fonction pour supprimer une transition;
 * @param t est la transition à supprimer;
 */
void automate_transition_destroy(__automate_transition__ *t);

/**
 * Lire un automate depuis un fichier texte et le charger en mémoire.
 * Format attendu :
 *   ligne 1 : nombre de symboles
 *   ligne 2 : nombre d'états
 *   ligne 3 : <nb états initiaux> <numéro> ...
 *   ligne 4 : <nb états terminaux> <numéro> ...
 *   ligne 5 : nombre de transitions
 *   lignes suivantes : <état départ><symbole><état arrivée>  (ex: "0a1")
 *
 * @param nom_fichier chemin vers le fichier .txt
 * @return un pointeur vers l'automate créé, ou NULL en cas d'erreur
 */
struct _automate_state_ *lire_automate_sur_fichier(const char *nom_fichier);

/**
 * Afficher l'automate sur la sortie standard :
 *   - états initiaux (marqués E) et terminaux (marqués S)
 *   - table des transitions bien alignée
 *
 * @param as l'automate à afficher
 */
void afficher_automate(const struct _automate_state_ *as);

#endif // AUTOMATE_IO_H
