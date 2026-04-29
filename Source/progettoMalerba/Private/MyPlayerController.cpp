/**
 * @file MyPlayerController.cpp
 * @author Pietro Malerba
 * @brief Implementazione del controller del player
 * @details In questo file implemento il controller umano: input, widget, banner, selezione unità e interazione con la griglia durante placement e movimento.
 */

#include "MyPlayerController.h"
#include "MyGameMode.h"
#include "CellActor.h"
#include "BaseUnit.h"
#include "TowerActor.h"
#include "Brawler.h"
#include "Jedi.h"
#include "Rebel.h"
#include "Sniper.h"
#include "Sith.h"
#include "Stormtrooper.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/SlateWrapperTypes.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "PerlinNoiseSettingsSubsystem.h"

void AMyPlayerController::BeginPlay(){
    Super::BeginPlay();

    // Reset esplicito dello stato input per evitare riferimenti stale tra sessioni/mappe.
    ActiveUnitClass = nullptr;
    SelectedUnitForMovement = nullptr;
    HighlightedCells.Empty();
    HighlightedAttackCells.Empty();

    FString MapName = GetWorld()->GetMapName();

    // Nel menu abilito esclusivamente interazione UI.
    if (MapName.Contains("MainMenuMap")){
        bShowMouseCursor = true;
        bEnableClickEvents = true;
        bEnableMouseOverEvents = true;

        if (MenuWidgetClass){
            CurrentMenuWidget = CreateWidget<UUserWidget>(this, MenuWidgetClass);
            UUserWidget* MenuWidget = CurrentMenuWidget;
            if (MenuWidget){
                MenuWidget->AddToViewport();
                FInputModeUIOnly UIOnly;
                UIOnly.SetWidgetToFocus(MenuWidget->TakeWidget());
                SetInputMode(UIOnly);
            }
        }
    }
    // Nella mappa di gioco abilito input misto UI + mondo.
    else{
        // Inizio configurando la camera.
        TArray<AActor*> FoundActors;
        UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("MiaCamera"), FoundActors);
        if (FoundActors.Num() > 0){
            this->SetViewTargetWithBlend(FoundActors[0], 0.0f);
        }
        
        // Poi configuro widget e input.
        bShowMouseCursor = true;
        bEnableClickEvents = true;
        bEnableMouseOverEvents = true;

        if (!IsValid(CurrentPlacementWidget) && NewPlacementWidgetClass){
                // Mantengo un riferimento persistente per aggiornare il banner in seguito.
            CurrentPlacementWidget = CreateWidget<UUserWidget>(this, NewPlacementWidgetClass);
            if (CurrentPlacementWidget){
                CurrentPlacementWidget->AddToViewport();
                
                FInputModeGameAndUI InputMode;
                InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                InputMode.SetHideCursorDuringCapture(false);
                
                SetInputMode(InputMode);

                // Imposto un messaggio iniziale neutro prima che la FSM aggiorni il banner.
                UpdateBanner(TEXT("lancio moneta in corso..."), FLinearColor::White);

                CacheHealthBarWidgets();
                CacheMoveLogWidget();
                CacheTurnButton();
                RefreshUnitHealthBars();
                RefreshTurnButtonState();
                if (!GetWorldTimerManager().IsTimerActive(TimerHandle_HealthBarRefresh)){
                    GetWorldTimerManager().SetTimer(TimerHandle_HealthBarRefresh, this, &AMyPlayerController::RefreshUnitHealthBars, 0.8f, true);
                }
            }
        }
        else if (IsValid(CurrentPlacementWidget)){
            // Se il widget è già presente (es. creato da ShowPlacementUI), riallineo solo cache/timer.
            CacheHealthBarWidgets();
            CacheMoveLogWidget();
            CacheTurnButton();
            RefreshUnitHealthBars();
            RefreshTurnButtonState();
            if (!GetWorldTimerManager().IsTimerActive(TimerHandle_HealthBarRefresh)){
                GetWorldTimerManager().SetTimer(TimerHandle_HealthBarRefresh, this, &AMyPlayerController::RefreshUnitHealthBars, 0.8f, true);
            }
        }
    }
}

/**
 * @brief Mostra l'HUD di piazzamento e gioco
 * @details Crea il widget di piazzamento se non esiste già e sincronizza cache, barra HP, log e bottone turno.
 */
