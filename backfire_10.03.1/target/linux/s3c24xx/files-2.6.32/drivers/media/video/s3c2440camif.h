#ifndef __S3C2440CAMIF_H__
#define __S3C2440CAMIF_H__

#define MIN_C_WIDTH		32
#define MAX_C_WIDTH		1280
#define MIN_C_HEIGHT	48
#define MAX_C_HEIGHT	1024

#define MIN_P_WIDTH		32
#define MAX_P_WIDTH		640
#define MIN_P_HEIGHT	48
#define MAX_P_HEIGHT	480

enum
{
	CAMIF_BUFF_INVALID = 0,
	CAMIF_BUFF_RGB565 = 1,
	CAMIF_BUFF_RGB24 = 2,
	CAMIF_BUFF_YCbCr420 = 3,
	CAMIF_BUFF_YCbCr422 = 4
};

/* image buffer for s3c2440 camif. */
struct s3c2440camif_buffer
{
	int state;
	ssize_t img_size;

	unsigned int order;
	unsigned long virt_base;
	unsigned long phy_base;
};

/* for s3c2440camif_dev->state field. */
enum
{
	CAMIF_STATE_FREE = 0,		// not openned
	CAMIF_STATE_READY = 1,		// openned, but standby
	CAMIF_STATE_PREVIEWING = 2,	// in previewing
	CAMIF_STATE_CODECING = 3	// in capturing
};

/* for s3c2440camif_dev->cmdcode field. */
enum
{
	CAMIF_CMD_NONE	= 0,
	CAMIF_CMD_SFMT	= 1<<0,		// source image format changed.
	CAMIF_CMD_WND	= 1<<1,		// window offset changed.
	CAMIF_CMD_ZOOM	= 1<<2,		// zoom picture in/out
	CAMIF_CMD_TFMT	= 1<<3,		// target image format changed.
	CAMIF_CMD_P2C	= 1<<4,		// need camif switches from p-path to c-path
	CAMIF_CMD_C2P	= 1<<5,		// neet camif switches from c-path to p-path

	CAMIF_CMD_STOP	= 1<<16		// stop capture
};

/* main s3c2440 camif structure. */
struct s3c2440camif_dev
{
	/* for sub-devices */
	struct list_head devlist;

	/* minor device */
	struct video_device * vfd;

	/* hardware clock. */
	struct clk * clk;

	/* reference count. */
	struct mutex rcmutex;
	int rc;

	/* the input images's format select. */
	int input;

	/* source(input) image size. */
	int srcHsize;
	int srcVsize;

	/* windowed image size. */
	int wndHsize;
	int wndVsize;

	/* codec-path target(output) image size. */
	int coTargetHsize;
	int coTargetVsize;

	/* preview-path target(preview) image size. */
	int preTargetHsize;
	int preTargetVsize;

	/* the camera interface state. */
	int state;	// CMAIF_STATE_FREE, CAMIF_STATE_PREVIEWING, CAMIF_STATE_CAPTURING.

	/* for executing camif commands. */
	int cmdcode;				// command code, CAMIF_CMD_START, CAMIF_CMD_CFG, etc.
	wait_queue_head_t cmdqueue;	// wait queue for waiting untile command completed (if in preview or in capturing).
};

/* opened file handle.*/
struct s3c2440camif_fh
{
	/* the camif */
	struct s3c2440camif_dev	* dev;

	/* master flag, only master openner could execute 'set' ioctls. */
	int master;
};

#define S3C244X_CAMIFREG(x) ((x) + camif_base_addr)

/* CAMIF control registers */
#define S3C244X_CISRCFMT		S3C244X_CAMIFREG(0x00)
#define S3C244X_CIWDOFST		S3C244X_CAMIFREG(0x04)
#define S3C244X_CIGCTRL			S3C244X_CAMIFREG(0x08)
#define S3C244X_CICOYSA1		S3C244X_CAMIFREG(0x18)
#define S3C244X_CICOYSA2		S3C244X_CAMIFREG(0x1C)
#define S3C244X_CICOYSA3		S3C244X_CAMIFREG(0x20)
#define S3C244X_CICOYSA4		S3C244X_CAMIFREG(0x24)
#define S3C244X_CICOCBSA1		S3C244X_CAMIFREG(0x28)
#define S3C244X_CICOCBSA2		S3C244X_CAMIFREG(0x2C)
#define S3C244X_CICOCBSA3		S3C244X_CAMIFREG(0x30)
#define S3C244X_CICOCBSA4		S3C244X_CAMIFREG(0x34)
#define S3C244X_CICOCRSA1		S3C244X_CAMIFREG(0x38)
#define S3C244X_CICOCRSA2		S3C244X_CAMIFREG(0x3C)
#define S3C244X_CICOCRSA3		S3C244X_CAMIFREG(0x40)
#define S3C244X_CICOCRSA4		S3C244X_CAMIFREG(0x44)
#define S3C244X_CICOTRGFMT		S3C244X_CAMIFREG(0x48)
#define S3C244X_CICOCTRL		S3C244X_CAMIFREG(0x4C)
#define S3C244X_CICOSCPRERATIO	S3C244X_CAMIFREG(0x50)
#define S3C244X_CICOSCPREDST	S3C244X_CAMIFREG(0x54)
#define S3C244X_CICOSCCTRL		S3C244X_CAMIFREG(0x58)
#define S3C244X_CICOTAREA		S3C244X_CAMIFREG(0x5C)
#define S3C244X_CICOSTATUS		S3C244X_CAMIFREG(0x64)
#define S3C244X_CIPRCLRSA1		S3C244X_CAMIFREG(0x6C)
#define S3C244X_CIPRCLRSA2		S3C244X_CAMIFREG(0x70)
#define S3C244X_CIPRCLRSA3		S3C244X_CAMIFREG(0x74)
#define S3C244X_CIPRCLRSA4		S3C244X_CAMIFREG(0x78)
#define S3C244X_CIPRTRGFMT		S3C244X_CAMIFREG(0x7C)
#define S3C244X_CIPRCTRL		S3C244X_CAMIFREG(0x80)
#define S3C244X_CIPRSCPRERATIO	S3C244X_CAMIFREG(0x84)
#define S3C244X_CIPRSCPREDST	S3C244X_CAMIFREG(0x88)
#define S3C244X_CIPRSCCTRL		S3C244X_CAMIFREG(0x8C)
#define S3C244X_CIPRTAREA		S3C244X_CAMIFREG(0x90)
#define S3C244X_CIPRSTATUS		S3C244X_CAMIFREG(0x98)
#define S3C244X_CIIMGCPT		S3C244X_CAMIFREG(0xA0)

#endif
