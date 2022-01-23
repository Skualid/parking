#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

#define TIME_MIN 5 // Tiempo mínimo de espera para entrar y salir
#define TIME_MAX 15 // Tiempo máximo que espera para entrar y salir

// Variables globales
int num_camiones;
int num_coches;
int plazas_por_planta;
int plantas;
int **parking;
int plazas_ocupadas = 0;
int plazas_libres;
int *plazas_libres_por_planta;
int *plazas_ocupadas_por_planta;
int plaza_contigua = 0;
int camiones_interiores = 0;
int *camiones_esperando;
int camiones_esperando_boolean = 0;
int signal_camion = 0;

// señales
pthread_mutex_t mutex; // para bloquear al resto de hilos en una sección crítica
pthread_cond_t no_lleno;
pthread_cond_t no_lleno_camion;
pthread_cond_t camion_interior_puede_buscar;

void *functionCoche (void *arg) {
    int id = *(int*) arg; // id del coche
    printf("\033[0;31mCoche matricula: %d\n\033[0m", id);
    while(1){ // bucle infinito, el coche siempre va a estar entrando y saliendo
        sleep(TIME_MIN + rand() % TIME_MAX); // tiempo que pasa hasta que entra
        pthread_mutex_lock(&mutex); // bloqueo el parking porque va a entrar un coche

        while(plazas_ocupadas == plazas_por_planta * plantas){ // si está lleno espero
            pthread_cond_wait(&no_lleno, &mutex); // libero el mutex temporalmente y cuando recibo la señal no_lleno lo recupero
        }
        // me recorro el parking en busca de una plaza libre
        int i = 0;
        int j = 0;
        while(parking[j][i] != 0 && j < plantas){
            i = 0;
            while(parking[j][i] != 0 && i < plazas_por_planta){
                i++;
            }
            if(i == plazas_por_planta){
                i = 0;
                j++;
            }
        }
        parking[j][i] = id; // aparaco en la plaza libre que he encontrado

        // actualizo las variables globales
        plazas_ocupadas++;
        plazas_ocupadas_por_planta[j]++;
        plazas_libres--;
        plazas_libres_por_planta[j]--;

        // indico la operación realizada e imprimo el parking actualizado
        printf("\033[0;36mENTRADA: Coche %d ha aparcado en la plaza %d de la planta %d. Plazas libres: %d\n\033[0m", id, i + 1, j + 1, plazas_libres);
        printf("\033[0;36mPlaza:    \033[0m");
        for( int i = 0; i < plazas_por_planta; i++){
            if(i % 9 == i){
                printf("\033[0;36m[00%d] \033[0m", i+1);
            } else {
                printf("\033[0;36m[0%d] \033[0m", i+1);
            }
        }
        printf("\n");
        for (int i = 0; i < plantas; i++) {
            printf("\033[0;36mPlanta %d: \033[0m", i+1);
            for (int j = 0; j < plazas_por_planta; j++){
                if (parking[i][j] < 0) {
                    printf("[\033[0;33m%d\033[0m] ", abs(parking[i][j]));
                }else if(parking[i][j] == 0){
                    printf("[\033[0;32m000\033[0m] ");
                }else if(parking[i][j] % 10 == parking[i][j]){
                    printf("[\033[0;31m00%d\033[0m] ", parking[i][j]);
                } else if(parking[i][j] % 100 == parking[i][j]) {
                    printf("[\033[0;31m0%d\033[0m] ", parking[i][j]);
                } else {
                    printf("[\033[0;34m%d\033[0m] ", parking[i][j]);
                }
            }
            printf("\n");
        }
        printf("\n");

        // envío señales de control para informar del estado del nuevo estado del parking
        if(plazas_libres > 0 && camiones_interiores > 0){ // si hay camiones esperando a que se libere otra plaza les envío una señal informando que hay hueco
            pthread_cond_signal(&camion_interior_puede_buscar);
        }else if (plazas_libres > 0) { // envío una señal a los coches de que hay por lo menos un hueco
            pthread_cond_signal(&no_lleno);
        }

        pthread_mutex_unlock(&mutex); // libero el mutex para que entre otro coche o camión
        sleep(TIME_MIN + rand() % TIME_MAX); // tiempo que espero hasta que salga
        pthread_mutex_lock(&mutex); // vuelvo a bloquear el mutex porque va a salir el coche

        // busco el coche en el parking
        i = 0;
        j = 0;
        while(parking[j][i] != id && j < plantas){
            i = 0;
            while(parking[j][i] != id && i < plazas_por_planta){
                i++;
            }
            if(i == plazas_por_planta){
                i = 0;
                j++;
            }
        }
        parking[j][i] = 0; // libero la plaza
        // actualizo las variables globales
        plazas_ocupadas--;
        plazas_ocupadas_por_planta[j]--;
        plazas_libres++;
        plazas_libres_por_planta[j]++;

        // indico la operación realizada e imprimo el parking actualizado
        printf("\033[0;35mSALIDA: Coche %d saliendo de la plaza %d de la planta %d. Plazas libres: %d\n\033[0m", id, i + 1, j + 1, plazas_libres);
        printf("\033[0;36mPlaza:    \033[0m");
        for( int i = 0; i < plazas_por_planta; i++){
            if(i % 9 == i){
                printf("\033[0;36m[00%d] \033[0m", i+1);
            } else {
                printf("\033[0;36m[0%d] \033[0m", i+1);
            }
        }
        printf("\n");
        for (int i = 0; i < plantas; i++) {
            printf("\033[0;36mPlanta %d: \033[0m", i+1);
            for (int j = 0; j < plazas_por_planta; j++){
                if (parking[i][j] < 0) {
                    printf("[\033[0;33m%d\033[0m] ", abs(parking[i][j]));
                }else if(parking[i][j] == 0){
                    printf("[\033[0;32m000\033[0m] ");
                }else if(parking[i][j] % 10 == parking[i][j]){
                    printf("[\033[0;31m00%d\033[0m] ", parking[i][j]);
                } else if(parking[i][j] % 100 == parking[i][j]) {
                    printf("[\033[0;31m0%d\033[0m] ", parking[i][j]);
                } else {
                    printf("[\033[0;34m%d\033[0m] ", parking[i][j]);
                }
            }
            printf("\n");
        }
        printf("\n");

        // envío señales de control para informar del estado del nuevo estado del parking
        if (camiones_interiores > 0) { // si hay camiones esperando a que se libere otra plaza les envío una señal informando que hay hueco
            pthread_cond_signal(&camion_interior_puede_buscar);
        }else { // si no hay camiones esperando
            int i = 0;
            camiones_esperando_boolean = 0;
            while(i < num_camiones && camiones_esperando_boolean == 0){ // busco si hay camiones esperando fuera del parking
                if(camiones_esperando[i] != 0){
                    camiones_esperando_boolean = 1;
                }
                i++;
            }
            if(camiones_esperando_boolean && signal_camion < 5){ // si hay camiones esperando fuera del parking les mando una señal para que entren
                pthread_cond_signal(&no_lleno_camion);
                signal_camion++;
            } else { // si no hay camiones esperando fuera o si se han enviado 5 señales seguidas para que entre un camión dejo entrar a un coche para que no muera de inanición
                if(signal_camion == 5){
                    printf("Fuerzo la entrada de un coche:\n"); // indico que he forzado a que entre un coche
                }
                pthread_cond_signal(&no_lleno);
                signal_camion = 0;
            }
        }
        pthread_mutex_unlock(&mutex); // libero el mutex para que entre otro coche o camión
    }
}

