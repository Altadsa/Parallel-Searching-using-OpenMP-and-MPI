/////////////////////////////////////////////////////////////////////
//
// Author: Adam Coyle (40178464)
// Date: December 2020
// Program: project_OMP
// Description: Parallelisation of a naive pattern searching algorithm
// using OpenMP. The program reads a control file and set of texts and
// patterns from a user-specified directory. The program searches a
// text for a pattern, both of which are specified by an entry in the 
// control file, and the result of the search is output to a file,
// result_OMP.txt.
//
/////////////////////////////////////////////////////////////////////

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

/// <summary>
/// Reads data from files named filename, writing data into the data array, and 
/// filelengths into the lengths array
/// </summary>
/// <param name="maxFiles">The maximum number of files to read.</param>
/// <param name="directory">The Directory to read the file from.</param>
/// <param name="filename">The Filename to identify which files to read.</param>
/// <param name="data">The Character Array to store the read data.</param>
/// <param name="lengths">The Integer Array to store the lengths of the files.</param>
/// <returns>The number of files read.</returns>
int readFiles(const int maxFiles, char* filename, char *data[], int lengths[])
{
    int count = 0;
    FILE *f;
    char fileName[1000];
    for (count; count < maxFiles; count++)
    {
#ifdef DOS
        sprintf (fileName, "%s\\%s%i.txt", directory, filename, count);
#else
        sprintf (fileName, "%s/%s%i.txt", directory, filename, count);
#endif

        f = fopen(fileName, "r");
        if (f == NULL)
            return 0;

        readFromFile(f, &data[count], &lengths[count]);
        printf("read %s %i\n", filename, count);
        fclose(f);

    }
    return count;
}

/// <summary>
/// Read the test cases from the control file in the input directory, and load them into an array.
/// </summary>
/// <returns>The number of tests in the control file.</returns>
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

/// <summary>
/// Writes the contents of a character buffer to file.
/// </summary>
/// <param name="buffer">The character buffer to be written to file.</param>
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

/// <summary>
/// Writes a test result to the buffer.
/// </summary>
/// <param name="buffer">Character buffer to be written to.</param>
/// <param name="textNumber">The Text number specified by the test case.</param>
/// <param name="patternNumber">The Pattern number specified by the test case.</param>
/// <param name="patternLocation">The location in the text the pattern was found.</param>
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

/// <summary>
/// Parallel searching algorithm which searches for any instance of a pattern
/// and completes after successfully finding the pattern.
/// </summary>
/// <param name="textNumber">The Text number specified by the test case.</param>
/// <param name="patternNumber">The Pattern number specified by the test case.</param>
/// <param name="buffer">The Buffer to write the result to.</param>
void findOccurrence(int textNumber, int patternNumber, char buffer[])
{

    // load data
    char *text = textData[textNumber];
    int textLength = textLengths[textNumber];

    char *pattern = patternData[patternNumber];
    int patternLength = patternLengths[patternNumber];

    int i, j, k, lastI, stopSearch;
    i=0;
    j=0;
    k=0;

    // last index in text to search from
    lastI = textLength-patternLength;

    // -1 denotes pattern not found
    int patternLoc = -1;

    // sharing pattern location since all threads depend on it to stop searching
    #pragma omp parallel for default(none) shared(patternLoc, buffer) \
    private(j, k) firstprivate(text, textLength, pattern, patternLength, lastI, textNumber, patternNumber) \
    num_threads(4) schedule(static,4)
    for (i = 0; i <= lastI; i++)
    {
        // pattern is already found, stop searching
        if (patternLoc >= 0)
        {
            continue;
        }
        else
        {
            k = i;
            j = 0;

            // stop searching if we find the pattern, or if it has been found by another thread
            while (j < patternLength && patternLoc == -1)
            {
                if (text[k] == pattern[j])
                {
                    k++;
                    j++;
                }
                else // no longer matching, break out
                {
                    break;
                }
            }

            if (j == patternLength)
            {
                // prevents multiple threads writing at the same time
                // since the condition within will be set at first pattern instance
                // so other threads waiting to check will not write
                #pragma omp critical(set)
                {
                    if (patternLoc == -1)
                    {
                        patternLoc = i;
                        // write -2 to denote pattern is found
                        //printf("Pattern found at %i\n", i);
                        writeToBuffer(buffer, textNumber, patternNumber, -2);
                    }
                }
            }

        }
    }

    // report pattern as not found
    if (patternLoc == -1)
    {
        writeToBuffer(buffer, textNumber, patternNumber, -1);
    }

}

