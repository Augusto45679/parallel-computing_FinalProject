#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>
#include <math.h>

// --- Prototipos de Funciones ---
static void generate_with_hash_table(int N, long long min_val, long long max_val, FILE* file);
static void generate_with_shuffle(int N, long long min_val, long long max_val, FILE* file);
static void shuffle(int *array, size_t n);
static unsigned long long rand_ull();
static void print_usage_and_exit(const char *prog_name);
static bool is_prime(int n);
static int next_prime(int n);
 
// --- Implementación de una Tabla Hash simple para enteros ---

typedef struct Node {
    int key;
    struct Node* next;
} Node;

typedef struct HashTable {
    int size;
    Node** table;
} HashTable;

/** @brief Crea una tabla hash de un tamaño dado. */
HashTable* create_hash_table(int size) {
    HashTable* ht = (HashTable*)malloc(sizeof(HashTable));
    if (!ht) return NULL;
    ht->size = size;
    ht->table = (Node**)calloc(size, sizeof(Node*)); // calloc inicializa a NULL
    if (!ht->table) {
        free(ht);
        return NULL;
    }
    return ht;
}

/** @brief Función de hash simple (variación de FNV-1a para mezclar bits). */
static unsigned int hash_function(int key, int size) {
    // Usar una función de hash un poco mejor que un simple módulo para distribuir mejor.
    unsigned int hash = 2166136261u;
    hash = (hash ^ key) * 16777619;
    return (unsigned int)key % size;
}

/** @brief Inserta una clave en la tabla hash. No verifica duplicados. */
void hash_insert(HashTable* ht, int key) {
    unsigned int index = hash_function(key, ht->size);
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (!new_node) return; // Fallo silencioso en caso de error de memoria
    new_node->key = key;
    new_node->next = ht->table[index];
    ht->table[index] = new_node;
}

/** @brief Busca una clave en la tabla hash. Devuelve true si la encuentra. */
bool hash_search(HashTable* ht, int key) {
    unsigned int index = hash_function(key, ht->size);
    Node* current = ht->table[index];
    while (current) {
        if (current->key == key) {
            return true;
        }
        current = current->next;
    }
    return false;
}

/** @brief Libera toda la memoria usada por la tabla hash. */
void free_hash_table(HashTable* ht) {
    if (!ht) return;
    for (int i = 0; i < ht->size; i++) {
        Node* current = ht->table[i];
        while (current) {
            Node* temp = current;
            current = current->next;
            free(temp);
        }
    }
    free(ht->table);
    free(ht);
}

/** @brief Genera un número aleatorio de 64 bits para un rango más amplio. */
static unsigned long long rand_ull() {    
    // RAND_MAX es usualmente 2^31-1. Combinamos 3 llamadas para obtener ~93 bits de aleatoriedad
    // y luego los reducimos a 64 para una mejor distribución y menos sesgo de módulo.
    unsigned long long r = (unsigned long long)rand();
    r = (r << 31) | (unsigned long long)rand();
    r = (r << 31) | (unsigned long long)rand();
    return r;
}

/** @brief Muestra las instrucciones de uso del programa y termina la ejecución. */
static void print_usage_and_exit(const char *prog_name) {
    fprintf(stderr, "Argumentos:\n");
    fprintf(stderr, "  <cantidad_N>      Número de enteros a generar (ej. 1000000).\n");
    fprintf(stderr, "  <archivo_salida>  Nombre del archivo de salida.\n");
    fprintf(stderr, "  [semilla]         (Opcional) Semilla para reproducibilidad.\n\n");
    fprintf(stderr, "Ejemplo para generar 1 millón de números:\n");
    fprintf(stderr, "  %s 1000000 datos_1M.txt\n", prog_name);
    fprintf(stderr, "Ejemplo con rango y semilla:\n");
    fprintf(stderr, "  %s 50000 data_50k.txt 0 100000 42\n", prog_name);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 6) {
        print_usage_and_exit(argv[0]);
    }

    long N_long = atol(argv[1]); // Usar atol para soportar números grandes
    if (N_long <= 0 || N_long > 100000000) { // Límite práctico
        fprintf(stderr, "Error: La cantidad de números debe ser un entero positivo (límite práctico 100M).\n");
        return EXIT_FAILURE;
    }
    int N = (int)N_long;

    const char *output_filename = argv[2];

    long long min_val = INT_MIN;
    long long max_val = INT_MAX;

    if (argc >= 5) {
        min_val = atoll(argv[3]);
        max_val = atoll(argv[4]);
        if (min_val >= max_val) {
            fprintf(stderr, "Error: <valor_min> (%lld) debe ser estrictamente menor que <valor_max> (%lld).\n", min_val, max_val);
            return EXIT_FAILURE;
        }
    }

    unsigned long long range_size = (unsigned long long)max_val - min_val + 1;
    if (range_size < (unsigned long long)N) {
        fprintf(stderr, "Error: No se pueden generar %d números únicos en un rango de solo %llu valores.\n", N, range_size - 1);
        fprintf(stderr, "Asegúrate de que (valor_max - valor_min) sea >= cantidad_N.\n");
        return EXIT_FAILURE;
    }

    unsigned int seed;
    if (argc == 4) { // ./prog N file seed
        seed = (unsigned int)atoi(argv[3]);
    } else if (argc == 6) { // ./prog N file min max seed
        seed = (unsigned int)atoi(argv[5]);
        printf("Usando semilla proporcionada: %u\n", seed);
    } else {
        seed = (unsigned int)time(NULL);
        printf("Usando semilla basada en el tiempo actual: %u\n", seed);
    }
    srand(seed);

    FILE *file = fopen(output_filename, "w");
    if (!file) {
        perror("Error abriendo el archivo de salida");
        return EXIT_FAILURE;
    }

    // Escribir N en la primera línea
    fprintf(file, "%d\n", N);

    // --- Decisión de Estrategia ---
    // Si el rango es muy grande comparado con N, la tabla hash es más eficiente en memoria.
    // Si el rango es manejable, el barajado es más rápido.
    // Un buen umbral es cuando el rango es unas pocas veces más grande que N.
    // Usamos 4*N como heurística.
    if (range_size < (unsigned long long)4 * N && range_size < 200000000UL) { // Límite práctico de memoria
        printf("Rango pequeño detectado. Usando estrategia de barajado (Fisher-Yates).\n");
        generate_with_shuffle(N, min_val, max_val, file);
    } else {
        printf("Rango grande detectado. Usando estrategia de tabla hash.\n");
        generate_with_hash_table(N, min_val, max_val, file);
    }

    fprintf(file, "\n"); // Añadimos un salto de línea final por buena práctica
    fclose(file);

    printf("¡Archivo '%s' generado exitosamente!\n", output_filename);

    return EXIT_SUCCESS;
}

