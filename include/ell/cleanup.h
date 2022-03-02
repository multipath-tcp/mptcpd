#pragma once

#define DEFINE_CLEANUP_FUNC(func)			\
	inline __attribute__((always_inline))		\
	void func ## _cleanup(struct l_settings *p) { func(*(struct l_settings **) p); }
