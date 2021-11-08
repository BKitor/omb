#define BENCHMARK "OSU MPI%s Reduce Latency Test"
/*
 * Copyright (C) 2002-2019 the Network-Based Computing Laboratory
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
	float x=1;
	double latency = 0.0, t_start = 0.0, t_stop = 0.0;
	double timer=0.0;
	double latency2 = 0.0, t_start2 = 0.0, t_stop2 = 0.0;
	double timer2=0.0;
	double latency3 = 0.0, t_start3 = 0.0, t_stop3 = 0.0;
	double timer3=0.0;
	double avg_time = 0.0, max_time = 0.0, min_time = 0.0;
	double avg_time2 = 0.0;
	double avg_time3 = 0.0;
	float *sendbuf, *recvbuf;
	int po_ret;
	size_t bufsize;
	set_header(HEADER);
	set_benchmark_name("osu_reduce");
	options.bench = COLLECTIVE;
	options.subtype = LAT;
	//*******mycode********
	int rand_val;
	double random_val;
	int seed;
	volatile long compute=0, a=0, b=0, c=0;
	//*******mycode********
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
	//*******mycode********
	if (rank==0) printf("imbalanced micro_benchmark11\n");
	seed = 123456789 + rank*1000;
	srand(seed);
	random_val = (int) 100*((double)rand()/(double)RAND_MAX);
	rand_val= (int) random_val;
	//*******mycode********
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
	if(numprocs < 2) {
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
	if (allocate_memory_coll((void**)&recvbuf, bufsize,
				options.accel)) {
		fprintf(stderr, "Could Not Allocate Memory [rank %d]\n", rank);
		MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE));
	}
	set_buffer(recvbuf, options.accel, 1, bufsize);
	bufsize = sizeof(float)*(options.max_message_size/sizeof(float));
	if (allocate_memory_coll((void**)&sendbuf, bufsize,
				options.accel)) {
		fprintf(stderr, "Could Not Allocate Memory [rank %d]\n", rank);
		MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE));
	}
	set_buffer(sendbuf, options.accel, 0, bufsize);
	print_preamble(rank);
	for(size=options.min_message_size; size*sizeof(float) <= options.max_message_size; size *= 2) {
		if(size > LARGE_MESSAGE_SIZE) {
			options.skip = options.skip_large;
			options.iterations = options.iterations_large;
		}
		// using the value of the “x” variable, I make the appropriate MIF for each message size!
		if ((4 <= size*4)&& (size*4 <= 1024))
		{x=2;}
		else if ((2048 <= size*4)&& (size*4 <= 4896))
		{x=4;}
		else if (size*4 == 8192)
		{x=7;}
		else if (size*4 == 16384)
		{x=14;}
		else if (size*4 == 32768)
		{x=20;}
		else if (size*4 == 65536)
		{x=24;}
		else if (size*4 == 131072)
		{x=50;}
		else if (size*4 == 262144)
		{x=100;}
		else if (size*4 == 524288)
		{x=150;}
		else if (size*4 == 1048576)
		{x=260;}
		else if (size*4 == 2097152)
		{x=530;}
		else if (size*4 == 4194304)
		{x=1060;}
		else if (size*4 == 8388608)
		{x=3500;}
		else if (size*4 == 16777216)
		{x=9200;}
		else if (size*4 == 33554432)
		{x=18000;}
		else if (size*4 == 67108864)
		{x=40000;}
		MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
		timer=0.0;
		timer2=0.0;
		timer3=0.0;
		for(i=0; i < options.iterations + options.skip ; i++) {
			//*******mycode********
			t_start3 = MPI_Wtime();
			for (int l=0; l<rand_val; l++)
				for (int g=0; g<1; g++)
					for (long k=0; k<100*x; k++)
						for (long f = 0; f < 2 ; f++){ a=a+1;
							b=b+1;
							c=c+1;
							compute=compute+(a*b+c);
						}
			t_stop3=MPI_Wtime();
			if(i>=options.skip){
				timer3+=t_stop3-t_start3;
			}
			//}
			//*******mycode********
			t_start = MPI_Wtime();
			MPI_CHECK(MPI_Reduce(sendbuf, recvbuf, size, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD
						));
			t_stop=MPI_Wtime();
			if(i>=options.skip){
				timer+=t_stop-t_start;
			}
			MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
	}
	latency = (double)(timer * 1e6) / options.iterations;
	latency3 = (double) (timer3 * 1e6)/options.iterations;
	MPI_CHECK(MPI_Reduce(&latency, &min_time, 1, MPI_DOUBLE, MPI_MIN, 0,
				MPI_COMM_WORLD));
	MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
	MPI_CHECK(MPI_Reduce(&latency, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0,
				MPI_COMM_WORLD));
	MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
	MPI_CHECK(MPI_Reduce(&latency, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0,
				MPI_COMM_WORLD));
	MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
	MPI_CHECK(MPI_Reduce(&latency3, &avg_time3, 1, MPI_DOUBLE, MPI_SUM, 0,
				MPI_COMM_WORLD));
	MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
	avg_time = avg_time/numprocs;
	avg_time3 = avg_time3/numprocs;
	//if (rank==0) printf("Hello average time 3 is :%f\n", avg_time3);
	print_stats(rank, size * sizeof(float), avg_time, min_time, max_time);
	MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
}
if (rank==0) printf("Hello average time 3 is :%f\n", avg_time3);
free_buffer(recvbuf, options.accel);
free_buffer(sendbuf, options.accel);
MPI_CHECK(MPI_Finalize());
if (NONE != options.accel) {
	if (cleanup_accel()) {
		fprintf(stderr, "Error cleaning up device\n");
		exit(EXIT_FAILURE);
	}
}
return EXIT_SUCCESS;
}