void *functionCamion (void *arg) {
    int id = *(int*) arg; // id del camion
    int id_aux = id%101; // id aux del camión
    printf("\033[0;34mCamion matricula: %d\n\033[0m", id);

    while(1){ // bucle infinito, el coche siempre va a estar entrando y saliendo
        sleep(TIME_MIN + rand() % TIME_MAX); // tiempo que pasa hasta que entra
        pthread_mutex_lock(&mutex); // bloqueo el parking porque va a entrar un camión

        while(plazas_ocupadas == plazas_por_planta * plantas){ // si el parking está lleno espero
            camiones_esperando[id_aux] = 1; // pongo a 1 al camión en el array de camiones_esperando
            pthread_cond_wait(&no_lleno_camion, &mutex); // libero el mutex temporalmente
        }

        camiones_esperando[id_aux] = 0; // pongo a 0 al camión en el array de camiones_esperando

        // busco 2 plazas contiguas libres
        int cabe_camion = 0;
        int i = plantas - 1;
        int k;
        int j;
        int m;
        int plazas;

        while(cabe_camion == 0 && i >= 0){
            plazas = plazas_libres_por_planta[i];
            k = plazas_por_planta-1;
            while(cabe_camion == 0 && plazas > 1){
                if(parking[i][k] == 0){
                    if(parking[i][k-1] == 0){
                        cabe_camion = 1;
                    }
                    plazas--;
                }
                k--;
            }
            if(cabe_camion == 0){
                i--;
            }
        }

        // si no he encontrado 2 plazas libres contiguas, busco 1 plaza libre y reservo el hueco
        if(cabe_camion == 0){
            i = plantas-1;
            j = plazas_por_planta-1;
            while(parking[i][j] != 0 && i >= 0){
                while(j >= 0 && parking[i][j] != 0){
                    j--;
                }
                if(j == -1){
                    j = plazas_por_planta-1;
                    i--;
                }
            }
            parking[i][j] = id*-1; // la reserva la hago con el id del camión en negativo

            // actualizo las variables globales
            plazas_ocupadas++;
            plazas_ocupadas_por_planta[i]++;
            plazas_libres--;
            plazas_libres_por_planta[i]--;

            printf("\033[0;33mRESEVA: Camion %d acaba de reservar en la plaza %d de la planta %d. Plazas libres: %d\n\033[0m", id, j+1, i, plazas_libres);
            printf("\033[0;36mPlaza:    \033[0m");
            for( int i = 0; i < plazas_por_planta; i++){
                if(i % 9 == i){
                    printf("\033[0;36m[00%d] \033[0m", i+1);
                } else {
                    printf("\033[0;36m[0%d] \033[0m", i+1);
                }
            }
            printf("\n");
            // indico la operación realizada e imprimo el parking actualizado
            for (int i = 0; i < plantas; i++) {
                printf("\033[0;36mPlanta %d: \033[0m", i+1);
                for (int j = 0; j < plazas_por_planta; j++){
                    if (parking[i][j] < 0) {
                        printf("[\033[0;33m%d\033[0m] ", abs(parking[i][j]));
                    }else if(parking[i][j] == 0){
                        printf("[\033[0;32m000\033[0m] ");
                    }else if(parking[i][j] % 10 == parking[i][j]){
                        printf("[\033[0;31m00%d\033[0m] ", parking[i][j]);
                    } else if(parking[i][j] % 100 == parking[i][j]) {
                        printf("[\033[0;31m0%d\033[0m] ", parking[i][j]);
                    } else {
                        printf("[\033[0;34m%d\033[0m] ", parking[i][j]);
                    }
                }
                printf("\n");
            }
            printf("\n");

            int plaza_camion = 0;
            camiones_interiores++; // indico que hay un camión más esperando a otra plaza libre para aparcar
            while(plaza_camion == 0){
                pthread_cond_wait(&camion_interior_puede_buscar, &mutex); // libero el mutex temporalmente hasta que reciba una señal que me permita buscar otra plaza
                // busco en las plazas contiguas y si hay otro hueco aparco en la plaza reservada y en la nueva
                if(j == 0 && parking[i][j+1] == 0){
                    plaza_camion = 1;
                    plaza_contigua = 1;

                    parking[i][j] = id;
                    parking[i][j+1] = id;
                } else if (j == plazas_por_planta-1 && parking[i][j-1] == 0) {
                    plaza_camion = 1;
                    plaza_contigua = 1;

                    parking[i][j] = id;
                    parking[i][j-1] = id;
                } else if (j > 0 && j < plazas_por_planta-1 && (parking[i][j-1] == 0 || parking[i][j+1] == 0)){
                    plaza_camion = 1;
                    plaza_contigua = 1;

                    if(parking[i][j-1] == 0){
                        parking[i][j] = id;
                        parking[i][j-1] = id;
                    } else {
                        parking[i][j] = id;
                        parking[i][j+1] = id;
                    }
                }

                // si no he encontrado otro hueco en las plazas contiguas libres, busco en todo el parking si hay dos plazas libres contiguas
                if(!plaza_camion){
                    m = plantas - 1;
                    while(plaza_camion == 0 && m >= 0){
                        plazas = plazas_libres_por_planta[m];
                        k = plazas_por_planta-1;
                        while(plaza_camion == 0 && plazas > 1){
                            if(parking[m][k] == 0){
                                if(parking[m][k-1] == 0){
                                    plaza_camion = 1;
                                    plaza_contigua = 0;
                                    parking[m][k] = id;
                                    parking[m][k-1] = id;
                                    parking[i][j] = 0;

                                    plazas_ocupadas--;
                                    plazas_ocupadas_por_planta[i]--;
                                    plazas_libres++;
                                    plazas_libres_por_planta[i]++;
                                }
                                plazas--;
                            }
                            k--;
                        }
                        if(plaza_camion == 0){
                            m--;
                        }
                    }
                }
            }

            // compruebo si el camión ha aparcado en la plaza reservada y una contigua o en otras dos
            if(plaza_contigua){ // si es en la reservada y una contigua uso las variables originales
                k = j;

                // actualizo las variables globales
                plazas_ocupadas++;
                plazas_ocupadas_por_planta[i]++;
                plazas_libres--;
                plazas_libres_por_planta[i]--;
            } else { // si es en otras dos plazas uso las otras variables
                i = m;

                // actualizo las varibles globales
                plazas_ocupadas+=2;
                plazas_ocupadas_por_planta[i]+=2;
                plazas_libres-=2;
                plazas_libres_por_planta[i]-=2;
            }
            camiones_interiores--; // indico que el camión ya no está esperando otra plaza
        } else { // si hay dos plazas disponibles entonces aparco en esas dos
            parking[i][k] = id;
            parking[i][k+1] = id;

            // actualizo las variables
            plazas_ocupadas+=2;
            plazas_ocupadas_por_planta[i]+=2;
            plazas_libres-=2;
            plazas_libres_por_planta[i]-=2;
        }

        // indico la operación realizada e imprimo el parking actualizado
        printf("\033[0;36mENTRADA: Camion %d ha aparcado en la plaza %d y %d de la planta %d. Plazas libres: %d\n\033[0m", id, k + 1, k + 2, i + 1, plazas_libres);
        printf("\033[0;36mPlaza:    \033[0m");
        for( int i = 0; i < plazas_por_planta; i++){
            if(i % 9 == i){
                printf("\033[0;36m[00%d] \033[0m", i+1);
            } else {
                printf("\033[0;36m[0%d] \033[0m", i+1);
            }
        }
        printf("\n");
        for (int i = 0; i < plantas; i++) {
            printf("\033[0;36mPlanta %d: \033[0m", i+1);
            for (int j = 0; j < plazas_por_planta; j++){
                if (parking[i][j] < 0) {
                    printf("[\033[0;33m%d\033[0m] ", abs(parking[i][j]));
                }else if(parking[i][j] == 0){
                    printf("[\033[0;32m000\033[0m] ");
                }else if(parking[i][j] % 10 == parking[i][j]){
                    printf("[\033[0;31m00%d\033[0m] ", parking[i][j]);
                } else if(parking[i][j] % 100 == parking[i][j]) {
                    printf("[\033[0;31m0%d\033[0m] ", parking[i][j]);
                } else {
                    printf("[\033[0;34m%d\033[0m] ", parking[i][j]);
                }
            }
            printf("\n");
        }
        printf("\n");

        // envío señales de control para informar del estado del nuevo estado del parking
        if(plazas_libres > 0 && camiones_interiores > 0){ // si hay camiones esperando a que se libere otra plaza les envío una señal informando que hay hueco
            pthread_cond_signal(&camion_interior_puede_buscar);
        }else if (plazas_libres > 0) { // envío una señal a los coches de que hay por lo menos un hueco
            pthread_cond_signal(&no_lleno);
        }

        pthread_mutex_unlock(&mutex); // libero el mutex para que entre otro coche o camión
        sleep(TIME_MIN + rand() % TIME_MAX); // tiempo que espero hasta que salga
        pthread_mutex_lock(&mutex); // vuelvo a bloquear el mutex porque va a salir el coche

        // busco el camión en el parking
        j = plazas_por_planta-1;
        i = plantas-1;
        while (parking[i][j] != id){
            while(parking[i][j] != id && j > 0) {
                j--;
            }
            if(j == 0){
                j = plazas_por_planta-1;
                i--;
            }
        }

        // libero las plazas
        parking[i][j] = 0;
        parking[i][j-1] = 0;

        // actualizo las varibales globales
        plazas_ocupadas-=2;
        plazas_ocupadas_por_planta[i]-=2;
        plazas_libres+=2;
        plazas_libres_por_planta[i]+=2;

        // indico la operación realizada e imprimo el parking actualizado
        printf("\033[0;35mSALIDA: Camion %d saliendo de la plaza %d y %d de la planta %d. Plazas libres: %d\n\033[0m", id, j, j + 1, i + 1, plazas_libres);
        printf("\033[0;36mPlaza:    \033[0m");
        for( int i = 0; i < plazas_por_planta; i++){
            if(i % 9 == i){
                printf("\033[0;36m[00%d] \033[0m", i+1);
            } else {
                printf("\033[0;36m[0%d] \033[0m", i+1);
            }
        }
        printf("\n");
        for (int i = 0; i < plantas; i++) {
            printf("\033[0;36mPlanta %d: \033[0m", i+1);
            for (int j = 0; j < plazas_por_planta; j++){
                if (parking[i][j] < 0) {
                    printf("[\033[0;33m%d\033[0m] ", abs(parking[i][j]));
                }else if(parking[i][j] == 0){
                    printf("[\033[0;32m000\033[0m] ");
                }else if(parking[i][j] % 10 == parking[i][j]){
                    printf("[\033[0;31m00%d\033[0m] ", parking[i][j]);
                } else if(parking[i][j] % 100 == parking[i][j]) {
                    printf("[\033[0;31m0%d\033[0m] ", parking[i][j]);
                } else {
                    printf("[\033[0;34m%d\033[0m] ", parking[i][j]);
                }
            }
            printf("\n");
        }
        printf("\n");

        // envío señales de control para informar del estado del nuevo estado del parking
        if (camiones_interiores > 0) { // si hay camiones esperando a que se libere otra plaza les envío una señal informando que hay hueco
            pthread_cond_signal(&camion_interior_puede_buscar);
        }else { // si no hay camiones esperando
            int i = 0;
            camiones_esperando_boolean = 0;
            while(i < num_camiones && camiones_esperando_boolean == 0){ // busco si hay camiones esperando fuera del parking
                if(camiones_esperando[i] != 0){
                    camiones_esperando_boolean = 1;
                }
                i++;
            }
            if(camiones_esperando_boolean && signal_camion < 5){ // si hay camiones esperando fuera del parking les mando una señal para que entren
                pthread_cond_signal(&no_lleno_camion);
                signal_camion++;
            } else { // si no hay camiones esperando fuera o si se han enviado 5 señales seguidas para que entre un camión dejo entrar a un coche para que no muera de inanición
                if(signal_camion == 5){
                    printf("Fuerzo la entrada de un coche:\n"); // indico que he forzado a que entre un coche
                }
                pthread_cond_signal(&no_lleno);
                signal_camion = 0;
            }
        }
        pthread_mutex_unlock(&mutex); // libero el mutex para que entre otro coche o camión
    }
}

