/**
 * @file Brawler.h
 * @author Pietro Malerba
 * @brief Dichiarazione della classe ABrawler
 * @details In questo file dichiaro la base astratta per le unità melee.
 */

#pragma once

#include "CoreMinimal.h"
#include "BaseUnit.h"
#include "GameFramework/Pawn.h"
#include "Brawler.generated.h"

/**
 * @class ABrawler
 * @brief Base astratta per le unità melee
 * @details Definisce il profilo statistico condiviso da tutte le unità da mischia.
 */
UCLASS(Abstract)
class PROGETTOMALERBA_API ABrawler : public ABaseUnit{
	GENERATED_BODY()

public:
	// Attributi statici autorevoli del profilo brawler.
	static constexpr int32 StaticHealth = 40;
	static constexpr int32 StaticMaxMovement = 6;
	static constexpr int32 StaticAttackRange = 1;
	static constexpr int32 StaticMinDamage = 1;
	static constexpr int32 StaticMaxDamage = 6;

	// Inizializzo le statistiche base del ramo melee.
	ABrawler();
};
