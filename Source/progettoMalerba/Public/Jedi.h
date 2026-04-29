/**
 * @file Jedi.h
 * @author Pietro Malerba
 * @brief Dichiarazione della classe AJedi
 * @details In questo file dichiaro l'unità melee controllata dal player umano.
 */

#pragma once

#include "CoreMinimal.h"
#include "Brawler.h"
#include "GameFramework/Pawn.h"
#include "Jedi.generated.h"

/**
 * @class AJedi
 * @brief Unità melee del player umano
 * @details Deriva da ABrawler e imposta l'appartenenza al team umano.
 */
UCLASS()
class PROGETTOMALERBA_API AJedi : public ABrawler{
	GENERATED_BODY()

public:
	// Inizializzo il flag di appartenenza al team umano.
	AJedi();
};
