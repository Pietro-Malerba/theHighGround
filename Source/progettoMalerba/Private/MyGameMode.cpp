/**
 * @file MyGameMode.cpp
 * @author Pietro Malerba
 * @brief Implementazione del GameMode principale del match
 * @details In questo file implemento il cuore delle regole di gioco: generazione griglia, piazzamento, turnazione, pathfinding height-aware, animazioni di movimento e AI bot.
 */

#include "MyGameMode.h"
#include "CellActor.h"
#include "MyPlayerController.h"
#include "TowerActor.h"
#include "BaseUnit.h"
#include "Jedi.h"
#include "Sith.h"
#include "Rebel.h"
#include "Stormtrooper.h"
#include "Brawler.h"
#include "Sniper.h"
#include "PerlinNoiseSettingsSubsystem.h"
#include "DrawDebugHelpers.h"

/**
 * @brief Costruttore del GameMode principale
 * @details Carica il controller del player e le classi Blueprint dei bot usate durante la partita.
 */
/**
 * @brief Costruttore del GameMode principale
 * @details Carica il controller del player e le classi Blueprint dei bot usate durante la partita.
 */
AMyGameMode::AMyGameMode(){
    DefaultPawnClass = nullptr;
    PrimaryActorTick.bCanEverTick = true;
    
    // Provo a usare il PlayerController Blueprint; se manca, ricado sulla classe C++.
    static ConstructorHelpers::FClassFinder<APlayerController> PCClass(TEXT("/Game/BluePrint/BP_MyPlayerController"));
    if (PCClass.Succeeded()){
        PlayerControllerClass = PCClass.Class;
    }
    else{
        // Fallback c++ se il blueprint non viene trovato
        PlayerControllerClass = AMyPlayerController::StaticClass();
    }

    // Inizializzo i seed usati dalla generazione procedurale.
    SeedX = FMath::RandRange(0.f, 10000.f);
    SeedY = FMath::RandRange(0.f, 10000.f);
    
    // Carico i blueprint delle unità bot per lo spawn nella fase placement.
    static ConstructorHelpers::FClassFinder<ABaseUnit> SithBP(TEXT("/Game/Blueprint/personaggi/BP_Sith"));
    static ConstructorHelpers::FClassFinder<ABaseUnit> SithBPFallback(TEXT("/Game/BluePrint/personaggi/BP_Sith"));
    if (SithBP.Succeeded()){
        SithClass = SithBP.Class;
    }
    else if (SithBPFallback.Succeeded()){
        SithClass = SithBPFallback.Class;
    }
    else{
        SithClass = ASith::StaticClass();
    }

    static ConstructorHelpers::FClassFinder<ABaseUnit> StormBP(TEXT("/Game/Blueprint/personaggi/BP_Stormtrooper"));
    static ConstructorHelpers::FClassFinder<ABaseUnit> StormBPFallback(TEXT("/Game/BluePrint/personaggi/BP_Stormtrooper"));
    if (StormBP.Succeeded()){
        StormtrooperClass = StormBP.Class;
    }
    else if (StormBPFallback.Succeeded()){
        StormtrooperClass = StormBPFallback.Class;
    }
    else{
        StormtrooperClass = AStormtrooper::StaticClass();
    }
}

/**
 * @brief Avvia una nuova partita
 * @details Genera la mappa, inizializza i tracker e fa partire la macchina a stati del match.
 */
void AMyGameMode::BeginPlay(){
    Super::BeginPlay();

    if (UGameInstance* GameInstance = GetGameInstance()){
        if (UPerlinNoiseSettingsSubsystem* PerlinSubsystem = GameInstance->GetSubsystem<UPerlinNoiseSettingsSubsystem>()){
            NoiseScale = PerlinSubsystem->GetPendingNoiseScale();

            const bool bUseAStar = PerlinSubsystem->GetUseAStar();
            const bool bUseGreedyBestFirst = PerlinSubsystem->GetUseGreedyBestFirst();
            PathfindingStrategy = bUseAStar
                ? EPathfindingStrategy::AStar
                : EPathfindingStrategy::GreedyBestFirst;

            if (!bUseAStar && !bUseGreedyBestFirst){
                PathfindingStrategy = EPathfindingStrategy::AStar;
                UE_LOG(LogTemp, Warning, TEXT("Nessun algoritmo selezionato: fallback su A*."));
            }
        }
    }
    
    // Avvio la generazione procedurale, includendo i controlli di giocabilità.
    GeneraGriglia();
    
    AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController());
    if (PC){
        PC->ShowPlacementUI();
    }
    
    // Eseguo il coin toss iniziale per decidere l'ordine dei piazzamenti.
    bHumanWonToss = FMath::RandBool();
    
    // Reinizializzo tracker e stato turni per una nuova partita.
    Tracker.bJediPlaced = false;
    Tracker.bRebelPlaced = false;
    Tracker.bSithPlaced = false;
    Tracker.bStormtrooperPlaced = false;
    UnitByCell.Empty();
    bHumanBattleTurn = true;
    bMovementTurnsInitialized = false;
    bHasStartedMovementTurnCycle = false;
    HumanMovesRemaining = 0;
    BotMovesRemaining = 0;
    HumanUnitsActedThisTurn.Reset();
    HumanUnitsMovedThisTurn.Reset();
    HumanUnitsAttackedThisTurn.Reset();
    BotUnitsActedThisTurn.Reset();
    BotUnitsMovedThisTurn.Reset();
    BotUnitsAttackedThisTurn.Reset();

    // Avvio la macchina a stati dal coin toss.
    CurrentState = EPlacementState::CoinToss;
    ExecuteStateLogic();
}

/**
 * @brief Tick del GameMode principale
 * @param DeltaSeconds Tempo trascorso dall'ultimo frame
 * @details Aggiorna i controlli di torre, gli indicatori UI e i contorni debug di movimento e attacco.
 */
void AMyGameMode::Tick(float DeltaSeconds){
    Super::Tick(DeltaSeconds);

    const float AttackLineThickness = 3.5f;
    const float TowerLineThickness = 5.5f;
    const float AttackContourZ = 10.0f;
    const float TowerContourZ = 18.0f;

    const FColor HumanAttackColor(57, 255, 20);     // Umano (verde)
    const FColor AIAttackColor(255, 49, 49);        // AI (rosso)
    const FColor NeutralTowerColor(0, 220, 255);    // Neutro (azzurro)
    const FColor ContestedTowerColor(255, 221, 64); // Contesa (ocra)

    RefreshTowerControlStates();

    if (AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController())){
        PC->RefreshTowerStatusIndicators();
    }

    auto DrawCellsContour = [this](const TSet<FIntPoint>& Cells, const FColor& Color, float ZOffset, float LineThickness){
        if (Cells.Num() == 0){
            return;
        }

        int32 MinX = TNumericLimits<int32>::Max();
        int32 MinY = TNumericLimits<int32>::Max();
        int32 MaxX = TNumericLimits<int32>::Min();
        int32 MaxY = TNumericLimits<int32>::Min();

        for (const FIntPoint& Coord : Cells){
            MinX = FMath::Min(MinX, Coord.X);
            MinY = FMath::Min(MinY, Coord.Y);
            MaxX = FMath::Max(MaxX, Coord.X);
            MaxY = FMath::Max(MaxY, Coord.Y);
        }

        const int32 FloodMinX = MinX - 1;
        const int32 FloodMinY = MinY - 1;
        const int32 FloodMaxX = MaxX + 1;
        const int32 FloodMaxY = MaxY + 1;

        auto IsInsideFloodBounds = [FloodMinX, FloodMinY, FloodMaxX, FloodMaxY](const FIntPoint& Coord){
            return Coord.X >= FloodMinX && Coord.X <= FloodMaxX && Coord.Y >= FloodMinY && Coord.Y <= FloodMaxY;
        };

        TSet<FIntPoint> OutsideEmptyCells;
        TArray<FIntPoint> Queue;
        Queue.Reserve((FloodMaxX - FloodMinX + 1) * (FloodMaxY - FloodMinY + 1));

        const FIntPoint FloodStart(FloodMinX, FloodMinY);
        OutsideEmptyCells.Add(FloodStart);
        Queue.Add(FloodStart);

        int32 Head = 0;
        while (Head < Queue.Num()){
            const FIntPoint Current = Queue[Head++];
            const TArray<FIntPoint> Neighbors = {
                Current + FIntPoint(1, 0), Current + FIntPoint(-1, 0),
                Current + FIntPoint(0, 1), Current + FIntPoint(0, -1)
            };

            for (const FIntPoint& Neighbor : Neighbors){
                if (!IsInsideFloodBounds(Neighbor)){
                    continue;
                }

                if (Cells.Contains(Neighbor) || OutsideEmptyCells.Contains(Neighbor)){
                    continue;
                }

                OutsideEmptyCells.Add(Neighbor);
                Queue.Add(Neighbor);
            }
        }

        for (const FIntPoint& Coord : Cells){
            const ACellActor* Cell = GetCellAtGridCoordinate(Coord);
            if (!Cell){
                continue;
            }

            const FVector BoxExtent = Cell->MeshComp ? Cell->MeshComp->Bounds.BoxExtent : FVector(50.f, 50.f, 1.f);
            const float HalfX = FMath::Max(1.0f, BoxExtent.X - 0.75f);
            const float HalfY = FMath::Max(1.0f, BoxExtent.Y - 0.75f);
            const FVector Center = Cell->GetActorLocation() + FVector(0.f, 0.f, ZOffset);
            const FVector TopLeft(Center.X - HalfX, Center.Y - HalfY, Center.Z);
            const FVector TopRight(Center.X + HalfX, Center.Y - HalfY, Center.Z);
            const FVector BottomRight(Center.X + HalfX, Center.Y + HalfY, Center.Z);
            const FVector BottomLeft(Center.X - HalfX, Center.Y + HalfY, Center.Z);

            // GridCoordinate usa assi invertiti rispetto al mondo:
            // - World +X <-> Grid (0, -1)
            // - World -X <-> Grid (0, +1)
            // - World +Y <-> Grid (-1, 0)
            // - World -Y <-> Grid (+1, 0)
            const FIntPoint Up = Coord + FIntPoint(1, 0);      // Lato alto (world -Y)
            const FIntPoint Right = Coord + FIntPoint(0, -1);  // Lato destro (world +X)
            const FIntPoint Down = Coord + FIntPoint(-1, 0);   // Lato basso (world +Y)
            const FIntPoint Left = Coord + FIntPoint(0, 1);    // Lato sinistro (world -X)

            if (!Cells.Contains(Up) && OutsideEmptyCells.Contains(Up)){
                DrawDebugLine(GetWorld(), TopLeft, TopRight, Color, false, 0.0f, 0, LineThickness);
            }
            if (!Cells.Contains(Right) && OutsideEmptyCells.Contains(Right)){
                DrawDebugLine(GetWorld(), TopRight, BottomRight, Color, false, 0.0f, 0, LineThickness);
            }
            if (!Cells.Contains(Down) && OutsideEmptyCells.Contains(Down)){
                DrawDebugLine(GetWorld(), BottomLeft, BottomRight, Color, false, 0.0f, 0, LineThickness);
            }
            if (!Cells.Contains(Left) && OutsideEmptyCells.Contains(Left)){
                DrawDebugLine(GetWorld(), TopLeft, BottomLeft, Color, false, 0.0f, 0, LineThickness);
            }
        }
    };

    AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController());
    const ABaseUnit* SelectedUnitForMovement = PC ? PC->GetSelectedUnitForMovement() : nullptr;

    // I contorni di attacco si vedono solo quando una tua unità è selezionata per il movimento.
    if (SelectedUnitForMovement){
        for (const TPair<FIntPoint, ABaseUnit*>& Elem : UnitByCell){
            ABaseUnit* Unit = Elem.Value;
            if (!Unit){
                continue;
            }

            if (ActiveMovementTimers.Contains(Unit)){
                continue;
            }

            const ACellActor* AttackerCell = GetCellAtGridCoordinate(Unit->GridPosition);
            if (!AttackerCell){
                continue;
            }

            const int32 AttackerHeight = AttackerCell->Altezza;

            TSet<FIntPoint> AttackAreaCells;
            for (int32 dx = -Unit->AttackRange; dx <= Unit->AttackRange; ++dx){
                const int32 MaxDy = Unit->AttackRange - FMath::Abs(dx);
                for (int32 dy = -MaxDy; dy <= MaxDy; ++dy){
                    const FIntPoint Candidate = Unit->GridPosition + FIntPoint(dx, dy);
                    const ACellActor* CandidateCell = GetCellAtGridCoordinate(Candidate);
                    if (CandidateCell && CandidateCell->Altezza <= AttackerHeight){
                        AttackAreaCells.Add(Candidate);
                    }
                }
            }

            const FColor UnitContourColor = Unit->IsAIDriven ? AIAttackColor : HumanAttackColor;
            DrawCellsContour(AttackAreaCells, UnitContourColor, AttackContourZ, AttackLineThickness);
        }
    }

    // Zona di cattura torri: quadrato 5x5 con torre al centro.
    for (ATowerActor* Tower : Torri){
        if (!Tower){
            continue;
        }

        const FIntPoint TowerGrid = LoopCoordinateToGridCoordinate(Tower->GetGridCoord());
        TSet<FIntPoint> CaptureZoneCells;

        constexpr int32 CaptureRange = 2;
        for (int32 dx = -CaptureRange; dx <= CaptureRange; ++dx){
            for (int32 dy = -CaptureRange; dy <= CaptureRange; ++dy){
                const FIntPoint Candidate = TowerGrid + FIntPoint(dx, dy);
                if (GetCellAtGridCoordinate(Candidate)){
                    CaptureZoneCells.Add(Candidate);
                }
            }
        }

        FColor TowerContourColor = NeutralTowerColor;
        switch (Tower->GetControlState()){
            case ETowerControlState::HUMAN:
                TowerContourColor = HumanAttackColor;
                break;
            case ETowerControlState::AI:
                TowerContourColor = AIAttackColor;
                break;
            case ETowerControlState::CONTESTED:
                TowerContourColor = ContestedTowerColor;
                break;
            case ETowerControlState::NEUTRAL:
            default:
                TowerContourColor = NeutralTowerColor;
                break;
        }

        DrawCellsContour(CaptureZoneCells, TowerContourColor, TowerContourZ, TowerLineThickness);
    }
}

