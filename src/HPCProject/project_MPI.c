#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <mpi.h>

#define MAX_TEXTS 20
#define MAX_PATTERNS 20 // based on assumptions from assignment brief

#define MAX_TESTS 1024

#define BYTES_PER_LINE 20 // 4 bytes per character * 5 characters
#define BUFFER_SIZE 2000

#define MASTER 0
#define SEARCH_LENGTH 1024


//char controlData[MAX_TESTS][3];

int maxTexts;
int maxPatterns;

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

int readTexts(char* directory, char** textData, int* textLengths)
{
    int textCount;

    for (textCount = 0; textCount < MAX_TEXTS; textCount++)
    {
        FILE* f;
        char fileName[1000];
#ifdef DOS
        sprintf(fileName, "%s\\text%d.txt", directory, textCount);
#else
        sprintf(fileName, "%s/text%d.txt", directory, textCount);
#endif
        f = fopen(fileName, "r");
        if (f == NULL)
            return textCount;
        readFromFile(f, &textData[textCount], &textLengths[textCount]);
        fclose(f);
    }

    return MAX_TEXTS;
}

int readPatterns(char* directory, char** patternData, int* patternLengths)
{
    int patternCount;
    for (patternCount = 0; patternCount < MAX_PATTERNS; patternCount++)
    {
        FILE* f;
        char fileName[1000];
#ifdef DOS
        sprintf(fileName, "%s\\pattern%i.txt", directory, patternCount);
#else
        sprintf(fileName, "%s/pattern%i.txt", directory, patternCount);
#endif
        f = fopen(fileName, "r");
        if (f == NULL)
            return patternCount;
        readFromFile(f, &patternData[patternCount], &patternLengths[patternCount]);
        fclose(f);
    }

    return MAX_PATTERNS;
}

int readControl(char* directory, char controlData[][3])
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

int findOccurrence(char* textData, char* patternData, int textLength, int patternLength)
{
    int i, j, k;
    int found = 0;

    i = 0;
    j = 0;
    k = 0;

    int lastI = textLength - patternLength;

    while (i <= lastI && j < patternLength)
    {
        if (textData[k] == patternData[j])
        {
            k++;
            j++;
        }
        else
        {
            i++;
            k = i;
            j = 0;
        }
    }

    if (j == patternLength)
    {
        return 1;
    }
    else
        return 0;
}

int findAllOccurrences(char* textData, char* patternData, int displacement, int textLength, int patternLength, int** results)
{
    int i, j, k;

    //int occurrences[64];
    int* occurrences = (int*)malloc(sizeof(int));

    i = 0;
    j = 0;
    k = 0;

    int lastI = textLength - patternLength;
    int found = 0;

    while (i <= lastI)
    {
        if (textData[k] == patternData[j])
        {
            k++;
            j++;

            if (j == patternLength)
            {
                //results = (int*)realloc(results, ++found * sizeof(int));
                
                found++;
                if (found > 1)
                {
                    occurrences = (int*)realloc(occurrences, found * sizeof(int));
                    occurrences[found - 1] = (i + displacement);
                }
                else
                    occurrences[found-1] = (i + displacement);
            }

        }
        else
        {
            i++;
            k = i;
            j = 0;
        }
    }

    if (found > 0)
    {
        //*results = (int*)realloc(*results, found * sizeof(int));
        *results = occurrences;
        //printf("\n");
        //for (i = 0; i < found; i++)
        //{
        //    //*results[i] = occurrences[i];
        //    printf("%i ", occurrences[i]);
        //}
        //printf("\n");
    }


    return found;

}

