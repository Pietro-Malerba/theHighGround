/**
 * @file MyGameMode.h
 * @author Pietro Malerba
 * @brief Dichiarazione del GameMode principale del match
 * @details In questo file dichiaro le regole core del match: generazione mappa, FSM turni, movimento e vittoria.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TowerActor.h"
#include "MyGameMode.generated.h"

// Dichiaro in avanti le classi usate nel GameMode.
class ACellActor;
class ATowerActor;
class ABaseUnit;

// Traccio i personaggi già piazzati per impedire duplicazioni durante il placement.
USTRUCT(BlueprintType)
struct FPlacementTracker{
    GENERATED_BODY()

    UPROPERTY()
    bool bJediPlaced = false;

    UPROPERTY()
    bool bRebelPlaced = false;

    UPROPERTY()
    bool bSithPlaced = false;

    UPROPERTY()
    bool bStormtrooperPlaced = false;
};

USTRUCT()
struct FTeamTurnActionFlags{
    GENERATED_BODY()

    UPROPERTY()
    bool HasSniperMoved = false;

    UPROPERTY()
    bool HasSniperAttacked = false;

    UPROPERTY()
    bool HasBrawlerMoved = false;

    UPROPERTY()
    bool HasBrawlerAttacked = false;

    UPROPERTY()
    bool bSniperInterruptedVoluntarily = false;

    UPROPERTY()
    bool bBrawlerInterruptedVoluntarily = false;
};

UENUM(BlueprintType)
enum class EPlacementState : uint8{
    CoinToss,
    Human_0,
    AI_0,
    Human_1,
    AI_1,
    InizializzazioneGioco
};

UENUM(BlueprintType)
enum class EPathfindingStrategy : uint8{
    AStar,
    GreedyBestFirst
};

/**
 * @class AMyGameMode
 * @brief GameMode principale del progetto
 * @details Gestisce generazione della griglia, placement, turnazione, movimento, attacco e controllo delle torri.
 */
UCLASS()
class PROGETTOMALERBA_API AMyGameMode : public AGameModeBase{
    GENERATED_BODY()

public:
    AMyGameMode();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category = "Generazione")
    float NoiseScale = 0.12f; // Traccio la scala di rumore per il perlin noise.
    TArray<ATowerActor*> Torri;
    TSubclassOf<ATowerActor> ClasseTorre;

    // Traccio i seed per ottenere una mappa diversa a ogni avvio.
    float SeedX;
    float SeedY;

    void GeneraGriglia();
    
    FPlacementTracker Tracker;
    bool RegisterUnitPlacement(TSubclassOf<ABaseUnit> UnitClass);
    bool CanPlaceUnit(TSubclassOf<ABaseUnit> UnitClass) const;
    bool IsHumanPlacementTurn() const;
    bool IsHumanActionTurn() const;
    bool IsMovementPhase() const;
    void RegisterSpawnedUnit(ABaseUnit* Unit, ACellActor* Cell);
    ABaseUnit* GetUnitAtCell(const FIntPoint& Coord) const;
    ACellActor* GetCellAtGridCoordinate(const FIntPoint& Coord) const;
    void GetReachableMovementCells(const ABaseUnit* Unit, TArray<ACellActor*>& OutCells) const;
    void GetEnemiesInAttackRange(const ABaseUnit* Unit, TArray<ABaseUnit*>& OutEnemies) const;
    bool CanUnitMoveThisTurn(const ABaseUnit* Unit) const;
    bool CanUnitAttackThisTurn(const ABaseUnit* Unit) const;
    void SetPlacementBoardDimmed(bool bEnable);
    bool TryMoveUnit(ABaseUnit* Unit, const FIntPoint& TargetCoord, FString& OutError);
    bool TryAttackUnit(ABaseUnit* Attacker, ABaseUnit* Target, FString& OutError);
    void NotifyHumanMovementResolved(ABaseUnit* MovedUnit);
    void NotifyHumanActionResolved(ABaseUnit* ActingUnit);
    void NotifyBotActionResolved(ABaseUnit* ActingUnit);
    bool CanHumanEndTurn() const;
    bool EndHumanTurn(FString* OutError = nullptr);
    FIntPoint GridCoordinateToLoopCoordinate(const FIntPoint& GridCoord) const;
    FIntPoint LoopCoordinateToGridCoordinate(const FIntPoint& LoopCoord) const;
    
    // Memorizzo l'esito del coin toss che decide l'ordine iniziale dei piazzamenti.
    bool bHumanWonToss = true;
    
    void AdvancePlacementState();
    void ExecuteStateLogic();
    void ExecuteBotMove();
    void RefreshTowerControlStates();
    
    class ACellActor* GetRandomValidBotCell();
    
    UPROPERTY(EditAnywhere, Category = "Setup")
    TSubclassOf<class ABaseUnit> BotUnitClass;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float MovementTau = 0.2f;
    
protected:
    
    EPlacementState CurrentState;
    
    UPROPERTY(EditAnywhere, Category = "Bot")
    TSubclassOf<class ABaseUnit> SithClass;

    UPROPERTY(EditAnywhere, Category = "Bot")
    TSubclassOf<class ABaseUnit> StormtrooperClass;

    bool bHumanBattleTurn = true;
    bool bMovementTurnsInitialized = false;
    bool bHasStartedMovementTurnCycle = false;
    bool bExecutingStateLogic = false; // Guard per prevenire re-entrancy in ExecuteStateLogic
    TSet<ABaseUnit*> HumanUnitsActedThisTurn;
    TSet<ABaseUnit*> HumanUnitsMovedThisTurn;
    TSet<ABaseUnit*> HumanUnitsAttackedThisTurn;
    TSet<ABaseUnit*> BotUnitsActedThisTurn;
    TSet<ABaseUnit*> BotUnitsMovedThisTurn;
    TSet<ABaseUnit*> BotUnitsAttackedThisTurn;
    FTeamTurnActionFlags HumanTurnActionFlags;
    FTeamTurnActionFlags BotTurnActionFlags;
    TSet<FIntPoint> ReservedBotTowerTargets;
    TMap<ABaseUnit*, ATowerActor*> BotUnitTowerTargets;
    
    
