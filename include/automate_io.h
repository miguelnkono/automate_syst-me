#ifndef AUTOMATE_IO_H
#define AUTOMATE_IO_H

// data_structure.h est autonome : il définit __automate_transition__,
// __automate_state__, __da_header__, et les macros DA.
// automate_io.h s'appuie sur ces types — sens unique, pas de cycle.
#include "data_structure.h"

/**
 * Tester si un état est présent dans un groupe (initiaux ou finaux).
 * @param groupe  le groupe à tester
 * @param etat    le numéro d'état cherché
 * @return        1 si présent, 0 sinon
 */
int _groupe_contient(const __automate_node_info__ *groupe, uint8_t etat);

/**
 * Résultats des trois tests structurels sur un automate.
 * Calculés une seule fois après la lecture, puis transmis au pipeline.
 *
 * @param est_standard           1 si standard, 0 sinon
 * @param est_deterministe       1 si déterministe, 0 sinon
 * @param est_complet            1 si complet, 0 sinon
 * @param nb_initiaux_exces      surplus d'états initiaux (standard exige exactement 1)
 * @param etat_initial_est_cible 1 si l'état initial est cible d'une transition
 * @param etat_nd                premier état non-déterministe (-1 si aucun)
 * @param symbole_nd             symbole ambigu pour etat_nd  (-1 si aucun)
 * @param etat_incomplet         premier état sans transition sur un symbole (-1 si aucun)
 * @param symbole_incomplet      symbole manquant pour etat_incomplet (-1 si aucun)
 */
typedef struct _automate_tests_
{
    int est_standard;
    int est_deterministe;
    int est_complet;

    int nb_initiaux_exces;
    int etat_initial_est_cible;

    int etat_nd;
    int symbole_nd;

    int etat_incomplet;
    int symbole_incomplet;
} __automate_tests__;

/**
 * Effectuer les trois tests structurels et retourner les résultats.
 * @param as  l'automate à tester
 * @return    struct __automate_tests__ remplie
 */
__automate_tests__ automate_tester(const __automate_state__ *as);

/**
 * Afficher les résultats des tests avec les raisons de non-conformité.
 * @param t  résultats retournés par automate_tester()
 */
void afficher_tests(const __automate_tests__ *t);

/**
 * Lire un automate depuis un fichier texte et le charger en mémoire.
 * Format :
 *   ligne 1 : nombre de symboles
 *   ligne 2 : nombre d'états
 *   ligne 3 : <nb états initiaux> <numéro> ...
 *   ligne 4 : <nb états terminaux> <numéro> ...
 *   ligne 5 : nombre de transitions
 *   lignes suivantes : <état départ><symbole><état arrivée>  (ex: "0a1")
 *
 * @param nom_fichier  chemin vers le fichier .txt
 * @return             pointeur vers l'automate, ou NULL en cas d'erreur
 */
__automate_state__ *lire_automate_sur_fichier(const char *nom_fichier);

/**
 * Afficher l'automate : états initiaux (E), terminaux (S), table des transitions.
 * @param as  l'automate à afficher
 */
void afficher_automate(const __automate_state__ *as);

#endif // AUTOMATE_IO_H