int processData(int procId, int searchMode, char* textData, char* patternData, int displacement, int textLength, int patternLength, int** results)
{

    if (searchMode == 0)
    {
        int result = findOccurrence(textData, patternData, textLength, patternLength);
        if (result)
        {
            //printf("Proc %i found pattern at position %i\n", procId, result);
            *results = (int*)malloc(1 * sizeof(int));
            *results[0] = result;
            return 1;
        }
        return 0;
        
    }
    else
    {
        //results = (int*)malloc(1*sizeof(int));
        int* dResults = (int*)malloc(sizeof(int));
        int found = findAllOccurrences(textData, patternData, displacement, textLength, patternLength, &dResults);
        *results = dResults;
        //int i;
        //printf("Found = %i\n", found);
        //for (i = 0; i < found; i++)
        //{
        //    printf("Proc %i found pattern at position %i\n", procId, results[i]);
        //}
        return found;
    }
}

/// <summary>
/// Distributes text length among processes.
/// </summary>
/// <param name="procWork">Array to contain the workload (length of allocated text) of each process.</param>
/// <param name="nProc">Number of processes in the program.</param>
/// <param name="textLength">The length of the full text.</param>
void divideWorkload(int* procWork, int nProc, int textLength, int patternLength)
{
    // calculate the base number of elements for each process
    int nElements = textLength / nProc;
    //printf("\nnElements = %i\n", nElements); // confirm base elements 

    // set base number of elements for each process i
    int i;
    for (i = 0; i < nProc; i++)
    {
        (*(procWork + i)) = nElements;
    }

    int remainder = textLength % nProc;
    //printf("\nRemainder = %i\n", remainder); // confirm remainder

    // if there are no remainders, we can continue with the program
    if (remainder > 0)
    {
        // assign remainders to slave processes
        // we work backwards through processes such that only slave processes
        // take on any extra workload
        for (i = (nProc - 1); i > ((nProc - 1) - remainder); i--)
        {
            (*(procWork + i)) += 1;
        }
    }



    // if the pattern length is greater than 1, we assign extra work just 
    // to detect any patterns occurring across 2 processes
    if (patternLength > 1)
    {
        // assign overflow to first processes
        for (i = 0; i < (nProc - 1); i++)
        {
            (*(procWork + i)) += patternLength;
        }
    }



}

/// <summary>
/// Sets the displacement in the full text for each process.
/// </summary>
/// <param name="displs">Array to contain the displacement of each process.</param>
/// <param name="procWork">Array containing the workload of each process.</param>
/// <param name="nProc">Number of processes in the program.</param>
void setDisplacement(int* displs, int* procWork, int nProc, int patternLength)
{
    int i;
    // displacement at i dependent on i-1. We can set displs[0] to 0 since we know it starts there
    displs[0] = 0;
    for (i = 1; i < nProc; i++)
    {
        if (patternLength == 1)
            displs[i] = displs[i - 1] + (procWork[i - 1]);
        else
            displs[i] = displs[i - 1] + (procWork[i - 1] - patternLength);
        //printf("Process %i displacement = %i\n", i, displs[i]);
    }
}

/// <summary>
/// Prints out the number of elements to be received by each process. 
/// For Debugging purposes only.
/// </summary>
/// <param name="procWorkload">Pointer to array containing the workloads for each elements.</param>
/// <param name="nProc">The number of processes used in the program.</param>
void debugPrintWorkload(int* procWorkload, int nProc)
{
    int i;
    for (i = 0; i < nProc; i++)
    {
        printf("\nProcess %i receiving %i elements\n", i, (*(procWorkload + i)));
    }
}

void debugPrintDisplacement(int* displacement, int nProc)
{
    int i;
    for (i = 0; i < nProc; i++)
    {
        printf("\nProcess %i start at text index %i\n", i, (*(displacement + i)));
    }
}