void AMyPlayerController::ShowPlacementUI(){
    if (CurrentPlacementWidget){
        return;
    }

    if (NewPlacementWidgetClass){
        CurrentPlacementWidget = CreateWidget<UUserWidget>(this, NewPlacementWidgetClass);
        if (CurrentPlacementWidget){
            CurrentPlacementWidget->AddToViewport();
            
            // Permetto l'interazione sia con la UI che con la griglia.
            FInputModeGameAndUI InputMode;
            InputMode.SetWidgetToFocus(CurrentPlacementWidget->TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            SetInputMode(InputMode);
            
            bShowMouseCursor = true;

            CacheHealthBarWidgets();
            CacheMoveLogWidget();
            CacheTurnButton();
            RefreshUnitHealthBars();
            RefreshTurnButtonState();
            if (!GetWorldTimerManager().IsTimerActive(TimerHandle_HealthBarRefresh)){
                GetWorldTimerManager().SetTimer(TimerHandle_HealthBarRefresh, this, &AMyPlayerController::RefreshUnitHealthBars, 0.8f, true);
            }
        }
    }
}

/**
 * @brief Mostra la schermata di fine partita
 * @param bHumanWon Indica se il player umano ha vinto
 * @details Rimuove l'HUD di gioco, ferma i timer di aggiornamento e presenta il widget di game over con i pulsanti di navigazione.
 */
void AMyPlayerController::ShowGameOverScreen(bool bHumanWon){
    // Nascondo il widget di gioco attivo
    if (CurrentPlacementWidget){
        CurrentPlacementWidget->RemoveFromParent();
        CurrentPlacementWidget = nullptr;
    }

    ProgressBarSith = nullptr;
    ProgressBarStormtrooper = nullptr;
    ProgressBarJedi = nullptr;
    ProgressBarRebel = nullptr;
    TextHPSith = nullptr;
    TextHPStormtrooper = nullptr;
    TextHPJedi = nullptr;
    TextHPRebel = nullptr;
    MoveLogScrollBox = nullptr;
    TurnButton = nullptr;

    // Pulisco il timer di refresh
    if (GetWorldTimerManager().IsTimerActive(TimerHandle_HealthBarRefresh)){
        GetWorldTimerManager().ClearTimer(TimerHandle_HealthBarRefresh);
    }

    // Mostro il widget di fine partita
    if (GameOverWidgetClass){
        CurrentGameOverWidget = CreateWidget<UUserWidget>(this, GameOverWidgetClass);
        if (CurrentGameOverWidget){
            CurrentGameOverWidget->AddToViewport();

            // Imposto il testo di risultato
            if (UTextBlock* ResultText = Cast<UTextBlock>(CurrentGameOverWidget->GetWidgetFromName(TEXT("RISULTATO")))){
                const FText ResultMessage = bHumanWon ? FText::FromString(TEXT("HAI VINTO")) : FText::FromString(TEXT("HAI PERSO"));
                ResultText->SetText(ResultMessage);
            }

            // Configuro i pulsanti
            if (UButton* BtnMenu = Cast<UButton>(CurrentGameOverWidget->GetWidgetFromName(TEXT("Btn_Menu")))){
                BtnMenu->OnClicked.AddDynamic(this, &AMyPlayerController::OnGameOverMenuClicked);
            }

            if (UButton* BtnNewGame = Cast<UButton>(CurrentGameOverWidget->GetWidgetFromName(TEXT("Btn_NewGame")))){
                BtnNewGame->OnClicked.AddDynamic(this, &AMyPlayerController::OnGameOverNewGameClicked);
            }

            // Permetto l'interazione con la UI
            FInputModeUIOnly InputMode;
            InputMode.SetWidgetToFocus(CurrentGameOverWidget->TakeWidget());
            SetInputMode(InputMode);
            bShowMouseCursor = true;
        }
    }
}

/**
 * @brief Legge il valore corrente dello slider Perlin dal menu
 * @return Valore dello slider normalizzato tra 0 e 1
 * @details Usa un fallback sicuro quando il widget del menu non è valido o lo slider non è disponibile.
 */
float AMyPlayerController::ReadPerlinSliderValue() const{
    // In alcuni flussi il widget menu può risultare stale/pending kill: fallback sicuro.
    if (!IsValid(CurrentMenuWidget)){
        return 0.5f;
    }

    UWidget* SliderWidget = CurrentMenuWidget->GetWidgetFromName(TEXT("Perlin_Slider"));
    if (!IsValid(SliderWidget)){
        return 0.5f;
    }

    if (USlider* PerlinSlider = Cast<USlider>(SliderWidget)){
        const float SliderValue = FMath::Clamp(PerlinSlider->GetValue(), 0.0f, 1.0f);
        return SliderValue;
    }

    return 0.5f;
}

/**
 * @brief Converte il valore dello slider nella scala di rumore del Perlin
 * @param SliderValue Valore di input normalizzato tra 0 e 1
 * @return Scala di rumore da applicare alla generazione della mappa
 * @details Mappa l'intervallo dello slider in una curva a due tratti per ottenere terreni più piatti o più frastagliati.
 */
float AMyPlayerController::MapPerlinSliderToNoiseScale(float SliderValue) const{
    const float ClampedValue = FMath::Clamp(SliderValue, 0.0f, 1.0f);
    const float FlatNoiseScale = 0.01f;
    const float DefaultNoiseScale = 0.12f;
    const float RoughNoiseScale = 0.30f;

    if (ClampedValue <= 0.5f){
        const float Alpha = ClampedValue / 0.5f;
        return FMath::Lerp(FlatNoiseScale, DefaultNoiseScale, Alpha);
    }

    const float Alpha = (ClampedValue - 0.5f) / 0.5f;
    return FMath::Lerp(DefaultNoiseScale, RoughNoiseScale, Alpha);
}

/**
 * @brief Legge il valore di una checkbox dal menu
 * @param WidgetName Nome del widget da leggere
 * @param bDefaultValue Valore di fallback se il widget non è disponibile
 * @return Stato della checkbox oppure il fallback se il widget non è valido
 */
bool AMyPlayerController::ReadCheckboxValueByName(const FName& WidgetName, bool bDefaultValue) const{
    if (!IsValid(CurrentMenuWidget)){
        return bDefaultValue;
    }

    UWidget* CheckboxWidget = CurrentMenuWidget->GetWidgetFromName(WidgetName);
    if (!IsValid(CheckboxWidget)){
        return bDefaultValue;
    }

    if (const UCheckBox* Checkbox = Cast<UCheckBox>(CheckboxWidget)){
        return Checkbox->IsChecked();
    }

    return bDefaultValue;
}

/**
 * @brief Legge la selezione del pathfinding dal menu
 * @param bOutUseAStar Output per la selezione di A*
 * @param bOutUseGreedyBestFirst Output per la selezione di Greedy Best-First
 * @return true se almeno un algoritmo è selezionato
 */
bool AMyPlayerController::ReadPathfindingSelectionFromMenu(bool& bOutUseAStar, bool& bOutUseGreedyBestFirst) const{
    bOutUseAStar = ReadCheckboxValueByName(TEXT("CB_AStar"), true);
    bOutUseGreedyBestFirst = ReadCheckboxValueByName(TEXT("CB_Heuristic"), false);
    return bOutUseAStar || bOutUseGreedyBestFirst;
}

/**
 * @brief Recupera il text block di warning del menu
 * @return Puntatore al text block warning oppure nullptr se il menu non è valido
 */
UTextBlock* AMyPlayerController::GetMenuWarningTextBlock() const{
    if (!IsValid(CurrentMenuWidget) || !MenuWidgetClass || !CurrentMenuWidget->IsA(MenuWidgetClass)){
        return nullptr;
    }

    return Cast<UTextBlock>(CurrentMenuWidget->GetWidgetFromName(TEXT("warning_AI")));
}

/**
 * @brief Mostra un warning temporaneo nel menu
 * @param Message Messaggio da mostrare
 * @param Duration Durata del warning prima del ripristino
 * @details Aggiorna il text block del warning e programma il fade/reset del testo.
 */
void AMyPlayerController::ShowMenuWarning(const FString& Message, float Duration){
    UTextBlock* WarningText = GetMenuWarningTextBlock();
    if (!IsValid(WarningText)){
        return;
    }

    GetWorldTimerManager().ClearTimer(TimerHandle_MenuWarningFade);

    MenuWarningFadeElapsed = 0.0f;
    MenuWarningFadeDuration = FMath::Max(Duration, 0.01f);

    WarningText->SetText(FText::FromString(Message));
    WarningText->SetVisibility(ESlateVisibility::Visible);
    WarningText->SetRenderOpacity(0.0f);

    constexpr float FadeStep = 0.05f;
    GetWorldTimerManager().SetTimer(TimerHandle_MenuWarningFade, this, &AMyPlayerController::UpdateMenuWarningFade, FadeStep, true);
}

/**
 * @brief Aggiorna il fade del warning del menu
 * @details Gestisce la dissolvenza progressiva del messaggio di warning mostrato nel menu principale.
 */
void AMyPlayerController::UpdateMenuWarningFade(){
    UTextBlock* WarningText = GetMenuWarningTextBlock();
    if (!IsValid(WarningText)){
        return;
    }

    constexpr float FadeStep = 0.05f;
    MenuWarningFadeElapsed = FMath::Min(MenuWarningFadeElapsed + FadeStep, MenuWarningFadeDuration);

    const float Alpha = FMath::Clamp(MenuWarningFadeElapsed / MenuWarningFadeDuration, 0.0f, 1.0f);
    WarningText->SetRenderOpacity(Alpha);

    if (MenuWarningFadeElapsed >= MenuWarningFadeDuration){
        GetWorldTimerManager().ClearTimer(TimerHandle_MenuWarningFade);
        WarningText->SetRenderOpacity(1.0f);
    }
}

/**
 * @brief Avvia una nuova partita dal menu
 * @details Legge le impostazioni del menu, le salva nel subsystem persistente e apre la mappa di gioco configurata.
 */
void AMyPlayerController::StartNewGameFromMenu(){
    const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
    if (!CurrentLevelName.Equals(TEXT("MainMenuMap"), ESearchCase::IgnoreCase)){
        // Se chiamata fuori dal menu principale (es. binding BP sul widget di fine partita),
        // instrado sempre sulla logica corretta di nuova partita post-match.
        OnGameOverNewGameClicked();
        return;
    }

    const float SliderValue = ReadPerlinSliderValue();
    const float NoiseScale = MapPerlinSliderToNoiseScale(SliderValue);

    bool bUseAStar = true;
    bool bUseGreedyBestFirst = false;
    const bool bHasAtLeastOneAlgorithm = ReadPathfindingSelectionFromMenu(bUseAStar, bUseGreedyBestFirst);
    if (!bHasAtLeastOneAlgorithm){
        ShowMenuWarning(TEXT("Seleziona almeno un algoritmo (A* o Heuristic)"), 3.0f);
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance()){
        if (UPerlinNoiseSettingsSubsystem* PerlinSubsystem = GameInstance->GetSubsystem<UPerlinNoiseSettingsSubsystem>()){
            PerlinSubsystem->SetPendingNoiseScale(NoiseScale);
            PerlinSubsystem->SetPathfindingSelection(bUseAStar, bUseGreedyBestFirst);
        }
    }

    const FName TargetLevel = GameplayMapName.IsNone() ? FName(TEXT("MappaPiatta")) : GameplayMapName;
    UGameplayStatics::OpenLevel(this, TargetLevel);
}

/**
 * @brief Spawna un'unità sulla cella selezionata
 * @param TargetCell Cella su cui effettuare lo spawn
 * @details Gestisce placement, validazione della cella, selezione di turno e avanzamento della FSM di piazzamento o di combattimento.
 */
void AMyPlayerController::SpawnUnit(ACellActor* TargetCell){
    if (!TargetCell) return;

    AMyGameMode* MyGM = Cast<AMyGameMode>(GetWorld()->GetAuthGameMode());
    if (!MyGM){
        MyGM = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));
    }

    if (!MyGM){
        UE_LOG(LogTemp, Error, TEXT("SpawnUnit: GameMode non disponibile."));
        return;
    }

    if (!MyGM->IsHumanActionTurn()){
        UpdateBanner(TEXT("ATTENDI: TURNO DELL'AVVERSARIO"), FLinearColor::Red, 2.0f);
        return;
    }

    // In fase movimento applico il flusso a due click: selezione e poi destinazione.
    if (MyGM->IsMovementPhase()){
        ABaseUnit* ClickedUnit = MyGM->GetUnitAtCell(TargetCell->GridCoordinate);

        if (!SelectedUnitForMovement){
            if (ClickedUnit && !ClickedUnit->IsAIDriven){
                const bool bCanMove = MyGM->CanUnitMoveThisTurn(ClickedUnit);
                const bool bCanAttack = MyGM->CanUnitAttackThisTurn(ClickedUnit);

                if (!bCanMove && !bCanAttack){
                    UpdateBanner(TEXT("QUESTA UNITÀ NON PUÒ AGIRE IN QUESTO TURNO"), FLinearColor::Red, 2.0f);
                    return;
                }

                SelectedUnitForMovement = ClickedUnit;
                if (bCanMove){
                    ShowValidMoves(SelectedUnitForMovement);
                }
                if (bCanAttack){
                    ShowAttackTargets(SelectedUnitForMovement);
                }
                UpdateBanner(TEXT("UNITÀ SELEZIONATA: SCEGLI DESTINAZIONE"), FLinearColor::White);
            }
            else{
                UpdateBanner(TEXT("SELEZIONA PRIMA UNA TUA UNITÀ"), FLinearColor::Red, 2.0f);
            }
            return;
        }

        if (ClickedUnit == SelectedUnitForMovement){
            ClearMovementHighlights();
            SelectedUnitForMovement = nullptr;
            UpdateBanner(TEXT("SELEZIONE ANNULLATA"), FLinearColor::White);
            return;
        }

        if (ClickedUnit && !ClickedUnit->IsAIDriven){
            const bool bCanMove = MyGM->CanUnitMoveThisTurn(ClickedUnit);
            const bool bCanAttack = MyGM->CanUnitAttackThisTurn(ClickedUnit);

            if (!bCanMove && !bCanAttack){
                UpdateBanner(TEXT("QUESTA UNITÀ NON PUÒ AGIRE IN QUESTO TURNO"), FLinearColor::Red, 2.0f);
                return;
            }

            SelectedUnitForMovement = ClickedUnit;
            if (bCanMove){
                ShowValidMoves(SelectedUnitForMovement);
            }
            if (bCanAttack){
                ShowAttackTargets(SelectedUnitForMovement);
            }
            UpdateBanner(TEXT("UNITÀ CAMBIATA: SCEGLI DESTINAZIONE"), FLinearColor::White);
            return;
        }

        FString MoveError;
        if (MyGM->TryMoveUnit(SelectedUnitForMovement, TargetCell->GridCoordinate, MoveError)){
            ABaseUnit* MovedUnit = SelectedUnitForMovement;
            ClearMovementHighlights();
            UpdateBanner(TEXT("MOVIMENTO ESEGUITO"), FLinearColor::White);
            SelectedUnitForMovement = nullptr;
            MyGM->NotifyHumanMovementResolved(MovedUnit);
        }
        else{
            const FString BannerError = MoveError.IsEmpty() ? TEXT("MOVIMENTO NON VALIDO") : MoveError;
            UpdateBanner(BannerError, FLinearColor::Red, 2.0f);
        }

        return;
    }

    if (!ActiveUnitClass) return;

    // Verifico la calpestabilità: l'altezza 0 resta acqua o ostacolo.
    if (!TargetCell || !ActiveUnitClass) return;
    
    if (!TargetCell->IsWalkable()){
        if (GEngine){
            UpdateBanner(TEXT("ERRORE: CELLA NON AGIBILE!"), FLinearColor::Red, 3.0f);
        }
        return;
    }

    // Applico il vincolo di spawn: il player umano piazza solo nelle prime tre righe.
    if (TargetCell->GridCoordinate.X >= 3){
        UpdateBanner(TEXT("ERRORE: PUOI PIAZZARE SOLO NELLE PRIME 3 RIGHE!"), FLinearColor::Red, 3.0f);
        return;
    }

    // Se i vincoli passano, istanzio l'unità e la registro nel modello logico.
    FActorSpawnParameters SpawnParams;
    FVector SpawnLocation = TargetCell->GetActorLocation() + FVector(0.f, 0.f, 50.f);
    ABaseUnit* SpawnedUnit = GetWorld()->SpawnActor<ABaseUnit>(ActiveUnitClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

    if (SpawnedUnit){
        if (!MyGM){
            MyGM = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));
        }

        if (MyGM){
            if (!MyGM->RegisterUnitPlacement(ActiveUnitClass)){
                SpawnedUnit->Destroy();
                UpdateBanner(TEXT("ERRORE: UNITÀ GIÀ PIAZZATA!"), FLinearColor::Red, 3.0f);
                ActiveUnitClass = nullptr;
                return;
            }

            // Comunico a GameMode la posizione della nuova unità in griglia.
            MyGM->RegisterSpawnedUnit(SpawnedUnit, TargetCell);
            LogSpawn(SpawnedUnit);
            RefreshUnitHealthBars();
            
            // Avanzo la FSM.
            MyGM->AdvancePlacementState();
        }
        else{
            UE_LOG(LogTemp, Error, TEXT("SpawnUnit: GameMode non disponibile, impossibile avanzare lo stato di piazzamento."));
        }

        ActiveUnitClass = nullptr;
    }
}

