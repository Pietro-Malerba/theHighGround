/**
 * @file MyPlayerController.h
 * @author Pietro Malerba
 * @brief Dichiarazione del controller del player
 * @details In questo file dichiaro il controller che gestisce input, UI e interazioni sulla griglia.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MyPlayerController.generated.h"

// Dichiaro in avanti i tipi gameplay usati dal controller.
class ABaseUnit;
class ACellActor;
class ATowerActor;
class UProgressBar;
class USlider;
class UScrollBox;
class UButton;
class UImage;
class UTextBlock;

/**
 * @class AMyPlayerController
 * @brief Controller principale del player
 * @details Gestisce input, menu, HUD, selezione delle unità e interazione con la griglia.
 */
UCLASS()
class PROGETTOMALERBA_API AMyPlayerController : public APlayerController{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<class UUserWidget> MenuWidgetClass;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<class UUserWidget> NewPlacementWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<class UUserWidget> GameOverWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
    FName GameplayMapName = FName(TEXT("MappaPiatta"));

    // Traccio l'istanza attiva del widget di gioco per aggiornarla a runtime.
    UPROPERTY()
    class UUserWidget* CurrentPlacementWidget = nullptr;

    // Traccio il widget del menu per leggere lo slider Perlin.
    UPROPERTY()
    class UUserWidget* CurrentMenuWidget = nullptr;

    // Traccio il widget di fine partita.
    UPROPERTY()
    class UUserWidget* CurrentGameOverWidget = nullptr;

    // Mostro il widget di piazzamento quando entro nella mappa di gioco.
    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowPlacementUI();

    // Mostro il widget di fine partita.
    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowGameOverScreen(bool bHumanWon);

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void StartNewGameFromMenu();
    
    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void SpawnUnit(ACellActor* TargetCell);
    
    UFUNCTION(BlueprintCallable, Category = "Placement")
    void SelectUnitForPlacement(TSubclassOf<ABaseUnit> UnitClass);
    
    // Aggiorno il banner di stato; posso invocarlo anche dai Blueprint.
    UFUNCTION(BlueprintCallable, Category = "UI")
    void UpdateBanner(const FString& Message, FLinearColor Color = FLinearColor::White, float Duration = 0.0f);
    
    UFUNCTION(BlueprintCallable, Category = "UI")
    void ClearBanner();

    // Aggiorno stato/visibilità del bottone fine turno.
    void RefreshTurnButtonState();

    // Registro le azioni di partita nella ScrollBox del widget.
    void LogSpawn(ABaseUnit* Unit);
    void LogMoveAction(class ABaseUnit* Actor, FIntPoint From, FIntPoint To);
    void LogAttackAction(class ABaseUnit* Attacker, class ABaseUnit* Target, int32 Damage, bool bKilled);
    void LogAction(class ABaseUnit* Actor, class ABaseUnit* Target, int32 Damage, bool bIsCounter);
    void LogEndTurnAction();
    void LogTowerControlStatus(bool bForHuman, int32 ControlledTowers, int32 ConsecutiveTurns);

    // Evidenzio le celle raggiungibili dall'unità selezionata.
    void ShowValidMoves(ABaseUnit* Unit);
    void ShowAttackTargets(ABaseUnit* Unit);

    // Espongo questa API agli attori della griglia per orchestrare selezione e movimento.
    void HandleUnitSelection(ABaseUnit* ClickedUnit);
    void HandleCellClick(ACellActor* ClickedCell);
    void SelectUnit(ABaseUnit* Unit);
    bool HasSelectedUnitForMovement() const;
    ABaseUnit* GetSelectedUnitForMovement() const;

    // Aggiorno le health bar del widget in base alle unità attualmente in partita.
    void RefreshUnitHealthBars();

    // Aggiorno le icone del semaforo torri nel widget di gioco.
    void RefreshTowerStatusIndicators();

    // Funzione per convertire coordinate in "formato scacchiera" (es. A1)
    FString ConvertGridToChess(FIntPoint Coord);

    // Funzione per aggiungere la riga al widget
    void AddEntryToScrollBox(const FString& Message, FLinearColor Color);
    
private:
    UPROPERTY()
    TSubclassOf<ABaseUnit> ActiveUnitClass;

