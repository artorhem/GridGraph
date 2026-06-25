#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <chrono>

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <text-edge-list> [output.bin]\n", argv[0]);
        return 1;
    }

    const char *input = argv[1];
    const char *output = (argc >= 3) ? argv[2] : nullptr;
    std::string output_str;
    if (!output) {
        output_str = std::string(input) + ".bin";
        output = output_str.c_str();
    }

    FILE * fin = fopen(input, "r");
    assert(fin != nullptr);
    FILE * fout = fopen(output, "wb");
    assert(fout != nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    char * line = new char [1024];
    size_t line_length;
    int max_vid = 0;
    long edges = 0;
    while (getline(&line, &line_length, fin) != -1) {
        if (line[0] == '#') continue;
        int src, dst;
        assert(sscanf(line, "%d %d", &src, &dst) == 2);
        fwrite(&src, sizeof(int), 1, fout);
        fwrite(&dst, sizeof(int), 1, fout);
        if (src > max_vid) max_vid = src;
        if (dst > max_vid) max_vid = dst;
        edges++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    double secs = std::chrono::duration<double>(end - start).count();

    fclose(fin);
    fclose(fout);
    delete[] line;

    printf("output_file: %s\n", output);
    printf("time: %.6f\n", secs);
    printf("edges: %ld\n", edges);
    printf("|V|=%d\n", max_vid + 1);
    return 0;
}