/// <summary>
/// Parallel searching algorithm which searches for all instances of a pattern
/// and completes only after searching the entire text.
/// </summary>
/// <param name="textNumber">The Text number specified by the test case.</param>
/// <param name="patternNumber">The Pattern number specified by the test case.</param>
/// <param name="buffer">The Buffer to write the result to.</param>
void findAllOccurrences(int textNumber, int patternNumber, char buffer[])
{
    // load text and pattern data
    char *text = textData[textNumber];
    int textLength = textLengths[textNumber];

    char *pattern = patternData[patternNumber];
    int patternLength = patternLengths[patternNumber];

    int i, j, k, lastI;
    i=0;
    j=0;
    k=0;

    // last index in text to search from
    lastI = textLength-patternLength;

    // -1 denotes pattern not found
    int patternLoc = -1;

    // sharing the buffer was simpler than using private buffers and writing to file once buffer limits were reached
    // although patternLoc doesn't need to be shared here, I observed that it was quicker than using reduction
    // dynamic scheduling was chosen as it yielded lower elapsed cpu runtimes on average
    // also since I won't know in advance the large inputs, dynamic is often more useful for imbalanced workloads

    #pragma omp parallel for default(none) shared(buffer, patternLoc) \
    private(j, k) firstprivate(text, textLength, pattern, patternLength, lastI, textNumber, patternNumber) \
    num_threads(4) schedule(dynamic,4)
    for (i = 0; i <= lastI; i++)
    {
        k = i;
        j = 0;

        // since the pattern can be found in the middle of the loop
        // we check if pattern is found every loop to stop making comparisons as soon as possible.
        while (j < patternLength && text[k] == pattern[j])
        {
            k++;
            j++;
        }
        
        if (j == patternLength)
        {
            // allow only one thread at a time to write to buffer
            #pragma omp critical(set)
            {
                //printf("Pattern found at %i\n", i);
                writeToBuffer(buffer, textNumber, patternNumber, i);
                patternLoc = 1;
            }
        }
    }

    // report pattern as unfound
    if (patternLoc == -1)
    {
        writeToBuffer(buffer, textNumber, patternNumber, -1);
    }

}


/// <summary>
/// Runs a searching algorithm on the specified text/pattern combination.
/// </summary>
/// <param name="searchType">Search mode used to determine which searching algorithm to use.</param>
/// <param name="textNumber">The Text number specified by the test case.</param>
/// <param name="patternNumber">The Pattern number specified by the test case.</param>
/// <param name="buffer">The buffer to write the results to.</param>
void runTest(int searchType, int textNumber, int patternNumber, char buffer[])
{
    // if pattern is larger than text, write result as pattern not found
    if (textLengths[textNumber] < patternLengths[patternNumber])
    {
        writeToBuffer(buffer, textNumber, patternNumber, -1);
        return;
    }

    if (searchType == 0) // find any occurrence
    {
        //printf("Searching for pattern occurrence\n");
        findOccurrence(textNumber, patternNumber, buffer);
    }
    else // find all occurrences
    {
        //printf("Searching for all pattern occurrences\n");
        findAllOccurrences(textNumber, patternNumber, buffer);
    }



}

/// <summary>
/// Gets the current time in nanoseconds.
/// </summary
/// <returns>The time in nanoseconds.</returns>
long getNanos()
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}


int main(int argc, char **argv)
{
    // program requires inputs directory to be specified.
    if (argc < 1)
    {
        printf("Not enough arguments: No inputs directory provided.");
        exit(0);
    }
    directory = argv[1];

    // read texts and patterns into arrays.
    textCount = readFiles(MAX_TEXTS, "text", textData, textLengths);
    patternCount = readFiles(MAX_PATTERNS, "pattern", patternData, patternLengths);

    //printf("Text Count = %i, Pattern Count = %i\n", textCount, patternCount);

    // read control file data
    int testCount = readControl();

    // initialise buffer
    char buffer[BUFFER_SIZE];
    sprintf(buffer, "");

    // start time of program
    long elapsedTime = getNanos();

    int idx = 0;
    for (idx; idx < testCount; idx++)
    {
        // start time of test
        long time = getNanos();

        runTest(controlData[idx][0],controlData[idx][1],controlData[idx][2], buffer);

        // elapsed time of test
        time = getNanos() - time;
        printf("\nTest %i elapsed time = %.09f\n\n", idx, (double)time / 1.0e9);
    }

    // elapsed time of program
    elapsedTime = getNanos() - elapsedTime;
    printf("\nProgram elapsed time = %.09f\n\n", (double)elapsedTime / 1.0e9);

    // write any remaining data file
    writeBufferToOutput(buffer);


}