    UPROPERTY()
    ABaseUnit* SelectedUnitForMovement = nullptr;

    // Memorizzo le celle evidenziate per poterle ripulire in modo consistente.
    UPROPERTY()
    TArray<ACellActor*> HighlightedCells; 

    UPROPERTY()
    TArray<ACellActor*> HighlightedAttackCells;

    // Cache delle progress bar del widget placement (isVariable nel blueprint).
    UPROPERTY()
    UProgressBar* ProgressBarSith = nullptr;

    UPROPERTY()
    UProgressBar* ProgressBarStormtrooper = nullptr;

    UPROPERTY()
    UProgressBar* ProgressBarJedi = nullptr;

    UPROPERTY()
    UProgressBar* ProgressBarRebel = nullptr;

    // Cache dei text block HP nel formato corrente/totale (es. 13/40).
    UPROPERTY()
    UTextBlock* TextHPSith = nullptr;

    UPROPERTY()
    UTextBlock* TextHPStormtrooper = nullptr;

    UPROPERTY()
    UTextBlock* TextHPRebel = nullptr;

    UPROPERTY()
    UTextBlock* TextHPJedi = nullptr;

    // Cache della ScrollBox usata per il tracking mosse.
    UPROPERTY()
    UScrollBox* MoveLogScrollBox = nullptr;

    UPROPERTY()
    UButton* TurnButton = nullptr;

    // Risolvo i puntatori alle progress bar dal widget corrente.
    void CacheHealthBarWidgets();

    // Restituisco il massimo HP per tipo unità.
    int32 GetMaxHealthForClass(TSubclassOf<ABaseUnit> UnitClass) const;
    FString GetUnitCodeForLog(const ABaseUnit* Unit) const;
    FString GridCoordToBoardLabel(const FIntPoint& Coord) const;
    void CacheMoveLogWidget();
    void CacheTurnButton();
    void AppendMoveLogLine(const FString& LogLine, const FLinearColor& TextColor = FLinearColor::White);

    float ReadPerlinSliderValue() const;
    float MapPerlinSliderToNoiseScale(float SliderValue) const;
    bool ReadCheckboxValueByName(const FName& WidgetName, bool bDefaultValue = false) const;
    bool ReadPathfindingSelectionFromMenu(bool& bOutUseAStar, bool& bOutUseGreedyBestFirst) const;
    UTextBlock* GetMenuWarningTextBlock() const;

    // Ripristino lo stato visivo delle celle evidenziate.
    void ClearMovementHighlights();

protected:
    virtual void BeginPlay() override;

    // Definisco il tipo di HUD configurabile da Editor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<class UUserWidget> HUDWidgetClass;

    // Traccio l'istanza runtime dell'HUD.
    UPROPERTY()
    class UUserWidget* HUDWidgetInstance;
    
    bool CanPlaceUnit(TSubclassOf<ABaseUnit> UnitClass);
    
    // Gestisco il timer usato per i messaggi temporanei del banner.
    FTimerHandle TimerHandle_ErrorReset;

    // Gestisco il fade-in temporizzato del warning nel menu.
    FTimerHandle TimerHandle_MenuWarningFade;

    // Timer periodico per tenere sincronizzate le health bar con il gameplay.
    FTimerHandle TimerHandle_HealthBarRefresh;

    // Ripristino il messaggio di stato persistente dopo un errore temporaneo.
    void ResetBannerStatus();
    void ShowMenuWarning(const FString& Message, float Duration = 3.0f);
    void UpdateMenuWarningFade();

    UFUNCTION()
    void HandleTurnButtonClicked();

    UFUNCTION()
    void OnGameOverMenuClicked();

    UFUNCTION()
    void OnGameOverNewGameClicked();

    // Memorizzo l'ultimo messaggio informativo (bianco) da poter ripristinare.
    FString LastStatusMessage;

    float MenuWarningFadeElapsed = 0.0f;
    float MenuWarningFadeDuration = 3.0f;

    // Cache dell'ultimo rendering per evitare rewrite inutili nello stesso stato/frame logico.
    FString LastRenderedBannerMessage;
    FLinearColor LastRenderedBannerColor = FLinearColor::Transparent;
};
