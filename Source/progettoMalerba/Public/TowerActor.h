/**
 * @file TowerActor.h
 * @author Pietro Malerba
 * @brief Dichiarazione dell'attore torre
 * @details In questo file dichiaro l'attore torre e la sua coordinata logica in griglia.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "TowerActor.generated.h"

/**
 * @brief Stati di controllo possibili per una torre
 */
UENUM(BlueprintType)
enum class ETowerControlState : uint8 {
    NEUTRAL = 0,  // Nessuno controlla la torre
    HUMAN = 1,    // Controllata dal player umano
    AI = 2,       // Controllata dal player AI
    CONTESTED = 3 // Presenza di entrambi i team nella zona di cattura
};

/**
 * @class ATowerActor
 * @brief Attore torre della griglia
 * @details Gestisce coordinata logica, stato di controllo e materiali visivi della torre.
 */
UCLASS()
class PROGETTOMALERBA_API ATowerActor : public AActor{
    GENERATED_BODY()
    
public:
    // Inizializzo la geometria visuale della torre.
    ATowerActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grafica")
    class UStaticMeshComponent* StickerPlane;

    UPROPERTY(Transient)
    class UMaterialInterface* NeutralMaterial;

    UPROPERTY(Transient)
    class UMaterialInterface* ContestedMaterial;

    UPROPERTY(Transient)
    class UMaterialInterface* HumanMaterial;

    UPROPERTY(Transient)
    class UMaterialInterface* AIMaterial;

    UPROPERTY(VisibleAnywhere, Category = "Logica")
    FIntPoint GridCoord;

    // Traccia lo stato di controllo della torre
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Logica")
    ETowerControlState ControlState = ETowerControlState::NEUTRAL;

    // Aggiorno la coordinata logica della torre.
    void SetGridCoord(FIntPoint Coord);

    // Espongo la coordinata logica corrente.
    FIntPoint GetGridCoord() const;

    // Cambio lo stato di controllo della torre
    void SetControlState(ETowerControlState NewState);

    // Restituisco lo stato di controllo
    ETowerControlState GetControlState() const;

protected:
    virtual void BeginPlay() override;

private:
    void ApplyControlStateVisuals();
};
