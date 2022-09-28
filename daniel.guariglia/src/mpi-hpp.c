/*
* Author: Guariglia Daniel 0000916433
* 
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h> /* for ceil() */
#include <assert.h>
#include <time.h>
#include <mpi.h>

typedef enum
{
    WALL,
    GAS,
    EMPTY
} cell_value_t;

typedef enum
{
    ODD_PHASE = -1,
    EVEN_PHASE = 1
} phase_t;

/* type of a cell of the domain */
typedef unsigned char cell_t;

/* Simplifies indexing on a N*N grid */
int IDX(int i, int j, int N)
{
    /* wrap-around */
    i = (i + N) % N;
    j = (j + N) % N;
    return i * N + j;
}

/* Swap the content of cells a and b, provided that neither is a WALL;
   otherwise, do nothing. */
void swap_cells(cell_t *a, cell_t *b)
{
    if ((*a != WALL) && (*b != WALL))
    {
        const cell_t tmp = *a;
        *a = *b;
        *b = tmp;
    }
}

/* Compute the `next` grid given the `cur`-rent configuration. */
void step(const cell_t *cur, cell_t *next, int Nrow, int Ncol, phase_t phase, int my_rank)
{
    int i, j;

    assert(cur != NULL);
    assert(next != NULL);

    /* Loop over all coordinates (i,j) s.t. both i and j are even */
    for (i = 0; i < Nrow; i += 2)
    {
        for (j = 0; j < Ncol; j += 2)
        {
            /**
             * If phase==EVEN_PHASE:
             * ab
             * cd
             *
             * If phase==ODD_PHASE:
             * dc
             * ba
             */
            int a = 0;
            int b = 0;
            int c = 0;
            int d = 0;
            // se è la fase pari calcolo gli indici "nel modo classico"
            if (phase == EVEN_PHASE)
            {
                a = IDX(i, j, Ncol);
                b = IDX(i, j + phase, Ncol);
                c = IDX(i + phase, j, Ncol);
                d = IDX(i + phase, j + phase, Ncol);
            }

            // se la fase è dispari calcolo gli indici in modo diverso

            if (phase == ODD_PHASE)
            {
                // se sto considerando la prima colonna calcolo gli indici considerando il wrap
                if (j == 0)
                {
                    a = IDX(i + 1, j, Ncol);
                    b = IDX(i + 1, j + phase, Ncol);
                    c = IDX(i, j, Ncol);
                    d = IDX(i, j + phase, Ncol);
                }
                else
                {
                    a = IDX(i + 1, j, Ncol);
                    b = IDX(i + 1, j + phase, Ncol);
                    c = IDX(i, j, Ncol);
                    d = IDX(i, j + phase, Ncol);
                }
            }
            next[a] = cur[a];
            next[b] = cur[b];
            next[c] = cur[c];
            next[d] = cur[d];
            if ((((next[a] == EMPTY) != (next[b] == EMPTY)) &&
                 ((next[c] == EMPTY) != (next[d] == EMPTY))) ||
                (next[a] == WALL) || (next[b] == WALL) ||
                (next[c] == WALL) || (next[d] == WALL))
            {
                swap_cells(&next[a], &next[b]);
                swap_cells(&next[c], &next[d]);
            }
            else
            {
                swap_cells(&next[a], &next[d]);
                swap_cells(&next[b], &next[c]);
            }
        }
    }
}

/**
 ** The functions below are used to draw onto the grid; since they are
 ** called during initialization only, they do not need to be
 ** parallelized.
 **/
void box(cell_t *grid, int N, float x1, float y1, float x2, float y2, cell_value_t t)
{
    const int ix1 = ceil(fminf(x1, x2) * N);
    const int ix2 = ceil(fmaxf(x1, x2) * N);
    const int iy1 = ceil(fminf(y1, y1) * N);
    const int iy2 = ceil(fmaxf(y1, y2) * N);
    int i, j;
    for (i = iy1; i <= iy2; i++)
    {
        for (j = ix1; j <= ix2; j++)
        {
            const int ij = IDX(N - 1 - i, j, N);
            grid[ij] = t;
        }
    }
}