/**
 * @brief Calcola lo stato di controllo di una torre
 * @param Tower Torre da valutare
 * @return Stato di controllo risultante
 * @details Confronta le presenze delle unità nelle celle vicine alla torre e determina se il controllo è neutro, umano, AI o conteso.
 */
ETowerControlState AMyGameMode::EvaluateTowerControlState(const ATowerActor* Tower) const{
    if (!Tower){
        return ETowerControlState::NEUTRAL;
    }

    const FIntPoint TowerGrid = LoopCoordinateToGridCoordinate(Tower->GetGridCoord());
    constexpr int32 CaptureRange = 2;

    bool bHumanPresent = false;
    bool bAIPresent = false;

    for (int32 dx = -CaptureRange; dx <= CaptureRange; ++dx){
        for (int32 dy = -CaptureRange; dy <= CaptureRange; ++dy){
            const FIntPoint Candidate = TowerGrid + FIntPoint(dx, dy);
            if (const ABaseUnit* Unit = GetUnitAtCell(Candidate)){
                if (Unit->IsAIDriven){
                    bAIPresent = true;
                }
                else{
                    bHumanPresent = true;
                }

                if (bHumanPresent && bAIPresent){
                    return ETowerControlState::CONTESTED;
                }
            }
        }
    }

    if (bHumanPresent){
        return ETowerControlState::HUMAN;
    }

    if (bAIPresent){
        return ETowerControlState::AI;
    }

    // Sticky control: se nessuno è nella zona, mantengo il proprietario corrente
    // (HUMAN/AI) finchè non entra almeno un'unità avversaria.
    const ETowerControlState PreviousState = Tower->GetControlState();
    if (PreviousState == ETowerControlState::HUMAN || PreviousState == ETowerControlState::AI){
        return PreviousState;
    }

    // Se non c'è proprietario attivo (es. neutrale/contesa) e la zona è vuota, torno neutrale.
    return ETowerControlState::NEUTRAL;
}

/**
 * @brief Aggiorna lo stato di controllo di tutte le torri
 * @details Ricalcola il controllo di ogni torre presente nel registro del GameMode.
 */
void AMyGameMode::RefreshTowerControlStates(){
    for (ATowerActor* Tower : Torri){
        if (!Tower){
            continue;
        }

        Tower->SetControlState(EvaluateTowerControlState(Tower));
    }
}

// Genero la griglia procedurale finchè non ottengo una mappa giocabile anche dopo il piazzamento torri.
/**
 * @brief Genera la griglia procedurale della partita
 * @details Crea la mappa con Perlin Noise, verifica la giocabilità e ripete la generazione se la mappa o il posizionamento torri non sono validi.
 */
void AMyGameMode::GeneraGriglia(){
    bool bIsValid = false;

    while (!bIsValid){
        CleanGrid(); // Riparto sempre da una griglia pulita.

        // Rigenero i seed a ogni tentativo per evitare mappe ripetitive.
        SeedX = FMath::RandRange(0.f, 10000.f);
        SeedY = FMath::RandRange(0.f, 10000.f);

        // Genero la griglia usando Perlin Noise per variare l'altezza e il tipo di terreno.
        int32 Dimensione = 25;
        float Spaziatura = 102.0f;
        float OffsetGriglia = -(Dimensione * Spaziatura) / 2;

        // Itero su una griglia logica e genero le celle corrispondenti in world-space, memorizzandole in una mappa per accesso futuro.
        for (int32 x = 0; x < Dimensione; x++){
            for (int32 y = 0; y < Dimensione; y++){
                float CoordX = (x * NoiseScale) + SeedX;
                float CoordY = (y * NoiseScale) + SeedY;
                float RawNoise = FMath::PerlinNoise2D(FVector2D(CoordX, CoordY));
                float NormalizedNoise = (FMath::Clamp(RawNoise * 1.5f, -1.0f, 1.0f) + 1.0f) * 0.5f;

                // Classifico l'altezza in 5 livelli distinti per variare il colore e il gameplay (es. coperture naturali).
                int32 AltezzaFinale = (NormalizedNoise < 0.25f) ? 0 :
                                     (NormalizedNoise < 0.40f) ? 1 :
                                     (NormalizedNoise < 0.60f) ? 2 :
                                     (NormalizedNoise < 0.75f) ? 3 : 4;

                FVector Posizione(x * Spaziatura + OffsetGriglia, y * Spaziatura + OffsetGriglia, 0.0f);

                // Effettivo spawn dell'attore cella e configurazione della coordinata logica in griglia per il GameMode.
                ACellActor* NuovaCella = GetWorld()->SpawnActor<ACellActor>(
                    ACellActor::StaticClass(),
                    Posizione,
                    FRotator::ZeroRotator
                );

                if (NuovaCella){
                    NuovaCella->Altezza = AltezzaFinale;
                    // Gli assi sono invertiti
                    NuovaCella->GridCoordinate = FIntPoint(Dimensione - y - 1, Dimensione - x - 1);
                    NuovaCella->AggiornaColore();
                    MappaCelle.Add(FIntPoint(x, y), NuovaCella);
                }
            }
        }

        // Primo controllo: la mappa senza torri deve essere connessa.
        bIsValid = IsGridPlayable();
        if (!bIsValid){
            UE_LOG(LogTemp, Warning, TEXT("Mappa non valida (isole trovate). Rigenerazione..."));
            continue;
        }

        // Piazzo le torri solo dopo aver validato la connettività base.
        PosizionaTorri();

        // Secondo controllo: il piazzamento torri non deve spezzare la mappa.
        bIsValid = IsGridPlayable();
        if (!bIsValid){
            UE_LOG(LogTemp, Warning, TEXT("Torri hanno creato una mappa non giocabile. Rigenerazione..."));
        }
    }
}

// Distruggo e ripulisco tutte le strutture della griglia precedente prima di rigenerare.
/**
 * @brief Pulisce la griglia corrente
 * @details Distrugge le celle esistenti e svuota tutte le strutture logiche legate alla mappa.
 */
void AMyGameMode::CleanGrid(){
    // Distruggo tutti gli attori cella ancora presenti in memoria.
    for (auto& Elem : MappaCelle){
        // Verifico il puntatore prima di distruggere l'attore.
        if (Elem.Value){
            Elem.Value->Destroy();
        }
    }

    // Svuoto tutte le strutture logiche per evitare riferimenti stale.
    MappaCelle.Empty();
    UnitByCell.Empty();
    
    // Ripulisco anche il contenitore delle torri.
    Torri.Empty();
    
    // Registro un log di conferma utile durante debug/regenerazioni.
    UE_LOG(LogTemp, Log, TEXT("Griglia pulita. Pronta per una nuova generazione."));
}

// Coordino il piazzamento della torre centrale e delle due laterali simmetriche.
/**
 * @brief Posiziona le torri della partita
 * @details Colloca la torre centrale e le torri laterali rispettando i vincoli di mappa e giocabilità.
 */
void AMyGameMode::PosizionaTorri(){
    FIntPoint CoordCentrale;
    if (PosizionaTorreCentrale(CoordCentrale)){
        SpawnTorre(CoordCentrale);
        
        if (!PosizionaTorriLaterali(CoordCentrale)){
            UE_LOG(LogTemp, Warning, TEXT("Impossibile piazzare le torri laterali."));
        }
    }
    else{
        UE_LOG(LogTemp, Warning, TEXT("Impossibile piazzare la torre centrale."));
    }
}

// Cerco una coordinata valida per la torre centrale partendo dal centro della mappa.
/**
 * @brief Individua e posiziona la torre centrale
 * @param OutCoordinata Coordinata logica scelta per la torre centrale
 * @return true se la torre centrale è stata posizionata correttamente
 */
bool AMyGameMode::PosizionaTorreCentrale(FIntPoint& OutCoordinata){
    int32 CentroX = DimensioneMappa / 2;
    int32 CentroY = DimensioneMappa / 2;
    
    // Cerco la centrale partendo dal centro con raggio crescente.
    for (int32 DeltaX = 0; DeltaX < DimensioneMappa; DeltaX++){
        // Valuto candidate simmetriche rispetto al centro.
        int32 Xs[] = { CentroX - DeltaX, CentroX + DeltaX };
        
        for (int32 X : Xs){
            FIntPoint Candidata(X, CentroY);
            
            if (CellaValidaPerTorre(Candidata)){
                OutCoordinata = Candidata;
                return true;
            }
        }
        
        // DeltaX = 0 produce un doppio check equivalente, accettabile per semplicità.
    }

    return false;
}

// Cerco una coppia di torri laterali simmetriche che rispetti tutti i vincoli di design.
/**
 * @brief Posiziona le torri laterali
 * @param CoordinataCentrale Coordinata della torre centrale usata come riferimento
 * @return true se le torri laterali sono state posizionate correttamente
 */
