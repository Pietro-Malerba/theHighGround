/**
 * @file Stormtrooper.h
 * @author Pietro Malerba
 * @brief Dichiarazione della classe AStormtrooper
 * @details In questo file dichiaro l'unità ranged controllata dall'AI.
 */

#pragma once

#include "CoreMinimal.h"
#include "Sniper.h"
#include "GameFramework/Pawn.h"
#include "Stormtrooper.generated.h"

/**
 * @class AStormtrooper
 * @brief Unità ranged dell'AI
 * @details Deriva da ASniper e imposta l'appartenenza al team avversario.
 */
UCLASS()
class PROGETTOMALERBA_API AStormtrooper : public ASniper{
	GENERATED_BODY()

public:
	// Inizializzo il flag di appartenenza al team AI.
	AStormtrooper();
};
