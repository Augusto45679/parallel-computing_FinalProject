# Diagrama de Flujo - QuickSort Paralelo Balanceado

```mermaid
flowchart TD
    Start([Inicio]) --> MPI_Init
    MPI_Init --> RankSize[Obtener rank y size MPI]
    RankSize --> CheckRoot{¿Rank == 0?}
    
    CheckRoot -->|Sí| ReadFile[Leer archivo de entrada]
    ReadFile --> ValidateN{¿N divisible por<br>world_size?}
    ValidateN -->|No| Error[Error y Abort]
    ValidateN -->|Sí| BcastN
    
    CheckRoot -->|No| BcastN[MPI_Bcast de N]
    
    BcastN --> ScatterInit[MPI_Scatter datos iniciales]
    ScatterInit --> ParallelQS[QuickSort Paralelo]
    
    subgraph ParallelQS [Algoritmo QuickSort Paralelo]
        P1{¿Comm_size < 2?}
        P1 -->|Sí| LocalSort[Qsort local y retornar]
        P1 -->|No| CalcMedian[Calcular mediana local]
        CalcMedian --> GatherMedians[MPI_Gather medianas]
        GatherMedians --> RootMedian{¿Rank == 0?}
        RootMedian -->|Sí| SortMedians[Ordenar medianas<br>y calcular pivote]
        RootMedian -->|No| BcastPivot
        SortMedians --> BcastPivot[MPI_Bcast pivote]
        BcastPivot --> Partition[Partición in-place]
        Partition --> DetermineGroup[Determinar grupo<br>0: bajos, 1: altos]
        DetermineGroup --> Exchange[MPI_Sendrecv<br>intercambio datos]
        Exchange --> Reallocate[Reasignar memoria local]
        Reallocate --> SplitComm[MPI_Comm_split]
        SplitComm --> RecursiveCall[Llamada recursiva<br>QuickSort Paralelo]
        RecursiveCall --> FreeComm[MPI_Comm_free]
    end
    
    ParallelQS --> CountPrimes[Contar primos locales]
    CountPrimes --> ReducePrimes[MPI_Reduce<br>conteo de primos]
    ReducePrimes --> GatherSizes[MPI_Gather<br>tamaños finales]
    GatherSizes --> RootDispl{¿Rank == 0?}
    
    RootDispl -->|Sí| CalcDispl[Calcular displacements]
    RootDispl -->|No| GatherV
    CalcDispl --> GatherV[MPI_Gatherv<br>datos ordenados]
    
    GatherV --> Timing[Calcular métricas<br>de tiempo]
    Timing --> GatherTiming[MPI_Gather<br>métricas]
    GatherTiming --> RootOutput{¿Rank == 0?}
    
    RootOutput -->|Sí| PrintResults[Imprimir resultados<br>y estadísticas]
    RootOutput -->|No| MPI_Finalize
    PrintResults --> MPI_Finalize
    Error --> MPI_Finalize
    MPI_Finalize --> End([Fin])