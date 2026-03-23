#ifndef PIPELINE_H
#define PIPELINE_H

#include "automate_io.h"
#include "data_structure.h"

#include <stdint.h>
#include <threads.h>

typedef enum _etape_statut_ {
  ETAPE_EN_ATTENTE,
  ETAPE_EN_COURS,
  ETAPE_PRETE,
  ETAPE_IGNOREE,
  ETAPE_ERREUR
} __etape_statut__;

/**
 * Une étape du pipeline.
 * Chaque étape correspond à une opération sur l'automate.
 * Le thread de cette étape signal via _cnd_ quand il passe à ETAPE_PRETE.
 *
 * @param _thread_      le thread C11 qui exécute l'opération
 * @param _mtx_         mutex protégeant _statut_ et _resultat_
 * @param _cnd_         condition signalée quand _statut_ == ETAPE_PRETE |
 * ETAPE_IGNOREE | ETAPE_ERREUR
 * @param _statut_      état courant de l'étape
 * @param _resultat_    automate produit par l'étape (NULL si pas encore prêt ou
 * ignoré)
 */
typedef struct _etape_pipeline_ {
  thrd_t _thread_;
  mtx_t _mtx_;
  cnd_t _cnd_;
  __etape_statut__ _statut_;
  __automate_state__ *_resultat_;
} __etape_pipeline__;

/**
 * Le pipeline regroupe toutes les étapes de traitement d'un automate.
 * Les étapes sont chaînées : chaque thread attend la fin de l'étape précédente.
 *
 * @param _af_original_          automate lu depuis le fichier (lecture seule
 * par les threads)
 * @param _etape_standardisation_ standardisation si nécessaire
 * @param _etape_determinisation_ déterminisation + complétion
 * @param _etape_minimisation_    minimisation (attend T2)
 * @param _etape_complementaire_  complémentaire (attend T3)
 */
typedef struct _pipeline_ {
  const __automate_state__ *_af_original_;
  __automate_tests__ _tests_;

  __etape_pipeline__ _etape_standardisation_;
  __etape_pipeline__ _etape_determinisation_;
  __etape_pipeline__ _etape_minimisation_;
  __etape_pipeline__ _etape_complementaire_;
} __pipeline__;

/**
 * Créer et initialiser un pipeline à partir d'un automate déjà chargé en
 * mémoire. Lance immédiatement tous les threads en arrière-plan.
 *
 * @param af  automate source (doit rester valide pendant toute la durée du
 * pipeline)
 * @return    pointeur vers le pipeline, ou NULL en cas d'erreur
 */
__pipeline__ *pipeline_creer(const __automate_state__ *af);

/**
 * Attendre la fin de toutes les étapes et libérer toutes les ressources du
 * pipeline. Les __automate_state__ produits par les étapes sont également
 * détruits.
 *
 * @param p  le pipeline à détruire
 */
void pipeline_detruire(__pipeline__ *p);

/**
 * Attendre qu'une étape soit terminée et retourner son statut final.
 * Bloque le thread appelant si l'étape est encore EN_COURS.
 * Fonction utilitaire utilisée par le main avant d'afficher un résultat.
 *
 * @param etape  l'étape à attendre
 * @return       le statut final (ETAPE_PRETE, ETAPE_IGNOREE ou ETAPE_ERREUR)
 */
__etape_statut__ pipeline_attendre_etape(__etape_pipeline__ *etape);

#endif // PIPELINE_H
