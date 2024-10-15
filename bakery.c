#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

#define STORAGE_MAX_COUNT 30 // Maximale Lagerkapazität
#define STORAGE_SUPPLIER_COUNT 20 // Sobald 2/3 gefüllt sind, holt der Lieferant die Ware ab
#define BAKING_STATIONS 6 // Anzahl der Backstationen (Threads)

typedef struct{
    int count;
    pthread_mutex_t mutex; // Mutex zum Schutz des Lagerbestands
} storage_t;

// Condition Variables und ihre zugehörigen Mutexes für Synchronisation
pthread_cond_t supplier_cond;
pthread_mutex_t supplier_mutex;

pthread_cond_t baking_cond;
pthread_mutex_t baking_mutex;

storage_t storage;

volatile sig_atomic_t shutdownFlag = 0; // Flag zum sauberen Beenden des Programms
volatile sig_atomic_t supplierInformed = 0; // Flag, damit der Lieferant nur einmal informiert wird

// Funktion für die Backstationen (Threads)
void* bakingBread(void* arg){
    while (!shutdownFlag){
        // Kritischer Bereich beginnt - Schutz des Lagerbestands
        pthread_mutex_lock(&storage.mutex);
        // Brot backen, wenn noch Platz im Lager ist
        if (storage.count < STORAGE_MAX_COUNT){
            printf("Lagerstand wird erhoeht\n");
            storage.count += 1;
            // Lieferant informieren, wenn 2/3 des Lagers gefüllt sind
            if (storage.count >= STORAGE_SUPPLIER_COUNT){
                pthread_mutex_lock(&supplier_mutex);
                if (supplierInformed == 0) {
                    printf("Lieferant wird informiert\n");
                    pthread_cond_signal(&supplier_cond); // Lieferant wecken
                    supplierInformed = 1;
                }
                pthread_mutex_unlock(&supplier_mutex);
            }
            pthread_mutex_unlock(&storage.mutex); // Kritischer Bereich endet
            sleep(1); // Simuliert Zeit zum Backen eines Brotes
        } else {
            pthread_mutex_unlock(&storage.mutex); // Kritischer Bereich endet
            // Warte, bis Platz im Lager frei wird
            printf("Warte bis Platz im Lager\n");
            pthread_mutex_lock(&baking_mutex);
            while (storage.count >= STORAGE_MAX_COUNT) {
                pthread_cond_wait(&baking_cond, &baking_mutex); // Warten, bis Platz frei wird
            }
            pthread_mutex_unlock(&baking_mutex);
            printf("Warte bis Platz im Lager: Aufgeweckt\n");
        }
    }
    printf("Backstation beendet sich\n");
    return NULL;
}

// Funktion für den Lieferanten (Thread)
void* deliverBread(void* arg){
    while (!shutdownFlag){
        // Lieferant wartet auf Signal zum Abholen der Ware
        printf("Lieferant wartet auf Anruf\n");
        pthread_mutex_lock(&supplier_mutex);
        pthread_cond_wait(&supplier_cond, &supplier_mutex); // Warten, bis er benachrichtigt wird
        pthread_mutex_unlock(&supplier_mutex);
        printf("Lieferant hat Anruf bekommen\n");

        if (!shutdownFlag){
            // Simuliert das Abholen der Ware (zwischen 1-4 Sekunden)
            sleep((rand() % 4) + 1);
            // Lager leeren
            printf("Lieferant hat Lager geleert\n");
            pthread_mutex_lock(&storage.mutex);
            storage.count = 0;
            pthread_mutex_unlock(&storage.mutex);
            supplierInformed = 0; // Lieferant kann erneut benachrichtigt werden
            // Alle Backstationen wecken, da das Lager nun leer ist
            pthread_mutex_lock(&baking_mutex);
            pthread_cond_broadcast(&baking_cond); // Weckt alle Backstationen
            pthread_mutex_unlock(&baking_mutex);
        }
    }
    printf("Lieferant beendet sich\n");
    return NULL;
}

// Signalfunktion für Qualitätskontrolle - Entfernt zufällig einige Brote aus dem Lager
void qualityCheck(int signum){
    printf("qualityCheck\n");
    int count = (rand() % 10) + 1; // Zufällige Anzahl an Broten, die entfernt werden
    pthread_mutex_lock(&storage.mutex);
    if (storage.count < count){
        count = storage.count; // Kann nicht mehr entfernt werden als vorhanden
    }
    storage.count -= count;
    pthread_mutex_unlock(&storage.mutex);
    // Backstationen wecken, um weiter Brot zu backen
    pthread_mutex_lock(&baking_mutex);
    pthread_cond_broadcast(&baking_cond); // Weckt alle Backstationen
    pthread_mutex_unlock(&baking_mutex);
    alarm(5); // Setzt den Alarm für die nächste Qualitätskontrolle
}

