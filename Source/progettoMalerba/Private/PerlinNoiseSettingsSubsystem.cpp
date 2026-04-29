/**
 * @file PerlinNoiseSettingsSubsystem.cpp
 * @author Pietro Malerba
 * @brief Implementazione del subsystem per le impostazioni Perlin
 * @details In questo file implemento il contenitore persistente del valore Perlin scelto nel menu.
 */

#include "PerlinNoiseSettingsSubsystem.h"

/**
 * @brief Imposta la scala del rumore Perlin in modo clampato
 * @param InNoiseScale Nuovo valore della scala di rumore
 */
void UPerlinNoiseSettingsSubsystem::SetPendingNoiseScale(float InNoiseScale){
	PendingNoiseScale = FMath::Clamp(InNoiseScale, 0.01f, 0.30f);
}

/**
 * @brief Restituisce la scala del rumore Perlin in attesa di applicazione
 * @return Scala di rumore persistita nel subsystem
 */
float UPerlinNoiseSettingsSubsystem::GetPendingNoiseScale() const{
	return PendingNoiseScale;
}

/**
 * @brief Salva la selezione del pathfinding
 * @param bInUseAStar Indica se A* è attivo
 * @param bInUseGreedyBestFirst Indica se Greedy Best-First è attivo
 */
void UPerlinNoiseSettingsSubsystem::SetPathfindingSelection(bool bInUseAStar, bool bInUseGreedyBestFirst){
	bUseAStar = bInUseAStar;
	bUseGreedyBestFirst = bInUseGreedyBestFirst;
}

/**
 * @brief Indica se A* è selezionato
 * @return true se A* è attivo
 */
bool UPerlinNoiseSettingsSubsystem::GetUseAStar() const{
	return bUseAStar;
}

/**
 * @brief Indica se Greedy Best-First è selezionato
 * @return true se Greedy Best-First è attivo
 */
bool UPerlinNoiseSettingsSubsystem::GetUseGreedyBestFirst() const{
	return bUseGreedyBestFirst;
}