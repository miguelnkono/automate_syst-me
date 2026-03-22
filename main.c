#include "automate_io.h"
#include "data_structure.h"

#include <stdio.h>
#include <string.h>

#define NOM_FICHIER_MAX 256

int main(void)
{
    char choix[NOM_FICHIER_MAX];

    while (1)
    {
        printf("Quel automate voulez-vous utiliser ?\n");
        printf("(numéro ou nom de fichier, 'fin' pour quitter) > ");

        if (fgets(choix, sizeof(choix), stdin) == NULL)
            break;

        // supprimer le '\n' terminal
        choix[strcspn(choix, "\n")] = '\0';

        if (strcmp(choix, "fin") == 0)
            break;

        // construire le nom de fichier : si l'utilisateur tape "1" → "1.txt",
        // s'il tape déjà "exemple_1.txt" on l'utilise tel quel
        char nom_fichier[NOM_FICHIER_MAX];
        if (strchr(choix, '.') == NULL)
            snprintf(nom_fichier, sizeof(nom_fichier), "%s.txt", choix);
        else
            strncpy(nom_fichier, choix, sizeof(nom_fichier) - 1);

        printf("\nChargement de : %s\n\n", nom_fichier);

        __automate_state__ *af = lire_automate_sur_fichier(nom_fichier);
        if (af == NULL)
        {
            printf("Impossible de charger l'automate. Vérifiez le nom du fichier.\n\n");
            continue;
        }

        afficher_automate(af);

        automate_state_destroy(af);
    }

    printf("\nAu revoir !\n");
    return 0;
}
