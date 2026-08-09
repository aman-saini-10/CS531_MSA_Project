#include <stdio.h>

#define MAX_LINE_LENGTH 100

int main() {
    FILE *inputFile, *outputFile;
    char line[MAX_LINE_LENGTH];

    // Open input file for reading
    inputFile = fopen("Financial1.spc", "r");
    if (inputFile == NULL) {
        printf("Error opening input file\n");
        return 1;
    }

    // Open output file for writing
    outputFile = fopen("Financial1_output.txt", "w");
    if (outputFile == NULL) {
        printf("Error opening output file\n");
        fclose(inputFile);
        return 1;
    }

    // Read lines from input file and write to output file
    while (fgets(line, MAX_LINE_LENGTH, inputFile) != NULL) {
        int field1;
        long field2, field3;
        char field4[1];
        double field5;
        sscanf(line, "%d,%ld,%ld,%c,%Lf", &field1, &field2, &field3, field4, &field5);
        fprintf(outputFile, "%d,%ld,%ld,%c,%0.6Lf\n", field1, field2, field3, field4, field5);
    }

    // Close files
    fclose(inputFile);
    fclose(outputFile);

    return 0;
}
