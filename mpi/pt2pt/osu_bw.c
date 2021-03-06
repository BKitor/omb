#define BENCHMARK "OSU MPI%s Bandwidth Test"
/*
 * Copyright (C) 2002-2021 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University. 
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */

#include <osu_util_mpi.h>

#ifdef _ENABLE_CUDA_KERNEL_
double measure_kernel_lo(char **, int, int);
void touch_managed_src(char **, int, int);
void touch_managed_dst(char **, int, int);
#endif /* #ifdef _ENABLE_CUDA_KERNEL_ */
double calculate_total(double, double, double, int);

int
main (int argc, char *argv[])
{
    int myid, numprocs, i, j, k;
    int size;
    char **s_buf, **r_buf;
    double t_start = 0.0, t_end = 0.0, t_lo = 0.0, t_total = 0.0;
    int window_size = 64;
    int po_ret = 0;
    options.bench = PT2PT;
    options.subtype = BW;

    set_header(HEADER);
    set_benchmark_name("osu_bw");

    po_ret = process_options(argc, argv);
    if (PO_OKAY == po_ret && NONE != options.accel) {
        if (init_accel()) {
            fprintf(stderr, "Error initializing device\n");
            exit(EXIT_FAILURE);
        }
    }

    window_size = options.window_size;
    if (options.buf_num == MULTIPLE) {
        s_buf = malloc(sizeof(char *) * window_size);
        r_buf = malloc(sizeof(char *) * window_size);
    } else {
        s_buf = malloc(sizeof(char *) * 1);
        r_buf = malloc(sizeof(char *) * 1);
    }
    
    MPI_CHECK(MPI_Init(&argc, &argv));
    MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &numprocs));
    MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &myid));

    if (0 == myid) {
        switch (po_ret) {
            case PO_CUDA_NOT_AVAIL:
                fprintf(stderr, "CUDA support not enabled.  Please recompile "
                        "benchmark with CUDA support.\n");
                break;
            case PO_OPENACC_NOT_AVAIL:
                fprintf(stderr, "OPENACC support not enabled.  Please "
                        "recompile benchmark with OPENACC support.\n");
                break;
            case PO_BAD_USAGE:
                print_bad_usage_message(myid);
                break;
            case PO_HELP_MESSAGE:
                print_help_message(myid);
                break;
            case PO_VERSION_MESSAGE:
                print_version_message(myid);
                MPI_CHECK(MPI_Finalize());
                exit(EXIT_SUCCESS);
            case PO_OKAY:
                break;
        }
    }

    switch (po_ret) {
        case PO_CUDA_NOT_AVAIL:
        case PO_OPENACC_NOT_AVAIL:
        case PO_BAD_USAGE:
            MPI_CHECK(MPI_Finalize());
            exit(EXIT_FAILURE);
        case PO_HELP_MESSAGE:
        case PO_VERSION_MESSAGE:
            MPI_CHECK(MPI_Finalize());
            exit(EXIT_SUCCESS);
        case PO_OKAY:
            break;
    }

    if (numprocs != 2) {
        if (myid == 0) {
            fprintf(stderr, "This test requires exactly two processes\n");
        }

        MPI_CHECK(MPI_Finalize());
        exit(EXIT_FAILURE);
    }

#ifdef _ENABLE_CUDA_KERNEL_
    if (options.src == 'M' || options.dst == 'M') {
        if (options.buf_num == SINGLE) {
            fprintf(stderr, "Warning: Tests involving managed buffers will use multiple buffers by default\n");
        }
        options.buf_num = MULTIPLE;
    }
