#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>

// --- Prototipos de Funciones ---

// Función de comparación para qsort
int compare_integers(const void *a, const void *b);

// Función para verificar si un número es primo
bool is_prime(int n);

// El algoritmo principal de Quick Sort paralelo y recursivo
void parallel_quicksort(int **local_array, int *local_n, MPI_Comm comm);

// --- Función Principal ---

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int N = 32; // Tamaño total del arreglo (debe ser divisible por world_size)
    int *global_array = NULL;

    if (world_rank == 0) {
        if (argc > 1) {
            N = atoi(argv[1]);
        }
        if (N % world_size != 0) {
            fprintf(stderr, "El tamaño del arreglo N debe ser divisible por el número de procesos.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        global_array = (int *)malloc(N * sizeof(int));
        srand(time(NULL));
        printf("Arreglo original (N=%d):\n", N);
        for (int i = 0; i < N; i++) {
            global_array[i] = rand() % 100;
            printf("%d ", global_array[i]);
        }
        printf("\n\n");
    }
    
    // Asegurarnos que N sea consistente en todos los procesos
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int local_n = N / world_size;
    int *local_array = (int *)malloc(local_n * sizeof(int));

    // 1. DISTRIBUCIÓN (Colectiva): Repartir el arreglo global entre todos los procesos
    MPI_Scatter(global_array, local_n, MPI_INT, local_array, local_n, MPI_INT, 0, MPI_COMM_WORLD);
    
    if (world_rank == 0) {
        free(global_array); // El proceso raíz ya no necesita el arreglo global
        global_array = NULL;
    }

    // 2. ORDENAMIENTO: Llamar a la función de ordenamiento paralelo
    parallel_quicksort(&local_array, &local_n, MPI_COMM_WORLD);

    // 3. CONTEO DE PRIMOS: Cada proceso cuenta sus primos locales
    int local_prime_count = 0;
    for (int i = 0; i < local_n; i++) {
        if (is_prime(local_array[i])) {
            local_prime_count++;
        }
    }
    
    int total_prime_count = 0;
    // (Colectiva) Sumar todos los conteos locales en el proceso raíz
    MPI_Reduce(&local_prime_count, &total_prime_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    // 4. RECOLECCIÓN (Colectiva): Juntar los arreglos ordenados en el proceso raíz
    // Primero, el raíz necesita saber cuántos elementos recibirá de cada proceso
    int *recv_counts = NULL;
    int *displacements = NULL;
    
    if (world_rank == 0) {
        recv_counts = (int *)malloc(world_size * sizeof(int));
        displacements = (int *)malloc(world_size * sizeof(int));
    }

    MPI_Gather(&local_n, 1, MPI_INT, recv_counts, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    if (world_rank == 0) {
        global_array = (int *)malloc(N * sizeof(int));
        displacements[0] = 0;
        for (int i = 1; i < world_size; i++) {
            displacements[i] = displacements[i - 1] + recv_counts[i - 1];
        }
    }
    
    // MPI_Gatherv se usa porque los trozos finales pueden ser de tamaños diferentes
    MPI_Gatherv(local_array, local_n, MPI_INT, global_array, recv_counts, displacements, MPI_INT, 0, MPI_COMM_WORLD);
    
    // 5. RESULTADO FINAL
    if (world_rank == 0) {
        printf("Arreglo ordenado:\n");
        for (int i = 0; i < N; i++) {
            printf("%d ", global_array[i]);
        }
        printf("\n\n");
        printf("Total de números primos encontrados: %d\n", total_prime_count);

        free(global_array);
        free(recv_counts);
        free(displacements);
    }
    
    free(local_array);
    MPI_Finalize();
    return 0;
}

// --- Implementación de Quick Sort Paralelo ---

void parallel_quicksort(int **local_array_ptr, int *local_n_ptr, MPI_Comm comm) {
    int comm_rank, comm_size;
    MPI_Comm_rank(comm, &comm_rank);
    MPI_Comm_size(comm, &comm_size);

    // Caso base: si solo hay un proceso en el comunicador, ordena localmente y termina.
    if (comm_size < 2) {
        if (*local_n_ptr > 0) {
            qsort(*local_array_ptr, *local_n_ptr, sizeof(int), compare_integers);
        }
        return;
    }

    // a. SELECCIÓN Y TRANSMISIÓN DEL PIVOTE (Colectiva)
    int pivot = 0;
    if (comm_rank == 0) {
        // Elegir el elemento del medio como pivote para un mejor rendimiento
        if (*local_n_ptr > 0) {
            pivot = (*local_array_ptr)[*local_n_ptr / 2];
        }
    }
    MPI_Bcast(&pivot, 1, MPI_INT, 0, comm);

    // b. PARTICIÓN LOCAL: Cada proceso divide su arreglo local en dos
    int *less = (int *)malloc(*local_n_ptr * sizeof(int));
    int *greater = (int *)malloc(*local_n_ptr * sizeof(int));
    int less_count = 0, greater_count = 0;

    for (int i = 0; i < *local_n_ptr; i++) {
        if ((*local_array_ptr)[i] <= pivot) {
            less[less_count++] = (*local_array_ptr)[i];
        } else {
            greater[greater_count++] = (*local_array_ptr)[i];
        }
    }

    // c. INTERCAMBIO DE DATOS (Punto a Punto)
    int color = (comm_rank < comm_size / 2) ? 0 : 1; // 0 para el grupo "bajo", 1 para el "alto"
    int partner_rank;
    
    if (color == 0) { // Grupo bajo, debe enviar los > pivote
        partner_rank = comm_rank + (comm_size / 2);
        MPI_Send(greater, greater_count, MPI_INT, partner_rank, 0, comm);
        free(greater); // Ya no necesitamos 'greater'
        
        // Recibir los < pivote del partner
        MPI_Status status;
        MPI_Probe(partner_rank, 0, comm, &status);
        int incoming_count;
        MPI_Get_count(&status, MPI_INT, &incoming_count);

        *local_array_ptr = (int*)realloc(less, (less_count + incoming_count) * sizeof(int));
        MPI_Recv(*local_array_ptr + less_count, incoming_count, MPI_INT, partner_rank, 0, comm, MPI_STATUS_IGNORE);
        *local_n_ptr = less_count + incoming_count;

    } else { // Grupo alto, debe enviar los <= pivote
        partner_rank = comm_rank - (comm_size / 2);
        MPI_Send(less, less_count, MPI_INT, partner_rank, 0, comm);
        free(less); // Ya no necesitamos 'less'

        // Recibir los > pivote del partner
        MPI_Status status;
        MPI_Probe(partner_rank, 0, comm, &status);
        int incoming_count;
        MPI_Get_count(&status, MPI_INT, &incoming_count);

        *local_array_ptr = (int*)realloc(greater, (greater_count + incoming_count) * sizeof(int));
        MPI_Recv(*local_array_ptr + greater_count, incoming_count, MPI_INT, partner_rank, 0, comm, MPI_STATUS_IGNORE);
        *local_n_ptr = greater_count + incoming_count;
    }

    // d. RECURSIÓN: Dividir el comunicador y llamar a la función de nuevo
    MPI_Comm new_comm;
    MPI_Comm_split(comm, color, comm_rank, &new_comm);
    parallel_quicksort(local_array_ptr, local_n_ptr, new_comm);
    
    // Liberar el nuevo comunicador
    MPI_Comm_free(&new_comm);
}

// --- Funciones Auxiliares ---

int compare_integers(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

bool is_prime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int i = 5; i * i <= n; i = i + 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return false;
    }
    return true;
}