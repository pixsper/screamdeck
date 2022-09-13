#ifndef SCREAMDECK_H
#define SCREAMDECK_H

#include <stdbool.h>
#include <wchar.h>

#ifdef screamdeck_EXPORTS
#ifdef _MSC_VER
#define DLL_API __declspec(dllexport)
#elif defined __GNUC__
#define DLL_API __attribute__ ((visibility("default")))
#endif
#else
#ifdef _MSC_VER
#define DLL_API __declspec(dllimport)
#elif defined __GNUC__
#define DLL_API __attribute__ ((visibility ("hidden")))
#endif
#endif

#define SCDK_IMAGE_WIDTH 1034
#define SCDK_IMAGE_HEIGHT 498

#define SCDK_KEY_IMAGE_HEIGHT 96
#define SCDK_KEY_IMAGE_WIDTH 96
#define SCDK_KEY_GAP_WIDTH 38

#define SCDK_KEY_GRID_WIDTH 8
#define SCDK_KEY_GRID_HEIGHT 4


typedef struct scdk_device_info_t
{
	wchar_t* serial_number;

	struct scdk_device_info_t* next;

} scdk_device_info_t;

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


DLL_API scdk_device_info_t* scdk_enumerate(void);

DLL_API void scdk_free_enumeration(scdk_device_info_t* devices);

DLL_API bool scdk_open(scdk_device_t* p_device, const wchar_t* serial_number);

DLL_API void scdk_free(scdk_device_t device);

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
