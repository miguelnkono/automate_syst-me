#define _POSIX_C_SOURCE 200809L
#include "automate_io.h"

#include "pipeline.h"
#include "automate_io.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Initialiser une étape : mutex, condition, statut EN_ATTENTE, résultat NULL.
 * @return 0 si OK, -1 si erreur d'initialisation des primitives C11
 */
static int _etape_init(__etape_pipeline__ *etape)
{
    etape->_statut_ = ETAPE_EN_ATTENTE;
    etape->_resultat_ = NULL;

  if (mtx_init(&etape->_mtx_, mtx_plain) != thrd_success) {
    fprintf(stderr, "pipeline: impossible d'initialiser le mutex\n");
    return -1;
  }
  if (cnd_init(&etape->_cnd_) != thrd_success) {
    fprintf(stderr, "pipeline: impossible d'initialiser la condition\n");
    mtx_destroy(&etape->_mtx_);
    return -1;
  }
  return 0;
}

/**
 * Détruire les primitives d'une étape et libérer son résultat si présent.
 */
static void _etape_detruire(__etape_pipeline__ *etape) {
  cnd_destroy(&etape->_cnd_);
  mtx_destroy(&etape->_mtx_);

  if (etape->_resultat_ != NULL) {
    automate_state_destroy(etape->_resultat_);
    etape->_resultat_ = NULL;
  }
}

/**
 * Marquer une étape comme terminée avec un statut donné et signaler tous les
 * threads qui l'attendent via cnd_broadcast.
 */
static void _etape_signaler(__etape_pipeline__ *etape,
                            __etape_statut__    statut,
                            __automate_state__ *resultat)
{
    mtx_lock(&etape->_mtx_);
    etape->_statut_   = statut;
    etape->_resultat_ = resultat;
    cnd_broadcast(&etape->_cnd_);
    mtx_unlock(&etape->_mtx_);
}

static __automate_state__ *_op_standardiser(const __automate_state__ *af)
{
    __automate_state__ *sfa = automate_state_create();
    if (sfa == NULL) return NULL;

    int nb_sym   = af->_nombre_symboles_;
    int nb_etats = af->_nombre_etats_;
    size_t nb_tr = _DA_LENGTH(af->_transitions_);

  // le nouvel état initial i' reçoit le numero nb_etats
  uint8_t i_prime = (uint8_t)nb_etats;

    sfa->_nombre_symboles_   = (uint8_t)nb_sym;
    sfa->_nombre_etats_      = (uint8_t)(nb_etats + 1);
    sfa->_nombre_transaction_ = 0;  // mis à jour au fur et à mesure

    sfa->_etat_initiaux_._etats_ = 1;
    _DA_PUSH(uint8_t, &sfa->_etat_initiaux_._etat_numeros_, i_prime);

    // Si au moins un état initial de af était final, i' est aussi final.
    int i_prime_est_final = 0;
    for (uint8_t f = 0; f < af->_etat_finaux_._etats_; f++)
    {
        uint8_t ef = _DA_GET(uint8_t, af->_etat_finaux_._etat_numeros_, f);
        _DA_PUSH(uint8_t, &sfa->_etat_finaux_._etat_numeros_, ef);
        sfa->_etat_finaux_._etats_++;

        // vérifier si cet état final est aussi un état initial de af
        for (uint8_t ii = 0; ii < af->_etat_initiaux_._etats_; ii++)
        {
            if (_DA_GET(uint8_t, af->_etat_initiaux_._etat_numeros_, ii) == ef)
            {
                i_prime_est_final = 1;
            }
        }
    }
    if (i_prime_est_final)
    {
        _DA_PUSH(uint8_t, &sfa->_etat_finaux_._etat_numeros_, i_prime);
        sfa->_etat_finaux_._etats_++;
    }

  // Transitions : copier toutes les transitions existantes
  for (size_t k = 0; k < nb_tr; k++) {
    __automate_transition__ t =
        _DA_GET(__automate_transition__, af->_transitions_, k);
    _DA_PUSH(__automate_transition__, &sfa->_transitions_, t);
    sfa->_nombre_transaction_++;
  }

    // Transitions de i' : dupliquer celles de chaque état initial de af
    // Pour chaque état initial ii de af, pour chaque transition ii -sym-> dest,
    // ajouter i' -sym-> dest dans sfa.
    for (uint8_t ii = 0; ii < af->_etat_initiaux_._etats_; ii++)
    {
        uint8_t etat_init = _DA_GET(uint8_t, af->_etat_initiaux_._etat_numeros_, ii);

        for (size_t k = 0; k < nb_tr; k++)
        {
            __automate_transition__ t = _DA_GET(__automate_transition__, af->_transitions_, k);
            if (t.etat_depart == etat_init)
            {
                __automate_transition__ nouvelle;
                nouvelle.etat_depart = i_prime;
                nouvelle.symbole     = t.symbole;
                nouvelle.etat_arrive = t.etat_arrive;
                _DA_PUSH(__automate_transition__, &sfa->_transitions_, nouvelle);
                sfa->_nombre_transaction_++;
            }
        }
    }

  return sfa;
}