void circle(cell_t *grid, int N, float x, float y, float r, cell_value_t t)
{
    const int ix = ceil(x * N);
    const int iy = ceil(y * N);
    const int ir = ceil(r * N);
    int dx, dy;
    for (dy = -ir; dy <= ir; dy++)
    {
        for (dx = -ir; dx <= ir; dx++)
        {
            if (dx * dx + dy * dy <= ir * ir)
            {
                const int ij = IDX(N - 1 - iy - dy, ix + dx, N);
                grid[ij] = t;
            }
        }
    }
}

void random_fill(cell_t *grid, int N, float x1, float y1, float x2, float y2, float p)
{
    const int ix1 = ceil(fminf(x1, x2) * N);
    const int ix2 = ceil(fmaxf(x1, x2) * N);
    const int iy1 = ceil(fminf(y1, y1) * N);
    const int iy2 = ceil(fmaxf(y1, y2) * N);
    int i, j;
    for (i = iy1; i <= iy2; i++)
    {
        for (j = ix1; j <= ix2; j++)
        {
            const int ij = IDX(N - 1 - i, j, N);
            if (grid[ij] == EMPTY && ((float)rand()) / RAND_MAX < p)
                grid[ij] = GAS;
        }
    }
}

void read_problem(FILE *filein, cell_t *grid, int N)
{
    int i, j;
    int nread;
    char op;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < N; j++)
        {
            const int ij = IDX(i, j, N);
            grid[ij] = EMPTY;
        }
    }

    while ((nread = fscanf(filein, " %c", &op)) == 1)
    {
        int t;
        float x1, y1, x2, y2, r, p;
        int retval;

        switch (op)
        {
        case 'c': /* circle */
            retval = fscanf(filein, "%f %f %f %d", &x1, &y1, &r, &t);
            assert(retval == 4);
            circle(grid, N, x1, y1, r, t);
            break;
        case 'b': /* box */
            retval = fscanf(filein, "%f %f %f %f %d", &x1, &y1, &x2, &y2, &t);
            assert(retval == 5);
            box(grid, N, x1, y1, x2, y2, t);
            break;
        case 'r': /* random_fill */
            retval = fscanf(filein, "%f %f %f %f %f", &x1, &y1, &x2, &y2, &p);
            assert(retval == 5);
            random_fill(grid, N, x1, y1, x2, y2, p);
            break;
        default:
            fprintf(stderr, "FATAL: Unrecognized command `%c`\n", op);
            exit(EXIT_FAILURE);
        }
    }
}

/* Write an image of `grid` to a file in PGM (Portable Graymap)
   format. `frameno` is the time step number, used for labeling the
   output file. */
