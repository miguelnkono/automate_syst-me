#include "automate_io.h"
#include "data_structure.h"
#include "pipeline.h"

#include <stdio.h>
#include <string.h>

#define NOM_FICHIER_MAX 256

// Affichage du statut d'une étape
static void _afficher_statut_etape(const char *nom, __etape_pipeline__ *etape)
{
    mtx_lock(&etape->_mtx_);
    __etape_statut__ s = etape->_statut_;
    mtx_unlock(&etape->_mtx_);

    const char *label;
    switch (s)
    {
    case ETAPE_EN_ATTENTE:
        label = "[ en attente ]";
        break;
    case ETAPE_EN_COURS:
        label = "[ en cours...  ]";
        break;
    case ETAPE_PRETE:
        label = "[ prêt         ]";
        break;
    case ETAPE_IGNOREE:
        label = "[ ignorée      ]";
        break;
    case ETAPE_ERREUR:
        label = "[ erreur       ]";
        break;
    default:
        label = "[ ?            ]";
        break;
    }
    printf("  %-22s %s\n", nom, label);
}

// Menu de traitement d'un automate
static void _menu_automate(const __automate_state__ *af, __pipeline__ *p)
{
    char saisie[16];

    while (1)
    {
        printf("\n--- Opérations disponibles ---\n");
        _afficher_statut_etape("1. Standardisation", &p->_etape_standardisation_);
        _afficher_statut_etape("2. Déterminisation", &p->_etape_determinisation_);
        _afficher_statut_etape("3. Minimisation", &p->_etape_minimisation_);
        _afficher_statut_etape("4. Complémentaire", &p->_etape_complementaire_);
        printf("  0. Retour (choisir un autre automate)\n");
        printf("> ");

        if (fgets(saisie, sizeof(saisie), stdin) == NULL)
            break;
        saisie[strcspn(saisie, "\n")] = '\0';

        if (strcmp(saisie, "0") == 0)
            break;

        __etape_pipeline__ *etape = NULL;
        const char *label = NULL;

        if (strcmp(saisie, "1") == 0)
        {
            etape = &p->_etape_standardisation_;
            label = "Standardisation";
        }
        else if (strcmp(saisie, "2") == 0)
        {
            etape = &p->_etape_determinisation_;
            label = "Déterminisation";
        }
        else if (strcmp(saisie, "3") == 0)
        {
            etape = &p->_etape_minimisation_;
            label = "Minimisation";
        }
        else if (strcmp(saisie, "4") == 0)
        {
            etape = &p->_etape_complementaire_;
            label = "Complémentaire";
        }
        else
        {
            printf("Choix invalide.\n");
            continue;
        }

        // Attendre le résultat si pas encore prêt
        mtx_lock(&etape->_mtx_);
        int deja_pret = (etape->_statut_ != ETAPE_EN_ATTENTE &&
                         etape->_statut_ != ETAPE_EN_COURS);
        mtx_unlock(&etape->_mtx_);

        if (!deja_pret)
            printf("Calcul en cours, patientez...\n");

        __etape_statut__ statut = pipeline_attendre_etape(etape);

        // Afficher le résultat
        printf("\n=== %s ===\n", label);
        switch (statut)
        {
        case ETAPE_PRETE:
            // afficher le journal de calcul s'il existe (ex: partitions minimisation)
            if (etape->_resultat_->_log_ != NULL)
                printf("%s", etape->_resultat_->_log_);
            afficher_automate(etape->_resultat_);
            break;
        case ETAPE_IGNOREE:
            printf("Opération non nécessaire (automate déjà dans cet état).\n");
            break;
        case ETAPE_ERREUR:
            printf("Une erreur s'est produite lors du calcul.\n");
            break;
        default:
            break;
        }

        (void)af;
    }
}

// Boucle principale

int main(void)
{
    char choix[NOM_FICHIER_MAX];

    while (1)
    {
        printf("Quel automate voulez-vous utiliser ?\n");
        printf("(numéro ou nom de fichier, 'fin' pour quitter) > ");

        if (fgets(choix, sizeof(choix), stdin) == NULL)
            break;
        choix[strcspn(choix, "\n")] = '\0';

        if (strcmp(choix, "fin") == 0)
            break;

        // construire le nom de fichier
        char nom_fichier[NOM_FICHIER_MAX];
        if (strchr(choix, '.') == NULL)
            snprintf(nom_fichier, sizeof(nom_fichier) - 1, "%s.txt", choix);
        else
            strncpy(nom_fichier, choix, sizeof(nom_fichier) - 1);

        printf("\nChargement de : %s\n\n", nom_fichier);

        // Lecture
        __automate_state__ *af = lire_automate_sur_fichier(nom_fichier);
        if (af == NULL)
        {
            printf("Impossible de charger l'automate. Vérifiez le nom du fichier.\n\n");
            continue;
        }

        // Affichage immédiat
        afficher_automate(af);

        // Les résultats sont aussi stockés dans le pipeline pour guider les threads.
        __automate_tests__ tests = automate_tester(af);
        afficher_tests(&tests);

        // Les threads démarrent ici pendant que l'utilisateur lit l'affichage.
        __pipeline__ *p = pipeline_creer(af);
        if (p == NULL)
        {
            printf("Impossible de lancer le pipeline de traitement.\n\n");
            automate_state_destroy(af);
            continue;
        }

        // Menu interactif (async : l'utilisateur n'attend pas les threads)
        _menu_automate(af, p);

        // Nettoyage : join + libération
        pipeline_detruire(p);
        automate_state_destroy(af);
    }

    printf("\nAu revoir !\n");
    return 0;
}