bool AMyGameMode::PosizionaTorriLaterali(FIntPoint CoordinataCentrale){
    // Uso il semiasse sinistro come origine di ricerca per la coppia di torri laterali.
    FIntPoint CentroS(6, 12);
    
    // Definisco il raggio massimo della scansione a spirale.
    int32 MaxRaggio = 12;

    for (int32 Raggio = 1; Raggio <= MaxRaggio; Raggio++){
        // Espando progressivamente il perimetro attorno al centro sinistro.
        for (int32 x = -Raggio; x <= Raggio; x++){
            for (int32 y = -Raggio; y <= Raggio; y++){
                // Esamino solo il bordo del quadrato corrente per ridurre i check.
                if (FMath::Abs(x) != Raggio && FMath::Abs(y) != Raggio) continue;

                FIntPoint P1 = CentroS + FIntPoint(x, y);
                
                // Calcolo il punto simmetrico rispetto al centro mappa.
                FIntPoint P2(DimensioneMappa - 1 - P1.X, DimensioneMappa - 1 - P1.Y);

                // Valido entrambe le candidate prima di applicare il vincolo distanza.
                if (CellaValidaPerTorre(P1) && CellaValidaPerTorre(P2)){
                    // Impongo distanza minima dalla torre centrale con metrica Chebyshev.
                    int32 DistP1 = FMath::Max(FMath::Abs(P1.X - CoordinataCentrale.X), FMath::Abs(P1.Y - CoordinataCentrale.Y));
                    int32 DistP2 = FMath::Max(FMath::Abs(P2.X - CoordinataCentrale.X), FMath::Abs(P2.Y - CoordinataCentrale.Y));

                    if (DistP1 >= 3 && DistP2 >= 3){
                        // Ho trovato una coppia valida e simmetrica.
                        SpawnTorre(P1);
                        SpawnTorre(P2);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// Verifico se una cella può ospitare una torre secondo i vincoli topologici della mappa.
/**
 * @brief Verifica se una coordinata è valida per il posizionamento di una torre
 * @param Coordinata Coordinata da testare
 * @return true se la cella rispetta i vincoli progettuali della torre
 */
bool AMyGameMode::CellaValidaPerTorre(FIntPoint Coordinata){
    // Verifico che la coordinata esista nella mappa logica.
    if (!MappaCelle.Contains(Coordinata)) return false;

    ACellActor* Cella = MappaCelle[Coordinata];

    // Vincoli base: terra e assenza di torre preesistente.
    if (Cella->Altezza == 0 || Cella->bHasTowers) return false;

    // Vincolo di design: limito il piazzamento torri alla fascia centrale.
    if (Coordinata.Y < 5 || Coordinata.Y > 19) return false;

    return true;
}

// Spawno una torre in world-space e sincronizzo lo stato logico della cella associata.
/**
 * @brief Spawna una torre sulla griglia
 * @param Coordinata Coordinata logica della cella che ospiterà la torre
 * @details Aggiorna anche lo stato logico della cella corrispondente per impedirne l'uso come terreno libero.
 */
void AMyGameMode::SpawnTorre(FIntPoint Coordinata){
    if (!MappaCelle.Contains(Coordinata)) return;

    ACellActor* CellaPuntata = MappaCelle[Coordinata];
    
    // Sollevo lo sticker per evitare Z-fighting con il piano della cella.
    FVector Posizione = CellaPuntata->GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);

    // Spawno l'attore torre e poi lo registro nella logica di mappa.
    ATowerActor* NuovaTorre = GetWorld()->SpawnActor<ATowerActor>(
        ATowerActor::StaticClass(),
        Posizione,
        FRotator::ZeroRotator
    );
    
    if (NuovaTorre){
        NuovaTorre->SetGridCoord(Coordinata);
        NuovaTorre->SetControlState(ETowerControlState::NEUTRAL);
        CellaPuntata->bHasTowers = true;
        Torri.Add(NuovaTorre);
        
        UE_LOG(LogTemp, Log, TEXT("Sticker Torre piazzato in: %s"), *Coordinata.ToString());
    }
}

// Controllo che tutte le celle calpestabili siano connesse in un'unica componente.
/**
 * @brief Verifica se la griglia è completamente giocabile
 * @return true se tutte le celle attraversabili appartengono a un'unica componente connessa
 * @details Usa una visita in ampiezza sulla griglia per evitare isole non raggiungibili.
 */
bool AMyGameMode::IsGridPlayable(){
    TArray<FIntPoint> TutteLeCelleTerra;
    
    // Raccoglio tutte le celle attraversabili: terra e senza torre.
    for (auto& Elem : MappaCelle){
        if (Elem.Value && Elem.Value->Altezza > 0 && !Elem.Value->bHasTowers){
            TutteLeCelleTerra.Add(Elem.Key);
        }
    }

    // Se non esiste terra attraversabile, la mappa è invalida.
    if (TutteLeCelleTerra.Num() == 0) return false;

    // Eseguo una BFS dalla prima cella valida.
    TSet<FIntPoint> Visited;
    ExploreGrid(TutteLeCelleTerra[0], Visited);

    // La mappa è giocabile solo se tutte le celle attraversabili risultano connesse.
    return Visited.Num() == TutteLeCelleTerra.Num();
}

// Eseguo una BFS sulla griglia per marcare tutte le celle raggiungibili da una partenza.
/**
 * @brief Esplora la griglia a partire da una cella iniziale
 * @param StartCoord Coordinata di partenza dell'esplorazione
 * @param Visited Insieme delle celle già visitate
 * @details Funzione ausiliaria usata per il controllo di connettività della mappa.
 */
void AMyGameMode::ExploreGrid(FIntPoint StartCoord, TSet<FIntPoint>& Visited){
    TArray<FIntPoint> Queue;
    Queue.Add(StartCoord);
    Visited.Add(StartCoord);

    int32 Head = 0;
    while (Head < Queue.Num()){
        FIntPoint Current = Queue[Head++];
        
        TArray<FIntPoint> Neighbors = {
            Current + FIntPoint(1, 0), Current + FIntPoint(-1, 0),
            Current + FIntPoint(0, 1), Current + FIntPoint(0, -1)
        };

        for (FIntPoint Neighbor : Neighbors){
            if (MappaCelle.Contains(Neighbor) && !Visited.Contains(Neighbor)){
                ACellActor* CellaVicina = MappaCelle[Neighbor];
                // In BFS considero solo celle attraversabili secondo le regole base.
                if (CellaVicina->Altezza > 0 && !CellaVicina->bHasTowers){
                    Visited.Add(Neighbor);
                    Queue.Add(Neighbor);
                }
            }
        }
    }
}

// Registro il piazzamento di una classe unità impedendo duplicati per tipo.
/**
 * @brief Registra il piazzamento di una classe di unità
 * @param UnitClass Classe dell'unità piazzata
 * @return true se la classe è stata registrata correttamente
 * @details Evita duplicazioni durante il placement iniziale.
 */
bool AMyGameMode::RegisterUnitPlacement(TSubclassOf<ABaseUnit> UnitClass){
    if (!UnitClass) return false;

    UE_LOG(LogTemp, Warning, TEXT("Tentativo piazzamento: %s"), *UnitClass->GetName());

    if (UnitClass->IsChildOf(AJedi::StaticClass())){
        if (Tracker.bJediPlaced) { UE_LOG(LogTemp, Error, TEXT("Jedi già presente!")); return false; }
        Tracker.bJediPlaced = true;
        return true;
    }
    
    if (UnitClass->IsChildOf(ASith::StaticClass())){
        if (Tracker.bSithPlaced) { UE_LOG(LogTemp, Error, TEXT("Sith già presente!")); return false; }
        Tracker.bSithPlaced = true;
        return true;
    }

    if (UnitClass->IsChildOf(ARebel::StaticClass())){
        if (Tracker.bRebelPlaced) { UE_LOG(LogTemp, Error, TEXT("Rebel già presente!")); return false; }
        Tracker.bRebelPlaced = true;
        return true;
    }
    
    if (UnitClass->IsChildOf(AStormtrooper::StaticClass())){
        if (Tracker.bStormtrooperPlaced) { UE_LOG(LogTemp, Error, TEXT("Stormtrooper già presente!")); return false; }
        Tracker.bStormtrooperPlaced = true;
        return true;
    }

    return false;
}

// Controllo se una classe unità è ancora disponibile per il piazzamento.
/**
 * @brief Verifica se una classe di unità può ancora essere piazzata
 * @param UnitClass Classe da controllare
 * @return true se il piazzamento è ancora consentito
 */
bool AMyGameMode::CanPlaceUnit(TSubclassOf<ABaseUnit> UnitClass) const{
    if (!UnitClass) return false;

    if (UnitClass->IsChildOf(AJedi::StaticClass())) {
        return !Tracker.bJediPlaced;
    }
    if (UnitClass->IsChildOf(ASith::StaticClass())) {
        return !Tracker.bSithPlaced;
    }
    if (UnitClass->IsChildOf(ARebel::StaticClass())) {
        return !Tracker.bRebelPlaced;
    }
    if (UnitClass->IsChildOf(AStormtrooper::StaticClass())) {
        return !Tracker.bStormtrooperPlaced;
    }

    return false; // Classe non riconosciuta dal placement tracker.
}

// Applico/rimuovo la desaturazione nelle celle non disponibili durante il placement umano.
/**
 * @brief Abilita o disabilita il dimming del board di placement
 * @param bEnable Stato richiesto per la desaturazione della board
 * @details Viene usato per evidenziare visivamente il lato attivo durante il placement umano.
 */
void AMyGameMode::SetPlacementBoardDimmed(bool bEnable){
    for (auto& Elem : MappaCelle){
        if (Elem.Value){
            const bool bShouldDim = bEnable && Elem.Value->GridCoordinate.X >= 3;
            Elem.Value->SetPlacementDimmed(bShouldDim);
        }
    }
}

// Ritorno true solo negli stati FSM in cui il player umano sta piazzando unità.
/**
 * @brief Indica se è il turno di placement umano
 * @return true se il GameMode si trova in uno stato di placement umano
 */
bool AMyGameMode::IsHumanPlacementTurn() const{
    return CurrentState == EPlacementState::Human_0 || CurrentState == EPlacementState::Human_1;
}

// Riconosco la fase di movimento/battaglia all'interno della FSM.
/**
 * @brief Indica se il gioco si trova nella fase di movimento
 * @return true quando il match è entrato nella fase di combattimento/movimento
 */
bool AMyGameMode::IsMovementPhase() const{
    return CurrentState == EPlacementState::InizializzazioneGioco;
}

// Converto coordinate griglia (UI) in coordinate interne della mappa logica.
/**
 * @brief Converte una coordinata logica in coordinata interna di loop
 * @param GridCoord Coordinata logica esterna
 * @return Coordinata usata come chiave nelle strutture interne della griglia
 */
FIntPoint AMyGameMode::GridCoordinateToLoopCoordinate(const FIntPoint& GridCoord) const{
    const int32 Dimensione = 25;
    return FIntPoint(Dimensione - 1 - GridCoord.Y, Dimensione - 1 - GridCoord.X);
}

// Converto coordinate interne in coordinate griglia usate da UI e attori cella.
/**
 * @brief Converte una coordinata interna di loop in coordinata logica
 * @param LoopCoord Coordinata interna usata dalle mappe del GameMode
 * @return Coordinata logica esposta al gameplay
 */
FIntPoint AMyGameMode::LoopCoordinateToGridCoordinate(const FIntPoint& LoopCoord) const{
    const int32 Dimensione = 25;
    return FIntPoint(Dimensione - 1 - LoopCoord.Y, Dimensione - 1 - LoopCoord.X);
}

// Stabilisco se l'azione corrente spetta al player umano in base a fase e turno.
/**
 * @brief Indica se è il turno di azione del player umano
 * @return true se il turno corrente appartiene al player umano
 */
bool AMyGameMode::IsHumanActionTurn() const{
    if (IsHumanPlacementTurn()){
        return true;
    }

    if (IsMovementPhase()){
        return bHumanBattleTurn;
    }

    return false;
}

// Registro unità e cella nelle strutture di occupazione subito dopo uno spawn.
/**
 * @brief Registra una unità appena spawnata sulla griglia
 * @param Unit Unità da registrare
 * @param Cell Cella che ospita l'unità
 * @details Aggiorna il registro di occupazione e allinea la posizione logica dell'unità con quella della cella.
 */
void AMyGameMode::RegisterSpawnedUnit(ABaseUnit* Unit, ACellActor* Cell){
    if (!Unit || !Cell){
        return;
    }

    Unit->GridPosition = Cell->GridCoordinate;
    Unit->InitializeSpawnDataIfNeeded(Cell->GridCoordinate);
    const FIntPoint LoopCoord = GridCoordinateToLoopCoordinate(Cell->GridCoordinate);
    UnitByCell.Add(LoopCoord, Unit);
    Cell->bHasUnit = true;
}

// Recupero l'unità presente in una cella, se esiste.
/**
 * @brief Restituisce l'unità presente in una cella
 * @param Coord Coordinata logica della cella
 * @return Puntatore all'unità occupante oppure nullptr
 */
ABaseUnit* AMyGameMode::GetUnitAtCell(const FIntPoint& Coord) const{
    const FIntPoint LoopCoord = GridCoordinateToLoopCoordinate(Coord);
    if (ABaseUnit* const* FoundUnit = UnitByCell.Find(LoopCoord)){
        return *FoundUnit;
    }

    return nullptr;
}

// Recupero il puntatore alla cella in coordinate griglia, se esiste.
/**
 * @brief Restituisce la cella logica corrispondente a una coordinata
 * @param Coord Coordinata logica della cella
 * @return Puntatore alla cella oppure nullptr
 */
ACellActor* AMyGameMode::GetCellAtGridCoordinate(const FIntPoint& Coord) const{
    const FIntPoint LoopCoord = GridCoordinateToLoopCoordinate(Coord);
    if (ACellActor* const* FoundCell = MappaCelle.Find(LoopCoord)){
        return *FoundCell;
    }

    return nullptr;
}

// Valuto se un singolo step tra due celle è ammesso e ne calcolo il costo di movimento.
/**
 * @brief Valuta il costo e la possibilità di attraversare uno step di movimento
 * @param Unit Unità che si sta muovendo
 * @param FromGridCoord Coordinata di partenza
 * @param ToGridCoord Coordinata di arrivo
 * @param OutStepCost Costo prodotto dallo step se valido
 * @return true se lo step è attraversabile
 */
bool AMyGameMode::CanTraverseStep(const ABaseUnit* Unit, const FIntPoint& FromGridCoord, const FIntPoint& ToGridCoord, int32& OutStepCost) const{
    OutStepCost = 0;

    if (!Unit){
        return false;
    }

    const ACellActor* FromCell = GetCellAtGridCoordinate(FromGridCoord);
    const ACellActor* ToCell = GetCellAtGridCoordinate(ToGridCoord);
    if (!FromCell || !ToCell){
        return false;
    }

    if (ToCell->Altezza <= 0 || ToCell->bHasTower || ToCell->bHasTowers){
        return false;
    }

    const ABaseUnit* OccupyingUnit = GetUnitAtCell(ToGridCoord);
    if (OccupyingUnit && OccupyingUnit != Unit){
        return false;
    }

    // Le celle di spawn restano riservate all'unità proprietaria per garantire il respawn.
    if (IsSpawnCellReservedForOtherUnit(Unit, ToGridCoord)){
        return false;
    }

    // Vincolo principale: in un singolo step non posso salire oltre +1 di altezza.
    const int32 HeightDelta = ToCell->Altezza - FromCell->Altezza;
    if (HeightDelta > 1){
        return false;
    }

    // Il costo cresce in salita, così privilegio percorsi meno ripidi.
    OutStepCost = 1 + FMath::Max(0, HeightDelta);
    return true;
}

// Le celle spawn sono riservate: nessuna altra unità può occuparle.
/**
 * @brief Verifica se una cella di spawn è riservata a un'altra unità
 * @param Unit Unità che vuole usare la cella
 * @param Coord Coordinata di spawn da controllare
 * @return true se la cella è riservata a un'altra unità
 */
bool AMyGameMode::IsSpawnCellReservedForOtherUnit(const ABaseUnit* Unit, const FIntPoint& Coord) const{
    for (const TPair<FIntPoint, ABaseUnit*>& Elem : UnitByCell){
        const ABaseUnit* OtherUnit = Elem.Value;
        if (!OtherUnit || OtherUnit == Unit || !OtherUnit->bHasInitialSpawnPosition){
            continue;
        }

        if (OtherUnit->InitialSpawnGridPosition == Coord){
            return true;
        }
    }

    return false;
}

// Riposiziona l'unità sulla sua cella di spawn iniziale e ripristina gli HP.
/**
 * @brief Prova a far respawnare un'unità nella sua cella iniziale
 * @param Unit Unità da respawnare
 * @param OutError Messaggio di errore in caso di fallimento
 * @return true se il respawn è andato a buon fine
 */
bool AMyGameMode::TryRespawnUnit(ABaseUnit* Unit, FString* OutError){
    auto SetError = [OutError](const TCHAR* Message){
        if (OutError){
            *OutError = Message;
        }
    };

    if (!Unit){
        SetError(TEXT("Respawn non valido"));
        return false;
    }

    if (!Unit->bHasInitialSpawnPosition){
        SetError(TEXT("Spawn iniziale non impostato"));
        return false;
    }

    const FIntPoint SpawnCoord = Unit->InitialSpawnGridPosition;
    ACellActor* SpawnCell = GetCellAtGridCoordinate(SpawnCoord);
    if (!SpawnCell){
        SetError(TEXT("Cella di spawn non valida"));
        return false;
    }

    if (SpawnCell->Altezza <= 0 || SpawnCell->bHasTower || SpawnCell->bHasTowers){
        SetError(TEXT("Cella di spawn non disponibile"));
        return false;
    }

    if (ABaseUnit* OccupyingUnit = GetUnitAtCell(SpawnCoord)){
        if (OccupyingUnit != Unit){
            SetError(TEXT("Cella di spawn occupata"));
            return false;
        }
    }

    const FIntPoint DeathCoord = Unit->GridPosition;
    const FIntPoint DeathLoopCoord = GridCoordinateToLoopCoordinate(DeathCoord);
    if (ABaseUnit* const* OccupantAtDeathCell = UnitByCell.Find(DeathLoopCoord)){
        if (*OccupantAtDeathCell == Unit){
            UnitByCell.Remove(DeathLoopCoord);
        }
    }

    if (ACellActor* DeathCell = GetCellAtGridCoordinate(DeathCoord)){
        DeathCell->bHasUnit = false;
    }

    // La logica di morte/respawn resta nell'unità base.
    Unit->Die(this);

    if (FTimerHandle* ActiveTimer = ActiveMovementTimers.Find(Unit)){
        GetWorldTimerManager().ClearTimer(*ActiveTimer);
        ActiveMovementTimers.Remove(Unit);
    }
    ActiveMovementPaths.Remove(Unit);
    ActiveMovementIndices.Remove(Unit);

    const FIntPoint SpawnLoopCoord = GridCoordinateToLoopCoordinate(SpawnCoord);
    UnitByCell.Add(SpawnLoopCoord, Unit);
    SpawnCell->bHasUnit = true;

    Unit->SetActorLocation(SpawnCell->GetActorLocation() + FVector(0.f, 0.f, 50.f));

    return true;
}

// Costruisco un percorso a costo minimo entro il budget movimento dell'unità.
/**
 * @brief Costruisce il percorso di movimento migliore verso una destinazione
 * @param Unit Unità che si muove
 * @param TargetCoord Coordinata di destinazione
 * @param OutPath Percorso ricostruito dal GameMode
 * @param OutPathCost Costo totale del percorso, se richiesto
 * @return true se esiste un percorso valido entro il budget di movimento
 * @details Usa una ricerca guidata da A* o Greedy Best-First, mantenendo un open set e un dizionario dei predecessori.
 */
bool AMyGameMode::BuildMovementPath(const ABaseUnit* Unit, const FIntPoint& TargetCoord, TArray<FIntPoint>& OutPath, int32* OutPathCost) const{
    OutPath.Reset();

    if (OutPathCost){
        *OutPathCost = TNumericLimits<int32>::Max();
    }

    if (!Unit){
        return false;
    }

    const FIntPoint StartCoord = Unit->GridPosition;
    if (StartCoord == TargetCoord){
        return false;
    }

    TMap<FIntPoint, int32> BestCost;
    TMap<FIntPoint, FIntPoint> Predecessor;
    TArray<FIntPoint> OpenSet;

    BestCost.Add(StartCoord, 0);
    OpenSet.Add(StartCoord);

    const auto HeuristicCost = [](const FIntPoint& From, const FIntPoint& To){
        return FMath::Abs(From.X - To.X) + FMath::Abs(From.Y - To.Y);
    };

    // Selezione dinamica: A* (f = g + h) oppure Greedy Best-First (f = h).
    while (OpenSet.Num() > 0){
        int32 BestIndex = 0;
        int32 BestFScore = TNumericLimits<int32>::Max();
        for (int32 i = 0; i < OpenSet.Num(); ++i){
            const FIntPoint Candidate = OpenSet[i];
            const int32 CandidateG = BestCost.FindRef(Candidate);
            const int32 CandidateH = HeuristicCost(Candidate, TargetCoord);
            const int32 CandidateF = (PathfindingStrategy == EPathfindingStrategy::AStar)
                ? CandidateG + CandidateH
                : CandidateH;
            if (CandidateF < BestFScore){
                BestFScore = CandidateF;
                BestIndex = i;
            }
        }

        const FIntPoint Current = OpenSet[BestIndex];
        OpenSet.RemoveAtSwap(BestIndex);

        if (Current == TargetCoord){
            break;
        }

        const int32 CurrentCost = BestCost.FindRef(Current);
        if (CurrentCost >= Unit->MaxMovement){
            continue;
        }

        const TArray<FIntPoint> Neighbors = {
            Current + FIntPoint(1, 0), Current + FIntPoint(-1, 0),
            Current + FIntPoint(0, 1), Current + FIntPoint(0, -1)
        };

        for (const FIntPoint& Neighbor : Neighbors){
            int32 StepCost = 0;
            if (!CanTraverseStep(Unit, Current, Neighbor, StepCost)){
                continue;
            }

            const int32 NewCost = CurrentCost + StepCost;
            if (NewCost > Unit->MaxMovement){
                continue;
            }

            const int32* ExistingCost = BestCost.Find(Neighbor);
            if (!ExistingCost || NewCost < *ExistingCost){
                BestCost.Add(Neighbor, NewCost);
                Predecessor.Add(Neighbor, Current);

                if (!OpenSet.Contains(Neighbor)){
                    OpenSet.Add(Neighbor);
                }
            }
        }
    }

    if (!BestCost.Contains(TargetCoord)){
        return false;
    }

    if (OutPathCost){
        *OutPathCost = BestCost.FindRef(TargetCoord);
    }

    // Ricostruisco il percorso risalendo i predecessori dal target allo start.
    TArray<FIntPoint> ReversePath;
    FIntPoint Cursor = TargetCoord;
    ReversePath.Add(Cursor);

    while (Cursor != StartCoord){
        const FIntPoint* Prev = Predecessor.Find(Cursor);
        if (!Prev){
            return false;
        }

        Cursor = *Prev;
        ReversePath.Add(Cursor);
    }

    for (int32 i = ReversePath.Num() - 1; i >= 0; --i){
        OutPath.Add(ReversePath[i]);
    }

    return OutPath.Num() > 1;
}

// Avvio l'animazione step-by-step di un'unità lungo il percorso calcolato.
/**
 * @brief Avvia l'animazione di movimento di una unità
 * @param Unit Unità da animare
 * @param Path Percorso da seguire
 * @details Memorizza il path attivo e programma il timer per avanzare passo dopo passo.
 */
void AMyGameMode::StartUnitMovementAnimation(ABaseUnit* Unit, const TArray<FIntPoint>& Path){
    if (!Unit || Path.Num() <= 1){
        return;
    }

    if (FTimerHandle* ExistingTimer = ActiveMovementTimers.Find(Unit)){
        GetWorldTimerManager().ClearTimer(*ExistingTimer);
    }

    ActiveMovementPaths.Add(Unit, Path);
    ActiveMovementIndices.Add(Unit, 1);

    FTimerHandle StepTimer;
    FTimerDelegate StepDelegate;
    StepDelegate.BindUObject(this, &AMyGameMode::AdvanceUnitMovementStep, Unit);
    GetWorldTimerManager().SetTimer(StepTimer, StepDelegate, MovementTau, true);
    ActiveMovementTimers.Add(Unit, StepTimer);
}

// Avanzo di un passo l'animazione movimento dell'unità e termino quando il percorsoèfinito.
/**
 * @brief Avanza di un passo l'animazione di movimento di una unità
 * @param Unit Unità in movimento
 * @details Consuma il path attivo e conclude il movimento quando non restano più step da eseguire.
 */
void AMyGameMode::AdvanceUnitMovementStep(ABaseUnit* Unit){
    if (!Unit){
        return;
    }

    TArray<FIntPoint>* PathPtr = ActiveMovementPaths.Find(Unit);
    int32* IndexPtr = ActiveMovementIndices.Find(Unit);
    FTimerHandle* TimerPtr = ActiveMovementTimers.Find(Unit);
    if (!PathPtr || !IndexPtr || !TimerPtr){
        return;
    }

    if (*IndexPtr >= PathPtr->Num()){
        GetWorldTimerManager().ClearTimer(*TimerPtr);
        ActiveMovementTimers.Remove(Unit);
        ActiveMovementIndices.Remove(Unit);
        ActiveMovementPaths.Remove(Unit);
        return;
    }

    const FIntPoint StepCoord = (*PathPtr)[*IndexPtr];
    if (ACellActor* StepCell = GetCellAtGridCoordinate(StepCoord)){
        Unit->SetActorLocation(StepCell->GetActorLocation() + FVector(0.f, 0.f, 50.f));
    }

    ++(*IndexPtr);
}

// Calcolo tutte le celle raggiungibili con lo stesso modello di costo usato nel movimento reale.
/**
 * @brief Calcola le celle di movimento raggiungibili da un'unità
 * @param Unit Unità da analizzare
 * @param OutCells Celle raggiungibili da popolare
 * @details Popola la lista delle celle che il controller umano può evidenziare come destinazioni valide.
 */
void AMyGameMode::GetReachableMovementCells(const ABaseUnit* Unit, TArray<ACellActor*>& OutCells) const{
    OutCells.Reset();

    if (!Unit){
        return;
    }

    const FIntPoint StartCoord = Unit->GridPosition;
    if (!GetCellAtGridCoordinate(StartCoord)){
        return;
    }

    TMap<FIntPoint, int32> BestCost;
    TArray<FIntPoint> OpenSet;

    BestCost.Add(StartCoord, 0);
    OpenSet.Add(StartCoord);

    // Espansione a costo minimo per allineare highlight e validazione del movimento.
    while (OpenSet.Num() > 0){
        int32 BestIndex = 0;
        int32 CurrentBestCost = TNumericLimits<int32>::Max();
        for (int32 i = 0; i < OpenSet.Num(); ++i){
            const int32 Cost = BestCost.FindRef(OpenSet[i]);
            if (Cost < CurrentBestCost){
                CurrentBestCost = Cost;
                BestIndex = i;
            }
        }

        const FIntPoint Current = OpenSet[BestIndex];
        OpenSet.RemoveAtSwap(BestIndex);

        const int32 CurrentCost = BestCost.FindRef(Current);
        if (CurrentCost >= Unit->MaxMovement){
            continue;
        }

        const TArray<FIntPoint> Neighbors = {
            Current + FIntPoint(1, 0), Current + FIntPoint(-1, 0),
            Current + FIntPoint(0, 1), Current + FIntPoint(0, -1)
        };

        for (const FIntPoint& Neighbor : Neighbors){
            int32 StepCost = 0;
            if (!CanTraverseStep(Unit, Current, Neighbor, StepCost)){
                continue;
            }

            const int32 NewCost = CurrentCost + StepCost;
            if (NewCost > Unit->MaxMovement){
                continue;
            }

            const int32* ExistingCost = BestCost.Find(Neighbor);
            if (!ExistingCost || NewCost < *ExistingCost){
                BestCost.Add(Neighbor, NewCost);

                if (!OpenSet.Contains(Neighbor)){
                    OpenSet.Add(Neighbor);
                }
            }
        }
    }

    for (const TPair<FIntPoint, int32>& Elem : BestCost){
        if (Elem.Key == StartCoord || Elem.Value <= 0 || Elem.Value > Unit->MaxMovement){
            continue;
        }

        if (ACellActor* Cell = GetCellAtGridCoordinate(Elem.Key)){
            OutCells.Add(Cell);
        }
    }
}

// Ottiene tutti i nemici che si trovano nel range di attacco dell'unità
/**
 * @brief Calcola i nemici attaccabili da una unità
 * @param Unit Unità attaccante da analizzare
 * @param OutEnemies Lista dei bersagli validi da popolarsi
 * @details Filtra le unità avversarie presenti in raggio rispettando quota, linea di tiro e regole di attacco.
 */
void AMyGameMode::GetEnemiesInAttackRange(const ABaseUnit* Unit, TArray<ABaseUnit*>& OutEnemies) const{
    OutEnemies.Reset();

    if (!Unit || !CanUnitAttackThisTurn(Unit)){
        return;
    }

    // Itera su tutte le unità sulla griglia
    for (const TPair<FIntPoint, ABaseUnit*>& Elem : UnitByCell){
        if (!Elem.Value || Elem.Value == Unit){
            continue;
        }

        // Controlla se il bersaglio è nemico (diverso team)
        if (Elem.Value->IsAIDriven == Unit->IsAIDriven){
            continue;
        }

        if (IsAttackValid(Unit, Elem.Value, nullptr)){
            OutEnemies.Add(Elem.Value);
        }
    }
}

// Ritorna il costo effettivo dell'attacco: eventuale raddoppio solo in salita.
/**
 * @brief Calcola il costo effettivo di un attacco
 * @param Attacker Unità che attacca
 * @param Target Bersaglio dell'attacco
 * @return Costo di attacco effettivo usato per la validazione del range
 */
int32 AMyGameMode::GetEffectiveAttackCost(const ABaseUnit* Attacker, const ABaseUnit* Target) const{
    if (!Attacker || !Target){
        return TNumericLimits<int32>::Max();
    }

    const ACellActor* AttackerCell = GetCellAtGridCoordinate(Attacker->GridPosition);
    const ACellActor* TargetCell = GetCellAtGridCoordinate(Target->GridPosition);
    if (!AttackerCell || !TargetCell){
        return TNumericLimits<int32>::Max();
    }

    const int32 HorizontalDist = FMath::Abs(Target->GridPosition.X - Attacker->GridPosition.X) +
        FMath::Abs(Target->GridPosition.Y - Attacker->GridPosition.Y);

    int32 AttackCost = HorizontalDist;
    if (TargetCell->Altezza > AttackerCell->Altezza){
        AttackCost *= 2;
    }

    return AttackCost;
}

// Negli attacchi ranged considero solo la geometria tra le unità: le torri bloccano il movimento, non il tiro.
/**
 * @brief Verifica se una linea di tiro è libera
 * @param Attacker Unità che spara
 * @param Target Bersaglio dell'attacco
 * @return true se non ci sono ostacoli lungo la traiettoria
 */
bool AMyGameMode::HasClearRangedLineOfFire(const ABaseUnit* Attacker, const ABaseUnit* Target) const{
    if (!Attacker || !Target){
        return false;
    }

    if (Attacker->AttackRange <= 1){
        return true;
    }

    int32 X0 = Attacker->GridPosition.X;
    int32 Y0 = Attacker->GridPosition.Y;
    const int32 X1 = Target->GridPosition.X;
    const int32 Y1 = Target->GridPosition.Y;

    const int32 Dx = FMath::Abs(X1 - X0);
    const int32 Dy = FMath::Abs(Y1 - Y0);
    const int32 Sx = (X0 < X1) ? 1 : -1;
    const int32 Sy = (Y0 < Y1) ? 1 : -1;
    int32 Err = Dx - Dy;

    while (!(X0 == X1 && Y0 == Y1)){
        const int32 E2 = 2 * Err;
        if (E2 > -Dy){
            Err -= Dy;
            X0 += Sx;
        }
        if (E2 < Dx){
            Err += Dx;
            Y0 += Sy;
        }

        // Salto la cella target: deve rimanere colpibile se valida.
        if (X0 == X1 && Y0 == Y1){
            break;
        }

        const FIntPoint SampleCoord(X0, Y0);
        const ACellActor* SampleCell = GetCellAtGridCoordinate(SampleCoord);
        if (!SampleCell){
            return false;
        }

        // L'attacco non viene influenzato dalle torri: restano solo un vincolo di calpestabilità.
    }

    return true;
}

// Regole condivise di validazione attacco usate da highlight, input umano e bot.
/**
 * @brief Verifica se un attacco è valido
 * @param Attacker Unità attaccante
 * @param Target Bersaglio dell'attacco
 * @param OutError Messaggio di errore in caso di fallimento
 * @return true se l'attacco rispetta tutti i vincoli di gameplay
 */
bool AMyGameMode::IsAttackValid(const ABaseUnit* Attacker, const ABaseUnit* Target, FString* OutError) const{
    auto SetError = [OutError](const TCHAR* Message){
        if (OutError){
            *OutError = Message;
        }
    };

    if (!Attacker || !Target){
        SetError(TEXT("Attacco non valido"));
        return false;
    }

    if (Attacker == Target || Attacker->IsAIDriven == Target->IsAIDriven){
        SetError(TEXT("Bersaglio non valido"));
        return false;
    }

    const ACellActor* AttackerCell = GetCellAtGridCoordinate(Attacker->GridPosition);
    const ACellActor* TargetCell = GetCellAtGridCoordinate(Target->GridPosition);
    if (!AttackerCell || !TargetCell){
        SetError(TEXT("Attacco non valido"));
        return false;
    }

    if (TargetCell->Altezza > AttackerCell->Altezza){
        SetError(TEXT("Bersaglio su elevazione troppo alta"));
        return false;
    }

    const int32 AttackCost = GetEffectiveAttackCost(Attacker, Target);
    if (AttackCost > Attacker->AttackRange){
        SetError(TEXT("Bersaglio fuori range"));
        return false;
    }

    if (!HasClearRangedLineOfFire(Attacker, Target)){
        SetError(TEXT("Linea di tiro ostruita"));
        return false;
    }

    return true;
}

// Conteggio massimo di unità movibili dal team nel turno corrente.
/**
 * @brief Restituisce il numero di unità movibili per un team
 * @param bForHuman true per il team umano, false per il bot
 * @return Numero massimo di unità movibili nel turno corrente
 */
int32 AMyGameMode::GetMovableUnitCount(bool bForHuman) const{
    int32 Count = 0;
    for (const TPair<FIntPoint, ABaseUnit*>& Elem : UnitByCell){
        if (!Elem.Value){
            continue;
        }

        const bool bIsHumanUnit = !Elem.Value->IsAIDriven;
        if (bIsHumanUnit == bForHuman){
            ++Count;
        }
    }

    // Regola attuale: ogni player muove al massimo due pezzi a turno.
    return FMath::Min(2, Count);
}

/**
 * @brief Indica se una unità è uno sniper
 * @param Unit Unità da classificare
 * @return true se l'unità appartiene alla famiglia sniper
 */
bool AMyGameMode::IsSniperUnit(const ABaseUnit* Unit) const{
    return Unit && (Unit->IsA(ASniper::StaticClass()) || Unit->IsA(ARebel::StaticClass()) || Unit->IsA(AStormtrooper::StaticClass()));
}

/**
 * @brief Indica se una unità è un brawler
 * @param Unit Unità da classificare
 * @return true se l'unità appartiene alla famiglia brawler
 */
bool AMyGameMode::IsBrawlerUnit(const ABaseUnit* Unit) const{
    return Unit && (Unit->IsA(ABrawler::StaticClass()) || Unit->IsA(AJedi::StaticClass()) || Unit->IsA(ASith::StaticClass()));
}

/**
 * @brief Reset dei flag di azione di un team
 * @param bForHuman true per il team umano, false per il bot
 * @details Ripristina lo stato di movimento/attacco per sniper e brawler del team selezionato.
 */
void AMyGameMode::ResetTeamTurnActionFlags(bool bForHuman){
    FTeamTurnActionFlags& Flags = bForHuman ? HumanTurnActionFlags : BotTurnActionFlags;
    Flags = FTeamTurnActionFlags();

    bool bHasSniper = false;
    bool bHasBrawler = false;
    for (const TPair<FIntPoint, ABaseUnit*>& Elem : UnitByCell){
        const ABaseUnit* Unit = Elem.Value;
        if (!Unit){
            continue;
        }

        const bool bIsHumanUnit = !Unit->IsAIDriven;
        if (bIsHumanUnit != bForHuman){
            continue;
        }

        bHasSniper = bHasSniper || IsSniperUnit(Unit);
        bHasBrawler = bHasBrawler || IsBrawlerUnit(Unit);
    }

    // Se una tipologia non è presente nel team, la considero già completa.
    if (!bHasSniper){
        Flags.HasSniperMoved = true;
        Flags.HasSniperAttacked = true;
    }
    if (!bHasBrawler){
        Flags.HasBrawlerMoved = true;
        Flags.HasBrawlerAttacked = true;
    }
}

/**
 * @brief Aggiorna i flag di azione di un team dopo un'azione
 * @param bForHuman true per il team umano, false per il bot
 * @param Unit Unità che ha agito
 * @param bMoved Indica se l'unità ha mosso
 * @param bAttacked Indica se l'unità ha attaccato
 */
void AMyGameMode::UpdateTeamTurnActionFlagsForAction(bool bForHuman, const ABaseUnit* Unit, bool bMoved, bool bAttacked){
    if (!Unit){
        return;
    }

    const bool bIsHumanUnit = !Unit->IsAIDriven;
    if (bIsHumanUnit != bForHuman){
        return;
    }

    FTeamTurnActionFlags& Flags = bForHuman ? HumanTurnActionFlags : BotTurnActionFlags;
    if (IsSniperUnit(Unit)){
        if (bMoved){
            Flags.HasSniperMoved = true;
        }
        if (bAttacked){
            Flags.HasSniperAttacked = true;
        }
        return;
    }

    if (IsBrawlerUnit(Unit)){
        if (bMoved){
            Flags.HasBrawlerMoved = true;
        }
        if (bAttacked){
            Flags.HasBrawlerAttacked = true;
        }
    }
}

/**
 * @brief Verifica se un ruolo ha ancora attacchi disponibili
 * @param bForHuman true per il team umano, false per il bot
 * @param bForSniper true per il ruolo sniper, false per il ruolo brawler
 * @return true se esiste almeno un attacco ancora eseguibile per quel ruolo
 */
bool AMyGameMode::HasAnyAvailableAttackForRole(bool bForHuman, bool bForSniper) const{
    for (const TPair<FIntPoint, ABaseUnit*>& Elem : UnitByCell){
        ABaseUnit* Unit = Elem.Value;
        if (!Unit){
            continue;
        }

        const bool bIsHumanUnit = !Unit->IsAIDriven;
        if (bIsHumanUnit != bForHuman){
            continue;
        }

        const bool bIsSniper = IsSniperUnit(Unit);
        if (bIsSniper != bForSniper){
            continue;
        }

        if (!CanUnitAttackThisTurn(Unit)){
            continue;
        }

        TArray<ABaseUnit*> Targets;
        GetEnemiesInAttackRange(Unit, Targets);
        if (Targets.Num() > 0){
            return true;
        }
    }

    return false;
}

/**
 * @brief Verifica se i flag di azione del team sono completi
 * @param bForHuman true per il team umano, false per il bot
 * @return true se il team ha completato la copertura richiesta per sniper e brawler
 */
bool AMyGameMode::AreTeamTurnActionFlagsComplete(bool bForHuman) const {
    const FTeamTurnActionFlags& Flags = bForHuman ? HumanTurnActionFlags : BotTurnActionFlags;

    const bool bSniperForcedDone = Flags.HasSniperAttacked || 
                                   (Flags.HasSniperMoved && !HasAnyAvailableAttackForRole(bForHuman, true));


    const bool bBrawlerForcedDone = Flags.HasBrawlerAttacked || 
                                    (Flags.HasBrawlerMoved && !HasAnyAvailableAttackForRole(bForHuman, false));

    return bSniperForcedDone && bBrawlerForcedDone;
}

// Inizializzo un nuovo turno di movimento per umano o bot e resetto i relativi tracker.
/**
 * @brief Inizializza un nuovo turno di movimento
 * @param bForHuman true se il nuovo turno appartiene al player umano
 * @details Reset dei tracker, conteggio mosse e assegnazioni torre del team selezionato.
 */
void AMyGameMode::BeginMovementTurn(bool bForHuman){
    const bool bPreviousHumanBattleTurn = bHumanBattleTurn;
    const bool bWasMovementCycleStarted = bHasStartedMovementTurnCycle;

    if (bHasStartedMovementTurnCycle){
        if (AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController())){
            PC->LogEndTurnAction();
        }
    }
    else{
        bHasStartedMovementTurnCycle = true;
    }

    bHumanBattleTurn = bForHuman;

    if (bHumanBattleTurn){
        HumanUnitsActedThisTurn.Reset();
        HumanUnitsMovedThisTurn.Reset();
        HumanUnitsAttackedThisTurn.Reset();
        HumanMovesRemaining = GetMovableUnitCount(true);
        ResetTeamTurnActionFlags(true);
    }
    else{
        BotUnitsActedThisTurn.Reset();
        BotUnitsMovedThisTurn.Reset();
        BotUnitsAttackedThisTurn.Reset();
        BotMovesRemaining = GetMovableUnitCount(false);
        ResetTeamTurnActionFlags(false);
        ReservedBotTowerTargets.Reset();
        BotUnitTowerTargets.Reset();
        BuildBotTowerAssignments();
    }

    if (AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController())){
        PC->RefreshTurnButtonState();
    }

    // Verifica condizione di vittoria a ogni cambio turno, ma solo dopo
    // che il ciclo movimento è realmente iniziato.
    if (bWasMovementCycleStarted && (bHumanBattleTurn != bPreviousHumanBattleTurn)){
        RefreshTowerControlStates();
        CheckGameWinCondition(bHumanBattleTurn);
    }
}

// Verifico se una specifica unità può ancora muovere nel turno attivo.
/**
 * @brief Verifica se un'unità può muoversi nel turno corrente
 * @param Unit Unità da controllare
 * @return true se l'unità può ancora muovere
 */
bool AMyGameMode::CanUnitMoveThisTurn(const ABaseUnit* Unit) const{
    if (!Unit || !IsMovementPhase()){
        return false;
    }

    const bool bIsHumanUnit = !Unit->IsAIDriven;
    if (bIsHumanUnit != bHumanBattleTurn){
        return false;
    }

    if (bIsHumanUnit){
        return !HumanUnitsMovedThisTurn.Contains(const_cast<ABaseUnit*>(Unit));
    }

    return !BotUnitsMovedThisTurn.Contains(const_cast<ABaseUnit*>(Unit));
}

/**
 * @brief Verifica se un'unità può attaccare nel turno corrente
 * @param Unit Unità da controllare
 * @return true se l'unità può ancora attaccare
 */
bool AMyGameMode::CanUnitAttackThisTurn(const ABaseUnit* Unit) const{
    if (!Unit || !IsMovementPhase()){
        return false;
    }

    const bool bIsHumanUnit = !Unit->IsAIDriven;
    if (bIsHumanUnit != bHumanBattleTurn){
        return false;
    }

    if (bIsHumanUnit){
        return !HumanUnitsAttackedThisTurn.Contains(const_cast<ABaseUnit*>(Unit));
    }

    return !BotUnitsAttackedThisTurn.Contains(const_cast<ABaseUnit*>(Unit));
}

// Eseguo tutte le validazioni di movimento e aggiorno stato logico + animazione se valide.
/**
 * @brief Prova a muovere una unità su una destinazione valida
 * @param Unit Unità da muovere
 * @param TargetCoord Coordinata di destinazione
 * @param OutError Messaggio di errore in caso di fallimento
 * @return true se il movimento è stato eseguito
 * @details Valida il path, aggiorna occupazione e avvia l'animazione di movimento.
 */
bool AMyGameMode::TryMoveUnit(ABaseUnit* Unit, const FIntPoint& TargetCoord, FString& OutError){
    OutError.Empty();

    if (!Unit){
        OutError = TEXT("Unità non valida");
        return false;
    }

    if (!CanUnitMoveThisTurn(Unit)){
        OutError = TEXT("Questa unità non può più muoversi in questo turno");
        return false;
    }

    const FIntPoint StartCoord = Unit->GridPosition;
    if (StartCoord == TargetCoord){
        OutError = TEXT("Seleziona una cella diversa");
        return false;
    }

    ACellActor* TargetCell = GetCellAtGridCoordinate(TargetCoord);
    if (!TargetCell){
        OutError = TEXT("Cella destinazione non valida");
        return false;
    }

    if (TargetCell->Altezza <= 0 || TargetCell->bHasTower || TargetCell->bHasTowers){
        OutError = TEXT("Destinazione non percorribile");
        return false;
    }

    if (ABaseUnit* OccupyingUnit = GetUnitAtCell(TargetCoord)){
        if (OccupyingUnit != Unit){
            OutError = TEXT("Destinazione occupata");
            return false;
        }
    }

    TArray<FIntPoint> Path;

    if (!BuildMovementPath(Unit, TargetCoord, Path)){
        OutError = TEXT("Cella non raggiungibile");
        return false;
    }

    // Verifico coerenza con l'insieme celle evidenziabili lato controller.
    TArray<ACellActor*> ReachableCells;
    GetReachableMovementCells(Unit, ReachableCells);

    bool bReachableTarget = false;
    for (ACellActor* Cell : ReachableCells){
        if (Cell && Cell->GridCoordinate == TargetCoord){
            bReachableTarget = true;
            break;
        }
    }

    if (!bReachableTarget){
        OutError = TEXT("Cella non raggiungibile");
        return false;
    }

    const FIntPoint StartLoopCoord = GridCoordinateToLoopCoordinate(StartCoord);
    const FIntPoint TargetLoopCoord = GridCoordinateToLoopCoordinate(TargetCoord);

    if (ACellActor** StartCellPtr = MappaCelle.Find(StartLoopCoord)){
        if (*StartCellPtr){
            (*StartCellPtr)->bHasUnit = false;
        }
    }

    TargetCell->bHasUnit = true;
    UnitByCell.Remove(StartLoopCoord);
    UnitByCell.Add(TargetLoopCoord, Unit);
    Unit->GridPosition = TargetCoord;
    if (Unit->IsAIDriven){
        BotUnitsMovedThisTurn.Add(Unit);
        UpdateTeamTurnActionFlagsForAction(false, Unit, true, false);
    }
    else{
        HumanUnitsMovedThisTurn.Add(Unit);
        UpdateTeamTurnActionFlagsForAction(true, Unit, true, false);
    }

    StartUnitMovementAnimation(Unit, Path);

    if (AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController())){
        PC->LogMoveAction(Unit, StartCoord, TargetCoord);
    }

    return true;
}

// Esegue un attacco valido e ripulisce la griglia se il bersaglio muore.
/**
 * @brief Prova a eseguire un attacco valido
 * @param Attacker Unità attaccante
 * @param Target Bersaglio dell'attacco
 * @param OutError Messaggio di errore in caso di fallimento
 * @return true se l'attacco è andato a buon fine
 * @details Applica il danno, registra il log e gestisce l'eventuale respawn del bersaglio.
 */
bool AMyGameMode::TryAttackUnit(ABaseUnit* Attacker, ABaseUnit* Target, FString& OutError){
    OutError.Empty();

    if (!CanUnitAttackThisTurn(Attacker)){
        OutError = TEXT("Questa unità ha già attaccato in questo turno");
        return false;
    }

    if (!IsAttackValid(Attacker, Target, &OutError)){
        return false;
    }

    Attacker->AttackUnit(Target);

    const int32 TargetHealthAfter = Target->Health;

    if (TargetHealthAfter <= 0){
        if (!TryRespawnUnit(Target, &OutError)){
            UE_LOG(LogTemp, Warning, TEXT("Respawn fallito per %s: %s"), *Target->GetName(), *OutError);
        }
    }

    if (Attacker->IsAIDriven){
        const bool bConsumedMove = !BotUnitsMovedThisTurn.Contains(Attacker);
        if (bConsumedMove){
            BotUnitsMovedThisTurn.Add(Attacker);
        }

        BotUnitsAttackedThisTurn.Add(Attacker);
        UpdateTeamTurnActionFlagsForAction(false, Attacker, bConsumedMove, true);
    }
    else{
        const bool bConsumedMove = !HumanUnitsMovedThisTurn.Contains(Attacker);
        if (bConsumedMove){
            HumanUnitsMovedThisTurn.Add(Attacker);
        }

        HumanUnitsAttackedThisTurn.Add(Attacker);
        UpdateTeamTurnActionFlagsForAction(true, Attacker, bConsumedMove, true);

        // Reset voluntary interrupt flag quando l'unità attacca
        FTeamTurnActionFlags& Flags = HumanTurnActionFlags;
        if (IsSniperUnit(Attacker)){
            Flags.bSniperInterruptedVoluntarily = false;
        }
        else if (IsBrawlerUnit(Attacker)){
            Flags.bBrawlerInterruptedVoluntarily = false;
        }
    }

    if (AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController())){
        PC->RefreshTurnButtonState();
    }

    return true;
}

// Registro la prima azione utile dell'unità umana nel turno corrente.
/**
 * @brief Notifica che un'azione umana di movimento è stata risolta
 * @param MovedUnit Unità che ha concluso il movimento
 * @details Aggiorna i tracker di turno e verifica l'eventuale completamento automatico del turno.
 */
void AMyGameMode::NotifyHumanMovementResolved(ABaseUnit* MovedUnit){
    if (!IsMovementPhase() || !bHumanBattleTurn || !MovedUnit || MovedUnit->IsAIDriven){
        return;
    }

    // Marca che l'unità ha mosso
    HumanUnitsMovedThisTurn.Add(MovedUnit);
    UpdateTeamTurnActionFlagsForAction(true, MovedUnit, true, false);

    // Verifica se l'unità ha nemici nell'area di attacco
    TArray<ABaseUnit*> EnemiesInRange;
    GetEnemiesInAttackRange(MovedUnit, EnemiesInRange);

    FTeamTurnActionFlags& Flags = HumanTurnActionFlags;
    if (IsSniperUnit(MovedUnit)){
        Flags.bSniperInterruptedVoluntarily = (EnemiesInRange.Num() > 0);
    }
    else if (IsBrawlerUnit(MovedUnit)){
        Flags.bBrawlerInterruptedVoluntarily = (EnemiesInRange.Num() > 0);
    }

    NotifyHumanActionResolved(MovedUnit);
}

/**
 * @brief Notifica che un'azione umana complessiva è stata risolta
 * @param ActingUnit Unità che ha completato l'azione
 * @details Aggiorna i tracker di turno e riduce il budget mosse residuo del team umano.
 */
void AMyGameMode::NotifyHumanActionResolved(ABaseUnit* ActingUnit){
    if (!IsMovementPhase() || !bHumanBattleTurn){
        return;
    }

    if (!ActingUnit || ActingUnit->IsAIDriven){
        return;
    }

    const bool bFirstActionThisTurn = !HumanUnitsActedThisTurn.Contains(ActingUnit);
    HumanUnitsActedThisTurn.Add(ActingUnit);

    if (bFirstActionThisTurn){
        HumanMovesRemaining = FMath::Max(0, HumanMovesRemaining - 1);
    }

    // Verifica se il turno dovrebbe terminare automaticamente
    if (ShouldTurnAutoComplete(true)){
        BeginMovementTurn(false);
        ExecuteStateLogic();
        return;
    }

    if (AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController())){
        PC->RefreshTurnButtonState();
    }

    ExecuteStateLogic();
}

