#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <algorithm>
#include <sys/wait.h>
#include <sched.h>
#include <queue>

#define waitTimeQuantum { volatile unsigned long i; for(i=0;i<1000000UL;i++);}

using namespace std;

typedef struct process
{
	//The name of the process
	string name;
	//The arrival time of the process
	int readyTime;
	//The execution time of the process
	int executeTime;
	//The pid of the child process when using fork()
	int pid;
	//Check whether the process is picked, for SJFMode only
	bool picked;
}Process;

bool p_cmp (Process a, Process b)
{
	if(a.readyTime == b.readyTime)
		return a.executeTime < b.executeTime;
	else
		return a.readyTime < b.readyTime;
}

bool psjf_cmp (Process a, Process b)
{
	if(a.executeTime == b.executeTime)
		return a.readyTime < b.readyTime;
	else
		return a.executeTime < b.executeTime;
}

double currentTime()
{
	struct timespec ts;
  	clock_gettime(CLOCK_REALTIME,&ts);
  	return ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

void schedulerInit()
{
	/*Set CPU*/
	cpu_set_t cpuMask;
  	CPU_ZERO(&cpuMask);
  	CPU_SET(0, &cpuMask);
	if(sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuMask) != 0)
		printf("Set affinity error.\n");
}

int spawnProcess(Process p)
{
	int pid;
	if (pid = fork())
		return pid;
	else 
	{
		double startTime, endTime;

		startTime = currentTime();//To Do : Get start time
			
		/*Set CPU*/
		cpu_set_t cpuMask;
	   	CPU_ZERO(&cpuMask);
	    	CPU_SET(1, &cpuMask);
	 	if(sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuMask) != 0)
			printf("Set affinity error.\n");

    		/*Execute*/
    		int child_timer = 0;
    		while(p.executeTime > child_timer)
    		{
    			waitTimeQuantum;
			child_timer ++;
    		}

	    	endTime = currentTime();//To Do : Get end timei
		char buffer[200];
		int length = snprintf(buffer, 200,"[Project1] %d %.9f %.9f", getpid(), startTime, endTime);
	    	syscall(333,buffer,length);
		exit(0);
	}
}

void wakeProcess(int pid, int priority)
{
	struct sched_param param;
	param.sched_priority = priority;
	if (sched_setscheduler(pid, SCHED_FIFO, &param) != 0)
		printf("wake failed.\n");
}

void haltProcess(int pid)
{
	struct sched_param param;
	param.sched_priority = 0;
	if (sched_setscheduler(pid, SCHED_IDLE, &param) != 0)
		printf("halt failed.\n");
}

void FifoMode(int n, vector<Process> processList)
{
	schedulerInit();

  	/*Spawn Process based on FIFO policy.*/
	int timer = 0;
  	for(int i = 0; i < n; i++)
  	{
  		while(processList[i].readyTime > timer)
  		{
  			waitTimeQuantum;
  			timer ++;
  		}
  		processList[i].pid = spawnProcess(processList[i]);
        wakeProcess(processList[i].pid, 99 - i);
		cout << processList[i].name << " " << processList[i].pid << endl;
  	}

  	/*Wait all children.*/
  	for(int i = 0; i < n; i++)
  		wait(NULL);
}

void RRMode(int n, vector<Process> processList)
{
	schedulerInit();

	/*Build a ready queue.*/
	queue<int> readyQueue;
	int timer = 0;
	int sliceTimer = 0;
	int jobsLeft = n;
	int spawnedJobs = 0;

	while(jobsLeft > 0)
	{
		while(spawnedJobs < n && processList[spawnedJobs].readyTime <= timer)
		{
			processList[spawnedJobs].pid = spawnProcess(processList[spawnedJobs]);
			cout << processList[spawnedJobs].name << " " << processList[spawnedJobs].pid << endl;
			readyQueue.push(processList[spawnedJobs].pid);
			if(readyQueue.front() == processList[spawnedJobs].pid)
				wakeProcess(processList[spawnedJobs].pid,99);
			else
				haltProcess(processList[spawnedJobs].pid);
			spawnedJobs++;
		}
		if (waitpid(readyQueue.front(), NULL, WNOHANG) == 0) 
		{
	        	if (timer > 0 && sliceTimer == 0) 
	        	{
	          		haltProcess(readyQueue.front());
	          		readyQueue.push(readyQueue.front());
				readyQueue.pop();
	          		wakeProcess(readyQueue.front(),99);
	        	}
	    	} 
	    	else if(!readyQueue.empty()) 
	    	{
	    		readyQueue.pop();
	    		wakeProcess(readyQueue.front(),99);
	    		jobsLeft--;
	    	}
		waitTimeQuantum;
		timer++;
		sliceTimer = timer % 500;
	}
}

