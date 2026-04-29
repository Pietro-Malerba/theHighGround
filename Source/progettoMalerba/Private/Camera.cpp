/**
 * @file Camera.cpp
 * @author Pietro Malerba
 * @brief Implementazione della camera fissa di gioco
 * @details In questo file implemento una camera fissa sopra la griglia di gioco.
 */

// Includo il file header corrispondente.
#include "Camera.h"
#include "Camera/CameraComponent.h"

/**
 * @brief Costruttore della camera fissa
 * @details Inizializza il componente camera e imposta posizione e rotazione fisse.
 */
ACamera::ACamera(){
    // Disabilito il tick perchè non mi serve.
    PrimaryActorTick.bCanEverTick = false;

    // Creo il componente camera.
    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
    RootComponent = CameraComponent;

    // Imposto la posizione fissa.
    SetActorLocation(FVector(1250.f, 1250.f, 2000.f));

    // Imposto la rotazione verso il basso.
    SetActorRotation(FRotator(-90.f, 0.f, 0.f));
}

/**
 * @brief BeginPlay della camera fissa
 * @details Non aggiunge logica extra rispetto al comportamento base.
 */
void ACamera::BeginPlay(){
	Super::BeginPlay();
}

/**
 * @brief Tick della camera fissa
 * @details Mantiene il comportamento base del parent.
 */
void ACamera::Tick(float DeltaTime){
	Super::Tick(DeltaTime);
}

