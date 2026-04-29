/**
 * @file TowerActor.cpp
 * @author Pietro Malerba
 * @brief Implementazione dell'attore torre
 * @details In questo file implemento l'actor torre usato sulla griglia.
 */

#include "TowerActor.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

/**
 * @brief Costruttore della torre
 * @details Inizializza mesh, materiali e stato visivo della torre senza tick runtime.
 */
ATowerActor::ATowerActor(){
    PrimaryActorTick.bCanEverTick = false;

    StickerPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StickerPlane"));
    RootComponent = StickerPlane;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (PlaneMesh.Succeeded()){
        StickerPlane->SetStaticMesh(PlaneMesh.Object);
    }

    StickerPlane->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.0f));
    StickerPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> NeutralMat(TEXT("/Game/Materiali/torri/M_TorreNeutra.M_TorreNeutra"));
    if (NeutralMat.Succeeded()){
        NeutralMaterial = NeutralMat.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ContestedMat(TEXT("/Game/Materiali/torri/M_TorreContesa.M_TorreContesa"));
    if (ContestedMat.Succeeded()){
        ContestedMaterial = ContestedMat.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> HumanMat(TEXT("/Game/Materiali/torri/M_TorreVerde.M_TorreVerde"));
    if (HumanMat.Succeeded()){
        HumanMaterial = HumanMat.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> AIMat(TEXT("/Game/Materiali/torri/M_TorreRossa.M_TorreRossa"));
    if (AIMat.Succeeded()){
        AIMaterial = AIMat.Object;
    }

    ApplyControlStateVisuals();
}

/**
 * @brief BeginPlay della torre
 * @details Non aggiunge logica extra oltre all'applicazione delle visuali del controllo.
 */
void ATowerActor::BeginPlay(){
	Super::BeginPlay();

    ApplyControlStateVisuals();
}

void ATowerActor::SetGridCoord(FIntPoint Coord){
    // Mantengo la coordinata separata dal world-space per semplificare la logica gameplay.
    GridCoord = Coord;
}

FIntPoint ATowerActor::GetGridCoord() const{
    // Restituisco la coordinata logica usata dal GameMode.
    return GridCoord;
}

void ATowerActor::SetControlState(ETowerControlState NewState){
    if (ControlState == NewState){
        return;
    }

    ControlState = NewState;
    ApplyControlStateVisuals();
}

ETowerControlState ATowerActor::GetControlState() const{
    return ControlState;
}

void ATowerActor::ApplyControlStateVisuals(){
    if (!StickerPlane){
        return;
    }

    UMaterialInterface* TargetMaterial = NeutralMaterial;
    switch (ControlState){
        case ETowerControlState::HUMAN:
            TargetMaterial = HumanMaterial ? HumanMaterial : NeutralMaterial;
            break;
        case ETowerControlState::AI:
            TargetMaterial = AIMaterial ? AIMaterial : NeutralMaterial;
            break;
        case ETowerControlState::CONTESTED:
            TargetMaterial = ContestedMaterial ? ContestedMaterial : NeutralMaterial;
            break;
        case ETowerControlState::NEUTRAL:
        default:
            TargetMaterial = NeutralMaterial;
            break;
    }

    if (TargetMaterial){
        StickerPlane->SetMaterial(0, TargetMaterial);
    }
}

