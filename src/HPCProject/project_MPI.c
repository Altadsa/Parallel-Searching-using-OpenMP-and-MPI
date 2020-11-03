#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <dirent.h>

#define MAX_TEXTS 20
#define MAX_PATTERNS 20 // based on assumptions from assignment brief

#define MAX_TESTS 1024

#define BYTES_PER_LINE 20 // 4 bytes per character * 5 characters
#define BUFFER_SIZE 2000

char *textData[MAX_TEXTS];
int textLengths[MAX_TEXTS];
int textCount;

char *patternData[MAX_PATTERNS];
int patternLengths[MAX_PATTERNS];
int patternCount;

char controlData[MAX_TESTS][3];

int maxTexts;
int maxPatterns;

char* directory;

void outOfMemory()
{
    fprintf (stderr, "Out of memory\n");
    exit (0);
}

void readFromFile (FILE *f, char **data, int *length)
{
    int ch;
    int allocatedLength;
    char *result;
    int resultLength = 0;

    allocatedLength = 0;
    result = NULL;



    ch = fgetc (f);
    while (ch >= 0)
    {
        resultLength++;
        if (resultLength > allocatedLength)
        {
            allocatedLength += 10000;
            result = (char *) realloc (result, sizeof(char)*allocatedLength);
            if (result == NULL)
                outOfMemory();
        }
        result[resultLength-1] = ch;
        ch = fgetc(f);
    }
    *data = result;
    *length = resultLength;
}

int readFiles(const int maxFiles, char* filename, char *data[], int lengths[], int *count)
{
    int idx = 0;
    FILE *f;
    char fileName[1000];
    for (idx; idx < maxFiles; idx++)
    {
#ifdef DOS
        sprintf (fileName, "%s\\%s%i.txt", directory, filename, idx);
#else
        sprintf (fileName, "%s/%s%i.txt", directory, filename, idx);
#endif

        f = fopen(fileName, "r");
        if (f == NULL)
            return 0;

        readFromFile(f, &data[*count], &lengths[*count]);
        printf("read %s %i\n", filename, idx);
        fclose(f);
        *count += 1;
    }
}

int readControl()
{
    FILE *f;
    char fileName[1000];

#ifdef DOS
    sprintf (fileName, "%s\\control.txt", directory);
#else
    sprintf (fileName, "%s/control.txt", directory);
#endif

    f = fopen(fileName, "r");
    if (f == NULL)
        return 0;

    int testCount = 0;
    int readResult;

    while ((readResult = fscanf(f, "%i %i %i", &controlData[testCount][0], &controlData[testCount][1], &controlData[testCount][2])) != EOF)
    {
        if (readResult == 3)
        {
            printf("Read control entry %i\n", testCount);
            printf("%i %i %i\n\n", controlData[testCount][0], controlData[testCount][1], controlData[testCount][2]);
            testCount++;
        }
        else
        {
            printf("End of Control File reached.\n\n");
        }
    }
    fclose(f);

    return testCount;
}

void writeBufferToOutput(char buffer[])
{
    FILE *f;
    char fileName[1000];

    sprintf(fileName, "result_OMP.txt");

    f = fopen(fileName, "a+");
    if (f == NULL)
    {
        fprintf(stderr, "writeBufferToOutput: could not open file %s", fileName);
        return;
    }
    fprintf(f, buffer);
    fclose(f);

    // clear buffer
    buffer[0] = '\0';
}

void writeToBuffer(char buffer[], int textNumber, int patternNumber, int patternLocation)
{
    // write full buffer to output and clear
    if (strlen(buffer) > BUFFER_SIZE - BYTES_PER_LINE)
    {
        writeBufferToOutput(buffer);
    }
    // append new result to buffer
    sprintf(buffer + strlen(buffer), "%i %i %i\n", textNumber, patternNumber, patternLocation);
}