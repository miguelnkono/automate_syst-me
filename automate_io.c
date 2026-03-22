#include "automate_io.h"
#include "data_structure.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

__automate_transition__ *automate_transition_create(uint8_t etat_depart, short symbole, uint8_t etat_arrive)
{
    __automate_transition__ *as = (__automate_transition__ *)malloc(sizeof(__automate_transition__));
    if (as == NULL)
    {
        fprintf(stderr, "Impossible de créer une transition\n");
        return NULL;
    }

    as->etat_depart = etat_depart;
    as->symbole = symbole;
    as->etat_arrive = etat_arrive;

    return as;
}

void automate_transition_destroy(__automate_transition__ *t)
{
    if (t != NULL)
    {
        free(t);
    }
}

/**
 * Tester si un état donné est présent dans un groupe (initiaux / finaux).
 */
static int _groupe_contient(const __automate_node_info__ *groupe, uint8_t etat)
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
    if (fscanf(f, " %hhu", &as->_nombre_symboles_) != 1)
        goto erreur;

    // ligne 2 : nombre d'états
    if (fscanf(f, " %hhu", &as->_nombre_etats_) != 1)
        goto erreur;

    // ligne 3 : états initiaux
    if (fscanf(f, " %hhu", &as->_etat_initiaux_._etats_) != 1)
        goto erreur;
    for (uint8_t i = 0; i < as->_etat_initiaux_._etats_; i++)
    {
        uint8_t num;
        if (fscanf(f, " %hhu", &num) != 1)
            goto erreur;
        _DA_PUSH(uint8_t, &as->_etat_initiaux_._etat_numéros_, num);
    }

    // ligne 4 : états finaux
    if (fscanf(f, " %hhu", &as->_etat_finaux_._etats_) != 1)
        goto erreur;
    for (uint8_t i = 0; i < as->_etat_finaux_._etats_; i++)
    {
        uint8_t num;
        if (fscanf(f, " %hhu", &num) != 1)
            goto erreur;
        _DA_PUSH(uint8_t, &as->_etat_finaux_._etat_numéros_, num);
    }

    // ligne 5 : nombre de transitions
    if (fscanf(f, " %hhu", &as->_nombre_transaction_) != 1)
        goto erreur;

    // lignes suivantes : transitions "0a1"
    for (uint8_t i = 0; i < as->_nombre_transaction_; i++)
    {
        char ligne[64];
        if (fscanf(f, " %63s", ligne) != 1)
            goto erreur;

        // le symbole est le premier caractère non-chiffre
        size_t len = strlen(ligne);
        size_t pos_sym = 0;
        while (pos_sym < len && (ligne[pos_sym] >= '0' && ligne[pos_sym] <= '9'))
            pos_sym++;

        if (pos_sym == 0 || pos_sym >= len - 1)
            goto erreur;

        // état de départ : chiffres avant le symbole
        char buf_dep[16];
        strncpy(buf_dep, ligne, pos_sym);
        buf_dep[pos_sym] = '\0';
        uint8_t dep = (uint8_t)atoi(buf_dep);

        char sym = ligne[pos_sym];

        // état d'arrivée : chiffres après le symbole
        uint8_t arr = (uint8_t)atoi(ligne + pos_sym + 1);

        __automate_transition__ t;
        t.etat_depart = dep;
        t.symbole = sym;
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

void afficher_automate(const __automate_state__ *as)
{
    if (as == NULL)
    {
        printf("(automate NULL)\n");
        return;
    }

    int nb_sym = as->_nombre_symboles_;
    int nb_etats = as->_nombre_etats_;

    printf("Alphabet   : { ");
    for (int s = 0; s < nb_sym; s++)
        printf("%c%s", 'a' + s, s < nb_sym - 1 ? ", " : " ");
    printf("}\n");

    printf("États      : { ");
    for (int e = 0; e < nb_etats; e++)
        printf("%d%s", e, e < nb_etats - 1 ? ", " : " ");
    printf("}\n");

    printf("Initiaux   : { ");
    for (uint8_t i = 0; i < as->_etat_initiaux_._etats_; i++)
        printf("%d%s",
               _DA_GET(uint8_t, as->_etat_initiaux_._etat_numéros_, i),
               i < as->_etat_initiaux_._etats_ - 1 ? ", " : " ");
    printf("}\n");

    printf("Terminaux  : { ");
    for (uint8_t i = 0; i < as->_etat_finaux_._etats_; i++)
        printf("%d%s",
               _DA_GET(uint8_t, as->_etat_finaux_._etat_numéros_, i),
               i < as->_etat_finaux_._etats_ - 1 ? ", " : " ");
    printf("}\n");

    printf("Transitions: %d\n\n", as->_nombre_transaction_);

    // ── Calcul des largeurs de colonnes ──
    //
    // Chaque cellule peut contenir plusieurs états séparés par des virgules,
    // ou "--". On calcule d'abord le contenu de chaque cellule pour connaître
    // la largeur maximale par colonne.

    // largeur colonne "état" (marqueur E/S + numéro)  ex: "ES 12"
    // on réserve : 2 caractères pour le marqueur + 1 espace + nb chiffres du dernier état
    char tampon[256];
    snprintf(tampon, sizeof(tampon), "%d", nb_etats - 1);
    int larg_etat = 2 + 1 + (int)strlen(tampon); // "ES " + "N"
    if (larg_etat < 5)
        larg_etat = 5; // minimum lisible

    // largeur par colonne symbole : max(1 char header, contenu max des cellules)
    int *larg_col = (int *)calloc(nb_sym, sizeof(int));
    if (larg_col == NULL)
    {
        fprintf(stderr, "Mémoire insuffisante\n");
        return;
    }

    for (int s = 0; s < nb_sym; s++)
        larg_col[s] = 1; // au moins la largeur du header ('a', 'b', ...)

    for (int e = 0; e < nb_etats; e++)
    {
        for (int s = 0; s < nb_sym; s++)
        {
            _cellule_transitions(as, (uint8_t)e, 'a' + s, tampon, sizeof(tampon));
            int len = (int)strlen(tampon);
            if (len > larg_col[s])
                larg_col[s] = len;
        }
    }

    // ── Ligne d'en-tête ──
    printf("%*s", larg_etat, "");
    for (int s = 0; s < nb_sym; s++)
        printf(" | %*c", larg_col[s], 'a' + s);
    printf("\n");

    // séparateur horizontal
    for (int i = 0; i < larg_etat; i++)
        putchar('-');
    for (int s = 0; s < nb_sym; s++)
    {
        printf("-+-");
        for (int i = 0; i < larg_col[s]; i++)
            putchar('-');
    }
    putchar('\n');

    // ── Lignes de transitions ──
    for (int e = 0; e < nb_etats; e++)
    {
        // marqueur E / S
        int est_initial = _groupe_contient(&as->_etat_initiaux_, (uint8_t)e);
        int est_final = _groupe_contient(&as->_etat_finaux_, (uint8_t)e);

        char marqueur[4] = "  ";
        if (est_initial && est_final)
        {
            marqueur[0] = 'E';
            marqueur[1] = 'S';
        }
        else if (est_initial)
        {
            marqueur[0] = 'E';
            marqueur[1] = ' ';
        }
        else if (est_final)
        {
            marqueur[0] = 'S';
            marqueur[1] = ' ';
        }

        // colonne état : marqueur + numéro alignés à droite dans larg_etat
        // on formate "ES 3" en respectant larg_etat
        snprintf(tampon, sizeof(tampon), "%s %d", marqueur, e);
        printf("%*s", larg_etat, tampon);

        // colonnes symboles
        for (int s = 0; s < nb_sym; s++)
        {
            char cellule[256];
            _cellule_transitions(as, (uint8_t)e, 'a' + s, cellule, sizeof(cellule));
            printf(" | %*s", larg_col[s], cellule);
        }
        putchar('\n');
    }

    free(larg_col);
    printf("\n");
}
