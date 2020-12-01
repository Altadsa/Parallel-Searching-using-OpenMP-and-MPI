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
    FILE* f;
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

void findFirstOccurrence(int textNumber, int patternNumber, char buffer[])
{
    char *text = textData[textNumber];
    int textLength = textLengths[textNumber];

    char *pattern = patternData[patternNumber];
    int patternLength = patternLengths[patternNumber];

    int i, j, k, lastI, stopSearch;
    i=0;
    j=0;
    k=0;
    lastI = textLength-patternLength;
    stopSearch=0;

    int patternLoc = -1;


    #pragma omp parallel for default(none) shared(patternLoc, buffer) \
    private(j, k) firstprivate(stopSearch, text, textLength, pattern, patternLength, lastI, textNumber, patternNumber) \
    num_threads(4) schedule(static,4)
    for (i = 0; i <= lastI; i++)
    {
        if (patternLoc >= 0)
        {
            continue;
        }
        else
        {
            k = i;
            j = 0;
            // since the pattern can be found in the middle of the loop
            // we check if pattern is found every loop to stop making comparisons as soon as possible.
            while (j < patternLength && stopSearch == 0 && patternLoc == -1)
            {
                if (text[k] == pattern[j])
                {
                    k++;
                    j++;
                }
                else
                {
                    stopSearch = 1;
                }
            }
            // condition can only be true once assuming only one occurrence of pattern in text.
            if (j == patternLength && patternLoc == -1)
            {
                #pragma omp critical(set)
                {
                    patternLoc = i;
                    writeToBuffer(buffer, textNumber, patternNumber, i);
                }
            }
            else
            {
                stopSearch = 0;
            }
        }
    }

    if (patternLoc == -1)
    {
        writeToBuffer(buffer, textNumber, patternNumber, -1);
    }

}

void findAllOccurrences(int textNumber, int patternNumber, char buffer[])
{
    char *text = textData[textNumber];
    int textLength = textLengths[textNumber];

    char *pattern = patternData[patternNumber];
    int patternLength = patternLengths[patternNumber];

    int i, j, k, lastI, stopSearch;
    i=0;
    j=0;
    k=0;
    lastI = textLength-patternLength;
    stopSearch=0;

    int patternLoc = -1;

    #pragma omp parallel for default(none) shared(buffer, patternLoc) \
    private(j, k) firstprivate(stopSearch, text, textLength, pattern, patternLength, lastI, textNumber, patternNumber) \
    num_threads(4) schedule(static,4)
    for (i = 0; i <= lastI; i++)
    {
        k = i;
        j = 0;
        // since the pattern can be found in the middle of the loop
        // we check if pattern is found every loop to stop making comparisons as soon as possible.
        while (j < patternLength && stopSearch == 0)
        {
            if (text[k] == pattern[j])
            {
                k++;
                j++;
            }
            else
            {
                stopSearch = 1;
            }
        }
        // condition can only be true once assuming only one occurrence of pattern in text.
        if (j == patternLength)
        {
            writeToBuffer(buffer, textNumber, patternNumber, i);
            if (patternLoc == -1)
            {
                #pragma omp critical(set)
                {
                    patternLoc = 1;
                }
            }
        }
        else
        {
            stopSearch = 0;
        }
    }

    if (patternLoc == -1)
    {
        writeToBuffer(buffer, textNumber, patternNumber, -1);
    }

}



void runTest(int searchType, int textNumber, int patternNumber, char buffer[])
{
    // if pattern is larger than text, write result as pattern not found
    if (textLengths[textNumber] < patternLengths[patternNumber])
    {
        writeToBuffer(buffer, textNumber, patternNumber, -1);
        return;
    }

    if (searchType == 0)
    {
        printf("Searching for pattern occurrence\n");
        findFirstOccurrence(textNumber, patternNumber, buffer);
    }
    else
    {
        printf("Searching for all pattern occurrences\n");
        findAllOccurrences(textNumber, patternNumber, buffer);
    }



}

int main(int argc, char **argv)
{
    if (argc < 1)
    {
        printf("Not enough arguments: No inputs directory provided.");
        exit(0);
    }

    directory = argv[1];

    readFiles(MAX_TEXTS, "text", textData, textLengths, &textCount);
    readFiles(MAX_PATTERNS, "pattern", patternData, patternLengths, &patternCount);

    printf("Text Count = %i, Pattern Count = %i\n", textCount, patternCount);

    int testCount = readControl();

    // initialise buffer
    char buffer[BUFFER_SIZE];
    sprintf(buffer, "");

    int idx = 0;
    for (idx; idx < testCount; idx++)
    {
        runTest(controlData[idx][0],controlData[idx][1],controlData[idx][2], buffer);
    }

    writeBufferToOutput(buffer);

}