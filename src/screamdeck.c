#include "screamdeck.h"

#include <stdlib.h>
#include <string.h>
#include <hidapi/hidapi.h>
#include <turbojpeg.h>
#include <xxhash.h>

#define SD_VENDOR_ID 0x0fd9

#define SCDK_MAX(a, b) a > b ? a : b
#define SCDK_MIN(a, b) a < b ? a : b
#define SCDK_CLAMP(x, lo, hi) SCDK_MIN(hi, (SCDK_MAX(lo, x)))

#define SD_OUT_FEATURE_REPORT_LENGTH 32
#define SD_OUT_REPORT_LENGTH 1024
#define SD_OUT_REPORT_HEADER_LENGTH 8
#define SD_OUT_REPORT_IMAGE_LENGTH (SD_OUT_REPORT_LENGTH - SD_OUT_REPORT_HEADER_LENGTH)
#define SD_IN_REPORT_HEADER_LENGTH 4

#define SDCK_INFO(name, type, columns, rows, key_width, key_height, key_gap_width, key_gap_height) static const scdk_device_type_info_t name =\
{                                                                                                                                             \
type,                                                                                                                                         \
columns, rows,                                                                                                                                \
key_width, key_height,                                                                                                                        \
key_gap_width, key_gap_height,                                                                                                                \
(key_width * columns) + (key_gap_width * (columns - 1)), (key_height * rows) + (key_gap_height * (rows - 1))                                  \
}

SDCK_INFO(SCDK_DEVICE_TYPE_INFO_MINI, SCDK_DEVICE_TYPE_MINI, 3, 2, 80, 80, 38, 38);
SDCK_INFO(SCDK_DEVICE_TYPE_INFO_MINI_MK2, SCDK_DEVICE_TYPE_MINI_MK2, 3, 2, 80, 80, 38, 38);
SDCK_INFO(SCDK_DEVICE_TYPE_INFO_ORIGINAL, SCDK_DEVICE_TYPE_ORIGINAL, 5, 3, 72, 72, 38, 38);
SDCK_INFO(SCDK_DEVICE_TYPE_INFO_ORIGINAL_MK2, SCDK_DEVICE_TYPE_ORIGINAL_MK2, 5, 3, 72, 72, 38, 38);
SDCK_INFO(SCDK_DEVICE_TYPE_INFO_MK2, SCDK_DEVICE_TYPE_MK2, 5, 3, 72, 72, 38, 38);
SDCK_INFO(SCDK_DEVICE_TYPE_INFO_XL, SCDK_DEVICE_TYPE_XL, 8, 4, 96, 96, 38, 38);
SDCK_INFO(SCDK_DEVICE_TYPE_INFO_XL_MK2, SCDK_DEVICE_TYPE_XL_MK2, 8, 4, 96, 96, 38, 38);

const scdk_device_type_info_t* scdk_get_device_type_info_from_type(scdk_device_type_e device_type)
{
	switch(device_type)
	{
		default:
		case SCDK_DEVICE_TYPE_NONE:
			return NULL;
		case SCDK_DEVICE_TYPE_MINI:
			return &SCDK_DEVICE_TYPE_INFO_MINI;
		case SCDK_DEVICE_TYPE_MINI_MK2:
			return &SCDK_DEVICE_TYPE_INFO_MINI_MK2;
		case SCDK_DEVICE_TYPE_ORIGINAL:
			return &SCDK_DEVICE_TYPE_INFO_ORIGINAL;
		case SCDK_DEVICE_TYPE_ORIGINAL_MK2:
			return &SCDK_DEVICE_TYPE_INFO_ORIGINAL_MK2;
		case SCDK_DEVICE_TYPE_MK2:
			return &SCDK_DEVICE_TYPE_INFO_MK2;
		case SCDK_DEVICE_TYPE_XL:
			return &SCDK_DEVICE_TYPE_INFO_XL;
		case SCDK_DEVICE_TYPE_XL_MK2:
			return &SCDK_DEVICE_TYPE_INFO_XL_MK2;
	}
}


typedef struct scdk_device_impl_t
{
	hid_device* device;
	const scdk_device_type_info_t* type_info;
	tjhandle jpeg_handle;

	size_t key_image_src_buffer_length;
	unsigned char* key_image_src_buffer;
	size_t key_image_dst_buffer_length;
	unsigned char* key_image_dst_buffer;
	unsigned char* hid_out_feature_report_buffer;
	unsigned char* hid_out_report_buffer;
	size_t hid_in_report_buffer_length;
	unsigned char* hid_in_report_buffer;

	XXH64_hash_t* key_image_hashes;
} scdk_device_impl_t;

