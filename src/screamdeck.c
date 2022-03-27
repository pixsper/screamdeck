#include "screamdeck.h"

#include <stdlib.h>
#include <string.h>
#include <hidapi/hidapi.h>
#include <turbojpeg.h>

#define SD_VENDOR_ID 0x0fd9
#define SD_XL_PRODUCT_ID 0x006c
#define SD_OUT_REPORT_LENGTH 1024
#define SD_OUT_REPORT_HEADER_LENGTH 8
#define SD_OUT_REPORT_IMAGE_LENGTH (SD_OUT_REPORT_LENGTH - SD_OUT_REPORT_HEADER_LENGTH)

typedef struct scdk_device_impl_t
{
	hid_device* device;
	tjhandle jpeg_handle;
	unsigned char* key_image_src_buffer;
	unsigned char* key_image_dst_buffer;
	unsigned char* hid_report_buffer;
} scdk_device_impl_t;

DLL_API scdk_device_info_t* scdk_enumerate(void)
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
	device_impl->hid_report_buffer = malloc(SD_OUT_REPORT_LENGTH);

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
	free(device_impl->hid_report_buffer);

	free(device_impl);
}

bool scdk_set_image(scdk_device_t device, const unsigned char* image_buffer, scdk_pixel_format_e pixel_format)
{
	if (device == NULL)
		return false;

	const scdk_device_impl_t* device_impl = device;

	const size_t pixel_length = pixel_format == SCDK_PIXEL_FORMAT_RGB || pixel_format == SCDK_PIXEL_FORMAT_BGR ? 3 : 4;

	const size_t key_image_src_length = SCDK_KEY_IMAGE_WIDTH * SCDK_KEY_IMAGE_HEIGHT * pixel_length;
	const size_t key_image_line_length = SCDK_KEY_IMAGE_WIDTH * pixel_length;

	for (int key_x = 0; key_x < 8; ++key_x)
	{
		for (int key_y = 0; key_y < 4; ++key_y)
		{
			for (int y = 0; y < SCDK_KEY_IMAGE_HEIGHT; ++y)
			{
				const unsigned char* src = image_buffer
					+ (((key_y * SCDK_KEY_IMAGE_HEIGHT + SCDK_KEY_GAP_WIDTH) + y) * key_image_line_length)
					+ (key_x * (SCDK_KEY_IMAGE_WIDTH + SCDK_KEY_GAP_WIDTH));
				unsigned char* dst = device_impl->key_image_src_buffer + (y * key_image_line_length);

				memcpy(dst, src, key_image_line_length);
			}

			if (!scdk_set_key_image(device, key_x, key_y, device_impl->key_image_src_buffer, pixel_format))
				return false;
		}
	}

	return true;
}


bool scdk_set_key_image(scdk_device_t device, int key_x, int key_y,
                        const unsigned char* image_buffer, scdk_pixel_format_e pixel_format)
{
	if (device == NULL)
		return false;

	if (key_x < 0 || key_x > 8 || key_y < 0 || key_y > 4)
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
	            turbo_pixel_format, &dst_buffer, &dst_buffer_length, TJSAMP_420, 80,TJFLAG_FASTDCT);

	const int report_count = (dst_buffer_length + (SD_OUT_REPORT_IMAGE_LENGTH - 1)) / SD_OUT_REPORT_IMAGE_LENGTH;

	unsigned char* image_p = dst_buffer;

	for (int i = 0; i < report_count; ++i)
	{
		const size_t image_length = min(dst_buffer_length - (image_p - dst_buffer), SD_OUT_REPORT_IMAGE_LENGTH);

		unsigned char* p = device_impl->hid_report_buffer;

		*p++ = 0x02;
		*p++ = 0x07;
		*p++ = key_x + (key_y * 8);
		*p++ = i == report_count - 1 ? 0x01 : 0x00;
		*p++ = image_length & 0xFF;
		*p++ = image_length >> 8;
		*p++ = i & 0xFF;
		*p++ = i >> 8;

		memcpy(p, image_p, image_length);
		p += image_length;
		image_p += image_length;

		while (p - device_impl->hid_report_buffer < SD_OUT_REPORT_LENGTH)
		{
			int test = p - device_impl->hid_report_buffer;

			*p++ = 0;
		}

		const int result = hid_write(device_impl->device, device_impl->hid_report_buffer, SD_OUT_REPORT_LENGTH);
		if (result == -1)
			return false;
	}

	return true;
}
