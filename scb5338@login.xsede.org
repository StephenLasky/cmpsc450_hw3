// to compile :     gcc-6 -fopenmp flt_val_sort.c -o flt_val_sort

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "qsort.h"

void stephen_merge_sort(float * a, int n);  // header for my merge sort
void print_arr(float * a, int n);           // header for printing the array

static double timer() {
    
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return ((double) (tp.tv_sec) + 1e-6 * tp.tv_usec);

    /* The code below is for another high resolution timer */
    /* I'm using gettimeofday because it's more portable */

    /*
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return ((double) (tp.tv_sec) + 1e-9 * tp.tv_nsec);
    */
}

/* comparison routine for C's qsort */
static int qs_cmpf(const void *u, const void *v) {

    if (*(float *)u > *(float *)v)
        return 1;
    else if (*(float *)u < *(float *)v)
        return -1;
    else
        return 0;
}

/* inline QSORT() comparison routine */
#define inline_qs_cmpf(a,b) ((*a)<(*b))


static int inline_qsort_serial(const float *A, const int n, const int num_iterations) {

    fprintf(stderr, "N %d\n", n);
    fprintf(stderr, "Using inline qsort implementation\n");
    fprintf(stderr, "Execution times (ms) for %d iterations:\n", num_iterations);

    int iter;
    double avg_elt;

    float *B;
    B = (float *) malloc(n * sizeof(float));
    assert(B != NULL);

    avg_elt = 0.0;

    for (iter = 0; iter < num_iterations; iter++) {
        
        int i;

        for (i=0; i<n; i++) {
            B[i] = A[i];
        }

        double elt;
        elt = timer();

        QSORT(float, B, n, inline_qs_cmpf);

        elt = timer() - elt;
        avg_elt += elt;
        fprintf(stderr, "%9.3lf\n", elt*1e3);

        /* correctness check */
        for (i=1; i<n; i++) {
            assert(B[i] >= B[i-1]);
        }

    }

    avg_elt = avg_elt/num_iterations;
    
    free(B);

    fprintf(stderr, "Average time: %9.3lf ms.\n", avg_elt*1e3);
    fprintf(stderr, "Average sort rate: %6.3lf MB/s\n", 4.0*n/(avg_elt*1e6));
    return 0;

}

static int qsort_serial(const float *A, const int n, const int num_iterations) {

    fprintf(stderr, "N %d\n", n);
    fprintf(stderr, "Using C qsort\n");
    fprintf(stderr, "Execution times (ms) for %d iterations:\n", num_iterations);

    int iter;
    double avg_elt;

    float *B;
    B = (float *) malloc(n * sizeof(float));
    assert(B != NULL);

    avg_elt = 0.0;

    for (iter = 0; iter < num_iterations; iter++) {
        
        int i;

        for (i=0; i<n; i++) {
            B[i] = A[i];
        }

        double elt;
        elt = timer();
        
//        printf("pre-sort: ");
//        print_arr(B,n);

//        qsort(B, n, sizeof(float), qs_cmpf);
//#pragma omp parallel
//        {
//#pragma omp single
        stephen_merge_sort(B, n);
//        }
        

        elt = timer() - elt;
        avg_elt += elt;
        fprintf(stderr, "%9.3lf\n", elt*1e3);
        
//        printf("post-sort: ");
//        print_arr(B,n);

        /* correctness check */
        for (i=1; i<n; i++) {
            assert(B[i] >= B[i-1]);
        }

    }

    avg_elt = avg_elt/num_iterations;
    
    free(B);

    fprintf(stderr, "Average time: %9.3lf ms.\n", avg_elt*1e3);
    fprintf(stderr, "Average sort rate: %6.3lf MB/s\n", 4.0*n/(avg_elt*1e6));
    return 0;

}



/* generate different inputs for testing sort */
int gen_input(float *A, int n, int input_type) {

    int i;

    /* uniform random values */
    if (input_type == 0) {

        srand(123);
        for (i=0; i<n; i++) {
            A[i] = ((float) rand())/((float) RAND_MAX);
        }

    /* sorted values */    
    } else if (input_type == 1) {

        for (i=0; i<n; i++) {
            A[i] = (float) i;
        }

    /* almost sorted */    
    } else if (input_type == 2) {

        for (i=0; i<n; i++) {
            A[i] = (float) i;
        }

        /* do a few shuffles */
        int num_shuffles = (n/100) + 1;
        srand(1234);
        for (i=0; i<num_shuffles; i++) {
            int j = (rand() % n);
            int k = (rand() % n);

            /* swap A[j] and A[k] */
            float tmpval = A[j];
            A[j] = A[k];
            A[k] = tmpval;
        }

    /* array with single unique value */    
    } else if (input_type == 3) {

        for (i=0; i<n; i++) {
            A[i] = 1.0;
        }

    /* sorted in reverse */    
    } else {

        for (i=0; i<n; i++) {
            A[i] = (float) (n + 1.0 - i);
        }

    }

    return 0;

}