scdk_device_info_t* scdk_enumerate(void)
{
	struct hid_device_info* hid_devices = hid_enumerate(SD_VENDOR_ID, 0);
	if (hid_devices == NULL)
		return NULL;

	scdk_device_info_t* scdk_devices = NULL;

	struct hid_device_info* hid_d = hid_devices;
	scdk_device_info_t* scdk_d = NULL;

	do
	{
		scdk_device_info_t* last = scdk_d;

		if (hid_d->product_id == SCDK_DEVICE_TYPE_ORIGINAL
		    || hid_d->product_id == SCDK_DEVICE_TYPE_ORIGINAL_MK2
		    || hid_d->product_id == SCDK_DEVICE_TYPE_MINI
		    || hid_d->product_id == SCDK_DEVICE_TYPE_MINI_MK2
		    || hid_d->product_id == SCDK_DEVICE_TYPE_XL
		    || hid_d->product_id == SCDK_DEVICE_TYPE_XL_MK2)
		{
			scdk_d = malloc(sizeof(scdk_device_info_t));
			scdk_d->next = NULL;
			scdk_d->device_type = hid_d->product_id;

			if (last)
				last->next = scdk_d;

			if (scdk_devices == NULL)
				scdk_devices = scdk_d;

			scdk_d->serial_number = malloc((wcslen(hid_d->serial_number) + 1) * sizeof(wchar_t));
			if (scdk_d->serial_number == NULL)
				abort();
			wcscpy(scdk_d->serial_number, hid_d->serial_number);
		}

		hid_d = hid_d->next;
	}
	while (hid_d != NULL);

	hid_free_enumeration(hid_devices);

	return scdk_devices;
}

void scdk_free_enumeration(scdk_device_info_t* devices)
{
	scdk_device_info_t* d = devices;
	while (d)
	{
		free(d->serial_number);

		scdk_device_info_t* last_d = d;
		d = d->next;
		free(last_d);
	}
}

bool scdk_open(scdk_device_t* p_device, scdk_device_type_e device_type, const wchar_t* serial_number)
{
	hid_device* hid_d = hid_open(SD_VENDOR_ID, device_type, serial_number);
	if (hid_d == NULL)
	{
		*p_device = NULL;
		return false;
	}

	struct hid_device_info* info = hid_get_device_info(hid_d);

	const scdk_device_type_info_t* type_info = scdk_get_device_type_info_from_type(info->product_id);
	if (!type_info)
	{
		*p_device = NULL;
		return false;
	}

	scdk_device_impl_t* device_impl = malloc(sizeof(scdk_device_impl_t));
	if (device_impl == NULL)
		abort();

	device_impl->device = hid_d;
	device_impl->type_info = type_info;
	device_impl->jpeg_handle = tjInitCompress();
	device_impl->key_image_src_buffer_length = device_impl->type_info->image_width * device_impl->type_info->image_height * 4;
	device_impl->key_image_src_buffer = malloc(device_impl->key_image_src_buffer_length);
	device_impl->key_image_dst_buffer_length = tjBufSize(device_impl->type_info->key_image_width, device_impl->type_info->key_image_height, TJSAMP_420);
	device_impl->key_image_dst_buffer = malloc(device_impl->key_image_dst_buffer_length);
	device_impl->hid_out_feature_report_buffer = malloc(SD_OUT_FEATURE_REPORT_LENGTH);
	device_impl->hid_out_report_buffer = malloc(SD_OUT_REPORT_LENGTH);
	device_impl->hid_in_report_buffer_length = (device_impl->type_info->rows * device_impl->type_info->columns) + SD_IN_REPORT_HEADER_LENGTH;
	device_impl->hid_in_report_buffer = malloc(device_impl->hid_in_report_buffer_length);
	device_impl->key_image_hashes = malloc(device_impl->type_info->columns * device_impl->type_info->rows * sizeof(XXH64_hash_t));

	*p_device = device_impl;
	return true;
}

