/////////////////////////////////////////////////////////////////////
//
// Author: Adam Coyle (40178464)
// Date: December 2020
// Program: project_MPI
// Description: Parallelisation of a naive pattern searching algorithm
// using MPI. The program implements a fine-grain solution using a 
// master slave model. 
// The master:
//      Reads the data
//      Loops over the test cases provided by the control file
//      Computes and sends the workloads to the slaves
//      Searches its own portion of text
//      Receives results from slaves
//      Writes results to file
//      Informs slaves if they should stop
//
// The slaves:
//      Receive data from the master and search for the pattern
//      Send the result back to the master if there is any
//      Wait for master to inform them if all tests are complete and they should stop working
//
/////////////////////////////////////////////////////////////////////

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

// message tag which master sends to processes still searching
#define EXECUTE 66
// message sent from slaves to master to indicate they completed their search 
#define PROCESS_DONE 67

// using global variables greatly reduces the number of parameters needed for functions
int testNumber; // test number
int procId; // process ID
int nProc; // number of processes in program

#pragma region I/O Functions
void outOfMemory()
{
    fprintf(stderr, "Out of memory\n");
    exit(0);
}

void printDebug()
{
    printf("\nStatement Reached!\n");
}

void readFromFile(FILE* f, char** data, int* length)
{
    int ch;
    int allocatedLength;
    char* result;
    int resultLength = 0;

    allocatedLength = 0;
    result = NULL;



    ch = fgetc(f);
    while (ch >= 0)
    {
        resultLength++;
        if (resultLength > allocatedLength)
        {
            allocatedLength += 10000;
            result = (char*)realloc(result, sizeof(char) * allocatedLength);
            if (result == NULL)
                outOfMemory();
        }
        result[resultLength - 1] = ch;
        ch = fgetc(f);
    }
    *data = result;
    *length = resultLength;
}