// Signalfunktion für einen sauberen Shutdown des Systems
void graceful_shutdown(int signum){
    printf("Graceful shutdown\n");
    shutdownFlag = 1;
    // Alle Backstationen wecken, damit sie sich beenden können
    pthread_mutex_lock(&baking_mutex);
    pthread_cond_broadcast(&baking_cond);
    pthread_mutex_unlock(&baking_mutex);
    // Lieferanten wecken, damit auch dieser sich beenden kann
    pthread_mutex_lock(&supplier_mutex);
    pthread_cond_broadcast(&supplier_cond);
    pthread_mutex_unlock(&supplier_mutex);
}

// Threadfunktion, die das Programm nach 40 Sekunden automatisch beendet
void* auto_shutdown(void* arg){
    int timer = 40;
    while (timer > 0){
        sleep(1); // Wartet 40 Sekunden
        timer--;
        if (shutdownFlag)
            return NULL;
    }
    graceful_shutdown(0);
    return NULL;
}


int main(void){
    // Zufallsgenerator initialisieren
    srand(time(NULL));

    // Initialisierung der Mutexes
    if (pthread_mutex_init(&storage.mutex, NULL) != 0) {
        perror("Fehler: 'storage' Mutex konnte nicht initialisiert werden\n");
        exit(1);
    }
    if (pthread_mutex_init(&supplier_mutex, NULL) != 0) {
        fprintf(stderr, "Fehler: 'supplier' Mutex konnte nicht initialisiert werden\n");
        exit(1);
    }
    if (pthread_mutex_init(&baking_mutex, NULL) != 0) {
        perror("Fehler: 'baking' Mutex konnte nicht initialisiert werden\n");
        exit(1);
    }

    // Initialisierung der Condition Variables
    if (pthread_cond_init(&supplier_cond, NULL) != 0) {
        perror("Fehler: 'supplier' Condition konnte nicht initialisiert werden\n");
        exit(1);
    }
    if (pthread_cond_init(&baking_cond, NULL) != 0) {
        perror("Fehler: 'baking' Condition konnte nicht initialisiert werden\n");
        exit(1);
    }

    // Registriere Signal Handler für den Alarm (qualityCheck)
    struct sigaction signal;
    signal.sa_handler = qualityCheck;
    sigemptyset(&signal.sa_mask);

    if (sigaction(SIGALRM, &signal, NULL) != 0){
        perror("Fehler beim registrieren des signal handler\n");
        exit(1);
    }

    // Registriere Signal Handler für den Graceful Shutdown
    struct sigaction shutdownSignal;
    shutdownSignal.sa_handler = graceful_shutdown;
    sigemptyset(&shutdownSignal.sa_mask);
    sigaddset(&shutdownSignal.sa_mask, SIGTERM);
    sigaddset(&shutdownSignal.sa_mask, SIGINT);
    sigaddset(&shutdownSignal.sa_mask, SIGQUIT);
    shutdownSignal.sa_flags = SA_RESTART;

    if (sigaction(SIGTERM, &shutdownSignal, NULL) != 0){
        perror("Fehler beim registrieren des signal handler\n");
        exit(1);
    }
    if (sigaction(SIGINT, &shutdownSignal, NULL) != 0){
        perror("Fehler beim registrieren des signal handler\n");
        exit(1);
    }
    if (sigaction(SIGQUIT, &shutdownSignal, NULL) != 0){
        perror("Fehler beim registrieren des signal handler\n");
        exit(1);
    }

    // Backstation-Threads erstellen
    pthread_t bakingThreads[BAKING_STATIONS];
    for (int i = 0; i < 6; i++){
        if (pthread_create(&bakingThreads[i], NULL, bakingBread, NULL) != 0) {
            perror("Fehler: Backstation Thread konnte nicht erstellt werden\n");
            exit(1);
        }
    }

    // Lieferanten-Thread erstellen
    pthread_t supplierThread;
    if (pthread_create(&supplierThread, NULL, deliverBread, NULL) != 0) {
        perror("Fehler: Lieferant Thread konnte nicht erstellt werden\n");
        exit(1);
    }

    // Shutdown-Thread erstellen
    pthread_t shutdownThread;
    if (pthread_create(&shutdownThread, NULL, auto_shutdown, NULL) != 0) {
        perror("Fehler: Shutdown Thread konnte nicht erstellt werden\n");
        exit(1);
    }

    alarm(5); // Startet die regelmäßige Qualitätskontrolle alle 5 Sekunden

    // Warten bis alle Backstation-Threads beendet sind
    for (int i = 0; i < 6; i++){
        pthread_join(bakingThreads[i], NULL);
    }

    // Warten bis der Lieferanten-Thread beendet ist
    pthread_join(supplierThread, NULL);

    // Warten bis der Shutdown-Thread beendet ist
    pthread_join(shutdownThread, NULL);

    // Ressourcen freigeben
    pthread_mutex_destroy(&storage.mutex);
    pthread_mutex_destroy(&supplier_mutex);
    pthread_mutex_destroy(&baking_mutex);
    pthread_cond_destroy(&supplier_cond);
    pthread_cond_destroy(&baking_cond);

    storage.count = 0;

    return 0;
}