bool scdk_open_first(scdk_device_t* p_device, scdk_device_type_e device_type)
{
	bool is_success = false;
	scdk_device_info_t* device_info = scdk_enumerate();
	while(device_info)
	{
		if (device_type == SCDK_DEVICE_TYPE_NONE || device_type == device_info->device_type)
		{
			is_success = scdk_open(p_device, device_info->device_type, device_info->serial_number);
			if (is_success)
				break;
		}

		device_info = device_info->next;
	}

	scdk_free_enumeration(device_info);
	return is_success;
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

const scdk_device_type_info_t* scdk_get_device_type_info(scdk_device_t device)
{
	scdk_device_impl_t* device_impl = device;
	return device_impl->type_info;
}

bool scdk_get_serial_number(scdk_device_t device, wchar_t* serial_number, size_t serial_number_length)
{
	const scdk_device_impl_t* device_impl = device;
	return hid_get_serial_number_string(device_impl->device, serial_number, serial_number_length) == 0;
}

int scdk_read_key(scdk_device_t device, bool* key_state_buffer, size_t key_state_buffer_length)
{
	const scdk_device_impl_t* device_impl = device;

	const int bytes = hid_read(device_impl->device, device_impl->hid_in_report_buffer,
	                                   device_impl->hid_in_report_buffer_length);
	if (bytes == -1)
		return -1;

	for (int i = SD_IN_REPORT_HEADER_LENGTH; i < bytes && i - SD_IN_REPORT_HEADER_LENGTH < (int)key_state_buffer_length; ++i)
		key_state_buffer[i - SD_IN_REPORT_HEADER_LENGTH] = device_impl->hid_in_report_buffer[i] > 0;

	return bytes;
}

int scdk_read_key_timeout(scdk_device_t device, bool* key_state_buffer, size_t key_state_buffer_length, int timeout_ms)
{
	const scdk_device_impl_t* device_impl = device;

	const int bytes = hid_read_timeout(device_impl->device, device_impl->hid_in_report_buffer,
	                                   device_impl->hid_in_report_buffer_length, timeout_ms);
	if (bytes == -1)
		return -1;

	for (int i = SD_IN_REPORT_HEADER_LENGTH; i < bytes && i - SD_IN_REPORT_HEADER_LENGTH < (int)key_state_buffer_length; ++i)
		key_state_buffer[i - SD_IN_REPORT_HEADER_LENGTH] = device_impl->hid_in_report_buffer[i] > 0;

	return bytes;
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
	const scdk_device_type_info_t* type_info = device_impl->type_info;
	const int key_image_line_length = device_impl->type_info->key_image_width * 3;


	for (int key_x = 0; key_x < type_info->columns; ++key_x)
	{
		for (int key_y = 0; key_y < type_info->rows; ++key_y)
		{
			for (int y = 0; y < type_info->key_image_height; ++y)
			{
				const int line = ((key_y * (type_info->key_image_height + type_info->key_gap_height)) + type_info->key_image_height) - y;
				const int row = (key_x * (type_info->key_image_width + type_info->key_gap_width)) * 3;

				const int offset = (line * (type_info->image_width * 3))
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

			XXH64_hash_t* last_hash = device_impl->key_image_hashes + key_x + (key_y * type_info->columns);
			const XXH64_hash_t hash = XXH64(device_impl->key_image_src_buffer,
			                                type_info->image_width * type_info->image_height * 4, 0);

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
	const scdk_device_type_info_t* type_info = device_impl->type_info;
	const size_t key_image_line_length = device_impl->type_info->key_image_width * 4;

	for (int key_x = 0; key_x < type_info->columns; ++key_x)
	{
		for (int key_y = 0; key_y < type_info->rows; ++key_y)
		{
			for (int y = 0; y < type_info->image_height; ++y)
			{
				const int line = ((key_y * (type_info->key_image_height + type_info->key_gap_height)) + type_info->key_image_height) - y -
					1;
				const int row = (key_x * (type_info->key_image_width + type_info->key_gap_width)) * 4;

				const int offset = (line * (type_info->image_width * 4))
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

			XXH64_hash_t* last_hash = device_impl->key_image_hashes + key_x + (key_y * type_info->columns);
			const XXH64_hash_t hash = XXH64(device_impl->key_image_src_buffer,
			                                type_info->key_image_width * type_info->key_image_height * 4, 0);

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

	const scdk_device_impl_t* device_impl = device;
	const scdk_device_type_info_t* type_info = device_impl->type_info;

	if (key_x < 0 || key_x > type_info->columns || key_y < 0 || key_y > type_info->rows)
		return false;

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

	tjCompress2(device_impl->jpeg_handle, image_buffer, type_info->key_image_width, 0, type_info->key_image_height,
	            turbo_pixel_format, &dst_buffer, &dst_buffer_length, TJSAMP_420,
				quality_percentage, TJFLAG_FASTDCT);

	const int report_count = (dst_buffer_length + (SD_OUT_REPORT_IMAGE_LENGTH - 1)) / SD_OUT_REPORT_IMAGE_LENGTH;

	unsigned char* image_p = dst_buffer;

	for (int i = 0; i < report_count; ++i)
	{
		const size_t image_length = SCDK_MIN(dst_buffer_length - (image_p - dst_buffer), SD_OUT_REPORT_IMAGE_LENGTH);

		unsigned char* p = device_impl->hid_out_report_buffer;

		*p++ = 0x02;
		*p++ = 0x07;
		*p++ = key_x + (key_y * type_info->columns);
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