/**
 * @brief Verifica se il tipo di unità selezionato può essere piazzato
 * @param UnitClass Classe dell'unità da piazzare
 * @return true se la classe è ancora disponibile, false se è già stata piazzata
 */
bool AMyPlayerController::CanPlaceUnit(TSubclassOf<ABaseUnit> UnitClass){
    AMyGameMode* MyGM = Cast<AMyGameMode>(GetWorld()->GetAuthGameMode());
    if (!MyGM){
        MyGM = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));
    }

    if (MyGM){
        // Chiedo al GameMode se l'unità è già stata piazzata.
        return MyGM->CanPlaceUnit(UnitClass);
    }

    // Evito falsi negativi in bootstrap: se il GameMode non è pronto, non blocco la UI.
    UE_LOG(LogTemp, Warning, TEXT("CanPlaceUnit: GameMode non disponibile, consento temporaneamente la selezione."));
    return true;
}

/**
 * @brief Seleziona il tipo di unità da piazzare
 * @param UnitClass Classe dell'unità scelta dal player
 * @details Valida il turno, controlla il team e aggiorna il banner con lo stato di selezione.
 */
void AMyPlayerController::SelectUnitForPlacement(TSubclassOf<ABaseUnit> UnitClass){
    if (!UnitClass) return;

    AMyGameMode* MyGM = Cast<AMyGameMode>(GetWorld()->GetAuthGameMode());
    if (!MyGM){
        MyGM = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));
    }

    if (MyGM && !MyGM->IsHumanActionTurn()){
        UpdateBanner(TEXT("ATTENDI: TURNO DELL'AVVERSARIO"), FLinearColor::Red, 2.0f);
        ActiveUnitClass = nullptr;
        return;
    }

    if (MyGM && MyGM->IsMovementPhase()){
        UpdateBanner(TEXT("FASE MOVIMENTO: CLICCA SULLA GRIGLIA"), FLinearColor::Red, 2.0f);
        ActiveUnitClass = nullptr;
        return;
    }

    // Controllo il team di appartenenza.
    if (UnitClass->IsChildOf(AJedi::StaticClass()) || UnitClass->IsChildOf(ARebel::StaticClass())){
        // Controllo se l'unità è già stata piazzata.
        if (!CanPlaceUnit(UnitClass)){
            // Comunico l'errore all'utente nel banner, senza interrompere il flusso input.
            UpdateBanner(TEXT("ERRORE: UNITÀ GIÀ PIAZZATA!"), FLinearColor::Red, 3.0f);
            ActiveUnitClass = nullptr;
            return;
        }

        // I messaggi bianchi rappresentano lo stato persistente del narratore di turno.
        UpdateBanner(TEXT("personaggio selezionato: scegli una cella valida per il piazzamento"), FLinearColor::White);
        ActiveUnitClass = UnitClass;
    }
    else{
        UpdateBanner(TEXT("ERRORE: TEAM AVVERSARIO!"), FLinearColor::Red, 3.0f);
        ActiveUnitClass = nullptr;
    }
}