// On représente chaque état de l'AFDC comme un bitmask uint64_t :
// le bit k est 1 si l'état k de l'AF original fait partie de cet ensemble.
// Limite : 64 états, ce qui couvre tous les automates du sujet.
//
// L'état poubelle est représenté par le bitmask 0 (ensemble vide).
// Il est ajouté seulement si au moins une case de la table est vide.

#define _DET_MAX_ETATS_AFDC 256   // max états dans l'AFDC (2^N peut exploser, on borne)
#define _DET_POUBELLE       UINT64_MAX  // sentinelle pour "pas encore attribué"

/**
 * Calculer δ*(masque, symbole) : l'ensemble des états atteignables depuis
 * tous les états du masque en lisant le symbole donné.
 */
static uint64_t _det_delta(const __automate_state__ *af,
                            uint64_t masque, char symbole)
{
    uint64_t res = 0;
    size_t nb_tr = _DA_LENGTH(af->_transitions_);

    for (int e = 0; e < af->_nombre_etats_; e++)
    {
        if (!(masque & ((uint64_t)1 << e))) continue;
        for (size_t k = 0; k < nb_tr; k++)
        {
            __automate_transition__ t =
                _DA_GET(__automate_transition__, af->_transitions_, k);
            if (t.etat_depart == (uint8_t)e && (char)t.symbole == symbole)
                res |= (uint64_t)1 << t.etat_arrive;
        }
    }
    return res;
}

/**
 * Construire le nom d'un état de l'AFDC à partir de son bitmask.
 * Ex: masque 0b0101 (états 0 et 2) → "0.2"
 * Masque 0 → "P" (poubelle)
 * Le résultat est alloué avec malloc — à libérer par l'appelant.
 */
static char *_det_nom_depuis_masque(uint64_t masque, int nb_etats_af)
{
    if (masque == 0) return strdup("P");

    char buf[512] = "";
    int premier = 1;
    for (int e = 0; e < nb_etats_af; e++)
    {
        if (masque & ((uint64_t)1 << e))
        {
            char num[16];
            if (!premier) strncat(buf, ".", sizeof(buf) - strlen(buf) - 1);
            snprintf(num, sizeof(num), "%d", e);
            strncat(buf, num, sizeof(buf) - strlen(buf) - 1);
            premier = 0;
        }
    }
    return strdup(buf);
}

