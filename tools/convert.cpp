#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <string>
#include <chrono>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <omp.h>

static inline int parse_int(const char *&p) {
    while (*p == ' ' || *p == '\t') p++;
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    int val = 0;
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }
    return sign * val;
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <text-edge-list> [output.bin]\n", argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    std::string output_str = (argc >= 3) ? argv[2] : std::string(input_path) + ".bin";
    const char *output_path = output_str.c_str();

    int fd_in = open(input_path, O_RDONLY);
    assert(fd_in >= 0);

    struct stat st;
    fstat(fd_in, &st);
    size_t file_size = st.st_size;

    const char *data = (const char *)mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd_in, 0);
    assert(data != MAP_FAILED);
    madvise((void *)data, file_size, MADV_SEQUENTIAL);

    int nthreads = omp_get_max_threads();
    int fd_out = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd_out >= 0);

    auto start = std::chrono::high_resolution_clock::now();

    // Divide file into chunks aligned to line boundaries
    std::vector<size_t> chunk_start(nthreads + 1);
    chunk_start[0] = 0;
    for (int t = 1; t < nthreads; t++) {
        size_t pos = (size_t)t * file_size / nthreads;
        while (pos < file_size && data[pos] != '\n') pos++;
        if (pos < file_size) pos++;
        chunk_start[t] = pos;
    }
    chunk_start[nthreads] = file_size;

    // Pass 1: count edges per chunk
    std::vector<long> edge_counts(nthreads, 0);
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        const char *p = data + chunk_start[tid];
        const char *end = data + chunk_start[tid + 1];
        long count = 0;
        while (p < end) {
            if (*p == '#' || *p == '\n') {
                while (p < end && *p != '\n') p++;
                if (p < end) p++;
                continue;
            }
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
            count++;
        }
        edge_counts[tid] = count;
    }

    // Prefix sum for output offsets
    std::vector<long> edge_offsets(nthreads + 1, 0);
    for (int t = 0; t < nthreads; t++) {
        edge_offsets[t + 1] = edge_offsets[t] + edge_counts[t];
    }
    long total_edges = edge_offsets[nthreads];

    // Pre-allocate output file
    ftruncate(fd_out, total_edges * 2 * sizeof(int));

    // Pass 2: parse and write in parallel
    std::vector<int> max_vids(nthreads, 0);
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        const char *p = data + chunk_start[tid];
        const char *end = data + chunk_start[tid + 1];
        long offset = edge_offsets[tid];
        int local_max = 0;

        const size_t BUF_EDGES = 1 << 16;
        int *buf = new int[BUF_EDGES * 2];
        size_t buf_pos = 0;

        while (p < end) {
            if (*p == '#' || *p == '\n') {
                while (p < end && *p != '\n') p++;
                if (p < end) p++;
                continue;
            }
            int src = parse_int(p);
            int dst = parse_int(p);
            while (p < end && *p != '\n') p++;
            if (p < end) p++;

            buf[buf_pos * 2] = src;
            buf[buf_pos * 2 + 1] = dst;
            buf_pos++;

            if (src > local_max) local_max = src;
            if (dst > local_max) local_max = dst;

            if (buf_pos == BUF_EDGES) {
                pwrite(fd_out, buf, buf_pos * 2 * sizeof(int),
                       offset * 2 * sizeof(int));
                offset += buf_pos;
                buf_pos = 0;
            }
        }
        if (buf_pos > 0) {
            pwrite(fd_out, buf, buf_pos * 2 * sizeof(int),
                   offset * 2 * sizeof(int));
        }
        max_vids[tid] = local_max;
        delete[] buf;
    }

    int global_max = 0;
    for (int t = 0; t < nthreads; t++) {
        if (max_vids[t] > global_max) global_max = max_vids[t];
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double secs = std::chrono::duration<double>(end_time - start).count();

    munmap((void *)data, file_size);
    close(fd_in);
    close(fd_out);

    printf("output_file: %s\n", output_path);
    printf("time: %.6f\n", secs);
    printf("edges: %ld\n", total_edges);
    printf("|V|=%d\n", global_max + 1);
    return 0;
}
