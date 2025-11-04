#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h> // Para memcpy

// --- Estructura para almacenar métricas de tiempo ---
typedef struct {
    double comp_time; // Tiempo dedicado a cómputo (qsort, is_prime, partición)
    double comm_time; // Tiempo dedicado a comunicaciones MPI
    double total_time; // Tiempo total de ejecución de la rutina
    double idle_time; // Tiempo de ocio/espera (total - comp - comm)
} Timing;


// --- Prototipos de Funciones ---
int compare_integers(const void *a, const void *b);
bool is_prime(int n);
int partition_inplace(int *array, int n, int pivot);
void parallel_quicksort(int **local_array, int *local_n, MPI_Comm comm, Timing *timing_data);

// --- Función Principal ---
int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    // Inicialización de variables de tiempo para el proceso local
    Timing local_timing = {0.0, 0.0, 0.0, 0.0};
    double start_time, end_time, temp_time;

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
    
    // --- Comunicación: Bcast de N ---
    temp_time = MPI_Wtime();
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    local_timing.comm_time += (MPI_Wtime() - temp_time);

    int local_n = N / world_size;
    int *local_array = (int *)malloc(local_n * sizeof(int));

    // --- Comunicación: Scatter inicial ---
    temp_time = MPI_Wtime();
    MPI_Scatter(global_array, local_n, MPI_INT, local_array, local_n, MPI_INT, 0, MPI_COMM_WORLD);
    local_timing.comm_time += (MPI_Wtime() - temp_time);
    
    if (world_rank == 0) {
        free(global_array);
        global_array = NULL;
    }

    // --- Algoritmo principal ---
    parallel_quicksort(&local_array, &local_n, MPI_COMM_WORLD, &local_timing);

    // --- Cómputo: Conteo de primos ---
    int local_prime_count = 0;
    temp_time = MPI_Wtime();
    for (int i = 0; i < local_n; i++) { 
        if (is_prime(local_array[i])) local_prime_count++; 
    }
    local_timing.comp_time += (MPI_Wtime() - temp_time);
    
    int total_prime_count = 0;
    // --- Comunicación: Reduce de primos ---
    temp_time = MPI_Wtime();
    MPI_Reduce(&local_prime_count, &total_prime_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    local_timing.comm_time += (MPI_Wtime() - temp_time);
    
    int *recv_counts = NULL;
    int *displacements = NULL;
    if (world_rank == 0) {
        recv_counts = (int *)malloc(world_size * sizeof(int));
        displacements = (int *)malloc(world_size * sizeof(int));
    }

    // --- Comunicación: Gather de tamaños finales ---
    temp_time = MPI_Wtime();
    MPI_Gather(&local_n, 1, MPI_INT, recv_counts, 1, MPI_INT, 0, MPI_COMM_WORLD);
    local_timing.comm_time += (MPI_Wtime() - temp_time);
    
    if (world_rank == 0) {
        // --- Cómputo: Cálculo de displacements ---
        temp_time = MPI_Wtime();
        int final_N = 0;
        for (int i = 0; i < world_size; i++) final_N += recv_counts[i];
        
        global_array = (int *)malloc(final_N * sizeof(int)); // Usar el tamaño final real
        
        displacements[0] = 0;
        for (int i = 1; i < world_size; i++) {
            displacements[i] = displacements[i - 1] + recv_counts[i - 1];
        }
        local_timing.comp_time += (MPI_Wtime() - temp_time);
    }
    
    // --- Comunicación: Gatherv final ---
    temp_time = MPI_Wtime();
    MPI_Gatherv(local_array, local_n, MPI_INT, global_array, recv_counts, displacements, MPI_INT, 0, MPI_COMM_WORLD);
    local_timing.comm_time += (MPI_Wtime() - temp_time);
    
    // --- Barrera final para medir el tiempo total de todos los procesos ---
    temp_time = MPI_Wtime();
    MPI_Barrier(MPI_COMM_WORLD); 
    local_timing.comm_time += (MPI_Wtime() - temp_time);
    
    end_time = MPI_Wtime();
    local_timing.total_time = end_time - start_time;
    
    // --- Cómputo: Cálculo de tiempo de ocio ---
    local_timing.idle_time = local_timing.total_time - local_timing.comp_time - local_timing.comm_time;
    if (local_timing.idle_time < 0.0) local_timing.idle_time = 0.0; // Evitar valores negativos por imprecisión
    
    // --- Recolección y reporte de métricas de tiempo ---
    Timing *all_timings = NULL;
    if (world_rank == 0) {
        all_timings = (Timing *)malloc(world_size * sizeof(Timing));
    }

    // Usar MPI_Gather para recolectar la estructura de todos los procesos
    MPI_Gather(&local_timing, sizeof(Timing), MPI_BYTE, 
               all_timings, sizeof(Timing), MPI_BYTE, 
               0, MPI_COMM_WORLD);

    if (world_rank == 0) {
        printf("\n--- Resultados ---\n");
        #ifdef DEBUG_PRINT
        printf("Arreglo ordenado:\n");
        int final_N = 0;
        for (int i = 0; i < world_size; i++) final_N += recv_counts[i];
        for (int i = 0; i < final_N; i++) { printf("%d ", global_array[i]); }
        printf("\n\n");
        #else
        printf("Arreglo ordenado correctamente.\n");
        #endif
        printf("Total de números primos encontrados: %d\n", total_prime_count);
        printf("Tiempo de ejecución total (Proceso 0): %f segundos\n", all_timings[0].total_time); // Usamos el tiempo total del proceso 0
        
        printf("\n--- Análisis de Tiempos por Proceso (segundos) ---\n");
        printf("| %-8s | %-12s | %-12s | %-12s | %-12s |\n", 
               "Proceso", "Total", "Cómputo", "Comunicación", "Ocio/Espera");
        printf("|----------|--------------|--------------|--------------|--------------|\n");
        for (int i = 0; i < world_size; i++) {
            printf("| %-8d | %-12.6f | %-12.6f | %-12.6f | %-12.6f |\n", 
                   i, 
                   all_timings[i].total_time, 
                   all_timings[i].comp_time, 
                   all_timings[i].comm_time, 
                   all_timings[i].idle_time);
        }
        
        free(global_array);
        free(recv_counts);
        free(displacements);
        free(all_timings);
    }
    
    free(local_array);
    MPI_Finalize();
    return 0;
}