/**
 * @brief Aggiorna il banner di stato del gameplay
 * @param Message Messaggio da visualizzare
 * @param Color Colore del testo
 * @param Duration Durata dell'eventuale messaggio di errore temporaneo
 * @details Gestisce prefissi contestuali di turno, cache del testo renderizzato e timer di reset del banner.
 */
void AMyPlayerController::UpdateBanner(const FString& Message, FLinearColor Color, float Duration){
    if (!IsValid(CurrentPlacementWidget) || !IsValid(GetWorld())) return;

    UTextBlock* Banner = Cast<UTextBlock>(CurrentPlacementWidget->GetWidgetFromName(TEXT("BannerText")));
    if (!IsValid(Banner)){
        return;
    }

    // Copia locale difensiva: evita dipendenze da eventuali riferimenti esterni/temporanei.
    const FString SafeMessage = Message;
    FString BannerMessage = SafeMessage;
    if (AMyGameMode* GM = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this))){
        if (GM->IsMovementPhase()){
            const FString LowerMessage = SafeMessage.ToLower();
            const bool bAlreadyPrefixed = LowerMessage.StartsWith(TEXT("turno hp")) || LowerMessage.StartsWith(TEXT("turno ai"));
            if (!bAlreadyPrefixed){
                const bool bHumanTurn = GM->IsHumanActionTurn();
                BannerMessage = FString(TEXT("turno "));
                BannerMessage += bHumanTurn ? TEXT("hp") : TEXT("ai");
                BannerMessage += TEXT(": ");
                BannerMessage += SafeMessage;
            }
        }
    }

    // Ignoro rewrite identici: evita overlap di stringhe nel banner
    if (Color == FLinearColor::White
        && BannerMessage.Equals(LastRenderedBannerMessage, ESearchCase::CaseSensitive)
        && LastRenderedBannerColor == FLinearColor::White
        && Banner->GetVisibility() == ESlateVisibility::Visible){
        return;
    }

    // Mantengo un solo timer attivo per evitare pending callback su stato vecchio.
    GetWorldTimerManager().ClearTimer(TimerHandle_ErrorReset);

    // Aggiornamento diretto del contenuto visuale del banner.
    Banner->SetText(FText::FromString(BannerMessage));
    Banner->SetColorAndOpacity(FSlateColor(Color));
    Banner->SetVisibility(ESlateVisibility::Visible);

    LastRenderedBannerMessage = BannerMessage;
    LastRenderedBannerColor = Color;

    // Salvo il messaggio informativo come baseline da ripristinare dopo gli errori.
    if (Color == FLinearColor::White){
        LastStatusMessage = BannerMessage;
    }
    // Un messaggio di errore rosso viene mostrato a tempo e poi ripristinato.
    else if (Duration > 0.0f && !LastStatusMessage.IsEmpty()){
        GetWorldTimerManager().SetTimer(TimerHandle_ErrorReset, this, &AMyPlayerController::ResetBannerStatus, Duration, false);
    }

    RefreshTurnButtonState();
}

/**
 * @brief Ripristina il messaggio di stato persistente
 * @details Richiama UpdateBanner con l'ultimo messaggio bianco salvato come stato base dell'interfaccia.
 */
void AMyPlayerController::ResetBannerStatus(){
    if (LastStatusMessage.IsEmpty()){
        return;
    }

    // Ripristino il messaggio bianco precedente.
    const FString CachedStatus = LastStatusMessage;
    UpdateBanner(CachedStatus, FLinearColor::White);
}

/**
 * @brief Pulisce il banner di stato
 * @details Nasconde il testo del banner e ne azzera il contenuto visivo.
 */
void AMyPlayerController::ClearBanner(){
    if (!IsValid(CurrentPlacementWidget)) return;

    UTextBlock* Banner = Cast<UTextBlock>(CurrentPlacementWidget->GetWidgetFromName(TEXT("BannerText")));
    if (IsValid(Banner)){
        Banner->SetVisibility(ESlateVisibility::Collapsed);
        Banner->SetText(FText::GetEmpty());
        Banner->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Banner->ForceLayoutPrepass();
    }

    LastRenderedBannerMessage.Empty();
    LastRenderedBannerColor = FLinearColor::Transparent;

    GetWorldTimerManager().ClearTimer(TimerHandle_ErrorReset);
    RefreshTurnButtonState();
}

/**
 * @brief Rimuove gli highlight di movimento e attacco dalle celle
 * @details Ripristina lo stato visivo delle celle memorizzate nelle cache di highlight.
 */