void SJFMode(int n, vector<Process> processList)
{
	schedulerInit();

	int order[n] = {};
	int runtime = processList[0].readyTime;
	for(int i = 0; i < n; i++)
	{
		int max = 100000000;
		int next = 0;
		for(int j = 0; j < n; j++)
		{
			if(processList[j].picked == false && runtime >= processList[j].readyTime && max > processList[j].executeTime)
			{
				max = processList[j].executeTime;
				next = j;
			}
		}
		order[i] = next;
                runtime += processList[next].executeTime;
                processList[next].picked = true;
	}
	runtime -= processList[order[n - 1]].executeTime;
 
	int timer = 0;
	int sIndex = 0;
	int rIndex = 0;
	int nextTime = processList[0].readyTime;
	while(runtime > timer)
	{
		while(sIndex < n && timer >= processList[sIndex].readyTime)
		{
			processList[sIndex].pid = spawnProcess(processList[sIndex]);
			haltProcess(processList[sIndex].pid);
			cout << processList[sIndex].name << " " << processList[sIndex].pid << endl;
			sIndex ++;
		}
		if(rIndex < n && timer >= nextTime && timer >= processList[order[rIndex]].readyTime)
		{
			wakeProcess(processList[order[rIndex]].pid, 99 - rIndex);
			nextTime += processList[order[rIndex]].executeTime;
			rIndex ++;
		}
		waitTimeQuantum;
		timer++;
	}

	for(int i = 0; i < n; i++)
  		wait(NULL);
}

void PSJFMode(int n, vector<Process> processList)
{
	schedulerInit();

	vector<Process> readyList;
	int timer = 0;
	int jobsLeft = n;
	int spawned = 0;
	int running = 0;
	int runningTime = 0;
	int flag = 0;

	while(jobsLeft > 0)
	{
		while(flag < n)
		{
			if(processList[flag].readyTime <= timer)
			{
				processList[flag].pid = spawnProcess(processList[flag]);
				cout << processList[flag].name << " " << processList[flag].pid << endl;
				readyList.insert(readyList.end(),processList[flag]);
				haltProcess(processList[flag].pid);
				flag++;
				spawned ++;
				if(running)
				{
					readyList[0].executeTime -= runningTime;
					runningTime = 0;
					haltProcess(readyList[0].pid);
					sort(readyList.begin(),readyList.end(),psjf_cmp);
					wakeProcess(readyList[0].pid, 99);
					running = readyList[0].pid;
				}
			}
			else
				break;
		}
		if(running)
		{
			if(waitpid(running,NULL,WNOHANG) != 0)
			{
				running = 0;
				jobsLeft--;
				spawned--;
				runningTime = 0;
				readyList.erase(readyList.begin());
			}
		}
		if(!running && spawned > 0)
		{
			sort(readyList.begin(),readyList.end(),psjf_cmp);
			wakeProcess(readyList[0].pid,99);
			running = readyList[0].pid;
		}
		waitTimeQuantum;
		timer++;
		if(running)
			runningTime ++;
	} 
}

int main()
{
	string mode;
	int n;
	vector<Process> processList;
	cin >> mode;
	cin >> n;
	for (int i = 0; i < n; i++)
	{
		Process *newProcess = new Process;
		cin >> newProcess->name;
		cin >> newProcess->readyTime;
		cin >> newProcess->executeTime;
		newProcess->pid = -1;
		newProcess->picked = false;
		processList.insert(processList.end(),*newProcess);
	}
	
	/*Sort the vector by its arrival time and execution time.*/
        sort(processList.begin(), processList.end(), p_cmp);

	if(mode == "FIFO")
		FifoMode(n,processList);
	else if(mode == "RR")
		RRMode(n,processList);
	else if(mode == "SJF")
		SJFMode(n,processList);
	else if(mode == "PSJF")
		PSJFMode(n,processList);
	else
		cout << "Scheduling policy error.";
}
