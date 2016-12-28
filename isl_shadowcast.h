#ifndef ISL_INCLUDE_SHADOWCAST_H_
#define ISL_INCLUDE_SHADOWCAST_H_

#include <stddef.h>

#ifdef ISLS_STATIC
	#define ISLS_DEF static
#else
	#define ISLS_DEF extern
#endif

typedef void (*isls_update)( int x, int y, void * );
typedef int (*isls_absorb)( int x, int y, void * );

typedef struct {
	isls_update update;
	isls_absorb absorb;
} isls_properties;

typedef enum {
	ISLS_OK,
	ISLS_ERROR_BAD_ALLOC,
	ISLS_ERROR_BAD_REALLOC,
	ISLS_ERROR_BAD_ARGUMENTS,	
} isls_status;

#if !defined(ISLS_MALLOC)&&!defined(ISLS_REALLOC)&&!defined(ISLS_FREE)
	#include <stdlib.h>
	#define ISLS_MALLOC malloc
	#define ISLS_REALLOC realloc
	#define ISLS_FREE free
#elif !defined(ISLS_MALLOC)||!defined(ISLS_REALLOC)||!defined(ISLS_FREE)
	#error "You must to define ISLS_MALLOC, ISLS_REALLOC, ISLS_FREE to remove stdlib dependency"
#endif

#ifdef __cplusplus
extern "C" {
#endif

ISLS_DEF isls_status isls_shadowcast( int x0, int y0, int power, isls_properties *properties, void *userdata );

#ifdef __cplusplus
}
#endif

#endif // ISL_INCLUDE_SHADOWCAST_H_

#ifdef ISL_SHADOWCAST_IMPLEMENTATION

typedef struct {
	int row;
	int start;
	int finish;
} isls__tuple;

typedef struct {
	isls__tuple *tuples;
	size_t allocated;
	size_t length;
} isls__stack;

// Minimal dynamic stack implementation for tuples storage
static isls_status isls__stack_grow( isls__stack *stack, size_t newalloc ) {
	isls__tuple *newtuples = ISLS_REALLOC( stack->tuples, sizeof( *stack->tuples ) * newalloc );
	if ( newtuples != NULL ) {
		stack->allocated = newalloc;
		stack->tuples = newtuples;
		return ISLS_OK;
	} else {
		return ISLS_ERROR_BAD_REALLOC;
	}
}

static isls_status isls__stack_push( isls__stack *stack, isls__tuple tuple ) {
	size_t index = stack->length;
	isls_status status = ISLS_OK;
	if ( stack->allocated <= stack->length ) {
		status = isls__stack_grow( stack, stack->allocated * 2 );
		if ( status != ISLS_OK ) {
			return status;
		}
	}
	stack->tuples[index] = tuple;
	stack->length++;
	return status;
}
// End of dynamic stack implementation for tuples storage


isls_status isls_shadowcast( int x0, int y0, int power, isls_properties *properties, void *userdata ) {
	int i, minx, miny, maxx, maxy, bounded;
	int *xxyy;
	isls_status status = ISLS_OK;
	const static int DIRECTIONS_8[8][4] = {
		{ 0,-1,-1, 0},
		{-1, 0, 0,-1},
		{ 0, 1,-1, 0},
		{ 1, 0, 0,-1},
		{ 0,-1, 1, 0},
		{-1, 0, 0, 1},
		{ 0, 1, 1, 0},
		{ 1, 0, 0, 1},
	};
	isls__stack stack = {ISLS_MALLOC(sizeof(isls__tuple)),1,0};

	if ( properties == NULL ) {
		return ISLS_ERROR_BAD_ARGUMENTS;
	}

	if ( stack.tuples == NULL ) {
		return ISLS_ERROR_BAD_ALLOC;
	}

	properties->update( x0, y0, userdata );
	for ( i = 0; i < 8; i++ ) {
		int xx = DIRECTIONS_8[i][0];
		int xy = DIRECTIONS_8[i][1];
		int yx = DIRECTIONS_8[i][2];
		int yy = DIRECTIONS_8[i][3];
		isls__tuple tuple = {1, 1, 0};
		stack.length = 0;
		status = isls__stack_push( &stack, tuple );
		if ( status != ISLS_OK ) {
			ISLS_FREE( stack.tuples );
			return status;
		}
		while ( stack.length > 0 ) {
			tuple = stack.tuples[--stack.length];
			int row = tuple.row;
			int start = tuple.start;
			int finish = tuple.finish;
			if ( start >= finish ) {
				int newstart = 0;
				int blocked = 0;
				int dy = -row;
				for ( ; dy > -power; dy-- ) {
					int leftdenom = 2*dy - 1;
					int rightdenom = 2*dy + 1;
					int xydy = xy * dy;
					int yydy = yy * dy;
					int dx = dy;
					int dx2 = 2*dy;
					for ( ; dx <= 0; dx++, dx2 += 2 ) {
						int x = x0 + dx * xx + xydy;
						int y = y0 + yx * dx + yydy;
						int leftslope = (dx2 - 1) / leftdenom;
						int rightslope = (dx2 + 1) / rightdenom;
						if ( start < rightslope ) {
							continue;
						}	else if ( finish > leftslope ) {
							break;
						} else {
							properties->update( x, y, userdata );
							int absorption = properties->absorb( x, y, userdata );
							if ( blocked ) {
								if ( absorption != 0 ) {
									newstart = rightslope;
								} else {
									blocked = 0;
									start = newstart;
								}
							} else if ( absorption != 0 && -dy < power ) {
								blocked = 1;
								tuple.row = -dy + 1;
							 	tuple.start = start;
								tuple.finish = leftslope;
								status = isls__stack_push( &stack, tuple );
								if ( status != ISLS_OK ) {
									ISLS_FREE( stack.tuples );
									return status;
								}
								newstart = rightslope;
							}
						}
					}
				}	
			}	
		}
	}
	ISLS_FREE( stack.tuples );
	return status;
}

#endif // ISL_SHADOWCAST_IMPLEMENTATION