/**
 * @brief Notifica che un'azione del bot è stata risolta
 * @param ActingUnit Unità che ha completato l'azione
 * @details Aggiorna i tracker del bot e verifica se il turno AI è completo.
 */
void AMyGameMode::NotifyBotActionResolved(ABaseUnit* ActingUnit){
    if (!IsMovementPhase() || bHumanBattleTurn){
        return;
    }

    if (!ActingUnit || !ActingUnit->IsAIDriven){
        return;
    }

    const bool bFirstActionThisTurn = !BotUnitsActedThisTurn.Contains(ActingUnit);
    BotUnitsActedThisTurn.Add(ActingUnit);

    if (bFirstActionThisTurn){
        BotMovesRemaining = FMath::Max(0, BotMovesRemaining - 1);
    }

    if (AreTeamTurnActionFlagsComplete(false)){
        BeginMovementTurn(true);
        ExecuteStateLogic();
        return;
    }

    ExecuteStateLogic();
}

/**
 * @brief Indica se il turno umano può essere terminato manualmente
 * @return true se il giocatore umano può premere il pulsante di fine turno
 */
bool AMyGameMode::CanHumanEndTurn() const{
    if (!IsMovementPhase() || !bHumanBattleTurn){
        return false;
    }

    // Fine turno manuale disponibile dopo 2 azioni umane nel turno corrente.
    return HumanMovesRemaining <= 0;
}

