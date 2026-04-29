/**
 * @file Camera.h
 * @author Pietro Malerba
 * @brief Dichiarazione della camera fissa di gioco
 * @details In questo file dichiaro la camera fissa usata per osservare la griglia dall'alto.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camera.generated.h"

/**
 * @class ACamera
 * @brief Camera fissa del progetto
 * @details Mantiene una visuale top-down stabile sopra la griglia di gioco.
 */
UCLASS()
class PROGETTOMALERBA_API ACamera : public AActor{
	GENERATED_BODY()
	
public:	
	// Inizializzo il setup della camera top-down.
	ACamera();

protected:
	// In BeginPlay mantengo il comportamento base del parent.
	virtual void BeginPlay() override;
    UPROPERTY(VisibleAnywhere)
    class UCameraComponent* CameraComponent;

public:	
	// In Tick mantengo il comportamento base del parent.
	virtual void Tick(float DeltaTime) override;

};
