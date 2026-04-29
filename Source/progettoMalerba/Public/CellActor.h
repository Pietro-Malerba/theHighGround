/**
 * @file CellActor.h
 * @author Pietro Malerba
 * @brief Dichiarazione della cella della griglia
 * @details In questo file dichiaro la cella della griglia con stato logico e stato visivo.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CellActor.generated.h"

/**
 * @class ACellActor
 * @brief Cella della griglia di gioco
 * @details Rappresenta una singola cella con altezza, occupazione, stato visivo e gestione dei click.
 */
UCLASS()
class PROGETTOMALERBA_API ACellActor : public AActor{
    GENERATED_BODY()
    
    public:
        // Traccio la presenza di una torre.
        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cella")
        bool bHasTower = false;

        // Traccio la presenza di un'unità.
        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cella")
        bool bHasUnit = false;

        // Verifico se la cella è calpestabile.
        UFUNCTION(BlueprintCallable, Category = "Cella")
        bool IsWalkable() const;
    
        UPROPERTY(VisibleAnywhere, Category = "Cella")
        FIntPoint GridCoordinate;
        
        
        ACellActor();

        UPROPERTY(VisibleAnywhere)
        UStaticMeshComponent* MeshComp;
        bool bHasTowers = false;

        // Traccio il valore dell'altezza (0-4).
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cella")
        int32 Altezza;

        // Aggiorno il colore in base all'altezza.
        void AggiornaColore();

        // Attivo o disattivo la desaturazione usata durante il placement del player.
        void SetPlacementDimmed(bool bEnable);

        // Evidenzio la cella per il movimento.
        void SetHighlight(bool bEnable);

        // Evidenzio la cella per indicare un nemico attaccabile (colore rosso).
        void SetEnemyHighlight(bool bEnable);
        
    protected:
        virtual void BeginPlay() override;

        // Chiamo questa funzione quando clicco sulla cella.
        UFUNCTION()
        void OnCellClicked(AActor* TouchedActor, FKey ButtonPressed);

    private:
        FLinearColor GetTerrainColor() const;
        void RefreshVisualState();

        // Memorizzo il colore originale per ripristinarlo.
        FLinearColor ColoreOriginale;
        FLinearColor BaseTerrainColor;
        bool bIsHighlighted = false;
        bool bIsPlacementDimmed = false;
        bool bIsEnemyHighlighted = false;
};
