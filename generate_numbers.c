#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

/**
 * @brief Baraja un arreglo de enteros usando el algoritmo Fisher-Yates.
 * @param array El arreglo a barajar.
 * @param n El número de elementos en el arreglo.
 */
void shuffle(int *array, size_t n) {
    if (n > 1) {
        for (size_t i = n - 1; i > 0; i--) {
            size_t j = rand() % (i + 1);
            int temp = array[j];
            array[j] = array[i];
            array[i] = temp;
        }
    }
}

/**
 * @brief Muestra las instrucciones de uso del programa y termina la ejecución.
 * @param prog_name El nombre del ejecutable (argv[0]).
 */
void print_usage_and_exit(const char *prog_name) {
    fprintf(stderr, "Uso: %s <cantidad_N> <archivo_salida> [valor_max] [semilla]\n\n", prog_name);
    fprintf(stderr, "Argumentos:\n");
    fprintf(stderr, "  <cantidad_N>      Número de enteros ÚNICOS a generar.\n");
    fprintf(stderr, "  <archivo_salida>  Nombre del archivo donde se guardarán los números.\n");
    fprintf(stderr, "  [valor_max]       (Opcional) Valor máximo (exclusivo) para los números generados. Rango: [0, valor_max-1].\n");
    fprintf(stderr, "                    Por defecto: 1,000,000. Debe ser >= cantidad_N.\n");
    fprintf(stderr, "  [semilla]         (Opcional) Semilla para el generador de números aleatorios.\n");
    fprintf(stderr, "                    Si no se provee, se usará el tiempo actual para mayor aleatoriedad.\n\n");
    fprintf(stderr, "Ejemplo para generar 32768 números para las pruebas:\n");
    fprintf(stderr, "  %s 32768 numeros32768.txt\n\n", prog_name);
    fprintf(stderr, "Ejemplo con semilla para reproducibilidad:\n");
    fprintf(stderr, "  %s 1024 datos_test.txt 10000 42\n", prog_name);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    // --- 1. Validación de Argumentos ---
    if (argc < 3 || argc > 5) {
        print_usage_and_exit(argv[0]);
    }

    long N_long = atol(argv[1]);
    if (N_long <= 0 || N_long > INT_MAX) {
        fprintf(stderr, "Error: La cantidad de números <cantidad_N> debe ser un entero positivo menor que %d.\n", INT_MAX);
        return EXIT_FAILURE;
    }
    int N = (int)N_long;

    const char *output_filename = argv[2];

    int max_val = 1000000;
    if (argc >= 4) {
        long max_val_long = atol(argv[3]);
        if (max_val_long <= 1 || max_val_long > INT_MAX) {
            fprintf(stderr, "Error: El valor máximo [valor_max] debe ser un entero mayor que 1 y menor que %d.\n", INT_MAX);
            return EXIT_FAILURE;
        }
        max_val = (int)max_val_long;
    }

    if (N > max_val) {
        fprintf(stderr, "Error: No se pueden generar %d números únicos en un rango de solo %d valores ([0, %d)).\n", N, max_val, max_val - 1);
        fprintf(stderr, "Asegúrate de que <cantidad_N> no sea mayor que [valor_max].\n");
        return EXIT_FAILURE;
    }

    unsigned int seed;
    if (argc == 5) {
        seed = (unsigned int)atoi(argv[4]);
        printf("Usando semilla proporcionada: %u\n", seed);
    } else {
        seed = (unsigned int)time(NULL);
        printf("Usando semilla basada en el tiempo actual: %u\n", seed);
    }
    srand(seed);

    // --- 2. Apertura del Archivo de Salida ---
    FILE *file = fopen(output_filename, "w");
    if (!file) {
        perror("Error abriendo el archivo de salida");
        return EXIT_FAILURE;
    }

    printf("Generando %d números aleatorios ÚNICOS en el rango [0, %d) y guardando en '%s'...\n", N, max_val, output_filename);

    // --- 3. Generación de Números Únicos (Fisher-Yates Shuffle) ---
    // Crear un arreglo con todos los valores posibles
    int *all_numbers = (int *)malloc(max_val * sizeof(int));
    if (!all_numbers) {
        perror("Error de asignación de memoria para el barajado");
        fclose(file);
        return EXIT_FAILURE;
    }
    for (int i = 0; i < max_val; i++) {
        all_numbers[i] = i;
    }

    // Barajar el arreglo completo
    shuffle(all_numbers, max_val);

    // --- 4. Escritura de Datos ---
    // Escribir la cantidad total N en la primera línea
    if (fprintf(file, "%d\n", N) < 0) {
        perror("Error escribiendo N en el archivo");
        free(all_numbers);
        fclose(file);
        return EXIT_FAILURE;
    }

    // Escribir los primeros N números del arreglo barajado
    for (int i = 0; i < N; i++) {
        if (fprintf(file, "%d\n", all_numbers[i]) < 0) {
            perror("Error escribiendo un número en el archivo");
            free(all_numbers);
            fclose(file);
            return EXIT_FAILURE;
        }
    }

    // --- 5. Cierre y Finalización ---
    free(all_numbers);
    fclose(file);

    printf("¡Archivo '%s' generado exitosamente!\n", output_filename);

    return EXIT_SUCCESS;
}
