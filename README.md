@mainpage

# The High Ground
Progetto di Pietro Malerba (matricola S5839759) per l'insegnamento di Progettazione e Analisi di Algoritmi.

## Panoramica
"The High Ground" è un gioco strategico a turni ambientato nell'universo di Star Wars. Il giocatore controlla un Jedi e un Rebel, mentre l'AI controlla un Sith e uno Stormtrooper. La partita si svolge su una griglia procedurale 25x25 con celle di altezze diverse, ostacoli naturali e tre torri centrali da conquistare.

L'obiettivo non è eliminare semplicemente le unità avversarie, ma mantenere il controllo di almeno due torri per due turni consecutivi. Il match è quindi costruito intorno a tre livelli di strategia: posizionamento iniziale, gestione del movimento e controllo del terreno elevato.

## Requisiti Di Progetto
1. Il progetto compila correttamente, il codice è ben commentato e ben strutturato.
    - Il codice è commentato in modo ordinato utilizzando Doxygen, la stuttura del progetto è chiarita nella [documentazione](https://pietro-malerba.github.io/progettoMalerba/index.html).
    - I file sono ordinati dall'engine in fase di generazione delle classi nelle cartelle [Public](Source/progettoMalerba/Public) e [Private](Source/progettoMalerba/Private) rispettivamente per le intestazioni e le implementazioni.

2. Griglia di gioco iniziale graficamente corretta e interamente visibile nello schermo.
    - La griglia di gioco è rappresentata da attori [`ACellActor`](Source/progettoMalerba/Public/CellActor.h) che visualizzano correttamente le altezze del terreno e sono posizionati in modo da essere completamente visibili da [`ACameraActor`](Source/progettoMalerba/Public/CameraActor.h) fissa in alto con una prospettiva ortografica.
    - La griglia è generata proceduralmente con Perlin Noise dalla funzione `GeneraGriglia()` nel file [MyGameMode.cpp](Source/progettoMalerba/Private/MyGameMode.cpp) e ogni cella è colorata in base alla sua altezza, con un materiale dinamico che permette di evidenziare le celle raggiungibili o attaccabili durante il turno.

3. Meccanismo di posizionamento Unità di Gioco e Torri come da specifiche.
    - Le torri sono rappresentate da attori [`ATowerActor`](Source/progettoMalerba/Public/TowerActor.h) posizionati al centro della griglia da un algoritmo di piazzamento simmetrico adattivo che, avendo come pivot il centro della mappa (cella 12,12 -> L12), posiziona le torri in modo da garantire un equilibrio strategico.
    - La connettività della griglia è verificata 2 volte con un algoritmo di flood fill: una volta dopo la generazione del terreno e una volta dopo il posizionamento delle torri, per assicurare che tutte le aree siano raggiungibili e che le torri non blocchino l'accesso a parti della mappa.
    - Il piazzamento iniziale delle unità segue una logica a turni stabilita da un Coin Toss virtuale il cui esito è, tra le altre cose, registrato nel log delle azioni.

4. AI che utilizza algoritmo A*.
    - Nel file [MyGameMode.cpp](Source/progettoMalerba/Private/MyGameMode.cpp) la funzione `BuildMovementPath()` implementa l'algoritmo A* usando come euristica la distanza di Manhattan, e tiene conto di ostacoli, differenze di altezza e occupazione delle celle per costruire un percorso valido entro il budget di movimento dell'unità.

5. Il gioco funziona a turni e termina quando un giocatore vince.
    - Il ciclo di gioco è gestito da una FSM implementata nella funzione `CheckGameWinCondition()` all'interno di [MyGameMode.cpp](Source/progettoMalerba/Private/MyGameMode.cpp) che verifica il controllo delle torri all'inizio di ogni turno di movimento e determina se una fazione ha mantenuto il controllo per due turni consecutivi, dichiarando così la vittoria e rimandando a un widget di fine partita.

6. Interfaccia grafica rappresentante lo stato corrente del gioco
    - Il widget `WBP_Placement` rappresenta la GUI principale durante la partita, mostrando informazioni come torri catturate, HP delle unità e log delle azioni.
    - Il [MyPlayerController.cpp](Source/progettoMalerba/Private/MyPlayerController.cpp) gestisce l'aggiornamento dinamico di questi elementi in risposta agli eventi di gioco, come movimenti, attacchi e cambiamenti di controllo delle torri.

7. Suggerimenti del range di movimento possibile per ciascuna unità cliccando sulla stessa, colorando opportunamente tutte le celle nel range.
    - Quando il giocatore seleziona un'unità, la funzione `HandleUnitSelection()` in [MyPlayerController.cpp](Source/progettoMalerba/Private/MyPlayerController.cpp) dopo le opportune verifiche, attraverso una serie di chiamate a funzioni di supporto, evidenzia le celle raggiungibili in viola e le celle contenenti nemici attaccabili in rosso, usando i materiali dinamici delle celle per modificare il loro aspetto in tempo reale.
    - Al fine di migliorare la UX, durante la visualizzazione dei range di movimento, vengono mostrati i limiti di range di attacco delle unità alleate e nemiche.
    - Durante l'intera partita, la zona di cattura delle torri è evidenziata in modo da guidare il giocatore verso gli obiettivi strategici, con un colore distintivo che differenzia le torri in base allo stato interno della FSM che gestisce il controllo delle stesse.

    

8. Implementazione del meccanismo del danno da contrattacco
    - Gli attacchi sono gestiti dalla [BaseUnit](Source/progettoMalerba/Private/BaseUnit.cpp) che, dopo aver applicato il danno al bersaglio, verifica se le condizioni per un contrattacco sono soddisfatte e, in caso affermativo, esegue un contrattacco immediato con danno casuale nel range indicato dalle specifiche.

9. Lista dello storico delle mosse eseguite.
    - La lista dello storico delle mosse è resa graficamente attraverso una `ScrollBox` all'interno del widget `WBP_Placement`, e viene aggiornata dinamicamente dalla funzione `AppendMoveLogLine()` nel file [MyPlayerController.cpp](Source/progettoMalerba/Private/MyPlayerController.cpp) ogni volta che un'azione significativa avviene, come spawn, movimento, attacco, contrattacco, controllo torri e fine turno, con messaggi formattati per chiarezza.

10. AI che utilizza algoritmi euristici ottimizzati di movimento
    - Grazie ad una CheckBox nel menu principale, il giocatore può scegliere tra l'algoritmo A* e una variante Greedy Best-First per il pathfinding dell'AI. 
    - La funzione `BuildMovementPath()` in [MyGameMode.cpp](Source/progettoMalerba/Private/MyGameMode.cpp) implementa entrambe le strategie, differenziandole solo nel criterio di selezione del nodo da espandere: A* valuta i nodi con $f = g + h$, mentre Greedy Best-First si basa esclusivamente su $h$.
    - Il secondo algoritmo è più rapido ma può produrre percorsi meno efficienti in quanto valuta un percorso solo sulla base della stima della distanza residua al target, senza considerare il costo già accumulato, rendendolo più suscettibile a deviazioni e ostacoli.


## Note di sviluppo
Il progetto è stato sviluppato in C++ e Blueprint con Unreal Engine 5.6.1. Il modulo di gioco usa anche UMG, Slate, SlateCore, Enhanced Input e Paper2D.

Le texture e le immagini di progetto sono state interamente generate con il modello "Nano Banana 2" di Google.