void write_image(const cell_t *grid, int N, int frameno)
{
    FILE *f;
    char fname[128];

    snprintf(fname, sizeof(fname), "hpp%05d.pgm", frameno);
    if ((f = fopen(fname, "w")) == NULL)
    {
        printf("Cannot open \"%s\" for writing\n", fname);
        abort();
    }
    fprintf(f, "P5\n");
    fprintf(f, "# produced by hpp\n");
    fprintf(f, "%d %d\n", N, N);
    fprintf(f, "%d\n", EMPTY); /* highest shade of grey (0=black) */
    fwrite(grid, 1, N * N, f);
    fclose(f);
}
void invert_row_for_EVEN(cell_t *my_next, int *sendcnts, int N, int comm_sz, int my_rank)
{
    // scambio delle ghost cells
    // i processi di indice pari inviano al thread di indice più piccolo la loro prima riga
    if (my_rank == 0)
    {
        // invio al processo di rank 1 l'ultima riga
        // ricevo dall'ultimo la nuova prima riga
        MPI_Send(
            &my_next[sendcnts[my_rank] * 2 * N], // buf
            N,                                   // count
            MPI_UNSIGNED_CHAR,                   // data type
            1,                                   // dest
            0,                                   // tag
            MPI_COMM_WORLD);

        MPI_Recv(
            my_next,           // buf
            N,                 // count
            MPI_UNSIGNED_CHAR, // data type
            comm_sz - 1,       // source
            MPI_ANY_TAG,       // tag
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE // status
        );
    }
    if (my_rank == comm_sz - 1)
    {
        // ricevo dal processo comm_sz -2 la nuova prima riga
        // invio al processo 0 l'ultima
        MPI_Recv(
            my_next,           // buf
            N,                 // count
            MPI_UNSIGNED_CHAR, // data type
            my_rank - 1,       // source
            MPI_ANY_TAG,       // tag
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE // status
        );

        MPI_Send(
            &my_next[sendcnts[my_rank] * 2 * N], // buf
            N,                                   // count
            MPI_UNSIGNED_CHAR,                   // data type
            0,                                   // dest
            0,                                   // tag
            MPI_COMM_WORLD);
    }
    if (my_rank > 0 && my_rank < comm_sz - 1)
    {
        if (my_rank % 2 == 0)
        {
            // invio a my_rank +1 l'ultima riga
            // ricevo da my_rank -1 la prima
            MPI_Send(
                &my_next[sendcnts[my_rank] * 2 * N], // buf
                N,                                   // count
                MPI_UNSIGNED_CHAR,                   // data type
                my_rank + 1,                         // dest
                0,                                   // tag
                MPI_COMM_WORLD);

            MPI_Recv(
                my_next,           // buf
                N,                 // count
                MPI_UNSIGNED_CHAR, // data type
                my_rank - 1,       // source
                MPI_ANY_TAG,       // tag
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE // status
            );
        }
        if (my_rank % 2 == 1)
        {
            // ricevo da my_rank -1 la prima
            // invio a my_rank +1 l'ultima
            MPI_Recv(
                my_next,           // buf
                N,                 // count
                MPI_UNSIGNED_CHAR, // data type
                my_rank - 1,       // source
                MPI_ANY_TAG,       // tag
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE // status
            );

            MPI_Send(
                &my_next[sendcnts[my_rank] * 2 * N], // buf
                N,                                   // count
                MPI_UNSIGNED_CHAR,                   // data type
                my_rank + 1,                         // dest
                0,                                   // tag
                MPI_COMM_WORLD);
        }
    }
}