int main(int argc, char *argv[]) {
    argc--; // resto un argumento para no contar con el argumeto de "myparking"
    // inicializo las variables según el número de argumentos introducidos
    if (argc == 2) { // 1: plazas, 2: plantas
        plazas_por_planta = atoi(argv[1]);
        plantas = atoi(argv[2]);
        num_camiones = 0;
        num_coches = 2 * plantas * plazas_por_planta;
    }else if (argc == 3) { // 1: plazas, 2: plantas, 3: coches
        plazas_por_planta = atoi(argv[1]);
        plantas = atoi(argv[2]);
        num_camiones = 0;
        num_coches = atoi(argv[3]);
    }else if (argc == 4) { // 1: plazas, 2: plantas, 3: coches, 4: camiones
        plazas_por_planta = atoi(argv[1]);
        plantas = atoi(argv[2]);
        num_camiones = atoi(argv[4]);
        num_coches = atoi(argv[3]);
    }else {
        printf("Algún error en los argumentos\n");
        return 1;
    }

    plazas_libres = plazas_por_planta * plantas;

    plazas_libres_por_planta = (int*)malloc(plantas * sizeof(int));
    plazas_ocupadas_por_planta = (int*)malloc(plantas * sizeof(int));
    for (int i = 0; i < plantas; i++){
        plazas_libres_por_planta[i] = plazas_por_planta;
        plazas_ocupadas_por_planta[i] = 0;

    }

    // inicializo el parking
    parking = (int**)malloc(plantas * sizeof(int*));
    for(int i=0; i < plantas; i++){
        parking[i] = (int*)malloc(plazas_por_planta * sizeof(int));
    }

    for (int i = 0; i < plantas; i++) {
        for (int j = 0; j < plazas_por_planta; j++){
            parking[i][j] = 0;
        }
    }

    // inicializo las señales
    pthread_mutex_init(&mutex, NULL); // mutex que limitará la entrada de vehiculos al parking de uno en uno
    //pthread_cond_init(&no_lleno, NULL); // señal que se enviará a los coches cuando quepa uno
    pthread_cond_init(&camion_interior_puede_buscar, NULL); // señal que es enviará a los camiones que tengan una plaza reservada
    pthread_cond_init(&no_lleno_camion, NULL); // señal que se enviará a los camiones cuando quepa 1 (o pueda reservar)

    // inicializo y relleno con los ids el array de coches
    int *coches = (int*)malloc(num_coches * sizeof(int));
    pthread_t *coches_t = (pthread_t*)malloc(num_coches * sizeof(pthread_t));

    for (int i = 0; i < num_coches; ++i) {
        coches[i] = i+1;
        // creo tantos hilos como coches haya
        pthread_create(&coches_t[i], NULL, functionCoche, (void*) &coches[i]);
    }

    // inicializo y relleno con los ids el array de camiones
    if (num_camiones > 0) {
        int *camiones = (int*)malloc(num_camiones * sizeof(int));
        camiones_esperando = (int*)malloc(num_camiones*sizeof(int));
        pthread_t *camiones_t = (pthread_t*)malloc(num_camiones * sizeof(pthread_t));

        for (int i = 0; i < num_camiones; ++i) {
            camiones_esperando[i] = 0;
            camiones[i] = i + 101;
            // creo tantos hilos como camiones haya
            pthread_create(&camiones_t[i], NULL, functionCamion, (void*) &camiones[i]);
        }
    }
    while(1);
}