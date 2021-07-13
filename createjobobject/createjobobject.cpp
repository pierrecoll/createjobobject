// createjobobject.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <iostream>

DWORD SetInformation(HANDLE hJob, JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli, int dwMemorySize);

int64_t GB = 1024 * 1024 * 1024;
size_t memory_limit = 0;
HANDLE hPort = NULL;
HANDLE hJob = NULL;

DWORD SetInformation(HANDLE hJob, JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli, int dwMemorySize)
{

    memory_limit = dwMemorySize * GB;
    jeli.ProcessMemoryLimit = memory_limit;
    jeli.BasicLimitInformation.LimitFlags |=
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    printf("Calling JobObjectExtendedLimitInformation with job memory limit set to %d Gb\r\n", dwMemorySize);
    if (!::SetInformationJobObject(hJob,
        JobObjectExtendedLimitInformation, &jeli,
        sizeof(jeli))) {
        return ::GetLastError();
    }
    AssignProcessToJobObject(hJob, GetCurrentProcess());
    printf("Job memory limit set to %d Gb\r\n",dwMemorySize);
    return ERROR_SUCCESS;
    //printf("Type enter\r\n");
    //getchar();
}

// Utility function to associate a completion port to a job object.
bool AssociateCompletionPort(HANDLE job, HANDLE port, void* key) {
    JOBOBJECT_ASSOCIATE_COMPLETION_PORT job_acp = { key, port };
    return ::SetInformationJobObject(job,
        JobObjectAssociateCompletionPortInformation,
        &job_acp, sizeof(job_acp))
        ? true
        : false;
}

DWORD WINAPI  JobNotificationThread()
{
    for (;;)
    {
        DWORD dwNoOfBytes = 0;
        ULONG_PTR ulKey = 0;
        OVERLAPPED* pov = NULL;
        /* Wait for a completion notification.*/

        printf("->GetQueuedCompletionStatus\r\n");
        BOOL bResult = GetQueuedCompletionStatus(
            hPort,         /* Completion port handle */
            &dwNoOfBytes,  /* Bytes transferred */
            &ulKey,
            &pov,          /* OVERLAPPED structure */
            INFINITE       /* Notification time-out interval */
        );
        if (!bResult)
        {
            printf("GetQueuedCompletionStatus error\r\n");
            exit (- 1L);
        }
        if (pov == NULL)
        {
            bResult = GetLastError();
            //continue;
        }

        /*
         * Associate actions to job based on notification message
             * and provide a way to exit the loop. Here we can do the handling
         * for the case if flag to disable CPU timer is set.
             */

        switch (dwNoOfBytes)
        {
            case  JOB_OBJECT_MSG_END_OF_JOB_TIME: //          1
                printf("JOB_OBJECT_MSG_END_OF_JOB_TIME\r\n");
                break;
            case  JOB_OBJECT_MSG_END_OF_PROCESS_TIME: //    2
                printf("JOB_OBJECT_MSG_END_OF_PROCESS_TIME\r\n");
                break;
            case  JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT: //  3
                printf("JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT\r\n");
                break;
            case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO: //    4
                printf("JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO\r\n");
                break;
            case  JOB_OBJECT_MSG_NEW_PROCESS: //        6
                printf("JOB_OBJECT_MSG_NEW_PROCESS\r\n");
                printf("New Process Id in Job : %d 0x%X\r\n", pov, pov);
                break;
            case  JOB_OBJECT_MSG_EXIT_PROCESS: //          7
                printf("JOB_OBJECT_MSG_EXIT_PROCESS\r\n");
                break;
            case  JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS: //   8
                printf("JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS\r\n");
                break;
            case  JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT: //    9
                printf("JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT\r\n");
                JOBOBJECT_LIMIT_VIOLATION_INFORMATION limitViolation;
                JOBOBJECT_NOTIFICATION_LIMIT_INFORMATION notificationLimit;

                printf("Process Id : %d 0x%X\r\n", pov, pov);

                ZeroMemory(&limitViolation, sizeof(limitViolation));
                bResult = QueryInformationJobObject(hJob,
                    JobObjectLimitViolationInformation,
                    &limitViolation, sizeof(limitViolation),
                    NULL);
                if (!bResult)
                {
                    DWORD error = GetLastError();
                    printf("QueryInformationJobObject failed with errorcode %d", error);
                }
                else
                {
                    printf("limitViolation.IoWriteBytes :%I64d\r\n", limitViolation.IoWriteBytes);
                    printf("limitViolation.JobMemory :%I64d\r\n", limitViolation.JobMemory);
                }

                ZeroMemory(&notificationLimit, sizeof(notificationLimit));
                bResult = QueryInformationJobObject(hJob,
                    JobObjectNotificationLimitInformation,
                    &notificationLimit, sizeof(notificationLimit),
                    NULL);
                if (!bResult)
                {
                    DWORD error = GetLastError();
                    printf("QueryInformationJobObject failed with errorcode %d", error);
                }
                else
                {
                    printf("notificationLimit.JobMemoryLimit :%I64d\r\n", notificationLimit.JobMemoryLimit);
                }
                TerminateJobObject(hJob, 7012);
                break;
            case  JOB_OBJECT_MSG_JOB_MEMORY_LIMIT: //        10
                printf("JOB_OBJECT_MSG_JOB_MEMORY_LIMIT\r\n");
                break;
            case JOB_OBJECT_MSG_NOTIFICATION_LIMIT: //    11
                printf("JOB_OBJECT_MSG_NOTIFICATION_LIMIT\r\n");
                break;
            case  JOB_OBJECT_MSG_JOB_CYCLE_TIME_LIMIT: //   12
                printf("JOB_OBJECT_MSG_JOB_CYCLE_TIME_LIMIT\r\n");
                break;
            case JOB_OBJECT_MSG_SILO_TERMINATED: //      13
                printf("JOB_OBJECT_MSG_SILO_TERMINATED\r\n");
                break;
            default:
                printf("unknow msg\r\n");
                break;
        }
    }
    return 0;
}

int main()
{
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};

	hJob= CreateJobObjectW(NULL, L"pierrelc_job");
    jeli.BasicLimitInformation.LimitFlags |=
        JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    int dwMemorySize = 1;
    DWORD dwError;
    for (dwMemorySize = 1; dwMemorySize < 128; dwMemorySize++)
    {
        dwError= SetInformation(hJob, jeli, dwMemorySize);
        if (dwError != ERROR_SUCCESS)
        {
            printf("JobObjectExtendedLimitInformation returning error : 0%X\r\n", dwError);
        }
    }

    printf("Calling JobObjectExtendedLimitInformation with job memory limit set to 1024*1024\r\n");
    memory_limit = 1024*1024;
    jeli.ProcessMemoryLimit = memory_limit;
    jeli.BasicLimitInformation.LimitFlags |=
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!::SetInformationJobObject(hJob,
        JobObjectExtendedLimitInformation, &jeli,
        sizeof(jeli))) {
        printf("JobObjectExtendedLimitInformation returning error : 0%X\r\n", GetLastError());
        return ::GetLastError();
    }

    hPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    DWORD key = 1;
    AssociateCompletionPort(hJob, hPort, &key);

    CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)JobNotificationThread, NULL, NULL, NULL);
    printf("Job memory limit 1024*1024 bytes\r\n");

    __try
    {
        for (int i = 1; i < 128; i++)
        {
            printf("allocating %d*1024 bytes\r\n", i);
            new int[i * 1024];
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        printf("Exiting process : %d\n", GetLastError());
    }
}
