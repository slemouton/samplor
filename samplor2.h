#define DEBUG 0
#define DEFAULT_SRATE 44100		/* default sampling rate for initialization */
#define MAX_VOICES 256			/* max # of voices */
#define WAITINGNOTES 10			/* max waiting notes (voice stealing) */
#define DEFAULT_VOICES 12		/* default number of voices */
#define WIND_SIZE 512			/* taille des fenetres */
#define NUM_WINS 7				/* nombre de fenetres */
#define LIST_END ((void*)0)		/* end of list */
#define TEXT 0
#define BINARY 1
#define TEXT 0
#define IGNORE_LOOP 0			/* play all buffer ignoring loop */
#define LOOP 1					/* loop mode for loop_flag */
#define ALLER_RETOUR_LOOP 2		/* forward-backward */
#define IN_ALLER_RETOUR_LOOP 3	/* in forward-backward loop */
#define FINISHING_LOOP 4	/* in forward-backward loop */
#define SAMPLOR_MAX_OUTCOUNT 16 /* maximum number of signal outlets */

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#define DEFSIGFUN(NAME)	void NAME(int n,t_sample *const *in,t_sample *const *out)
#define SIGFUN(FUN) &thisType::FUN
#define SETSIGFUN(VAR,FUN) v_##VAR = FUN
#define DEFSIGCALL(NAME) \
inline void NAME(int n,t_sample *const *in,t_sample *const *out) { (this->*v_##NAME)(n,in,out); } \
void (thisType::*v_##NAME)(int n,t_sample *const *invecs,t_sample *const *outvecs)


std::pair<long,long> get_buffer_loop (const char * buf_name);

typedef double t_samplor_real;	/* samplor calculation has to be in double */
typedef short t_int16; 					///< a 2-byte int  @ingroup misc

typedef struct _samplorbuffer
{
    t_symbol   *b_name;     ///< name of the buffer
    t_int16    *b_samples;  ///< stored with interleaved channels if multi-channel
    t_float    *f_samples;  ///< stored with interleaved channels if multi-channel
    t_float    *f_upsamples; /// oversampling for a better interpolation - use libresample library
    long       b_frames;    ///< number of sample frames (each one is sizeof(float) * b_nchans bytes)
    long       b_nchans;    ///< number of channels
    long       b_size;      ///< size of buffer in floats
    long       b_sr;        ///< sampling rate of the buffer
    long       b_framesize;
    long       b_maxvalue;
    t_float    one_over_b_maxvalue; /// for optimisation !!
} t_samplorbuffer;

typedef struct _samplormmap
{
    t_symbol   *b_name;         /// name of the buffer
    int        fd;              /// file descriptor
    off_t offset;
    off_t pa_offset;            /// page offset
    size_t length;
    long index_start;           /// index du premier echantillon en memoire
    long index_end;             /// index du dernier echantillon en memoire
    char *addr;
    off_t      b_st_size;       /// file size as reported by fstat
    off_t      b_data_offset;   /// offset before the data as reported by libaudiofile
    int        byteOrder;
    long       b_frames;        /// number of sample frames (each one is sizeof(float) * b_nchans bytes)
    long       b_nchans;        /// number of channels
    long       b_size;          /// size of buffer in floats
    long       b_sr;            /// sampling rate of the buffer
    long       b_framesize;
    long       b_maxvalue;
    t_float    one_over_b_maxvalue; /// for optimisation !!
} t_samplormmap;

typedef struct _samplor_inputs {	/* samplor inputs */
	//	char *bufprefix;
	t_symbol *buf_name;
	//t_buffer *buf;
	flext::buffer *buf;
	t_samplorbuffer *samplor_buf;
    t_samplormmap *samplor_mmap;
	int samplenumber;
	t_int offset;			/* position in ms */
	t_int dur;				/* duration in ms */
	t_int envattack;		/* duration in ms */
	t_int envrelease;		/* duration in ms */
	t_int adsr_mode;        /* 0 (default) adsr in ms, 1 -> adsr in % of sound duration */
	t_int attack;			/* duration in ms */
	t_int decay;			/* duration in ms */
	t_int sustain;			/* value in % of amplitude */
	t_int release;			/* duration in ms */
	t_int susloopstart;      /*loop points in samples (new in version 2.91)*/
    t_int susloopend;
	t_samplor_real transp;
	t_float amp;
	t_float pan;
	t_float rev;
	t_int env;
	t_int chan;
	t_int chan2;
    t_int chan3;
    t_int chan4;
    t_samplor_real release_curve;	
} t_samplor_inputs;

typedef struct _samplor_params {	/* samplor parameters and pre-calculated values */
	t_samplor_real sr;			/* sampling rate in Hz */
	//   t_samplor_real sro2;		/* nyquist frequency in Hz */
	t_samplor_real sp;			/* sampling period in s */
	long vs;					/* vector size */
	// t_samplor_real pi;		/* pi (initialized with 4 * atan(1.)) */
	//t_samplor_real twopi;		/* two pi */
	//t_samplor_real piosr;		/* pi over sampling rate */
} t_samplor_params;


#include "linkedlist.h"


typedef struct _samplor {				/* samplor control structure */
    t_samplor_inputs inputs;			/* samplor inputs */
    t_samplor_params params;			/* samplor parameters */
    t_sample windows[NUM_WINS][WIND_SIZE];/* grain envelope */
    t_samplor_list list;				/* samplor list */
   	t_list waiting_notes;				/* notes en attente */
	long interpol;						/* special mode */
	long voice_stealing;				/* special mode */
	long loop_xfade;					/* special mode */
	long debug;
	long active_voices;  /* nombre de voix actives */
    long polyphony;  /* nombre de voix actives */
    t_samplor_real modwheel;
    long n_sf;
} t_samplor;

#if 0
typedef struct _sigsamplor {
	t_pxobject x_obj;
    t_samplor *ctlp;
    t_samplor ctl;
	long num_outputs;			/* nombre de sorties (1,2 ou 3) */
	long stereo_mode;

	t_sample *vectors[SAMPLOR_MAX_OUTCOUNT+1]; 
    long time;
	long count;
} t_sigsamplor;
#endif
/******************************************************************/

