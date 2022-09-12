#include "screamdeck.h"

#include <stdlib.h>
#include <string.h>
#include <hidapi/hidapi.h>
#include <turbojpeg.h>
#include <xxhash.h>

#define SD_VENDOR_ID 0x0fd9
#define SD_XL_PRODUCT_ID 0x006c
#define SD_OUT_FEATURE_REPORT_LENGTH 32
#define SD_OUT_REPORT_LENGTH 1024
#define SD_OUT_REPORT_HEADER_LENGTH 8
#define SD_OUT_REPORT_IMAGE_LENGTH (SD_OUT_REPORT_LENGTH - SD_OUT_REPORT_HEADER_LENGTH)
#define SD_IN_REPORT_HEADER_LENGTH 4
#define SD_IN_REPORT_LENGTH (SCDK_KEY_GRID_WIDTH * SCDK_KEY_GRID_HEIGHT) + SD_IN_REPORT_HEADER_LENGTH

#define SCDK_MAX(a, b) a > b ? a : b
#define SCDK_MIN(a, b) a < b ? a : b
#define SCDK_CLAMP(x, lo, hi) SCDK_MIN(hi, (SCDK_MAX(lo, x)))

typedef struct scdk_device_impl_t
{
	hid_device* device;
	tjhandle jpeg_handle;
	unsigned char* key_image_src_buffer;
	unsigned char* key_image_dst_buffer;
	unsigned char* hid_out_feature_report_buffer;
	unsigned char* hid_out_report_buffer;
	unsigned char* hid_in_report_buffer;

	XXH64_hash_t* key_image_hashes;
} scdk_device_impl_t;

scdk_device_info_t* scdk_enumerate(void)
{
	struct hid_device_info* hid_devices = hid_enumerate(SD_VENDOR_ID, SD_XL_PRODUCT_ID);
	if (hid_devices == NULL)
		return NULL;

	scdk_device_info_t* scdk_devices = NULL;

	struct hid_device_info* hid_d = hid_devices;
	scdk_device_info_t* scdk_d = NULL;

	do
	{
		scdk_device_info_t* last = scdk_d;

		scdk_d = malloc(sizeof(scdk_device_info_t));
		scdk_d->next = NULL;
		if (scdk_d == NULL)
			abort();

		if (last != NULL)
			last->next = scdk_d;

		if (scdk_devices == NULL)
			scdk_devices = scdk_d;

		scdk_d->serial_number = malloc((wcslen(hid_d->serial_number) + 1) * sizeof(wchar_t));
		if (scdk_d->serial_number == NULL)
			abort();
		wcscpy(scdk_d->serial_number, hid_d->serial_number);

		hid_d = hid_d->next;
	}
	while (hid_d != NULL);

	hid_free_enumeration(hid_devices);

	return scdk_devices;
}

void scdk_free_enumeration(scdk_device_info_t* devices)
{
	scdk_device_info_t* d = devices;
	while (d != NULL)
	{
		free(d->serial_number);

		d = devices->next;
		free(d);
	}
}

bool scdk_open(scdk_device_t* p_device, const wchar_t* serial_number)
{
	hid_device* hid_d = hid_open(SD_VENDOR_ID, SD_XL_PRODUCT_ID, serial_number);
	if (hid_d == NULL)
		return false;

	scdk_device_impl_t* device_impl = malloc(sizeof(scdk_device_impl_t));
	if (device_impl == NULL)
		abort();

	device_impl->device = hid_d;
	device_impl->jpeg_handle = tjInitCompress();
	device_impl->key_image_src_buffer = malloc(SCDK_KEY_IMAGE_WIDTH * SCDK_KEY_IMAGE_HEIGHT * 4);
	device_impl->key_image_dst_buffer = malloc(tjBufSize(SCDK_KEY_IMAGE_WIDTH, SCDK_KEY_IMAGE_HEIGHT, TJSAMP_420));
	device_impl->hid_out_feature_report_buffer = malloc(SD_OUT_FEATURE_REPORT_LENGTH);
	device_impl->hid_out_report_buffer = malloc(SD_OUT_REPORT_LENGTH);
	device_impl->hid_in_report_buffer = malloc(SD_IN_REPORT_LENGTH);
	device_impl->key_image_hashes = malloc(SCDK_KEY_GRID_WIDTH * SCDK_KEY_GRID_HEIGHT * sizeof(XXH64_hash_t));

	*p_device = device_impl;
	return true;
}