void AMyPlayerController::ClearMovementHighlights(){
    for (ACellActor* Cell : HighlightedCells){
        if (Cell){
            Cell->SetHighlight(false);
        }
    }
    HighlightedCells.Empty();

    for (ACellActor* Cell : HighlightedAttackCells){
        if (Cell){
            Cell->SetEnemyHighlight(false);
        }
    }
    HighlightedAttackCells.Empty();
}

/**
 * @brief Evidenzia le celle raggiungibili dall'unità selezionata
 * @param Unit Unità per cui calcolare i movimenti possibili
 * @details Pulisce gli highlight precedenti e marca tutte le celle raggiungibili con il colore di movimento.
 */
void AMyPlayerController::ShowValidMoves(ABaseUnit* Unit){
    if (!Unit) return;

    AMyGameMode* GM = Cast<AMyGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM || !GM->CanUnitMoveThisTurn(Unit)){
        return;
    }

    ClearMovementHighlights();

    TArray<ACellActor*> ReachableCells;
    GM->GetReachableMovementCells(Unit, ReachableCells);

    for (ACellActor* Cell : ReachableCells){
        if (Cell){
            Cell->SetHighlight(true);
            HighlightedCells.Add(Cell);
        }
    }
}

/**
 * @brief Evidenzia i bersagli attaccabili dall'unità selezionata
 * @param Unit Unità attualmente selezionata
 * @details Pulisce gli highlight precedenti e marca le celle dei nemici attaccabili con il colore rosso.
 */
void AMyPlayerController::ShowAttackTargets(ABaseUnit* Unit){
    if (!Unit) return;

    AMyGameMode* GM = Cast<AMyGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM){
        GM = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));
    }

    if (!GM || !GM->CanUnitAttackThisTurn(Unit)) return;

    TArray<ABaseUnit*> AttackableEnemies;
    GM->GetEnemiesInAttackRange(Unit, AttackableEnemies);

    for (ABaseUnit* Enemy : AttackableEnemies){
        if (!Enemy) continue;

        if (ACellActor* EnemyCell = GM->GetCellAtGridCoordinate(Enemy->GridPosition)){
            EnemyCell->SetEnemyHighlight(true);
            HighlightedAttackCells.Add(EnemyCell);
        }
    }
}

/**
 * @brief Gestisce la selezione di una singola unità
 * @param ClickedUnit Unità cliccata dal player
 * @details Permette selezione, deselezione e cambio unità, aggiornando gli highlight in base al tipo di azione disponibile.
 */
void AMyPlayerController::HandleUnitSelection(ABaseUnit* ClickedUnit){
    // Se clicco sulla stessa unità già selezionata, deseleziono tutto.
    if (SelectedUnitForMovement == ClickedUnit){
        ClearMovementHighlights();
        SelectedUnitForMovement = nullptr;
        return;
    }

    // Se l'unità appartiene al giocatore umano (non AI).
    if (ClickedUnit && !ClickedUnit->IsAIDriven){
        AMyGameMode* GM = Cast<AMyGameMode>(GetWorld()->GetAuthGameMode());
        if (!GM){
            GM = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));
        }

        const bool bCanMove = GM && GM->CanUnitMoveThisTurn(ClickedUnit);
        const bool bCanAttack = GM && GM->CanUnitAttackThisTurn(ClickedUnit);

        if (!bCanMove && !bCanAttack){
            UpdateBanner(TEXT("QUESTA UNITÀ HA GIÀ MOSSO IN QUESTO TURNO"), FLinearColor::Red, 2.0f);
            return;
        }

        SelectedUnitForMovement = ClickedUnit;
        if (bCanMove){
            ShowValidMoves(SelectedUnitForMovement);
        }
        if (bCanAttack){
            ShowAttackTargets(SelectedUnitForMovement);
        }
    }
}

/**
 * @brief Gestisce il click su una cella durante la fase di movimento
 * @param ClickedCell Cella cliccata dal player
 * @details Differenzia selezione unità, attacco e movimento in base al contenuto della cella e allo stato del turno.
 */
void AMyPlayerController::HandleCellClick(ACellActor* ClickedCell){
    if (!ClickedCell){
        return;
    }

    AMyGameMode* GM = Cast<AMyGameMode>(GetWorld()->GetAuthGameMode());
/**
 * @brief Seleziona un'unità dal codice esterno
 * @param Unit Unità da selezionare
 * @details Permette ai blueprint o ad altri attori di riusare la stessa logica di selezione del controller.
 */
    if (!GM){
        GM = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));
    }

    if (!GM || !GM->IsMovementPhase()){
        return;
    }

    // In fase movimento, se clicco una nostra unità la (de)seleziono e aggiorno subito gli highlight.
    if (ABaseUnit* ClickedUnit = GM->GetUnitAtCell(ClickedCell->GridCoordinate)){
        if (!ClickedUnit->IsAIDriven){
            HandleUnitSelection(ClickedUnit);
            if (SelectedUnitForMovement){
                UpdateBanner(TEXT("UNITÀ SELEZIONATA: SCEGLI DESTINAZIONE"), FLinearColor::White);
            }
            else{
                UpdateBanner(TEXT("SELEZIONE ANNULLATA"), FLinearColor::White);
            }
            return;
        }
    }

    if (!SelectedUnitForMovement){
        UpdateBanner(TEXT("SELEZIONA PRIMA UNA TUA UNITÀ"), FLinearColor::Red, 2.0f);
        return;
    }

    if (ABaseUnit* ClickedUnit = GM->GetUnitAtCell(ClickedCell->GridCoordinate)){
        if (ClickedUnit->IsAIDriven != SelectedUnitForMovement->IsAIDriven){
            TArray<ABaseUnit*> AttackTargets;
            GM->GetEnemiesInAttackRange(SelectedUnitForMovement, AttackTargets);

            if (!AttackTargets.Contains(ClickedUnit)){
                UpdateBanner(TEXT("BERSAGLIO FUORI RAGGIO"), FLinearColor::Red, 2.0f);
                return;
            }

            FString AttackError;
            if (GM->TryAttackUnit(SelectedUnitForMovement, ClickedUnit, AttackError)){
                ABaseUnit* AttackingUnit = SelectedUnitForMovement;
                ClearMovementHighlights();
                SelectedUnitForMovement = nullptr;
                UpdateBanner(TEXT("ATTACCO ESEGUITO"), FLinearColor::White);
                RefreshUnitHealthBars();
                GM->NotifyHumanActionResolved(AttackingUnit);
            }
            else{
                const FString BannerError = AttackError.IsEmpty() ? TEXT("ATTACCO NON VALIDO") : AttackError;
                UpdateBanner(BannerError, FLinearColor::Red, 2.0f);
            }

            return;
        }
    }

    if (!HighlightedCells.Contains(ClickedCell)){
        UpdateBanner(TEXT("CELLA NON RAGGIUNGIBILE"), FLinearColor::Red, 2.0f);
        return;
    }

    FString MoveError;
    if (GM->TryMoveUnit(SelectedUnitForMovement, ClickedCell->GridCoordinate, MoveError)){
        ABaseUnit* MovedUnit = SelectedUnitForMovement;
        ClearMovementHighlights();
        SelectedUnitForMovement = nullptr;
        UpdateBanner(TEXT("MOVIMENTO ESEGUITO"), FLinearColor::White);
        GM->NotifyHumanActionResolved(MovedUnit);
    }
    else{
        const FString BannerError = MoveError.IsEmpty() ? TEXT("MOVIMENTO NON VALIDO") : MoveError;
        UpdateBanner(BannerError, FLinearColor::Red, 2.0f);
    }
}

    // Espongo un helper pubblico per selezionare un'unità da codice esterno.
