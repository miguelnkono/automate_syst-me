#include "automate_io.h"

#include <stdlib.h>
#include <stdio.h>

__automate_transition__* automate_transition_create(uint8_t etat_depart, short symbole, uint8_t etat_arrive)
{
    __automate_transition__ *as = (__automate_transition__ *) malloc( sizeof(__automate_transition__) );
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
