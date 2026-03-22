#include "automate_io.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// ── Helpers internes ──────────────────────────────────────────────────────────

/**
 * Tester si un état donné est présent dans un groupe (initiaux / finaux).
 */
int _groupe_contient(const __automate_node_info__ *groupe, uint8_t etat)
{
    for (uint8_t i = 0; i < groupe->_etats_; i++)
    {
        if (_DA_GET(uint8_t, groupe->_etat_numéros_, i) == etat)
            return 1;
    }
    return 0;
}

/**
 * Construire la cellule de la table des transitions pour (état, symbole).
 * Plusieurs transitions possibles → "1,3" ; aucune → "--".
 * Résultat écrit dans `tampon` (doit faire au moins `taille` octets).
 */
static void _cellule_transitions(const __automate_state__ *as,
                                 uint8_t etat, char symbole,
                                 char *tampon, size_t taille)
{
    tampon[0] = '\0';
    size_t nb_transitions = _DA_LENGTH(as->_transitions_);

    for (size_t i = 0; i < nb_transitions; i++)
    {
        __automate_transition__ t = _DA_GET(__automate_transition__, as->_transitions_, i);
        if (t.etat_depart == etat && (char)t.symbole == symbole)
        {
            if (tampon[0] != '\0')
                strncat(tampon, ",", taille - strlen(tampon) - 1);

            char num[8];
            snprintf(num, sizeof(num), "%d", t.etat_arrive);
            strncat(tampon, num, taille - strlen(tampon) - 1);
        }
    }

    if (tampon[0] == '\0')
        strncpy(tampon, "--", taille - 1);
}

// ── Tests structurels ─────────────────────────────────────────────────────────

__automate_tests__ automate_tester(const __automate_state__ *as)
{
    __automate_tests__ t;

    t.est_standard           = 0;
    t.est_deterministe       = 0;
    t.est_complet            = 0;
    t.nb_initiaux_exces      = 0;
    t.etat_initial_est_cible = 0;
    t.etat_nd                = -1;
    t.symbole_nd             = -1;
    t.etat_incomplet         = -1;
    t.symbole_incomplet      = -1;

    int nb_sym   = as->_nombre_symboles_;
    int nb_etats = as->_nombre_etats_;
    size_t nb_tr = _DA_LENGTH(as->_transitions_);

    // ── Test standard ─────────────────────────────────────────────────────────
    // Condition 1 : exactement un état initial
    // Condition 2 : l'état initial n'est destination d'aucune transition

    if (as->_etat_initiaux_._etats_ == 1)
    {
        uint8_t i0 = _DA_GET(uint8_t, as->_etat_initiaux_._etat_numéros_, 0);

        // vérifier que i0 n'est la cible d'aucune transition
        int cible = 0;
        for (size_t k = 0; k < nb_tr && !cible; k++)
        {
            __automate_transition__ tr = _DA_GET(__automate_transition__, as->_transitions_, k);
            if (tr.etat_arrive == i0)
                cible = 1;
        }
        t.etat_initial_est_cible = cible;
        t.est_standard = !cible;
    }
    else
    {
        // plus d'un état initial → non standard, on note le surplus
        t.nb_initiaux_exces = (int)as->_etat_initiaux_._etats_ - 1;
    }

    // ── Test déterministe ─────────────────────────────────────────────────────
    // Condition 1 : exactement un état initial
    // Condition 2 : pour tout (état, symbole) au plus une transition

    if (as->_etat_initiaux_._etats_ == 1)
    {
        int nd_trouve = 0;

        for (int e = 0; e < nb_etats && !nd_trouve; e++)
        {
            for (int s = 0; s < nb_sym && !nd_trouve; s++)
            {
                char sym = (char)('a' + s);
                int  compte = 0;

                for (size_t k = 0; k < nb_tr; k++)
                {
                    __automate_transition__ tr = _DA_GET(__automate_transition__, as->_transitions_, k);
                    if (tr.etat_depart == (uint8_t)e && (char)tr.symbole == sym)
                        compte++;
                }

                if (compte > 1)
                {
                    nd_trouve      = 1;
                    t.etat_nd      = e;
                    t.symbole_nd   = s;
                }
            }
        }
        t.est_deterministe = !nd_trouve;
    }
    // si plusieurs états initiaux → non déterministe (etat_nd reste -1,
    // la raison est déjà capturée dans nb_initiaux_exces)

    // ── Test complet ──────────────────────────────────────────────────────────
    // Condition : pour tout (état, symbole) au moins une transition
    // (n'a de sens que si l'automate est déterministe, mais on teste quand même)

    int incomplet_trouve = 0;

    for (int e = 0; e < nb_etats && !incomplet_trouve; e++)
    {
        for (int s = 0; s < nb_sym && !incomplet_trouve; s++)
        {
            char sym    = (char)('a' + s);
            int  compte = 0;

            for (size_t k = 0; k < nb_tr; k++)
            {
                __automate_transition__ tr = _DA_GET(__automate_transition__, as->_transitions_, k);
                if (tr.etat_depart == (uint8_t)e && (char)tr.symbole == sym)
                    compte++;
            }

            if (compte == 0)
            {
                incomplet_trouve      = 1;
                t.etat_incomplet      = e;
                t.symbole_incomplet   = s;
            }
        }
    }
    t.est_complet = !incomplet_trouve;

    return t;
}