void AMyPlayerController::SelectUnit(ABaseUnit* Unit){
    if (Unit && !Unit->IsAIDriven){
        HandleUnitSelection(Unit);
    }
}

/**
 * @brief Indica se esiste un'unità attualmente selezionata
 * @return true se la selezione movimento è valida
 */
bool AMyPlayerController::HasSelectedUnitForMovement() const{
    return IsValid(SelectedUnitForMovement);
}

/**
 * @brief Restituisce l'unità attualmente selezionata
 * @return Puntatore all'unità selezionata oppure nullptr
 */
ABaseUnit* AMyPlayerController::GetSelectedUnitForMovement() const{
    return SelectedUnitForMovement;
}

/**
 * @brief Recupera e cache le progress bar delle unità
 * @details Cerca le progress bar del widget attivo e ne conserva i riferimenti per aggiornare la UI runtime.
 */
void AMyPlayerController::CacheHealthBarWidgets(){
    if (!CurrentPlacementWidget){
        return;
    }

    ProgressBarSith = Cast<UProgressBar>(CurrentPlacementWidget->GetWidgetFromName(TEXT("ProgressBar_Sith")));
    ProgressBarStormtrooper = Cast<UProgressBar>(CurrentPlacementWidget->GetWidgetFromName(TEXT("ProgressBar_Stormtrooper")));
    ProgressBarJedi = Cast<UProgressBar>(CurrentPlacementWidget->GetWidgetFromName(TEXT("ProgressBar_Jedi")));
    ProgressBarRebel = Cast<UProgressBar>(CurrentPlacementWidget->GetWidgetFromName(TEXT("ProgressBar_Rebel")));

    TextHPSith = Cast<UTextBlock>(CurrentPlacementWidget->GetWidgetFromName(TEXT("Text_HP_Sith")));
    TextHPStormtrooper = Cast<UTextBlock>(CurrentPlacementWidget->GetWidgetFromName(TEXT("Text_HP_Stormtrooper")));
    TextHPJedi = Cast<UTextBlock>(CurrentPlacementWidget->GetWidgetFromName(TEXT("Text_HP_Jedi")));
    TextHPRebel = Cast<UTextBlock>(CurrentPlacementWidget->GetWidgetFromName(TEXT("Text_HP_Rebel")));
}

/**
 * @brief Recupera e cache la ScrollBox del log azioni
 * @details Cerca il widget di log nel widget corrente e salva il riferimento per appendere le righe di log.
 */
void AMyPlayerController::CacheMoveLogWidget(){
    if (!CurrentPlacementWidget){
        MoveLogScrollBox = nullptr;
        return;
    }

    // Compatibilità con naming vecchio/nuovo del widget nel blueprint.
    MoveLogScrollBox = Cast<UScrollBox>(CurrentPlacementWidget->GetWidgetFromName(TEXT("ScrollBox")));
    if (!MoveLogScrollBox){
        MoveLogScrollBox = Cast<UScrollBox>(CurrentPlacementWidget->GetWidgetFromName(TEXT("LogScrollBox")));
    }
}

/**
 * @brief Recupera e cache il bottone di fine turno
 * @details Cerca il bottone nel widget di gioco e collega il relativo handler di click.
 */
void AMyPlayerController::CacheTurnButton(){
    if (!CurrentPlacementWidget){
        TurnButton = nullptr;
        return;
    }

    if (IsValid(TurnButton)){
        TurnButton->OnClicked.RemoveAll(this);
    }
    else{
        TurnButton = nullptr;
    }

    TurnButton = Cast<UButton>(CurrentPlacementWidget->GetWidgetFromName(TEXT("Btn_Turn")));
    if (TurnButton){
        TurnButton->OnClicked.RemoveAll(this);
        TurnButton->OnClicked.AddDynamic(this, &AMyPlayerController::HandleTurnButtonClicked);
    }
}

/**
 * @brief Aggiorna stato e visibilità del bottone di fine turno
 * @details Abilita o disabilita il bottone in base alla possibilità reale di chiudere il turno umano.
 */
void AMyPlayerController::RefreshTurnButtonState(){
    if (!IsValid(TurnButton)){
        CacheTurnButton();
    }

    if (!IsValid(TurnButton)){
        return;
    }

    AMyGameMode* GM = Cast<AMyGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM){
        GM = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));
    }

    const bool bCanEndTurn = GM && GM->CanHumanEndTurn();

    // Il bottone non deve sparire: resta sempre visibile e cambia solo stato/opacità.
    TurnButton->SetVisibility(ESlateVisibility::Visible);
    TurnButton->SetIsEnabled(bCanEndTurn);
    TurnButton->SetRenderOpacity(bCanEndTurn ? 1.0f : 0.35f);
}

/**
 * @brief Gestisce il click sul bottone di fine turno
 * @details Richiede al GameMode la chiusura del turno e ripulisce selezione e highlight se l'operazione va a buon fine.
 */
void AMyPlayerController::HandleTurnButtonClicked(){
    AMyGameMode* GM = Cast<AMyGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM){
        GM = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));
    }

    if (!GM){
        UpdateBanner(TEXT("GameMode non disponibile"), FLinearColor::Red, 2.0f);
        return;
    }

    FString EndTurnError;
    if (!GM->EndHumanTurn(&EndTurnError)){
        const FString BannerMessage = EndTurnError.IsEmpty()
            ? TEXT("COMPLETA ALMENO UN'AZIONE PER OGNI UNITÀ")
            : EndTurnError;
        UpdateBanner(BannerMessage, FLinearColor::Red, 2.0f);
        return;
    }

    ClearMovementHighlights();
    SelectedUnitForMovement = nullptr;
    RefreshTurnButtonState();
}

/**
 * @brief Restituisce il codice testuale usato nei log per una unità
 * @param Unit Unità da convertire in codice log
 * @return Codice sintetico della classe o del team dell'unità
 */
FString AMyPlayerController::GetUnitCodeForLog(const ABaseUnit* Unit) const{
    if (!Unit){
        return TEXT("UNK");
    }

    if (Unit->IsA(AJedi::StaticClass())){
        return TEXT("Br_J");
    }

    if (Unit->IsA(ASith::StaticClass())){
        return TEXT("Br_S");
    }

    if (Unit->IsA(ARebel::StaticClass())){
        return TEXT("Sn_R");
    }

    if (Unit->IsA(AStormtrooper::StaticClass())){
        return TEXT("Sn_S");
    }

    return Unit->IsAIDriven ? TEXT("AI") : TEXT("HU");
}

/**
 * @brief Converte una coordinata di griglia nel formato scacchiera
 * @param Coord Coordinata logica da convertire
 * @return Etichetta testuale della cella nel formato lettera+numero
 */
FString AMyPlayerController::GridCoordToBoardLabel(const FIntPoint& Coord) const{
    constexpr int32 BoardSize = 25;
    const int32 ClampedColumn = FMath::Clamp((BoardSize - 1) - Coord.Y, 0, BoardSize - 1);
    const int32 Row = FMath::Clamp(Coord.X, 0, BoardSize - 1);
    const TCHAR Column = static_cast<TCHAR>('A' + ClampedColumn);

    return FString::Printf(TEXT("%c%d"), Column, Row);
}

