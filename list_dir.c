#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>

#include "commons.h"
#include "config.h"
#include "list_dir.h"

/**
 * @struct _DirListenerArgs
 *
 * @var _DirListenerArgs::bufMutex 두 결과값 buffer 보호 Mutex
 * @var _DirListenerArgs::buf 항목들의 stat 결과 저장 공간
 * @var _DirListenerArgs::nameBuf 항목들의 이름 저장 공간
 * @var _DirListenerArgs::bufLen 두 buffer들의 길이
 * @var _DirListenerArgs::totalReadItems 총 읽어들인 개수 저장 변수
 * @var _DirListenerArgs::stopTrd 정지 여부 알림 Condition Variable
 * @var _DirListenerArgs::stopRequested 정지 여부 확인 변수
 * @var _DirListenerArgs::stopMutex 두 정지 변수 보호 Mutex
 */
struct _DirListenerArgs
{
	pthread_mutex_t *bufMutex;
	struct stat *buf;
	char (*nameBuf)[MAX_NAME_LEN + 1];
	size_t bufLen;
	size_t *totalReadItems;
	pthread_cond_t *stopTrd;
	bool *stopRequested;
	pthread_mutex_t *stopMutex;
};
typedef struct _DirListenerArgs DirListenerArgs;

static DirListenerArgs argsArr[MAX_DIRWINS]; // 각 Thread별 runtime 정보 저장
static unsigned int threadCnt = 0;			 // 생성된 Thread 개수

/**
 * (Thread의 loop 함수) 폴더 정보 반복해서 가져옴
 *
 * @param argsPtr thread의 runtime 정보
 * @return 없음
 */
static void *dirListener(void *argsPtr);

/**
 * 현 폴더의 항목 정보 읽어들임
 *
 * @param resultBuf 항목들의 stat 결과 저장할 공간
 * @param nameBuf 항목들의 이름 저장할 공간
 * @param bufLen 최대 읽어올 항목 수
 * @return 성공: (읽은 항목 수), 실패: -1
 */
static ssize_t listEntries(struct stat *resultBuf, char (*nameBuf)[MAX_NAME_LEN + 1], size_t bufLen);

/**
 * 깨어날 시간 계산
 *
 * @param wakeupUs 대기 시간
 * @return 깨어날 시간 (Clock: CLOCK_REALTIME 기준)
 */
static struct timespec getWakeupTime(uint32_t wakeupUs);

int startDirListender(
	pthread_t *newThread,
	pthread_mutex_t *bufMutex,
	struct stat *buf,
	char (*entryNames)[MAX_NAME_LEN + 1],
	size_t bufLen,
	size_t *totalReadItems,
	pthread_cond_t *stopTrd,
	bool *stopRequested,
	pthread_mutex_t *stopMutex)
{
	// 설정 초기화
	DirListenerArgs *newWinArg = argsArr + threadCnt;
	newWinArg->bufMutex = bufMutex;
	newWinArg->buf = buf;
	newWinArg->nameBuf = entryNames;
	newWinArg->bufLen = bufLen;
	newWinArg->totalReadItems = totalReadItems;
	newWinArg->stopTrd = stopTrd;
	newWinArg->stopRequested = stopRequested;
	newWinArg->stopMutex = stopMutex;

	// 새 쓰레드 생성
	if (pthread_create(newThread, NULL, dirListener, newWinArg) == -1)
	{
		return -1;
	}
	return ++threadCnt;
}