void afficher_tests(const __automate_tests__ *t)
{
    printf("=== Propriétés de l'automate ===\n\n");

    // ── Standard ──────────────────────────────────────────────────────────────
    if (t->est_standard)
    {
        printf("  Standard      : OUI\n");
    }
    else
    {
        printf("  Standard      : NON\n");
        if (t->nb_initiaux_exces > 0)
            printf("    → %d état(s) initial/initiaux en trop (doit en avoir exactement 1)\n",
                   t->nb_initiaux_exces);
        if (t->etat_initial_est_cible)
            printf("    → l'état initial est la cible d'au moins une transition\n");
    }

    // ── Déterministe ──────────────────────────────────────────────────────────
    if (t->est_deterministe)
    {
        printf("  Déterministe  : OUI\n");
    }
    else
    {
        printf("  Déterministe  : NON\n");
        if (t->nb_initiaux_exces > 0)
            printf("    → plusieurs états initiaux (%d en trop)\n", t->nb_initiaux_exces);
        if (t->etat_nd >= 0)
            printf("    → état %d : plusieurs transitions sur '%c'\n",
                   t->etat_nd, (char)('a' + t->symbole_nd));
    }

    // ── Complet ───────────────────────────────────────────────────────────────
    if (t->est_complet)
    {
        printf("  Complet       : OUI\n");
    }
    else
    {
        printf("  Complet       : NON\n");
        if (t->etat_incomplet >= 0)
            printf("    → état %d : aucune transition sur '%c'\n",
                   t->etat_incomplet, (char)('a' + t->symbole_incomplet));
    }

    printf("\n");
}

// ── Lecture ───────────────────────────────────────────────────────────────────

__automate_state__ *lire_automate_sur_fichier(const char *nom_fichier)
{
    FILE *f = fopen(nom_fichier, "r");
    if (f == NULL)
    {
        fprintf(stderr, "Impossible d'ouvrir le fichier : %s\n", nom_fichier);
        return NULL;
    }

    __automate_state__ *as = automate_state_create();
    if (as == NULL)
    {
        fclose(f);
        return NULL;
    }

    // ligne 1 : nombre de symboles
    if (fscanf(f, " %hhu", &as->_nombre_symboles_) != 1) goto erreur;

    // ligne 2 : nombre d'états
    if (fscanf(f, " %hhu", &as->_nombre_etats_) != 1) goto erreur;

    // ligne 3 : états initiaux
    if (fscanf(f, " %hhu", &as->_etat_initiaux_._etats_) != 1) goto erreur;
    for (uint8_t i = 0; i < as->_etat_initiaux_._etats_; i++)
    {
        uint8_t num;
        if (fscanf(f, " %hhu", &num) != 1) goto erreur;
        _DA_PUSH(uint8_t, &as->_etat_initiaux_._etat_numéros_, num);
    }

    // ligne 4 : états finaux
    if (fscanf(f, " %hhu", &as->_etat_finaux_._etats_) != 1) goto erreur;
    for (uint8_t i = 0; i < as->_etat_finaux_._etats_; i++)
    {
        uint8_t num;
        if (fscanf(f, " %hhu", &num) != 1) goto erreur;
        _DA_PUSH(uint8_t, &as->_etat_finaux_._etat_numéros_, num);
    }

    // ligne 5 : nombre de transitions
    if (fscanf(f, " %hhu", &as->_nombre_transaction_) != 1) goto erreur;

    // lignes suivantes : transitions "0a1"
    for (uint8_t i = 0; i < as->_nombre_transaction_; i++)
    {
        char ligne[64];
        if (fscanf(f, " %63s", ligne) != 1) goto erreur;

        // le symbole est le premier caractère non-chiffre
        size_t len = strlen(ligne);
        size_t pos_sym = 0;
        while (pos_sym < len && (ligne[pos_sym] >= '0' && ligne[pos_sym] <= '9'))
            pos_sym++;

        if (pos_sym == 0 || pos_sym >= len - 1) goto erreur;

        // état de départ : chiffres avant le symbole
        char buf_dep[16];
        strncpy(buf_dep, ligne, pos_sym);
        buf_dep[pos_sym] = '\0';
        uint8_t dep = (uint8_t) atoi(buf_dep);

        char sym = ligne[pos_sym];

        // état d'arrivée : chiffres après le symbole
        uint8_t arr = (uint8_t) atoi(ligne + pos_sym + 1);

        __automate_transition__ t;
        t.etat_depart = dep;
        t.symbole     = sym;
        t.etat_arrive = arr;
        _DA_PUSH(__automate_transition__, &as->_transitions_, t);
    }

    fclose(f);
    return as;

erreur:
    fprintf(stderr, "Erreur de lecture dans le fichier : %s\n", nom_fichier);
    fclose(f);
    automate_state_destroy(as);
    return NULL;
}

