#define BENCHMARK "BK OSU MPI%s Allreduce PAP Latency Test"
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

int main(int argc, char *argv[])
{
    int i, numprocs, rank, size;
    double latency = 0.0, t_start = 0.0, t_stop = 0.0;
    double timer=0.0;
    double avg_time = 0.0, max_time = 0.0, min_time = 0.0;
    float *sendbuf, *recvbuf;
    int po_ret;
    int errors = 0;
    size_t bufsize;
    options.bench = COLLECTIVE;
    options.subtype = LAT_PAP;
    
    // variables for calculating and applying an imbalance factor
    double p2p_lat_s_time = 0.0, lat_t_total = 0.0, p2p_latency = 0.0;
    double imbalance_factor = 0.0, im_s_time = 0.0, im_f_time = 0.0; 
    MPI_Status lat_req_stat;

    set_header(HEADER);
    set_benchmark_name("bk_osu_pap_allreduce");
    po_ret = process_options(argc, argv);

    if (PO_OKAY == po_ret && NONE != options.accel) {
        if (init_accel()) {
            fprintf(stderr, "Error initializing device\n");
            exit(EXIT_FAILURE);
        }
    }

    MPI_CHECK(MPI_Init(&argc, &argv));
    MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &numprocs));
    
    srand((int)MPI_Wtime() + rank);

    switch (po_ret) {
        case PO_BAD_USAGE:
            print_bad_usage_message(rank);
            MPI_CHECK(MPI_Finalize());
            exit(EXIT_FAILURE);
        case PO_HELP_MESSAGE:
            print_help_message(rank);
            MPI_CHECK(MPI_Finalize());
            exit(EXIT_SUCCESS);
        case PO_VERSION_MESSAGE:
            print_version_message(rank);
            MPI_CHECK(MPI_Finalize());
            exit(EXIT_SUCCESS);
        case PO_OKAY:
            break;
    }

    if (numprocs < 2) {
        if (rank == 0) {
            fprintf(stderr, "This test requires at least two processes\n");
        }

        MPI_CHECK(MPI_Finalize());
        exit(EXIT_FAILURE);
    }

    if (options.max_message_size > options.max_mem_limit) {
        if (rank == 0) {
            fprintf(stderr, "Warning! Increase the Max Memory Limit to be able to run up to %ld bytes.\n"
                            "Continuing with max message size of %ld bytes\n", 
                            options.max_message_size, options.max_mem_limit);
        }
        options.max_message_size = options.max_mem_limit;
    }

    options.min_message_size /= sizeof(float);
    if (options.min_message_size < MIN_MESSAGE_SIZE) {
        options.min_message_size = MIN_MESSAGE_SIZE;
    }

    bufsize = sizeof(float)*(options.max_message_size/sizeof(float));
    if (allocate_memory_coll((void**)&sendbuf, bufsize, options.accel)) {
        fprintf(stderr, "Could Not Allocate Memory [rank %d]\n", rank);
        MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE));
    }
    set_buffer(sendbuf, options.accel, 1, bufsize);

    bufsize = sizeof(float)*(options.max_message_size/sizeof(float));
    if (allocate_memory_coll((void**)&recvbuf, bufsize, options.accel)) {
        fprintf(stderr, "Could Not Allocate Memory [rank %d]\n", rank);
        MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE));
    }
    set_buffer(recvbuf, options.accel, 0, bufsize);

    print_preamble(rank);
    if (rank == 0)printf("# BK OSU Allreduce MIF %.2f\n", options.max_imbalance_factor);

    for (size=options.min_message_size; size*sizeof(float) <= options.max_message_size; size *= 2) {

        if (size > LARGE_MESSAGE_SIZE) {
            options.skip = options.skip_large;
            options.iterations = options.iterations_large;
        }
        
        // do a p2p lat test with rank (world_size - 1) to get lat for current msize
        if(rank == 0){
            p2p_lat_s_time = MPI_Wtime();
            for (int lat_i = 0; lat_i < options.iterations + options.skip; lat_i++){
                MPI_Send(sendbuf, size, MPI_FLOAT, numprocs-1, 102, MPI_COMM_WORLD);
                MPI_Recv(recvbuf, size, MPI_FLOAT, numprocs-1, 102, MPI_COMM_WORLD, &lat_req_stat);
            }
            lat_t_total = MPI_Wtime() - p2p_lat_s_time;
            p2p_latency = (lat_t_total) / (2.0 * (double)(options.iterations+options.skip));
        }else if (rank == (numprocs - 1)){
            for (int lat_i = 0; lat_i < options.iterations + options.skip; lat_i++){
                MPI_Recv(recvbuf, size, MPI_FLOAT, 0, 102, MPI_COMM_WORLD, &lat_req_stat);
                MPI_Send(sendbuf, size, MPI_FLOAT, 0, 102, MPI_COMM_WORLD);
            }
        }
        
        MPI_Bcast(&p2p_latency, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

        // MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
        // print_stats(rank, size * sizeof(float), p2p_latency*1e6, p2p_latency, p2p_lat_s_time);

        MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

        timer=0.0;
        for (i=0; i < options.iterations + options.skip ; i++) {
            if (options.validate) {
                set_buffer_float(sendbuf, 1, size, i, options.accel);
                set_buffer_float(recvbuf, 0, size, i, options.accel);
                MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
            }

            // calculate imbalance factor
            if(0 == rank){
                imbalance_factor = 0;
            }
			else if(1 == rank){
                imbalance_factor = options.max_imbalance_factor;
            }
            else{
               imbalance_factor = ((double)rand()/(double)RAND_MAX) * options.max_imbalance_factor;
            }
            // apply imbalance factor
			/*im_s_time = MPI_Wtime();*/
			/*im_f_time = im_s_time + imbalance_factor * p2p_latency;*/
			/*while(im_f_time > MPI_Wtime());*/
			int64_t sleep_time = (int64_t)(p2p_latency * imbalance_factor * 1e6);
			usleep(sleep_time);

            t_start = MPI_Wtime();
            MPI_CHECK(MPI_Allreduce(sendbuf, recvbuf, size, MPI_FLOAT, MPI_SUM, MPI_COMM_WORLD ));
            t_stop=MPI_Wtime();

            if (options.validate) {
                errors += validate_reduction(recvbuf, size, i, numprocs, options.accel);
            }

            if (i>=options.skip){
                timer+=t_stop-t_start;
            }
            MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
        }
        latency = (double)(timer * 1e6) / options.iterations;

        MPI_CHECK(MPI_Reduce(&latency, &min_time, 1, MPI_DOUBLE, MPI_MIN, 0,
                MPI_COMM_WORLD));
        MPI_CHECK(MPI_Reduce(&latency, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0,
                MPI_COMM_WORLD));
        MPI_CHECK(MPI_Reduce(&latency, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0,
                MPI_COMM_WORLD));
        avg_time = avg_time/numprocs;

        if (options.validate) {
            print_stats_validate(rank, size * sizeof(float), avg_time, min_time,
                                max_time, errors);
        } else {
            print_stats(rank, size * sizeof(float), avg_time, min_time, max_time);
        }
        MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
    }

    free_buffer(sendbuf, options.accel);
    free_buffer(recvbuf, options.accel);

    MPI_CHECK(MPI_Finalize());

    if (NONE != options.accel) {
        if (cleanup_accel()) {
            fprintf(stderr, "Error cleaning up device\n");
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}
