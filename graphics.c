// --- Includes ---

#include "types.h"
#include "defs.h"
#include "memlayout.h"
#include "x86.h"
#include <stddef.h>

// --- Definitions ---

// One-line functions for maths
#define min(a, b) 	((a) != (b) ? ((a) < (b) ? (a) : (b)) : (a))
#define max(a, b) 	((a) != (b) ? ((a) > (b) ? (a) : (b)) : (a))
#define clamp(x, upper, lower) (min((upper), max((x), (lower))))
#define abs(x) 		((x) < 0 ? -(x) : (x))

// Limits
#define MAX_DCS 	10
#define MAX_BATCH	200

// Batch case ids
#define NONE		0
#define MOVETO		1
#define LINETO		2
#define SETPEN		3
#define SELECTPEN	4
#define	FILLRECT	5


// --- Structs ---

struct rect {
	int top;		// y top
	int left;		// x left
	int bottom;		// y bottom
	int right;		// x right
};

struct graphicsOperation {
	int operationType; // Defined operation type
	int args[4];	// Operation parameters
};

struct graphicsBatch { // Storage of graphics batch
	int operationAmount; // Number of operations
	struct graphicsOperation operations[MAX_BATCH]; // Storage of the graphics operations
};

struct dc {
	int hdc;		// Handle to device context
	int currentX;	// Variables per hdc
	int currentY;
	int penIndex;
	struct graphicsBatch batch; // Instance of a graphics batch
};

struct dc dcs[MAX_DCS];	// Storage of device contexts

struct graphicsContext { // Storage of the current device context
	struct dc* currentDC; // Pointer to the current device context
};

struct graphicsContext globalContext; // Instance of the graphics context


// --- Functions ---

void clear320x200x256() {
	// Create a buffer for 320x200px
	size_t videoBuffer = 320 * 200 * sizeof(ushort);
	// Convert physical address 0xA0000 to virtual memory
    ushort* addr = (ushort*)P2V(0xA0000);
	// Store 0 (Black) to value at address with buffer
    stosb(addr, 0, videoBuffer);
	// Initialise each instance of device context
	size_t dcsSize = sizeof(dcs) / sizeof(dcs[0]);
	for (size_t i = 0; i < dcsSize; i++) {
		dcs[i].hdc = -1;
		dcs[i].currentX = 0;
		dcs[i].currentY = 0;
		dcs[i].penIndex = 15;
		dcs[i].batch.operationAmount = 0;
	}
}

int setpixel(int hdc, int x, int y) {
	// Convert physical address of given pixel to virtual memory
	ushort* addr = (ushort*)P2V(0xA0000 + (320 * y) + x);
	// Store colour index value at address
	stosb(addr, dcs[hdc].penIndex, 1); // 1 for buffer as vertical lines are 1 pixel thick
	return 0;
}

int moveto(int hdc, int x, int y) {
	// Clamp x between 0 and 320 pixels
	dcs[hdc].currentX = clamp(x, 320, 0);
	// Clamp y between 0 and 200 pixels
	dcs[hdc].currentY = clamp(y, 200, 0);
	return 0;
}

int lineto(int hdc, int newX, int newY) {
	// Setting scope variables for iterations
	int x = dcs[hdc].currentX;
	int y = dcs[hdc].currentY;

	// Distance for x
	int dx = abs(newX - x);
	// Increment value for x
	int sx = (x < newX) ? 1 : -1;
	// Distance for y
	int dy = -abs(newY - y);
	// Increment value for y
	int sy = (y < newY) ? 1 : -1;
	// Error parameter used for deciding which x/y to increment
	int err = dx + dy;
	
	// Bresenham line drawing algorithm
	while (x != newX || y != newY) {
		// Clamped due to pixel wrap-around
		setpixel(hdc, clamp(x, 319, 0), clamp(y, 199, 0));
		int err2 = 2 * err;
		if (err2 >= dy) {
			err += dy;
			x += sx;
		}
		if (err2 <= dx) {
			err += dx;
			y += sy;
		}
	}

	// Store ending coords as current coords
	dcs[hdc].currentX = newX;
	dcs[hdc].currentY = newY;

	return 0;
}