void processMaster(int nProc, char* directory)
{

#pragma region Declarations
    char buffer[BUFFER_SIZE];

    char* textData[MAX_TEXTS];
    int textLengths[MAX_TEXTS];
    int textCount = readTexts(directory, textData, textLengths);

    char* patternData[MAX_PATTERNS];
    int patternLengths[MAX_PATTERNS];
    int patternCount = readPatterns(directory, patternData, patternLengths);

    //printf("Text and pattern count: %i and %i", textCount, patternCount);

    char controlData[MAX_TESTS][3];
    int numberOfTests = readControl(directory, controlData);
    //numberOfTests = 1;
    int testNumber;
#pragma endregion

    for (testNumber = 0; testNumber < numberOfTests; testNumber++)
    {
        //printf("\nTest: %i", testNumber);

        int searchMode = controlData[testNumber][0];
        int textIndex = controlData[testNumber][1];
        int patternIndex = controlData[testNumber][2];

        // check if text is shorter than pattern
        if (textLengths[textIndex] < patternLengths[patternIndex])
        {
            printf("Test %i: Text shorter than Pattern.\n", testNumber);

            writeToBuffer(buffer, textIndex, patternIndex, -1);
            continue;
        }

        // store number of elements each process receives
        int* displs = (int*)malloc(nProc * sizeof(int));
        int* procWorkload = (int*)malloc(nProc * sizeof(int));

        //printf("\nDividing text of length %i among %i processes.\n", textLength, nProc);

#pragma region Send Data
        // send pattern length first so slaves know
        // how large the received pattern is
        MPI_Bcast(&patternLengths[patternIndex],
            1, MPI_INT, MASTER,
            MPI_COMM_WORLD);

        // send entire pattern to slaves
        MPI_Bcast(patternData[patternIndex],
            patternLengths[patternIndex], MPI_CHAR, MASTER,
            MPI_COMM_WORLD);

        MPI_Bcast(&searchMode,
            1, MPI_INT, MASTER,
            MPI_COMM_WORLD);

        // divide the workload among the processes
        divideWorkload(procWorkload, nProc, textLengths[textIndex], patternLengths[patternIndex]);
        //debugPrintWorkload(procWorkload, nProc);

        // get the displacement within the text for each process
        setDisplacement(displs, procWorkload, nProc, patternLengths[patternIndex]);

        //debugPrintDisplacement(displs, nProc);

        int nElements;
        // scatter workload to processes so they know how many elements are being received
        MPI_Scatter(procWorkload, 1,
            MPI_INT, &nElements, 1,
            MPI_INT, MASTER,
            MPI_COMM_WORLD);

        int masterDispls;
        // scatter the displacement to get the actual text index and not the relative index
        MPI_Scatter(displs, 1,
            MPI_INT, &masterDispls, 1,
            MPI_INT, MASTER,
            MPI_COMM_WORLD);

        char* masterWork = (char*)malloc(nElements * sizeof(char));
        // scatter the text data to each process
        // we use scatterv under the assumption that the workload MIGHT not be evenly distributed
        // even though the text data can be evenly distributed
        MPI_Scatterv(textData[textIndex],
            procWorkload, displs,
            MPI_CHAR, masterWork, 1,
            MPI_CHAR, MASTER,
            MPI_COMM_WORLD);
#pragma endregion

        int* results;
        int found = processData(MASTER, searchMode, textData[textIndex], patternData[patternIndex], masterDispls, nElements, patternLengths[patternIndex], &results);
        int total = found;

        int j = found;
        int n;
        for (n = 1; n < nProc; n++)
        {
            int procFound;

            MPI_Recv(&procFound, 1, MPI_INT,
                n, 0,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);

            if (procFound > 0)
            {
                int* result = (int*)malloc(procFound * sizeof(int));
                MPI_Recv(result, procFound,
                    MPI_INT, n, 0,
                    MPI_COMM_WORLD,
                    MPI_STATUS_IGNORE);


                total += procFound;

                results = (int*)realloc(results, total * sizeof(int));

                int i;
                for (i = 0; i < procFound; i++, j++)
                {
                    results[j] = result[i];
                }

                free(result);
            }


        }


        

        if (total > 0)
        {
            if (!searchMode)
            {
                printf("Test %i, search mode %i, text %i, pattern %i, found patterns at %i\n", testNumber, searchMode, textIndex, patternIndex, -2);
            }
            else
            {
                printf("Test %i, search mode %i, text %i, pattern %i, found %i patterns at ", testNumber, searchMode, textIndex, patternIndex, total);
                int i;
                for (i = 0; i < total; i++)
                {
                    printf("%i ", results[i]);
                }
                printf("\n");
            }

        }
        else
        {
            int r = -1;
            printf("Test %i, search mode %i, text %i, pattern %i, found patterns at %i\n", testNumber, searchMode, textIndex, patternIndex, r);
        }



        //free(results);

        free(procWorkload);
        free(displs);
        free(masterWork);

        int finished = testNumber == (numberOfTests - 1);
        MPI_Bcast(&finished, 1, MPI_INT,
            MASTER, MPI_COMM_WORLD);

        //free(results);

    }



    int i;

    //printf("\nProgram finished.\n");

}

