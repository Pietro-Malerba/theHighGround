/**
 * @file Rebel.h
 * @author Pietro Malerba
 * @brief Dichiarazione della classe ARebel
 * @details In questo file dichiaro l'unità ranged controllata dal player umano.
 */

#pragma once

#include "CoreMinimal.h"
#include "Sniper.h"
#include "GameFramework/Pawn.h"
#include "Rebel.generated.h"

/**
 * @class ARebel
 * @brief Unità ranged del player umano
 * @details Deriva da ASniper e imposta l'appartenenza al team umano.
 */
UCLASS()
class PROGETTOMALERBA_API ARebel : public ASniper{
	GENERATED_BODY()

public:
	// Inizializzo il flag di appartenenza al team umano.
	ARebel();
};