/**
 * @brief Chiude il turno umano
 * @param OutError Messaggio di errore in caso di fallimento
 * @return true se il turno è stato chiuso correttamente
 */
bool AMyGameMode::EndHumanTurn(FString* OutError){
    if (bGameEnded){
        if (OutError){
            *OutError = TEXT("La partita è già terminata");
        }
        return false;
    }

    if (!IsMovementPhase()){
        if (OutError){
            *OutError = TEXT("Il fine turno è disponibile solo in fase movimento");
        }
        return false;
    }

    if (!bHumanBattleTurn){
        if (OutError){
            *OutError = TEXT("Non è il tuo turno");
        }
        return false;
    }

    if (!CanHumanEndTurn()){
        if (OutError){
            const bool bSniperMoved = HumanTurnActionFlags.HasSniperMoved;
            const bool bBrawlerMoved = HumanTurnActionFlags.HasBrawlerMoved;

            if (bSniperMoved && !bBrawlerMoved){
                *OutError = TEXT("hai mosso lo sniper, fai un'azione anche con il brawler per continuare");
            }
            else if (!bSniperMoved && bBrawlerMoved){
                *OutError = TEXT("hai mosso il brawler, fai un'azione anche con lo sniper per continuare");
            }
            else{
                *OutError = TEXT("completa almeno un'azione per ogni unità");
            }
        }
        return false;
    }

    BeginMovementTurn(false);
    ExecuteStateLogic();
    return true;
}

