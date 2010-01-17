/* Stream handling functions.

	Copyright (C) 1993-1998 Sebastiano Vigna 
	Copyright (C) 1999-2010 Todd M. Lewis and Sebastiano Vigna

	This file is part of ne, the nice editor.

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the
	Free Software Foundation; either version 2, or (at your option) any
	later version.
	
	This program is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.
	
	You should have received a copy of the GNU General Public License along
	with this program; see the file COPYING.  If not, write to the Free
	Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
	02111-1307, USA.  */



#include "ne.h"

/* This is the least increment with which a char_stream is reallocated. */

#define CHAR_STREAM_SIZE_INC (2048)


/* Allocates a stream of size bytes. Note that a size of 0 is legal, in which
	case a char_stream structure is allocated, but its stream pointer is left
	NULL. */

char_stream *alloc_char_stream(const int size) {
	char_stream * const cs = calloc(1, sizeof *cs);
	if (cs) {
		if (!size || (cs->stream = calloc(size, sizeof *cs->stream))) {
			cs->size = size;

			assert_char_stream(cs);

			return cs;
		}
		free(cs);
	}
	return NULL;
}

/* Frees a stream. */

void free_char_stream(char_stream * const cs) {
	if (!cs) return;
	assert_char_stream(cs);
	free(cs->stream);
	free(cs);
}


/* Reallocates a stream. If cs is NULL, it is equivalent to
	alloc_char_stream(). Otherwise, the memory pointed by stream is
	realloc()ated to size bytes. If the reallocation is successfull, cs is
	returned, otherwise NULL. */

char_stream *realloc_char_stream(char_stream * const cs, const int size) {

	if (!cs) return alloc_char_stream(size);

	assert_char_stream(cs);

	if (!size) {
		free(cs->stream);
		cs->stream = NULL;
		cs->len = cs->size = 0;
		return cs;
	}

	if (cs->stream = realloc(cs->stream, size * sizeof *cs->stream)) {
		cs->size = size;
		if (cs->len > size) cs->len = size;
		return cs;
	}
	return NULL;
}



/* Concatenates a block of len bytes pointed to by s to a stream. The stream is
	extended if necessary. Returns an error code. */

int add_to_stream(char_stream * const cs, const unsigned char * const s, const int len) {

	if (!s) return OK;

	if (!cs) return ERROR;

	if (cs->size - cs->len < len && !realloc_char_stream(cs, cs->len + len + CHAR_STREAM_SIZE_INC)) return OUT_OF_MEMORY;

	memcpy(cs->stream + cs->len, s, len);
	cs->len += len;

	return OK;
}



/* Inserts a block of len bytes pointed to by s into a stream at offset pos. The
	stream is extended if necessary. Returns an error code. */

int insert_in_stream(char_stream *cs, const char *s, const int pos, const int len ) {
	int tail;

	if (!s || !len ) return OK;

	if (!cs) return ERROR;

	if (pos > cs->len) return ERROR;
		
   tail = cs->len - pos;
   
	if (cs->size - cs->len < len && !realloc_char_stream(cs, cs->len + len + CHAR_STREAM_SIZE_INC)) return OUT_OF_MEMORY;

	if (tail > 0) memmove(cs->stream + pos + len, cs->stream + pos, tail);
	memcpy(cs->stream + pos, s, len);
	cs->len += len;

	return OK;
}



/* Deletes a block of len bytes from stream cs at offset p. The stream size
	does not change. Returns an error code. */

int delete_from_stream(char_stream * const cs, const int pos, int len) {

	if (!len) return OK;

	if (!cs) return ERROR;
	if (len > cs->len) len = cs->len;

	memmove(cs->stream + pos, cs->stream + pos + len, cs->len - (pos + len));
	cs->len -= len;

	return OK;
}



/* Resets a character stream. If cs is NULL, an empty character stream is
	returned. If it is non-NULL, everything inside it is freed. The stream
	memory is deallocated, unless its size is smaller or equal to
	2*CHAR_STREAM_SIZE_INC (so that we won't continously allocate and deallocate
	small streams). */