int setpencolour(int index, int r, int g, int b) {
	// Index out of range
	if (index < 16 || index > 255) { return -1; }

	// Clamp RGB between 0 and 63
	r = clamp(r, 63, 0);
	g = clamp(g, 63, 0);
	b = clamp(b, 63, 0);

	// Output values to their corresponding ports
	outb(0x3C8, index);
	outb(0x3C9, r);
	outb(0x3C9, g);
	outb(0x3C9, b);

	return 0;
}

int selectpen(int hdc, int index) {
	// Index out of range
	if (index < 0 || index > 255) { return -1; }
	// Set the current pen
	dcs[hdc].penIndex = index;
	return 0;
}

int fillrect(int hdc, struct rect* rect) {
	// Invalid rect pointer
	if (rect == NULL) { return -1; }

	// Get virtual address of the starting point of the rect
    ushort* addr = (ushort*)P2V(0xA0000 + 320 * rect->top + rect->left);
	// Iterate through each row
	for (int y = rect->top; y <= rect->bottom; y++) {
		// Set the row colour to be the current pen's colour
		stosb(addr, dcs[hdc].penIndex, rect->right - rect->left + 1);
		// Increment address by 160 (half the x-size of the screen)
		addr += 160;
	}

	return 0;
}

int addBatchOperation(int operationType, int args[], int argsSize){
	// Gets the current device context to add operations to its batch
	struct dc* currentDC = globalContext.currentDC;

	// Pointer to existing device context?
	if (currentDC != NULL) {
		// Get the current device context's batch
		struct graphicsBatch* batch = &currentDC->batch;

		// Cap number of operations to MAX_BATCH
		if (batch->operationAmount < MAX_BATCH) {
			// Set the operation type and hdc to the batch's operation array
			batch->operations[batch->operationAmount].operationType = operationType;
		}

		// Assign arguments passed to the function as parameters for batch execution
		for (int i = 0; i < argsSize; i++) {
			batch->operations[batch->operationAmount].args[i] = args[i];
		}

		// Increment the batch's operation amount
		batch->operationAmount++;

		return 0;
	}

	// Pointer does not point to existing device context
	return -1;
}

void executeBatch(int hdc) {
	// Get a pointer to the current device context based on hdc
	struct dc* currentDC = &dcs[hdc];

	// Pointer to existing device context?
	if (currentDC != NULL) {
		// Get the current device context's batch
		struct graphicsBatch* batch = &currentDC->batch;
		
		// Iterate over the operations amount of the current batch
		for (int i = 0; i < batch->operationAmount; i++) {
			// Get the operation of the batch at index i
			struct graphicsOperation* operation = &batch->operations[i];
			// Default to OK status for now due to no default switch case
			int ret = 0;

			// Switch case over the current operation's type
			switch (operation->operationType) {
				// Acts as "null-terminator" of the array in the event of overlap of resused hdcs
				case NONE:
					ret = -1;
					break;

				case MOVETO:
					ret = moveto(hdc, operation->args[0], operation->args[1]);
					break;

				case LINETO:
					ret = lineto(hdc, operation->args[0], operation->args[1]);
					break;

				case SETPEN:
					ret = setpencolour(operation->args[0], operation->args[1], operation->args[2], operation->args[3]);
					break;

				case SELECTPEN:
					ret = selectpen(hdc, operation->args[0]);
					break;

				case FILLRECT:
					// Reconstruct struct rect from the arguments
					struct rect r;
					r.top = operation->args[0];
					r.left = operation->args[1];
					r.bottom = operation->args[2];
					r.right = operation->args[3];
					ret = fillrect(hdc, &r);
					break;
			}

			// Incase a function returns -1 indicating error, stop procesing current batch
			if (ret == -1) {
				return;
			}
		}
	}
}

