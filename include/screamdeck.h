#ifndef SCREAMDECK_H
#define SCREAMDECK_H

#include <stdbool.h>
#include <wchar.h>

#ifdef screamdeck_EXPORTS
#ifdef _MSC_VER
#define DLL_API __declspec(dllexport)
#else
#define DLL_API
#endif
#else
#ifdef _MSC_VER
#define DLL_API __declspec(dllimport)
#else
#define DLL_API
#endif
#endif

typedef enum scdk_device_type_e
{
	SCDK_DEVICE_TYPE_NONE = 0,
	SCDK_DEVICE_TYPE_ORIGINAL = 0x0060,
	SCDK_DEVICE_TYPE_ORIGINAL_MK2 = 0x006d,
	SCDK_DEVICE_TYPE_MK2 = 0x0080,
	SCDK_DEVICE_TYPE_MINI = 0x0063,
	SCDK_DEVICE_TYPE_MINI_MK2 = 0x0090,
	SCDK_DEVICE_TYPE_XL = 0x006c,
	SCDK_DEVICE_TYPE_XL_MK2 = 0x008f
} scdk_device_type_e;

typedef struct scdk_device_info_t
{
	wchar_t* serial_number;
	scdk_device_type_e device_type;

	struct scdk_device_info_t* next;

} scdk_device_info_t;

typedef struct scdk_device_type_info_t
{
	scdk_device_type_e device_type;
	int columns;
	int rows;
	int key_image_width;
	int key_image_height;
	int key_gap_width;
	int key_gap_height;
	int image_width;
	int image_height;

} scdk_device_type_info_t;

typedef void* scdk_device_t;

typedef enum scdk_pixel_format_e
{
	SCDK_PIXEL_FORMAT_RGB = 0,
	SCDK_PIXEL_FORMAT_BGR = 1,
	SCDK_PIXEL_FORMAT_RGBX = 2,
	SCDK_PIXEL_FORMAT_BGRX = 3,
	SCDK_PIXEL_FORMAT_XBGR = 4,
	SCDK_PIXEL_FORMAT_XRGB = 5,
	SCDK_PIXEL_FORMAT_RGBA = 6,
	SCDK_PIXEL_FORMAT_BGRA = 7,
	SCDK_PIXEL_FORMAT_ABGR = 8,
	SCDK_PIXEL_FORMAT_ARGB = 9,

} scdk_pixel_format_e;

DLL_API const scdk_device_type_info_t* scdk_get_device_type_info_from_type(scdk_device_type_e device_type);

DLL_API scdk_device_info_t* scdk_enumerate(void);

DLL_API void scdk_free_enumeration(scdk_device_info_t* devices);

DLL_API bool scdk_open(scdk_device_t* p_device, scdk_device_type_e device_type, const wchar_t* serial_number);

DLL_API bool scdk_open_first(scdk_device_t* p_device, scdk_device_type_e device_type);

DLL_API void scdk_free(scdk_device_t device);

DLL_API const scdk_device_type_info_t* scdk_get_device_type_info(scdk_device_t device);

DLL_API bool scdk_get_serial_number(scdk_device_t device, wchar_t* serial_number_buffer, size_t serial_number_buffer_length);

DLL_API int scdk_read_key(scdk_device_t device, bool* key_state_buffer, size_t key_state_buffer_length);

DLL_API int scdk_read_key_timeout(scdk_device_t device, bool* key_state_buffer, size_t key_state_buffer_length, int timeout_ms);

DLL_API bool scdk_set_image(scdk_device_t device, const unsigned char* image_buffer, 
	scdk_pixel_format_e pixel_format, int quality_percentage);

DLL_API bool scdk_set_image_24(scdk_device_t device, const unsigned char* image_buffer, 
	scdk_pixel_format_e pixel_format, int quality_percentage);

DLL_API bool scdk_set_image_32(scdk_device_t device, const unsigned char* image_buffer, 
	scdk_pixel_format_e pixel_format, int quality_percentage);

DLL_API bool scdk_set_key_image(scdk_device_t device, int key_x, int key_y,
	const unsigned char* image_buffer, scdk_pixel_format_e pixel_format, int quality_percentage);

DLL_API bool scdk_set_brightness(scdk_device_t device, int brightness_percentage);

DLL_API bool scdk_set_screensaver(scdk_device_t device);

#endif // SCREAMDECK_H