/**
 * @brief Verifica se esistono ancora azioni residue per un team
 * @param bForHuman true per il team umano, false per il bot
 * @return true se almeno una unità del team può ancora agire
 */
bool AMyGameMode::HasAnyRemainingActionForTeam(bool bForHuman) const{
    for (const TPair<FIntPoint, ABaseUnit*>& Elem : UnitByCell){
        const ABaseUnit* Unit = Elem.Value;
        if (!Unit){
            continue;
        }

        const bool bIsHumanUnit = !Unit->IsAIDriven;
        if (bIsHumanUnit != bForHuman){
            continue;
        }

        const bool bMoveRemaining = bForHuman
            ? !HumanUnitsMovedThisTurn.Contains(const_cast<ABaseUnit*>(Unit))
            : !BotUnitsMovedThisTurn.Contains(const_cast<ABaseUnit*>(Unit));
        const bool bAttackRemaining = bForHuman
            ? !HumanUnitsAttackedThisTurn.Contains(const_cast<ABaseUnit*>(Unit))
            : !BotUnitsAttackedThisTurn.Contains(const_cast<ABaseUnit*>(Unit));

        if (bMoveRemaining || bAttackRemaining){
            return true;
        }
    }

    return false;
}

/**
 * @brief Verifica se esistono ancora mosse residue per un team
 * @param bForHuman true per il team umano, false per il bot
 * @return true se almeno una unità del team può ancora muovere
 */
bool AMyGameMode::HasAnyRemainingMoveForTeam(bool bForHuman) const{
    for (const TPair<FIntPoint, ABaseUnit*>& Elem : UnitByCell){
        const ABaseUnit* Unit = Elem.Value;
        if (!Unit){
            continue;
        }

        const bool bIsHumanUnit = !Unit->IsAIDriven;
        if (bIsHumanUnit != bForHuman){
            continue;
        }

        if (bForHuman){
            if (!HumanUnitsMovedThisTurn.Contains(const_cast<ABaseUnit*>(Unit))){
                return true;
            }
        }
        else{
            if (!BotUnitsMovedThisTurn.Contains(const_cast<ABaseUnit*>(Unit))){
                return true;
            }
        }
    }

    return false;
}

