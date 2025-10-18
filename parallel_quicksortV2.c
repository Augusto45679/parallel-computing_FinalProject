#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h> // Para memcpy

// --- Prototipos de Funciones ---
int compare_integers(const void *a, const void *b);
bool is_prime(int n);
int partition_inplace(int *array, int n, int pivot);
void parallel_quicksort(int **local_array, int *local_n, MPI_Comm comm);

// --- Función Principal ---
int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    double start_time, end_time;
    MPI_Barrier(MPI_COMM_WORLD); 
    start_time = MPI_Wtime();

    int N = 0; 
    int *global_array = NULL;

    if (world_rank == 0) {
        if (argc != 2) {
            fprintf(stderr, "Uso: %s <archivo_de_entrada>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        FILE *file = fopen(argv[1], "r");
        if (!file) {
            perror("Error abriendo el archivo");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        // Leer N y los datos desde el archivo
        fscanf(file, "%d", &N);
        if (N % world_size != 0) {
            fprintf(stderr, "N (%d) debe ser divisible por el número de procesos (%d).\n", N, world_size);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        global_array = (int *)malloc(N * sizeof(int));
        for (int i = 0; i < N; i++) {
            fscanf(file, "%d", &global_array[i]);
        }
        fclose(file);

        #ifdef DEBUG_PRINT
        printf("Arreglo original (N=%d) leído desde %s:\n", N, argv[1]);
        for (int i = 0; i < N; i++) { printf("%d ", global_array[i]); }
        printf("\n\n");
        #else
        printf("Arreglo original (N=%d) leído desde %s.\n", N, argv[1]);
        #endif
    }
    
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int local_n = N / world_size;
    int *local_array = (int *)malloc(local_n * sizeof(int));

    MPI_Scatter(global_array, local_n, MPI_INT, local_array, local_n, MPI_INT, 0, MPI_COMM_WORLD);
    
    if (world_rank == 0) {
        free(global_array);
        global_array = NULL;
    }

    // --- Algoritmo principal ---
    parallel_quicksort(&local_array, &local_n, MPI_COMM_WORLD);

    // ... (El resto de la lógica de conteo de primos y recolección no cambia) ...
    int local_prime_count = 0;
    for (int i = 0; i < local_n; i++) { if (is_prime(local_array[i])) local_prime_count++; }
    
    int total_prime_count = 0;
    MPI_Reduce(&local_prime_count, &total_prime_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    
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
    
    MPI_Gatherv(local_array, local_n, MPI_INT, global_array, recv_counts, displacements, MPI_INT, 0, MPI_COMM_WORLD);
    
    MPI_Barrier(MPI_COMM_WORLD); 
    end_time = MPI_Wtime();

    if (world_rank == 0) {
        printf("\n--- Resultados ---\n");
        #ifdef DEBUG_PRINT
        printf("Arreglo ordenado:\n");
        for (int i = 0; i < N; i++) { printf("%d ", global_array[i]); }
        printf("\n\n");
        #else
        printf("Arreglo ordenado correctamente.\n");
        #endif
        printf("Total de números primos encontrados: %d\n", total_prime_count);
        printf("Tiempo de ejecución total: %f segundos\n", end_time - start_time);
        
        free(global_array);
        free(recv_counts);
        free(displacements);
    }
    
    free(local_array);
    MPI_Finalize();
    return 0;
}


// --- Implementación de Quick Sort Paralelo Mejorado ---
void parallel_quicksort(int **local_array_ptr, int *local_n_ptr, MPI_Comm comm) {
    int comm_rank, comm_size;
    MPI_Comm_rank(comm, &comm_rank);
    MPI_Comm_size(comm, &comm_size);
    int local_n = *local_n_ptr;
    int *local_array = *local_array_ptr;

    if (comm_size < 2) {
        if (local_n > 0) qsort(local_array, local_n, sizeof(int), compare_integers);
        return;
    }

    // ================== MEJORA 1: PIVOTE POR MEDIANA DE MEDIANOS ==================
    int pivot = 0;
    // 1. Cada proceso calcula su mediana local
    int local_median = 0;
    if (local_n > 0) {
        qsort(local_array, local_n, sizeof(int), compare_integers);
        local_median = local_array[local_n / 2];
    }

    // 2. El líder del grupo recolecta todas las medianas locales
    int *medians = NULL;
    if (comm_rank == 0) {
        medians = (int *)malloc(comm_size * sizeof(int));
    }
    MPI_Gather(&local_median, 1, MPI_INT, medians, 1, MPI_INT, 0, comm);

    // 3. El líder calcula la mediana de las medianas (el pivote final)
    if (comm_rank == 0) {
        qsort(medians, comm_size, sizeof(int), compare_integers);
        pivot = medians[comm_size / 2];
        free(medians);
    }

    // 4. El líder transmite el pivote robusto a todos
    MPI_Bcast(&pivot, 1, MPI_INT, 0, comm);
    // =============================================================================

    // ================== MEJORA 2: PARTICIÓN IN-PLACE ==================
    // No se crean nuevos arreglos 'less' y 'greater', ahorrando memoria.
    int split_point = partition_inplace(local_array, local_n, pivot);
    int less_count = split_point;
    int greater_count = local_n - split_point;
    // =================================================================

    // ================== MEJORA 3: INTERCAMBIO CON MPI_Sendrecv ==================
    // Esto previene deadlocks con mensajes grandes.
    int partner_rank;
    int color = (comm_rank < comm_size / 2) ? 0 : 1;
    
    int *incoming_buffer = NULL;
    int incoming_count = 0;

    if (color == 0) { // Grupo bajo: envía 'greater', recibe 'less'
        partner_rank = comm_rank + (comm_size / 2);
        // Primero, averigua cuántos datos vas a recibir
        MPI_Sendrecv(&greater_count, 1, MPI_INT, partner_rank, 0, 
                     &incoming_count, 1, MPI_INT, partner_rank, 0, comm, MPI_STATUS_IGNORE);

        incoming_buffer = (int *)malloc(incoming_count * sizeof(int));
        
        // Ahora, intercambia los datos
        MPI_Sendrecv(local_array + less_count, greater_count, MPI_INT, partner_rank, 1,
                     incoming_buffer, incoming_count, MPI_INT, partner_rank, 1, comm, MPI_STATUS_IGNORE);
        
        // Combina tus datos 'less' con los recibidos
        *local_array_ptr = (int *)realloc(local_array, (less_count + incoming_count) * sizeof(int));
        memcpy(*local_array_ptr + less_count, incoming_buffer, incoming_count * sizeof(int));
        *local_n_ptr = less_count + incoming_count;

    } else { // Grupo alto: envía 'less', recibe 'greater'
        partner_rank = comm_rank - (comm_size / 2);
        MPI_Sendrecv(&less_count, 1, MPI_INT, partner_rank, 0, 
                     &incoming_count, 1, MPI_INT, partner_rank, 0, comm, MPI_STATUS_IGNORE);

        incoming_buffer = (int *)malloc(incoming_count * sizeof(int));

        MPI_Sendrecv(local_array, less_count, MPI_INT, partner_rank, 1,
                     incoming_buffer, incoming_count, MPI_INT, partner_rank, 1, comm, MPI_STATUS_IGNORE);

        // Combina tus datos 'greater' con los recibidos
        int *new_local_array = (int *)malloc((greater_count + incoming_count) * sizeof(int));
        memcpy(new_local_array, local_array + less_count, greater_count * sizeof(int));
        memcpy(new_local_array + greater_count, incoming_buffer, incoming_count * sizeof(int));
        
        free(*local_array_ptr);
        *local_array_ptr = new_local_array;
        *local_n_ptr = greater_count + incoming_count;
    }
    free(incoming_buffer);
    // =============================================================================

    MPI_Comm new_comm;
    MPI_Comm_split(comm, color, comm_rank, &new_comm);
    parallel_quicksort(local_array_ptr, local_n_ptr, new_comm);
    MPI_Comm_free(&new_comm);
}

// --- Funciones Auxiliares ---

// Particiona un arreglo in-place y devuelve el número de elementos <= pivote
int partition_inplace(int *array, int n, int pivot) {
    int i = 0, j = n - 1;
    while (i <= j) {
        while (i < n && array[i] <= pivot) { i++; }
        while (j >= 0 && array[j] > pivot) { j--; }
        if (i < j) {
            int temp = array[i];
            array[i] = array[j];
            array[j] = temp;
        }
    }
    return i;
}

int compare_integers(const void *a, const void *b) { return (*(int *)a - *(int *)b); }
bool is_prime(int n) { if (n <= 1) return false; for (int i = 2; i * i <= n; i++) { if (n % i == 0) return false; } return true; }