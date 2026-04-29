/**
 * @file PerlinNoiseSettingsSubsystem.h
 * @author Pietro Malerba
 * @brief Dichiarazione del subsystem per le impostazioni Perlin
 * @details In questo file salvo le impostazioni Perlin scelte nel menu e le rendo disponibili tra i livelli.
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PerlinNoiseSettingsSubsystem.generated.h"

/**
 * @class UPerlinNoiseSettingsSubsystem
 * @brief Subsystem persistente per le impostazioni del menu
 * @details Conserva la scala Perlin e la selezione del pathfinding tra un livello e l'altro.
 */
UCLASS()
class PROGETTOMALERBA_API UPerlinNoiseSettingsSubsystem : public UGameInstanceSubsystem{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Perlin")
	float PendingNoiseScale = 0.12f;

	UPROPERTY(BlueprintReadWrite, Category = "Pathfinding")
	bool bUseAStar = true;

	UPROPERTY(BlueprintReadWrite, Category = "Pathfinding")
	bool bUseGreedyBestFirst = false;

	UFUNCTION(BlueprintCallable, Category = "Perlin")
	void SetPendingNoiseScale(float InNoiseScale);

	UFUNCTION(BlueprintCallable, Category = "Perlin")
	float GetPendingNoiseScale() const;

	UFUNCTION(BlueprintCallable, Category = "Pathfinding")
	void SetPathfindingSelection(bool bInUseAStar, bool bInUseGreedyBestFirst);

	UFUNCTION(BlueprintCallable, Category = "Pathfinding")
	bool GetUseAStar() const;

	UFUNCTION(BlueprintCallable, Category = "Pathfinding")
	bool GetUseGreedyBestFirst() const;
};