void scdk_free(scdk_device_t device)
{
	if (device == NULL)
		return;

	scdk_device_impl_t* device_impl = device;

	hid_close(device_impl->device);

	tjDestroy(device_impl->jpeg_handle);

	free(device_impl->key_image_src_buffer);
	free(device_impl->key_image_dst_buffer);
	free(device_impl->hid_out_feature_report_buffer);
	free(device_impl->hid_out_report_buffer);
	free(device_impl->hid_in_report_buffer);
	free(device_impl->key_image_hashes);

	free(device_impl);
}

bool scdk_read_key(scdk_device_t device, bool* key_state_buffer, int key_state_buffer_length, int timeout_ms)
{
	const scdk_device_impl_t* device_impl = device;

	int bytes = hid_read_timeout(device_impl->device, device_impl->hid_in_report_buffer,
	                             SD_IN_REPORT_LENGTH, timeout_ms);
	if (bytes == -1)
		return false;

	for (int i = SD_IN_REPORT_HEADER_LENGTH; i < bytes && i + SD_IN_REPORT_HEADER_LENGTH < key_state_buffer_length; ++i)
		key_state_buffer[i - SD_IN_REPORT_HEADER_LENGTH] = device_impl->hid_in_report_buffer[i] > 0;

	return true;
}

bool scdk_set_image(scdk_device_t device, const unsigned char* image_buffer,
                    scdk_pixel_format_e pixel_format, int quality_percentage)
{
	if (pixel_format == SCDK_PIXEL_FORMAT_RGB || pixel_format == SCDK_PIXEL_FORMAT_BGR)
		return scdk_set_image_24(device, image_buffer, pixel_format, quality_percentage);
	else
		return scdk_set_image_32(device, image_buffer, pixel_format, quality_percentage);
}

bool scdk_set_image_24(scdk_device_t device, const unsigned char* image_buffer,
                       scdk_pixel_format_e pixel_format, int quality_percentage)
{
	if (device == NULL || (pixel_format != SCDK_PIXEL_FORMAT_RGB && pixel_format != SCDK_PIXEL_FORMAT_BGR))
		return false;

	const scdk_device_impl_t* device_impl = device;

	const size_t key_image_line_length = SCDK_KEY_IMAGE_WIDTH * 3;

	for (int key_x = 0; key_x < SCDK_KEY_GRID_WIDTH; ++key_x)
	{
		for (int key_y = 0; key_y < SCDK_KEY_GRID_HEIGHT; ++key_y)
		{
			for (int y = 0; y < SCDK_KEY_IMAGE_HEIGHT; ++y)
			{
				const int line = ((key_y * (SCDK_KEY_IMAGE_HEIGHT + SCDK_KEY_GAP_WIDTH)) + SCDK_KEY_IMAGE_HEIGHT) - y;
				const int row = (key_x * (SCDK_KEY_IMAGE_WIDTH + SCDK_KEY_GAP_WIDTH)) * 3;

				const int offset = (line * (SCDK_IMAGE_WIDTH * 3))
					+ row
					+ key_image_line_length
					- 1;

				const unsigned char* src = image_buffer + offset;
				unsigned char* dst = device_impl->key_image_src_buffer + (y * key_image_line_length);

				unsigned char r, g, b;

				for (size_t i = 0; i < key_image_line_length; i += 3)
				{
					b = *src--;
					g = *src--;
					r = *src--;

					*dst++ = r;
					*dst++ = g;
					*dst++ = b;
				}
			}

			XXH64_hash_t* last_hash = device_impl->key_image_hashes + key_x + (key_y * SCDK_KEY_GRID_WIDTH);
			const XXH64_hash_t hash = XXH64(device_impl->key_image_src_buffer,
			                                SCDK_KEY_IMAGE_WIDTH * SCDK_KEY_IMAGE_HEIGHT * 4, 0);

			if (*last_hash != hash)
			{
				if (!scdk_set_key_image(device, key_x, key_y, device_impl->key_image_src_buffer, pixel_format,
				                        quality_percentage))
					return false;

				*last_hash = hash;
			}
		}
	}

	return true;
}