/**
 * @brief Appende una riga al log delle azioni
 * @param LogLine Testo da aggiungere al log
 * @param TextColor Colore della riga da visualizzare
 * @details Crea un nuovo text block nella ScrollBox e scrolla automaticamente all'ultima riga. Preserva l'opacità della ScrollBox dopo l'aggiunta.
 */
void AMyPlayerController::AppendMoveLogLine(const FString& LogLine, const FLinearColor& TextColor){
    if (!CurrentPlacementWidget){
        return;
    }

    if (!MoveLogScrollBox){
        CacheMoveLogWidget();
    }

    if (!MoveLogScrollBox){
        return;
    }

    const float OriginalOpacity = MoveLogScrollBox->GetRenderOpacity();

    UTextBlock* EntryText = NewObject<UTextBlock>(MoveLogScrollBox);
    if (!EntryText){
        return;
    }

    EntryText->SetText(FText::FromString(LogLine));
    EntryText->SetColorAndOpacity(FSlateColor(TextColor));
    EntryText->SetAutoWrapText(true);
    MoveLogScrollBox->AddChild(EntryText);
    MoveLogScrollBox->SetRenderOpacity(OriginalOpacity);
    MoveLogScrollBox->ScrollToEnd();
}

/**
 * @brief Registra lo spawn di un'unità nel log
 * @param Unit Unità appena piazzata
 */
void AMyPlayerController::LogSpawn(ABaseUnit* Unit){
    if (!Unit){
        return;
    }

    AppendMoveLogLine(FString::Printf(TEXT("%s => (%s)"), *GetUnitCodeForLog(Unit), *GridCoordToBoardLabel(Unit->GridPosition)));
}

/**
 * @brief Registra l'attacco di un'unità nel log
 * @param Attacker Unità attaccante
 * @param Target Bersaglio dell'attacco
 * @param Damage Danno inflitto
 * @param bKilled Indica se il bersaglio è stato eliminato
 */
void AMyPlayerController::LogAttackAction(ABaseUnit* Attacker, ABaseUnit* Target, int32 Damage, bool bKilled) {
    LogAction(Attacker, Target, Damage, false);
}

/**
 * @brief Registra una qualunque azione di combattimento nel log
 * @param Actor Unità che compie l'azione
 * @param Target Bersaglio dell'azione
 * @param Damage Danno inflitto
 * @param bIsCounter Indica se si tratta di un contrattacco
 */
void AMyPlayerController::LogAction(ABaseUnit* Actor, ABaseUnit* Target, int32 Damage, bool bIsCounter) {
    if (!Actor || !Target) return;

    FString P_ID = Actor->IsAIDriven ? TEXT("AI") : TEXT("HP");
    FString U_ID = Actor->IsA(ASniper::StaticClass()) ? TEXT("S") : TEXT("B");
    FString Cell = ConvertGridToChess(Target->GridPosition);
    
    FString LogStr;
    if (bIsCounter) {
        LogStr = FString::Printf(TEXT("%s: %s CTRC %s [%d]"), *P_ID, *U_ID, *Cell, Damage);
    } else {
        LogStr = FString::Printf(TEXT("%s: %s %s [%d]"), *P_ID, *U_ID, *Cell, Damage);
    }

    AddEntryToScrollBox(LogStr, FLinearColor::White);
}

/**
 * @brief Registra la chiusura di un turno nel log
 */
void AMyPlayerController::LogEndTurnAction(){
    AppendMoveLogLine(TEXT("---FINE-TURNO---"));
}

/**
 * @brief Registra lo stato di controllo delle torri nel log
 * @param bForHuman Indica se il team è quello umano
 * @param ControlledTowers Numero di torri controllate dal team
 * @param ConsecutiveTurns Numero di turni consecutivi di controllo
 */
void AMyPlayerController::LogTowerControlStatus(bool bForHuman, int32 ControlledTowers, int32 ConsecutiveTurns){
    const FString TeamLabel = bForHuman ? TEXT("HP") : TEXT("AI");
    const FLinearColor TextColor = bForHuman ? FLinearColor(0.0f, 1.0f, 0.0f) : FLinearColor(1.0f, 0.0f, 0.0f);
    const FString Message = FString::Printf(
        TEXT("%s controlla %d torri: %d/2 turni"),
        *TeamLabel,
        ControlledTowers,
        ConsecutiveTurns
    );

    AppendMoveLogLine(Message, TextColor);
}

/**
 * @brief Gestisce il click sul pulsante menu della schermata game over
 * @details Torna al menu principale chiudendo la schermata di fine partita.
 */
void AMyPlayerController::OnGameOverMenuClicked(){
    // Torno al MainMenuMap e mostro il menu iniziale
    UGameplayStatics::OpenLevel(GetWorld(), FName(TEXT("MainMenuMap")));
}

/**
 * @brief Gestisce il click sul pulsante nuova partita della schermata game over
 * @details Richiama la stessa logica di avvio partita usata dal menu principale.
 */
void AMyPlayerController::OnGameOverNewGameClicked(){
    if (IsValid(CurrentGameOverWidget)){
        CurrentGameOverWidget->RemoveFromParent();
        CurrentGameOverWidget = nullptr;
    }

    // Rigenero la mappa del gioco (stesso livello)
    const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
    UGameplayStatics::OpenLevel(GetWorld(), FName(*CurrentLevelName));
}

/**
 * @brief Restituisce gli HP massimi associati a una classe di unità
 * @param UnitClass Classe dell'unità da valutare
 * @return HP massimi associati alla classe o 1 come fallback
 */
int32 AMyPlayerController::GetMaxHealthForClass(TSubclassOf<ABaseUnit> UnitClass) const{
    if (!UnitClass){
        return 1;
    }

    if (UnitClass->IsChildOf(AJedi::StaticClass())){
        return ABrawler::StaticHealth;
    }

    if (UnitClass->IsChildOf(ARebel::StaticClass())){
        return ASniper::StaticHealth;
    }

    if (UnitClass->IsChildOf(ASith::StaticClass())){
        return ABrawler::StaticHealth;
    }

    if (UnitClass->IsChildOf(AStormtrooper::StaticClass())){
        return ASniper::StaticHealth;
    }

    if (UnitClass->IsChildOf(ABrawler::StaticClass())){
        return 40;
    }

    if (UnitClass->IsChildOf(ASniper::StaticClass())){
        return 20;
    }

    return 1;
}

/**
 * @brief Aggiorna le barre HP delle unità nell'HUD
 * @details Scansiona le unità presenti nel livello e aggiorna le progress bar in base agli HP correnti e massimi.
 */
