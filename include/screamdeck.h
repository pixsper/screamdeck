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


typedef struct scdk_device_info_t
{
	wchar_t* serial_number;

	struct scdk_device_info_t* next;

} scdk_device_info_t;

typedef void* scdk_device_t;

typedef enum scdk_pixel_format_e
{
	SCDK_PIXEL_FORMAT_RGB = 0,
	SCDK_PIXEL_FORMAT_BGR,
	SCDK_PIXEL_FORMAT_RGBX,
	SCDK_PIXEL_FORMAT_BGRX,
	SCDK_PIXEL_FORMAT_XBGR,
	SCDK_PIXEL_FORMAT_XRGB,
	SCDK_PIXEL_FORMAT_RGBA,
	SCDK_PIXEL_FORMAT_BGRA,
	SCDK_PIXEL_FORMAT_ABGR,
	SCDK_PIXEL_FORMAT_ARGB,

} scdk_pixel_format_e;


DLL_API scdk_device_info_t* scdk_enumerate(void);

DLL_API void scdk_free_enumeration(scdk_device_info_t* devices);

DLL_API bool scdk_open(scdk_device_t* p_device, const wchar_t* serial_number);

DLL_API void scdk_free(scdk_device_t device);

DLL_API bool scdk_set_image(scdk_device_t device, const unsigned char* image_buffer, scdk_pixel_format_e pixel_format);

DLL_API bool scdk_set_key_image(scdk_device_t device, int key_x, int key_y,
	const unsigned char* image_buffer, scdk_pixel_format_e pixel_format);

#endif // SCREAMDECK_H
