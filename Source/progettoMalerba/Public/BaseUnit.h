/** 
 * @file BaseUnit.h
 * @author Pietro Malerba
 * @brief Dichiarazione della classe ABaseUnit
 * @details In questo file dichiaro la base astratta delle unità di gioco.
 */


#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BaseUnit.generated.h"

class AMyGameMode;

/** 
 * @class ABaseUnit
 * @brief Classe astratta per le unità di gioco
 * @details Rappresenta la base per tutte le unità di gioco nel gioco.
 */
UCLASS(Abstract)
class PROGETTOMALERBA_API ABaseUnit : public APawn{
    GENERATED_BODY()

public:
    ABaseUnit();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats")
    int32 Health;           ///< HP attuali dell'unità

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats")
    int32 MaxMovement;      ///< Range massimo di movimento [in celle]

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats")
    int32 AttackRange;      ///< Raggio di attacco [in celle]

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats")
    int32 MinDamage;        ///< Danno minimo per il calcolo del danno casuale.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats")
    int32 MaxDamage;        ///< Danno massimo per il calcolo del danno casuale.

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit Stats")
    int32 MaxHealth;        ///< HP massimi, usati per il respawn e per il controllo di morte.

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    FIntPoint GridPosition; ///< Posizione attuale sulla griglia

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    FIntPoint InitialSpawnGridPosition; ///< Coordinata di spawn iniziale

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    bool bHasInitialSpawnPosition = false; ///< Indica se è stata definita una posizione di spawn iniziale

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
    bool IsAIDriven;        ///< Indica se l'unità è controllata dall'IA o da un giocatore umano

    /**
     * @brief Funzione per ricevere danni
     * @param Amount Quantità di danni da applicare
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void TakeDamageUnit(int32 Amount);

    /**
     * @brief Funzione di morte
     * @param GameMode Puntatore al game mode
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void Die(AMyGameMode* GameMode = nullptr);

    /**
     * @brief Funzione per calcolare il danno inferto
     * @return Quantità di danno calcolata
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    int32 CalculateDamage();

    /**
     * @brief Funzione di attacco verso un'unità avversaria
     * @param TargetUnit Puntatore all'unità bersaglio
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void AttackUnit(ABaseUnit* TargetUnit);

    /**
    * @brief Funzione per inizializzare i dati di spawn se necessario
    * @param SpawnCoord Coordinata di spawn da utilizzare se non è già stata definita
    */
    UFUNCTION(BlueprintCallable, Category = "Spawn")
    void InitializeSpawnDataIfNeeded(const FIntPoint& SpawnCoord);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graphics", meta = (AllowPrivateAccess = "true"))
    class UStaticMeshComponent* TokenMesh;  ///< Componente StaticMesh per lo sticker dell'unità

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics")
    UTexture2D* UnitTokenTexture;           ///< Texture specifica per lo sticker dell'unità, assegnata dal Blueprint
    
    /**
     * @brief Funzione chiamata durante la costruzione dell'unità
     * @param Transform Trasformazione da applicare
     */
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY()
    class UMaterialInstanceDynamic* DynamicTokenMaterial;   ///< Materiale dinamico per applicare la texture specifica allo sticker

    /**
     * @brief Funzione chiamata all'inizio della partita
     */
    virtual void BeginPlay() override;
};
