/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016-2018 NXP
 * Copyright (c) 2018-2019, Vladimir Oltean <olteanv@gmail.com>
 */
#ifndef _LINUX_PACKING_H
#define _LINUX_PACKING_H

#include <linux/array_size.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/types.h>

#define GEN_PACKED_FIELD_STRUCT(__type)		\
	struct packed_field_ ## __type {	\
		__type startbit;		\
		__type endbit;			\
		__type offset;			\
		__type size;			\
	}

/* struct packed_field_u8. Use with bit offsets < 256, buffers < 32B and
 * unpacked structures < 256B.
 */
GEN_PACKED_FIELD_STRUCT(u8);

/* struct packed_field_u16. Use with bit offsets < 65536, buffers < 8KB and
 * unpacked structures < 64KB.
 */
GEN_PACKED_FIELD_STRUCT(u16);

#define PACKED_FIELD(start, end, struct_name, struct_field)	\
{								\
	(start),							\
	(end),							\
	offsetof(struct_name, struct_field),			\
	sizeof_field(struct_name, struct_field),		\
}

#define QUIRK_MSB_ON_THE_RIGHT	BIT(0)
#define QUIRK_LITTLE_ENDIAN	BIT(1)
#define QUIRK_LSW32_IS_FIRST	BIT(2)

enum packing_op {
	PACK,
	UNPACK,
};

/**
 * packing - Convert numbers (currently u64) between a packed and an unpacked
 *	     format. Unpacked means laid out in memory in the CPU's native
 *	     understanding of integers, while packed means anything else that
 *	     requires translation.
 *
 * @pbuf: Pointer to a buffer holding the packed value.
 * @uval: Pointer to an u64 holding the unpacked value.
 * @startbit: The index (in logical notation, compensated for quirks) where
 *	      the packed value starts within pbuf. Must be larger than, or
 *	      equal to, endbit.
 * @endbit: The index (in logical notation, compensated for quirks) where
 *	    the packed value ends within pbuf. Must be smaller than, or equal
 *	    to, startbit.
 * @op: If PACK, then uval will be treated as const pointer and copied (packed)
 *	into pbuf, between startbit and endbit.
 *	If UNPACK, then pbuf will be treated as const pointer and the logical
 *	value between startbit and endbit will be copied (unpacked) to uval.
 * @quirks: A bit mask of QUIRK_LITTLE_ENDIAN, QUIRK_LSW32_IS_FIRST and
 *	    QUIRK_MSB_ON_THE_RIGHT.
 *
 * Return: 0 on success, EINVAL or ERANGE if called incorrectly. Assuming
 *	   correct usage, return code may be discarded.
 *	   If op is PACK, pbuf is modified.
 *	   If op is UNPACK, uval is modified.
 */
int packing(void *pbuf, u64 *uval, int startbit, int endbit, size_t pbuflen,
	    enum packing_op op, u8 quirks);

static inline u64 packed_field_get_u64(const void *ustruct,
				       size_t field_offset,
				       size_t field_size)
{
	const u8 *field = (const u8 *)ustruct + field_offset;
	u64 val64;
	u32 val32;
	u16 val16;

	switch (field_size) {
	case 1:
		return *field;
	case 2:
		memcpy(&val16, field, sizeof(val16));
		return val16;
	case 4:
		memcpy(&val32, field, sizeof(val32));
		return val32;
	case 8:
		memcpy(&val64, field, sizeof(val64));
		return val64;
	default:
		WARN_ON_ONCE(1);
		return 0;
	}
}

static inline void packed_field_set_u64(void *ustruct, size_t field_offset,
					size_t field_size, u64 uval)
{
	u8 *field = (u8 *)ustruct + field_offset;
	u32 val32;
	u16 val16;
	u8 val8;

	switch (field_size) {
	case 1:
		val8 = uval;
		memcpy(field, &val8, sizeof(val8));
		break;
	case 2:
		val16 = uval;
		memcpy(field, &val16, sizeof(val16));
		break;
	case 4:
		val32 = uval;
		memcpy(field, &val32, sizeof(val32));
		break;
	case 8:
		memcpy(field, &uval, sizeof(uval));
		break;
	default:
		WARN_ON_ONCE(1);
		break;
	}
}

static inline u64 packed_field_mask(size_t startbit, size_t endbit)
{
	size_t width = startbit - endbit + 1;

	if (width >= 64)
		return ~0ULL;

	return GENMASK_ULL(width - 1, 0);
}

#define DEFINE_PACKED_FIELD_ACCESSORS(__type)				\
static inline void pack_fields_ ## __type(void *pbuf, size_t pbuflen, \
					  const void *ustruct,		\
					  const struct packed_field_ ## __type *fields, \
					  size_t num_fields, u8 quirks)	\
{									\
	size_t i;							\
									\
	for (i = 0; i < num_fields; i++) {				\
		const struct packed_field_ ## __type *field = &fields[i]; \
		u64 uval;						\
		u64 mask;						\
									\
		uval = packed_field_get_u64(ustruct, field->offset,	\
					    field->size);		\
		mask = packed_field_mask(field->startbit, field->endbit); \
		WARN_ON_ONCE(uval & ~mask);				\
		uval &= mask;						\
		WARN_ON_ONCE(packing(pbuf, &uval, field->startbit,	\
				     field->endbit, pbuflen, PACK,	\
				     quirks));				\
	}								\
}									\
									\
static inline void unpack_fields_ ## __type(const void *pbuf,		\
					    size_t pbuflen, void *ustruct, \
					    const struct packed_field_ ## __type *fields, \
					    size_t num_fields, u8 quirks) \
{									\
	size_t i;							\
									\
	for (i = 0; i < num_fields; i++) {				\
		const struct packed_field_ ## __type *field = &fields[i]; \
		u64 uval;						\
									\
		if (WARN_ON_ONCE(packing((void *)pbuf, &uval,		\
					 field->startbit, field->endbit,	\
					 pbuflen, UNPACK, quirks)))	\
			continue;					\
									\
		packed_field_set_u64(ustruct, field->offset,		\
				     field->size, uval);		\
	}								\
}

DEFINE_PACKED_FIELD_ACCESSORS(u8)
DEFINE_PACKED_FIELD_ACCESSORS(u16)

#define pack_fields(pbuf, pbuflen, ustruct, fields, quirks)		\
	_Generic((fields),						\
		 const struct packed_field_u8 * : pack_fields_u8,	\
		 const struct packed_field_u16 * : pack_fields_u16	\
		)((pbuf), (pbuflen), (ustruct), (fields),		\
		  ARRAY_SIZE(fields), (quirks))

#define unpack_fields(pbuf, pbuflen, ustruct, fields, quirks)		\
	_Generic((fields),						\
		 const struct packed_field_u8 * : unpack_fields_u8,	\
		 const struct packed_field_u16 * : unpack_fields_u16	\
		)((pbuf), (pbuflen), (ustruct), (fields),		\
		  ARRAY_SIZE(fields), (quirks))

#endif