/// <summary>
/// Reads data from files named filename, writing data into the data array, and 
/// filelengths into the lengths array
/// </summary>
/// <param name="maxFiles"></param>
/// <param name="filename"></param>
/// <param name="data"></param>
/// <param name="lengths"></param>
/// <param name="count"></param>
/// <returns></returns>
int readFiles(const int maxFiles, char* directory, char* filename, char* data[], int lengths[])
{
    int count = 0;
    FILE* f;
    char fileName[1000];
    for (count; count < maxFiles; count++)
    {
#ifdef DOS
        sprintf(fileName, "%s\\%s%i.txt", directory, filename, count);
#else
        sprintf(fileName, "%s/%s%i.txt", directory, filename, count);
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
int readControl(char* directory, char controlData[][3])
{
    FILE* f;
    char fileName[1000];

#ifdef DOS
    sprintf(fileName, "%s\\control.txt", directory);
#else
    sprintf(fileName, "%s/control.txt", directory);
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
            //printf("Read control entry %i\n", testCount);
            //printf("%i %i %i\n\n", controlData[testCount][0], controlData[testCount][1], controlData[testCount][2]);
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

    sprintf(fileName, "result_MPI.txt");

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
#pragma endregion

#pragma region Helper Functions
/// <summary>
/// Distributes text length among processes.
/// </summary>
/// <param name="procWork">Array to contain the workload (length of allocated text) of each process.</param>
/// <param name="nProc">Number of processes in the program.</param>
/// <param name="textLength">The length of the full text.</param>
void divideWorkload(int* procWork, int textLength, int patternLength)
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
    // to detect any patterns occurring across processes
    if (patternLength > 1)
    {
        // assign overflow to first processes
        for (i = 0; i < (nProc - 1); i++)
        {
            (*(procWork + i)) += patternLength;
            if ((*(procWork + i)) > textLength)
            {
                (*(procWork + i)) -= ((*(procWork + i)) - textLength);
            }
        }
    }
}

/// <summary>
/// Sets the displacement in the full text for each process.
/// </summary>
/// <param name="displs">Array to contain the displacement of each process.</param>
/// <param name="procWork">Array containing the workload of each process.</param>
/// <param name="nProc">Number of processes in the program.</param>
void setDisplacement(int* displs, int* procWork, int patternLength)
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
void debugPrintWorkload(int* procWorkload)
{
    int i;
    for (i = 0; i < nProc; i++)
    {
        printf("\nProcess %i receiving %i elements\n", i, (*(procWorkload + i)));
    }
}

/// <summary>
/// Prints out text displacement of each process
/// For debugging purposes only.
/// </summary>
/// <param name="displacement">Pointer to array containing the displacements for each process.</param>
/// <param name="nProc">The number of processes used in the program.</param>
void debugPrintDisplacement(int* displacement)
{
    int i;
    for (i = 0; i < nProc; i++)
    {
        printf("\nProcess %i start at text index %i\n", i, (*(displacement + i)));
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

#pragma endregion

/// <summary>
/// Searches for any occurrences of a pattern, completing once an occurrence has been found.
/// The master must probe for messages from the slaves indicating that they have completed their search.
/// If the result sent by the slaves is 1 (indicating that pattern has been found) then the master
/// messages all processes still searching that they should stop. Otherwise, the process is marked as complete
/// so the master does not try to send future messages to that process.
/// If the master finds the pattern, then it informs any remaining processes to stop their search.
/// </summary>
/// <param name="textData">The portion of Text to be searched.</param>
/// <param name="patternData">The Pattern to search for.</param>
/// <param name="textLength">The Length of the portion of Text.</param>
/// <param name="patternLength">The Length of the Pattern.</param>
/// <param name="displacement">The Displacement of the portion of Text.</param>
/// <returns>The number of occurrences of the Pattern within the portion of Text.</returns>
int masterFindOccurrence(char* textData, char* patternData, int textLength, int patternLength, int displacement)
{
    MPI_Status status;
    int masterTracker[4] = {0, 0, 0, 0}; // tracks search progress of other processes

    int i, j, k;
    int found = 0;

    int lastI = textLength - patternLength;

    i = 0;
    j = 0;
    k = 0;

    int message = 0; // indicates if a message is waiting to be received from slave

    while (i <= lastI && j < patternLength)
    {
        k++;
        j++;

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

        // check for message from slaves
        MPI_Iprobe(MPI_ANY_SOURCE, PROCESS_DONE, MPI_COMM_WORLD, &message, &status);

        if (message)
        {
            int slaveResult = 0;

            // revceive search result from slave
            MPI_Recv(&slaveResult, 1, MPI_INT, MPI_ANY_SOURCE, PROCESS_DONE, MPI_COMM_WORLD, &status);
            if (masterTracker[status.MPI_SOURCE]) //ensures slave can only send its result once
                continue;
            //printf("Received from %i\n", status.MPI_SOURCE);
            masterTracker[status.MPI_SOURCE] = 1; // indicates slave process has completed search

            // if slave was successful, inform all other processes that are still searching to stop
            if (slaveResult)
            {
                //printf("Master on test received value from process %i\n", status.MPI_SOURCE);
                int n;
                for (n = 1; n < 4; n++)
                {
                    if (!masterTracker[n])
                    {
                        MPI_Send(&found, 1, MPI_INT, MASTER, EXECUTE, MPI_COMM_WORLD);
                    }
                }

                return found;
            }
        }

    }

    // if master finds the pattern, inform all other processes that are still searching to stop
    if (j == patternLength)
    {
        printf("Found at %i\n", (i + displacement));
        found = 1;
        int n;
        for (n = 1; n < 4; n++)
        {
            if (!masterTracker[n])
            {
                MPI_Send(&found, 1, MPI_INT, MASTER, EXECUTE, MPI_COMM_WORLD);
            }
        }
    }

    // return the result
    return found;

}

/// <summary>
/// Searches for any occurrences of a pattern, completing once an occurrence has been found.
/// The slaves must probe for a message from the master which indicates that the pattern has already been
/// found by another process. Otherwise, if a slave finds the pattern, or completes its search before a pattern
/// has been found, it must inform the master that it has completed its search.
/// </summary>
/// <param name="textData">The portion of Text to be searched.</param>
/// <param name="patternData">The Pattern to search for.</param>
/// <param name="textLength">The Length of the portion of Text.</param>
/// <param name="patternLength">The Length of the Pattern.</param>
/// <param name="displacement">The Displacement of the portion of Text.</param>
/// <returns>The number of occurrences of the Pattern within the portion of Text.</returns>
int slaveFindOccurrence(char* textData, char* patternData, int textLength, int patternLength, int displacement)
{
    MPI_Status status;

    int i, j, k;
    int found = 0;

    i = 0;
    j = 0;
    k = 0;

    int lastI = textLength - patternLength;

    int message; // indicates whether or not a message is waiting to be received from master
    

    while (i <= lastI && j < patternLength && !found)
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

        // check for message from master
        MPI_Iprobe(MASTER, EXECUTE, MPI_COMM_WORLD, &message, MPI_STATUS_IGNORE);

        if (message) //receive message and stop searching
        {
            int commMessage;
            MPI_Recv(&commMessage, 1, MPI_INT, MASTER, EXECUTE, MPI_COMM_WORLD, &status);

            return found;
        }

    }

    // if pattern is found by slave
    if (j == patternLength)
    {
        printf("Slave Found at %i\n", (i+displacement));
        found = 1;
    }

    // send result to master
    MPI_Send(&found, 1, MPI_INT, MASTER, PROCESS_DONE, MPI_COMM_WORLD);

    // return result
    return found;
}

/// <summary>
/// Searches for all occurrences of a pattern, only completing once the 
/// entire portion of text has been searched.
/// </summary>
/// <param name="textData">The portion of Text to be searched.</param>
/// <param name="patternData">The Pattern to search for.</param>
/// <param name="displacement">The Displacement of the portion of Text.</param>
/// <param name="textLength">The Length of the portion of Text.</param>
/// <param name="patternLength">The Length of the Pattern.</param>
/// <param name="results">Array of integer results to store location of Pattern occurrences.</param>
/// <returns>The number of occurrences of the Pattern within the portion of Text.</returns>
int findAllOccurrences(char* textData, char* patternData, int displacement, int textLength, int patternLength, int** results)
{
    int i, j, k;

    // we allocate an array within this function and set the value of results to it
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
            ++k;
            ++j;

            // if we find the pattern, resize the occurrences array and add the result
            if (j == patternLength)
            {                
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

    // set results only if we find the pattern
    if (found > 0)
    {
        *results = occurrences;
    }

    // return result
    return found;

}

/// <summary>
/// Searches a portion of Text for a Pattern, storing the results.
/// </summary>
/// <param name="searchMode">The Search Mode used to determine which searching algorithm to use.
/// 0 - Find any occurrence
/// 1 - Find all occurrences</param>
/// <param name="textData">The portion of Text to be searched.</param>
/// <param name="patternData">The Pattern to search for.</param>
/// <param name="displacement">The displacement of the portion of text.</param>
/// <param name="textLength">The length of the portion of text to search.</param>
/// <param name="patternLength">The length of the pattern.</param>
/// <param name="results">Array of integer results to store locations of any found patterns.</param>
/// <returns>The number of pattern occurrences found in the text.</returns>
int processData(int searchMode, char* textData, char* patternData, int displacement, int textLength, int patternLength, int** results)
{
    if (searchMode == 0) // find any occurrence
    {
        int result;
        if (procId == MASTER) // master has unique set of functions to complete whilst searching
            result = masterFindOccurrence(textData, patternData, textLength, patternLength, displacement);
        else // slaves must send results of search to master so they have a unique search
            result = slaveFindOccurrence(textData, patternData, textLength, patternLength, displacement);

        *results = (int*)malloc(1 * sizeof(int));
        if (result)
        {
            *results[0] = -2; // set -2 for finding any occurrence
            return 1; // only interested if pattern occurs, not in the number of occurrences
        }
        return 0;
        
    }
    else
    {
        // pass search results into the function and assign the results to it
        int* searchResults = (int*)malloc(sizeof(int));
        int found = findAllOccurrences(textData, patternData, displacement, textLength, patternLength, &searchResults);
        *results = searchResults;
        return found;
    }
}

/// <summary>
/// Master instructions: Master reads in text, pattern and control data. For each test, the master
/// calculates workload distribution and displacements, then sends the relevant search data to the slaves,
/// carries out its own search before receiving the search results of the slaves and writing them to file.
/// Finally, if the master has completed all tests, it sends a finish flag to the slaves.
/// </summary>
/// <param name="directory">The directory to read the data from.</param>
void processMaster(char* directory)
{

#pragma region Declarations

    // initialize data variables within master process

    char buffer[BUFFER_SIZE];
    buffer[0] = '\0';

    char* textData[MAX_TEXTS];
    int textLengths[MAX_TEXTS];
    int textCount = readFiles(MAX_TEXTS, directory, "text", textData, textLengths);

    char* patternData[MAX_PATTERNS];
    int patternLengths[MAX_PATTERNS];
    int patternCount = readFiles(MAX_PATTERNS, directory, "pattern", patternData, patternLengths);

    char controlData[MAX_TESTS][3];
    int numberOfTests = readControl(directory, controlData);

#pragma endregion

    long programTime = getNanos();
    //int testNumber;
    for (testNumber = 0; testNumber < numberOfTests; testNumber++)
    {
        //printf("\nTest: %i", testNumber);
        long time = getNanos();

        // test variables
        int searchMode = controlData[testNumber][0];
        int textIndex = controlData[testNumber][1];
        int patternIndex = controlData[testNumber][2];

        int testTextLength = textLengths[textIndex];
        int testPatternLength = patternLengths[patternIndex];

        // check if text is shorter than pattern
        if (testTextLength < testPatternLength)
        {
            printf("Test %i: Text shorter than Pattern.\n", testNumber);

            writeToBuffer(buffer, textIndex, patternIndex, -1);
            continue;
        }

        // store number of elements each process receives
        int* displs = (int*)malloc(nProc * sizeof(int));
        int* procWorkload = (int*)malloc(nProc * sizeof(int));

#pragma region Send Data

        MPI_Bcast(&testNumber,
            1, MPI_INT, MASTER,
            MPI_COMM_WORLD);

        // send pattern length first so slaves know
        // how large the received pattern is
        MPI_Bcast(&testPatternLength,
            1, MPI_INT, MASTER,
            MPI_COMM_WORLD);

        // send entire pattern to slaves
        MPI_Bcast(patternData[patternIndex],
            testPatternLength, MPI_CHAR, MASTER,
            MPI_COMM_WORLD);

        MPI_Bcast(&searchMode,
            1, MPI_INT, MASTER,
            MPI_COMM_WORLD);

        // divide the workload among the processes
        divideWorkload(procWorkload, testTextLength, testPatternLength);


        // get the displacement within the text for each process
        setDisplacement(displs, procWorkload, testPatternLength);

        //debugPrintWorkload(procWorkload);
        //debugPrintDisplacement(displs);

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

        // we use send instead of scatterv as we had done previously, since we address patterns across processes
        // by simply adding the length of the pattern to the first workload, which means the total workload of the
        // processes is greater than the size of the text
        int n;
        for (n = 1; n < nProc; n++)
        {
            int dis = (*(displs+n));
            int work = (*(procWorkload + n));
            MPI_Send(&textData[textIndex][dis],
                work, MPI_CHAR,
                n, 1, MPI_COMM_WORLD);
        }

#pragma endregion

        // get results

        // process master workload
        int* results = NULL;
        int found = processData(searchMode, textData[textIndex], patternData[patternIndex], masterDispls, nElements, testPatternLength, &results);
        
        // get results from slave processes
        int total = found;
        int j = found;
        
        for (n = 1; n < nProc; n++)
        {
            // receive number of found instances by the process
            int procFound;
            MPI_Recv(&procFound, 1, MPI_INT,
                n, 0,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);

            // only continue if there are not 0 results
            if (procFound == 0)
            {
                continue;
            }

            // receive data from process
            int* result = (int*)malloc(procFound * sizeof(int));
            MPI_Recv(result, procFound,
                MPI_INT, n, 0,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);

            // add received results to total
            total += procFound;

            results = (int*)realloc(results, total * sizeof(int));

            int i;
            for (i = 0; i < procFound; i++, j++)
            {
                results[j] = result[i];
            }

            free(result);

        }

        

        time = getNanos() - time;
        printf("\nTest %i elapsed time = %.09f\n\n", testNumber, (double)time / 1.0e9);

        // write result to file
        if (total > 0)
        {
            // search mode 0, always writes -2 to file
            if (!searchMode)
            {
                writeToBuffer(buffer, textIndex, patternIndex, -2);
                //printf("Test %i, search mode %i, text %i, pattern %i, found patterns at %i\n", testNumber, searchMode, textIndex, patternIndex, -2);
            }
            else // search mode 1, writes actual text index to file
            {
                //printf("Test %i, search mode %i, text %i, pattern %i, found %i patterns at ", testNumber, searchMode, textIndex, patternIndex, total);
                int i;
                for (i = 0; i < total; i++)
                {
                    writeToBuffer(buffer, textIndex, patternIndex, results[i]);
                    //printf("%i ", results[i]);
                }
                //printf("\n");
            }

        }
        else // no pattern found, write -1 to file
        {
            //printf("Test %i, search mode %i, text %i, pattern %i, found patterns at %i\n", testNumber, searchMode, textIndex, patternIndex, -1);
            writeToBuffer(buffer, textIndex, patternIndex, -1);
        }


        // free arrays
        free(results);
        free(procWorkload);
        free(displs);

        // send message to slaves to keep waiting for new data or to stop
        int finished = testNumber == (numberOfTests - 1);
        MPI_Bcast(&finished, 1, MPI_INT,
            MASTER, MPI_COMM_WORLD);

    }

    programTime = getNanos() - programTime;
    printf("\n\nProgram elapsed time = %.09f\n\n", (double)programTime / 1.0e9);

    // in case buffer hasn't done so, we write buffer data to file
    writeBufferToOutput(buffer);

}

/// <summary>
/// Slave instructions: Receive relevant search data and perform a search before sending 
/// the result of the search back to the master. Receive the broadcasted finished flag to
/// determine if slaves should continue to receive data.
/// </summary>
void processSlave()
{

    // tracks whether or not there are still tests to complete
    int finished = 0;

    while (!finished)
    {

#pragma region Declarations and Data Recept

            MPI_Bcast(&testNumber,
            1, MPI_INT, MASTER,
            MPI_COMM_WORLD);

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
        MPI_Recv(textData, textLength,
            MPI_CHAR, MASTER, 1,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE);

#pragma endregion

        // stores results of pattern search
        int* results = NULL;
        int found = processData(searchMode, textData, patternData, startIndex, textLength, patternLength, &results);

        // sending results to master if there are any
        MPI_Send(&found, 1, MPI_INT,
            MASTER, 0,
            MPI_COMM_WORLD);

        if (found > 0)
        {
            MPI_Send(results, found, MPI_INT, MASTER, 0,
                MPI_COMM_WORLD);
        }

        free(results);
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

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nProc);
    MPI_Comm_rank(MPI_COMM_WORLD, &procId);

    // exit if no input directory specified, or fewer than 4 cores
    if (argc < 2 || nProc < 4)
    {
        printf("Not enough arguments: No inputs directory provided.");
        exit(0);
    }

    // determine which function to run based on process ID
    if (procId == MASTER)
    {
        processMaster(argv[1]);
    }
    else
    {
        processSlave();
    }

    MPI_Finalize();


}