/**
 * @brief Verifica se tutte le unità attive del team hanno già agito almeno una volta
 * @param bForHuman true per il team umano, false per il bot
 * @return true se tutte le unità attive hanno già compiuto un'azione nel turno corrente
 */
bool AMyGameMode::HaveAllActiveUnitsActedOnce(bool bForHuman) const{
    for (const TPair<FIntPoint, ABaseUnit*>& Elem : UnitByCell){
        const ABaseUnit* Unit = Elem.Value;
        if (!Unit){
            continue;
        }

        const bool bIsHumanUnit = !Unit->IsAIDriven;
        if (bIsHumanUnit != bForHuman){
            continue;
        }

        const bool bActedOnce = bForHuman
            ? HumanUnitsActedThisTurn.Contains(const_cast<ABaseUnit*>(Unit))
            : BotUnitsActedThisTurn.Contains(const_cast<ABaseUnit*>(Unit));
        if (!bActedOnce){
            return false;
        }
    }

    return true;
}

/**
 * @brief Verifica se tutte le unità del team hanno già mosso in questo turno
 * @param bForHuman true per il team umano, false per il bot
 * @return true se il turno di movimento è stato completato
 */
bool AMyGameMode::HaveAllUnitsMovedThisTurn(bool bForHuman) const{
    const FTeamTurnActionFlags& Flags = bForHuman ? HumanTurnActionFlags : BotTurnActionFlags;
    
    // Se una tipologia di unità non è presente, considero che ha già "mosso"
    bool bHasSniper = false;
    bool bHasBrawler = false;
    for (const TPair<FIntPoint, ABaseUnit*>& Elem : UnitByCell){
        const ABaseUnit* Unit = Elem.Value;
        if (!Unit) continue;
        
        const bool bIsHumanUnit = !Unit->IsAIDriven;
        if (bIsHumanUnit != bForHuman) continue;
        
        bHasSniper = bHasSniper || IsSniperUnit(Unit);
        bHasBrawler = bHasBrawler || IsBrawlerUnit(Unit);
    }
    
    // Entrambi devono aver mosso (o non essere presenti)
    const bool bSniperMoved = !bHasSniper || Flags.HasSniperMoved;
    const bool bBrawlerMoved = !bHasBrawler || Flags.HasBrawlerMoved;
    
    return bSniperMoved && bBrawlerMoved;
}

/**
 * @brief Verifica se il team ha interrotto volontariamente una sequenza di azioni
 * @param bForHuman true per il team umano, false per il bot
 * @return true se almeno una unità ha interrotto volontariamente il proprio ciclo
 */
bool AMyGameMode::HaveAnyUnitInterruptedVoluntarily(bool bForHuman) const{
    const FTeamTurnActionFlags& Flags = bForHuman ? HumanTurnActionFlags : BotTurnActionFlags;
    return Flags.bSniperInterruptedVoluntarily || Flags.bBrawlerInterruptedVoluntarily;
}

/**
 * @brief Verifica se il turno può auto-completarsi
 * @param bForHuman true per il team umano, false per il bot
 * @return true se il turno deve terminare automaticamente
 */
bool AMyGameMode::ShouldTurnAutoComplete(bool bForHuman) const{
    if (!HaveAllUnitsMovedThisTurn(bForHuman)){
        return false;
    }
    
    if (HaveAnyUnitInterruptedVoluntarily(bForHuman)){
        return false;
    }
    
    // Se non ci sono interruzioni volontarie e tutti hanno mosso, il turno deve finire automaticamente
    return AreTeamTurnActionFlagsComplete(bForHuman);
}

/**
 * @brief Costruisce il messaggio guida per il player umano
 * @return Messaggio di guidance contestuale da mostrare nel banner
 */
FString AMyGameMode::BuildHumanMovementGuidanceMessage() const{
    const bool bSniperMoved = HumanTurnActionFlags.HasSniperMoved;
    const bool bBrawlerMoved = HumanTurnActionFlags.HasBrawlerMoved;

    if (!bSniperMoved && !bBrawlerMoved){
        return TEXT("seleziona una tua unità e poi scegli una cella o un bersaglio");
    }

    if (bSniperMoved && !bBrawlerMoved){
        return TEXT("hai mosso lo sniper, fai un'azione anche con il brawler per continuare");
    }

    if (!bSniperMoved && bBrawlerMoved){
        return TEXT("hai mosso il brawler, fai un'azione anche con lo sniper per continuare");
    }

    if (CanHumanEndTurn()){
        return TEXT("puoi premere fine turno oppure attaccare prima di chiudere");
    }

    return TEXT("completa un'azione con ogni unità per continuare");
}

/**
 * @brief Assegna le torri alle unità del bot
 * @details Distribuisce i target torre fra le unità AI per guidare la logica di movimento verso obiettivi differenti.
 */
void AMyGameMode::BuildBotTowerAssignments(){
    BotUnitTowerTargets.Reset();
    ReservedBotTowerTargets.Reset();

    TArray<ABaseUnit*> BotUnits;
    for (const TPair<FIntPoint, ABaseUnit*>& Elem : UnitByCell){
        if (Elem.Value && Elem.Value->IsAIDriven){
            BotUnits.Add(Elem.Value);
        }
    }

    if (BotUnits.Num() == 0 || Torri.Num() == 0){
        return;
    }

    const auto ManhattanDistance = [](const FIntPoint& A, const FIntPoint& B){
        return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
    };

    struct FBotTowerAssignmentCandidate{
        ABaseUnit* Unit = nullptr;
        ATowerActor* Tower = nullptr;
        int32 PathCost = TNumericLimits<int32>::Max();
        int32 TowerDistance = TNumericLimits<int32>::Max();
    };

    const auto FindBestApproachForPair = [this, &ManhattanDistance](ABaseUnit* BotUnit, ATowerActor* Tower) -> FBotTowerAssignmentCandidate{
        FBotTowerAssignmentCandidate BestCandidate;
        if (!BotUnit || !Tower){
            return BestCandidate;
        }

        TArray<ACellActor*> ReachableCells;
        GetReachableMovementCells(BotUnit, ReachableCells);

        const FIntPoint TowerGrid = LoopCoordinateToGridCoordinate(Tower->GetGridCoord());
        for (ACellActor* CandidateCell : ReachableCells){
            if (!CandidateCell){
                continue;
            }

            TArray<FIntPoint> CandidatePath;
            int32 CandidatePathCost = TNumericLimits<int32>::Max();
            if (!BuildMovementPath(BotUnit, CandidateCell->GridCoordinate, CandidatePath, &CandidatePathCost)){
                continue;
            }

            const int32 CandidateTowerDistance = ManhattanDistance(CandidateCell->GridCoordinate, TowerGrid);
            if (!BestCandidate.Unit || CandidatePathCost < BestCandidate.PathCost ||
                (CandidatePathCost == BestCandidate.PathCost && CandidateTowerDistance < BestCandidate.TowerDistance)){
                BestCandidate.Unit = BotUnit;
                BestCandidate.Tower = Tower;
                BestCandidate.PathCost = CandidatePathCost;
                BestCandidate.TowerDistance = CandidateTowerDistance;
            }
        }

        return BestCandidate;
    };

    TArray<FBotTowerAssignmentCandidate> Candidates;
    for (ABaseUnit* BotUnit : BotUnits){
        if (!BotUnit){
            continue;
        }

        for (ATowerActor* Tower : Torri){
            const FBotTowerAssignmentCandidate Candidate = FindBestApproachForPair(BotUnit, Tower);
            if (Candidate.Unit && Candidate.Tower){
                Candidates.Add(Candidate);
            }
        }
    }

    Candidates.Sort([](const FBotTowerAssignmentCandidate& A, const FBotTowerAssignmentCandidate& B){
        if (A.PathCost != B.PathCost){
            return A.PathCost < B.PathCost;
        }

        return A.TowerDistance < B.TowerDistance;
    });

    TSet<ABaseUnit*> AssignedUnits;
    TSet<ATowerActor*> AssignedTowers;
    for (const FBotTowerAssignmentCandidate& Candidate : Candidates){
        if (!Candidate.Unit || !Candidate.Tower){
            continue;
        }

        if (AssignedUnits.Contains(Candidate.Unit) || AssignedTowers.Contains(Candidate.Tower)){
            continue;
        }

        BotUnitTowerTargets.Add(Candidate.Unit, Candidate.Tower);
        AssignedUnits.Add(Candidate.Unit);
        AssignedTowers.Add(Candidate.Tower);
        ReservedBotTowerTargets.Add(Candidate.Tower->GetGridCoord());
    }
}

/**
 * @brief Conta le torri controllate da un team
 * @param bForHuman true per il team umano, false per il bot
 * @return Numero di torri controllate dal team selezionato
 */
int32 AMyGameMode::CountPlayerTowersControlled(bool bForHuman) const{
    int32 TowerCount = 0;
    for (const ATowerActor* Tower : Torri){
        if (!Tower){
            continue;
        }

        const ETowerControlState State = Tower->GetControlState();
        const bool bTargetTeam = bForHuman
            ? (State == ETowerControlState::HUMAN)
            : (State == ETowerControlState::AI);

        if (bTargetTeam){
            TowerCount++;
        }
    }

    return TowerCount;
}

/**
 * @brief Verifica e aggiorna la condizione di vittoria
 * @param bStartingHumanTurn Indica quale team sta iniziando il proprio turno
 * @details Aggiorna i contatori di controllo consecutivo e termina la partita quando una fazione mantiene il controllo richiesto.
 */
void AMyGameMode::CheckGameWinCondition(bool bStartingHumanTurn){
    if (bGameEnded){
        return;
    }

    const int32 HumanTowers = CountPlayerTowersControlled(true);
    const int32 BotTowers = CountPlayerTowersControlled(false);

    // Il conteggio si aggiorna solo all'inizio del turno della stessa squadra:
    // cattura al turno 1 -> check 1/2 al turno 3 -> check 2/2 al turno 5.
    if (bStartingHumanTurn){
        if (HumanTowers >= 2){
            HumanConsecutiveTowerTurns++;
            AnnounceTowerControlStatus(true, HumanTowers, HumanConsecutiveTowerTurns);
        }
        else{
            HumanConsecutiveTowerTurns = 0;
        }

        if (BotTowers < 2){
            BotConsecutiveTowerTurns = 0;
        }
    }
    else{
        if (BotTowers >= 2){
            BotConsecutiveTowerTurns++;
            AnnounceTowerControlStatus(false, BotTowers, BotConsecutiveTowerTurns);
        }
        else{
            BotConsecutiveTowerTurns = 0;
        }

        if (HumanTowers < 2){
            HumanConsecutiveTowerTurns = 0;
        }
    }

    // Vittoria se 2 turni consecutivi di controllo
    if (HumanConsecutiveTowerTurns >= 2){
        EndGame(true);
    }
    else if (BotConsecutiveTowerTurns >= 2){
        EndGame(false);
    }
}

/**
 * @brief Notifica lo stato di controllo delle torri al controller
 * @param bForHuman true per il team umano, false per il bot
 * @param ControlledTowers Numero di torri controllate
 * @param ConsecutiveTurns Numero di turni consecutivi di controllo
 */
void AMyGameMode::AnnounceTowerControlStatus(bool bForHuman, int32 ControlledTowers, int32 ConsecutiveTurns){
    if (AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController())){
        PC->LogTowerControlStatus(bForHuman, ControlledTowers, ConsecutiveTurns);
    }
}

/**
 * @brief Termina la partita
 * @param bHumanWon true se il player umano ha vinto
 * @details Blocca timer e scheduling residui e mostra la schermata di fine partita.
 */
void AMyGameMode::EndGame(bool bHumanWon){
    if (bGameEnded){
        return;
    }

    bGameEnded = true;
    GetWorldTimerManager().ClearTimer(CoinTossTimerHandle);
    GetWorldTimerManager().ClearTimer(BotThinkingTimerHandle);
    GetWorldTimerManager().ClearTimer(NextTurnTimerHandle);

    AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController());
    if (PC){
        PC->ShowGameOverScreen(bHumanWon);
    }
}

// Muovo il bot con una logica semplice: a ogni unità assegno una torre e la inseguo con A*.
/**
 * @brief Esegue una mossa obiettivo del bot
 * @return true se il bot è riuscito a completare una mossa o un'azione utile
 * @details Analizza le unità AI, sceglie la migliore combinazione movimento/attacco e le indirizza verso le torri assegnate.
 */
