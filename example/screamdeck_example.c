#include <screamdeck.h>
#include <stdio.h>
#include <stdlib.h>
#include <turbojpeg.h>

#include <Windows.h>

scdk_device_t device;
unsigned char* buffer;

CRITICAL_SECTION critical_section;
ULONGLONG time;

void __stdcall
TimerCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
	if (!TryEnterCriticalSection(&critical_section))
		return;

	ULONGLONG time_now = GetTickCount64();
	printf("FPS = %f\n", 1000. / (time_now - time));
	time = time_now;
	scdk_set_image(device, buffer, SCDK_PIXEL_FORMAT_RGB);

	LeaveCriticalSection(&critical_section);
}

int main(int argc, char* argv[])
{
	scdk_open(&device, NULL);
	if (device == NULL)
		return -1;

	buffer = malloc(SCDK_IMAGE_WIDTH * SCDK_IMAGE_HEIGHT * 3);
	if (buffer == NULL)
		return -1;

	tjhandle handle = tjInitDecompress();


	unsigned long file_length;
	FILE* file = fopen("C:\\LocalDev\\screamdeck\\example\\test.jpg", "rb");
	if (!file)
		return -1;

	fseek(file, 0, SEEK_END);
	file_length = ftell(file);
	fseek(file, 0, SEEK_SET);
	unsigned char* file_buffer = malloc(file_length + 1);
	if (file_buffer == NULL)
		return -1;

	fread(file_buffer, file_length, 1, file);
	fclose(file);


	int result = tjDecompress2(handle, file_buffer, file_length, buffer,
		SCDK_IMAGE_WIDTH, SCDK_IMAGE_WIDTH * 3, SCDK_IMAGE_HEIGHT, TJPF_RGB, 0);

	fclose(file);

	if (!InitializeCriticalSectionAndSpinCount(&critical_section,0x00000400))
		return;

	HANDLE timer_handle;
	CreateTimerQueueTimer(&timer_handle, NULL, TimerCallback, NULL, 1000 / 40, 1000 / 40, WT_EXECUTEDEFAULT);

	getchar();

	DeleteTimerQueueTimer(NULL, timer_handle, NULL);

	DeleteCriticalSection(&critical_section);

	scdk_free(device);

	free(buffer);

	return 0;
}