char_stream *reset_stream(char_stream * const cs) {

	if (!cs) return alloc_char_stream(0);

	assert_char_stream(cs);

	cs->len = 0;

	if (cs->size > 2 * CHAR_STREAM_SIZE_INC) {
		cs->size = 0;
		free(cs->stream);
		cs->stream = NULL;
	}

	return cs;
}


/* Sets the encoding of this stream by guessing it. The source type is used to
	avoid guessing UTF-8 when the source of this clip is ENC_8_BIT. */

void set_stream_encoding(char_stream * const cs, const encoding_type source) {
	cs->encoding = detect_encoding(cs->stream, cs->len);
	if (source == ENC_8_BIT && cs->encoding == ENC_UTF8) cs->encoding = ENC_8_BIT;
}


/* These two functions load a stream in memory. Carriage returns and line feeds
	are converted to NULLs. You can pass NULL for cs, and a char stream will be
	allocated for you. If preserve_cr is TRUE, CRs are preserved. If binary 
   is true, the stream is filled exactly with the file content. */

char_stream *load_stream(char_stream * cs, const char *name, const int preserve_cr, const int binary) {

	int fh;

	assert_char_stream(cs);

	assert(name != NULL);

	name = tilde_expand(name);

	if (is_migrated(name) || is_directory(name)) return NULL;

	cs = load_stream_from_fh(cs, fh = open(name, READ_FLAGS), preserve_cr, binary);
	if (fh >= 0) close(fh);

	return cs;
}

char_stream *load_stream_from_fh(char_stream *cs, const int fh, const int preserve_cr, const int binary) {

	int i, j, len;
	char terminators[] = { 0x0d, 0x0a };

	if (preserve_cr) terminators[0] = 0;

	if (fh < 0) return NULL;

	assert_char_stream(cs);

	len = lseek(fh, 0, SEEK_END);

	if (len < 0) return NULL;

	lseek(fh, 0, SEEK_SET);

	if (!(cs = realloc_char_stream(cs, len))) return NULL;

	if (read(fh, cs->stream, len) < len) {
		free_char_stream(cs);
		return NULL;
	}

	if (binary) {
		cs->len = len;
		assert_char_stream(cs);
		return cs;
	}

	for(i = j = 0; i < len; i++, j++) {
		if (i < len - 1 && !preserve_cr && cs->stream[i] == '\r' && cs->stream[i + 1] == '\n') i++;
		cs->stream[j] = cs->stream[i];
		
		if (cs->stream[j] == terminators[0] || cs->stream[j] == terminators[1]) cs->stream[j] = 0;
	}

	memset(cs->stream + j, 0, len - j);

	cs->len = j;

	assert_char_stream(cs);

	return cs;
}



/* These two functions save a stream to file. NULLs are converted to line
	feeds. If CRLF is TRUE, we save CR/LF pairs as line terminators. If binary 
   is true, the stream is dump literally. We return an error code. */

int save_stream(const char_stream *const cs, const char *name, const int CRLF, const int binary) {

	int fh, error;

	if (!cs) return ERROR;

	assert_char_stream(cs);

	assert(name != NULL);

	name = tilde_expand(name);

	if (is_migrated(name)) return  FILE_IS_MIGRATED ;
	if (is_directory(name)) return  FILE_IS_DIRECTORY ;

	if ((fh = open(name, WRITE_FLAGS, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))>=0) {
		error = save_stream_to_fh(cs, fh, CRLF, binary);
		close(fh);
		return error;
	}

	return CANT_OPEN_FILE;
}


int save_stream_to_fh(const char_stream *const cs, const int fh, const int CRLF, const int binary) {

	int pos = 0, len;

	if (!cs) return ERROR;

	assert_char_stream(cs);

	if (binary) {
		if (write(fh, cs->stream, cs->len) < cs->len) return ERROR_WHILE_WRITING;
		return OK;
	}

	while(pos < cs->len) {
		len = strnlen_ne(cs->stream + pos, cs->len - pos);

		if (write(fh, cs->stream + pos, len) < len) return ERROR_WHILE_WRITING;

		if (pos + len < cs->len) {
			if (CRLF && write(fh, "\r", 1) < 1) return ERROR_WHILE_WRITING;
			if (write(fh, "\n", 1) < 1) return ERROR_WHILE_WRITING;
		}

		pos += len + 1;
	}

	return OK;
}