/**
 * @brief Baraja un arreglo de enteros usando el algoritmo Fisher-Yates.
 */
static void shuffle(int *array, size_t n) {
    if (n > 1) {
        for (size_t i = n - 1; i > 0; i--) {
            size_t j = (size_t)(rand_ull() % (i + 1)); // Modulo bias es aceptable aquí
            int temp = array[j];
            array[j] = array[i];
            array[i] = temp;
        }
    }
}

/**
 * @brief Genera N números únicos usando el método de "generar y probar" con una tabla hash.
 *        Ideal para rangos muy grandes donde N es relativamente pequeño.
 */
static void generate_with_hash_table(int N, long long min_val, long long max_val, FILE* file) {
    // --- Tabla Hash para verificar unicidad ---
    // Un buen tamaño es ~2x la cantidad de elementos y primo para minimizar colisiones.
    int ht_size = next_prime(N * 2);
    HashTable* used_numbers = create_hash_table(ht_size);
    if (!used_numbers) {
        perror("Error creando la tabla hash");
        // No podemos usar exit() si esta función es llamada desde main,
        // así que simplemente retornamos. El archivo estará incompleto.
        return;
    }

    unsigned long long range = (unsigned long long)max_val - min_val + 1;

    for (int i = 0; i < N; ) {
        unsigned long long rand_val = rand_ull();
        long long num_ll = min_val + (rand_val % range);
        int num = (int)num_ll;

        if (!hash_search(used_numbers, num)) {
            hash_insert(used_numbers, num);
            if (fprintf(file, "%d ", num) < 0) { // Cambiado \n por un espacio
                perror("Error escribiendo en el archivo");
                break; // Salir del bucle en caso de error
            }
            i++; // Solo incrementar si el número fue único y se escribió
        }
    }

    free_hash_table(used_numbers);
}

/**
 * @brief Genera N números únicos creando un arreglo con todos los valores del rango,
 *        barajándolo y tomando los primeros N.
 *        Ideal para rangos de tamaño moderado.
 */
static void generate_with_shuffle(int N, long long min_val, long long max_val, FILE* file) {
    unsigned long long range_size_ull = (unsigned long long)max_val - min_val + 1;
    size_t range_size = (size_t)range_size_ull;
    
    int *all_numbers = (int *)malloc(range_size * sizeof(int));
    if (!all_numbers) {
        perror("Error de asignación de memoria para el barajado. El rango puede ser demasiado grande para este método");
        return;
    }

    for (size_t i = 0; i < range_size; i++) {
        all_numbers[i] = (int)(min_val + i);
    }

    shuffle(all_numbers, range_size);

    for (int i = 0; i < N; i++) {
        if (fprintf(file, "%d ", all_numbers[i]) < 0) {
            perror("Error escribiendo un número en el archivo");
            break;
        }
    }

    free(all_numbers);
}

static bool is_prime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int i = 5; i * i <= n; i = i + 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return false;
    }
    return true;
}

static int next_prime(int n) {
    if (n <= 1) return 2;
    int prime = n;
    while (true) { if (is_prime(prime)) return prime; prime++; }
}
