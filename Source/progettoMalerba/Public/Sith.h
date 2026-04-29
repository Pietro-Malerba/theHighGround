/**
 * @file Sith.h
 * @author Pietro Malerba
 * @brief Dichiarazione della classe ASith
 * @details In questo file dichiaro l'unità melee controllata dall'AI.
 */

#pragma once

#include "CoreMinimal.h"
#include "Brawler.h"
#include "GameFramework/Pawn.h"
#include "Sith.generated.h"

/**
 * @class ASith
 * @brief Unità melee dell'AI
 * @details Deriva da ABrawler e imposta l'appartenenza al team avversario.
 */
UCLASS()
class PROGETTOMALERBA_API ASith : public ABrawler{
	GENERATED_BODY()

public:
	// Inizializzo il flag di appartenenza al team AI.
	ASith();
};