#endif /* #ifdef _ENABLE_CUDA_KERNEL_ */

    if (options.buf_num == SINGLE) {
        if (allocate_memory_pt2pt(&s_buf[0], &r_buf[0], myid)) {
            /* Error allocating memory */
            MPI_CHECK(MPI_Finalize());
            exit(EXIT_FAILURE);
        }
    }

    print_header(myid, BW);

    /* Bandwidth test */
    for (size = options.min_message_size; size <= options.max_message_size; size *= 2) {
        if (options.buf_num == MULTIPLE) {
            for (i = 0; i < window_size; i++) {
                if (allocate_memory_pt2pt_size(&s_buf[i], &r_buf[i], myid, size)) {
                    /* Error allocating memory */
                    MPI_CHECK(MPI_Finalize());
                    exit(EXIT_FAILURE);
                }
            }
    
            for (i = 0; i < window_size; i++) {
                set_buffer_pt2pt(s_buf[i], myid, options.accel, 'a', size);
                set_buffer_pt2pt(r_buf[i], myid, options.accel, 'b', size);
            }
        } else {
            set_buffer_pt2pt(s_buf[0], myid, options.accel, 'a', size);
            set_buffer_pt2pt(r_buf[0], myid, options.accel, 'b', size);
        }

        if (size > LARGE_MESSAGE_SIZE) {
            options.iterations = options.iterations_large;
            options.skip = options.skip_large;
        }

#ifdef _ENABLE_CUDA_KERNEL_
        if (options.dst == 'M' && options.MMdst == 'D') {
            t_lo = measure_kernel_lo(s_buf, size, window_size);
        }
#endif /* #ifdef _ENABLE_CUDA_KERNEL_ */

        MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
        t_total = 0.0;

        for (i = 0; i < options.iterations + options.skip; i++) {
            if (myid == 0) {
                if (i >= options.skip) {
                    t_start = MPI_Wtime();
                }

#ifdef _ENABLE_CUDA_KERNEL_
                if (options.src == 'M') {
                    touch_managed_src(s_buf, size, window_size);
                }
#endif /* #ifdef _ENABLE_CUDA_KERNEL_ */

                for (j = 0; j < window_size; j++) {
                    if (options.buf_num == SINGLE) {
                        MPI_CHECK(MPI_Isend(s_buf[0], size, MPI_CHAR, 1, 100, MPI_COMM_WORLD,
                                  request + j));
                    } else {
                        MPI_CHECK(MPI_Isend(s_buf[j], size, MPI_CHAR, 1, 100, MPI_COMM_WORLD,
                                  request + j));
                    }
                }
                MPI_CHECK(MPI_Waitall(window_size, request, reqstat));

                MPI_CHECK(MPI_Recv(r_buf[0], 4, MPI_CHAR, 1, 101, MPI_COMM_WORLD,
                          &reqstat[0]));

#ifdef _ENABLE_CUDA_KERNEL_
                if (options.src == 'M') {
                    touch_managed_src(r_buf, size, window_size);
                }
#endif /* #ifdef _ENABLE_CUDA_KERNEL_ */
                if (i >= options.skip) {
                    t_end = MPI_Wtime();
                    t_total += calculate_total(t_start, t_end, t_lo, window_size);
                }
            } else if (myid == 1) {
#ifdef _ENABLE_CUDA_KERNEL_
                if (options.dst == 'M') {
                    touch_managed_dst(s_buf, size, window_size);
                }
#endif /* #ifdef _ENABLE_CUDA_KERNEL_ */
                for (j = 0; j < window_size; j++) {
                    if (options.buf_num == SINGLE) {
                        MPI_CHECK(MPI_Irecv(r_buf[0], size, MPI_CHAR, 0, 100, MPI_COMM_WORLD,
                                  request + j));
                    } else {
                        MPI_CHECK(MPI_Irecv(r_buf[j], size, MPI_CHAR, 0, 100, MPI_COMM_WORLD,
                                  request + j));
                    }
                }
                MPI_CHECK(MPI_Waitall(window_size, request, reqstat));

#ifdef _ENABLE_CUDA_KERNEL_
                if (options.dst == 'M') {
                    touch_managed_dst(r_buf, size, window_size);
                }
#endif /* #ifdef _ENABLE_CUDA_KERNEL_ */

                MPI_CHECK(MPI_Send(s_buf[0], 4, MPI_CHAR, 0, 101, MPI_COMM_WORLD));
            }
        }

        if (myid == 0) {
            double tmp = size / 1e6 * options.iterations * window_size;
            fprintf(stdout, "%-*d%*.*f\n", 10, size, FIELD_WIDTH,
                    FLOAT_PRECISION, tmp / t_total);
            fflush(stdout);
        }

        if (options.buf_num == MULTIPLE) {
            for (i = 0; i < window_size; i++) {
                free_memory(s_buf[i], r_buf[i], myid);
            }
        }
    }

    if (options.buf_num == SINGLE) {
        free_memory(s_buf[0], r_buf[0], myid);
    }
    free(s_buf);
    free(r_buf);

    MPI_CHECK(MPI_Finalize());

    if (NONE != options.accel) {
        if (cleanup_accel()) {
            fprintf(stderr, "Error cleaning up device\n");
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}

#ifdef _ENABLE_CUDA_KERNEL_
double measure_kernel_lo(char **buf, int size, int window_size)
{
    int i;
    double t_lo = 0.0, t_start, t_end;

    for (i = 0; i < 10; i++) {
        launch_empty_kernel(buf[i%window_size], size);
    }

    for (i = 0; i < 1000; i++) {
        t_start = MPI_Wtime();
        launch_empty_kernel(buf[i%window_size], size);
        synchronize_stream();
        t_end = MPI_Wtime();
        t_lo = t_lo + (t_end - t_start);
    }

    t_lo = t_lo/1000;
    return t_lo;
}

void touch_managed_src(char **buf, int size, int window_size)
{
    int j;

    if (options.src == 'M') {
        if (options.MMsrc == 'D') {
            for (j = 0; j < window_size; j++) {
                touch_managed(buf[j], size);
                synchronize_stream();
            }
        } else if ((options.MMsrc == 'H') && size > PREFETCH_THRESHOLD) {
            for (j = 0; j < window_size; j++) {
                prefetch_data(buf[j], size, -1);
                synchronize_stream();
            }
        } else {
            for (j = 0; j < window_size; j++) {
                memset(buf[j], 'c', size);
            }
        }
    }
}

void touch_managed_dst(char **buf, int size, int window_size)
{
    int j;

    if (options.dst == 'M') {
        if (options.MMdst == 'D') {
            for (j = 0; j < window_size; j++) {
                touch_managed(buf[j], size);
                synchronize_stream();
            }
        } else if ((options.MMdst == 'H') && size > PREFETCH_THRESHOLD) {
            for (j = 0; j < window_size; j++) {
                prefetch_data(buf[j], size, -1);
                synchronize_stream();
            }
        } else {
            for (j = 0; j < window_size; j++) {
                memset(buf[j], 'c', size);
            }
        }
    }
}
#endif /* #ifdef _ENABLE_CUDA_KERNEL_ */

double calculate_total(double t_start, double t_end, double t_lo, int window_size)
{
    double t_total;

    if (options.dst == 'M' && options.MMdst == 'D') {
        t_total = ((t_end - t_start) - (t_lo * window_size));
    } else {
        t_total = (t_end - t_start);
    }

    return t_total;
}
