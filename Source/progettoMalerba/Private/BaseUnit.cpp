/**
 * @file BaseUnit.cpp
 * @author Pietro Malerba
 * @brief Implementazione della classe ABaseUnit
 * @details In questo file implemento la classe ABaseUnit, che rappresenta la base astratta per tutte le unità di gioco.
 */


#include "BaseUnit.h"
#include "MyPlayerController.h"
#include "Sniper.h"
#include "Brawler.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

/**
 * @brief Costruttore della classe ABaseUnit
 * @details Inizializza i membri della classe ABaseUnit, crea il componente StaticMesh per lo sticker dell'unità e carica la mesh e il materiale di default.
 */
ABaseUnit::ABaseUnit(){
    // Disabilito il tick perchè non mi serve per unità statiche
    PrimaryActorTick.bCanEverTick = false;

    // Creo il piano per lo sticker dell'unità
    TokenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TokenMesh"));
    RootComponent = TokenMesh;

    // Carico la mesh Plane di default
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (PlaneMeshAsset.Succeeded()){
        TokenMesh->SetStaticMesh(PlaneMeshAsset.Object);
    }

    // Carico il materiale custom M_TokenMaster
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MasterMat(TEXT("/Game/M_TokenMaster.M_TokenMaster"));
    if (MasterMat.Succeeded()){
        TokenMesh->SetMaterial(0, MasterMat.Object);
    }

    // Imposto lo sticker in orizzontale
    TokenMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.0f));
    TokenMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

/**
 * @brief Inizializza il materiale dinamico dello sticker dell'unità
 * @details In BeginPlay creo l'istanza dinamica del materiale base e applico la texture specifica assegnata dal Blueprint.
 */
void ABaseUnit::BeginPlay(){
    Super::BeginPlay();

    if (TokenMesh){
        UMaterialInterface* BaseMat = TokenMesh->GetMaterial(0);
        if (BaseMat){
            // Creo l'istanza dinamica.
            DynamicTokenMaterial = TokenMesh->CreateDynamicMaterialInstance(0, BaseMat);

            // Applico la texture specifica.
            if (DynamicTokenMaterial && UnitTokenTexture){
                DynamicTokenMaterial->SetTextureParameterValue(FName("TokenTexture"), UnitTokenTexture);
            }
        }
    }
}

/**
 * @brief Applica danno all'unità
 * @param Amount Quantità di danno da sottrarre agli HP correnti
 * @details Riduce gli HP dell'unità e, se arrivano a zero o sotto, forza la transizione nello stato di KO.
 */
void ABaseUnit::TakeDamageUnit(int32 Amount){
    // Decremento la salute.
    Health -= Amount;

    // Controllo se l'unitàèmorta e attivo la morte.
    if (Health <= 0){
        Health = 0;
        Die(nullptr);
    }
}

/**
 * @brief Gestisce lo stato di morte o respawn dell'unità
 * @param GameMode Puntatore al GameMode che può richiedere il respawn
 * @details Se il GameMode non è disponibile l'unità rimane in KO; se lo spawn iniziale è stato registrato, l'unità torna alla coordinata di spawn con HP ripristinati.
 */
void ABaseUnit::Die(AMyGameMode* GameMode){
    // Segno il KO locale.
    Health = 0;

    // Se non arriva il GameMode, rimango in stato KO e non forzo alcun respawn.
    if (!GameMode){
        UE_LOG(LogTemp, Log, TEXT("KO: %s in attesa di respawn"), *GetName());
        return;
    }

    if (!bHasInitialSpawnPosition){
        UE_LOG(LogTemp, Warning, TEXT("RESPAWN SKIPPED: %s non ha spawn iniziale"), *GetName());
        return;
    }

    MaxHealth = FMath::Max(1, MaxHealth);
    Health = MaxHealth;
    GridPosition = InitialSpawnGridPosition;

    UE_LOG(LogTemp, Log, TEXT("RESPAWN: %s torna a %s con %d HP"), *GetName(), *GridPosition.ToString(), Health);
}

/**
 * @brief Memorizza i dati di spawn iniziale se non sono già presenti
 * @param SpawnCoord Coordinata di spawn da registrare
 * @details Salva una sola volta la posizione iniziale e fissa anche il massimo HP di riferimento usato per i respawn.
 */
void ABaseUnit::InitializeSpawnDataIfNeeded(const FIntPoint& SpawnCoord){
    if (bHasInitialSpawnPosition){
        return;
    }

    InitialSpawnGridPosition = SpawnCoord;
    bHasInitialSpawnPosition = true;
    MaxHealth = FMath::Max(1, Health);
}

/**
 * @brief Aggiorna il materiale dinamico anche in fase di construction
 * @param Transform Trasformazione corrente dell'attore
 * @details Mantiene coerente lo sticker dell'unità in editor e a runtime quando cambia la texture assegnata.
 */
void ABaseUnit::OnConstruction(const FTransform& Transform){
    Super::OnConstruction(Transform);

    if (TokenMesh && UnitTokenTexture){
        // Se manca l'istanza dinamica, la creo.
        if (!DynamicTokenMaterial){
            UMaterialInterface* BaseMat = TokenMesh->GetMaterial(0);
            if (BaseMat){
                DynamicTokenMaterial = TokenMesh->CreateDynamicMaterialInstance(0, BaseMat);
            }
        }

        // Assegno la texture corrente.
        if (DynamicTokenMaterial){
            DynamicTokenMaterial->SetTextureParameterValue(FName("TokenTexture"), UnitTokenTexture);
        }
    }
}

/**
 * @brief Calcola il danno casuale dell'unità
 * @return Valore di danno compreso tra MinDamage e MaxDamage
 */
int32 ABaseUnit::CalculateDamage(){
    int32 DamageDealt = FMath::RandRange(MinDamage, MaxDamage);
    return DamageDealt;
}

/**
 * @brief Esegue un attacco contro un bersaglio
 * @param TargetUnit Unità bersaglio dell'attacco
 * @details Applica il danno generato dalle statistiche correnti, registra il log tramite PlayerController e gestisce il contrattacco per le unità sniper quando le condizioni sono soddisfatte.
 */
void ABaseUnit::AttackUnit(ABaseUnit* TargetUnit) {
    if (!TargetUnit) return;

    int32 DamageDealt = CalculateDamage();
    TargetUnit->TakeDamageUnit(DamageDealt);

    AMyPlayerController* PC = Cast<AMyPlayerController>(GetWorld()->GetFirstPlayerController());
    if (PC) {
        // Log attacco normale
        PC->LogAction(this, TargetUnit, DamageDealt, false);

        // Calcolo contrattacco 
        if (this->IsA(ASniper::StaticClass())) {
            float Distance = FVector::Dist(this->GetActorLocation(), TargetUnit->GetActorLocation()) / 100.0f;
            
            if (TargetUnit->IsA(ASniper::StaticClass()) || 
               (TargetUnit->IsA(ABrawler::StaticClass()) && Distance <= 1.5f)) {
                
                int32 CounterDamage = FMath::RandRange(1, 2);
                this->TakeDamageUnit(CounterDamage);
                
                // Log contrattacco
                PC->LogAction(TargetUnit, this, CounterDamage, true);
            }
        }
    }
}