void AMyPlayerController::RefreshUnitHealthBars(){
    if (!CurrentPlacementWidget){
        return;
    }

    if (!ProgressBarSith || !ProgressBarStormtrooper || !ProgressBarJedi || !ProgressBarRebel
        || !TextHPSith || !TextHPStormtrooper || !TextHPJedi || !TextHPRebel){
        CacheHealthBarWidgets();
    }

    int32 JediHealth = -1;
    int32 JediMaxHealth = -1;
    int32 RebelHealth = -1;
    int32 RebelMaxHealth = -1;
    int32 SithHealth = -1;
    int32 SithMaxHealth = -1;
    int32 StormtrooperHealth = -1;
    int32 StormtrooperMaxHealth = -1;

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseUnit::StaticClass(), FoundActors);

    for (AActor* Actor : FoundActors){
        ABaseUnit* Unit = Cast<ABaseUnit>(Actor);
        if (!Unit){
            continue;
        }

        const FString UnitClassName = Unit->GetClass()->GetName();

        if (Unit->IsA(AJedi::StaticClass()) || UnitClassName.Contains(TEXT("Jedi"))){
            JediHealth = Unit->Health;
            JediMaxHealth = Unit->MaxHealth;
        }
        else if (Unit->IsA(ARebel::StaticClass()) || UnitClassName.Contains(TEXT("Rebel"))){
            RebelHealth = Unit->Health;
            RebelMaxHealth = Unit->MaxHealth;
        }
        else if (Unit->IsA(ASith::StaticClass()) || UnitClassName.Contains(TEXT("Sith"))){
            SithHealth = Unit->Health;
            SithMaxHealth = Unit->MaxHealth;
        }
        else if (Unit->IsA(AStormtrooper::StaticClass()) || UnitClassName.Contains(TEXT("Stormtrooper"))){
            StormtrooperHealth = Unit->Health;
            StormtrooperMaxHealth = Unit->MaxHealth;
        }
    }

    const auto UpdateProgressBar = [this](UProgressBar* ProgressBar, int32 Health, TSubclassOf<ABaseUnit> UnitClass){
        if (!IsValid(ProgressBar) || Health < 0){
            return;
        }

        const float Percent = FMath::Clamp(static_cast<float>(Health) / static_cast<float>(GetMaxHealthForClass(UnitClass)), 0.0f, 1.0f);
        ProgressBar->SetPercent(Percent);
    };

    const auto UpdateHealthText = [this](UTextBlock* HealthText, int32 Health, int32 MaxHealth, TSubclassOf<ABaseUnit> UnitClass){
        if (!IsValid(HealthText)){
            return;
        }

        const int32 SafeMaxHealth = MaxHealth > 0 ? MaxHealth : GetMaxHealthForClass(UnitClass);
        const int32 SafeCurrentHealth = FMath::Max(0, Health);
        HealthText->SetVisibility(ESlateVisibility::Visible);
        HealthText->SetRenderOpacity(1.0f);
        HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), SafeCurrentHealth, SafeMaxHealth)));
    };

    UpdateProgressBar(ProgressBarSith, SithHealth, ASith::StaticClass());
    UpdateProgressBar(ProgressBarStormtrooper, StormtrooperHealth, AStormtrooper::StaticClass());
    UpdateProgressBar(ProgressBarJedi, JediHealth, AJedi::StaticClass());
    UpdateProgressBar(ProgressBarRebel, RebelHealth, ARebel::StaticClass());

    UpdateHealthText(TextHPSith, SithHealth, SithMaxHealth, ASith::StaticClass());
    UpdateHealthText(TextHPStormtrooper, StormtrooperHealth, StormtrooperMaxHealth, AStormtrooper::StaticClass());
    UpdateHealthText(TextHPJedi, JediHealth, JediMaxHealth, AJedi::StaticClass());
    UpdateHealthText(TextHPRebel, RebelHealth, RebelMaxHealth, ARebel::StaticClass());
}

/**
 * @brief Aggiorna gli indicatori visivi delle torri nell'HUD
 * @details Legge lo stato di controllo delle torri dal GameMode e aggiorna le immagini dei tre indicatori UI.
 */
void AMyPlayerController::RefreshTowerStatusIndicators(){
    if (!IsValid(CurrentPlacementWidget)){
        return;
    }

    AMyGameMode* GameMode = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GameMode){
        return;
    }

    TArray<ATowerActor*> Towers = GameMode->Torri;
    Towers.Sort([](const ATowerActor& A, const ATowerActor& B){
        return A.GetActorLocation().X < B.GetActorLocation().X;
    });

    auto LoadTowerTexture = [](const TCHAR* Path) -> UTexture2D* {
        return LoadObject<UTexture2D>(nullptr, Path);
    };

    static UTexture2D* TowerNeutralTexture = LoadTowerTexture(TEXT("/Game/Texture/torri/torre_neutra.torre_neutra"));
    static UTexture2D* TowerContestedTexture = LoadTowerTexture(TEXT("/Game/Texture/torri/torre_contesa.torre_contesa"));
    static UTexture2D* TowerHumanTexture = LoadTowerTexture(TEXT("/Game/Texture/torri/torre_verde.torre_verde"));
    static UTexture2D* TowerAiTexture = LoadTowerTexture(TEXT("/Game/Texture/torri/torre_rossa.torre_rossa"));

    const FName ImageNames[] = { TEXT("torre_sx"), TEXT("torre_c"), TEXT("torre_dx") };

    for (int32 Index = 0; Index < 3; ++Index){
        UImage* TowerImage = Cast<UImage>(CurrentPlacementWidget->GetWidgetFromName(ImageNames[Index]));
        if (!IsValid(TowerImage)){
            continue;
        }

        UTexture2D* TargetTexture = nullptr;
        if (Towers.IsValidIndex(Index) && Towers[Index]){
            switch (Towers[Index]->GetControlState()){
                case ETowerControlState::HUMAN:
                    TargetTexture = TowerHumanTexture;
                    break;
                case ETowerControlState::AI:
                    TargetTexture = TowerAiTexture;
                    break;
                case ETowerControlState::CONTESTED:
                    TargetTexture = TowerContestedTexture;
                    break;
                case ETowerControlState::NEUTRAL:
                default:
                    TargetTexture = TowerNeutralTexture;
                    break;
            }
        }

        if (TargetTexture){
            TowerImage->SetBrushFromTexture(TargetTexture, true);
        }
    }
}

/**
 * @brief Converte una coordinata di griglia nel formato scacchiera legacy
 * @param Coord Coordinata da convertire
 * @return Rappresentazione testuale della coordinata oppure "??" se fuori range
 */
FString AMyPlayerController::ConvertGridToChess(FIntPoint Coord) {
    // Mantengo la funzione legacy ma delego al formatter unico usato da tutto il log.
    return GridCoordToBoardLabel(Coord);
}

/**
 * @brief Registra un movimento nel log
 * @param Actor Unità che si muove
 * @param From Coordinata di partenza
 * @param To Coordinata di arrivo
 */
void AMyPlayerController::LogMoveAction(ABaseUnit* Actor, FIntPoint From, FIntPoint To) {
    if (!Actor){
        return;
    }

    FString P_ID = Actor->IsAIDriven ? TEXT("AI") : TEXT("HP");
    FString U_ID = Actor->IsA(ASniper::StaticClass()) ? TEXT("S") : TEXT("B");
    FString Start = ConvertGridToChess(From);
    FString End = ConvertGridToChess(To);

    FString LogStr = FString::Printf(TEXT("%s: %s %s -> %s"), *P_ID, *U_ID, *Start, *End);
    AddEntryToScrollBox(LogStr, FLinearColor::White);
}

/**
 * @brief Punto unico di scrittura del log azioni
 * @param Message Messaggio da aggiungere al log
 * @param Color Colore della riga
 * @details Permette di convogliare tutte le scritture del log verso la stessa routine di rendering.
 */
void AMyPlayerController::AddEntryToScrollBox(const FString& Message, FLinearColor Color) {
    // Unico punto di scrittura log: evita divergenze tra HUDWidgetInstance e CurrentPlacementWidget.
    AppendMoveLogLine(Message, Color);
}