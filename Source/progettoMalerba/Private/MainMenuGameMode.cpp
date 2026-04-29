/**
 * @file MainMenuGameMode.cpp
 * @author Pietro Malerba
 * @brief Implementazione del GameMode dedicato alla schermata di menu
 * @details In questo file configuro il controller usato nella scena del menu principale.
 */

#include "MainMenuGameMode.h"
#include "MyPlayerController.h"

/**
 * @brief Costruttore del GameMode del menu
 * @details Cerca il Blueprint del controller del menu e lo assegna come PlayerControllerClass per questa modalità di gioco.
 */
AMainMenuGameMode::AMainMenuGameMode(){
    static ConstructorHelpers::FClassFinder<APlayerController> PCClass(TEXT("/Game/BP_MyPlayerController.BP_MyPlayerController"));
    
    if (PCClass.Class != nullptr){
        PlayerControllerClass = PCClass.Class;
    }
}
