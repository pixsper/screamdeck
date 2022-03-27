#include <screamdeck.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
	scdk_device_t device;

	scdk_open(&device, NULL);
	if (device == NULL)
		return -1;

	unsigned char* buffer = malloc(SCDK_IMAGE_WIDTH * SCDK_IMAGE_HEIGHT * 3);
	if (buffer == NULL)
		return -1;

	for (size_t i = 0; i < SCDK_IMAGE_WIDTH * SCDK_IMAGE_HEIGHT * 3; i += 3)
	{
		buffer[i] = 255;
		buffer[i + 1] = 0;
		buffer[i + 2] = 255;
	}

	scdk_set_image(device, buffer, SCDK_PIXEL_FORMAT_RGB);

	scdk_free(device);

	free(buffer);
}