void processSlave(int procId)
{
    // Set up the receive for the flag from MASTER declaring the end
    //int done = 0;
    int doneFlag = 0;
    MPI_Request request;
    MPI_Status status;
    //MPI_Irecv(&done, 1, MPI_INT, MASTER, 1, MPI_COMM_WORLD, &request); // X

    int finished = 0;

    while (!finished)
    {

#pragma region Declarations and Data Recept
        // delcare data variables
        char* textData;
        int textLength;

        char* patternData;
        int patternLength;



        // receive pattern length before pattern data
        MPI_Bcast(&patternLength,
            1, MPI_INT, MASTER,
            MPI_COMM_WORLD);

        // allocate memory for pattern data
        patternData = (char*)malloc(patternLength * sizeof(char));

        // receive pattern data after getting the number of received elements
        MPI_Bcast(patternData,
            patternLength, MPI_CHAR, MASTER,
            MPI_COMM_WORLD);

        int searchMode;
        MPI_Bcast(&searchMode,
            1, MPI_INT, MASTER,
            MPI_COMM_WORLD);

        // receive the text length before the data
        MPI_Scatter(NULL, 1,
            MPI_INT, &textLength, 1,
            MPI_INT, MASTER,
            MPI_COMM_WORLD);

        // receive displacement in text data to calculate actual result
        int startIndex;
        MPI_Scatter(NULL, 1,
            MPI_INT, &startIndex, 1,
            MPI_INT, MASTER,
            MPI_COMM_WORLD);

        // allocate text data based on number of received elements
        textData = (char*)malloc(textLength * sizeof(char));
        // receive text data from master
        MPI_Scatterv(NULL,
            NULL, NULL,
            MPI_CHAR, textData, textLength,
            MPI_CHAR, MASTER,
            MPI_COMM_WORLD);
#pragma endregion

        int* results = NULL;
        int found = processData(procId, searchMode, textData, patternData, startIndex, textLength, patternLength, &results);

        MPI_Send(&found, 1, MPI_INT,
            MASTER, 0,
            MPI_COMM_WORLD);

        if (found > 0)
        {
            MPI_Send(results, found, MPI_INT, MASTER, 0,
                MPI_COMM_WORLD);
            //free(results);
        }

        free(textData);
        free(patternData);



        // receive finish flag before trying to receive pattern data
        MPI_Bcast(&finished,
            1, MPI_INT, 0,
            MPI_COMM_WORLD);

    }

}

void main(int argc, char** argv)
{
    int nProc, procId;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nProc);
    MPI_Comm_rank(MPI_COMM_WORLD, &procId);

    // exit if no input directory specified, or fewer than 4 cores
    if (argc < 2 || nProc < 4)
    {
        printf("Not enough arguments: No inputs directory provided.");
        exit(0);
    }


    if (procId == MASTER)
    {
        processMaster(nProc, argv[1]);
    }
    else
    {
        processSlave(procId);
    }

    MPI_Finalize();


}