void invert_row_for_ODD(cell_t *my_next, int *sendcnts, int N, int comm_sz, int my_rank)
{
    // scambio delle righe
    // i processi di indice pari inviano al thread di indice più piccolo la loro prima riga
    // successivamente ricevono una riga in ultima posizione 
    if (my_rank == 0)
    {
        // invio al ultimo processo
        // ricevo dall'uno
        MPI_Send(
            my_next,           // buf
            N,                 // count
            MPI_UNSIGNED_CHAR, // data type
            comm_sz - 1,       // dest
            0,                 // tag
            MPI_COMM_WORLD);

        MPI_Recv(
            &my_next[sendcnts[my_rank] * 2 * N], // buf
            N,                                   // count
            MPI_UNSIGNED_CHAR,                   // data type
            1,                                   // source
            MPI_ANY_TAG,                         // tag
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE // status
        );
    }
    if (my_rank == comm_sz - 1)
    {
        // ricevo dal processo 0
        // invio al processo comm_sz -2
        MPI_Recv(
            &my_next[sendcnts[my_rank] * 2 * N], // buf
            N,                                   // count
            MPI_UNSIGNED_CHAR,                   // data type
            0,                                   // source
            MPI_ANY_TAG,                         // tag
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE // status
        );

        MPI_Send(
            my_next,           // buf
            N,                 // count
            MPI_UNSIGNED_CHAR, // data type
            my_rank - 1,       // dest
            0,                 // tag
            MPI_COMM_WORLD);
    }
    if (my_rank > 0 && my_rank < comm_sz - 1)
    {
        if (my_rank % 2 == 0)
        {
            // invio a my_rank -1
            // ricevo da my_rank +1
            MPI_Send(
                my_next,           // buf
                N,                 // count
                MPI_UNSIGNED_CHAR, // data type
                my_rank - 1,       // dest
                0,                 // tag
                MPI_COMM_WORLD);

            MPI_Recv(
                &my_next[sendcnts[my_rank] * 2 * N], // buf
                N,                                   // count
                MPI_UNSIGNED_CHAR,                   // data type
                my_rank + 1,                         // source
                MPI_ANY_TAG,                         // tag
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE // status
            );
        }
        if (my_rank % 2 == 1)
        {
            // ricevo da my_rank +1
            // invio a my_rank -1
            MPI_Recv(
                &my_next[sendcnts[my_rank] * 2 * N], // buf
                N,                                   // count
                MPI_UNSIGNED_CHAR,                   // data type
                my_rank + 1,                         // source
                MPI_ANY_TAG,                         // tag
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE // status
            );

            MPI_Send(
                my_next,           // buf
                N,                 // count
                MPI_UNSIGNED_CHAR, // data type
                my_rank - 1,       // dest
                0,                 // tag
                MPI_COMM_WORLD);
        }
    }
}

void reconstruct_domain(cell_t *cur, cell_t *my_dom, int *sendcnts, int *displs, int N, int comm_sz, int my_rank, MPI_Datatype *two_row)
{
    // l'ultima riga di ogni dominio non è da considerare
    // l'ultima riga valida dell'ultimo processo è la prima riga del dominio complessivo

    /**
     * l'ultimo processo manda al processo 0 la sua penultima riga (la prima del dom complessivo)
     * ogni processo effettua una gatherv e invia i propi valori
     */
    if (my_rank == 0)
    {
        // ricevo la prima riga dall'ultimo processo nella prima posizione di cur
        MPI_Recv(
            cur,               // buf
            N,                 // count
            MPI_UNSIGNED_CHAR, // data type
            comm_sz - 1,       // source
            MPI_ANY_TAG,       // tag
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE // status
        );
        // ricevo l'ultima riga dall'ultimo processo
        MPI_Recv(
            &cur[N * (N - 1)], // buf  ricevo nel ultima riga
            N,                 // count
            MPI_UNSIGNED_CHAR, // data type
            comm_sz - 1,       // source
            MPI_ANY_TAG,       // tag
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE // status
        );
        sendcnts[comm_sz - 1]--;
    }
    if (my_rank == comm_sz - 1)
    {
        // invio la penultima riga al processo 0 che la riceve come prima riga
        MPI_Send(
            &my_dom[((sendcnts[my_rank] * 2) - 1) * N], // buf
            N,                                          // count
            MPI_UNSIGNED_CHAR,                          // data type
            0,                                          // dest
            0,                                          // tag
            MPI_COMM_WORLD);
        // invio la terzultima riga al processo 0 che la riceve come ultima
        MPI_Send(
            &my_dom[((sendcnts[my_rank] * 2) - 2) * N], // buf terzultima riga
            N,                                          // count
            MPI_UNSIGNED_CHAR,                          // data type
            0,                                          // dest
            0,                                          // tag
            MPI_COMM_WORLD);
        // abbasso il sendcont di uno
        sendcnts[my_rank]--;
    }

    // gather dei risultati verso il th 0

    MPI_Gatherv(
        my_dom,            // const void *sendbuf
        sendcnts[my_rank], // int sendcount
        *two_row,          // MPI_Datatype sendtype
        &cur[N],           // void *recvbuf         -> la prima riga è già completa
        sendcnts,          // const int recvcounts[]
        displs,            // const int displs[]
        *two_row,          // MPI_Datatype recvtype
        0,                 // int root
        MPI_COMM_WORLD     // MPI_Comm comm
    );

    // correggo il sendcont dell'ultimo processo per il prossimo ciclo
    if (my_rank == comm_sz - 1)
    {
        sendcnts[my_rank]++;
    }
}