bool scdk_set_image_32(scdk_device_t device, const unsigned char* image_buffer,
                       scdk_pixel_format_e pixel_format, int quality_percentage)
{
	if (device == NULL || pixel_format == SCDK_PIXEL_FORMAT_RGB || pixel_format == SCDK_PIXEL_FORMAT_BGR)
		return false;

	const scdk_device_impl_t* device_impl = device;

	const size_t key_image_line_length = SCDK_KEY_IMAGE_WIDTH * 4;

	for (int key_x = 0; key_x < SCDK_KEY_GRID_WIDTH; ++key_x)
	{
		for (int key_y = 0; key_y < SCDK_KEY_GRID_HEIGHT; ++key_y)
		{
			for (int y = 0; y < SCDK_KEY_IMAGE_HEIGHT; ++y)
			{
				const int line = ((key_y * (SCDK_KEY_IMAGE_HEIGHT + SCDK_KEY_GAP_WIDTH)) + SCDK_KEY_IMAGE_HEIGHT) - y -
					1;
				const int row = (key_x * (SCDK_KEY_IMAGE_WIDTH + SCDK_KEY_GAP_WIDTH)) * 4;

				const int offset = (line * (SCDK_IMAGE_WIDTH * 4))
					+ row
					+ key_image_line_length
					- 1;

				const unsigned char* src = image_buffer + offset;
				unsigned char* dst = device_impl->key_image_src_buffer + (y * key_image_line_length);

				unsigned char r, g, b, a;

				for (size_t i = 0; i < key_image_line_length; i += 4)
				{
					a = *src--;
					b = *src--;
					g = *src--;
					r = *src--;

					*dst++ = r;
					*dst++ = g;
					*dst++ = b;
					*dst++ = a;
				}
			}

			XXH64_hash_t* last_hash = device_impl->key_image_hashes + key_x + (key_y * SCDK_KEY_GRID_WIDTH);
			const XXH64_hash_t hash = XXH64(device_impl->key_image_src_buffer,
			                                SCDK_KEY_IMAGE_WIDTH * SCDK_KEY_IMAGE_HEIGHT * 4, 0);

			if (*last_hash != hash)
			{
				if (!scdk_set_key_image(device, key_x, key_y, device_impl->key_image_src_buffer, pixel_format,
				                        quality_percentage))
					return false;

				*last_hash = hash;
			}
		}
	}

	return true;
}


