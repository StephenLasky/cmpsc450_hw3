// to compile (no openMP): g++ uniq_str.cc -o uniq_str

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <string>

#ifdef _OPENMP
#include <omp.h>
#endif
#include "qsort.h"

int stephen_find_uniq(char **B, int num_strings, int * counts); // header for my function
int combine_partition(int p2_start, int p2_end, int * counts, char ** B, int uniq1, int uniq2); // header
int kamesh_find_uniq(char **B, int num_strings, int * counts); // header

void print_arr(char ** a, int n) {
    int i;
    for (i=0; i<n; i++)
        printf("'%s' ", a[i]);
    printf("\n");
}

void print_arr(int * a, int n) {
    int i;
    for (i=0; i<n; i++)
        printf("%d ", a[i]);
    printf("\n");
}

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

void stephen_merge_sort(char ** a, int n) {
    //printf("call n=%d ",n);
    //print_arr(a,n);
    
    // IF the fragment is small we sort the small fragment
    if (n <= 1) {
        return;
    }
    else if (n == 2) {
//        if (*a[0] > *a[1]) {
        if (strcmp(a[0], a[1]) > 0) {
//            printf("%s > %s so swap\n", a[0], a[1]);
            //printf("Pre-sort ");
            //print_arr(a,n);
            char * temp = a[0];
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
    
    
//    //#pragma omp task// firstprivate (a,n)
//    const int PARALLEL_LIMIT = 512; // only spawn threads at a minimum threshold to prevent too much thread overhead
//    if (PARALLEL_LIMIT>512) {
//#pragma omp parallel sections
//        {
//#pragma omp section
//            stephen_merge_sort(&a[0], n/2);     // left side
//#pragma omp section
//            stephen_merge_sort(&a[n/2], n-n/2); // right side
//        }
//    }
//    else {
        stephen_merge_sort(&a[0], n/2);     // left side
        stephen_merge_sort(&a[n/2], n-n/2); // right side
//    }
    
    //printf("pre-merge\n");
    //printf("left: ");
    //print_arr(a,n/2);
    //printf("right: ");
    //print_arr(&a[n/2], n-n/2);
    
    // now we must merge the two fragments
    // first we add a temporary array to hold the two fragments
    char ** temp_a = (char **)malloc(n * sizeof(char *));
    int temp_a_i = 0;
    int left_i = 0;
    int right_i = n/2;
    while (left_i < n/2 || right_i < n) {
        // case 1. we have not reached the end of EITHER fragment
        if (left_i < n/2 && right_i < n) {
//            if (*a[left_i] < *a[right_i]) {
            if (strcmp(a[left_i], a[right_i]) < 0) {
                temp_a[temp_a_i] = a[left_i];
//                printf("%s < %s so %s goes next\n", a[left_i], a[right_i], a[left_i]);
                left_i ++;
            }
            else {
                temp_a[temp_a_i] = a[right_i];
//                printf("%s >= %s so %s goes next\n", a[left_i], a[right_i], a[right_i]);
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
    
    memcpy(a, temp_a, n * sizeof(char **));   // copy our work to the main array
    free(temp_a);           // free the temporary array
    
    //printf("post-merge: ");
    //print_arr(a,n);
    
    // the fragements for this section are now successfully sorted. we now return.
    return;
    
}



/* comparison routine for C's qsort */
static int qs_cmpf(const void *u, const void *v) {

    const char **u_s = (const char **) u;
    const char **v_s = (const char **) v;
    return (strcmp(*u_s, *v_s));

}

/* inline QSORT() comparison routine */
#define inline_qs_cmpf(a,b) (strcmp((*a),(*b)) < 0)

/* comparison routine for STL sort */
class compare_str_cmpf {
    public:
        bool operator() (char *u, char *v) {

            int cmpval = strcmp(u, v);

            if (cmpval < 0)
                return true;
            else
                return false;
        }
};

int find_uniq_qsort(char *str_array, const int str_array_size, 
        const int num_strings, const int num_iterations) {

    fprintf(stderr, "N %d\n", num_strings);
    fprintf(stderr, "Using C qsort\n");
    fprintf(stderr, "Execution times (ms) for %d iterations:\n", num_iterations);

    int iter;
    double avg_elt;

    char **B;
    B = (char **) malloc(num_strings * sizeof(char *));
    assert(B != NULL);

    int *counts;
    counts = (int *) malloc(num_strings * sizeof(int));
    int *kamesh_counts;
    kamesh_counts = (int *) malloc(num_strings * sizeof(int));

    avg_elt = 0.0;

    for (iter = 0; iter < num_iterations; iter++) {
        
        int i, j;

        B[0] = &str_array[0];
        j = 1;
        for (i=0; i<str_array_size-1; i++) {
            if (str_array[i] == '\0') {
                B[j] = &str_array[i+1];
                j++;    
            }
        }
        assert(j == num_strings);

        for (i=0; i<num_strings; i++) {
            counts[i] = 0;
            kamesh_counts[i] = 0;
        }

        double elt;
        elt = timer();

//        qsort(B, num_strings, sizeof(char *), qs_cmpf);
        //printf("### PRE SORT ###\n");
        //print_arr(B,num_strings);
        stephen_merge_sort(B, num_strings);
        //printf("### POST SORT ###\n");
        //print_arr(B,num_strings);

        /*
        for (i=0; i<num_strings; i++) {
            fprintf(stderr, "%s\n", B[i]);
        }
        */

//        /* determine number of unique strings 
//           and count of each */
//        int num_uniq_strings = 1;
//        int string_occurrence_count = 1;
//        for (i=1; i<num_strings; i++) {
//            if (strcmp(B[i], B[i-1]) != 0) {
//                num_uniq_strings++;
//                counts[i-1] = string_occurrence_count;
//                string_occurrence_count = 1;
//            } else {
//                string_occurrence_count++;
//            }
//        }
//        counts[num_strings-1] = string_occurrence_count;
        
        /* parallel version */
        /* determine number of unique strings and count each */
        const int NUM_THREADS = 4;
        int * num_uniq_strings = (int *)malloc(sizeof(int) * NUM_THREADS);
        for (i=0; i<NUM_THREADS; i++) {
            int partition_num_strings = num_strings / NUM_THREADS;
            int start_position = partition_num_strings * i;
            if (i==NUM_THREADS-1)
                partition_num_strings += num_strings - partition_num_strings * NUM_THREADS;
            
            num_uniq_strings[i] = stephen_find_uniq(&B[start_position], partition_num_strings, &counts[start_position]);
        }
//        printf("before inconsistency fix: \n");
//        print_arr(counts, num_strings);
        
        /* now fix the inconsistensies */
        for (i=1; i<NUM_THREADS; i++) {
            int partition_num_strings = num_strings / NUM_THREADS;
            int start_position = partition_num_strings * i;
            if (i==NUM_THREADS-1)
                partition_num_strings += num_strings - partition_num_strings * NUM_THREADS;
            
            int end_position = start_position + partition_num_strings -1;
            num_uniq_strings[0] = combine_partition(start_position, end_position, counts, B, num_uniq_strings[0], num_uniq_strings[i]);
            
        }
        
        kamesh_find_uniq(B, num_strings, kamesh_counts);
        
//        printf("after inconsistency fix: \n");
//        print_arr(counts, num_strings);
//        print_arr(kamesh_counts, num_strings);
        
        
                                                    

        elt = timer() - elt;
        avg_elt += elt;
        fprintf(stderr, "%9.3lf\n", elt*1e3);


        /* optionally print out unique strings */
        /*
        for (i=0; i<num_strings; i++) {
            if (counts[i] != 0) {
                fprintf(stderr, "%s\t%d\n", B[i], counts[i]);
            }
        }
        fprintf(stderr, "Number of unique strings: %d\n", num_uniq_strings);
        */

        /* a complete correctness check */
                                                    
//        int total_strings = counts[0];
        for (i=1; i<num_strings; i++) {
            assert(strcmp(B[i], B[i-1]) >= 0);
            if (strcmp(B[i], B[i-1]) != 0) {
                if (counts[i-1] != kamesh_counts[i-1]) {
                    printf("%d %d\n", counts[i-1], kamesh_counts[i-1]);
//                    print_arr(counts, num_strings);
//                    print_arr(kamesh_counts, num_strings);
                }
                assert(counts[i-1] == kamesh_counts[i-1]);
                
            }
//            total_strings += counts[i];
            /* assure the counts to complete the check */
            
        }
//        assert(total_strings == num_strings);
                                                    
    }
                                                    
    avg_elt = avg_elt/num_iterations;
    
    free(B);
    free(counts);

    fprintf(stderr, "Average time: %9.3lf ms.\n", avg_elt*1e3);
    fprintf(stderr, "Average sort rate: %6.3lf MB/s\n", str_array_size/(avg_elt*1e6));
    return 0;
                                                    
}
                                                    
// returns the number of unique strings in this partition
int stephen_find_uniq(char **B, int num_strings, int * counts) {
//    printf("stephen called: ")
    int num_uniq_strings = 1;
    int string_occurrence_count = 1;
    int i;
    for (i=1; i<num_strings; i++) {
        if (strcmp(B[i], B[i-1]) != 0) {
            num_uniq_strings++;
            counts[i-1] = string_occurrence_count;
            counts[i-string_occurrence_count] = string_occurrence_count;
            string_occurrence_count = 1;
        } else {
            string_occurrence_count++;
        }
    }
    counts[num_strings-1] = string_occurrence_count;
    counts[num_strings-string_occurrence_count] = string_occurrence_count;
                   
    
    return num_uniq_strings;
}

// original counts
int kamesh_find_uniq(char **B, int num_strings, int * counts) {
    int num_uniq_strings = 1;
    int string_occurrence_count = 1;
    int i;
    for (i=1; i<num_strings; i++) {
        if (strcmp(B[i], B[i-1]) != 0) {
            num_uniq_strings++;
            counts[i-1] = string_occurrence_count;
            string_occurrence_count = 1;
        } else {
            string_occurrence_count++;
        }
    }
    counts[num_strings-1] = string_occurrence_count;
    
    return num_uniq_strings;
}


// it is assumed that this function is executed left to right
// because the number of partitions is small this function should be executed serially
int combine_partition(int p2_start, int p2_end, int * counts, char ** B, int uniq1, int uniq2) {
    // first we see if there is a problem
    if (strcmp(B[p2_start], B[p2_start-1]) != 0) {
        // if there is no problem, that is, the partitions did not cut a continuous partition, we just do the math and return
        return uniq1 + uniq2;
    }
    // fi there is a problem, then we have some work to do
    else {
        // first we make the continuous segment consistent
        int new_count = counts[p2_start-1] + counts[p2_start];
        counts[p2_start + counts[p2_start] -1] = new_count;
        return uniq1 + uniq2 - 1;
    }
}
                                                    


int find_uniq_inline_qsort(char *str_array, const int str_array_size,
        const int num_strings, const int num_iterations) {
    
    printf("Hello!\n");

    fprintf(stderr, "N %d\n", num_strings);
    fprintf(stderr, "Using inline qsort\n");
    fprintf(stderr, "Execution times (ms) for %d iterations:\n", num_iterations);

    int iter;
    double avg_elt;

    char **B;
    B = (char **) malloc(num_strings * sizeof(char *));
    assert(B != NULL);

    int *counts;
    counts = (int *) malloc(num_strings * sizeof(int));

    avg_elt = 0.0;

    for (iter = 0; iter < num_iterations; iter++) {
        
        int i, j;

        B[0] = &str_array[0];
        j = 1;
        for (i=0; i<str_array_size-1; i++) {
            if (str_array[i] == '\0') {
                B[j] = &str_array[i+1];
                j++;    
            }
        }
        assert(j == num_strings);

        for (i=0; i<num_strings; i++) {
            counts[i] = 0;
        }

        double elt;
        elt = timer();

        QSORT(char*, B, num_strings, inline_qs_cmpf);

        /*
        for (i=0; i<num_strings; i++) {
            fprintf(stderr, "%s\n", B[i]);
        }
        */

        /* determine number of unique strings 
           and count of each string */
        int num_uniq_strings = 1;
        int string_occurrence_count = 1;
        for (i=1; i<num_strings; i++) {
            if (strcmp(B[i], B[i-1]) != 0) {
                num_uniq_strings++;
                counts[i-1] = string_occurrence_count;
                string_occurrence_count = 1;
            } else {
                string_occurrence_count++;
            }
        }
        counts[num_strings-1] = string_occurrence_count;

        /* optionally print out unique strings */
        /*
        for (i=0; i<num_strings; i++) {
            if (counts[i] != 0) {
                fprintf(stderr, "%s\t%d\n", B[i], counts[i]);
            }
        }
        fprintf(stderr, "Number of unique strings: %d\n", num_uniq_strings);
        */

        elt = timer() - elt;
        avg_elt += elt;
        fprintf(stderr, "%9.3lf\n", elt*1e3);

        /* an incomplete correctness check */
        int total_strings = counts[0];
        for (i=1; i<num_strings; i++) {
            assert(strcmp(B[i], B[i-1]) >= 0);
            total_strings += counts[i];
        }
        assert(total_strings == num_strings);

    }

    avg_elt = avg_elt/num_iterations;
    
    free(B);
    free(counts);

    fprintf(stderr, "Average time: %9.3lf ms.\n", avg_elt*1e3);
    fprintf(stderr, "Average sort rate: %6.3lf MB/s\n", str_array_size/(avg_elt*1e6));


    return 0;

}

int find_uniq_stl_sort(char *str_array, const int str_array_size,
        const int num_strings, const int num_iterations) {

    fprintf(stderr, "N %d\n", num_strings);
    fprintf(stderr, "Using STL sort\n");
    fprintf(stderr, "Execution times (ms) for %d iterations:\n", num_iterations);

    int iter;
    double avg_elt;

    char **B;
    B = (char **) malloc(num_strings * sizeof(char *));
    assert(B != NULL);

    int *counts;
    counts = (int *) malloc(num_strings * sizeof(int));

    avg_elt = 0.0;

    for (iter = 0; iter < num_iterations; iter++) {
        
        int i, j;

        B[0] = &str_array[0];
        j = 1;
        for (i=0; i<str_array_size-1; i++) {
            if (str_array[i] == '\0') {
                B[j] = &str_array[i+1];
                j++;    
            }
        }
        assert(j == num_strings);

        for (i=0; i<num_strings; i++) {
            counts[i] = 0;
        }

        double elt;
        elt = timer();

        compare_str_cmpf cmpf;
        std::sort(B, B+num_strings, cmpf);

        /*
        for (i=0; i<num_strings; i++) {
            fprintf(stderr, "%s\n", B[i]);
        }
        */

        /* determine number of unique strings 
           and count of each string */
        int num_uniq_strings = 1;
        int string_occurrence_count = 1;
        for (i=1; i<num_strings; i++) {
            if (strcmp(B[i], B[i-1]) != 0) {
                num_uniq_strings++;
                counts[i-1] = string_occurrence_count;
                string_occurrence_count = 1;
            } else {
                string_occurrence_count++;
            }
        }
        counts[num_strings-1] = string_occurrence_count;

        /* optionally print out unique strings */
        /*
        for (i=0; i<num_strings; i++) {
            if (counts[i] != 0) {
                fprintf(stderr, "%s\t%d\n", B[i], counts[i]);
            }
        }
        fprintf(stderr, "Number of unique strings: %d\n", num_uniq_strings);
        */

        elt = timer() - elt;
        avg_elt += elt;
        fprintf(stderr, "%9.3lf\n", elt*1e3);

        /* an incomplete correctness check */
        int total_strings = counts[0];
        for (i=1; i<num_strings; i++) {
            assert(strcmp(B[i], B[i-1]) >= 0);
            total_strings += counts[i];
        }
        assert(total_strings == num_strings);

    }

    avg_elt = avg_elt/num_iterations;
    
    free(B);
    free(counts);

    fprintf(stderr, "Average time: %9.3lf ms.\n", avg_elt*1e3);
    fprintf(stderr, "Average sort rate: %6.3lf MB/s\n", str_array_size/(avg_elt*1e6));

    return 0;

}

int find_uniq_stl_map(char *str_array, int str_array_size, const int num_strings, const int num_iterations) {

    fprintf(stderr, "N %d\n", num_strings);
    fprintf(stderr, "Using a map\n");
    fprintf(stderr, "Execution times (ms) for %d iterations:\n", num_iterations);

    int iter;
    double avg_elt;

    char **B;
    B = (char **) malloc(num_strings * sizeof(char *));
    assert(B != NULL);

    int *counts;
    counts = (int *) malloc(num_strings * sizeof(int));

    avg_elt = 0.0;

    for (iter = 0; iter < num_iterations; iter++) {
        
        int i, j;

        B[0] = &str_array[0];
        j = 1;
        for (i=0; i<str_array_size-1; i++) {
            if (str_array[i] == '\0') {
                B[j] = &str_array[i+1];
                j++;    
            }
        }
        assert(j == num_strings);

        for (i=0; i<num_strings; i++) {
            counts[i] = 0;
        }

        double elt;
        elt = timer();

        std::map<std::string, int> str_map;

        for (i=0; i<num_strings; i++) {
            std::string curr_str(B[i]);
            //curr_str.assign(B[i], strlen(B[i]));
            str_map[curr_str]++;
        }

        fprintf(stderr, "Number of unique strings: %d\n", 
                ((int) str_map.size()));

        elt = timer() - elt;
        avg_elt += elt;
        fprintf(stderr, "%9.3lf\n", elt*1e3);

    }

    avg_elt = avg_elt/num_iterations;
    
    free(B);
    free(counts);

    fprintf(stderr, "Average time: %9.3lf ms.\n", avg_elt*1e3);
    fprintf(stderr, "Average sort rate: %6.3lf MB/s\n", str_array_size/(avg_elt*1e6));

    return 0;


    return 0;

}

int main(int argc, char **argv) {

    if (argc != 4) {
        fprintf(stderr, "%s <input file> <n> <alg_type>\n", argv[0]);
        fprintf(stderr, "alg_type 0: use C qsort, then find unique strings\n");
        fprintf(stderr, "         1: use inline qsort, then find unique strings\n");
        fprintf(stderr, "         2: use STL sort, then find unique strings\n");
        fprintf(stderr, "         3: use STL map\n");
        exit(1);
    }

    char *filename = argv[1];
    
    int num_strings;
    num_strings = atoi(argv[2]);

    /* get file size */
    struct stat file_stat;
    stat(filename, &file_stat);
    int file_size_bytes = file_stat.st_size;
    fprintf(stderr, "File size: %d bytes\n", file_size_bytes);

    /* load all strings from file */
    FILE *infp;
    infp = fopen(filename, "r");
    if (infp == NULL) {
        fprintf(stderr, "Error: Couldn't open file!\n");
        exit(2);
    }
    
    char *str_array = (char *) malloc(file_size_bytes * sizeof(char));
    fread(str_array, sizeof(char), file_size_bytes, infp);
    fclose(infp);

    /* replace end of line characters with string delimiters */
    int i;
    int num_strings_in_file = 0;
    for (i=0; i<file_size_bytes; i++) {
        if (str_array[i] == '\n') {
            str_array[i] = '\0';
            num_strings_in_file++;
        }
    }
    fprintf(stderr, "num strings read %d\n", num_strings_in_file);
    assert(num_strings == num_strings_in_file);

    int alg_type = atoi(argv[3]);
    assert((alg_type >= 0) && (alg_type <= 3));
    
    int num_iterations = 10;

    if (alg_type == 0) {
        find_uniq_qsort(str_array, file_size_bytes, num_strings, num_iterations);
    } else if (alg_type == 1) {
        find_uniq_inline_qsort(str_array, file_size_bytes, num_strings, num_iterations);
    } else if (alg_type == 2) {
        find_uniq_stl_sort(str_array, file_size_bytes, num_strings, num_iterations);
    } else if (alg_type == 3) {
        find_uniq_stl_map(str_array, file_size_bytes, num_strings, num_iterations);
    }

    free(str_array);

    return 0;
}
