/**
 * @file CellActor.cpp
 * @author Pietro Malerba
 * @brief Implementazione della classe ACellActor
 * @details In questo file implemento ACellActor, la singola cella della griglia di gioco.
 */

// Includo i file necessari.
#include "CellActor.h"
#include "MyPlayerController.h"
#include "MyGameMode.h"
#include "Kismet/GameplayStatics.h"

/**
 * @brief Costruttore di ACellActor
 * @details Inizializza mesh, collisioni, materiale di default e altezza iniziale della cella.
 */
ACellActor::ACellActor(){
    // Creo e configuro il componente mesh.
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = MeshComp;
    
    // Configuro le collisioni della mesh.
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComp->SetCollisionResponseToAllChannels(ECR_Block);

    // Carico la mesh del cubo.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (MeshAsset.Succeeded()){
        MeshComp->SetStaticMesh(MeshAsset.Object);
    }

    // Carico il materiale personalizzato per le celle.
    static ConstructorHelpers::FObjectFinder<UMaterial> MaterialAsset(TEXT("/Game/CellMaterial.CellMaterial"));
    if (MaterialAsset.Succeeded()){
        MeshComp->SetMaterial(0, MaterialAsset.Object);
    }

    // Imposto una scala più piatta della mesh.
    MeshComp->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.02f));

    // Inizializzo le variabili.
    Altezza = 0;
}

/**
 * @brief Ricava il colore del terreno in base all'altezza
 * @return Colore base associato al valore di Altezza
 */
FLinearColor ACellActor::GetTerrainColor() const{
    switch (Altezza){
        case 0:
            return FLinearColor(FColor::FromHex(TEXT("#5D7B93")));
        case 1:
            return FLinearColor(FColor::FromHex(TEXT("#8BA888")));
        case 2:
            return FLinearColor(FColor::FromHex(TEXT("#C9B67E")));
        case 3:
            return FLinearColor(FColor::FromHex(TEXT("#C98B6D")));
        case 4:
            return FLinearColor(FColor::FromHex(TEXT("#A66363")));
        default:
            return FLinearColor(0.05f, 0.05f, 0.05f);
    }
}

/**
 * @brief Aggiorna lo stato visivo della cella
 * @details Centralizza la logica di colore base, desaturazione di placement e highlight di movimento o attacco.
 */
void ACellActor::RefreshVisualState(){
    if (!MeshComp) return;

    UMaterialInstanceDynamic* DynMaterial = Cast<UMaterialInstanceDynamic>(MeshComp->GetMaterial(0));
    if (DynMaterial){
        FLinearColor TargetColor = ColoreOriginale;

        // In placement desaturo solo le celle non disponibili.
        if (bIsPlacementDimmed){
            TargetColor = FLinearColor::LerpUsingHSV(TargetColor, FLinearColor(0.35f, 0.35f, 0.35f), 0.55f);
        }

        // L'highlight di nemici attaccabili ha priorità massima (rosso).
        if (bIsEnemyHighlighted){
            TargetColor = FLinearColor(FColor::FromHex(TEXT("#FF3333")));
        }
        // L'highlight di movimento ha priorità sul colore base/desaturato.
        else if (bIsHighlighted){
            TargetColor = FLinearColor(FColor::FromHex(TEXT("#8B5CF6")));
        }

        DynMaterial->SetVectorParameterValue(FName("Color"), TargetColor);
        DynMaterial->SetScalarParameterValue(FName("IsHighlighted"), (bIsHighlighted || bIsEnemyHighlighted) ? 1.0f : 0.0f);
    }
}

/**
 * @brief Inizializza il comportamento runtime della cella
 * @details Prepara il materiale dinamico della cella e registra il callback di click.
 */
void ACellActor::BeginPlay(){
    Super::BeginPlay();

    AggiornaColore();

    OnClicked.AddDynamic(this, &ACellActor::OnCellClicked);
}

/**
 * @brief Aggiorna il colore base della cella
 * @details Salva il colore del terreno come baseline da usare durante i cambi di stato visuale.
 */
void ACellActor::AggiornaColore(){
    if (!MeshComp) return;

    BaseTerrainColor = GetTerrainColor();
    ColoreOriginale = BaseTerrainColor;

    UMaterialInstanceDynamic* DynMaterial = MeshComp->CreateDynamicMaterialInstance(0);
    if (DynMaterial){
        RefreshVisualState();
    }
}

/**
 * @brief Attiva o disattiva la desaturazione di placement
 * @param bEnable Nuovo stato della desaturazione
 * @details Espone un toggle semplice: lo stato viene poi renderizzato in RefreshVisualState.
 */
void ACellActor::SetPlacementDimmed(bool bEnable){
    bIsPlacementDimmed = bEnable;

    RefreshVisualState();
}

/**
 * @brief Gestisce il click sulla cella
 * @param TouchedActor Attore toccato dal click
 * @param ButtonPressed Bottone del mouse premuto
 * @details Instrada il click verso il flusso di placement o movement in base allo stato corrente del GameMode.
 */
void ACellActor::OnCellClicked(AActor* TouchedActor, FKey ButtonPressed){
    AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController());
    if (PC){
        AMyGameMode* GM = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(this));

        // In movement phase uso sempre il flow di HandleCellClick, che gestisce
        // selezione, cambio unità, movimento e attacco in modo coerente.
        if (GM && GM->IsMovementPhase()){
            PC->HandleCellClick(this);
        }
        else {
            // Altrimenti considero il click nel contesto placement/spawn.
            PC->SpawnUnit(this);
        }
    }
}

/**
 * @brief Indica se la cella è calpestabile
 * @return true se la cella può essere attraversata, false altrimenti
 */
bool ACellActor::IsWalkable() const{    
    return (Altezza > 0) && !bHasTower && !bHasTowers && !bHasUnit;
}

/**
 * @brief Evidenzia o deseleziona la cella per il movimento
 * @param bEnable Nuovo stato di highlight
 */
void ACellActor::SetHighlight(bool bEnable){
    bIsHighlighted = bEnable;
    RefreshVisualState();
}

/**
 * @brief Evidenzia la cella come bersaglio attaccabile
 * @param bEnable Nuovo stato dell'evidenziazione nemico
 */
void ACellActor::SetEnemyHighlight(bool bEnable){
    bIsEnemyHighlighted = bEnable;
    RefreshVisualState();
}