private:
    FTimerHandle CoinTossTimerHandle;
    FTimerHandle BotThinkingTimerHandle;
    FTimerHandle NextTurnTimerHandle;
    
    // Punto unico di avanzamento turno interno alla FSM.
    void NextTurn();
    
    // Mantengo fissa la dimensione della mappa per coerenza con coordinate e bilanciamento.
    int32 DimensioneMappa = 25;

    EPathfindingStrategy PathfindingStrategy = EPathfindingStrategy::AStar;
    
    // Mappa autoritativa delle celle spawnate, indicizzata in coordinate loop.
    TMap<FIntPoint, class ACellActor*> MappaCelle;

    // Occupazione corrente delle celle: una sola unità per coordinata.
    TMap<FIntPoint, class ABaseUnit*> UnitByCell;
    
    // Verifico la connettività della mappa per evitare isole non raggiungibili.
    bool IsGridPlayable();
    
    // Esploro la griglia.
    void ExploreGrid(FIntPoint StartCoord, TSet<FIntPoint>& Visited);

    // Pulisco la griglia prima di una nuova generazione.
    void CleanGrid();
    
    // Piazzo le torri.
    void PosizionaTorri();

    // Individuo e piazzo la torre centrale.
    bool PosizionaTorreCentrale(FIntPoint& CoordinataCentrale);

    // Individuo e piazzo le torri laterali.
    bool PosizionaTorriLaterali(FIntPoint CoordinataCentrale);

    // Verifico che la cella rispetti i vincoli di progettazione.
    bool CellaValidaPerTorre(FIntPoint Coordinata);

    ETowerControlState EvaluateTowerControlState(const ATowerActor* Tower) const;

    // Regola base di traversabilità: occupazione, ostacoli e dislivello.
    bool CanTraverseStep(const ABaseUnit* Unit, const FIntPoint& FromGridCoord, const FIntPoint& ToGridCoord, int32& OutStepCost) const;
    bool BuildMovementPath(const ABaseUnit* Unit, const FIntPoint& TargetCoord, TArray<FIntPoint>& OutPath, int32* OutPathCost = nullptr) const;
    void StartUnitMovementAnimation(ABaseUnit* Unit, const TArray<FIntPoint>& Path);
    void AdvanceUnitMovementStep(ABaseUnit* Unit);

    int32 GetMovableUnitCount(bool bForHuman) const;
    void BeginMovementTurn(bool bForHuman);
    void BuildBotTowerAssignments();
    bool IsSniperUnit(const ABaseUnit* Unit) const;
    bool IsBrawlerUnit(const ABaseUnit* Unit) const;
    void ResetTeamTurnActionFlags(bool bForHuman);
    void UpdateTeamTurnActionFlagsForAction(bool bForHuman, const ABaseUnit* Unit, bool bMoved, bool bAttacked);
    bool HasAnyAvailableAttackForRole(bool bForHuman, bool bForSniper) const;
    bool AreTeamTurnActionFlagsComplete(bool bForHuman) const;
    bool HaveAllUnitsMovedThisTurn(bool bForHuman) const;
    bool HaveAnyUnitInterruptedVoluntarily(bool bForHuman) const;
    bool ShouldTurnAutoComplete(bool bForHuman) const;
    FString BuildHumanMovementGuidanceMessage() const;

    bool ExecuteBotObjectiveMovement();

    bool IsAttackValid(const ABaseUnit* Attacker, const ABaseUnit* Target, FString* OutError = nullptr) const;
    bool HasClearRangedLineOfFire(const ABaseUnit* Attacker, const ABaseUnit* Target) const;
    int32 GetEffectiveAttackCost(const ABaseUnit* Attacker, const ABaseUnit* Target) const;
    bool IsSpawnCellReservedForOtherUnit(const ABaseUnit* Unit, const FIntPoint& Coord) const;
    bool TryRespawnUnit(ABaseUnit* Unit, FString* OutError = nullptr);

    int32 HumanMovesRemaining = 0;
    int32 BotMovesRemaining = 0;
    TMap<ABaseUnit*, TArray<FIntPoint>> ActiveMovementPaths;
    TMap<ABaseUnit*, int32> ActiveMovementIndices;
    TMap<ABaseUnit*, FTimerHandle> ActiveMovementTimers;

    bool HasAnyRemainingMoveForTeam(bool bForHuman) const;
    bool HasAnyRemainingActionForTeam(bool bForHuman) const;
    bool HaveAllActiveUnitsActedOnce(bool bForHuman) const;
    
    // Tracking vittoria: controllo consecutivo di 2 torri per 2 turni.
    int32 HumanConsecutiveTowerTurns = 0;
    int32 BotConsecutiveTowerTurns = 0;
    bool bGameEnded = false;
    
    int32 CountPlayerTowersControlled(bool bForHuman) const;
    void CheckGameWinCondition(bool bStartingHumanTurn);
    void AnnounceTowerControlStatus(bool bForHuman, int32 ControlledTowers, int32 ConsecutiveTurns);
    void EndGame(bool bHumanWon);
    
    // Spawno la torre e aggiorno lo stato logico della cella.
    void SpawnTorre(FIntPoint Coordinata);
};