// --- Implementación de Quick Sort Paralelo Mejorado ---
void parallel_quicksort(int **local_array_ptr, int *local_n_ptr, MPI_Comm comm, Timing *timing_data) {
    int comm_rank, comm_size;
    MPI_Comm_rank(comm, &comm_rank);
    MPI_Comm_size(comm, &comm_size);
    int local_n = *local_n_ptr;
    int *local_array = *local_array_ptr;
    
    double temp_time;

    if (comm_size < 2) {
        // --- Cómputo: qsort local final ---
        temp_time = MPI_Wtime();
        if (local_n > 0) qsort(local_array, local_n, sizeof(int), compare_integers);
        timing_data->comp_time += (MPI_Wtime() - temp_time);
        return;
    }

    // ================== MEJORA 1: PIVOTE POR MEDIANA DE MEDIANOS ==================
    int pivot = 0;
    // 1. Cada proceso calcula su mediana local
    int local_median = 0;
    // --- Cómputo: qsort y cálculo de mediana local ---
    temp_time = MPI_Wtime();
    if (local_n > 0) {
        qsort(local_array, local_n, sizeof(int), compare_integers);
        local_median = local_array[local_n / 2];
    }
    timing_data->comp_time += (MPI_Wtime() - temp_time);

    // 2. El líder del grupo recolecta todas las medianas locales
    int *medians = NULL;
    if (comm_rank == 0) {
        medians = (int *)malloc(comm_size * sizeof(int));
    }
    // --- Comunicación: Gather de medianas ---
    temp_time = MPI_Wtime();
    MPI_Gather(&local_median, 1, MPI_INT, medians, 1, MPI_INT, 0, comm);
    timing_data->comm_time += (MPI_Wtime() - temp_time);

    // 3. El líder calcula la mediana de las medianas (el pivote final)
    if (comm_rank == 0) {
        // --- Cómputo: qsort de medianas y cálculo de pivote ---
        temp_time = MPI_Wtime();
        qsort(medians, comm_size, sizeof(int), compare_integers);
        pivot = medians[comm_size / 2];
        timing_data->comp_time += (MPI_Wtime() - temp_time);
        free(medians);
    }

    // 4. El líder transmite el pivote robusto a todos
    // --- Comunicación: Bcast del pivote ---
    temp_time = MPI_Wtime();
    MPI_Bcast(&pivot, 1, MPI_INT, 0, comm);
    timing_data->comm_time += (MPI_Wtime() - temp_time);
    // =============================================================================

    // ================== MEJORA 2: PARTICIÓN IN-PLACE ==================
    // --- Cómputo: Partición in-place ---
    temp_time = MPI_Wtime();
    int split_point = partition_inplace(local_array, local_n, pivot);
    timing_data->comp_time += (MPI_Wtime() - temp_time);
    
    int less_count = split_point;
    int greater_count = local_n - split_point;
    // =================================================================

    // ================== MEJORA 3: INTERCAMBIO CON MPI_Sendrecv ==================
    int partner_rank;
    int color = (comm_rank < comm_size / 2) ? 0 : 1;
    
    int *incoming_buffer = NULL;
    int incoming_count = 0;
    
    // --- Comunicación: Intercambio con Sendrecv (dos fases) ---
    temp_time = MPI_Wtime();

    if (color == 0) { // Grupo bajo: envía 'greater', recibe 'less'
        partner_rank = comm_rank + (comm_size / 2);
        
        // Fase 1: Intercambio de conteos
        MPI_Sendrecv(&greater_count, 1, MPI_INT, partner_rank, 0, 
                     &incoming_count, 1, MPI_INT, partner_rank, 0, comm, MPI_STATUS_IGNORE);

        incoming_buffer = (int *)malloc(incoming_count * sizeof(int));
        
        // Fase 2: Intercambio de datos
        MPI_Sendrecv(local_array + less_count, greater_count, MPI_INT, partner_rank, 1,
                     incoming_buffer, incoming_count, MPI_INT, partner_rank, 1, comm, MPI_STATUS_IGNORE);
        
        timing_data->comm_time += (MPI_Wtime() - temp_time); // Fin de la medición de comunicación
        
        // --- Cómputo: Reasignación y copia de datos (Grupo Bajo) ---
        temp_time = MPI_Wtime();
        
        // Usamos realloc y memcpy, como en el código original, para el grupo 0
        *local_array_ptr = (int *)realloc(local_array, (less_count + incoming_count) * sizeof(int));
        if (*local_array_ptr == NULL) { /* Manejo de error de realloc */ MPI_Abort(comm, 99); }
        memcpy(*local_array_ptr + less_count, incoming_buffer, incoming_count * sizeof(int));
        *local_n_ptr = less_count + incoming_count;

        timing_data->comp_time += (MPI_Wtime() - temp_time); // Fin de la medición de cómputo (realloc/memcpy)

    } else { // Grupo alto: envía 'less', recibe 'greater'
        partner_rank = comm_rank - (comm_size / 2);
        
        // Fase 1: Intercambio de conteos
        MPI_Sendrecv(&less_count, 1, MPI_INT, partner_rank, 0, 
                     &incoming_count, 1, MPI_INT, partner_rank, 0, comm, MPI_STATUS_IGNORE);

        incoming_buffer = (int *)malloc(incoming_count * sizeof(int));

        // Fase 2: Intercambio de datos
        MPI_Sendrecv(local_array, less_count, MPI_INT, partner_rank, 1,
                     incoming_buffer, incoming_count, MPI_INT, partner_rank, 1, comm, MPI_STATUS_IGNORE);
        
        timing_data->comm_time += (MPI_Wtime() - temp_time); // Fin de la medición de comunicación
        
        // --- Cómputo: Asignación de nuevo arreglo y copias de datos (Grupo Alto) ---
        temp_time = MPI_Wtime();

        // El grupo 1 usa malloc y free, ya que el arreglo original no contiene el inicio del nuevo arreglo.
        int *new_local_array = (int *)malloc((greater_count + incoming_count) * sizeof(int));
        memcpy(new_local_array, local_array + less_count, greater_count * sizeof(int));
        memcpy(new_local_array + greater_count, incoming_buffer, incoming_count * sizeof(int));
        
        free(*local_array_ptr);
        *local_array_ptr = new_local_array;
        *local_n_ptr = greater_count + incoming_count;

        timing_data->comp_time += (MPI_Wtime() - temp_time); // Fin de la medición de cómputo (malloc/memcpy/free)
    }
    free(incoming_buffer);
    // =============================================================================
    
    // --- Comunicación: Split de comunicador ---
    temp_time = MPI_Wtime();
    MPI_Comm new_comm;
    MPI_Comm_split(comm, color, comm_rank, &new_comm);
    timing_data->comm_time += (MPI_Wtime() - temp_time);
    
    // Llamada recursiva con el nuevo comunicador. Se pasa el mismo puntero 'timing_data'.
    parallel_quicksort(local_array_ptr, local_n_ptr, new_comm, timing_data);

    // --- Comunicación: Free de comunicador ---
    temp_time = MPI_Wtime();
    MPI_Comm_free(&new_comm);
    timing_data->comm_time += (MPI_Wtime() - temp_time);
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