static __automate_state__ *_op_determiniser(const __automate_state__ *af)
{
    int nb_sym      = af->_nombre_symboles_;
    int nb_etats_af = af->_nombre_etats_;

    // Table de l'AFDC : masques[i] = bitmask de l'état i de l'AFDC
    uint64_t masques[_DET_MAX_ETATS_AFDC];
    int      nb_etats_afdc = 0;
    int      besoin_poubelle = 0;

  // file de travail : indices dans masques[] à traiter
  int file[_DET_MAX_ETATS_AFDC];
  int file_debut = 0, file_fin = 0;

    // État initial de l'AFDC = ensemble des états initiaux de l'AF
    uint64_t masque_init = 0;
    for (uint8_t i = 0; i < af->_etat_initiaux_._etats_; i++)
        masque_init |= (uint64_t)1 << _DA_GET(uint8_t,
                                              af->_etat_initiaux_._etat_numeros_, i);

  masques[nb_etats_afdc++] = masque_init;
  file[file_fin++] = 0;

  // Table de transitions de l'AFDC : trans[i][s] = indice état arrivée
  // -1 = poubelle
  int trans[_DET_MAX_ETATS_AFDC][26];
  for (int i = 0; i < _DET_MAX_ETATS_AFDC; i++)
    for (int s = 0; s < 26; s++)
      trans[i][s] = -1;

  while (file_debut < file_fin) {
    int idx = file[file_debut++];
    uint64_t masque = masques[idx];

    for (int s = 0; s < nb_sym; s++) {
      uint64_t dest = _det_delta(af, masque, (char)('a' + s));

            if (dest == 0)
            {
                // case vide → poubelle
                besoin_poubelle = 1;
                trans[idx][s]   = -1;
                continue;
            }

            // chercher si dest est déjà connu
            int found = -1;
            for (int j = 0; j < nb_etats_afdc; j++)
                if (masques[j] == dest) { found = j; break; }

            if (found == -1)
            {
                // nouvel état
                if (nb_etats_afdc >= _DET_MAX_ETATS_AFDC)
                {
                    fprintf(stderr,
                            "determinisation: trop d'états (%d max)\n",
                            _DET_MAX_ETATS_AFDC);
                    return NULL;
                }
                found = nb_etats_afdc;
                masques[nb_etats_afdc++] = dest;
                file[file_fin++]         = found;
            }
            trans[idx][s] = found;
        }
    }

  int idx_poubelle = -1;
  if (besoin_poubelle) {
    idx_poubelle = nb_etats_afdc++;
    // la poubelle se boucle sur elle-même pour tous les symboles
    for (int s = 0; s < nb_sym; s++)
      trans[idx_poubelle][s] = idx_poubelle;
    // corriger les cases vides pour pointer vers la poubelle
    for (int i = 0; i < nb_etats_afdc - 1; i++)
      for (int s = 0; s < nb_sym; s++)
        if (trans[i][s] == -1)
          trans[i][s] = idx_poubelle;
  }

    __automate_state__ *afdc = automate_state_create();
    if (afdc == NULL) return NULL;

    afdc->_nombre_symboles_ = (uint8_t)nb_sym;
    afdc->_nombre_etats_    = (uint8_t)nb_etats_afdc;

    // état initial : indice 0 (= masque_init)
    afdc->_etat_initiaux_._etats_ = 1;
    _DA_PUSH(uint8_t, &afdc->_etat_initiaux_._etat_numeros_, 0);

    // états finaux : tout état dont le masque contient au moins un état final de AF
    for (int i = 0; i < nb_etats_afdc; i++)
    {
        if (i == idx_poubelle) continue;   // la poubelle n'est jamais finale
        int est_final = 0;
        for (uint8_t f = 0; f < af->_etat_finaux_._etats_ && !est_final; f++)
        {
            uint8_t ef = _DA_GET(uint8_t, af->_etat_finaux_._etat_numeros_, f);
            if (masques[i] & ((uint64_t)1 << ef)) est_final = 1;
        }
        if (est_final)
        {
            _DA_PUSH(uint8_t, &afdc->_etat_finaux_._etat_numeros_, (uint8_t)i);
            afdc->_etat_finaux_._etats_++;
        }
    }

    // transitions
    for (int i = 0; i < nb_etats_afdc; i++)
        for (int s = 0; s < nb_sym; s++)
        {
            __automate_transition__ t;
            t.etat_depart = (uint8_t)i;
            t.symbole     = (short)('a' + s);
            t.etat_arrive = (uint8_t)trans[i][s];
            _DA_PUSH(__automate_transition__, &afdc->_transitions_, t);
            afdc->_nombre_transaction_++;
        }

    afdc->_noms_etats_ = (char **) calloc(nb_etats_afdc, sizeof(char *));
    if (afdc->_noms_etats_ == NULL)
    {
        automate_state_destroy(afdc);
        return NULL;
    }
    for (int i = 0; i < nb_etats_afdc; i++)
    {
        if (i == idx_poubelle)
            afdc->_noms_etats_[i] = strdup("P");
        else
            afdc->_noms_etats_[i] = _det_nom_depuis_masque(masques[i], nb_etats_af);
    }

  return afdc;
}

#define _MIN_MAX_ETATS 256

/**
 * Construire le nom d'un état du AFDCM à partir des noms des états de l'AFDC
 * qui le composent (séparés par ",").
 * Alloué avec malloc — à libérer par l'appelant.
 */