int main(int argc, char *argv[])
{
    int t, N, nsteps;
    FILE *filein;
    int my_rank, comm_sz;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);

    //variabili per il calcolo del tempo di esecuzione
    double begin = 0;
    double end;

    if(my_rank == 0){
        begin = MPI_Wtime();
    }
    srand(1234); /* Initialize PRNG deterministically */

    if ((argc < 2) || (argc > 4))
    {
        fprintf(stderr, "Usage: %s [N [S]] input\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc > 2)
    {
        N = atoi(argv[1]);
    }
    else
    {
        N = 512;
    }

    if (argc > 3)
    {
        nsteps = atoi(argv[2]);
    }
    else
    {
        nsteps = 32;
    }

    if (N % 2 != 0)
    {
        fprintf(stderr, "FATAL: the domain size N must be even\n");
        return EXIT_FAILURE;
    }

    if (comm_sz > N / 2)
    {
        fprintf(stderr, "FATAL: number of MPI-process %d must be <= of domain size/2 (%d) \n", comm_sz, N / 2);
        return EXIT_FAILURE;
    }

    if ((filein = fopen(argv[argc - 1], "r")) == NULL)
    {
        fprintf(stderr, "FATAL: can not open \"%s\" for reading\n", argv[argc - 1]);
        return EXIT_FAILURE;
    }
    
    //datatype usato per semplificare lo scambio di dati
    MPI_Datatype two_row;
    int two_row_dim = 2 * N;
    MPI_Type_contiguous(two_row_dim, MPI_UNSIGNED_CHAR, &two_row);
    MPI_Type_commit(&two_row);

    cell_t *cur = NULL;
    const size_t GRID_SIZE = N * N * sizeof(cell_t);
    // array di offset (in row)
    int *displs = NULL;   
    // array contatore elementi da inviare (in two_row)
    int *sendcnts = NULL; 

    displs = (int *)malloc(comm_sz * sizeof(int));
    sendcnts = (int *)malloc(comm_sz * sizeof(int)); 

    int i;
    //calcolo array per l'invio di dati per scatterv e gatherv
    for (i = 0; i < comm_sz; i++)
    {
        int start = ((N / 2) * i) / comm_sz;
        int end = ((N / 2) * (i + 1)) / comm_sz;
        sendcnts[i] = end - start;
        displs[i] = start;
    }

    //il processo 0 carica il dominio
    if (my_rank == 0)
    {
        cur = (cell_t *)malloc(GRID_SIZE);
        assert(cur != NULL);
        read_problem(filein, cur, N);
    }
    // ogni processo crea il proprio dominio e next (compresa una riga aggiuntiva)
    cell_t *my_dom = (cell_t *)malloc(N + (sendcnts[my_rank] * 2 * N) * sizeof(cell_t));
    cell_t *my_next = (cell_t *)malloc(N + (sendcnts[my_rank] * 2 * N) * sizeof(cell_t));
    assert(my_next != NULL);

    for (t = 0; t < nsteps; t++)
    {
        // set di sendcnts e displs
        for (i = 0; i < comm_sz; i++)
        {
            int start = ((N / 2) * i) / comm_sz;
            int end = ((N / 2) * (i + 1)) / comm_sz;
            sendcnts[i] = end - start;
            displs[i] = start;
        }

        // uso la scatterv per distribuire i dati
        MPI_Scatterv(
            cur,               // senedbuf
            sendcnts,          // sendcount
            displs,            // offsets
            two_row,           // datatype
            my_dom,            // recvbuf
            sendcnts[my_rank], // recvcount
            two_row,           // recv data type
            0,                 // root
            MPI_COMM_WORLD);

#ifdef DUMP_ALL
        if (my_rank == 0)
        {
            write_image(cur, N, t);
        }
#endif
        //esecuzione della fase pari (viene esclusa l'ultima riga)
        step(my_dom, my_next, sendcnts[my_rank] * 2, N, EVEN_PHASE, my_rank);
        //scambio di righe tra processi
        invert_row_for_ODD(my_next, sendcnts, N, comm_sz, my_rank);
        //esecuzione della fase dispari (viene esclusa la prima riga)
        step(&my_next[N], my_dom, sendcnts[my_rank] * 2, N, ODD_PHASE, my_rank);
        //viene ricostruito il dominio complessivo nel processo 0
        reconstruct_domain(cur, my_dom, sendcnts, displs, N, comm_sz, my_rank, &two_row);
    }
#ifdef DUMP_ALL
    /* Reverse all particles and go back to the initial state */
    for (; t < 2 * nsteps; t++)
    {
        if (my_rank == 0)
        {
            printf("%d \n", t - nsteps);
            write_image(cur, N, t);
        }
        // set di sendcnts e displs

        for (i = 0; i < comm_sz; i++)
        {
            int start = ((N / 2) * i) / comm_sz;
            int end = ((N / 2) * (i + 1)) / comm_sz;
            sendcnts[i] = end - start;
            displs[i] = start;
        }

        // uso la scatterv per distribuire i dati
        MPI_Scatterv(
            cur,               // senedbuf
            sendcnts,          // sendcount
            displs,            // offsets
            two_row,           // datatype
            my_dom,            // recvbuf
            sendcnts[my_rank], // recvcount
            two_row,           // recv data type
            0,                 // root
            MPI_COMM_WORLD);

            //scambi di righe in preparazione alla fase dispari
        invert_row_for_ODD(my_dom, sendcnts, N, comm_sz, my_rank);
        step(&my_dom[N], my_next, sendcnts[my_rank] * 2, N, ODD_PHASE, my_rank);
        //il dom viene ricostruito nel processo 0
        reconstruct_domain(cur, my_next, sendcnts, displs, N, comm_sz, my_rank, &two_row);
        
        for (i = 0; i < comm_sz; i++)
        {
            int start = ((N / 2) * i) / comm_sz;
            int end = ((N / 2) * (i + 1)) / comm_sz;
            sendcnts[i] = end - start;
            displs[i] = start;
        }

        //il dom viene diviso per l'esecuzione della fase pari
        MPI_Scatterv(
            cur,               // senedbuf
            sendcnts,          // sendcount
            displs,            // offsets
            two_row,           // datatype
            my_dom,            // recvbuf
            sendcnts[my_rank], // recvcount
            two_row,           // recv data type
            0,                 // root
            MPI_COMM_WORLD);

            step(my_dom, my_next, sendcnts[my_rank] * 2, N, EVEN_PHASE, my_rank);

            //il dom complessivo viene ricostruito nel processo 0
            MPI_Gatherv(
            my_next,            // const void *sendbuf
            sendcnts[my_rank], // int sendcount
            two_row,           // MPI_Datatype sendtype
            cur,               // void *recvbuf        
            sendcnts,          // const int recvcounts[]
            displs,            // const int displs[]
            two_row,           // MPI_Datatype recvtype
            0,                 // int root
            MPI_COMM_WORLD     // MPI_Comm comm
        );

    }
#endif
    if (my_rank == 0)
    {
        write_image(cur, N, t);
    }
    if(cur != NULL){
        free(cur);
    }
    if(my_next != NULL){
        free(my_next);
    }
    if(my_dom != NULL){
        free(my_dom);
    }

    if(my_rank == 0){
        end = MPI_Wtime();
        double time_spent = (double)(end - begin);
        printf("Elapsed time: %lf \n", time_spent);
    }

    fclose(filein);
    MPI_Finalize();
    return EXIT_SUCCESS;
}