int beginpaint(int hwnd) {
	// Incase of unavailable hdc
	int hdc = -1;
	size_t dcsSize = sizeof(dcs) / sizeof(dcs[0]);
	// Find available hdc
	for (size_t i = 0; i < dcsSize; i++) {
		if (dcs[i].hdc == -1) {
			dcs[i].hdc = i;
			hdc = i;
			break;
		}
	}

	// Max device contexts reached
	if (hdc == -1) {
		return -1;
	}

	// Sets the current device context to store the current batch
	globalContext.currentDC = &dcs[hdc];

	return hdc;
}

int endpaint(int hdc) {
	// Compare bounds of hdc to dcs bounds
	if (hdc >= 0 && hdc < MAX_DCS && dcs[hdc].hdc == hdc) {
		// Run the batch operations
		executeBatch(hdc);

		// Reset device context of hdc
		dcs[hdc].hdc = -1;
		dcs[hdc].currentX = 0;
		dcs[hdc].currentY = 0;
		dcs[hdc].penIndex = 15;

		// Clear the device context's batch (operationType, hdc, and args) up to operationAmount
		for (int i = 0; i < dcs[hdc].batch.operationAmount; i++) {
			dcs[hdc].batch.operations->operationType = 0;
			
			for (int j = 0; j < 4; j++) {
				dcs[hdc].batch.operations->args[j] = 0;
			}
		}

		// Reset the batch's operation amount
		dcs[hdc].batch.operationAmount = 0;

		return 0;
	} else {
		return -1; // Invalid hdc
	}
}


// --- SYS calls ---

int sys_moveto(void) {
	// Initialise parameters
	int hdc, x, y;

	// Check if parameters have values
	if (argint(0, &hdc) < 0 || argint(1, &x) < 0 || argint(2, &y) < 0) {
		return -1;
	}

	// Package up parameters into args array
	int args[] = {x, y};

	// Add operation to batch
	return addBatchOperation(1, args, sizeof(args)/sizeof(args[0]));
}

int sys_lineto(void) {
	int hdc, x, y;

	if (argint(0, &hdc) < 0 || argint(1, &x) < 0 || argint(2, &y) < 0) {
		return -1;
	}

	int args[] = {x, y};

	return addBatchOperation(2, args, sizeof(args)/sizeof(args[0]));
}

int sys_setpencolour(void) {
	int index, r, g, b;

	if (argint(0, &index) < 0 || argint(1, &r) < 0 || argint(2, &g) < 0 || argint(3, &b) < 0) {
		return -1;
	}

	int args[] = {index, r, g, b};

	return addBatchOperation(3, args, sizeof(args)/sizeof(args[0]));
}

int sys_selectpen(void) {
	int hdc, index;

	if (argint(0, &hdc) < 0 || argint(1, &index) < 0) {
		return -1;
	}

	int args[] = {index};

	return addBatchOperation(4, args, sizeof(args)/sizeof(args[0]));
}

int sys_fillrect(void) {
	int hdc;
	struct rect *rect;

	if (argint(0, &hdc) < 0 || argptr(1, (void*)&rect, sizeof(*rect)) < 0) {
		return -1;
	}

	// Construct args array from struct rect due to type mis-match
	int args[4];
	args[0] = rect->top;
	args[1] = rect->left;
	args[2] = rect->bottom;
	args[3] = rect->right;

	return addBatchOperation(5, args, sizeof(args)/sizeof(args[0]));
}

int sys_beginpaint(void) {
	int hwnd;

	if (argint(0, &hwnd) < 0) {
		return -1;
	}

	return beginpaint(hwnd);
}

int sys_endpaint(void) {
	int hdc;

	if (argint(0, &hdc) < 0) {
		return -1;
	}

	return endpaint(hdc);
}