static char *_min_nom_classe(const __automate_state__ *afdc,
                              int *membres, int nb_membres)
{
    char buf[1024] = "";
    for (int i = 0; i < nb_membres; i++)
    {
        if (i > 0) strncat(buf, ",", sizeof(buf) - strlen(buf) - 1);
        const char *nom;
        char tmp[64];
        if (afdc->_noms_etats_ && afdc->_noms_etats_[membres[i]])
            nom = afdc->_noms_etats_[membres[i]];
        else
        {
            snprintf(tmp, sizeof(tmp), "%d", membres[i]);
            nom = tmp;
        }
        strncat(buf, nom, sizeof(buf) - strlen(buf) - 1);
    }
    return strdup(buf);
}

static __automate_state__ *_op_minimiser(const __automate_state__ *afdc)
{
    int nb_etats = afdc->_nombre_etats_;
    int nb_sym   = afdc->_nombre_symboles_;

  int partition[_MIN_MAX_ETATS];
  int nb_classes = 0;

    // P0 : classe 0 = non-finaux, classe 1 = finaux
    // (si tous finaux ou tous non-finaux → une seule classe)
    for (int e = 0; e < nb_etats; e++) partition[e] = 0;

    for (uint8_t f = 0; f < afdc->_etat_finaux_._etats_; f++)
    {
        uint8_t ef = _DA_GET(uint8_t, afdc->_etat_finaux_._etat_numeros_, f);
        partition[ef] = 1;
    }

    // compter les classes réellement présentes et les renumeroter 0..k
    int present[2] = {0, 0};
    for (int e = 0; e < nb_etats; e++) present[partition[e]] = 1;
    if (!present[0])
    {
        // tous finaux → une seule classe 0
        for (int e = 0; e < nb_etats; e++) partition[e] = 0;
        nb_classes = 1;
    }
    else if (!present[1])
    {
        nb_classes = 1;   // tous non-finaux
    }
    else
    {
        nb_classes = 2;
    }

    // ── Journal des partitions successives (stocké dans _log_, affiché par le main) ──
    // On évite tout printf ici : ce thread tourne en parallèle du main thread
    // qui peut lui-même être en train d'afficher. Tout texte produit pendant
    // le calcul est accumulé dans ce buffer puis transféré dans afdcm->_log_.
    size_t log_cap  = 4096;
    size_t log_len  = 0;
    char  *log_buf  = (char *) malloc(log_cap);
    if (log_buf == NULL) return NULL;
    log_buf[0] = '\0';

#define _LOG_APPEND(fmt, ...) \
    do { \
        int _n = snprintf(log_buf + log_len, log_cap - log_len, fmt, ##__VA_ARGS__); \
        if (_n > 0) { \
            log_len += (size_t)_n; \
            if (log_len >= log_cap - 256) { \
                log_cap *= 2; \
                char *_tmp = realloc(log_buf, log_cap); \
                if (_tmp == NULL) { free(log_buf); return NULL; } \
                log_buf = _tmp; \
            } \
        } \
    } while (0)

  _LOG_APPEND("\n  [Minimisation — partitions successives]\n");

  // table de transitions de l'AFDC : delta[e][s] = état arrivée
  int delta[_MIN_MAX_ETATS][26];
  for (int e = 0; e < nb_etats; e++)
    for (int s = 0; s < nb_sym; s++)
      delta[e][s] = -1;

  size_t nb_tr = _DA_LENGTH(afdc->_transitions_);
  for (size_t k = 0; k < nb_tr; k++) {
    __automate_transition__ t =
        _DA_GET(__automate_transition__, afdc->_transitions_, k);
    int s = (char)t.symbole - 'a';
    delta[t.etat_depart][s] = t.etat_arrive;
  }

    int iteration = 0;
    int change    = 1;

    while (change)
    {
        // Enregistrer la partition courante dans le log
        _LOG_APPEND("  P%d : { ", iteration++);
        for (int c = 0; c < nb_classes; c++)
        {
            _LOG_APPEND("{");
            int premier = 1;
            for (int e = 0; e < nb_etats; e++)
            {
                if (partition[e] == c)
                {
                    if (!premier) _LOG_APPEND(",");
                    if (afdc->_noms_etats_ && afdc->_noms_etats_[e])
                        _LOG_APPEND("%s", afdc->_noms_etats_[e]);
                    else
                        _LOG_APPEND("%d", e);
                    premier = 0;
                }
            }
            _LOG_APPEND("}%s", c < nb_classes - 1 ? ", " : "");
        }
        _LOG_APPEND(" }\n");

        change = 0;
        int nouvelle_partition[_MIN_MAX_ETATS];
        for (int e = 0; e < nb_etats; e++) nouvelle_partition[e] = partition[e];
        int nouveau_nb_classes = nb_classes;

        for (int c = 0; c < nb_classes; c++)
        {
            int membres[_MIN_MAX_ETATS];
            int nb_membres = 0;
            for (int e = 0; e < nb_etats; e++)
                if (partition[e] == c) membres[nb_membres++] = e;

            if (nb_membres <= 1) continue;

            int ref = membres[0];
            for (int i = 1; i < nb_membres; i++)
            {
                int e        = membres[i];
                int different = 0;
                for (int s = 0; s < nb_sym && !different; s++)
                {
                    int dest_ref   = delta[ref][s];
                    int dest_e     = delta[e][s];
                    int classe_ref = (dest_ref >= 0) ? partition[dest_ref] : -1;
                    int classe_e   = (dest_e   >= 0) ? partition[dest_e]   : -1;
                    if (classe_ref != classe_e) different = 1;
                }
                if (different)
                {
                    nouvelle_partition[e] = nouveau_nb_classes++;
                    change = 1;
                }
            }
        }

        for (int e = 0; e < nb_etats; e++) partition[e] = nouvelle_partition[e];
        nb_classes = nouveau_nb_classes;
    }

  _LOG_APPEND("  → stable en %d itération(s), %d état(s)\n", iteration,
              nb_classes);

  if (nb_classes == nb_etats)
    _LOG_APPEND("  L'automate est déjà minimal.\n");

#undef _LOG_APPEND

    // Construire l'AFDCM
    __automate_state__ *afdcm = automate_state_create();
    if (afdcm == NULL) { free(log_buf); return NULL; }

    afdcm->_nombre_symboles_ = (uint8_t)nb_sym;
    afdcm->_nombre_etats_    = (uint8_t)nb_classes;

    // état initial : classe de l'état initial de l'AFDC
    uint8_t init_afdc = _DA_GET(uint8_t, afdc->_etat_initiaux_._etat_numeros_, 0);
    uint8_t classe_init = (uint8_t)partition[init_afdc];
    afdcm->_etat_initiaux_._etats_ = 1;
    _DA_PUSH(uint8_t, &afdcm->_etat_initiaux_._etat_numeros_, classe_init);

    // états finaux : classes qui contiennent au moins un état final de l'AFDC
    int deja_final[_MIN_MAX_ETATS] = {0};
    for (uint8_t f = 0; f < afdc->_etat_finaux_._etats_; f++)
    {
        uint8_t ef = _DA_GET(uint8_t, afdc->_etat_finaux_._etat_numeros_, f);
        int c = partition[ef];
        if (!deja_final[c])
        {
            deja_final[c] = 1;
            _DA_PUSH(uint8_t, &afdcm->_etat_finaux_._etat_numeros_, (uint8_t)c);
            afdcm->_etat_finaux_._etats_++;
        }
    }

    // transitions : pour chaque classe c, prendre un représentant et ses transitions
    int deja_trans[_MIN_MAX_ETATS][26] = {{0}};
    for (int e = 0; e < nb_etats; e++)
    {
        int c = partition[e];
        for (int s = 0; s < nb_sym; s++)
        {
            if (deja_trans[c][s]) continue;
            deja_trans[c][s] = 1;
            int dest = delta[e][s];
            if (dest < 0) continue;
            __automate_transition__ t;
            t.etat_depart = (uint8_t)c;
            t.symbole     = (short)('a' + s);
            t.etat_arrive = (uint8_t)partition[dest];
            _DA_PUSH(__automate_transition__, &afdcm->_transitions_, t);
            afdcm->_nombre_transaction_++;
        }
    }

    afdcm->_noms_etats_ = (char **) calloc(nb_classes, sizeof(char *));
    if (afdcm->_noms_etats_ == NULL)
    {
        free(log_buf);
        automate_state_destroy(afdcm);
        return NULL;
    }
    for (int c = 0; c < nb_classes; c++)
    {
        int membres[_MIN_MAX_ETATS];
        int nb_membres = 0;
        for (int e = 0; e < nb_etats; e++)
            if (partition[e] == c) membres[nb_membres++] = e;
        afdcm->_noms_etats_[c] = _min_nom_classe(afdc, membres, nb_membres);
    }

  // transférer le journal dans l'automate — sera affiché par le main thread
  afdcm->_log_ = log_buf;

  return afdcm;
}

static __automate_state__ *_op_complementaire(const __automate_state__ *afdcm)
{
    int nb_etats = afdcm->_nombre_etats_;
    int nb_sym   = afdcm->_nombre_symboles_;

    __automate_state__ *acomp = automate_state_create();
    if (acomp == NULL) return NULL;

    acomp->_nombre_symboles_ = (uint8_t)nb_sym;
    acomp->_nombre_etats_    = (uint8_t)nb_etats;

    acomp->_etat_initiaux_._etats_ = afdcm->_etat_initiaux_._etats_;
    for (uint8_t i = 0; i < afdcm->_etat_initiaux_._etats_; i++)
    {
        uint8_t e = _DA_GET(uint8_t, afdcm->_etat_initiaux_._etat_numeros_, i);
        _DA_PUSH(uint8_t, &acomp->_etat_initiaux_._etat_numeros_, e);
    }

    for (int e = 0; e < nb_etats; e++)
    {
        if (!_groupe_contient(&afdcm->_etat_finaux_, (uint8_t)e))
        {
            _DA_PUSH(uint8_t, &acomp->_etat_finaux_._etat_numeros_, (uint8_t)e);
            acomp->_etat_finaux_._etats_++;
        }
    }

  size_t nb_tr = _DA_LENGTH(afdcm->_transitions_);
  for (size_t k = 0; k < nb_tr; k++) {
    __automate_transition__ t =
        _DA_GET(__automate_transition__, afdcm->_transitions_, k);
    _DA_PUSH(__automate_transition__, &acomp->_transitions_, t);
    acomp->_nombre_transaction_++;
  }

    if (afdcm->_noms_etats_ != NULL)
    {
        acomp->_noms_etats_ = (char **) calloc(nb_etats, sizeof(char *));
        if (acomp->_noms_etats_ == NULL)
        {
            automate_state_destroy(acomp);
            return NULL;
        }
        for (int e = 0; e < nb_etats; e++)
        {
            if (afdcm->_noms_etats_[e] != NULL)
                acomp->_noms_etats_[e] = strdup(afdcm->_noms_etats_[e]);
        }
    }

  return acomp;
}

// T1 : standardisation — repart de l'automate original
typedef struct
{
    const __automate_state__  *_af_;
    const __automate_tests__  *_tests_;
    __etape_pipeline__         *_etape_;
} __args_standardisation__;

// T2 : déterminisation — repart de l'automate original
typedef struct
{
    const __automate_state__  *_af_;
    const __automate_tests__  *_tests_;
    __etape_pipeline__         *_etape_;
} __args_determinisation__;

// T3 : minimisation — attend T2
typedef struct
{
    const __automate_state__  *_af_;
    const __automate_tests__  *_tests_;
    __etape_pipeline__         *_etape_determinisation_;
    __etape_pipeline__         *_etape_;
} __args_minimisation__;

// T4 : complémentaire — attend T3
typedef struct
{
    const __automate_state__  *_af_;
    const __automate_tests__  *_tests_;
    __etape_pipeline__         *_etape_minimisation_;
    __etape_pipeline__         *_etape_;
} __args_complementaire__;

static int _thread_standardisation(void *arg) {
  __args_standardisation__ *a = (__args_standardisation__ *)arg;

  mtx_lock(&a->_etape_->_mtx_);
  a->_etape_->_statut_ = ETAPE_EN_COURS;
  mtx_unlock(&a->_etape_->_mtx_);

  // si déjà standard : rien à faire
  if (a->_tests_->est_standard) {
    _etape_signaler(a->_etape_, ETAPE_IGNOREE, NULL);
    free(a);
    return 0;
  }

  __automate_state__ *res = _op_standardiser(a->_af_);

  _etape_signaler(a->_etape_, res != NULL ? ETAPE_PRETE : ETAPE_ERREUR, res);

  free(a);
  return 0;
}

static int _thread_determinisation(void *arg) {
  __args_determinisation__ *a = (__args_determinisation__ *)arg;

  mtx_lock(&a->_etape_->_mtx_);
  a->_etape_->_statut_ = ETAPE_EN_COURS;
  mtx_unlock(&a->_etape_->_mtx_);

  // si déjà déterministe et complet : l'AFDC est l'automate lui-même
  // on le signale comme ignoré (T3 et T4 travailleront à partir de l'original)
  if (a->_tests_->est_deterministe && a->_tests_->est_complet) {
    _etape_signaler(a->_etape_, ETAPE_IGNOREE, NULL);
    free(a);
    return 0;
  }

  __automate_state__ *res = _op_determiniser(a->_af_);

  _etape_signaler(a->_etape_, res != NULL ? ETAPE_PRETE : ETAPE_ERREUR, res);

  free(a);
  return 0;
}

static int _thread_minimisation(void *arg) {
  __args_minimisation__ *a = (__args_minimisation__ *)arg;

  mtx_lock(&a->_etape_->_mtx_);
  a->_etape_->_statut_ = ETAPE_EN_COURS;
  mtx_unlock(&a->_etape_->_mtx_);

    // attendre que la déterminisation soit terminée
    mtx_lock(&a->_etape_determinisation_->_mtx_);
    while (a->_etape_determinisation_->_statut_ == ETAPE_EN_ATTENTE ||
           a->_etape_determinisation_->_statut_ == ETAPE_EN_COURS)
    {
        cnd_wait(&a->_etape_determinisation_->_cnd_,
                 &a->_etape_determinisation_->_mtx_);
    }
    __automate_state__ *afdc   = a->_etape_determinisation_->_resultat_;
    __etape_statut__    statut = a->_etape_determinisation_->_statut_;
    mtx_unlock(&a->_etape_determinisation_->_mtx_);

  // si T2 ignoré c'est que l'original EST déjà l'AFDC → on l'utilise
  // directement
  if (statut == ETAPE_IGNOREE && a->_tests_->est_deterministe &&
      a->_tests_->est_complet)
    afdc = ((__automate_state__ *)(a->_af_));

  // si T2 a échoué → on ne peut pas minimiser
  if (statut == ETAPE_ERREUR || afdc == NULL) {
    _etape_signaler(a->_etape_, ETAPE_IGNOREE, NULL);
    free(a);
    return 0;
  }

  __automate_state__ *res = _op_minimiser(afdc);

  _etape_signaler(a->_etape_, res != NULL ? ETAPE_PRETE : ETAPE_ERREUR, res);

  free(a);
  return 0;
}

static int _thread_complementaire(void *arg) {
  __args_complementaire__ *a = (__args_complementaire__ *)arg;

  mtx_lock(&a->_etape_->_mtx_);
  a->_etape_->_statut_ = ETAPE_EN_COURS;
  mtx_unlock(&a->_etape_->_mtx_);

    // attendre que la minimisation soit terminée
    mtx_lock(&a->_etape_minimisation_->_mtx_);
    while (a->_etape_minimisation_->_statut_ == ETAPE_EN_ATTENTE ||
           a->_etape_minimisation_->_statut_ == ETAPE_EN_COURS)
    {
        cnd_wait(&a->_etape_minimisation_->_cnd_,
                 &a->_etape_minimisation_->_mtx_);
    }
    __automate_state__ *afdcm  = a->_etape_minimisation_->_resultat_;
    __etape_statut__    statut = a->_etape_minimisation_->_statut_;
    mtx_unlock(&a->_etape_minimisation_->_mtx_);

  if (statut == ETAPE_ERREUR || afdcm == NULL) {
    _etape_signaler(a->_etape_, ETAPE_IGNOREE, NULL);
    free(a);
    return 0;
  }

  __automate_state__ *res = _op_complementaire(afdcm);

  _etape_signaler(a->_etape_, res != NULL ? ETAPE_PRETE : ETAPE_ERREUR, res);

  free(a);
  return 0;
}

__pipeline__ *pipeline_creer(const __automate_state__ *af)
{
    __pipeline__ *p = (__pipeline__ *) malloc(sizeof(__pipeline__));
    if (p == NULL)
    {
        fprintf(stderr, "pipeline: impossible d'allouer le pipeline\n");
        return NULL;
    }

  p->_af_original_ = af;

  // Les résultats sont stockés dans p->_tests_ et transmis en lecture seule
  // à chaque thread via leurs args. Pas de race condition : les threads ne
  // modifient jamais _tests_.
  p->_tests_ = automate_tester(af);

    // initialiser toutes les étapes
    if (_etape_init(&p->_etape_standardisation_)  != 0) goto erreur;
    if (_etape_init(&p->_etape_determinisation_)  != 0) goto erreur;
    if (_etape_init(&p->_etape_minimisation_)     != 0) goto erreur;
    if (_etape_init(&p->_etape_complementaire_)   != 0) goto erreur;

    // Préparer les arguments de chaque thread
    __args_standardisation__ *args_std = malloc(sizeof(__args_standardisation__));
    __args_determinisation__ *args_det = malloc(sizeof(__args_determinisation__));
    __args_minimisation__    *args_min = malloc(sizeof(__args_minimisation__));
    __args_complementaire__  *args_cmp = malloc(sizeof(__args_complementaire__));

    if (!args_std || !args_det || !args_min || !args_cmp)
    {
        free(args_std); free(args_det); free(args_min); free(args_cmp);
        fprintf(stderr, "pipeline: impossible d'allouer les arguments des threads\n");
        goto erreur;
    }

    args_std->_af_     = af;
    args_std->_tests_  = &p->_tests_;
    args_std->_etape_  = &p->_etape_standardisation_;

    args_det->_af_     = af;
    args_det->_tests_  = &p->_tests_;
    args_det->_etape_  = &p->_etape_determinisation_;

    args_min->_af_                    = af;
    args_min->_tests_                 = &p->_tests_;
    args_min->_etape_determinisation_ = &p->_etape_determinisation_;
    args_min->_etape_                 = &p->_etape_minimisation_;

    args_cmp->_af_                   = af;
    args_cmp->_tests_                = &p->_tests_;
    args_cmp->_etape_minimisation_   = &p->_etape_minimisation_;
    args_cmp->_etape_                = &p->_etape_complementaire_;

    // Lancer les threads
    if (thrd_create(&p->_etape_standardisation_._thread_,
                    _thread_standardisation, args_std) != thrd_success)
    {
        fprintf(stderr, "pipeline: impossible de lancer T1 (standardisation)\n");
        free(args_std); free(args_det); free(args_min); free(args_cmp);
        goto erreur;
    }

    if (thrd_create(&p->_etape_determinisation_._thread_,
                    _thread_determinisation, args_det) != thrd_success)
    {
        fprintf(stderr, "pipeline: impossible de lancer T2 (déterminisation)\n");
        thrd_join(p->_etape_standardisation_._thread_, NULL);
        free(args_det); free(args_min); free(args_cmp);
        goto erreur;
    }

    if (thrd_create(&p->_etape_minimisation_._thread_,
                    _thread_minimisation, args_min) != thrd_success)
    {
        fprintf(stderr, "pipeline: impossible de lancer T3 (minimisation)\n");
        thrd_join(p->_etape_standardisation_._thread_, NULL);
        thrd_join(p->_etape_determinisation_._thread_, NULL);
        free(args_min); free(args_cmp);
        goto erreur;
    }

    if (thrd_create(&p->_etape_complementaire_._thread_,
                    _thread_complementaire, args_cmp) != thrd_success)
    {
        fprintf(stderr, "pipeline: impossible de lancer T4 (complémentaire)\n");
        thrd_join(p->_etape_standardisation_._thread_, NULL);
        thrd_join(p->_etape_determinisation_._thread_, NULL);
        thrd_join(p->_etape_minimisation_._thread_,    NULL);
        free(args_cmp);
        goto erreur;
    }

  return p;

erreur:
  free(p);
  return NULL;
}

__etape_statut__ pipeline_attendre_etape(__etape_pipeline__ *etape) {
  mtx_lock(&etape->_mtx_);
  while (etape->_statut_ == ETAPE_EN_ATTENTE ||
         etape->_statut_ == ETAPE_EN_COURS) {
    cnd_wait(&etape->_cnd_, &etape->_mtx_);
  }
  __etape_statut__ statut = etape->_statut_;
  mtx_unlock(&etape->_mtx_);
  return statut;
}

void pipeline_detruire(__pipeline__ *p)
{
    if (p == NULL) return;

    // joindre tous les threads pour s'assurer qu'ils ont terminé
    thrd_join(p->_etape_standardisation_._thread_, NULL);
    thrd_join(p->_etape_determinisation_._thread_, NULL);
    thrd_join(p->_etape_minimisation_._thread_,    NULL);
    thrd_join(p->_etape_complementaire_._thread_,  NULL);

  // détruire les primitives et libérer les résultats
  _etape_detruire(&p->_etape_standardisation_);
  _etape_detruire(&p->_etape_determinisation_);
  _etape_detruire(&p->_etape_minimisation_);
  _etape_detruire(&p->_etape_complementaire_);

  free(p);
}
