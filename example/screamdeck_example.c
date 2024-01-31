#include <screamdeck.h>
#include <stdio.h>
#include <stdlib.h>
#include <turbojpeg.h>

int main(int argc, char* argv[])
{
	scdk_device_t device = NULL;
	unsigned char* buffer;

	scdk_device_info_t* devices = scdk_enumerate();
	scdk_free_enumeration(devices);

	scdk_open_first(&device, SCDK_DEVICE_TYPE_XL);
	if (device == NULL)
		return -1;

	const scdk_device_type_info_t* type_info = scdk_get_device_type_info(device);

	buffer = malloc(type_info->image_width * type_info->image_height * 3);
	if (buffer == NULL)
		return -1;

	tjhandle handle = tjInitDecompress();


	unsigned long file_length;
	FILE* file = fopen("../example/test.jpg", "rb");
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
		type_info->image_width, type_info->image_width * 3, type_info->image_height, TJPF_RGB, 0);

	fclose(file);

	wchar_t serial_number[32];
	scdk_get_serial_number(device, serial_number, 32);

	scdk_set_brightness(device, 100);

	scdk_set_image(device, buffer, SCDK_PIXEL_FORMAT_RGB, 100);

	bool* keys = (bool*)calloc(type_info->columns * type_info->rows, sizeof(bool));
	bool* keys_buffer = (bool*)calloc(type_info->columns * type_info->rows, sizeof(bool));
	while(true)
	{
		const int key_result = scdk_read_key_timeout(device, keys_buffer, type_info->columns * type_info->rows, 1000 / 60);
		if (key_result == -1)
		{
			break;
		}
		else if (key_result == 0)
		{
			for(int i = 0; i < type_info->columns * type_info->rows; ++i)
			{
				if (keys[i] != keys_buffer[i])
				{
					if (keys[i])
						printf("Key %d down\n", i);
					else
						printf("Key %d up\n", i);

					keys[i] = keys_buffer[i];
				}
			}
		}
	}
	free(keys);
	free(keys_buffer);

	scdk_free(device);

	free(buffer);

	return 0;
}
