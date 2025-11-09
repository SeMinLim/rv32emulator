#include "cachesim.h"
#include <stdlib.h>

uint32_t g_cache[CACHE_SETS][CACHE_WAYS][CACHE_LINE_WORD] = {0};
uint32_t g_tags[CACHE_SETS][CACHE_WAYS] = {0};
uint8_t g_flags[CACHE_SETS][CACHE_WAYS] = {0};

#define FLAG_VALID (1u<<0)
#define FLAG_DIRTY (1u<<1)


uint32_t cache_calc_idx(uint32_t addr) {
		return ((addr>>(CACHE_LINE_WORD_SZ+2)) & ((1<<CACHE_SETS_SZ)-1));
}
uint32_t cache_calc_tag(uint32_t addr) {
		return (addr >> (CACHE_SETS_SZ+CACHE_LINE_WORD_SZ+2));
}
uint32_t cache_calc_word_idx(uint32_t addr) {
		return ((addr>>2)&((1<<CACHE_LINE_WORD_SZ)-1));
}
uint32_t cache_calc_byte_idx(uint32_t addr) {
		return (addr&0x3);
}
uint32_t cache_reassemble_addr(uint32_t idx, uint32_t tag) {
		return (idx<<(CACHE_LINE_WORD_SZ+2)) | (tag<<(CACHE_SETS_SZ+CACHE_LINE_WORD_SZ+2));
}

bool is_flag_valid(uint8_t f)     { return (f & FLAG_VALID) != 0; }
bool is_flag_dirty(uint8_t f)     { return (f & FLAG_DIRTY) != 0; }
uint8_t set_flag_valid(uint8_t f) { return f | FLAG_VALID; }
uint8_t set_flag_dirty(uint8_t f) { return f | FLAG_DIRTY; }
uint8_t clr_flag_dirty(uint8_t f) { return (uint8_t)(f & ~FLAG_DIRTY); }
uint8_t set_flag_invalid(uint8_t f){ return (uint8_t)(f & ~(FLAG_VALID | FLAG_DIRTY)); }

int cache_peek(uint32_t addr, int bytes) {
	uint32_t idx = cache_calc_idx(addr);
	uint32_t tag = cache_calc_tag(addr);

	if ( cache_calc_idx(addr) != cache_calc_idx(addr+bytes-1) ) {
		printf( "ERROR: request spans line boundary\n" );
	}

	for ( int i = 0; i < CACHE_WAYS; i++ ) {
		if ( g_tags[idx][i] == tag && is_flag_valid(g_flags[idx][i]) ) return i;
	}

	return -1;
}
void cache_write(uint32_t addr, uint32_t data, int bytes) {
	uint32_t idx = cache_calc_idx(addr);
	uint32_t tag = cache_calc_tag(addr);
	uint32_t wid = cache_calc_word_idx(addr);
	int boff = (addr&(0x3));
	int way = cache_peek(addr,bytes);
	if ( way < 0 ) {
		return;
	}

	switch ( bytes ) {
		case 1: {
			uint8_t* cl = (((uint8_t*)&(g_cache[idx][way][wid]))+boff);
			*cl = (data&(0xff));
			break;
		}
		case 2: {
			uint8_t* cl = (((uint8_t*)&(g_cache[idx][way][wid]))+boff);
			*(uint16_t*)cl = (data&(0xffff));
			break;
		}
		case 4: {
			g_cache[idx][way][wid] = data;
			break;
		}
	}
	g_flags[idx][way] = set_flag_dirty(g_flags[idx][way]);
}
uint32_t cache_read(uint32_t addr, int bytes) {
	uint32_t idx = cache_calc_idx(addr);
	uint32_t tag = cache_calc_tag(addr);
	uint32_t wid = cache_calc_word_idx(addr);
	int boff = (addr&(0x3));
	int way = cache_peek(addr,bytes);
	if ( way < 0 ) {
		return 0xffffffff;
	}

	uint32_t ret = 0xffffffff;
	switch ( bytes ) {
		case 1: {
			uint8_t* cl = (((uint8_t*)&(g_cache[idx][way][wid]))+boff);
			ret = *cl;
			break;
		}
		case 2: {
			uint8_t* cl = (((uint8_t*)&(g_cache[idx][way][wid]))+boff);
			ret = *(uint16_t*)cl;
			break;
		}
		case 4: {
			ret = g_cache[idx][way][wid];
			break;
		}
	}

	return ret;
}

void cache_update(uint32_t addr, uint32_t data) {
	uint32_t idx = cache_calc_idx(addr);
	uint32_t tag = cache_calc_tag(addr);
	uint32_t wid = cache_calc_word_idx(addr);
	int way = cache_peek(addr,4);
	if ( way < 0 ) {
		for ( int i = 0; i < CACHE_WAYS; i++ ) {
			if ( !is_flag_valid(g_flags[idx][i]) ) {
				way = i;
				break;
			}
		}
	}

	g_cache[idx][way][wid] = data;
	g_tags[idx][way] = tag;
	g_flags[idx][way] = set_flag_valid(g_flags[idx][way]);
	g_flags[idx][way] = clr_flag_dirty(set_flag_valid(g_flags[idx][way]));
}

int cache_flush(uint32_t addr, uint8_t* mem) {
	uint32_t idx = cache_calc_idx(addr);
	int way = 0;

	if (!is_flag_valid(g_flags[idx][way])) return 0;

	int flushed = 0;
	if (is_flag_dirty(g_flags[idx][way])) {
		uint32_t tag = g_tags[idx][way];
		for (int i = 0; i < CACHE_LINE_WORD; i++) {
			uint32_t data  = g_cache[idx][way][i];
			uint32_t maddr = cache_reassemble_addr(idx, tag) + (i * 4);
			*(uint32_t*)(mem + maddr) = data;
		}
		g_flags[idx][way] = clr_flag_dirty(g_flags[idx][way]);
		flushed = CACHE_LINE_WORD;
	}

	g_flags[idx][way] = set_flag_invalid(g_flags[idx][way]);
	return flushed;
}