bool scdk_set_key_image(scdk_device_t device, int key_x, int key_y, const unsigned char* image_buffer,
                        scdk_pixel_format_e pixel_format, int quality_percentage)
{
	if (device == NULL)
		return false;

	if (key_x < 0 || key_x > SCDK_KEY_GRID_WIDTH || key_y < 0 || key_y > SCDK_KEY_GRID_HEIGHT)
		return false;

	const scdk_device_impl_t* device_impl = device;

	enum TJPF turbo_pixel_format;
	switch (pixel_format)
	{
	case SCDK_PIXEL_FORMAT_RGB: turbo_pixel_format = TJPF_RGB;
		break;
	case SCDK_PIXEL_FORMAT_BGR: turbo_pixel_format = TJPF_BGR;
		break;
	case SCDK_PIXEL_FORMAT_RGBX: turbo_pixel_format = TJPF_RGBX;
		break;
	case SCDK_PIXEL_FORMAT_BGRX: turbo_pixel_format = TJPF_BGRX;
		break;
	case SCDK_PIXEL_FORMAT_XBGR: turbo_pixel_format = TJPF_XBGR;
		break;
	case SCDK_PIXEL_FORMAT_XRGB: turbo_pixel_format = TJPF_XRGB;
		break;
	case SCDK_PIXEL_FORMAT_RGBA: turbo_pixel_format = TJPF_RGBA;
		break;
	case SCDK_PIXEL_FORMAT_BGRA: turbo_pixel_format = TJPF_BGRA;
		break;
	case SCDK_PIXEL_FORMAT_ABGR: turbo_pixel_format = TJPF_ABGR;
		break;
	case SCDK_PIXEL_FORMAT_ARGB: turbo_pixel_format = TJPF_ARGB;
		break;
	default: return false;
	}

	unsigned char* dst_buffer = device_impl->key_image_dst_buffer;
	unsigned long dst_buffer_length;

	tjCompress2(device_impl->jpeg_handle, image_buffer, SCDK_KEY_IMAGE_WIDTH, 0, SCDK_KEY_IMAGE_HEIGHT,
	            turbo_pixel_format, &dst_buffer, &dst_buffer_length, TJSAMP_420, quality_percentage, TJFLAG_FASTDCT);

	const int report_count = (dst_buffer_length + (SD_OUT_REPORT_IMAGE_LENGTH - 1)) / SD_OUT_REPORT_IMAGE_LENGTH;

	unsigned char* image_p = dst_buffer;

	for (int i = 0; i < report_count; ++i)
	{
		const size_t image_length = SCDK_MIN(dst_buffer_length - (image_p - dst_buffer), SD_OUT_REPORT_IMAGE_LENGTH);

		unsigned char* p = device_impl->hid_out_report_buffer;

		*p++ = 0x02;
		*p++ = 0x07;
		*p++ = key_x + (key_y * SCDK_KEY_GRID_WIDTH);
		*p++ = i == report_count - 1 ? 0x01 : 0x00;
		*p++ = image_length & 0xFF;
		*p++ = image_length >> 8;
		*p++ = i & 0xFF;
		*p++ = i >> 8;

		memcpy(p, image_p, image_length);
		p += image_length;
		image_p += image_length;

		while (p - device_impl->hid_out_report_buffer < SD_OUT_REPORT_LENGTH)
			*p++ = 0;

		const int result = hid_write(device_impl->device, device_impl->hid_out_report_buffer, SD_OUT_REPORT_LENGTH);
		if (result == -1)
			return false;
	}

	return true;
}

bool scdk_set_brightness(scdk_device_t device, int brightness_percentage)
{
	const scdk_device_impl_t* device_impl = device;
	unsigned char* p = device_impl->hid_out_report_buffer;

	*p++ = 0x03;
	*p++ = 0x08;
	*p++ = SCDK_CLAMP(brightness_percentage, 0, 100);
	*p++ = 0x23;
	*p++ = 0xB8;
	*p++ = 0x01;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0xA5;
	*p++ = 0x49;
	*p++ = 0xCD;
	*p++ = 0x02;
	*p++ = 0xFE;
	*p++ = 0x7F;
	*p++ = 0x00;
	*p++ = 0x00;

	const int result = hid_send_feature_report(device_impl->device, device_impl->hid_out_report_buffer,
	                                           SD_OUT_FEATURE_REPORT_LENGTH);
	return result != -1;
}

bool scdk_set_screensaver(scdk_device_t device)
{
	const scdk_device_impl_t* device_impl = device;
	unsigned char* p = device_impl->hid_out_report_buffer;

	*p++ = 0x03;
	*p++ = 0x02;

	while (p - device_impl->hid_out_report_buffer < SD_OUT_FEATURE_REPORT_LENGTH)
		*p++ = 0;

	const int result = hid_send_feature_report(device_impl->device, device_impl->hid_out_report_buffer,
	                                           SD_OUT_FEATURE_REPORT_LENGTH);
	return result != -1;
}
