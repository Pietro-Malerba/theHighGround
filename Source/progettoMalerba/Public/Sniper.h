/**
 * @file Sniper.h
 * @author Pietro Malerba
 * @brief Dichiarazione della classe ASniper
 * @details In questo file dichiaro la base astratta per le unità ranged.
 */

#pragma once

#include "CoreMinimal.h"
#include "BaseUnit.h"
#include "GameFramework/Pawn.h"
#include "Sniper.generated.h"

/**
 * @class ASniper
 * @brief Base astratta per le unità ranged
 * @details Definisce il profilo statistico condiviso da tutte le unità a distanza.
 */
UCLASS(Abstract)
class PROGETTOMALERBA_API ASniper : public ABaseUnit{
	GENERATED_BODY()

public:
	// Attributi statici autorevoli del profilo sniper.
	static constexpr int32 StaticHealth = 20;
	static constexpr int32 StaticMaxMovement = 4;
	static constexpr int32 StaticAttackRange = 10;
	static constexpr int32 StaticMinDamage = 4;
	static constexpr int32 StaticMaxDamage = 8;

	// Inizializzo le statistiche base del ramo ranged.
	ASniper();

    // Estendo qui in futuro la logica di contrattacco comune agli sniper.
};
