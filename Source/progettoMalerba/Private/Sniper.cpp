/**
 * @file Sniper.cpp
 * @author Pietro Malerba
 * @brief Implementazione della classe ASniper
 * @details In questo file implemento ASniper, la base astratta per le unità a distanza.
 */

#include "Sniper.h"

/**
 * @brief Costruttore di ASniper
 * @details Imposta le statistiche del profilo sniper secondo le specifiche del ramo ranged.
 */
ASniper::ASniper(){
    Health = StaticHealth;
    MaxMovement = StaticMaxMovement;
    AttackRange = StaticAttackRange;
    MinDamage = StaticMinDamage;
    MaxDamage = StaticMaxDamage;
}
