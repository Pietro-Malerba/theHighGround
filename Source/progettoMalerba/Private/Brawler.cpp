/**
 * @file Brawler.cpp
 * @author Pietro Malerba
 * @brief Implementazione della classe ABrawler
 * @details In questo file implemento ABrawler, la base astratta per le unità di combattimento corpo a corpo.
 */

// Includo il file header corrispondente.
#include "Brawler.h"

/**
 * @brief Costruttore di ABrawler
 * @details Imposta le statistiche del brawler secondo le specifiche del profilo melee.
 */
ABrawler::ABrawler(){
    Health = StaticHealth;
    MaxMovement = StaticMaxMovement;
    AttackRange = StaticAttackRange;
    MinDamage = StaticMinDamage;
    MaxDamage = StaticMaxDamage;
}
