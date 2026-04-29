/**
 * @file MainMenuGameMode.h
 * @author Pietro Malerba
 * @brief Dichiarazione del GameMode dedicato alla schermata di menu
 * @details In questo file dichiaro il GameMode usato esclusivamente per la schermata principale.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

/**
 * @class AMainMenuGameMode
 * @brief GameMode della schermata di menu
 * @details Imposta il controller usato nella scena del menu principale.
 */
UCLASS()
class PROGETTOMALERBA_API AMainMenuGameMode : public AGameModeBase{
    GENERATED_BODY()

public:
    AMainMenuGameMode();
};
