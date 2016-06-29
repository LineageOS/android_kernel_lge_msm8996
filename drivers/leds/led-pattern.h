#ifndef __LED_PATTERN_H_INCLUDED
#define __LED_PATTERN_H_INCLUDED

struct led_pattern_ops
{
	ssize_t	(*select)(const char *string_format, size_t string_size);
	ssize_t	(*input)(const char *string_format, size_t string_size);
	ssize_t	(*blink)(const char *string_format, size_t string_size);
	ssize_t	(*onoff)(const char *string_format, size_t string_size);
	ssize_t	(*scale)(const char *string_format, size_t string_size);
};

extern void led_pattern_register(struct led_pattern_ops *led_platform_operations);

#endif	/* __LED_PATTERN_H_INCLUDED */