// ── Affichage ─────────────────────────────────────────────────────────────────

/**
 * Retourner le nom d'affichage d'un état :
 * - si _noms_etats_ est défini et le nom non NULL → ce nom
 * - sinon → son numéro converti en chaîne dans tampon
 */
static const char *_nom_etat(const __automate_state__ *as, int e,
                              char *tampon, size_t taille)
{
    if (as->_noms_etats_ != NULL && as->_noms_etats_[e] != NULL)
        return as->_noms_etats_[e];
    snprintf(tampon, taille, "%d", e);
    return tampon;
}

void afficher_automate(const __automate_state__ *as)
{
    if (as == NULL)
    {
        printf("(automate NULL)\n");
        return;
    }

    int nb_sym   = as->_nombre_symboles_;
    int nb_etats = as->_nombre_etats_;

    // ── En-tête informatif ────────────────────────────────────────────────────
    printf("Alphabet   : { ");
    for (int s = 0; s < nb_sym; s++)
        printf("%c%s", 'a' + s, s < nb_sym - 1 ? ", " : " ");
    printf("}\n");

    // pour les états, utiliser les noms si disponibles
    char tmp[64];
    printf("États      : { ");
    for (int e = 0; e < nb_etats; e++)
        printf("%s%s", _nom_etat(as, e, tmp, sizeof(tmp)),
               e < nb_etats - 1 ? ", " : " ");
    printf("}\n");

    printf("Initiaux   : { ");
    for (uint8_t i = 0; i < as->_etat_initiaux_._etats_; i++)
    {
        uint8_t e = _DA_GET(uint8_t, as->_etat_initiaux_._etat_numéros_, i);
        printf("%s%s", _nom_etat(as, e, tmp, sizeof(tmp)),
               i < as->_etat_initiaux_._etats_ - 1 ? ", " : " ");
    }
    printf("}\n");

    printf("Terminaux  : { ");
    for (uint8_t i = 0; i < as->_etat_finaux_._etats_; i++)
    {
        uint8_t e = _DA_GET(uint8_t, as->_etat_finaux_._etat_numéros_, i);
        printf("%s%s", _nom_etat(as, e, tmp, sizeof(tmp)),
               i < as->_etat_finaux_._etats_ - 1 ? ", " : " ");
    }
    printf("}\n");

    printf("Transitions: %d\n\n", as->_nombre_transaction_);

    // ── Calcul des largeurs de colonnes ───────────────────────────────────────
    char tampon[256];

    // largeur colonne état : max sur tous les noms + 3 (marqueur "ES ")
    int larg_etat = 5;
    for (int e = 0; e < nb_etats; e++)
    {
        int len = (int)strlen(_nom_etat(as, e, tmp, sizeof(tmp))) + 3;
        if (len > larg_etat) larg_etat = len;
    }

    // largeur par colonne symbole
    int *larg_col = (int *) calloc(nb_sym, sizeof(int));
    if (larg_col == NULL) { fprintf(stderr, "Mémoire insuffisante\n"); return; }
    for (int s = 0; s < nb_sym; s++) larg_col[s] = 1;

    for (int e = 0; e < nb_etats; e++)
        for (int s = 0; s < nb_sym; s++)
        {
            _cellule_transitions(as, (uint8_t)e, 'a' + s, tampon, sizeof(tampon));
            // les noms dans les cellules utilisent les noms d'états si dispo
            if (as->_noms_etats_ != NULL)
            {
                // reconstruire la cellule avec noms
                char cellule[256] = "";
                size_t nb_tr = _DA_LENGTH(as->_transitions_);
                char sym = (char)('a' + s);
                for (size_t k = 0; k < nb_tr; k++)
                {
                    __automate_transition__ t =
                        _DA_GET(__automate_transition__, as->_transitions_, k);
                    if (t.etat_depart == (uint8_t)e && (char)t.symbole == sym)
                    {
                        if (cellule[0] != '\0')
                            strncat(cellule, ",",
                                    sizeof(cellule) - strlen(cellule) - 1);
                        strncat(cellule,
                                _nom_etat(as, t.etat_arrive, tmp, sizeof(tmp)),
                                sizeof(cellule) - strlen(cellule) - 1);
                    }
                }
                if (cellule[0] == '\0') strncpy(cellule, "--", sizeof(cellule)-1);
                int len = (int)strlen(cellule);
                if (len > larg_col[s]) larg_col[s] = len;
            }
            else
            {
                int len = (int)strlen(tampon);
                if (len > larg_col[s]) larg_col[s] = len;
            }
        }

    // ── En-tête de la table ───────────────────────────────────────────────────
    printf("%*s", larg_etat, "");
    for (int s = 0; s < nb_sym; s++)
        printf(" | %*c", larg_col[s], 'a' + s);
    printf("\n");

    for (int i = 0; i < larg_etat; i++) putchar('-');
    for (int s = 0; s < nb_sym; s++)
    {
        printf("-+-");
        for (int i = 0; i < larg_col[s]; i++) putchar('-');
    }
    putchar('\n');

    // ── Lignes de transitions ─────────────────────────────────────────────────
    for (int e = 0; e < nb_etats; e++)
    {
        int est_initial = _groupe_contient(&as->_etat_initiaux_, (uint8_t)e);
        int est_final   = _groupe_contient(&as->_etat_finaux_,   (uint8_t)e);

        char marqueur[4] = "  ";
        if (est_initial && est_final) { marqueur[0] = 'E'; marqueur[1] = 'S'; }
        else if (est_initial)         { marqueur[0] = 'E'; marqueur[1] = ' '; }
        else if (est_final)           { marqueur[0] = 'S'; marqueur[1] = ' '; }

        snprintf(tampon, sizeof(tampon), "%s %s",
                 marqueur, _nom_etat(as, e, tmp, sizeof(tmp)));
        printf("%*s", larg_etat, tampon);

        for (int s = 0; s < nb_sym; s++)
        {
            char cellule[256];
            if (as->_noms_etats_ != NULL)
            {
                cellule[0] = '\0';
                size_t nb_tr = _DA_LENGTH(as->_transitions_);
                char sym = (char)('a' + s);
                for (size_t k = 0; k < nb_tr; k++)
                {
                    __automate_transition__ t =
                        _DA_GET(__automate_transition__, as->_transitions_, k);
                    if (t.etat_depart == (uint8_t)e && (char)t.symbole == sym)
                    {
                        if (cellule[0] != '\0')
                            strncat(cellule, ",",
                                    sizeof(cellule) - strlen(cellule) - 1);
                        strncat(cellule,
                                _nom_etat(as, t.etat_arrive, tmp, sizeof(tmp)),
                                sizeof(cellule) - strlen(cellule) - 1);
                    }
                }
                if (cellule[0] == '\0') strncpy(cellule, "--", sizeof(cellule)-1);
            }
            else
            {
                _cellule_transitions(as, (uint8_t)e, 'a' + s,
                                     cellule, sizeof(cellule));
            }
            printf(" | %*s", larg_col[s], cellule);
        }
        putchar('\n');
    }

    free(larg_col);
    printf("\n");
}