void *dirListener(void *argsPtr)
{
	DirListenerArgs args = *(DirListenerArgs *)argsPtr;
	struct timespec startTime, wakeupTime;
	uint64_t elapsedUSec;
	ssize_t readItems;
	int ret;

	while (1)
	{
		CHECK_FAIL(clock_gettime(CLOCK_REALTIME, &startTime)); // iteration 시작 시간 저장

		//////////////////////////////////////////////////////
		// 작업 요청이 들어왔는지 확인
		pthread_mutex_lock(&jobMutex);
		if (currentJob != JOB_NONE)
		{
			switch (currentJob)
			{
			case JOB_ENTER:
				performEnter(args.buf, args.nameBuf, args.bufLen); // 선택된 디렉토리로 이동 함수
			case JOB_COPY:
				performCopy(); //  복사 함수
				break;
			case JOB_MOVE:
				performMove(); // 이동 함수
				break;
			case JOB_REMOVE:
				performRemove(); // 삭제 함수
				break;
			default:
				break;
			}
			// 작업 완료 후, 상태 초기화
			currentJob = JOB_NONE;
		}
		pthread_mutex_unlock(&jobMutex);

		///////////////////////////////////////////////////

		// 현재 폴더 내용 가져옴
		pthread_mutex_lock(args.bufMutex);							  // 결과값 보호 Mutex 획득
		readItems = listEntries(args.buf, args.nameBuf, args.bufLen); // 내용 가져오기
		if (readItems == -1)
			CHECK_FAIL(-1);
		*args.totalReadItems = readItems;
		pthread_mutex_unlock(args.bufMutex); // 결과값 보호 Mutex 해제

		// 설정된 간격만큼 지연 및 종료 요청 처리
		pthread_mutex_lock(args.stopMutex); // 종료 요청 보호 Mutex 획득

		if (*args.stopRequested)
		{ // 종료 요청되었으면: 중지
			pthread_mutex_unlock(args.stopMutex);
			break;
		}

		elapsedUSec = getElapsedTime(startTime); // 실제 지연 시간 계산
		if (elapsedUSec > DIR_INTERVAL_USEC)
		{																			 // 지연 필요하면
			wakeupTime = getWakeupTime(DIR_INTERVAL_USEC - elapsedUSec);			 // 재개할 '절대 시간' 계산
			ret = pthread_cond_timedwait(args.stopTrd, args.stopMutex, &wakeupTime); // 정지 요청 기다리며, 대기
			pthread_mutex_unlock(args.stopMutex);
			switch (ret)
			{
			case ETIMEDOUT: // 대기 완료 -> 실행 재개
				break;
			case EINTR: // 종료 요청됨: 중지
				goto EXIT;
			}
		}
	}
EXIT:
	return NULL;
}

ssize_t listEntries(struct stat *resultBuf, char (*nameBuf)[MAX_NAME_LEN + 1], size_t bufLen)
{
	// TODO: scandirat & versionsort 이용 구현
	// (해당 함수 사용 시, 별도 정렬 불필요)
	// (자동으로 malloc()됨 -> 현재 static buffer 관련 수정 필요, 구현 시 free() 유의하여 구현)
	// (주의: 해당 함수 glibc 비표준 함수임)

	DIR *dir = opendir(".");
	if (dir == NULL)
	{
		return -1;
	}

	size_t readItems = 0;
	errno = 0; // errno 변수는 각 Thread별로 존재 -> Race Condition 없음
	for (struct dirent *ent = readdir(dir); readItems < bufLen; ent = readdir(dir))
	{ // 최대 buffer 길이 만큼의 항목들 읽어들임
		if (ent == NULL)
		{ // 읽기 끝
			if (errno != 0)
			{ // 오류 시
				return -1;
			}
			return readItems; // 오류 없음 -> 계속 진행
		}
		strncpy(nameBuf[readItems], ent->d_name, MAX_NAME_LEN); // 이름 복사
		nameBuf[readItems][MAX_NAME_LEN] = '\0';				// 끝에 null 문자 추가: 파일 이름 매우 긴 경우, strncpy()는 끝에 null문자 쓰지 않을 수도 있음
		if (stat(ent->d_name, resultBuf + readItems) == -1)
		{ // stat 읽어들임
			return -1;
		}
		errno = 0;
		readItems++;
	}
	return readItems;
}

struct timespec getWakeupTime(uint32_t wakeupUs)
{
	struct timespec time;
	CHECK_FAIL(clock_gettime(CLOCK_REALTIME, &time));  // 현재 시간 가져옴
	uint64_t newNsec = time.tv_nsec + wakeupUs * 1000; // 현재 시간 + 지연 시간 계산
	time.tv_sec += newNsec / (1000 * 1000 * 1000);	   // 깨어날 초 설정
	time.tv_nsec = newNsec % (1000 * 1000 * 1000);	   // 깨어날 나노초 설정
	return time;
}

void performCopy()
{
}
void performMove()
{
}
void performRemove()
{
}
void performEnter(struct stat *resultBuf, char (*nameBuf)[MAX_NAME_LEN + 1], size_t bufLen)
{
	if (S_ISDIR(resultBuf->st_mode))
	{
		chdir("..");
	}
	else
	{
		printw("no dir");
	}
}