bool AMyGameMode::ExecuteBotObjectiveMovement(){
    TArray<ABaseUnit*> BotUnits;
    for (const TPair<FIntPoint, ABaseUnit*>& Elem : UnitByCell){
        if (Elem.Value && Elem.Value->IsAIDriven){
            BotUnits.Add(Elem.Value);
        }
    }

    if (BotUnits.Num() == 0 || Torri.Num() == 0){
        return false;
    }

    if (BotUnitTowerTargets.Num() == 0){
        BuildBotTowerAssignments();
    }

    const auto ManhattanDistance = [](const FIntPoint& A, const FIntPoint& B){
        return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
    };

    struct FBotMoveChoice{
        ABaseUnit* Unit = nullptr;
        ATowerActor* Tower = nullptr;
        ACellActor* Destination = nullptr;
        int32 PathCost = TNumericLimits<int32>::Max();
        int32 TowerDistance = TNumericLimits<int32>::Max();
    };

    const auto FindBestMoveTowardsTower = [this, &ManhattanDistance](ABaseUnit* BotUnit, ATowerActor* Tower) -> FBotMoveChoice{
        FBotMoveChoice BestChoice;
        if (!BotUnit || !Tower || !CanUnitMoveThisTurn(BotUnit)){
            return BestChoice;
        }

        const FIntPoint TowerGrid = LoopCoordinateToGridCoordinate(Tower->GetGridCoord());
        TArray<ACellActor*> ReachableCells;
        GetReachableMovementCells(BotUnit, ReachableCells);

        for (ACellActor* CandidateCell : ReachableCells){
            if (!CandidateCell){
                continue;
            }

            TArray<FIntPoint> Path;
            int32 PathCost = TNumericLimits<int32>::Max();
            if (!BuildMovementPath(BotUnit, CandidateCell->GridCoordinate, Path, &PathCost)){
                continue;
            }

            const int32 DistanceToTower = ManhattanDistance(CandidateCell->GridCoordinate, TowerGrid);
            if (!BestChoice.Unit || DistanceToTower < BestChoice.TowerDistance ||
                (DistanceToTower == BestChoice.TowerDistance && PathCost < BestChoice.PathCost)){
                BestChoice.Unit = BotUnit;
                BestChoice.Tower = Tower;
                BestChoice.Destination = CandidateCell;
                BestChoice.PathCost = PathCost;
                BestChoice.TowerDistance = DistanceToTower;
            }
        }

        return BestChoice;
    };

    FBotMoveChoice BestMove;
    for (ABaseUnit* BotUnit : BotUnits){
        if (!BotUnit || !CanUnitMoveThisTurn(BotUnit)){
            continue;
        }

        ATowerActor* const* AssignedTowerPtr = BotUnitTowerTargets.Find(BotUnit);
        ATowerActor* AssignedTower = AssignedTowerPtr ? *AssignedTowerPtr : nullptr;
        if (!AssignedTower){
            continue;
        }

        const FBotMoveChoice Candidate = FindBestMoveTowardsTower(BotUnit, AssignedTower);
        if (Candidate.Unit && (!BestMove.Unit || Candidate.PathCost < BestMove.PathCost ||
            (Candidate.PathCost == BestMove.PathCost && Candidate.TowerDistance < BestMove.TowerDistance))){
            BestMove = Candidate;
        }
    }

    auto AttackWithUnit = [this](ABaseUnit* BotUnit) -> bool{
        if (!BotUnit || !CanUnitAttackThisTurn(BotUnit)){
            return false;
        }

        TArray<ABaseUnit*> AttackTargets;
        GetEnemiesInAttackRange(BotUnit, AttackTargets);
        if (AttackTargets.Num() == 0){
            return false;
        }

        FString AttackError;
        return TryAttackUnit(BotUnit, AttackTargets[0], AttackError);
    };

    if (BestMove.Unit && BestMove.Destination){
        FString MoveError;
        if (TryMoveUnit(BestMove.Unit, BestMove.Destination->GridCoordinate, MoveError)){
            AttackWithUnit(BestMove.Unit);
            NotifyBotActionResolved(BestMove.Unit);
            return true;
        }
    }

    for (ABaseUnit* BotUnit : BotUnits){
        if (AttackWithUnit(BotUnit)){
            NotifyBotActionResolved(BotUnit);
            return true;
        }
    }

    for (ABaseUnit* BotUnit : BotUnits){
        if (CanUnitMoveThisTurn(BotUnit)){
            NotifyBotActionResolved(BotUnit);
            return false;
        }
    }

    return false;
}

// Avanzo lo stato del placement in modo deterministico in base all'ordine deciso dal coin toss.
/**
 * @brief Avanza lo stato del placement
 * @details Esegue la transizione deterministica della FSM di piazzamento in base al coin toss iniziale.
 */
void AMyGameMode::AdvancePlacementState(){
    switch (CurrentState){
        case EPlacementState::CoinToss:
            CurrentState = bHumanWonToss ? EPlacementState::Human_0 : EPlacementState::AI_0;
            break;

        case EPlacementState::Human_0:
            CurrentState = bHumanWonToss ? EPlacementState::AI_0 : EPlacementState::AI_1;
            break;

        case EPlacementState::AI_0:
            CurrentState = bHumanWonToss ? EPlacementState::Human_1 : EPlacementState::Human_0;
            break;

        case EPlacementState::Human_1:
            CurrentState = bHumanWonToss ? EPlacementState::AI_1 : EPlacementState::InizializzazioneGioco;
            break;

        case EPlacementState::AI_1:
            CurrentState = bHumanWonToss ? EPlacementState::InizializzazioneGioco : EPlacementState::Human_1;
            break;

        case EPlacementState::InizializzazioneGioco:
            return;
    }
    ExecuteStateLogic();
}

// Eseguo gli effetti di stato della FSM: banner, timer e attivazione logiche automatiche.
/**
 * @brief Esegue la logica della macchina a stati
 * @details Coordina banner, timer, passaggi automatici e transizione tra placement e fase di movimento.
 */
void AMyGameMode::ExecuteStateLogic(){
    if (bGameEnded){
        return;
    }

    // se ExecuteStateLogic() è già in corso, ignoro la chiamata.
    if (bExecutingStateLogic){
        return;
    }

    bExecutingStateLogic = true;

    AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC){
        bExecutingStateLogic = false;
        return;
    }

    // Applico la desaturazione solo durante il placement umano.
    SetPlacementBoardDimmed(IsHumanPlacementTurn());

    switch (CurrentState){
        case EPlacementState::CoinToss:{
            PC->UpdateBanner(
                bHumanWonToss
                    ? TEXT("la monetaèstata lanciata - inizi tu")
                    : TEXT("la monetaèstata lanciata - inizia l'avversario"),
                FLinearColor::White
            );

            // Log del risultato del cointoss in giallo
            const FString CoinTossResult = bHumanWonToss 
                ? TEXT("LANCIO DELLA MONETA: Vince HP")
                : TEXT("LANCIO DELLA MONETA: Vince AI");
            PC->AddEntryToScrollBox(CoinTossResult, FLinearColor::Yellow);

            if (!GetWorldTimerManager().IsTimerActive(CoinTossTimerHandle)){
                GetWorldTimerManager().SetTimer(CoinTossTimerHandle, this, &AMyGameMode::AdvancePlacementState, 3.0f, false);
            }
            break;
        }

        case EPlacementState::Human_0:
        case EPlacementState::Human_1:
            PC->UpdateBanner(TEXT("seleziona un personaggio nel menu a destra e poi una cella valida"), FLinearColor::White);
            break;

        case EPlacementState::AI_0:
        case EPlacementState::AI_1:{
            PC->UpdateBanner(TEXT("l'avversario sta piazzando le unità"), FLinearColor::White);

            if (!GetWorldTimerManager().IsTimerActive(BotThinkingTimerHandle)){
                const float RandomDelay = 1.5f + FMath::FRandRange(0.0f, 1.5f);
                GetWorldTimerManager().SetTimer(BotThinkingTimerHandle, this, &AMyGameMode::ExecuteBotMove, RandomDelay, false);
            }
            break;
        }

        case EPlacementState::InizializzazioneGioco:
            if (!bMovementTurnsInitialized){
                bMovementTurnsInitialized = true;
                BeginMovementTurn(bHumanWonToss);
            }

            if (!bHumanBattleTurn && AreTeamTurnActionFlagsComplete(false)){
                BeginMovementTurn(true);
            }

            if (bHumanBattleTurn){
                PC->UpdateBanner(BuildHumanMovementGuidanceMessage(), FLinearColor::White);
            }
            else{
                if (!GetWorldTimerManager().IsTimerActive(BotThinkingTimerHandle)){
                    PC->UpdateBanner(TEXT("turno ai: il nemico sta pensando"), FLinearColor::White);
                    const float RandomDelay = 0.9f + FMath::FRandRange(0.0f, 0.8f);
                    GetWorldTimerManager().SetTimer(BotThinkingTimerHandle, this, &AMyGameMode::ExecuteBotMove, RandomDelay, false);
                }
                else{
                    PC->UpdateBanner(TEXT("turno ai: il nemico si sta avvicinando alle torri"), FLinearColor::White);
                }
            }
            break;

        default:
            break;
    }

    bExecutingStateLogic = false;
}

// Seleziono una cella casuale valida per il placement bot nel suo lato di mappa.
/**
 * @brief Restituisce una cella valida per il piazzamento del bot
 * @return Puntatore a una cella valida nel lato AI della mappa oppure nullptr
 */
ACellActor* AMyGameMode::GetRandomValidBotCell(){
    TArray<ACellActor*> ValidCells;
    for (const TPair<FIntPoint, ACellActor*>& Elem : MappaCelle){
        ACellActor* Cell = Elem.Value;
        if (!Cell){
            continue;
        }

        // Il bot piazza nelle 3 righe opposte a quelle del player umano.
        const bool bIsBotSpawnRow = Cell->GridCoordinate.X >= (DimensioneMappa - 3);
        if (!bIsBotSpawnRow){
            continue;
        }

        if (Cell->IsWalkable()){
            ValidCells.Add(Cell);
        }
    }

    if (ValidCells.Num() > 0){
        int32 RandomIndex = FMath::RandRange(0, ValidCells.Num() - 1);
        return ValidCells[RandomIndex];
    }

    return nullptr;
}

// Eseguo la mossa del bot in placement o movement, includendo scheduling via timer.
/**
 * @brief Esegue la mossa del bot
 * @details Gestisce il placement AI e, in fase movimento, coordina il rilancio del turno o nuove azioni finché il ciclo non è completo.
 */
void AMyGameMode::ExecuteBotMove(){
    if (bGameEnded){
        return;
    }

    if (IsMovementPhase()){
        const bool bMoved = ExecuteBotObjectiveMovement();

        if (!bMoved){
            if (bHumanBattleTurn){
                ExecuteStateLogic();
                return;
            }

            if (AreTeamTurnActionFlagsComplete(false) && !bHumanBattleTurn){
                BeginMovementTurn(true);
                ExecuteStateLogic();
                return;
            }

            GetWorldTimerManager().ClearTimer(BotThinkingTimerHandle);
            GetWorldTimerManager().SetTimer(BotThinkingTimerHandle, this, &AMyGameMode::ExecuteBotMove, 0.35f, false);
            return;
        }

        if (bHumanBattleTurn){
            ExecuteStateLogic();
            return;
        }

        if (!AreTeamTurnActionFlagsComplete(false)){
            GetWorldTimerManager().ClearTimer(BotThinkingTimerHandle);
            GetWorldTimerManager().SetTimer(BotThinkingTimerHandle, this, &AMyGameMode::ExecuteBotMove, 0.45f, false);
            return;
        }

        BeginMovementTurn(true);
        ExecuteStateLogic();
        return;
    }

    // In placement il bot deve piazzare esattamente 1 brawler (Sith) e 1 sniper (Stormtrooper).
    TArray<TSubclassOf<ABaseUnit>> MissingBotClasses;
    if (!Tracker.bSithPlaced && SithClass){
        MissingBotClasses.Add(SithClass);
    }
    if (!Tracker.bStormtrooperPlaced && StormtrooperClass){
        MissingBotClasses.Add(StormtrooperClass);
    }

    // Se mancano entrambe le classi, ne scelgo una a caso; al turno successivo verrà forzata l'altra.
    BotUnitClass = MissingBotClasses.Num() > 0
        ? MissingBotClasses[FMath::RandRange(0, MissingBotClasses.Num() - 1)]
        : nullptr;

    // Cerco una cella valida nella zona di spawn bot.
    ACellActor* TargetCell = GetRandomValidBotCell();

    // Spawno l'unità e la registro nelle strutture logiche.
    if (TargetCell && BotUnitClass){
        FVector SpawnLocation = TargetCell->GetActorLocation() + FVector(0.f, 0.f, 50.f);
        ABaseUnit* SpawnedUnit = GetWorld()->SpawnActor<ABaseUnit>(BotUnitClass, SpawnLocation, FRotator::ZeroRotator);

        if (SpawnedUnit){
            RegisterSpawnedUnit(SpawnedUnit, TargetCell);

            // Blocco esplicitamente i duplicati classe: se la registrazione fallisce, annullo lo spawn.
            if (!RegisterUnitPlacement(BotUnitClass)){
                const FIntPoint LoopCoord = GridCoordinateToLoopCoordinate(TargetCell->GridCoordinate);
                UnitByCell.Remove(LoopCoord);
                TargetCell->bHasUnit = false;
                SpawnedUnit->Destroy();
            }
            else if (AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController())){
                PC->LogSpawn(SpawnedUnit);
            }
        }
    }
    else{
        // Mantengo log separati per capire subito il punto di fallimento.
        if (!TargetCell) UE_LOG(LogTemp, Error, TEXT("BOT FAIL: Cella non trovata nelle coordinate specificate!"));
        if (!BotUnitClass) UE_LOG(LogTemp, Error, TEXT("BOT FAIL: Classe SithClass o StormtrooperClass non assegnata!"));
    }

    // Avanzo sempre lo stato per evitare stalli della FSM.
    if (!GetWorldTimerManager().IsTimerActive(NextTurnTimerHandle)){
        GetWorldTimerManager().SetTimer(NextTurnTimerHandle, this, &AMyGameMode::AdvancePlacementState, 1.0f, false);
    }
}