void print_arr(float * a, int n) {
    int i;
    for (i=0; i<n; i++)
        printf("%f ", a[i]);
    printf("\n");
}

void stephen_merge_sort(float * a, int n) {
    //printf("call n=%d ",n);
    //print_arr(a,n);
    
    // IF the fragment is small we sort the small fragment
    if (n <= 1) {
        return;
    }
    else if (n == 2) {
        if (a[0] > a[1]) {
            //printf("Pre-sort ");
            //print_arr(a,n);
            float temp = a[0];
            a[0] = a[1];
            a[1] = temp;
            //printf("Post-sort ");
            //print_arr(a,n);
        }
        return;
    }
    

    
    // OTHERWISE the fragment is large (n>2) and so we must split the fragment
    // Boundary calculations:
    //          START   SIZE
    //  LEFT    0       n/2
    //  RIGHT   n/2     n-n/2
    
    
//#pragma omp task// firstprivate (a,n)
    const int PARALLEL_LIMIT = 512; // only spawn threads at a minimum threshold to prevent too much thread overhead
    if (PARALLEL_LIMIT>512) {
#pragma omp parallel sections
    {
#pragma omp section
    stephen_merge_sort(&a[0], n/2);     // left side
#pragma omp section
    stephen_merge_sort(&a[n/2], n-n/2); // right side
    }
    }
    else {
        stephen_merge_sort(&a[0], n/2);     // left side
        stephen_merge_sort(&a[n/2], n-n/2); // right side
    }
    
    //printf("pre-merge\n");
    //printf("left: ");
    //print_arr(a,n/2);
    //printf("right: ");
    //print_arr(&a[n/2], n-n/2);
    
    // now we must merge the two fragments
    // first we add a temporary array to hold the two fragments
    float * temp_a = (float *)malloc(n * sizeof(float));
    int temp_a_i = 0;
    int left_i = 0;
    int right_i = n/2;
    while (left_i < n/2 || right_i < n) {
        // case 1. we have not reached the end of EITHER fragment
        if (left_i < n/2 && right_i < n) {
            if (a[left_i] < a[right_i]) {
                temp_a[temp_a_i] = a[left_i];
                left_i ++;
            }
            else {
                temp_a[temp_a_i] = a[right_i];
                right_i ++;
            }
        }
        // case 2. we have not reached the end of ONLY the left side
        else if (left_i < n/2) {
            temp_a[temp_a_i] = a[left_i];
            left_i ++;
        }
        // case 3. we have not reached the end of ONLY the right side
        else {
            temp_a[temp_a_i] = a[right_i];
            right_i ++;
        }
        temp_a_i ++;
    }
    
    //printf("temp array: ");
    //print_arr(temp_a, n);
    
    memcpy(a, temp_a, n * sizeof(float));   // copy our work to the main array
    free(temp_a);           // free the temporary array
    
    //printf("post-merge: ");
    //print_arr(a,n);
    
    // the fragements for this section are now successfully sorted. we now return.
    return;
    
}


int main(int argc, char **argv) {

    if (argc != 4) {
        fprintf(stderr, "%s <n> <input_type> <alg_type>\n", argv[0]);
        fprintf(stderr, "input_type 0: uniform random\n");
        fprintf(stderr, "           1: already sorted\n");
        fprintf(stderr, "           2: almost sorted\n");
        fprintf(stderr, "           3: single unique value\n");
        fprintf(stderr, "           4: sorted in reverse\n");
        fprintf(stderr, "alg_type 0: use C qsort\n");
        fprintf(stderr, "         1: use inline qsort\n");
        exit(1);
    }
//    printf("num threads: %d\n", omp_num_threads());

    int n;

    n = atoi(argv[1]);

    assert(n > 0);
    assert(n <= 1000000000);

    float *A;
    A = (float *) malloc(n * sizeof(float));
    assert(A != 0);

    int input_type = atoi(argv[2]);
    assert(input_type >= 0);
    assert(input_type <= 4);

    gen_input(A, n, input_type);

    int alg_type = atoi(argv[3]);

    int num_iterations = 10;
    
    assert((alg_type == 0) || (alg_type == 1));

    if (alg_type == 0) {
        qsort_serial(A, n, num_iterations);
    } else if (alg_type == 1) {    
        inline_qsort_serial(A, n, num_iterations);
    }

    free(A);

    return 0;
}
